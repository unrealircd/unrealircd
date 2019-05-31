/*
 * Stores active TKLines (G:Lines etc) inside a .db file for persistency
 * (C) Copyright 2019 Gottem and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

#define TKL_DB_VERSION 1100
#define TKL_DB_SAVE_EVERY 893

// Some macros
#ifndef _WIN32
	#define OpenFile(fd, file, flags) fd = open(file, flags, S_IRUSR | S_IWUSR)
#else
	#define OpenFile(fd, file, flags) fd = open(file, flags, S_IREAD | S_IWRITE)
#endif

#define FreeTKLRead() \
 	do { \
		/* Some of these might be NULL */ \
		if(tkltype) MyFree(tkltype); \
		if(usermask) MyFree(usermask); \
		if(hostmask) MyFree(hostmask); \
		if(reason) MyFree(reason); \
		if(setby) MyFree(setby); \
		if(spamf_check) MyFree(spamf_check); \
		if(spamf_matchtype) MyFree(spamf_matchtype); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if((x)) { \
			close(fd); \
			config_warn("[storetkl] Read error from the persistent storage file '%s' (possible corruption)", cfg.database); \
			FreeTKLRead(); \
			return -1; \
		} \
	} while(0)

#define W_SAFE(x) \
	do { \
		if((x)) { \
			close(fd); \
			config_warn("[storetkl] Write error from the persistent storage file '%s' (DATABASE NOT SAVED)", cfg.database); \
			return -1; \
		} \
	} while(0)

#define IsMDErr(x, y, z) \
	do { \
		if(!(x)) { \
			config_error("A critical error occurred when registering ModData for %s: %s", MOD_HEADER(y).name, ModuleGetErrorStr((z)->handle)); \
			return MOD_FAILED; \
		} \
	} while(0)

// Forward declarations
void storetkl_moddata_free(ModData *md);
void setcfg(void);
void freecfg(void);
int storetkl_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int storetkl_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(writeDB_evt);
int writeDB(void);
int readDB(void);
static inline int read_data(int fd, void *buf, size_t len);
static inline int write_data(int fd, void *buf, size_t len);
static int write_str(int fd, char *x);
static int read_str(int fd, char **x);

// Globals
static ModDataInfo *storetkl_md;
static unsigned tkl_db_version = TKL_DB_VERSION;
struct cfgstruct {
	char *database;
};
static struct cfgstruct cfg;

// Stored .db might still work with flags instead of actual values (will be corrected on next write)
// This backport stuff will eventually be removed ;]
unsigned backport_tkl1000;

ModuleHeader MOD_HEADER(m_storetkl) = {
	"m_storetkl",
	"v1.10",
	"Stores active TKL entries persistently/across IRCd restarts",
	"3.2-b8-1",
	NULL
};

MOD_TEST(m_storetkl) {
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, storetkl_configtest);
	return MOD_SUCCESS;
}

