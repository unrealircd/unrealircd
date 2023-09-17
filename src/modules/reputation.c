/*
 * reputation - Provides a scoring system for "known users".
 * (C) Copyright 2015-2019 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2 or later
 *
 * How this works is simple:
 * Every 5 minutes the IP address of all the connected users receive
 * a point. Registered users receive 2 points every 5 minutes.
 * The total reputation score is then later used, by other modules, for
 * example to make decisions such as to reject or allow a user if the
 * server is under attack.
 * The reputation scores are saved in a database. By default this file
 * is data/reputation.db (often ~/unrealircd/data/reputation.db).
 *
 * See also https://www.unrealircd.org/docs/Connthrottle
 */

#include "unrealircd.h"

#define REPUTATION_VERSION "1.2"

/* Change to #define to benchmark. Note that this will add random
 * reputation entries so should never be used on production servers!!!
 */
#undef BENCHMARK
#undef TEST

/* Benchmark results (2GHz Xeon Skylake, compiled with -O2, Linux):
 * 10k random IP's with various expire times:
 * - load db:  23 ms
 * - expiry:    1 ms
 * - save db:   7 ms
 * 100k random IP's with various expire times:
 * - load db: 103 ms
 * - expiry:   10 ms
 * - save db:  32 ms
 * So, even for 100,000 unique IP's, the initial load of the database
 * would delay the UnrealIRCd boot process only for 0.1 second.
 * The writing of the db, which happens every 5 minutes, for such
 * amount of IP's takes 32ms (0.03 second).
 * Of course, exact figures will depend on the storage and cache.
 * That being said, the file for 100k random IP's is slightly under
 * 3MB, so not big, which likely means the timing will be similar
 * for a broad number of (storage) systems.
 */
 
#ifndef TEST
 #define BUMP_SCORE_EVERY	300
 #define DELETE_OLD_EVERY	605
 #define SAVE_DB_EVERY		902
#else
 #define BUMP_SCORE_EVERY 	3
 #define DELETE_OLD_EVERY	3
 #define SAVE_DB_EVERY		3
#endif

#ifndef CALLBACKTYPE_REPUTATION_STARTTIME
 #define CALLBACKTYPE_REPUTATION_STARTTIME 5
#endif

ModuleHeader MOD_HEADER
  = {
	"reputation",
	REPUTATION_VERSION,
	"Known IP's scoring system",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Defines */

#define MAXEXPIRES 10

#define REPUTATION_SCORE_CAP 10000

#define UPDATE_SCORE_MARGIN 1

#define REPUTATION_HASH_TABLE_SIZE 2048

#define Reputation(client)	moddata_client(client, reputation_md).l

#define WARN_WRITE_ERROR(fname) \
	do { \
		unreal_log(ULOG_ERROR, "reputation", "REPUTATION_FILE_WRITE_ERROR", NULL, \
			   "[reputation] Error writing to temporary database file $filename: $system_error", \
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


/* Definitions (structs, etc.) */

struct cfgstruct {
	int expire_score[MAXEXPIRES];
	long expire_time[MAXEXPIRES];
	char *database;
	char *db_secret;
};

typedef struct ReputationEntry ReputationEntry;

struct ReputationEntry {
	ReputationEntry *prev, *next;
	unsigned short score; /**< score for the user */
	long last_seen; /**< user last seen (unix timestamp) */
	int marker; /**< internal marker, not written to db */
	char ip[1]; /*< ip address */
};

/* Global variables */

static struct cfgstruct cfg; /**< Current configuration */
static struct cfgstruct test; /**< Testing configuration (not active yet) */
long reputation_starttime = 0;
long reputation_writtentime = 0;

static ReputationEntry *ReputationHashTable[REPUTATION_HASH_TABLE_SIZE];
static char siphashkey_reputation[SIPHASH_KEY_LENGTH];

static ModuleInfo ModInf;

ModDataInfo *reputation_md; /* Module Data structure which we acquire */

/* Forward declarations */
void reputation_md_free(ModData *m);
const char *reputation_md_serialize(ModData *m);
void reputation_md_unserialize(const char *str, ModData *m);
void reputation_config_setdefaults(struct cfgstruct *cfg);
void reputation_free_config(struct cfgstruct *cfg);
CMD_FUNC(reputation_cmd);
CMD_FUNC(reputationunperm);
int reputation_whois(Client *client, Client *target, NameValuePrioList **list);
int reputation_set_on_connect(Client *client);
int reputation_pre_lconnect(Client *client);
int reputation_ip_change(Client *client, const char *oldip);
int reputation_connect_extinfo(Client *client, NameValuePrioList **list);
int reputation_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reputation_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int reputation_config_posttest(int *errs);
static uint64_t hash_reputation_entry(const char *ip);
ReputationEntry *find_reputation_entry(const char *ip);
void add_reputation_entry(ReputationEntry *e);
EVENT(delete_old_records);
EVENT(add_scores);
EVENT(reputation_save_db_evt);
int reputation_load_db(void);
int reputation_save_db(void);
int reputation_starttime_callback(void);
void _ban_act_set_reputation(Client *client, BanAction *action);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	memcpy(&ModInf, modinfo, modinfo->size);
	memset(&cfg, 0, sizeof(cfg));
	memset(&test, 0, sizeof(cfg));
	reputation_config_setdefaults(&test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, reputation_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, reputation_config_posttest);
	CallbackAdd(modinfo->handle, CALLBACKTYPE_REPUTATION_STARTTIME, reputation_starttime_callback);
	EfunctionAddVoid(modinfo->handle, EFUNC_BAN_ACT_SET_REPUTATION, _ban_act_set_reputation);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);

	memset(&ReputationHashTable, 0, sizeof(ReputationHashTable));
	siphash_generate_key(siphashkey_reputation);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "reputation";
	mreq.free = reputation_md_free;
	mreq.serialize = reputation_md_serialize;
	mreq.unserialize = reputation_md_unserialize;
	mreq.sync = 0; /* local! */
	mreq.type = MODDATATYPE_CLIENT;
	reputation_md = ModDataAdd(modinfo->handle, mreq);
	if (!reputation_md)
		abort();

	reputation_config_setdefaults(&cfg);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, reputation_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, reputation_whois);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, reputation_set_on_connect);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, 0, reputation_ip_change);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 2000000000, reputation_pre_lconnect); /* (prio: last) */
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, -1000000000, reputation_set_on_connect); /* (prio: near-first) */
	HookAdd(modinfo->handle, HOOKTYPE_CONNECT_EXTINFO, 0, reputation_connect_extinfo); /* (prio: near-first) */
	CommandAdd(ModInf.handle, "REPUTATION", reputation_cmd, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(ModInf.handle, "REPUTATIONUNPERM", reputationunperm, MAXPARA, CMD_USER|CMD_SERVER);
	return MOD_SUCCESS;
}

