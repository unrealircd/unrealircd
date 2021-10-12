/*
 * Stores channel settings for +P channels in a .db file
 * (C) Copyright 2019 Syzop, Gottem and the UnrealIRCd team
 * License: GPLv2
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"channeldb",
	"1.0",
	"Stores and retrieves channel settings for persistent (+P) channels",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Database version */
#define CHANNELDB_VERSION 100
/* Save channels to file every <this> seconds */
#define CHANNELDB_SAVE_EVERY 300
/* The very first save after boot, apply this delta, this
 * so we don't coincide with other (potentially) expensive
 * I/O events like saving tkldb.
 */
#define CHANNELDB_SAVE_EVERY_DELTA -15

#define MAGIC_CHANNEL_START	0x11111111
#define MAGIC_CHANNEL_END	0x22222222

#ifdef DEBUGMODE
 #define BENCHMARK
#endif

#define WARN_WRITE_ERROR(fname) \
	do { \
		sendto_realops_and_log("[channeldb] Error writing to temporary database file " \
		                       "'%s': %s (DATABASE NOT SAVED)", \
		                       fname, unrealdb_get_error_string()); \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
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
void channeldb_moddata_free(ModData *md);
void setcfg(struct cfgstruct *cfg);
void freecfg(struct cfgstruct *cfg);
int channeldb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int channeldb_config_posttest(int *errs);
int channeldb_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(write_channeldb_evt);
int write_channeldb(void);
int write_channel_entry(UnrealDB *db, const char *tmpfname, Channel *channel);
int read_channeldb(void);
static void set_channel_mode(Channel *channel, char *modes, char *parameters);

/* Global variables */
static uint32_t channeldb_version = CHANNELDB_VERSION;
static struct cfgstruct cfg;
static struct cfgstruct test;

static long channeldb_next_event = 0;

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	memset(&test, 0, sizeof(test));
	setcfg(&test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, channeldb_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, channeldb_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	/* We must unload early, when all channel modes and such are still in place: */
	ModuleSetOptions(modinfo->handle, MOD_OPT_UNLOAD_PRIORITY, -99999999);

	LoadPersistentLong(modinfo, channeldb_next_event);

	setcfg(&cfg);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, channeldb_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!channeldb_next_event)
	{
		/* If this is the first time that our module is loaded, then read the database. */
		if (!read_channeldb())
		{
			char fname[512];
			snprintf(fname, sizeof(fname), "%s.corrupt", cfg.database);
			if (rename(cfg.database, fname) == 0)
				config_warn("[channeldb] Existing database renamed to %s and starting a new one...", fname);
			else
				config_warn("[channeldb] Failed to rename database from %s to %s: %s", cfg.database, fname, strerror(errno));
		}
		channeldb_next_event = TStime() + CHANNELDB_SAVE_EVERY + CHANNELDB_SAVE_EVERY_DELTA;
	}
	EventAdd(modinfo->handle, "channeldb_write_channeldb", write_channeldb_evt, NULL, 1000, 0);
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	if (loop.ircd_terminating)
		write_channeldb();
	freecfg(&test);
	freecfg(&cfg);
	SavePersistentLong(modinfo, channeldb_next_event);
	return MOD_SUCCESS;
}

void channeldb_moddata_free(ModData *md)
{
	if (md->i)
		md->i = 0;
}

void setcfg(struct cfgstruct *cfg)
{
	// Default: data/channel.db
	safe_strdup(cfg->database, "channel.db");
	convert_to_absolute_path(&cfg->database, PERMDATADIR);
}

void freecfg(struct cfgstruct *cfg)
{
	safe_free(cfg->database);
	safe_free(cfg->db_secret);
}

