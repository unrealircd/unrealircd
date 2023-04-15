/*
 * Stores WHOWAS history in a .db file
 * (C) Copyright 2023 Syzop
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"whowasdb",
	"1.0",
	"Stores and retrieves WHOWAS history",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Our header */
#define WHOWASDB_HEADER		0x57484F57
/* Database version */
#define WHOWASDB_VERSION 100
/* Save whowas of users to file every <this> seconds */
#define WHOWASDB_SAVE_EVERY 300
/* The very first save after boot, apply this delta, this
 * so we don't coincide with other (potentially) expensive
 * I/O events like saving tkldb.
 */
#define WHOWASDB_SAVE_EVERY_DELTA -60

#define MAGIC_WHOWASDB_START	0x11111111
#define MAGIC_WHOWASDB_END		0x22222222

// #undef BENCHMARK

#define WARN_WRITE_ERROR(fname) \
	do { \
		unreal_log(ULOG_ERROR, "whowasdb", "WHOWASDB_FILE_WRITE_ERROR", NULL, \
			   "[whowasdb] Error writing to temporary database file $filename: $system_error", \
			   log_data_string("filename", fname), \
			   log_data_string("system_error", unrealdb_get_error_string())); \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
			WARN_WRITE_ERROR(tmpfname); \
			unrealdb_close(db); \
			return 0; \
		} \
	} while(0)

#define W_SAFE_PROPERTY(db, x, y) \
	do { \
		if (x && y && (!unrealdb_write_str(db, x) || !unrealdb_write_str(db, y))) \
		{ \
			WARN_WRITE_ERROR(tmpfname); \
			unrealdb_close(db); \
			return 0; \
		} \
	} while(0)

#define IsMDErr(x, y, z) \
	do { \
		if (!(x)) { \
			config_error("A critical error occurred when registering ModData for %s: %s", MOD_HEADER.name, ModuleGetErrorStr((z)->handle)); \
			return MOD_FAILED; \
		} \
	} while(0)

/* Structs */
struct cfgstruct {
	char *database;
	char *db_secret;
};

/* Forward declarations */
void whowasdb_moddata_free(ModData *md);
void setcfg(struct cfgstruct *cfg);
void freecfg(struct cfgstruct *cfg);
int whowasdb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int whowasdb_config_posttest(int *errs);
int whowasdb_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(write_whowasdb_evt);
int write_whowasdb(void);
int write_whowas_entry(UnrealDB *db, const char *tmpfname, WhoWas *e);
int read_whowasdb(void);

/* External variables */
extern WhoWas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
extern WhoWas MODVAR *WHOWASHASH[WHOWAS_HASH_TABLE_SIZE];
extern MODVAR int whowas_next;

/* Global variables */
static uint32_t whowasdb_version = WHOWASDB_VERSION;
static struct cfgstruct cfg;
static struct cfgstruct test;

static long whowasdb_next_event = 0;

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	memset(&test, 0, sizeof(test));
	setcfg(&test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, whowasdb_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, whowasdb_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	LoadPersistentLong(modinfo, whowasdb_next_event);

	setcfg(&cfg);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, whowasdb_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!whowasdb_next_event)
	{
		/* If this is the first time that our module is loaded, then read the database. */
		if (!read_whowasdb())
		{
			char fname[512];
			snprintf(fname, sizeof(fname), "%s.corrupt", cfg.database);
			if (rename(cfg.database, fname) == 0)
				config_warn("[whowasdb] Existing database renamed to %s and starting a new one...", fname);
			else
				config_warn("[whowasdb] Failed to rename database from %s to %s: %s", cfg.database, fname, strerror(errno));
		}
		whowasdb_next_event = TStime() + WHOWASDB_SAVE_EVERY + WHOWASDB_SAVE_EVERY_DELTA;
	}
	EventAdd(modinfo->handle, "whowasdb_write_whowasdb", write_whowasdb_evt, NULL, 1000, 0);
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	if (loop.terminating)
		write_whowasdb();
	freecfg(&test);
	freecfg(&cfg);
	SavePersistentLong(modinfo, whowasdb_next_event);
	return MOD_SUCCESS;
}