#ifdef BENCHMARK
void reputation_benchmark(int entries)
{
	char ip[64];
	int i;
	ReputationEntry *e;

	srand(1234); // fixed seed

	for (i = 0; i < entries; i++)
	{
		ReputationEntry *e = safe_alloc(sizeof(ReputationEntry) + 64);
		snprintf(e->ip, 63, "%d.%d.%d.%d", rand()%255, rand()%255, rand()%255, rand()%255);
		e->score = rand()%255 + 1;
		e->last_seen = TStime();
		if (find_reputation_entry(e->ip))
		{
			safe_free(e);
			continue;
		}
		add_reputation_entry(e);
	}
}
#endif
MOD_LOAD()
{
	reputation_load_db();
	if (reputation_starttime == 0)
		reputation_starttime = TStime();
	EventAdd(ModInf.handle, "delete_old_records", delete_old_records, NULL, DELETE_OLD_EVERY*1000, 0);
	EventAdd(ModInf.handle, "add_scores", add_scores, NULL, BUMP_SCORE_EVERY*1000, 0);
	EventAdd(ModInf.handle, "reputation_save_db", reputation_save_db_evt, NULL, SAVE_DB_EVERY*1000, 0);
#ifdef BENCHMARK
	reputation_benchmark(10000);
#endif
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	if (loop.terminating)
		reputation_save_db();
	reputation_free_config(&test);
	reputation_free_config(&cfg);
	return MOD_SUCCESS;
}

void reputation_config_setdefaults(struct cfgstruct *cfg)
{
	/* data/reputation.db */
	safe_strdup(cfg->database, "reputation.db");
	convert_to_absolute_path(&cfg->database, PERMDATADIR);

	/* EXPIRES the following entries if the IP does appear for some time: */
	/* <=2 points after 1 hour */
	cfg->expire_score[0] = 2;
#ifndef TEST
	cfg->expire_time[0]   = 3600;
#else
	cfg->expire_time[0]   = 36;
#endif
	/* <=6 points after 7 days */
	cfg->expire_score[1] = 6;
	cfg->expire_time[1]   = 86400*7;
	/* ANY result that has not been seen for 30 days */
	cfg->expire_score[2] = -1;
	cfg->expire_time[2]   = 86400*30;
}

void reputation_free_config(struct cfgstruct *cfg)
{
	safe_free(cfg->database);
	safe_free(cfg->db_secret);
}