MOD_INIT(m_storetkl) {
	MARK_AS_OFFICIAL_MODULE(modinfo);

	// MOD_INIT is also called on rehash, so we gotta make sure to not re-add TKLines every time
	// There might be a cleaner way to do this though (i.e. without moddata) :D
	setcfg();
	if(!(storetkl_md = findmoddata_byname("storetkl_inited", MODDATATYPE_CLIENT))) {
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_CLIENT;
		mreq.name = "storetkl_inited";
		mreq.free = storetkl_moddata_free;
		mreq.serialize = NULL;
		mreq.unserialize = NULL;
		mreq.sync = 0;
		storetkl_md = ModDataAdd(modinfo->handle, mreq);
		IsMDErr(storetkl_md, m_storetkl, modinfo);
		readDB();
		moddata_client((&me), storetkl_md).i = 1;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, storetkl_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD(m_storetkl) {
	EventAddEx(modinfo->handle, "storetkl_writedb", TKL_DB_SAVE_EVERY, 0, writeDB_evt, NULL);
	if(ModuleGetError(modinfo->handle) != MODERR_NOERROR) {
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER(m_storetkl).name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_storetkl) {
	writeDB();
	freecfg();
	return MOD_SUCCESS;
}

void storetkl_moddata_free(ModData *md) {
	if(md->i)
		md->i = 0;
}

void setcfg(void) {
	// Default: data/tkl.db
	cfg.database = strdup("tkl.db");
	convert_to_absolute_path(&cfg.database, PERMDATADIR);
}

void freecfg(void) {
	MyFree(cfg.database);
}

int storetkl_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::storetkl::database
	if(type != CONFIG_SET)
		return 0;

	if(!ce || strcmp(ce->ce_varname, "storetkl"))
		return 0;

	for(cep = ce->ce_entries; cep; cep = cep->ce_next) {
		if(!cep->ce_vardata) {
			config_error("%s:%i: blank set::storetkl::%s without value", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
		if(!strcmp(cep->ce_varname, "database")) {
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
			continue;
		}
		config_error("%s:%i: unknown directive set::storetkl::%s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int storetkl_configrun(ConfigFile *cf, ConfigEntry *ce, int type) {
	ConfigEntry *cep;

	// We are only interested in set::storetkl::database
	if(type != CONFIG_SET)
		return 0;

	if(!ce || strcmp(ce->ce_varname, "storetkl"))
		return 0;

	for(cep = ce->ce_entries; cep; cep = cep->ce_next) {
		if(!strcmp(cep->ce_varname, "database"))
			safestrdup(cfg.database, cep->ce_vardata);
	}
	return 1;
}

EVENT(writeDB_evt) {
	writeDB();
}

int writeDB(void) {
	char tmpfname[512];
	int fd;
	uint64_t tklcount;
	int index;
	aTKline *tkl;
	char *tkltype = NULL;
	char usermask_subtype[256]; // Might need to prefix something to usermask
	uint64_t expire_at, set_at, spamf_tkl_duration; // To prevent 32 vs 64 bit incompatibilies regarding the TS data type(def)

	// Write to a tempfile first, then rename it if everything succeeded
	snprintf(tmpfname, sizeof(tmpfname), "%s.tmp", cfg.database);
	OpenFile(fd, tmpfname, O_CREAT | O_WRONLY | O_TRUNC);
	if(fd == -1) {
		config_warn("[storetkl] Unable to open the persistent storage file '%s' for writing: %s", cfg.database, strerror(errno));
		return -1;
	}

	W_SAFE(write_data(fd, &tkl_db_version, sizeof(tkl_db_version)));
	tklcount = 0;
	for(index = 0; index < TKLISTLEN; index++) {
		for(tkl = tklines[index]; tkl; tkl = tkl->next) {
			// Local spamfilter means it was added through the conf or is built-in, so let's not even write those
			if((tkl->type & TKL_SPAMF) && !(tkl->type & TKL_GLOBAL))
				continue;
			tklcount++;
		}
	}

	W_SAFE(write_data(fd, &tklcount, sizeof(tklcount)));
	tkltype = malloc(sizeof(char) * 2); // No need to keep reallocating it in a loop
	tkltype[1] = '\0';
	for(index = 0; index < TKLISTLEN; index++) {
		for(tkl = tklines[index]; tkl; tkl = tkl->next) {
			// Since we can't just write 'tkl' in its entirety, we have to get the relevant variables instead
			// These will be used to reconstruct the proper internal m_tkl() call ;]
			if((tkl->type & TKL_SPAMF) && !(tkl->type & TKL_GLOBAL)) // Also need to skip this here
				continue;
			tkltype[0] = tkl_typetochar(tkl->type);

			W_SAFE(write_str(fd, tkltype)); // TKL char
			W_SAFE(write_data(fd, &tkl->subtype, sizeof(tkl->subtype))); // Unsigned short (only used for spamfilters/softbans but set to 0 for everything else regardless)

			// Might be a softban
			if(!tkl->ptr.spamf && (tkl->subtype & TKL_SUBTYPE_SOFT)) {
				snprintf(usermask_subtype, sizeof(usermask_subtype), "%%%s", tkl->usermask);
				W_SAFE(write_str(fd, usermask_subtype)); // User mask (targets for spamfilter, like 'cp'), includes % for softbans
			}
			else
				W_SAFE(write_str(fd, tkl->usermask));

			W_SAFE(write_str(fd, tkl->hostmask)); // Host mask (action for spamfilter, like 'block')
			W_SAFE(write_str(fd, tkl->reason)); // Ban reason (TKL time for spamfilters, in case of a gline action)
			W_SAFE(write_str(fd, tkl->setby));

			expire_at = tkl->expire_at;
			set_at = tkl->set_at;
			W_SAFE(write_data(fd, &expire_at, sizeof(expire_at)));
			W_SAFE(write_data(fd, &set_at, sizeof(set_at)));

			if(tkl->ptr.spamf) {
				W_SAFE(write_str(fd, "SPAMF")); // Write a string so we know to expect more when reading the DB
				W_SAFE(write_data(fd, &tkl->ptr.spamf->action, sizeof(tkl->ptr.spamf->action))); // Unsigned short (block, GZ:Line, etc; also refer to BAN_ACT_*)
				W_SAFE(write_str(fd, tkl->ptr.spamf->tkl_reason));

				spamf_tkl_duration = tkl->ptr.spamf->tkl_duration;
				W_SAFE(write_data(fd, &spamf_tkl_duration, sizeof(spamf_tkl_duration)));
				W_SAFE(write_str(fd, tkl->ptr.spamf->expr->str)); // Actual expression/regex/etc

				// Expression type (simple/PCRE), see also enum MatchType
				if(tkl->ptr.spamf->expr->type == MATCH_PCRE_REGEX)
					W_SAFE(write_str(fd, "regex"));
				else
					W_SAFE(write_str(fd, "simple"));
			}
			else
				W_SAFE(write_str(fd, "NOSPAMF"));
		}
	}
	MyFree(tkltype);

	// Everything seems to have gone well, attempt to rename the tempfile
	close(fd);
	if(rename(tmpfname, cfg.database) < 0) {
		config_warn("[storetkl] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
		return -1;
	}
	return 0;
}

int readDB(void) {
	int fd;
	uint64_t i;
	uint64_t tklcount = 0;
	size_t tklcount_tkl1000 = 0;
	uint64_t num = 0;
	uint64_t rewrite = 0;
	unsigned version;

	// Variables for all TKL types
	char *tkltype = NULL;
	char *usermask = NULL;
	char *hostmask = NULL;
	char *reason = NULL;
	char *setby = NULL;

	// Some stuff related to spamfilters
	char *spamf_check = NULL;
	char *spamf_matchtype = NULL;

	ircd_log(LOG_ERROR, "[storetkl] Reading stored X:Lines from '%s'", cfg.database);
	sendto_realops("[storetkl] Reading stored X:Lines from '%s'", cfg.database); // Probably won't be seen ever, but just in case ;]
	OpenFile(fd, cfg.database, O_RDONLY);
	if(fd == -1) {
		if(errno != ENOENT)
			config_warn("[storetkl] Unable to open the persistent storage file '%s' for reading: %s", cfg.database, strerror(errno));
		return -1;
	}

	R_SAFE(read_data(fd, &version, sizeof(version)));
	if(version > tkl_db_version) { // Older DBs should still work with newer versions of this module
		config_warn("[storetkl] File '%s' has a wrong database version: expected it to be <= %u but got %u instead", cfg.database, tkl_db_version, version);
		close(fd);
		return -1;
	}

	backport_tkl1000 = (version <= 1000 ? 1 : 0);
	if(backport_tkl1000) {
		R_SAFE(read_data(fd, &tklcount_tkl1000, sizeof(tklcount_tkl1000)));
		tklcount = tklcount_tkl1000;
	}
	else
		R_SAFE(read_data(fd, &tklcount, sizeof(tklcount)));

	for(i = 1; i <= tklcount; i++) {
		int type;
		unsigned short subtype;
		int parc = 0;

		// Variables for all TKL types
		usermask = NULL;
		hostmask = NULL;
		reason = NULL;
		setby = NULL;
		char tklflag;
		tkltype = NULL;
		uint64_t expire_at, set_at;
		TS expire_at_tkl1000, set_at_tkl1000;
		char setTime[100], expTime[100], spamfTime[100];

		// Some stuff related to spamfilters
		spamf_check = NULL;
		int spamf = 0;
		unsigned short spamf_action;
		char *spamf_tkl_reason = NULL;
		TS spamf_tkl_duration_tkl1000;
		uint64_t spamf_tkl_duration;
		char *spamf_expr = NULL;
		MatchType matchtype;
		spamf_matchtype = NULL;

		int doadd = 1;
		aTKline *tkl;

		char *tkllayer[13] = { // Args for m_tkl()
			me.name, // 0: Server name
			"+", // 1: Direction
			NULL, // 2: Type, like G
			NULL, // 3: User mask (targets for spamfilter)
			NULL, // 4: Host mask (action for spamfilter)
			NULL, // 5: Set by who
			NULL, // 6: Expiration time
			NULL, // 7: Set-at time
			NULL, // 8: Reason (TKL time for spamfilters in case of a gline action etc)
			NULL, // 9: Spamfilter only: TKL reason (w/ underscores and all)
			NULL, // 10: Spamfilter only: Match type (simple/regex)
			NULL, // 11: Spamfilter only: Match string/regex
			NULL, // 12: Some functions rely on the post-last entry being NULL =]
		};

		if(backport_tkl1000) {
			R_SAFE(read_data(fd, &type, sizeof(type)));
			tkltype = malloc(sizeof(char) * 2);
			tklflag = tkl_typetochar(type);
			tkltype[0] = tklflag;
			tkltype[1] = '\0';
		}
		else {
			R_SAFE(read_str(fd, &tkltype)); // No need for tkl_typetochar() on read anymore
			tklflag = tkltype[0];
		}

		R_SAFE(read_data(fd, &subtype, sizeof(subtype)));
		R_SAFE(read_str(fd, &usermask));
		R_SAFE(read_str(fd, &hostmask));
		R_SAFE(read_str(fd, &reason));
		R_SAFE(read_str(fd, &setby));

		if(backport_tkl1000) {
			R_SAFE(read_data(fd, &expire_at_tkl1000, sizeof(expire_at_tkl1000)));
			R_SAFE(read_data(fd, &set_at_tkl1000, sizeof(set_at_tkl1000)));
			expire_at = expire_at_tkl1000;
			set_at = set_at_tkl1000;
		}
		else {
			R_SAFE(read_data(fd, &expire_at, sizeof(expire_at)));
			R_SAFE(read_data(fd, &set_at, sizeof(set_at)));
		}

		R_SAFE(read_str(fd, &spamf_check));
		if(!strcmp(spamf_check, "SPAMF")) {
			spamf = 1;
			R_SAFE(read_data(fd, &spamf_action, sizeof(spamf_action)));
			R_SAFE(read_str(fd, &spamf_tkl_reason));

			if(backport_tkl1000) {
				R_SAFE(read_data(fd, &spamf_tkl_duration_tkl1000, sizeof(spamf_tkl_duration_tkl1000)));
				spamf_tkl_duration = spamf_tkl_duration_tkl1000;
			}
			else
				R_SAFE(read_data(fd, &spamf_tkl_duration, sizeof(spamf_tkl_duration)));

			R_SAFE(read_str(fd, &spamf_expr));
			if(backport_tkl1000) {
				R_SAFE(read_data(fd, &matchtype, sizeof(matchtype)));
				if(matchtype == MATCH_PCRE_REGEX)
					spamf_matchtype = strdup("regex");
				else
					spamf_matchtype = strdup("simple");
			}
			else {
				// We have better compatibility by just using the strings, since its MATCH_* counterpart might just change in value someday
				R_SAFE(read_str(fd, &spamf_matchtype));
				if(!strcmp(spamf_matchtype, "regex"))
					matchtype = MATCH_PCRE_REGEX;
				else
					matchtype = MATCH_SIMPLE;
			}
		}

		// Should probably not re-add if it should be expired to begin with
		if(expire_at != 0 && expire_at <= TStime()) {
			ircd_log(LOG_ERROR, "[storetkl] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason);
			sendto_realops("[storetkl] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason); // Probably won't be seen ever, but just in case ;]
			rewrite++;
			FreeTKLRead();
			continue;
		}

		ircsnprintf(setTime, sizeof(setTime), "%li", set_at);
		ircsnprintf(expTime, sizeof(expTime), "%li", expire_at);

		if(spamf && tklflag == 'f') // Just in case someone modifies the DB to contain local spamfilters, as well as for v1000
			doadd = 0;

		// Build TKL args
		// All of these except [8] are the same for all (only odd one is spamfilter)
		parc = 9;
		tkllayer[2] = tkltype;
		tkllayer[3] = usermask;
		tkllayer[4] = hostmask;
		tkllayer[5] = setby;
		tkllayer[6] = expTime;
		tkllayer[7] = setTime;
		tkllayer[8] = reason;

		if(spamf) {
			parc = 12;
			// Make sure this particular TKLine isn't already active somehow
			for(tkl = tklines[tkl_hash(tklflag)]; doadd && tkl; tkl = tkl->next) {
				// We can assume it's the same spamfilter if all of the following match: spamfilter expression, targets, TKL reason, action, matchtype and TKL duration
				if(!strcmp(tkl->ptr.spamf->expr->str, spamf_expr) && !strcmp(tkl->usermask, usermask) && !strcmp(tkl->ptr.spamf->tkl_reason, spamf_tkl_reason) &&
					tkl->ptr.spamf->action == spamf_action && tkl->ptr.spamf->expr->type == matchtype && tkl->ptr.spamf->tkl_duration == spamf_tkl_duration) {
					doadd = 0;
					break;
				}
			}

			if(doadd) {
				ircsnprintf(spamfTime, sizeof(spamfTime), "%li", spamf_tkl_duration);
				tkllayer[8] = spamfTime;
				tkllayer[9] = spamf_tkl_reason;
				tkllayer[10] = spamf_matchtype;
				tkllayer[11] = spamf_expr;
			}
		}
		else {
			for(tkl = tklines[tkl_hash(tklflag)]; tkl; tkl = tkl->next) {
				if(!strcmp(tkl->usermask, usermask) && !strcmp(tkl->hostmask, hostmask) && !strcmp(tkl->reason, reason) && tkl->expire_at == expire_at) {
					doadd = 0;
					break;
				}
			}
		}

		if(doadd) {
			m_tkl(&me, &me, NULL, parc, tkllayer);
			num++;
		}
		FreeTKLRead();
	}
	close(fd);

	if(num) {
		ircd_log(LOG_ERROR, "[storetkl] Re-added %li X:Lines", num);
		sendto_realops("[storetkl] Re-added %li X:Lines", num); // Probably won't be seen ever, but just in case ;]
	}

	if(rewrite) {
		ircd_log(LOG_ERROR, "[storetkl] Rewriting DB file due to %li skipped/expired X:Line%s", rewrite, (rewrite > 1 ? "s" : ""));
		sendto_realops("[storetkl] Rewriting DB file due to %li skipped/expired X:Line%s", rewrite, (rewrite > 1 ? "s" : "")); // Probably won't be seen ever, but just in case ;]
		return writeDB();
	}

	return 0;
}

static inline int read_data(int fd, void *buf, size_t len) {
	if((size_t)read(fd, buf, len) < len)
		return -1;
	return 0;
}

static inline int write_data(int fd, void *buf, size_t len) {
	if((size_t)write(fd, buf, len) < len)
		return -1;
	return 0;
}

static int write_str(int fd, char *x) {
	uint64_t len = (x ? strlen(x) : 0);
	if(write_data(fd, &len, sizeof(len)))
		return -1;
	if(len) {
		if(write_data(fd, x, sizeof(char) * len))
			return -1;
	}
	return 0;
}

static int read_str(int fd, char **x) {
	uint64_t len;
	size_t len_tkl1000; // len used to be of type size_t, but this has portability problems
	size_t size;
	if(backport_tkl1000) {
		if(read_data(fd, &len_tkl1000, sizeof(len_tkl1000)))
			return -1;
		len = len_tkl1000;
	}
	else {
		if(read_data(fd, &len, sizeof(len)))
			return -1;
	}
	if(!len) {
		*x = NULL;
		return 0;
	}

	size = sizeof(char) * len;
	*x = (char *)MyMalloc(size + sizeof(char));
	if(read_data(fd, *x, size)) {
		MyFree(*x);
		*x = NULL;
		return -1;
	}
	(*x)[len] = 0;
	return 0;
}