int channeldb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::channeldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "channeldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: blank set::channeldb::%s without value", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		} else
		if (!strcmp(cep->ce_varname, "database"))
		{
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
			safe_strdup(test.database, cep->ce_vardata);
		} else
		if (!strcmp(cep->ce_varname, "db-secret"))
		{
			char *err;
			if ((err = unrealdb_test_secret(cep->ce_vardata)))
			{
				config_error("%s:%i: set::channeldb::db-secret: %s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, err);
				errors++;
				continue;
			}
			safe_strdup(test.db_secret, cep->ce_vardata);
		} else
		{
			config_error("%s:%i: unknown directive set::channeldb::%s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int channeldb_config_posttest(int *errs)
{
	int errors = 0;
	char *errstr;

	if (test.database && ((errstr = unrealdb_test_db(test.database, test.db_secret))))
	{
		config_error("[channeldb] %s", errstr);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int channeldb_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::channeldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "channeldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "database"))
			safe_strdup(cfg.database, cep->ce_vardata);
		else if (!strcmp(cep->ce_varname, "db-secret"))
			safe_strdup(cfg.db_secret, cep->ce_vardata);
	}
	return 1;
}

EVENT(write_channeldb_evt)
{
	if (channeldb_next_event > TStime())
		return;
	channeldb_next_event = TStime() + CHANNELDB_SAVE_EVERY;
	write_channeldb();
}

int write_channeldb(void)
{
	char tmpfname[512];
	UnrealDB *db;
	Channel *channel;
	int cnt = 0;
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

	W_SAFE(unrealdb_write_int32(db, channeldb_version));

	/* First, count +P channels and write the count to the database */
	for (channel = channels; channel; channel=channel->nextch)
		if (has_channel_mode(channel, 'P'))
			cnt++;
	W_SAFE(unrealdb_write_int64(db, cnt));

	for (channel = channels; channel; channel=channel->nextch)
	{
		/* We only care about +P (persistent) channels */
		if (has_channel_mode(channel, 'P'))
		{
			if (!write_channel_entry(db, tmpfname, channel))
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
		sendto_realops_and_log("[channeldb] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
		return 0;
	}
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	config_status("[channeldb] Benchmark: SAVE DB: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
	return 1;
}

int write_listmode(UnrealDB *db, const char *tmpfname, Ban *lst)
{
	Ban *l;
	int cnt = 0;

	/* First count and write the list count */
	for (l = lst; l; l = l->next)
		cnt++;
	W_SAFE(unrealdb_write_int32(db, cnt));

	for (l = lst; l; l = l->next)
	{
		/* The entry, setby, seton */
		W_SAFE(unrealdb_write_str(db, l->banstr));
		W_SAFE(unrealdb_write_str(db, l->who));
		W_SAFE(unrealdb_write_int64(db, l->when));
	}
	return 1;
}

int write_channel_entry(UnrealDB *db, const char *tmpfname, Channel *channel)
{
	W_SAFE(unrealdb_write_int32(db, MAGIC_CHANNEL_START));
	/* Channel name */
	W_SAFE(unrealdb_write_str(db, channel->chname));
	/* Channel creation time */
	W_SAFE(unrealdb_write_int64(db, channel->creationtime));
	/* Topic (topic, setby, seton) */
	W_SAFE(unrealdb_write_str(db, channel->topic));
	W_SAFE(unrealdb_write_str(db, channel->topic_nick));
	W_SAFE(unrealdb_write_int64(db, channel->topic_time));
	/* Basic channel modes (eg: +sntkl key 55) */
	channel_modes(&me, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel);
	W_SAFE(unrealdb_write_str(db, modebuf));
	W_SAFE(unrealdb_write_str(db, parabuf));
	/* Mode lock */
	W_SAFE(unrealdb_write_str(db, channel->mode_lock));
	/* List modes (bans, exempts, invex) */
	if (!write_listmode(db, tmpfname, channel->banlist))
		return 0;
	if (!write_listmode(db, tmpfname, channel->exlist))
		return 0;
	if (!write_listmode(db, tmpfname, channel->invexlist))
		return 0;
	W_SAFE(unrealdb_write_int32(db, MAGIC_CHANNEL_END));
	return 1;
}

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[channeldb] Read error from database file '%s' (possible corruption): %s", cfg.database, unrealdb_get_error_string()); \
			if (e) \
			{ \
				safe_free(e->banstr); \
				safe_free(e->who); \
				safe_free(e); \
			} \
			return 0; \
		} \
	} while(0)

int read_listmode(UnrealDB *db, Ban **lst)
{
	uint32_t total;
	uint64_t when;
	int i;
	Ban *e = NULL;

	R_SAFE(unrealdb_read_int32(db, &total));

	for (i = 0; i < total; i++)
	{
		e = safe_alloc(sizeof(Ban));
		R_SAFE(unrealdb_read_str(db, &e->banstr));
		R_SAFE(unrealdb_read_str(db, &e->who));
		R_SAFE(unrealdb_read_int64(db, &when));
		e->when = when;
		e->next = *lst;
		*lst = e;
	}

	return 1;
}
#undef R_SAFE

#define FreeChannelEntry() \
 	do { \
		/* Some of these might be NULL */ \
		safe_free(chname); \
		safe_free(topic); \
		safe_free(topic_nick); \
		safe_free(modes1); \
		safe_free(modes2); \
		safe_free(mode_lock); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[channeldb] Read error from database file '%s' (possible corruption): %s", cfg.database, unrealdb_get_error_string()); \
			unrealdb_close(db); \
			FreeChannelEntry(); \
			return 0; \
		} \
	} while(0)

int read_channeldb(void)
{
	UnrealDB *db;
	uint32_t version;
	int added = 0;
	int i;
	uint64_t count = 0;
	uint32_t magic;
	// Variables for the channels
	// Some of them need to be declared and NULL initialised early due to the macro FreeChannelEntry() being used by R_SAFE() on error
	char *chname = NULL;
	uint64_t creationtime = 0;
	char *topic = NULL;
	char *topic_nick = NULL;
	uint64_t topic_time = 0;
	char *modes1 = NULL;
	char *modes2 = NULL;
	char *mode_lock = NULL;
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
			config_warn("[channeldb] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else
		if (unrealdb_get_error_code() == UNREALDB_ERROR_NOTCRYPTED)
		{
			/* Re-open as unencrypted */
			db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, NULL);
			if (!db)
			{
				/* This should actually never happen, unless some weird I/O error */
				config_warn("[channeldb] Unable to open the database file '%s': %s", cfg.database, unrealdb_get_error_string());
				return 0;
			}
		} else
		{
			config_warn("[channeldb] Unable to open the database file '%s' for reading: %s", cfg.database, unrealdb_get_error_string());
			return 0;
		}
	}

	R_SAFE(unrealdb_read_int32(db, &version));
	if (version > channeldb_version)
	{
		config_warn("[channeldb] Database '%s' has a wrong version: expected it to be <= %u but got %u instead", cfg.database, channeldb_version, version);
		unrealdb_close(db);
		return 0;
	}

	R_SAFE(unrealdb_read_int64(db, &count));

	for (i=1; i <= count; i++)
	{
		// Variables
		chname = NULL;
		creationtime = 0;
		topic = NULL;
		topic_nick = NULL;
		topic_time = 0;
		modes1 = NULL;
		modes2 = NULL;
		mode_lock = NULL;
		
		Channel *channel;
		R_SAFE(unrealdb_read_int32(db, &magic));
		if (magic != MAGIC_CHANNEL_START)
		{
			config_error("[channeldb] Corrupt database (%s) - channel magic start is 0x%x. Further reading aborted.", cfg.database, magic);
			break;
		}
		R_SAFE(unrealdb_read_str(db, &chname));
		R_SAFE(unrealdb_read_int64(db, &creationtime));
		R_SAFE(unrealdb_read_str(db, &topic));
		R_SAFE(unrealdb_read_str(db, &topic_nick));
		R_SAFE(unrealdb_read_int64(db, &topic_time));
		R_SAFE(unrealdb_read_str(db, &modes1));
		R_SAFE(unrealdb_read_str(db, &modes2));
		R_SAFE(unrealdb_read_str(db, &mode_lock));
		/* If we got this far, we can create/initialize the channel with the above */
		channel = get_channel(&me, chname, CREATE);
		channel->creationtime = creationtime;
		safe_strdup(channel->topic, topic);
		safe_strdup(channel->topic_nick, topic_nick);
		channel->topic_time = topic_time;
		safe_strdup(channel->mode_lock, mode_lock);
		set_channel_mode(channel, modes1, modes2);
		R_SAFE(read_listmode(db, &channel->banlist));
		R_SAFE(read_listmode(db, &channel->exlist));
		R_SAFE(read_listmode(db, &channel->invexlist));
		R_SAFE(unrealdb_read_int32(db, &magic));
		FreeChannelEntry();
		added++;
		if (magic != MAGIC_CHANNEL_END)
		{
			config_error("[channeldb] Corrupt database (%s) - channel magic end is 0x%x. Further reading aborted.", cfg.database, magic);
			break;
		}
	}

	unrealdb_close(db);

	if (added)
		sendto_realops_and_log("[channeldb] Added %d persistent channels (+P)", added);
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "[channeldb] Benchmark: LOAD DB: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
	return 1;
}
#undef FreeChannelEntry
#undef R_SAFE

static void set_channel_mode(Channel *channel, char *modes, char *parameters)
{
	char buf[512];
	char *p, *param;
	int myparc = 1, i;
	char *myparv[64];

	memset(&myparv, 0, sizeof(myparv));
	myparv[0] = raw_strdup(modes);

	strlcpy(buf, parameters, sizeof(buf));
	for (param = strtoken(&p, buf, " "); param; param = strtoken(&p, NULL, " "))
		myparv[myparc++] = raw_strdup(param);
	myparv[myparc] = NULL;

	SetULine(&me); // hack for crash.. set ulined so no access checks.
	do_mode(channel, &me, NULL, myparc, myparv, 0, 0);
	ClearULine(&me); // and clear it again..

	for (i = 0; i < myparc; i++)
		safe_free(myparv[i]);
}
// FIXME: move above function to m_mode and make efunc, available for all modules anyway