int reputation_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::reputation.. */
	if (!ce || strcmp(ce->name, "reputation"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank set::reputation::%s without value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
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
				config_error("%s:%i: set::channeldb::db-secret: %s", cep->file->filename, cep->line_number, err);
				errors++;
				continue;
			}
			safe_strdup(test.db_secret, cep->value);
		} else
		{
			config_error("%s:%i: unknown directive set::reputation::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int reputation_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::reputation.. */
	if (!ce || strcmp(ce->name, "reputation"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database"))
		{
			safe_strdup(cfg.database, cep->value);
		} else
		if (!strcmp(cep->name, "db-secret"))
		{
			safe_strdup(cfg.db_secret, cep->value);
		}
	}
	return 1;
}

int reputation_config_posttest(int *errs)
{
	int errors = 0;
	char *errstr;

	if (test.database && ((errstr = unrealdb_test_db(test.database, test.db_secret))))
	{
		config_error("[reputation] %s", errstr);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/** Parse database header and set variables appropriately */
int parse_db_header_old(char *buf)
{
	char *header=NULL, *version=NULL, *starttime=NULL, *writtentime=NULL;
	char *p=NULL;

	if (strncmp(buf, "REPDB", 5))
		return 0;

	header = strtoken(&p, buf, " ");
	if (!header)
		return 0;

	version = strtoken(&p, NULL, " ");
	if (!version || (atoi(version) != 1))
		return 0;

	starttime = strtoken(&p, NULL, " ");
	if (!starttime)
		return 0;

	writtentime = strtoken(&p, NULL, " ");
	if (!writtentime)
		return 0;

	reputation_starttime = atol(starttime);
	reputation_writtentime = atol(writtentime);

	return 1;
}

void reputation_load_db_old(void)
{
	FILE *fd;
	char buf[512], *p;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	fd = fopen(cfg.database, "r");
	if (!fd)
	{
		config_warn("WARNING: Could not open/read database '%s': %s", cfg.database, strerror(ERRNO));
		return;
	}

	memset(buf, 0, sizeof(buf));
	if (fgets(buf, 512, fd) == NULL)
	{
		config_error("WARNING: Database file corrupt ('%s')", cfg.database);
		fclose(fd);
		return;
	}

	/* Header contains: REPDB <version> <starttime> <writtentime>
	 * Where:
	 * REPDB:        Literally the string "REPDB".
	 * <version>     This is version 1 at the time of this writing.
	 * <starttime>   The time that recording of reputation started,
	 *               in other words: when this module was first loaded, ever.
	 * <writtentime> Time that the database was last written.
	 */
	if (!parse_db_header_old(buf))
	{
		config_error("WARNING: Cannot load database %s. Error reading header. "
		             "Database corrupt? Or are you downgrading from a newer "
		             "UnrealIRCd version perhaps? This is not supported.",
		             cfg.database);
		fclose(fd);
		return;
	}

	while(fgets(buf, 512, fd) != NULL)
	{
		char *ip = NULL, *score = NULL, *last_seen = NULL;
		ReputationEntry *e;

		stripcrlf(buf);
		/* Format: <ip> <score> <last seen> */
		ip = strtoken(&p, buf, " ");
		if (!ip)
			continue;
		score = strtoken(&p, NULL, " ");
		if (!score)
			continue;
		last_seen = strtoken(&p, NULL, " ");
		if (!last_seen)
			continue;

		e = safe_alloc(sizeof(ReputationEntry)+strlen(ip));
		strcpy(e->ip, ip); /* safe, see alloc above */
		e->score = atoi(score);
		e->last_seen = atol(last_seen);

		add_reputation_entry(e);
	}
	fclose(fd);

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_BENCHMARK", NULL,
	           "[reputation] Benchmark: LOAD DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
}

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[reputation] Read error from database file '%s' (possible corruption): %s", cfg.database, unrealdb_get_error_string()); \
			unrealdb_close(db); \
			safe_free(ip); \
			return 0; \
		} \
	} while(0)

int reputation_load_db_new(UnrealDB *db)
{
	uint64_t l_db_version = 0;
	uint64_t l_starttime = 0;
	uint64_t l_writtentime = 0;
	uint64_t count = 0;
	uint64_t i;
	char *ip = NULL;
	uint16_t score;
	uint64_t last_seen;
	ReputationEntry *e;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	R_SAFE(unrealdb_read_int64(db, &l_db_version)); /* reputation db version */
	if (l_db_version > 2)
	{
		config_error("[reputation] Reputation DB is of a newer version (%ld) than supported by us (%ld). "
		             "Did you perhaps downgrade your UnrealIRCd?",
		             (long)l_db_version, (long)2);
		unrealdb_close(db);
		return 0;
	}
	R_SAFE(unrealdb_read_int64(db, &l_starttime)); /* starttime of data gathering */
	R_SAFE(unrealdb_read_int64(db, &l_writtentime)); /* current time */
	R_SAFE(unrealdb_read_int64(db, &count)); /* number of entries */

	reputation_starttime = l_starttime;
	reputation_writtentime = l_writtentime;

	for (i=0; i < count; i++)
	{
		R_SAFE(unrealdb_read_str(db, &ip));
		R_SAFE(unrealdb_read_int16(db, &score));
		R_SAFE(unrealdb_read_int64(db, &last_seen));

		e = safe_alloc(sizeof(ReputationEntry)+strlen(ip));
		strcpy(e->ip, ip); /* safe, see alloc above */
		e->score = score;
		e->last_seen = last_seen;
		add_reputation_entry(e);
		safe_free(ip);
	}
	unrealdb_close(db);
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_BENCHMARK", NULL,
	           "Reputation benchmark: LOAD DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}

/** Load the reputation DB.
 * Strategy is:
 * 1) Check for the old header "REPDB 1", if so then call reputation_load_db_old().
 * 2) Otherwise, open with unrealdb routine
 * 3) If that fails due to a password provided but the file is unrealdb without password
 *    then fallback to open without a password (so users can easily upgrade to encrypted)
 */
int reputation_load_db(void)
{
	FILE *fd;
	UnrealDB *db;
	char buf[512];

	fd = fopen(cfg.database, "r");
	if (!fd)
	{
		/* Database does not exist. Could be first boot */
		config_warn("[reputation] No database present at '%s', will start a new one", cfg.database);
		return 1;
	}

	*buf = '\0';
	if (fgets(buf, sizeof(buf), fd) == NULL)
	{
		fclose(fd);
		config_warn("[reputation] Database at '%s' is 0 bytes", cfg.database);
		return 1;
	}
	fclose(fd);
	if (!strncmp(buf, "REPDB 1 ", 8))
	{
		reputation_load_db_old();
		return 1; /* not so good to always pretend succes */
	}

	/* Otherwise, it is an unrealdb, crypted or not */
	db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, cfg.db_secret);
	if (!db)
	{
		if (unrealdb_get_error_code() == UNREALDB_ERROR_FILENOTFOUND)
		{
			/* Database does not exist. Could be first boot */
			config_warn("[reputation] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else
		if (unrealdb_get_error_code() == UNREALDB_ERROR_NOTCRYPTED)
		{
			db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, NULL);
		}
		if (!db)
		{
			config_error("[reputation] Unable to open the database file '%s' for reading: %s", cfg.database, unrealdb_get_error_string());
			return 0;
		}
	}
	return reputation_load_db_new(db);
}

int reputation_save_db_old(void)
{
	FILE *fd;
	char tmpfname[512];
	int i;
	ReputationEntry *e;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	/* We write to a temporary file. Only to rename it later if everything was ok */
	snprintf(tmpfname, sizeof(tmpfname), "%s.%x.tmp", cfg.database, getrandom32());

	fd = fopen(tmpfname, "w");
	if (!fd)
	{
		config_error("ERROR: Could not open/write database '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
		return 0;
	}

	if (fprintf(fd, "REPDB 1 %lld %lld\n", (long long)reputation_starttime, (long long)TStime()) < 0)
		goto write_fail;

	for (i = 0; i < REPUTATION_HASH_TABLE_SIZE; i++)
	{
		for (e = ReputationHashTable[i]; e; e = e->next)
		{
			if (fprintf(fd, "%s %d %lld\n", e->ip, (int)e->score, (long long)e->last_seen) < 0)
			{
write_fail:
				config_error("ERROR writing to '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
				fclose(fd);
				return 0;
			}
		}
	}

	if (fclose(fd) < 0)
	{
		config_error("ERROR writing to '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
		return 0;
	}

	/* Everything went fine. We rename our temporary file to the existing
	 * DB file (will overwrite), which is more or less an atomic operation.
	 */
#ifdef _WIN32
	/* The rename operation cannot be atomic on Windows as it will cause a "file exists" error */
	unlink(cfg.database);
#endif
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("ERROR renaming '%s' to '%s': %s -- DATABASE *NOT* SAVED!!!",
			tmpfname, cfg.database, strerror(ERRNO));
		return 0;
	}

	reputation_writtentime = TStime();

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_BENCHMARK", NULL,
	           "Reputation benchmark: SAVE DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif

	return 1;
}

int reputation_save_db(void)
{
	UnrealDB *db;
	char tmpfname[512];
	int i;
	uint64_t count;
	ReputationEntry *e;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

#ifdef TEST
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_TEST", NULL, "Reputation in running in test mode. Saving DB's....");
#endif

	/* Comment this out after one or more releases (means you cannot downgrade to <=5.0.9.1 anymore) */
	if (cfg.db_secret == NULL)
		return reputation_save_db_old();

	/* We write to a temporary file. Only to rename it later if everything was ok */
	snprintf(tmpfname, sizeof(tmpfname), "%s.%x.tmp", cfg.database, getrandom32());

	db = unrealdb_open(tmpfname, UNREALDB_MODE_WRITE, cfg.db_secret);
	if (!db)
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	/* Write header */
	W_SAFE(unrealdb_write_int64(db, 2)); /* reputation db version */
	W_SAFE(unrealdb_write_int64(db, reputation_starttime)); /* starttime of data gathering */
	W_SAFE(unrealdb_write_int64(db, TStime())); /* current time */

	/* Count entries */
	count = 0;
	for (i = 0; i < REPUTATION_HASH_TABLE_SIZE; i++)
		for (e = ReputationHashTable[i]; e; e = e->next)
			count++;
	W_SAFE(unrealdb_write_int64(db, count)); /* Number of DB entries */

	/* Now write the actual individual entries: */
	for (i = 0; i < REPUTATION_HASH_TABLE_SIZE; i++)
	{
		for (e = ReputationHashTable[i]; e; e = e->next)
		{
			W_SAFE(unrealdb_write_str(db, e->ip));
			W_SAFE(unrealdb_write_int16(db, e->score));
			W_SAFE(unrealdb_write_int64(db, e->last_seen));
		}
	}

	if (!unrealdb_close(db))
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	/* Everything went fine. We rename our temporary file to the existing
	 * DB file (will overwrite), which is more or less an atomic operation.
	 */
#ifdef _WIN32
	/* The rename operation cannot be atomic on Windows as it will cause a "file exists" error */
	unlink(cfg.database);
#endif
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("ERROR renaming '%s' to '%s': %s -- DATABASE *NOT* SAVED!!!",
			tmpfname, cfg.database, strerror(ERRNO));
		return 0;
	}

	reputation_writtentime = TStime();

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_BENCHMARK", NULL,
	           "Reputation benchmark: SAVE DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}

static uint64_t hash_reputation_entry(const char *ip)
{
	return siphash(ip, siphashkey_reputation) % REPUTATION_HASH_TABLE_SIZE;
}

void add_reputation_entry(ReputationEntry *e)
{
	int hashv = hash_reputation_entry(e->ip);

	AddListItem(e, ReputationHashTable[hashv]);
}

ReputationEntry *find_reputation_entry(const char *ip)
{
	ReputationEntry *e;
	int hashv = hash_reputation_entry(ip);

	for (e = ReputationHashTable[hashv]; e; e = e->next)
		if (!strcmp(e->ip, ip))
			return e;

	return NULL;
}

int reputation_lookup_score_and_set(Client *client)
{
	char *ip = client->ip;
	ReputationEntry *e;

	Reputation(client) = 0; /* (re-)set to zero (yes, important!) */
	if (ip)
	{
		e = find_reputation_entry(ip);
		if (e)
		{
			Reputation(client) = e->score; /* SET MODDATA */
		}
	}
	return Reputation(client);
}

/** Called when the user connects.
 * Locally: very early, just after the TCP/IP connection has
 * been established, before any data.
 * Remote user: early in the HOOKTYPE_REMOTE_CONNECT hook.
 */
int reputation_set_on_connect(Client *client)
{
	reputation_lookup_score_and_set(client);
	return 0;
}

int reputation_ip_change(Client *client, const char *oldip)
{
	reputation_lookup_score_and_set(client);
	return 0;
}

int reputation_pre_lconnect(Client *client)
{
	/* User will likely be accepted. Inform other servers about the score
	 * we have for this user. For more information about this type of
	 * server to server traffic, see the reputation_server_cmd function.
	 *
	 * Note that we use reputation_lookup_score_and_set() here
	 * and not Reputation(client) because we want to RE-LOOKUP
	 * the score for the IP in the database. We do this because
	 * between reputation_set_on_connect() and reputation_pre_lconnect()
	 * the IP of the user may have been changed due to IP-spoofing
	 * (WEBIRC).
	 */
	int score = reputation_lookup_score_and_set(client);

	sendto_server(NULL, 0, 0, NULL, ":%s REPUTATION %s %d", me.id, GetIP(client), score);

	return 0;
}

EVENT(add_scores)
{
	static int marker = 0;
	char *ip;
	Client *client;
	ReputationEntry *e;

	/* This marker is used so we only bump score for an IP entry
	 * once and not twice (or more) if there are multiple users
	 * with the same IP address.
	 */
	marker += 2;

	/* These macros make the code below easier to read. Also,
	 * this explains why we just did marker+=2 and not marker++.
	 */
	#define MARKER_UNREGISTERED_USER (marker)
	#define MARKER_REGISTERED_USER (marker+1)

	list_for_each_entry(client, &client_list, client_node)
	{
		if (!IsUser(client))
			continue; /* skip servers, unknowns, etc.. */

		ip = client->ip;
		if (!ip)
			continue;

		e = find_reputation_entry(ip);
		if (!e)
		{
			/* Create */
			e = safe_alloc(sizeof(ReputationEntry)+strlen(ip));
			strcpy(e->ip, ip); /* safe, allocated above */
			add_reputation_entry(e);
		}

		/* If this is not a duplicate entry, then bump the score.. */
		if ((e->marker != MARKER_UNREGISTERED_USER) && (e->marker != MARKER_REGISTERED_USER))
		{
			e->marker = MARKER_UNREGISTERED_USER;
			if (e->score < REPUTATION_SCORE_CAP)
			{
				/* Regular users receive a point. */
				e->score++;
				/* Registered users receive an additional point */
				if (IsLoggedIn(client) && (e->score < REPUTATION_SCORE_CAP))
				{
					e->score++;
					e->marker = MARKER_REGISTERED_USER;
				}
			}
		} else
		if ((e->marker == MARKER_UNREGISTERED_USER) && IsLoggedIn(client) && (e->score < REPUTATION_SCORE_CAP))
		{
			/* This is to catch a special case:
			 * If there are 2 or more users with the same IP
			 * address and the first user was not registered
			 * then the IP entry only received a score bump of +1.
			 * If the 2nd user (with same IP) is a registered
			 * user then the IP should actually receive a
			 * score bump of +2 (in total).
			 */
			e->score++;
			e->marker = MARKER_REGISTERED_USER;
		}

		e->last_seen = TStime();
		Reputation(client) = e->score; /* update moddata */
	}
}

/** Is this entry expired? */
static inline int is_reputation_expired(ReputationEntry *e)
{
	int i;
	for (i = 0; i < MAXEXPIRES; i++)
	{
		if (cfg.expire_time[i] == 0)
			break; /* end of all entries */
		if ((e->score <= cfg.expire_score[i]) && (TStime() - e->last_seen > cfg.expire_time[i]))
			return 1;
	}
	return 0;
}

/** If the reputation changed (due to server syncing) then update the
 * individual users reputation score as well.
 */
void reputation_changed_update_users(ReputationEntry *e)
{
	Client *client;

	list_for_each_entry(client, &client_list, client_node)
	{
		if (client->ip && !strcmp(e->ip, client->ip))
			Reputation(client) = e->score;
	}
	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (client->ip && !strcmp(e->ip, client->ip))
			Reputation(client) = e->score;
	}
}

EVENT(delete_old_records)
{
	int i;
	ReputationEntry *e, *e_next;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	for (i = 0; i < REPUTATION_HASH_TABLE_SIZE; i++)
	{
		for (e = ReputationHashTable[i]; e; e = e_next)
		{
			e_next = e->next;

			if (is_reputation_expired(e))
			{
#ifdef DEBUGMODE
				unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_EXPIRY", NULL,
				           "Deleting expired entry for $ip (score $score, last seen $time_delta seconds ago)",
				           log_data_string("ip", e->ip),
				           log_data_integer("score", e->score),
				           log_data_integer("time_delta", TStime() - e->last_seen));
#endif
				DelListItem(e, ReputationHashTable[i]);
				safe_free(e);
			}
		}
	}

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_BENCHMARK", NULL,
	           "Reputation benchmark: EXPIRY IN MEM: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
}

EVENT(reputation_save_db_evt)
{
	reputation_save_db();
}

CMD_FUNC(reputationunperm)
{
	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	ModuleSetOptions(ModInf.handle, MOD_OPT_PERM, 0);

	unreal_log(ULOG_INFO, "reputation", "REPUTATIONUNPERM_COMMAND", client,
	           "$client used /REPUTATIONUNPERM. On next REHASH the module can be RELOADED or UNLOADED. "
	           "Note however that for a few minutes the scoring may be skipped, so don't do this too often.");
}

int reputation_connect_extinfo(Client *client, NameValuePrioList **list)
{
	add_fmt_nvplist(list, 0, "reputation", "%d", GetReputation(client));
	return 0;
}

int count_reputation_records(void)
{
	int i;
	ReputationEntry *e;
	int total = 0;

	for (i = 0; i < REPUTATION_HASH_TABLE_SIZE; i++)
		for (e = ReputationHashTable[i]; e; e = e->next)
			total++;

	return total;
}

void reputation_channel_query(Client *client, Channel *channel)
{
	Member *m;
	char buf[512];
	char tbuf[256];
	char **nicks;
	int *scores;
	int cnt = 0, i, j;
	ReputationEntry *e;

	sendtxtnumeric(client, "Users and reputation scores for %s:", channel->name);

	/* Step 1: build a list of nicks and their reputation */
	nicks = safe_alloc((channel->users+1) * sizeof(char *));
	scores = safe_alloc((channel->users+1) * sizeof(int));
	for (m = channel->members; m; m = m->next)
	{
		nicks[cnt] = m->client->name;
		if (m->client->ip)
		{
			e = find_reputation_entry(m->client->ip);
			if (e)
				scores[cnt] = e->score;
		}
		if (++cnt > channel->users)
		{
			unreal_log(ULOG_WARNING, "bug", "REPUTATION_CHANNEL_QUERY_BUG", client,
				   "[BUG] reputation_channel_query() expected $expected_users users, but $found_users (or more) users were present in $channel",
				   log_data_integer("expected_users", channel->users),
				   log_data_integer("found_users", cnt),
				   log_data_string("channel", channel->name));
#ifdef DEBUGMODE
			abort();
#endif
			break; /* safety net */
		}
	}

	/* Step 2: lazy selection sort */
	for (i = 0; i < cnt && nicks[i]; i++)
	{
		for (j = i+1; j < cnt && nicks[j]; j++)
		{
			if (scores[i] < scores[j])
			{
				char *nick_tmp;
				int score_tmp;
				nick_tmp = nicks[i];
				score_tmp = scores[i];
				nicks[i] = nicks[j];
				scores[i] = scores[j];
				nicks[j] = nick_tmp;
				scores[j] = score_tmp;
			}
		}
	}

	/* Step 3: send the (ordered) list to the user */
	*buf = '\0';
	for (i = 0; i < cnt && nicks[i]; i++)
	{
		snprintf(tbuf, sizeof(tbuf), "%s\00314(%d)\003 ", nicks[i], scores[i]);
		if ((strlen(tbuf)+strlen(buf) > 400) || !nicks[i+1])
		{
			sendtxtnumeric(client, "%s%s", buf, tbuf);
			*buf = '\0';
		} else {
			strlcat(buf, tbuf, sizeof(buf));
		}
	}
	sendtxtnumeric(client, "End of list.");
	safe_free(nicks);
	safe_free(scores);
}

void reputation_list_query(Client *client, int maxscore)
{
	Client *target;
	ReputationEntry *e;

	sendtxtnumeric(client, "Users and reputation scores <%d:", maxscore);

	list_for_each_entry(target, &client_list, client_node)
	{
		int score = 0;

		if (!IsUser(target) || IsULine(target) || !target->ip)
			continue;

		e = find_reputation_entry(target->ip);
		if (e)
			score = e->score;
		if (score >= maxscore)
			continue;
		sendtxtnumeric(client, "%s!%s@%s [%s] \017(score: %d)",
			target->name,
			target->user->username,
			target->user->realhost,
			target->ip,
			score);
	}
	sendtxtnumeric(client, "End of list.");
}

CMD_FUNC(reputation_user_cmd)
{
	ReputationEntry *e;
	const char *ip;

	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnotice(client, "Reputation module statistics:");
		sendnotice(client, "Recording for: %lld seconds (since unixtime %lld)",
			(long long)(TStime() - reputation_starttime),
			(long long)reputation_starttime);
		if (reputation_writtentime)
		{
			sendnotice(client, "Last successful db write: %lld seconds ago (unixtime %lld)",
				(long long)(TStime() - reputation_writtentime),
				(long long)reputation_writtentime);
		} else {
			sendnotice(client, "Last successful db write: never");
		}
		sendnotice(client, "Current number of records (IP's): %d", count_reputation_records());
		sendnotice(client, "-");
		sendnotice(client, "Available commands:");
		sendnotice(client, "/REPUTATION <nick>          Show reputation info about nick name");
		sendnotice(client, "/REPUTATION <nick> <value>  Adjust the reputation score of the IP address of nick to <value>");
		sendnotice(client, "/REPUTATION <ip>            Show reputation info about IP address");
		sendnotice(client, "/REPUTATION <ip> <value>    Adjust the reputation score of the IP address to <value>");
		sendnotice(client, "/REPUTATION <channel>       List users in channel along with their reputation score");
		sendnotice(client, "/REPUTATION <NN             List users with reputation score below value NN");
		return;
	}

	if (strchr(parv[1], '.') || strchr(parv[1], ':'))
	{
		ip = parv[1];
	} else
	if (parv[1][0] == '#')
	{
		Channel *channel = find_channel(parv[1]);
		if (!channel)
		{
			sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
			return;
		}
		/* corner case: ircop without proper permissions and not in channel */
		if (!ValidatePermissionsForPath("channel:see:names:invisible",client,NULL,NULL,NULL) && !IsMember(client,channel))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, channel->name);
			return;
		}
		reputation_channel_query(client, channel);
		return;
	} else
	if (parv[1][0] == '<')
	{
		int max = atoi(parv[1] + 1);
		if (max < 1)
		{
			sendnotice(client, "REPUTATION: Invalid search value specified. Use for example '/REPUTATION <5' to search on less-than-five");
			return;
		}
		reputation_list_query(client, max);
		return;
	} else {
		Client *target = find_user(parv[1], NULL);
		if (!target)
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
			return;
		}
		ip = target->ip;
		if (!ip)
		{
			sendnotice(client, "No IP address information available for user '%s'.", parv[1]); /* e.g. services */
			return;
		}
	}

	e = find_reputation_entry(ip);
	if (!e)
	{
		sendnotice(client, "No reputation record found for IP %s", ip);
		return;
	}

	if ((parc > 2) && !BadPtr(parv[2]))
	{
		/* Request to change value */
		if (!ValidatePermissionsForPath("client:set:reputation",client,NULL,NULL,NULL))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			return;
		}
		int v = atoi(parv[2]);
		if (v > REPUTATION_SCORE_CAP)
			v = REPUTATION_SCORE_CAP;
		if (v < 0)
			v = 0;
		e->score = v;
		reputation_changed_update_users(e);
		sendto_server(&me, 0, 0, NULL,
			      ":%s REPUTATION %s *%d*",
			      me.id,
			      e->ip,
			      e->score);
		sendnotice(client, "Reputation of IP %s set to %hd", e->ip, e->score);
		return;
	}

	sendnotice(client, "****************************************************");
	sendnotice(client, "Reputation record for IP %s:", ip);
	sendnotice(client, "    Score: %hd", e->score);
	sendnotice(client, "Last seen: %lld seconds ago (unixtime: %lld)",
		(long long)(TStime() - e->last_seen),
		(long long)e->last_seen);
	sendnotice(client, "****************************************************");
}

