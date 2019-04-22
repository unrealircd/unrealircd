/*
 * reputation - Provides a scoring system for "known users".
 * (C) Copyright 2015-2019 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2
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

#define REPUTATION_VERSION "1.0.1"

#undef TEST

#define BENCHMARK
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

ModuleHeader MOD_HEADER(reputation)
  = {
	"reputation",
	REPUTATION_VERSION,
	"Known IP's scoring system",
	"3.2-b8-1",
	NULL 
    };

#define MAXEXPIRES 10

#define REPUTATION_SCORE_CAP 10000

#define UPDATE_SCORE_MARGIN 1

#define Reputation(acptr)	moddata_client(acptr, reputation_md).l

struct cfgstruct {
	int expire_score[MAXEXPIRES];
	long expire_time[MAXEXPIRES];
	char *database;
};
static struct cfgstruct cfg;

typedef struct reputationentry ReputationEntry;

struct reputationentry {
	ReputationEntry *prev, *next;
	unsigned short score; /**< score for the user */
	long last_seen; /**< user last seen (unix timestamp) */
	int marker; /**< internal marker, not written to db */
	char ip[1]; /*< ip address */
};

long reputation_starttime = 0;
long reputation_writtentime = 0;

#define REPUTATION_HASH_SIZE 1327
static ReputationEntry *ReputationHashTable[REPUTATION_HASH_SIZE];

static ModuleInfo ModInf;

ModDataInfo *reputation_md; /* Module Data structure which we acquire */

/* Forward declarations */
void reputation_md_free(ModData *m);
char *reputation_md_serialize(ModData *m);
void reputation_md_unserialize(char *str, ModData *m);
void config_setdefaults(void);
CMD_FUNC(reputation_cmd);
CMD_FUNC(reputationunperm);
int reputation_whois(aClient *sptr, aClient *acptr);
int reputation_handshake(aClient *sptr);
int reputation_pre_lconnect(aClient *sptr);
int reputation_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reputation_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int reputation_config_posttest(int *errs);
unsigned long hash_djb2(char *str);
int hash_reputation_entry(char *ip);
void add_reputation_entry(ReputationEntry *e);
EVENT(delete_old_records);
EVENT(add_scores);
EVENT(save_db_evt);
void load_db(void);
void save_db(void);
int reputation_starttime_callback(void);

MOD_TEST(reputation)
{
	memcpy(&ModInf, modinfo, modinfo->size);
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, reputation_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, reputation_config_posttest);
	CallbackAddEx(modinfo->handle, CALLBACKTYPE_REPUTATION_STARTTIME, reputation_starttime_callback);
	return MOD_SUCCESS;
}

MOD_INIT(reputation)
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);
	memset(&ReputationHashTable, 0, sizeof(ReputationHashTable));

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

	config_setdefaults();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, reputation_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, reputation_whois);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, reputation_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 2000000000, reputation_pre_lconnect); /* (prio: last) */
	CommandAdd(ModInf.handle, "REPUTATION", reputation_cmd, MAXPARA, M_USER|M_SERVER);
	CommandAdd(ModInf.handle, "REPUTATIONUNPERM", reputationunperm, MAXPARA, M_USER|M_SERVER);
	return MOD_SUCCESS;
}