void whowasdb_moddata_free(ModData *md)
{
	if (md->i)
		md->i = 0;
}

void setcfg(struct cfgstruct *cfg)
{
	// Default: data/whowas.db
	safe_strdup(cfg->database, "whowas.db");
	convert_to_absolute_path(&cfg->database, PERMDATADIR);
}

void freecfg(struct cfgstruct *cfg)
{
	safe_free(cfg->database);
	safe_free(cfg->db_secret);
}

int whowasdb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::whowasdb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "whowasdb"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank set::whowasdb::%s without value", cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "database"))
		{
			convert_to_absolute_path(&cep->value, PERMDATADIR);
			safe_strdup(test.database, cep->value);
		} else
		if (!strcmp(cep->name, "db-secret"))
		{
			const char *err;
			if ((err = unrealdb_test_secret(cep->value)))
			{
				config_error("%s:%i: set::whowasdb::db-secret: %s", cep->file->filename, cep->line_number, err);
				errors++;
				continue;
			}
			safe_strdup(test.db_secret, cep->value);
		} else
		{
			config_error("%s:%i: unknown directive set::whowasdb::%s", cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int whowasdb_config_posttest(int *errs)
{
	int errors = 0;
	char *errstr;

	if (test.database && ((errstr = unrealdb_test_db(test.database, test.db_secret))))
	{
		config_error("[whowasdb] %s", errstr);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int whowasdb_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::whowasdb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "whowasdb"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database"))
			safe_strdup(cfg.database, cep->value);
		else if (!strcmp(cep->name, "db-secret"))
			safe_strdup(cfg.db_secret, cep->value);
	}
	return 1;
}

EVENT(write_whowasdb_evt)
{
	if (whowasdb_next_event > TStime())
		return;
	whowasdb_next_event = TStime() + WHOWASDB_SAVE_EVERY;
	write_whowasdb();
}

int count_whowas_and_user_entries(void)
{
	int i;
	int cnt = 0;
	Client *client;

	for (i=0; i < NICKNAMEHISTORYLENGTH; i++)
	{
		WhoWas *e = &WHOWAS[i];
		if (e->name)
			cnt++;
	}

	list_for_each_entry(client, &client_list, client_node)
		if (IsUser(client))
			cnt++;

	return cnt;
}

int write_whowasdb(void)
{
	char tmpfname[512];
	UnrealDB *db;
	WhoWas *e;
	Client *client;
	int cnt, i;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	// Write to a tempfile first, then rename it if everything succeeded
	snprintf(tmpfname, sizeof(tmpfname), "%s.%x.tmp", cfg.database, getrandom32());
	db = unrealdb_open(tmpfname, UNREALDB_MODE_WRITE, cfg.db_secret);
	if (!db)
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	W_SAFE(unrealdb_write_int32(db, WHOWASDB_HEADER));
	W_SAFE(unrealdb_write_int32(db, whowasdb_version));

	cnt = count_whowas_and_user_entries();
	W_SAFE(unrealdb_write_int64(db, cnt));

	for (i=0; i < NICKNAMEHISTORYLENGTH; i++)
	{
		WhoWas *e = &WHOWAS[i];
		if (e->name)
		{
			if (!write_whowas_entry(db, tmpfname, e))
				return 0;
		}
	}

	/* Add all the currently connected users to WHOWAS history (as if they left just now) */
	list_for_each_entry(client, &client_list, client_node)
	{
		if (IsUser(client))
		{
			WhoWas *e = safe_alloc(sizeof(WhoWas));
			int ret;

			create_whowas_entry(client, e, WHOWAS_EVENT_SERVER_TERMINATING);
			ret = write_whowas_entry(db, tmpfname, e);
			free_whowas_fields(e);
			safe_free(e);

			if (ret == 0)
				return 0;
		}
	}


	// Everything seems to have gone well, attempt to close and rename the tempfile
	if (!unrealdb_close(db))
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

#ifdef _WIN32
	/* The rename operation cannot be atomic on Windows as it will cause a "file exists" error */
	unlink(cfg.database);
#endif
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("[whowasdb] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
		return 0;
	}
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	config_status("[whowasdb] Benchmark: SAVE DB: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
	return 1;
}

int write_whowas_entry(UnrealDB *db, const char *tmpfname, WhoWas *e)
{
	char logontime[64];
	char logofftime[64];
	char event[16];

	snprintf(logontime, sizeof(logontime), "%lld", (long long)e->logon);
	snprintf(logofftime, sizeof(logofftime), "%lld", (long long)e->logoff);
	snprintf(event, sizeof(event), "%d", e->event);

	W_SAFE(unrealdb_write_int32(db, MAGIC_WHOWASDB_START));
	W_SAFE_PROPERTY(db, "nick", e->name);
	W_SAFE_PROPERTY(db, "logontime", logontime);
	W_SAFE_PROPERTY(db, "logofftime", logofftime);
	W_SAFE_PROPERTY(db, "event", event);
	W_SAFE_PROPERTY(db, "username", e->username);
	W_SAFE_PROPERTY(db, "hostname", e->hostname);
	W_SAFE_PROPERTY(db, "ip", e->ip);
	W_SAFE_PROPERTY(db, "realname", e->realname);
	W_SAFE_PROPERTY(db, "server", e->servername);
	W_SAFE_PROPERTY(db, "virthost", e->virthost);
	W_SAFE_PROPERTY(db, "account", e->account);
	W_SAFE_PROPERTY(db, "end", "");
	W_SAFE(unrealdb_write_int32(db, MAGIC_WHOWASDB_END));
	return 1;
}

#define FreeWhowasEntry() \
 	do { \
		/* Some of these might be NULL */ \
		safe_free(key); \
		safe_free(value); \
		safe_free(nick); \
		safe_free(username); \
		safe_free(hostname); \
		safe_free(ip); \
		safe_free(realname); \
		logontime = logofftime = 0; \
		event = 0; \
		safe_free(server); \
		safe_free(virthost); \
		safe_free(account); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[whowasdb] Read error from database file '%s' (possible corruption): %s", cfg.database, unrealdb_get_error_string()); \
			unrealdb_close(db); \
			FreeWhowasEntry(); \
			return 0; \
		} \
	} while(0)

int read_whowasdb(void)
{
	UnrealDB *db;
	uint32_t version;
	int added = 0;
	int i;
	uint64_t count = 0;
	uint32_t magic;
	char *key = NULL;
	char *value = NULL;
	char *nick = NULL;
	char *username = NULL;
	char *hostname = NULL;
	char *ip = NULL;
	char *realname = NULL;
	long long logontime = 0;
	long long logofftime = 0;
	int event = 0;
	char *server = NULL;
	char *virthost = NULL;
	char *account = NULL;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, cfg.db_secret);
	if (!db)
	{
		if (unrealdb_get_error_code() == UNREALDB_ERROR_FILENOTFOUND)
		{
			/* Database does not exist. Could be first boot */
			config_warn("[whowasdb] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else
		if (unrealdb_get_error_code() == UNREALDB_ERROR_NOTCRYPTED)
		{
			/* Re-open as unencrypted */
			db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, NULL);
			if (!db)
			{
				/* This should actually never happen, unless some weird I/O error */
				config_warn("[whowasdb] Unable to open the database file '%s': %s", cfg.database, unrealdb_get_error_string());
				return 0;
			}
		} else
		{
			config_warn("[whowasdb] Unable to open the database file '%s' for reading: %s", cfg.database, unrealdb_get_error_string());
			return 0;
		}
	}

	R_SAFE(unrealdb_read_int32(db, &version));
	if (version != WHOWASDB_HEADER)
	{
		config_warn("[whowasdb] Database '%s' is not a whowas db (incorrect header)", cfg.database);
		unrealdb_close(db);
		return 0;
	}
	R_SAFE(unrealdb_read_int32(db, &version));
	if (version > whowasdb_version)
	{
		config_warn("[whowasdb] Database '%s' has a wrong version: expected it to be <= %u but got %u instead", cfg.database, whowasdb_version, version);
		unrealdb_close(db);
		return 0;
	}

	R_SAFE(unrealdb_read_int64(db, &count));

	for (i=1; i <= count; i++)
	{
		// Variables
		key = value = NULL;
		nick = username = hostname = ip = realname = virthost = account = server = NULL;
		logontime = logofftime = 0;
		event = 0;

		R_SAFE(unrealdb_read_int32(db, &magic));
		if (magic != MAGIC_WHOWASDB_START)
		{
			config_error("[whowasdb] Corrupt database (%s) - whowasdb magic start is 0x%x. Further reading aborted.", cfg.database, magic);
			break;
		}
		while(1)
		{
			R_SAFE(unrealdb_read_str(db, &key));
			R_SAFE(unrealdb_read_str(db, &value));
			if (!strcmp(key, "nick"))
			{
				nick = value;
			} else
			if (!strcmp(key, "username"))
			{
				username = value;
			} else
			if (!strcmp(key, "hostname"))
			{
				hostname = value;
			} else
			if (!strcmp(key, "ip"))
			{
				ip = value;
			} else
			if (!strcmp(key, "realname"))
			{
				realname = value;
			} else
			if (!strcmp(key, "logontime"))
			{
				logontime = atoll(value);
				safe_free(value);
			} else
			if (!strcmp(key, "logofftime"))
			{
				logofftime = atoll(value);
				safe_free(value);
			} else
			if (!strcmp(key, "event"))
			{
				event = atoi(value);
				if ((event < WHOWAS_LOWEST_EVENT) || (event > WHOWAS_HIGHEST_EVENT))
					event = WHOWAS_EVENT_QUIT; /* safety */
				safe_free(value);
			} else
			if (!strcmp(key, "server"))
			{
				server = value;
			} else
			if (!strcmp(key, "virthost"))
			{
				virthost = value;
			} else
			if (!strcmp(key, "account"))
			{
				account = value;
			} else
			if (!strcmp(key, "end"))
			{
				safe_free(key);
				safe_free(value);
				break; /* DONE! */
			} else
			{
				safe_free(value);
				/* just don't do anything with it -- ignored (future compatible).
				 * FALLTHROUGH...
				 */
			}
			safe_free(key);
		}
		R_SAFE(unrealdb_read_int32(db, &magic));
		if (magic != MAGIC_WHOWASDB_END)
		{
			config_error("[whowasdb] Corrupt database (%s) - whowasdb magic end is 0x%x. Further reading aborted.", cfg.database, magic);
			FreeWhowasEntry();
			break;
		}

		if (nick && username && hostname && realname)
		{
			WhoWas *e = &WHOWAS[whowas_next];
			if (e->hashv != -1)
				free_whowas_fields(e);
			/* Set values */
			unreal_log(ULOG_DEBUG, "whowasdb", "WHOWASDB_READ_RECORD", NULL,
			           "[whowasdb] Adding '$nick'...",
			           log_data_string("nick", nick));
			e->hashv = hash_whowas_name(nick);
			e->logon = logontime;
			e->logoff = logofftime;
			e->event = event;
			safe_strdup(e->name, nick);
			safe_strdup(e->username, username);
			safe_strdup(e->hostname, hostname);
			safe_strdup(e->ip, ip);
			if (virthost)
				safe_strdup(e->virthost, virthost);
			else
				safe_strdup(e->virthost, "");
			e->servername = find_or_add(server); /* scache */
			safe_strdup(e->realname, realname);
			safe_strdup(e->account, account);
			e->online = NULL;
			/* Server is special - scache shit */
			/* Add to hash table */
			add_whowas_to_list(&WHOWASHASH[e->hashv], e);
			/* And advance pointer (well, integer) */
			whowas_next++;
			if (whowas_next == NICKNAMEHISTORYLENGTH)
				whowas_next = 0;
		}

		FreeWhowasEntry();
		added++;
	}

	unrealdb_close(db);

	if (added)
		config_status("[whowasdb] Added %d WHOWAS items", added);
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "whowasdb", "WHOWASDB_BENCHMARK", NULL,
	           "[whowasdb] Benchmark: LOAD DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}
#undef FreeWhowasEntry
#undef R_SAFE