/** The REPUTATION server command handler.
 * Syntax: :server REPUTATION <ip> <score>
 * Where the <score> may be prefixed by an asterisk (*).
 *
 * The best way to explain this command is to illustrate by example:
 * :servera REPUTATION 1.2.3.4 0
 * Then serverb, which might have a score of 2 for this IP, will:
 * - Send back to the servera direction:  :serverb REPUTATION 1.2.3.4 *2
 *   So the original server (and direction) receive a score update.
 * - Propagate to non-servera direction: :servera REPUTATION 1.2.3.4 2
 *   So use the new higher score (2 rather than 0).
 * Then the next server may do the same. It MUST propagate to non-serverb
 * direction and MAY (again) update the score even higher.
 *
 * If the score is not prefixed by * then the server may do as above and
 * send back to the uplink an "update" of the score. If, however, the
 * score is prefixed by * then the server will NEVER send back to the
 * uplink, it may only propagate. This is to prevent loops.
 *
 * Since UnrealIRCd 6.0.2+ there is now also asterisk-score-asterisk:
 * :server REPUTATION 1.2.3.4 *2*
 * The leading asterisk means no reply will be sent back, ever, and the
 * trailing asterisk will mean it is a "FORCED SET", which means that
 * servers should set the reputation to that value, even if it is lower.
 * This way reputation can be reduced and the reducation can be synced
 * across servers, which was not possible before 6.0.2.
 *
 * Note that some margin is used when deciding if the server should send
 * back score updates. This is defined by UPDATE_SCORE_MARGIN.
 * If this is for example set to 1 then a point difference of 1 will not
 * yield a score update since such a minor score update is not worth the
 * server to server traffic. Also, due to timing differences a score
 * difference of 1 is quite likely to hapen in normal circumstances.
 */