MOD_LOAD(reputation)
{
	load_db();
	if (reputation_starttime == 0)
		reputation_starttime = TStime();
	EventAddEx(ModInf.handle, "delete_old_records", DELETE_OLD_EVERY, 0, delete_old_records, NULL);
	EventAddEx(ModInf.handle, "add_scores", BUMP_SCORE_EVERY, 0, add_scores, NULL);
	EventAddEx(ModInf.handle, "save_db", SAVE_DB_EVERY, 0, save_db_evt, NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(reputation)
{
	save_db();
	return MOD_SUCCESS;
}

void config_setdefaults(void)
{
	/* data/reputation.db */
	cfg.database = strdup("reputation.db");
	convert_to_absolute_path(&cfg.database, PERMDATADIR);

	/* EXPIRES the following entries if the IP does appear for some time: */
	/* <=2 points after 1 hour */
	cfg.expire_score[0] = 2;
#ifndef TEST
	cfg.expire_time[0]   = 3600;
#else
	cfg.expire_time[0]   = 36;
#endif
	/* <=6 points after 7 days */
	cfg.expire_score[1] = 6;
	cfg.expire_time[1]   = 86400*7;
	/* ANY result that has not been seen for 30 days */
	cfg.expire_score[2] = -1;
	cfg.expire_time[2]   = 86400*30;
}

int reputation_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::reputation.. */
	if (!ce || strcmp(ce->ce_varname, "reputation"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: blank set::reputation::%s without value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		} else
		if (!strcmp(cep->ce_varname, "database"))
		{
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
		} else
		{
			config_error("%s:%i: unknown directive set::reputation::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
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
	if (!ce || strcmp(ce->ce_varname, "reputation"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "database"))
		{
			safestrdup(cfg.database, cep->ce_vardata);
		}
	}
	return 1;
}

int reputation_config_posttest(int *errs)
{
	int errors = 0;

	*errs = errors;
	return errors ? -1 : 1;
}

/** Cut off string on first occurance of CR or LF */
void stripcrlf(char *buf)
{
	for (; *buf; buf++)
	{
		if ((*buf == '\n') || (*buf == '\r'))
		{
			*buf = '\0';
			return;
		}
	}
}

/** Parse database header and set variables appropriately */
int parse_db_header(char *buf)
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

void load_db(void)
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
		config_error("WARNING: Could not open/read database '%s': %s", cfg.database, strerror(ERRNO));
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
	if (!parse_db_header(buf))
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
		
		e = MyMallocEx(sizeof(ReputationEntry)+strlen(ip));
		strcpy(e->ip, ip); /* safe, see alloc above */
		e->score = atoi(score);
		e->last_seen = atol(last_seen);
		
		add_reputation_entry(e);
	}
	fclose(fd);

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "Reputation benchmark: LOAD DB: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
}

void save_db(void)
{
	FILE *fd;
	char tmpfname[512];
	char buf[512], *p;
	int i;
	ReputationEntry *e;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif
	
#ifdef TEST
	sendto_realops("REPUTATION IS RUNNING IN TEST MODE. SAVING DB'S...");
#endif

	/* We write to a temporary file. Only to rename it later if everything was ok */
	snprintf(tmpfname, sizeof(tmpfname), "%s.tmp", cfg.database);
	
	fd = fopen(tmpfname, "w");
	if (!fd)
	{
		config_error("ERROR: Could not open/write database '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
		return;
	}

	if (fprintf(fd, "REPDB 1 %ld %ld\n", reputation_starttime, TStime()) < 0)
		goto write_fail;

	for (i = 0; i < REPUTATION_HASH_SIZE; i++)
	{
		for (e = ReputationHashTable[i]; e; e = e->next)
		{
			if (fprintf(fd, "%s %d %ld\n", e->ip, (int)e->score, e->last_seen) < 0)
			{
write_fail:
				config_error("ERROR writing to '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
				fclose(fd);
				return;
			}
		}
	}

	if (fclose(fd) < 0)
	{
		config_error("ERROR writing to '%s': %s -- DATABASE *NOT* SAVED!!!", tmpfname, strerror(ERRNO));
		return;
	}
	
	/* Everything went fine. We rename our temporary file to the existing
	 * DB file (will overwrite), which is more or less an atomic operation.
	 */
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("ERROR renaming '%s' to '%s': %s -- DATABASE *NOT* SAVED!!!",
			tmpfname, cfg.database, strerror(ERRNO));
		return;
	}

	reputation_writtentime = TStime();

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "Reputation benchmark: SAVE DB: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif

	return;
}

/* One of DJB2's hashing algorithm. Modified to use tolower(). */
unsigned long hash_djb2(char *str)
{
	unsigned long hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + tolower(c); /* hash * 33 + c */

	return hash;
}

int hash_reputation_entry(char *ip)
{
	unsigned long alpha, beta, result;

	return hash_djb2(ip) % REPUTATION_HASH_SIZE;
}

void add_reputation_entry(ReputationEntry *e)
{
	int hashv = hash_reputation_entry(e->ip);

	AddListItem(e, ReputationHashTable[hashv]);
}

ReputationEntry *find_reputation_entry(char *ip)
{
	ReputationEntry *e;
	int hashv = hash_reputation_entry(ip);

	for (e = ReputationHashTable[hashv]; e; e = e->next)
		if (!strcmp(e->ip, ip))
			return e;

	return NULL;
}

/** Called when the user connects (very early, just after the
 * TCP/IP connection has been established, before any data).
 */
int reputation_handshake(aClient *acptr)
{
	char *ip = acptr->ip;
	ReputationEntry *e;

	if (ip)
	{
		e = find_reputation_entry(ip);
		if (e)
		{
			Reputation(acptr) = e->score; /* SET MODDATA */
		}
	}
	return 0;
}

int reputation_pre_lconnect(aClient *sptr)
{
	/* User will likely be accepted. Inform other servers about the score
	 * we have for this user. For more information about this type of
	 * server to server traffic, see the reputation_server_cmd function.
	 */
	ReputationEntry *e = find_reputation_entry(GetIP(sptr));
	sendto_server(NULL, 0, 0, ":%s REPUTATION %s %hd", me.name, GetIP(sptr), e ? e->score : 0);

	return 0;
}

EVENT(add_scores)
{
	static int marker = 0;
	char *ip;
	aClient *acptr;
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
	
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsPerson(acptr))
			continue; /* skip servers, unknowns, etc.. */

		ip = acptr->ip;
		if (!ip)
			continue;

		e = find_reputation_entry(ip);
		if (!e)
		{
			/* Create */
			e = MyMallocEx(sizeof(ReputationEntry)+strlen(ip));
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
				if (IsLoggedIn(acptr) && (e->score < REPUTATION_SCORE_CAP))
				{
					e->score++;
					e->marker = MARKER_REGISTERED_USER;
				}
			}
		} else
		if ((e->marker == MARKER_UNREGISTERED_USER) && IsLoggedIn(acptr) && (e->score < REPUTATION_SCORE_CAP))
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
		Reputation(acptr) = e->score; /* update moddata */
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

EVENT(delete_old_records)
{
	int i;
	ReputationEntry *e, *e_next;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif
	
	for (i = 0; i < REPUTATION_HASH_SIZE; i++)
	{
		for (e = ReputationHashTable[i]; e; e = e_next)
		{
			e_next = e->next;
			
			if (is_reputation_expired(e))
			{
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "Deleting expired entry for '%s' (score %hd, last seen %ld seconds ago)",
				         e->ip, e->score, TStime() - e->last_seen);
#endif
				DelListItem(e, ReputationHashTable[i]);
				MyFree(e);
			}
		}
	}

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "Reputation benchmark: EXPIRY IN MEM: %ld microseconds",
		((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));
#endif
}

