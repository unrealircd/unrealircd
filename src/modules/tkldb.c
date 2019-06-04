/*
 * Stores active *-Lines (G:Lines etc) inside a .db file for persistency
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
		safefree(tkltype); \
		safefree(usermask); \
		safefree(hostmask); \
		safefree(reason); \
		safefree(setby); \
		safefree(spamf_check); \
		safefree(spamf_matchtype); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[tkldb] Read error from the persistent storage file '%s' (possible corruption): %s", cfg.database, strerror(errno)); \
			close(fd); \
			FreeTKLRead(); \
			return 0; \
		} \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[tkldb] Write error from the persistent storage tempfile '%s': %s (DATABASE NOT SAVED)", tmpfname, strerror(errno)); \
			close(fd); \
			return 0; \
		} \
	} while(0)

#define IsMDErr(x, y, z) \
	do { \
		if (!(x)) { \
			config_error("A critical error occurred when registering ModData for %s: %s", MOD_HEADER(y).name, ModuleGetErrorStr((z)->handle)); \
			return MOD_FAILED; \
		} \
	} while(0)

// Forward declarations
void tkldb_moddata_free(ModData *md);
void setcfg(void);
void freecfg(void);
int tkldb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int tkldb_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(write_tkldb_evt);
int write_tkldb(void);
int write_tkline(int fd, const char *tmpfname, aTKline *tkl);
int read_tkldb(void);
static inline int read_data(int fd, void *buf, size_t len);
static inline int write_data(int fd, void *buf, size_t len);
static int write_str(int fd, char *x);
static int read_str(int fd, char **x);

// Globals
static ModDataInfo *tkldb_md;
static uint32_t tkl_db_version = TKL_DB_VERSION;
struct cfgstruct {
	char *database;

	// Stored .db might still work with flags instead of actual values (will be corrected on next write)
	// This backport stuff will eventually be removed ;]
	unsigned int backport_tkl1000;
};
static struct cfgstruct cfg;

ModuleHeader MOD_HEADER(tkldb) = {
	"tkldb",
	"v1.10",
	"Stores active TKL entries (*-Lines) persistently/across IRCd restarts",
	"3.2-b8-1",
	NULL
};

MOD_TEST(tkldb)
{
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkldb_configtest);
	return MOD_SUCCESS;
}

MOD_INIT(tkldb)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	// MOD_INIT is also called on rehash, so we gotta make sure to not re-add *-Lines every time
	// There might be a cleaner way to do this though (i.e. without moddata) :D
	setcfg();
	if (!(tkldb_md = findmoddata_byname("tkldb_inited", MODDATATYPE_CLIENT)))
	{
		ModDataInfo mreq;
		memset(&mreq, 0, sizeof(mreq));
		mreq.type = MODDATATYPE_CLIENT;
		mreq.name = "tkldb_inited";
		mreq.free = tkldb_moddata_free;
		mreq.serialize = NULL;
		mreq.unserialize = NULL;
		mreq.sync = 0;
		tkldb_md = ModDataAdd(modinfo->handle, mreq);
		IsMDErr(tkldb_md, tkldb, modinfo);
		if (!read_tkldb())
			return MOD_FAILED;
		moddata_client((&me), tkldb_md).i = 1;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkldb_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD(tkldb)
{
	EventAddEx(modinfo->handle, "tkldb_write_tkldb", TKL_DB_SAVE_EVERY, 0, write_tkldb_evt, NULL);
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER(tkldb).name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD(tkldb)
{
	write_tkldb();
	freecfg();
	return MOD_SUCCESS;
}

void tkldb_moddata_free(ModData *md)
{
	if (md->i)
		md->i = 0;
}

void setcfg(void)
{
	// Default: data/tkl.db
	cfg.database = strdup("tkl.db");
	convert_to_absolute_path(&cfg.database, PERMDATADIR);
	cfg.backport_tkl1000 = 0;
}

void freecfg(void)
{
	MyFree(cfg.database);
}

int tkldb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "tkldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata) {
			config_error("%s:%i: blank set::tkldb::%s without value", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "database")) {
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
			continue;
		}
		config_error("%s:%i: unknown directive set::tkldb::%s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int tkldb_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "tkldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "database"))
			safestrdup(cfg.database, cep->ce_vardata);
	}
	return 1;
}

EVENT(write_tkldb_evt)
{
	write_tkldb();
}

int write_tkldb(void)
{
	char tmpfname[512];
	int fd;
	uint64_t tklcount;
	int index, index2;
	aTKline *tkl;

	// Write to a tempfile first, then rename it if everything succeeded
	snprintf(tmpfname, sizeof(tmpfname), "%s.tmp", cfg.database);
	OpenFile(fd, tmpfname, O_CREAT | O_WRONLY | O_TRUNC);
	if (fd < 0)
	{
		config_warn("[tkldb] Unable to open the persistent storage tempfile '%s' for writing: %s", tmpfname, strerror(errno));
		return 0;
	}

	W_SAFE(write_data(fd, &tkl_db_version, sizeof(tkl_db_version)));

	// Count the *-Lines
	tklcount = 0;

	// First the ones in the hash table
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
				tklcount++;
		}
	}
	// Then the regular *-Lines
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			// Local spamfilter means it was added through the conf or is built-in, so let's not even write those
			if ((tkl->type & TKL_SPAMF) && !(tkl->type & TKL_GLOBAL))
				continue;
			tklcount++;
		}
	}
	W_SAFE(write_data(fd, &tklcount, sizeof(tklcount)));

	// Now write the actual *-Lines, first the ones in the hash table
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next) {
				if(!write_tkline(fd, tmpfname, tkl)) // write_tkline() closes the fd on errors itself
					return 0;
			}
		}
	}
	// Then the regular *-Lines
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			// Local spamfilter means it was added through the conf or is built-in, so let's not even write those
			if ((tkl->type & TKL_SPAMF) && !(tkl->type & TKL_GLOBAL)) // Also need to skip this here
				continue;
			if(!write_tkline(fd, tmpfname, tkl))
				return 0;
		}
	}

	// Everything seems to have gone well, attempt to close and rename the tempfile
	if(close(fd) < 0)
	{
		config_warn("[tkldb] Got an error when trying to close the persistent storage file '%s' (possible corruption occurred, DATABASE NOT SAVED): %s", cfg.database, strerror(errno));
		return 0;
	}
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_warn("[tkldb] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
		return 0;
	}
	return 1;
}

int write_tkline(int fd, const char *tmpfname, aTKline *tkl)
{
	// Since we can't just write 'tkl' in its entirety, we have to get the relevant variables instead
	// These will be used to reconstruct the proper internal m_tkl() call ;]
	char tkltype[2];
	char usermask_subtype[256]; // Might need to prefix something to usermask
	char spamf_action; // Storing the action not as unsigned short, but as char is more reliable for the future
	uint64_t expire_at, set_at, spamf_tkl_duration; // To prevent 32 vs 64 bit incompatibilies regarding the TS data type(def)

	tkltype[0] = tkl_typetochar(tkl->type);
	tkltype[1] = '\0';
	W_SAFE(write_str(fd, tkltype)); // TKL char

	// Might be a softban
	if (!tkl->ptr.spamf && (tkl->subtype & TKL_SUBTYPE_SOFT))
	{
		snprintf(usermask_subtype, sizeof(usermask_subtype), "%%%s", tkl->usermask);
		W_SAFE(write_str(fd, usermask_subtype)); // User mask (targets for spamfilter, like 'cpnNPqdatu'), includes % for softbans
	} else
	{
		W_SAFE(write_str(fd, tkl->usermask));
	}

	W_SAFE(write_str(fd, tkl->hostmask)); // Host mask (action for spamfilter, like 'block')
	W_SAFE(write_str(fd, tkl->reason)); // Ban reason (TKL time for spamfilters, in case of a gline action)
	W_SAFE(write_str(fd, tkl->setby));

	expire_at = tkl->expire_at;
	set_at = tkl->set_at;
	W_SAFE(write_data(fd, &expire_at, sizeof(expire_at)));
	W_SAFE(write_data(fd, &set_at, sizeof(set_at)));

	if (tkl->ptr.spamf)
	{
		W_SAFE(write_str(fd, "SPAMF")); // Write a string so we know to expect more when reading the DB
		spamf_action = banact_valtochar(tkl->ptr.spamf->action); // Block, GZ:Line, etc; also refer to BAN_ACT_*
		W_SAFE(write_data(fd, &spamf_action, sizeof(spamf_action)));
		W_SAFE(write_str(fd, tkl->ptr.spamf->tkl_reason));
		spamf_tkl_duration = tkl->ptr.spamf->tkl_duration;
		W_SAFE(write_data(fd, &spamf_tkl_duration, sizeof(spamf_tkl_duration)));
		W_SAFE(write_str(fd, tkl->ptr.spamf->expr->str)); // Actual expression/regex/etc

		// Expression type (simple/PCRE), see also enum MatchType
		if (tkl->ptr.spamf->expr->type == MATCH_PCRE_REGEX)
			W_SAFE(write_str(fd, "regex"));
		else
			W_SAFE(write_str(fd, "simple"));
	} else
	{
		W_SAFE(write_str(fd, "NOSPAMF"));
	}
	return 1;
}

int read_tkldb(void)
{
	int fd;
	uint64_t i;
	uint64_t tklcount = 0;
	size_t tklcount_tkl1000 = 0;
	uint64_t added = 0;
	uint64_t skipped = 0;
	uint32_t version;

	// Variables for all TKL types
	// Some of them need to be declared and NULL initialised early due to the macro FreeTKLRead() being used by R_SAFE() on error
	char *tkltype = NULL;
	char *usermask = NULL;
	char *hostmask = NULL;
	char *reason = NULL;
	char *setby = NULL;

	// Some stuff related to spamfilters
	char *spamf_check = NULL;
	char *spamf_matchtype = NULL;

	ircd_log(LOG_ERROR, "[tkldb] Reading stored *-Lines from '%s'", cfg.database);
	sendto_realops("[tkldb] Reading stored *-Lines from '%s'", cfg.database); // Probably won't be seen ever, but just in case ;]
	OpenFile(fd, cfg.database, O_RDONLY);
	if (fd < 0)
	{
		if (errno == ENOENT)
		{
			/* Database does not exist. Could be first boot */
			config_warn("[tkldb] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else {
			config_warn("[tkldb] Unable to open the persistent storage file '%s' for reading: %s", cfg.database, strerror(errno));
			return 0;
		}
	}

	R_SAFE(read_data(fd, &version, sizeof(version)));
	if (version > tkl_db_version)
	{
		// Older DBs should still work with newer versions of this module
		config_warn("[tkldb] Database '%s' has a wrong version: expected it to be <= %u but got %u instead", cfg.database, tkl_db_version, version);
		if(close(fd) < 0)
			config_warn("[tkldb] Got an error when trying to close the persistent storage file '%s' (possible corruption occurred): %s", cfg.database, strerror(errno));
		return 0;
	}

	cfg.backport_tkl1000 = (version <= 1000 ? 1 : 0);
	if (cfg.backport_tkl1000)
	{
		R_SAFE(read_data(fd, &tklcount_tkl1000, sizeof(tklcount_tkl1000)));
		tklcount = tklcount_tkl1000;
	} else
	{
		R_SAFE(read_data(fd, &tklcount, sizeof(tklcount)));
	}

	for (i = 1; i <= tklcount; i++)
	{
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
		char spamf_action;
		unsigned short spamf_actionval;
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

		if (cfg.backport_tkl1000)
		{
			R_SAFE(read_data(fd, &type, sizeof(type)));
			tklflag = tkl_typetochar(type);
			tkltype = MyMallocEx(2);
			tkltype[0] = tklflag;
			tkltype[1] = '\0';
			R_SAFE(read_data(fd, &subtype, sizeof(subtype))); // Subtype is kinda redundant so we're not using it past v1000 anymore
		}
		else {
			R_SAFE(read_str(fd, &tkltype)); // No need for tkl_typetochar() on read anymore
			tklflag = tkltype[0];
		}

		R_SAFE(read_str(fd, &usermask));
		R_SAFE(read_str(fd, &hostmask));
		R_SAFE(read_str(fd, &reason));
		R_SAFE(read_str(fd, &setby));

		if (cfg.backport_tkl1000)
		{
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
		if (!strcmp(spamf_check, "SPAMF"))
		{
			spamf = 1;

			if (cfg.backport_tkl1000)
			{
				R_SAFE(read_data(fd, &spamf_actionval, sizeof(spamf_actionval)));
			} else {
				R_SAFE(read_data(fd, &spamf_action, sizeof(spamf_action)));
				spamf_actionval = banact_chartoval(spamf_action);
			}

			R_SAFE(read_str(fd, &spamf_tkl_reason));

			if (cfg.backport_tkl1000)
			{
				R_SAFE(read_data(fd, &spamf_tkl_duration_tkl1000, sizeof(spamf_tkl_duration_tkl1000)));
				spamf_tkl_duration = spamf_tkl_duration_tkl1000;
			} else {
				R_SAFE(read_data(fd, &spamf_tkl_duration, sizeof(spamf_tkl_duration)));
			}

			R_SAFE(read_str(fd, &spamf_expr));
			if (cfg.backport_tkl1000)
			{
				R_SAFE(read_data(fd, &matchtype, sizeof(matchtype)));
				if (matchtype == MATCH_PCRE_REGEX)
					spamf_matchtype = strdup("regex");
				else
					spamf_matchtype = strdup("simple");
			}
			else {
				// We have better compatibility by just using the strings, since its MATCH_* counterpart might just change in value someday
				R_SAFE(read_str(fd, &spamf_matchtype));
				if (!strcmp(spamf_matchtype, "regex"))
					matchtype = MATCH_PCRE_REGEX;
				else
					matchtype = MATCH_SIMPLE;
			}
		}

		// Should probably not re-add if it should be expired to begin with
		if (expire_at != 0 && expire_at <= TStime())
		{
			ircd_log(LOG_ERROR, "[tkldb] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason);
			sendto_realops("[tkldb] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason); // Probably won't be seen ever, but just in case ;]
			skipped++;
			FreeTKLRead();
			continue;
		}

		ircsnprintf(setTime, sizeof(setTime), "%li", set_at);
		ircsnprintf(expTime, sizeof(expTime), "%li", expire_at);

		if (spamf && tklflag == 'f') // Just in case someone modifies the DB to contain local spamfilters, as well as for v1000
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

		if (spamf)
		{
			parc = 12;
			// Make sure this particular TKLine isn't already active somehow
			for (tkl = tklines[tkl_hash(tklflag)]; doadd && tkl; tkl = tkl->next)
			{
				// We can assume it's the same spamfilter if all of the following match: spamfilter expression, targets, TKL reason, action, matchtype and TKL duration
				if (!strcmp(tkl->ptr.spamf->expr->str, spamf_expr) && !strcmp(tkl->usermask, usermask) && !strcmp(tkl->ptr.spamf->tkl_reason, spamf_tkl_reason) &&
					tkl->ptr.spamf->action == spamf_action && tkl->ptr.spamf->expr->type == matchtype && tkl->ptr.spamf->tkl_duration == spamf_tkl_duration)
				{
					doadd = 0;
					break;
				}
			}

			if (doadd)
			{
				ircsnprintf(spamfTime, sizeof(spamfTime), "%li", spamf_tkl_duration);
				tkllayer[8] = spamfTime;
				tkllayer[9] = spamf_tkl_reason;
				tkllayer[10] = spamf_matchtype;
				tkllayer[11] = spamf_expr;
			}
		}
		else {
			for (tkl = tklines[tkl_hash(tklflag)]; tkl; tkl = tkl->next)
			{
				if (!strcmp(tkl->usermask, usermask) && !strcmp(tkl->hostmask, hostmask) && !strcmp(tkl->reason, reason) && tkl->expire_at == expire_at)
				{
					doadd = 0;
					break;
				}
			}
		}

		if (doadd)
		{
			m_tkl(&me, &me, NULL, parc, tkllayer);
			added++;
		}
		FreeTKLRead();
	}

	// No need to return from the function at this point, as all *-Lines have been successfully retrieved anyways =]
	if(close(fd) < 0)
		config_warn("[tkldb] Got an error when trying to close the persistent storage file '%s' (possible corruption occurred): %s", cfg.database, strerror(errno));

	if (added || skipped)
	{
		ircd_log(LOG_ERROR, "[tkldb] Re-added %li and skipped %li *-Lines", added, skipped);
		sendto_realops("[tkldb] Re-added %li and skipped %li *-Lines", added, skipped); // Probably won't be seen ever, but just in case ;]
	}
	return 1;
}

static inline int read_data(int fd, void *buf, size_t len)
{
	if ((size_t)read(fd, buf, len) < len)
		return 0;
	return 1;
}

static inline int write_data(int fd, void *buf, size_t len)
{
	if ((size_t)write(fd, buf, len) < len)
		return 0;
	return 1;
}

static int write_str(int fd, char *x)
{
	uint64_t len = (x ? strlen(x) : 0);
	if (!write_data(fd, &len, sizeof(len)))
		return 0;
	if (len)
	{
		if (!write_data(fd, x, len))
			return 0;
	}
	return 1;
}

static int read_str(int fd, char **x)
{
	uint64_t len;
	size_t len_tkl1000; // len used to be of type size_t, but this has portability problems when writing to/reading from binary files
	size_t size;

	*x = NULL;

	if (cfg.backport_tkl1000)
	{
		if (!read_data(fd, &len_tkl1000, sizeof(len_tkl1000)))
			return 0;
		len = len_tkl1000;
	} else
	{
		if (!read_data(fd, &len, sizeof(len)))
			return 0;
	}

	if (!len)
	{
		*x = strdup(""); // It's safer for m_tkl to work with empty strings instead of NULLs
		return 1;
	}

	size = len;
	*x = MyMallocEx(size + 1);
	if (!read_data(fd, *x, size))
	{
		MyFree(*x);
		*x = NULL;
		return 0;
	}
	(*x)[len] = 0;
	return 1;
}