CMD_FUNC(reputation_server_cmd)
{
	ReputationEntry *e;
	const char *ip;
	int score;
	int allow_reply;
	int forced = 0;

	/* :server REPUTATION <ip> <score> */
	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "REPUTATION");
		return;
	}

	ip = parv[1];

	if (parv[2][0] == '*')
	{
		allow_reply = 0;
		score = atoi(parv[2]+1);
		if (parv[2][1] && (parv[2][strlen(parv[2])-1] == '*'))
			forced = 1;
	} else {
		allow_reply = 1;
		score = atoi(parv[2]);
	}

	if (score > REPUTATION_SCORE_CAP)
		score = REPUTATION_SCORE_CAP;

	e = find_reputation_entry(ip);
	if (allow_reply && e && (e->score > score) && (e->score - score > UPDATE_SCORE_MARGIN))
	{
		/* We have a higher score, inform the client direction about it.
		 * This will prefix the score with a * so servers will never reply to it.
		 */
		sendto_one(client, NULL, ":%s REPUTATION %s *%d", me.id, parv[1], e->score);
#ifdef DEBUGMODE
		unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_DIFFERS", client,
			   "Reputation score for for $ip from $client is $their_score, but we have $score, sending back $score",
			   log_data_string("ip", ip),
			   log_data_integer("their_score", score),
			   log_data_integer("score", e->score));