EVENT(save_db_evt)
{
	save_db();
}

CMD_FUNC(reputationunperm)
{
	if (!IsOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	ModuleSetOptions(ModInf.handle, MOD_OPT_PERM, 0);

	sendto_realops("%s used /REPUTATIONUNPERM. On next REHASH the module can be RELOADED or UNLOADED. "
	               "Note however that for a few minutes the scoring may be skipped, so don't do this too often.",
	               sptr->name);
	return 0;
}

int count_reputation_records(void)
{
	int i;
	ReputationEntry *e;
	int total = 0;

	for (i = 0; i < REPUTATION_HASH_SIZE; i++)
		for (e = ReputationHashTable[i]; e; e = e->next)
			total++;

	return total;
}

CMD_FUNC(reputation_user_cmd)
{
	ReputationEntry *e;
	char *ip;

	if (!IsOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}
	
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnotice(sptr, "Reputation module statistics:");
		sendnotice(sptr, "Recording for: %ld seconds (since unixtime %ld)",
			TStime() - reputation_starttime, reputation_starttime);
		if (reputation_writtentime)
		{
			sendnotice(sptr, "Last successful db write: %ld seconds ago (unixtime %ld)",
				TStime() - reputation_writtentime, reputation_writtentime);
		} else {
			sendnotice(sptr, "Last successful db write: never");
		}
		sendnotice(sptr, "Current number of records (IP's): %d", count_reputation_records());
		sendnotice(sptr, "-");
		sendnotice(sptr, "For more specific information, use: /REPUTATION [nick|IP-address]");
		return 0;
	}
	
	if (strchr(parv[1], '.') || strchr(parv[1], ':'))
	{
		ip = parv[1];
	} else {
		aClient *acptr = find_person(parv[1], NULL);
		if (!acptr)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, parv[1]);
			return 0;
		}
		ip = acptr->ip;
		if (!ip)
		{
			sendnotice(sptr, "No IP address information available for user '%s'.", parv[1]); /* e.g. services */
			return 0;
		}
	}
	
	e = find_reputation_entry(ip);
	if (!e)
	{
		sendnotice(sptr, "No reputation record found for IP %s", ip);
		return 0;
	}

	sendnotice(sptr, "****************************************************");
	sendnotice(sptr, "Reputation record for IP %s:", ip);
	sendnotice(sptr, "    Score: %hd", e->score);
	sendnotice(sptr, "Last seen: %ld seconds ago (unixtime: %ld)",
		TStime() - e->last_seen, e->last_seen);
	sendnotice(sptr, "****************************************************");
	return 0;
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
	char *ip;
	int score;
	long since;
	int allow_reply;

	/* :server REPUTATION <ip> <score> */
	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "REPUTATION");
		return 0;
	}
	
	ip = parv[1];

	if (parv[2][0] == '*')
	{
		allow_reply = 0;
		score = atoi(parv[2]+1);
	} else {
		allow_reply = 1;
		score = atoi(parv[2]);
	}

	if (score > REPUTATION_SCORE_CAP)
		score = REPUTATION_SCORE_CAP;

	e = find_reputation_entry(ip);
	if (allow_reply && e && (e->score > score) && (e->score - score > UPDATE_SCORE_MARGIN))
	{
		/* We have a higher score, inform the cptr direction about it.
		 * This will prefix the score with a * so servers will never reply to it.
		 */
		sendto_one(cptr, ":%s REPUTATION %s *%d", me.name, parv[1], e->score);
#ifdef DEBUGMODE
		ircd_log(LOG_ERROR, "[reputation] Score for '%s' from %s is %d, but we have %d, sending back %d",
			ip, sptr->name, score, e->score, e->score);
#endif
		score = e->score; /* Update for propagation in the non-cptr direction */
	}

	/* Update our score if sender has a higher score */
	if (e && (score > e->score))
	{
#ifdef DEBUGMODE
		ircd_log(LOG_ERROR, "[reputation] Score for '%s' from %s is %d, but we have %d, updating our score to %d",
			ip, sptr->name, score, e->score, score);
#endif
		e->score = score;
	}

	/* If we don't have any entry for this IP, add it now. */
	if (!e && (score > 0))
	{
#ifdef DEBUGMODE
		ircd_log(LOG_ERROR, "[reputation] Score for '%s' from %s is %d, we had no entry, adding it",
			ip, sptr->name, score);
#endif
		e = MyMallocEx(sizeof(ReputationEntry)+strlen(ip));
		strcpy(e->ip, ip); /* safe, see alloc above */
		e->score = score;
		e->last_seen = TStime();
		add_reputation_entry(e);
	}

	/* Propagate to the non-cptr direction (score may be updated) */
	sendto_server(cptr, 0, 0, ":%s REPUTATION %s %s%d",
	              sptr->name,
	              parv[1],
	              allow_reply ? "" : "*",
	              score);

	return 0;
}

