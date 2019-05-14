/*
 * Stores TKLines (G:Lines etc) inside a .db file for persistency
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

#define TKL_DB_VERSION 1000

// Some macros
#ifndef _WIN32
	#define OpenFile(fd, file, flags) fd = open(file, flags, S_IRUSR | S_IWUSR)
#else
	#define OpenFile(fd, file, flags) fd = open(file, flags, S_IREAD | S_IWRITE)
#endif

#define R_SAFE(x) \
	do { \
		if((x)) \
		{ \
			close(fd); \
			config_warn("[storetkl] Read error from the persistent storage file '%s' on server %s (possible corruption)", cfg.database, me.name); \
			return -1; \
		} \
	} while (0)

#define W_SAFE(x) \
	do { \
		if((x)) \
		{ \
			close(fd); \
			config_warn("[storetkl] Write error from the persistent storage file '%s' on server %s (possible corruption)", cfg.database, me.name); \
			return -1; \
		} \
	} while (0)

#define IsMDErr(x, y, z) \
	do { \
		if(!(x)) { \
			config_error("A critical error occurred when registering ModData for %s: %s", MOD_HEADER(y).name, ModuleGetErrorStr((z)->handle)); \
			return MOD_FAILED; \
		} \
	} while(0)

// Forward declarations
void storetkl_moddata_free(ModData *md);
int storetkl_hook_tkl_add(aClient *cptr, aClient *sptr, aTKline *tkl, int parc, char *parv[]);
int storetkl_hook_tkl_del(aClient *cptr, aClient *sptr, aTKline *tkl, int parc, char *parv[]);
void setcfg(void);
void freecfg(void);
int readDB(void);
int writeDB(aTKline *origtkl, char what);
static inline int read_data(int fd, void *buf, size_t count);
static inline int write_data(int fd, void *buf, size_t count);
static int write_str(int fd, char *x);
static int read_str(int fd, char **x);

// Globals
static ModDataInfo *storetkl_md;
static unsigned tkl_db_version = TKL_DB_VERSION;
struct cfgstruct {
	char *database;
};
static struct cfgstruct cfg;

ModuleHeader MOD_HEADER(m_storetkl) = {
	"m_storetkl",
	"v1.02",
	"Store TKL entries persistently across IRCd restarts",
	"3.2-b8-1",
	NULL
};

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
	HookAdd(modinfo->handle, HOOKTYPE_TKL_ADD, 999, storetkl_hook_tkl_add);
	HookAdd(modinfo->handle, HOOKTYPE_TKL_DEL, 999, storetkl_hook_tkl_del);
	return MOD_SUCCESS;
}

MOD_LOAD(m_storetkl) {
	if(ModuleGetError(modinfo->handle) != MODERR_NOERROR) {
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER(m_storetkl).name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_storetkl) {
	freecfg();
	return MOD_SUCCESS;
}

void storetkl_moddata_free(ModData *md) {
	if(md->i)
		md->i = 0;
}

int storetkl_hook_tkl_add(aClient *cptr, aClient *sptr, aTKline *tkl, int parc, char *parv[]) {
	writeDB(tkl, '+');
	return HOOK_CONTINUE;
}
int storetkl_hook_tkl_del(aClient *cptr, aClient *sptr, aTKline *tkl, int parc, char *parv[]) {
	writeDB(tkl, '-');
	return HOOK_CONTINUE;
}

int writeDB(aTKline *origtkl, char what) {
	int fd;
	size_t count;
	int index;
	aTKline *tkl;

	OpenFile(fd, cfg.database, O_CREAT | O_WRONLY | O_TRUNC);
	if(fd == -1) {
		config_warn("[storetkl] Unable to open the persistent storage file '%s' for writing on server %s: %s", cfg.database, me.name, strerror(errno));
		return -1;
	}

	W_SAFE(write_data(fd, &tkl_db_version, sizeof(tkl_db_version)));

	count = 0;
	for(index = 0; index < TKLISTLEN; index++) {
		for(tkl = tklines[index]; tkl; tkl = tkl->next) {
			if(origtkl && what == '-' && origtkl == tkl)
				continue;
			count++;
		}
	}

	W_SAFE(write_data(fd, &count, sizeof(count)));

	for(index = 0; index < TKLISTLEN; index++) {
		for(tkl = tklines[index]; tkl; tkl = tkl->next) {
			if(origtkl && what == '-' && origtkl == tkl)
				continue;

			// Since we can't just write 'tkl' in its entirety, we have to get the relevant variables instead
			// These will be used to reconstruct the proper internal m_tkl() call ;]
			W_SAFE(write_data(fd, &tkl->type, sizeof(tkl->type))); // Integer (G:Line, Q:Line, etc; also refer to TKL_*)
			W_SAFE(write_data(fd, &tkl->subtype, sizeof(tkl->subtype))); // Unsigned short (only used for spamfilters but set to 0 for everything else regardless)
			W_SAFE(write_str(fd, tkl->usermask)); // User mask (targets for spamfilter, like 'cp')
			W_SAFE(write_str(fd, tkl->hostmask)); // Host mask (action for spamfilter, like 'block')
			W_SAFE(write_str(fd, tkl->reason)); // Ban reason (TKL time for spamfilters, in case of a gline action)
			W_SAFE(write_str(fd, tkl->setby));
			W_SAFE(write_data(fd, &tkl->expire_at, sizeof(tkl->expire_at)));
			W_SAFE(write_data(fd, &tkl->set_at, sizeof(tkl->set_at)));

			if(tkl->ptr.spamf) {
				W_SAFE(write_str(fd, "SPAMF")); // Write a string so we know to expect more when reading the DB
				W_SAFE(write_data(fd, &tkl->ptr.spamf->action, sizeof(tkl->ptr.spamf->action))); // Unsigned short (block, GZ:Line, etc; also refer to BAN_ACT_*)
				W_SAFE(write_str(fd, tkl->ptr.spamf->tkl_reason));
				W_SAFE(write_data(fd, &tkl->ptr.spamf->tkl_duration, sizeof(tkl->ptr.spamf->tkl_duration)));
				W_SAFE(write_str(fd, tkl->ptr.spamf->expr->str)); // Actual expression/regex/etc
				W_SAFE(write_data(fd, &tkl->ptr.spamf->expr->type, sizeof(tkl->ptr.spamf->expr->type))); // Integer (expression type [simple/PCRE]; see also enum MatchType)
			}
			else
				W_SAFE(write_str(fd, "NOSPAMF"));
		}
	}

	close(fd);
	return 0;
}

void setcfg(void) {
	// data/tkl.db
	cfg.database = strdup("tkl.db");
	convert_to_absolute_path(&cfg.database, PERMDATADIR);
}

void freecfg(void) {
	MyFree(cfg.database);
}

int readDB(void) {
	int fd;
	size_t count;
	int i;
	unsigned num = 0;
	unsigned rewrite = 0;
	unsigned version;

	ircd_log(LOG_ERROR, "[storetkl] Reading stored X:Lines from '%s'", cfg.database);
	sendto_realops("[storetkl] Reading stored X:Lines from '%s'", cfg.database); // Probably won't be seen ever, but just in case ;]
	OpenFile(fd, cfg.database, O_RDONLY);

	if(fd == -1) {
		if(errno != ENOENT)
			config_warn("[storetkl] Unable to open the persistent storage file '%s' for reading on server %s: %s", cfg.database, me.name, strerror(errno));
		return -1;
	}

	R_SAFE(read_data(fd, &version, sizeof(version)));

	if(version > tkl_db_version) { // Older DBs should still work with newer versions of this module
		config_warn("File '%s' has a wrong database version on server %s: expected it to be <= %u but got %u instead", cfg.database, me.name, tkl_db_version, version);
		close(fd);
		return -1;
	}

	R_SAFE(read_data(fd, &count, sizeof(count)));

	for(i = 1; i <= count; i++) {
		int type;
		unsigned short subtype;
		int parc = 0;

		// Variables for all TKL types
		char *usermask = NULL;
		char *hostmask = NULL;
		char *reason = NULL;
		char *setby = NULL;
		char tklflag;
		char *tkltype = NULL;
		TS expire_at, set_at;
		char setTime[100], expTime[100], spamfTime[100];

		// Some stuff related to spamfilters
		char *spamf_check = NULL;
		int spamf = 0;
		unsigned short spamf_action;
		char *spamf_tkl_reason = NULL;
		TS spamf_tkl_duration;
		char *spamf_expr = NULL;
		MatchType matchtype;
		char *spamf_matchtype;

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

		R_SAFE(read_data(fd, &type, sizeof(type)));
		R_SAFE(read_data(fd, &subtype, sizeof(subtype)));
		R_SAFE(read_str(fd, &usermask));
		R_SAFE(read_str(fd, &hostmask));
		R_SAFE(read_str(fd, &reason));
		R_SAFE(read_str(fd, &setby));
		R_SAFE(read_data(fd, &expire_at, sizeof(expire_at)));
		R_SAFE(read_data(fd, &set_at, sizeof(set_at)));
		R_SAFE(read_str(fd, &spamf_check));

		if(!strcmp(spamf_check, "SPAMF")) {
			spamf = 1;
			R_SAFE(read_data(fd, &spamf_action, sizeof(spamf_action)));
			R_SAFE(read_str(fd, &spamf_tkl_reason));
			R_SAFE(read_data(fd, &spamf_tkl_duration, sizeof(spamf_tkl_duration)));
			R_SAFE(read_str(fd, &spamf_expr));
			R_SAFE(read_data(fd, &matchtype, sizeof(matchtype)));
		}

		tkltype = malloc(sizeof(char) * 2);
		tklflag = tkl_typetochar(type);
		tkltype[0] = tklflag;
		tkltype[1] = '\0';

		// Should probably not re-add if it should be expired to begin with
		if(expire_at != 0 && expire_at <= TStime()) {
			ircd_log(LOG_ERROR, "[storetkl] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason);
			sendto_realops("[storetkl] Not re-adding %c:Line '%s@%s' [%s] because it should be expired", tklflag, usermask, hostmask, reason); // Probably won't be seen ever, but just in case ;]
			rewrite++;
			free(tkltype);
			if(spamf_check) free(spamf_check);
			if(usermask) free(usermask);
			if(hostmask) free(hostmask);
			if(reason) free(reason);
			if(setby) free(setby);
			continue;
		}

		ircsnprintf(setTime, sizeof(setTime), "%li", set_at);
		ircsnprintf(expTime, sizeof(expTime), "%li", expire_at);

		if(spamf && tklflag == 'f') // Lowercase 'f' means it was added through the conf or is built-in, so let's not touch those at all
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
				if(matchtype == MATCH_PCRE_REGEX)
					spamf_matchtype = "regex";
				else
					spamf_matchtype = "simple";
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

		free(tkltype);
		if(spamf_check) free(spamf_check);
		if(usermask) free(usermask);
		if(hostmask) free(hostmask);
		if(reason) free(reason);
		if(setby) free(setby);
	}

	close(fd);

	if(num) {
		ircd_log(LOG_ERROR, "[storetkl] Re-added %d X:Lines", num);
		sendto_realops("[storetkl] Re-added %d X:Lines", num); // Probably won't be seen ever, but just in case ;]
	}

	if(rewrite) {
		ircd_log(LOG_ERROR, "[storetkl] Rewriting DB file due to %d skipped/expired X:Line%s", rewrite, (rewrite > 1 ? "s" : ""));
		sendto_realops("[storetkl] Rewriting DB file due to %d skipped/expired X:Line%s", rewrite, (rewrite > 1 ? "s" : "")); // Probably won't be seen ever, but just in case ;]
		return writeDB(NULL, 0);
	}

	return 0;
}

static inline int read_data(int fd, void *buf, size_t count) {
	if((size_t)read(fd, buf, count) < count)
		return -1;
	return 0;
}

static inline int write_data(int fd, void *buf, size_t count) {
	if((size_t)write(fd, buf, count) < count)
		return -1;
	return 0;
}

static int write_str(int fd, char *x) {
	size_t count = x ? strlen(x) : 0;
	if(write_data(fd, &count, sizeof count))
		return -1;
	if(count) {
		if(write_data(fd, x, sizeof(char) * count))
			return -1;
	}
	return 0;
}

static int read_str(int fd, char **x) {
	size_t count;
	if(read_data(fd, &count, sizeof count))
		return -1;
	if(!count) {
		*x = NULL;
		return 0;
	}

	*x = (char *)MyMalloc(sizeof(char) * count + 1);
	if(read_data(fd, *x, sizeof(char) * count)) {
		MyFree(*x);
		*x = NULL;
		return -1;
	}
	(*x)[count] = 0;

	return 0;
}