#endif
		score = e->score; /* Update for propagation in the non-client direction */
	}

	/* Update our score if sender has a higher score */
	if (e && (score > e->score))
	{
#ifdef DEBUGMODE
		unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_DIFFERS", client,
			   "Reputation score for for $ip from $client is $their_score, but we have $score, updating our score to $score",
			   log_data_string("ip", ip),
			   log_data_integer("their_score", score),
			   log_data_integer("score", e->score));
#endif
		e->score = score;
		reputation_changed_update_users(e);
	} else
	if (forced)
	{
#ifdef DEBUGMODE
		unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_DECREASE", client,
			   "Reputation score for for $ip from $client is $score, force-setting to $their_score.",
			   log_data_string("ip", ip),
			   log_data_integer("their_score", score),
			   log_data_integer("score", e->score));
#endif
		e->score = score;
		reputation_changed_update_users(e);
	}

	/* If we don't have any entry for this IP, add it now. */
	if (!e && (score > 0))
	{
#ifdef DEBUGMODE
		unreal_log(ULOG_DEBUG, "reputation", "REPUTATION_NEW", client,
			   "Reputation score for for $ip from $client is $their_score, we had no entry, adding it",
			   log_data_string("ip", ip),
			   log_data_integer("their_score", score),
			   log_data_integer("score", 0));
#endif
		e = safe_alloc(sizeof(ReputationEntry)+strlen(ip));
		strcpy(e->ip, ip); /* safe, see alloc above */
		e->score = score;
		e->last_seen = TStime();
		add_reputation_entry(e);
		reputation_changed_update_users(e);
	}

	/* Propagate to the non-client direction (score may be updated) */
	sendto_server(client, 0, 0, NULL,
	              ":%s REPUTATION %s %s%d%s",
	              client->id,
	              parv[1],
	              allow_reply ? "" : "*",
	              score,
	              forced ? "*" : "");
}