CMD_FUNC(reputation_cmd)
{
	if (MyClient(sptr))
		return reputation_user_cmd(cptr, sptr, parc, parv);

	if (IsServer(sptr))
		return reputation_server_cmd(cptr, sptr, parc, parv);
	return 0;
}

int reputation_whois(aClient *sptr, aClient *acptr)
{
	int reputation = Reputation(acptr);

	if (!IsOper(sptr))
		return 0; /* only opers can see this.. */
	
	if (reputation > 0)
	{
		sendto_one(sptr, ":%s %d %s %s :is using an IP with a reputation score of %d",
			me.name, RPL_WHOISSPECIAL, sptr->name,
			acptr->name, reputation);
	}
	return 0;
}

void reputation_md_free(ModData *m)
{
	/* we have nothing to free actually, but we must set to zero */
	m->l = 0;
}

char *reputation_md_serialize(ModData *m)
{
	static char buf[32];
	if (m->i == 0)
		return NULL; /* not set (reputation always starts at 1) */
	snprintf(buf, sizeof(buf), "%d", m->i);
	return buf;
}

void reputation_md_unserialize(char *str, ModData *m)
{
	m->i = atoi(str);
}

int reputation_starttime_callback(void)
{
	/* NB: fix this by 2038 */
	return (int)reputation_starttime;
}