CMD_FUNC(reputation_cmd)
{
	if (MyUser(client))
		CALL_CMD_FUNC(reputation_user_cmd);
	else if (IsServer(client) || IsMe(client))
		CALL_CMD_FUNC(reputation_server_cmd);
}

int reputation_whois(Client *client, Client *target, NameValuePrioList **list)
{
	int reputation;

	if (whois_get_policy(client, target, "reputation") != WHOIS_CONFIG_DETAILS_FULL)
		return 0;

	reputation = Reputation(target);
	if (reputation > 0)
	{
		add_nvplist_numeric_fmt(list, 0, "reputation", client, RPL_WHOISSPECIAL,
		                        "%s :is using an IP with a reputation score of %d",
		                        target->name, reputation);
	}
	return 0;
}

void reputation_md_free(ModData *m)
{
	/* we have nothing to free actually, but we must set to zero */
	m->l = 0;
}

const char *reputation_md_serialize(ModData *m)
{
	static char buf[32];
	if (m->i == 0)
		return NULL; /* not set (reputation always starts at 1) */
	snprintf(buf, sizeof(buf), "%d", m->i);
	return buf;
}

void reputation_md_unserialize(const char *str, ModData *m)
{
	m->i = atoi(str);
}

int reputation_starttime_callback(void)
{
	/* NB: fix this by 2038 */
	return (int)reputation_starttime;
}

void _ban_act_set_reputation(Client *client, BanAction *action)
{
	ReputationEntry *e;
	int value;

	if ((client->ip == NULL) || IsDead(client))
		return; /* Wait, this is impossible, right? */

	/* There's a problem with adjusting reputation scores
	 * when a user is in "unknown state", especially when decreasing
	 * it, which is the common case here.
	 * This is because we might be a new server with not much
	 * reputation scores in the database, so giving everyone a 0,
	 * this is automatically fixed post-connect by other servers
	 * syncing in normally.
	 * However, if we allow setting/decreasing reputation here in
	 * this function, before the adjustment happens, then we may be
	 * decreasing/setting to a very low value (because we think
	 * the user has 0 rep) while in fact the user has high rep
	 * and should still have high rep (just a little lower than before).
	 * Instead of breaking my head on this, let's just not deal with
	 * unknown users for now. Can always enhance this later.
	 */
	if (!IsUser(client))
		return;

	e = find_reputation_entry(client->ip);
	if (!e)
	{
		/* Create */
		e = safe_alloc(sizeof(ReputationEntry)+strlen(client->ip));
		strcpy(e->ip, client->ip); /* safe, allocated above */
		add_reputation_entry(e);
	}

	value = e->score;

	switch (action->var_action)
	{
		case VAR_ACT_SET:
			value = action->value;
			break;
		case VAR_ACT_INCREASE:
			value += action->value;
			if (value > REPUTATION_SCORE_CAP)
				value = REPUTATION_SCORE_CAP;
			break;
		case VAR_ACT_DECREASE:
			value -= action->value;
			if (value < 0)
				value = 0;
			break;
		default:
			abort();
	}

	/* Nothing changed? Then we are done. */
	if (e->score == value)
		return;

	e->score = value;
	reputation_changed_update_users(e);
	sendto_server(&me, 0, 0, NULL,
	              ":%s REPUTATION %s *%d*",
	              me.id,
	              e->ip,
	              e->score);
}
