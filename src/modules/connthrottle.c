/*
 * connthrottle - Connection throttler
 * (C) Copyright 2004-2020 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 * See https://www.unrealircd.org/docs/Connthrottle
 */

#include "unrealircd.h"

#define CONNTHROTTLE_VERSION "1.3"

#ifndef CALLBACKTYPE_REPUTATION_STARTTIME
 #define CALLBACKTYPE_REPUTATION_STARTTIME 5
#endif

ModuleHeader MOD_HEADER
  = {
	"connthrottle",
	CONNTHROTTLE_VERSION,
	"Connection throttler - by Syzop",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef struct {
	int count;
	int period;
} ThrottleSetting;

struct cfgstruct {
	/* set::connthrottle::known-users: */
	ThrottleSetting local;
	ThrottleSetting global;
	/* set::connthrottle::except: */
	SecurityGroup *except;
	/* set::connthrottle::disabled-when: */
	long reputation_gathering;
	int start_delay;
	/* set::connthrottle (generic): */
	char *reason;
	/* set::connthrottle::ipv6-unknown-users-limit (one entry per tier) */
	int ipv6_unknown_users_limit[3];
};
static struct cfgstruct cfg;

typedef struct {
	int count;
	long t;
} ThrottleCounter;

typedef struct UCounter UCounter;
struct UCounter {
	ThrottleCounter local;		/**< Local counter */
	ThrottleCounter global;		/**< Global counter */
	int rejected_clients;		/**< Number of rejected clients this minute */
	int allowed_except;		/**< Number of allowed clients - on except list */
	int allowed_unknown_users;	/**< Number of allowed clients - not on except list */
	char disabled;			/**< Module disabled by oper? */
	int throttling_this_minute;	/**< Did we do any throttling this minute? */
	int throttling_previous_minute;	/**< Did we do any throttling previous minute? */
	int throttling_banner_displayed;/**< Big we-are-now-throttling banner displayed? */
	time_t next_event;		/**< When is next event? (for "last 60 seconds" stats) */
};
UCounter *ucounter = NULL;

#define MSG_THROTTLE "THROTTLE"

/* Forward declarations */
int ct_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int ct_config_posttest(int *errs);
int ct_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int ct_pre_lconnect(Client *client);
int ct_lconnect(Client *);
int ct_rconnect(Client *);
CMD_FUNC(ct_throttle);
EVENT(connthrottle_evt);
void ucounter_free(ModData *m);
RPC_CALL_FUNC(rpc_connthrottle_status);
RPC_CALL_FUNC(rpc_connthrottle_set);
RPC_CALL_FUNC(rpc_connthrottle_reset);

/* IPv6 wider-prefix bucket tracking (/56, /48, /32) */

#define CT_NUM_TIERS 3
#define CT_BUCKET_HASH_SIZE 2048

#ifdef DEBUGMODE
/** Self-check of IPv6 CIDR limits every <this> msec */
#define CONNTHROTTLE_CHECK 1000
#endif

/* Per-client classification stored in our ModData slot.
 * CT_CATEGORY_NONE (value 0) is the moddata default and means
 * "this client has not been added to our buckets yet".
 */
typedef enum {
	CT_CATEGORY_NONE              = 0,
	CT_CATEGORY_KNOWN_USERS       = 1,
	CT_CATEGORY_EXCEPTED_UNKNOWNS = 2,
	CT_CATEGORY_UNKNOWN_USERS     = 3,
} ConnThrottleCategory;

typedef struct ConnThrottleBucket ConnThrottleBucket;
struct ConnThrottleBucket {
	ConnThrottleBucket *prev, *next;
	char rawip[16];
	int known_users;
	int excepted_unknowns;
	int unknown_users;
#if defined(CONNTHROTTLE_CHECK)
	int check_known;
	int check_excepted;
	int check_unknown;
#endif
};

static const int ct_tier_prefix[CT_NUM_TIERS] = { 56, 48, 32 };
static ConnThrottleBucket **ct_bucket_hash[CT_NUM_TIERS];
static char *siphashkey_ct_buckets = NULL;
static ModDataInfo *connthrottle_md = NULL;

/* Per-client classification cached in ModData. CT_CATEGORY_NONE means
 * "this client has not been added to our buckets yet" (or has been removed).
 */
#define CT_CATEGORY(client)	moddata_client((client), connthrottle_md).l

static void ct_make_rawip(Client *client, int tier, char *out);
static uint64_t ct_hash_bucket(const char *masked);
static ConnThrottleBucket *ct_find_bucket(int tier, const char *masked);
static ConnThrottleBucket *ct_add_bucket(int tier, const char *masked);
static void ct_bucket_increment(ConnThrottleBucket *b, ConnThrottleCategory category);
static void ct_bucket_decrement(ConnThrottleBucket *b, ConnThrottleCategory category);
static ConnThrottleCategory ct_classify(Client *client);
static void ct_bucket_bump_client(Client *client, ConnThrottleCategory category);
static void ct_bucket_unbump_client(Client *client, ConnThrottleCategory category);
static void ct_buckets_rebuild(void);
static void ct_buckets_free(void);
static const char *ct_module_status_text(void);
const char *ct_allow_client(Client *client, ConfigItem_allow *aconf);
int ct_remote_connect_buckets(Client *client);
int ct_quit(Client *client, MessageTag *mtags, const char *comment);
int ct_known_user_cache_change(Client *client);
int stats_connthrottle(Client *client, const char *para);
#if defined(CONNTHROTTLE_CHECK)
EVENT(ct_check);
#endif

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	
	/* Defaults: */
	cfg.local.count = 20; cfg.local.period = 60;
	cfg.global.count = 30; cfg.global.period = 60;
	cfg.start_delay = 180;		/* 3 minutes */
	cfg.reputation_gathering = 7*86400;	/* 1 week */
	safe_strdup(cfg.reason, "Throttled: Too many users trying to connect, please wait a while and try again");
	cfg.except = safe_alloc(sizeof(SecurityGroup));
	cfg.except->reputation_score = 24;
	cfg.except->identified = 1;
	cfg.except->webirc = 0;
	cfg.ipv6_unknown_users_limit[0] = 8;	/* /56 */
	cfg.ipv6_unknown_users_limit[1] = 32;	/* /48 */
	cfg.ipv6_unknown_users_limit[2] = 256;	/* /32 */

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, ct_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, ct_config_posttest);
	
	return MOD_SUCCESS;
}

MOD_INIT()
{
	RPCHandlerInfo r;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, ucounter, ucounter_free);
	if (!ucounter)
		ucounter = safe_alloc(sizeof(UCounter));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, ct_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, ct_pre_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, ct_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, ct_rconnect);

	/* IPv6 wider-prefix bucket tracking */
	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "connthrottle_category";
	mreq.type = MODDATATYPE_CLIENT;
	mreq.sync = 0;	/* local only, no S2S sync */
	connthrottle_md = ModDataAdd(modinfo->handle, mreq);
	if (!connthrottle_md)
	{
		config_error("[connthrottle] Could not register ModData 'connthrottle_category'");
		return MOD_FAILED;
	}
	siphashkey_ct_buckets = safe_alloc(SIPHASH_KEY_LENGTH);
	siphash_generate_key(siphashkey_ct_buckets);
	ct_bucket_hash[0] = safe_alloc(sizeof(ConnThrottleBucket *) * CT_BUCKET_HASH_SIZE);
	ct_bucket_hash[1] = safe_alloc(sizeof(ConnThrottleBucket *) * CT_BUCKET_HASH_SIZE);
	ct_bucket_hash[2] = safe_alloc(sizeof(ConnThrottleBucket *) * CT_BUCKET_HASH_SIZE);
	HookAddConstString(modinfo->handle, HOOKTYPE_ALLOW_CLIENT, 0, ct_allow_client);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, ct_remote_connect_buckets);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, ct_quit);
	HookAdd(modinfo->handle, HOOKTYPE_UNKUSER_QUIT, 0, ct_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, ct_quit);
	HookAdd(modinfo->handle, HOOKTYPE_KNOWN_USER_CACHE_CHANGE, 0, ct_known_user_cache_change);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, stats_connthrottle);

	CommandAdd(modinfo->handle, MSG_THROTTLE, ct_throttle, MAXPARA, CMD_USER|CMD_SERVER);

	/* RPC handlers */
	memset(&r, 0, sizeof(r));
	r.method = "connthrottle.status";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_connthrottle_status;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[connthrottle] Could not register RPC handler 'connthrottle.status'");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "connthrottle.set";
	r.call = rpc_connthrottle_set;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[connthrottle] Could not register RPC handler 'connthrottle.set'");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "connthrottle.reset";
	r.call = rpc_connthrottle_reset;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[connthrottle] Could not register RPC handler 'connthrottle.reset'");
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "connthrottle_evt", connthrottle_evt, NULL, 1000, 0);
#if defined(CONNTHROTTLE_CHECK)
	EventAdd(modinfo->handle, "ct_check", ct_check, NULL, CONNTHROTTLE_CHECK, 0);
#endif
	ct_buckets_rebuild();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, ucounter);
	safe_free(cfg.reason);
	free_security_group(cfg.except);

	/* IPv6 wider-prefix bucket tracking */
	ct_buckets_free();
	safe_free(siphashkey_ct_buckets);

	return MOD_SUCCESS;
}

/** This function checks if the reputation module is loaded.
 * If not, then the module will error, since we depend on it.
 */
int ct_config_posttest(int *errs)
{
	int errors = 0;

	/* Note: we use Callbacks[] here, but this is only for checking. Don't
	 * let this confuse you. At any other place you must use RCallbacks[].
	 */
	if (Callbacks[CALLBACKTYPE_REPUTATION_STARTTIME] == NULL)
	{
		config_error("The 'connthrottle' module requires the 'reputation' "
		             "module to be loaded as well.");
		config_error("Add the following to your configuration file: "
		             "loadmodule \"reputation\";");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/** Test the set::connthrottle configuration */
int ct_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp, *ceppp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::connthrottle.. */
	if (!ce || !ce->name || strcmp(ce->name, "connthrottle"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "known-users"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "minimum-reputation-score"))
				{
					int cnt = atoi(cepp->value);
					if (cnt < 1)
					{
						config_error("%s:%i: set::connthrottle::known-users::minimum-reputation-score should be at least 1",
							cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->name, "sasl-bypass"))
				{
				} else
				if (!strcmp(cepp->name, "webirc-bypass"))
				{
				} else
				{
					config_error_unknown(cepp->file->filename, cepp->line_number,
					                     "set::connthrottle::known-users", cepp->name);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->name, "new-users"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "local-throttle"))
				{
					int cnt, period;
					if (!config_parse_flood(cepp->value, &cnt, &period) ||
					    (cnt < 1) || (cnt > 2000000000) || (period > 2000000000))
					{
						config_error("%s:%i: set::connthrottle::new-users::local-throttle error. "
							     "Syntax is <count>:<period> (eg 6:60), "
							     "and count and period should be non-zero.",
							     cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->name, "global-throttle"))
				{
					int cnt, period;
					if (!config_parse_flood(cepp->value, &cnt, &period) ||
					    (cnt < 1) || (cnt > 2000000000) || (period > 2000000000))
					{
						config_error("%s:%i: set::connthrottle::new-users::global-throttle error. "
							     "Syntax is <count>:<period> (eg 6:60), "
							     "and count and period should be non-zero.",
							     cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
				} else
				{
					config_error_unknown(cepp->file->filename, cepp->line_number,
					                     "set::connthrottle::new-users", cepp->name);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->name, "disabled-when"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->name, "start-delay"))
				{
					int cnt = config_checkval(cepp->value, CFG_TIME);
					if ((cnt < 0) || (cnt > 3600))
					{
						config_error("%s:%i: set::connthrottle::disabled-when::start-delay should be in range 0-3600",
							cepp->file->filename, cepp->line_number);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->name, "reputation-gathering"))
				{
				} else
				{
					config_error_unknown(cepp->file->filename, cepp->line_number,
					                     "set::connthrottle::disabled-when", cepp->name);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->name, "reason"))
		{
			CheckNull(cep);
		} else
		if (!strcmp(cep->name, "ipv6-unknown-users-limit"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (strcmp(cepp->name, "cidr-56") &&
				    strcmp(cepp->name, "cidr-48") &&
				    strcmp(cepp->name, "cidr-32"))
				{
					config_error_unknown(cepp->file->filename, cepp->line_number,
					                     "set::connthrottle::ipv6-unknown-users-limit", cepp->name);
					errors++;
					continue;
				}
				for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
				{
					CheckNull(ceppp);
					if (!strcmp(ceppp->name, "max"))
					{
						int v = atoi(ceppp->value);
						if ((v < 0) || (v > 1000000))
						{
							config_error("%s:%i: set::connthrottle::ipv6-unknown-users-limit::%s::max should be in range 0-1000000",
							             ceppp->file->filename, ceppp->line_number, cepp->name);
							errors++;
						}
					} else
					{
						config_error_unknown(ceppp->file->filename, ceppp->line_number,
						                     "set::connthrottle::ipv6-unknown-users-limit::cidr-N", ceppp->name);
						errors++;
					}
				}
			}
		} else
		{
			config_error("%s:%i: unknown directive set::connthrottle::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

/* Configure ourselves based on the set::connthrottle settings */
int ct_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp, *ceppp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::connthrottle.. */
	if (!ce || !ce->name || strcmp(ce->name, "connthrottle"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &cfg.except);
		} else
		if (!strcmp(cep->name, "known-users"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "minimum-reputation-score"))
					cfg.except->reputation_score = atoi(cepp->value);
				else if (!strcmp(cepp->name, "sasl-bypass"))
					cfg.except->identified = config_checkval(cepp->value, CFG_YESNO);
				else if (!strcmp(cepp->name, "webirc-bypass"))
					cfg.except->webirc = config_checkval(cepp->value, CFG_YESNO);
			}
		} else
		if (!strcmp(cep->name, "new-users"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "local-throttle"))
					config_parse_flood(cepp->value, &cfg.local.count, &cfg.local.period);
				else if (!strcmp(cepp->name, "global-throttle"))
					config_parse_flood(cepp->value, &cfg.global.count, &cfg.global.period);
			}
		} else
		if (!strcmp(cep->name, "disabled-when"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "start-delay"))
					cfg.start_delay = config_checkval(cepp->value, CFG_TIME);
				else if (!strcmp(cepp->name, "reputation-gathering"))
					cfg.reputation_gathering = config_checkval(cepp->value, CFG_TIME);
			}
		} else
		if (!strcmp(cep->name, "reason"))
		{
			safe_free(cfg.reason);
			cfg.reason = safe_alloc(strlen(cep->value)+16);
			sprintf(cfg.reason, "Throttled: %s", cep->value);
		} else
		if (!strcmp(cep->name, "ipv6-unknown-users-limit"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				int tier;
				if (!strcmp(cepp->name, "cidr-56"))
					tier = 0;
				else if (!strcmp(cepp->name, "cidr-48"))
					tier = 1;
				else if (!strcmp(cepp->name, "cidr-32"))
					tier = 2;
				else
					continue;
				for (ceppp = cepp->items; ceppp; ceppp = ceppp->next)
				{
					if (!strcmp(ceppp->name, "max"))
						cfg.ipv6_unknown_users_limit[tier] = atoi(ceppp->value);
				}
			}
		}
	}
	return 1;
}

/** Returns 1 if the 'reputation' module is still gathering
 * data, such as in the first week of when it is loaded.
 * This behavior is configured via set::disabled-when::reputation-gathering
 */
int still_reputation_gathering(void)
{
	int v;

	if (RCallbacks[CALLBACKTYPE_REPUTATION_STARTTIME] == NULL)
		return 1; /* Reputation module not loaded, disable us */

	v = RCallbacks[CALLBACKTYPE_REPUTATION_STARTTIME]->func.intfunc();

	if (TStime() - v < cfg.reputation_gathering)
		return 1; /* Still gathering reputation data (eg: first week) */

	return 0;
}

EVENT(connthrottle_evt)
{
	char buf[512];

	if (ucounter->next_event > TStime())
		return;
	ucounter->next_event = TStime() + 60;

	if (ucounter->rejected_clients)
	{
		unreal_log(ULOG_INFO, "connthrottle", "CONNTHROTTLE_REPORT", NULL,
		           "ConnThrottle] Stats for this server past 60 secs: "
		           "Connections rejected: $num_rejected. "
		           "Accepted: $num_accepted_except except user(s) and "
		           "$num_accepted_unknown_users new user(s).",
		           log_data_integer("num_rejected", ucounter->rejected_clients),
		           log_data_integer("num_accepted_except", ucounter->allowed_except),
		           log_data_integer("num_accepted_unknown_users", ucounter->allowed_unknown_users));
	}

	/* Reset stats for next message */
	ucounter->rejected_clients = 0;
	ucounter->allowed_except = 0;
	ucounter->allowed_unknown_users = 0;

	ucounter->throttling_previous_minute = ucounter->throttling_this_minute;
	ucounter->throttling_this_minute = 0; /* reset */
	ucounter->throttling_banner_displayed = 0; /* reset */
}

#define THROT_LOCAL 1
#define THROT_GLOBAL 2
int ct_pre_lconnect(Client *client)
{
	int throttle=0;
	int score;

	if (me.local->creationtime + cfg.start_delay > TStime())
		return HOOK_CONTINUE; /* no throttle: start delay */

	if (ucounter->disabled)
		return HOOK_CONTINUE; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return HOOK_CONTINUE; /* still gathering reputation data */

	if (user_allowed_by_security_group(client, cfg.except) ||
	    find_tkl_exception(TKL_CONNECT_FLOOD, client))
		return HOOK_CONTINUE; /* allowed: user is exempt */

	/* If we reach this then the user is NEW */

	/* +1 global client would reach global limit? */
	if ((TStime() - ucounter->global.t < cfg.global.period) && (ucounter->global.count+1 > cfg.global.count))
		throttle |= THROT_GLOBAL;

	/* +1 local client would reach local limit? */
	if ((TStime() - ucounter->local.t < cfg.local.period) && (ucounter->local.count+1 > cfg.local.count))
		throttle |= THROT_LOCAL;

	if (throttle)
	{
		ucounter->throttling_this_minute = 1;
		ucounter->rejected_clients++;
		/* We send the LARGE banner if throttling was activated */
		if (!ucounter->throttling_previous_minute && !ucounter->throttling_banner_displayed)
		{
			unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTTLE_ACTIVATED", NULL,
			           "[ConnThrottle] Connection throttling has been ACTIVATED due to a HIGH CONNECTION RATE.\n"
			           "Users with IP addresses that have not been seen before will be rejected above the set connection rate. Known users can still get in.\n"
			           "or more information see https://www.unrealircd.org/docs/ConnThrottle");
			ucounter->throttling_banner_displayed = 1;
		}
		exit_client(client, NULL, cfg.reason);
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}

/** Increase the connect counter(s), nothing else. */
void bump_connect_counter(int local_connect)
{
	if (local_connect)
	{
		/* Bump local connect counter */
		if (TStime() - ucounter->local.t >= cfg.local.period)
		{
			ucounter->local.t = TStime();
			ucounter->local.count = 1;
		} else {
			ucounter->local.count++;
		}
	}

	/* Bump global connect counter */
	if (TStime() - ucounter->global.t >= cfg.global.period)
	{
		ucounter->global.t = TStime();
		ucounter->global.count = 1;
	} else {
		ucounter->global.count++;
	}
}

int ct_lconnect(Client *client)
{
	int score;

	if (me.local->creationtime + cfg.start_delay > TStime())
		return 0; /* no throttle: start delay */

	if (ucounter->disabled)
		return 0; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return 0; /* still gathering reputation data */

	if (user_allowed_by_security_group(client, cfg.except) ||
	    find_tkl_exception(TKL_CONNECT_FLOOD, client))
	{
		ucounter->allowed_except++;
		return HOOK_CONTINUE; /* allowed: user is exempt */
	}

	/* Allowed NEW user */
	ucounter->allowed_unknown_users++;

	bump_connect_counter(1);

	return 0;
}

int ct_rconnect(Client *client)
{
	int score;

	if (!IsSynched(client->uplink))
		return 0; /* Netmerge: skip */

	if (IsULine(client))
		return 0; /* U:lined, such as services: skip */

#if UNREAL_VERSION_TIME >= 201915
	/* On UnrealIRCd 4.2.3+ we can see the boot time (start time)
	 * of the remote server. This way we can apply the
	 * set::disabled-when::start-delay restriction on remote
	 * servers as well.
	 */
	if (client->uplink && client->uplink->server && client->uplink->server->boottime &&
	    (TStime() - client->uplink->server->boottime < cfg.start_delay))
	{
		return 0;
	}
#endif

	if (user_allowed_by_security_group(client, cfg.except) ||
	    find_tkl_exception(TKL_CONNECT_FLOOD, client))
		return 0; /* user is exempt */

	bump_connect_counter(0);

	return 0;
}

static const char *ct_module_status_text(void)
{
	static char buf[256];

	if (ucounter->disabled)
		return "Module DISABLED on oper request. To re-enable, type: /THROTTLE ON";
	if (still_reputation_gathering())
		return "Module DISABLED because the 'reputation' module has not gathered enough data yet (set::connthrottle::disabled-when::reputation-gathering).";
	if (me.local->creationtime + cfg.start_delay > TStime())
	{
		snprintf(buf, sizeof(buf),
		         "Module DISABLED due to start-delay (set::connthrottle::disabled-when::start-delay), will be enabled in %lld second(s).",
		         (long long)((me.local->creationtime + cfg.start_delay) - TStime()));
		return buf;
	}
	return "Module ENABLED";
}

static void ct_throttle_usage(Client *client)
{
	sendnotice(client, "Usage: /THROTTLE [ON|OFF|STATUS|RESET]");
	sendnotice(client, " ON:     Enabled protection");
	sendnotice(client, " OFF:    Disables protection");
	sendnotice(client, " STATUS: Status report");
	sendnotice(client, " RESET:  Resets all counters(&more)");
	sendnotice(client, "NOTE: All commands only affect this server. Remote servers are not affected.");
}

void ct_off(Client *client)
{
	if (ucounter->disabled)
		return; /* Already off */

	ucounter->disabled = 1;
	unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTTLE_MODULE_DISABLED", client,
		   "[ConnThrottle] $client.details DISABLED the connthrottle module.");
}

void ct_on(Client *client)
{
	if (!ucounter->disabled)
		return; /* Already on */

	unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTTLE_MODULE_ENABLED", client,
		   "[ConnThrottle] $client.details ENABLED the connthrottle module.");
	ucounter->disabled = 0;
}

void ct_reset(Client *client)
{
	memset(ucounter, 0, sizeof(UCounter));
	unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTTLE_RESET", client,
		   "[ConnThrottle] $client.details did a RESET on the statistics/counters.");
}

CMD_FUNC(ct_throttle)
{
	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		ct_throttle_usage(client);
		return;
	}

	if (!strcasecmp(parv[1], "STATS") || !strcasecmp(parv[1], "STATUS"))
	{
		sendnotice(client, "STATUS:");
		sendnotice(client, "%s", ct_module_status_text());
	} else
	if (!strcasecmp(parv[1], "OFF"))
	{
		if (ucounter->disabled == 1)
		{
			sendnotice(client, "Already OFF");
			return;
		}
		ct_off(client);
	} else
	if (!strcasecmp(parv[1], "ON"))
	{
		if (ucounter->disabled == 0)
		{
			sendnotice(client, "Already ON");
			return;
		}
		ct_on(client);
	} else
	if (!strcasecmp(parv[1], "RESET"))
	{
		ct_reset(client);
	} else
	{
		sendnotice(client, "Unknown option '%s'", parv[1]);
		ct_throttle_usage(client);
	}
}

void ucounter_free(ModData *m)
{
	safe_free(ucounter);
}

/* /STATS 9 (or /STATS connthrottle) — dumps module status, configured per-tier
 * limits, and the IPv6 prefix bucket contents. Empty buckets are filtered.
 */
int stats_connthrottle(Client *client, const char *para)
{
	int tier, i;
	ConnThrottleBucket *b;
	char ipbuf[64];

	if (strcmp(para, "9") && strcasecmp(para, "connthrottle"))
		return 0;

	if (!ValidatePermissionsForPath("server:info:stats", client, NULL, NULL, NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return 0;
	}

	sendtxtnumeric(client, "%s", ct_module_status_text());

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
		sendtxtnumeric(client, "Limit /%d = %d",
		               ct_tier_prefix[tier],
		               cfg.ipv6_unknown_users_limit[tier]);

	sendtxtnumeric(client, "IPv6 prefix buckets:");
	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		for (i = 0; i < CT_BUCKET_HASH_SIZE; i++)
		{
			for (b = ct_bucket_hash[tier][i]; b; b = b->next)
			{
				if (b->known_users == 0 && b->excepted_unknowns == 0 && b->unknown_users == 0)
					continue;
				if (!inet_ntop(AF_INET6, b->rawip, ipbuf, sizeof(ipbuf)))
					strlcpy(ipbuf, "<invalid>", sizeof(ipbuf));
				sendtxtnumeric(client, "%s/%d known=%d excepted=%d unknown=%d",
				               ipbuf, ct_tier_prefix[tier],
				               b->known_users, b->excepted_unknowns, b->unknown_users);
			}
		}
	}
	return 1;
}

#if defined(CONNTHROTTLE_CHECK)
static void ct_check_walk_one(Client *client)
{
	ConnThrottleBucket *b;
	ConnThrottleCategory category;
	int tier;
	char masked[16];

	category = CT_CATEGORY(client);
	if (category == CT_CATEGORY_NONE)
		return;	/* not in our buckets */
	if (!IsIPV6(client) || !client->ip)
	{
		unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_CHECK_NO_IP", client,
		           "[BUG] connthrottle counter check: client has category but no IPv6 IP");
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}
	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		ct_make_rawip(client, tier, masked);
		b = ct_find_bucket(tier, masked);
		if (!b)
		{
			unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_CHECK_NO_BUCKET", client,
			           "[BUG] connthrottle counter check: client has category but no bucket at /$prefix",
			           log_data_integer("prefix", ct_tier_prefix[tier]));
#ifdef DEBUGMODE
			abort();
#endif
			continue;
		}
		switch (category)
		{
			case CT_CATEGORY_KNOWN_USERS:       b->check_known++;    break;
			case CT_CATEGORY_EXCEPTED_UNKNOWNS: b->check_excepted++; break;
			case CT_CATEGORY_UNKNOWN_USERS:     b->check_unknown++;  break;
			case CT_CATEGORY_NONE:              break;	/* unreachable per filter */
		}
	}
}

EVENT(ct_check)
{
	ConnThrottleBucket *b;
	Client *client;
	int tier, i;

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
		for (i = 0; i < CT_BUCKET_HASH_SIZE; i++)
			for (b = ct_bucket_hash[tier][i]; b; b = b->next)
				b->check_known = b->check_excepted = b->check_unknown = 0;

	list_for_each_entry(client, &client_list, client_node)
		ct_check_walk_one(client);
	list_for_each_entry(client, &unknown_list, lclient_node)
		ct_check_walk_one(client);

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		for (i = 0; i < CT_BUCKET_HASH_SIZE; i++)
		{
			for (b = ct_bucket_hash[tier][i]; b; b = b->next)
			{
				if (b->check_known    != b->known_users ||
				    b->check_excepted != b->excepted_unknowns ||
				    b->check_unknown  != b->unknown_users)
				{
					unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_CHECK_DRIFT", NULL,
					           "[BUG] connthrottle bucket counter drift at /$prefix: "
					           "live=(known=$live_k excepted=$live_e unknown=$live_u) "
					           "computed=(known=$exp_k excepted=$exp_e unknown=$exp_u)",
					           log_data_integer("prefix", ct_tier_prefix[tier]),
					           log_data_integer("live_k", b->known_users),
					           log_data_integer("live_e", b->excepted_unknowns),
					           log_data_integer("live_u", b->unknown_users),
					           log_data_integer("exp_k", b->check_known),
					           log_data_integer("exp_e", b->check_excepted),
					           log_data_integer("exp_u", b->check_unknown));
#ifdef DEBUGMODE
					abort();
#endif
				}
			}
		}
	}
}
#endif

/* ==================== RPC HANDLERS ==================== */

RPC_CALL_FUNC(rpc_connthrottle_status)
{
	json_t *result, *counters, *config, *stats;
	time_t start_delay_end;
	int start_delay_remaining;
	int reputation_gathering;

	result = json_object();

	/* Basic state */
	json_object_set_new(result, "enabled", json_boolean(!ucounter->disabled));
	json_object_set_new(result, "throttling_this_minute", json_boolean(ucounter->throttling_this_minute));
	json_object_set_new(result, "throttling_previous_minute", json_boolean(ucounter->throttling_previous_minute));

	/* Calculate state info */
	start_delay_end = me.local->creationtime + cfg.start_delay;
	if (start_delay_end > TStime())
		start_delay_remaining = (int)(start_delay_end - TStime());
	else
		start_delay_remaining = 0;
	reputation_gathering = still_reputation_gathering();

	/* Determine overall state */
	if (ucounter->disabled)
		json_object_set_new(result, "state", json_string_unreal("disabled_by_oper"));
	else if (start_delay_remaining > 0)
		json_object_set_new(result, "state", json_string_unreal("start_delay"));
	else if (reputation_gathering)
		json_object_set_new(result, "state", json_string_unreal("reputation_gathering"));
	else if (ucounter->throttling_this_minute)
		json_object_set_new(result, "state", json_string_unreal("throttling"));
	else
		json_object_set_new(result, "state", json_string_unreal("monitoring"));

	json_object_set_new(result, "start_delay_remaining", json_integer(start_delay_remaining));
	json_object_set_new(result, "reputation_gathering", json_boolean(reputation_gathering));

	/* Current counters */
	counters = json_object();
	json_object_set_new(counters, "local_count", json_integer(ucounter->local.count));
	json_object_set_new(counters, "global_count", json_integer(ucounter->global.count));
	if (ucounter->local.t > 0)
		json_object_set_new(counters, "local_period_start", json_timestamp(ucounter->local.t));
	if (ucounter->global.t > 0)
		json_object_set_new(counters, "global_period_start", json_timestamp(ucounter->global.t));
	json_object_set_new(result, "counters", counters);

	/* Statistics for last minute */
	stats = json_object();
	json_object_set_new(stats, "rejected_clients", json_integer(ucounter->rejected_clients));
	json_object_set_new(stats, "allowed_except", json_integer(ucounter->allowed_except));
	json_object_set_new(stats, "allowed_unknown_users", json_integer(ucounter->allowed_unknown_users));
	json_object_set_new(result, "stats_last_minute", stats);

	/* Configuration */
	config = json_object();
	json_object_set_new(config, "local_throttle_count", json_integer(cfg.local.count));
	json_object_set_new(config, "local_throttle_period", json_integer(cfg.local.period));
	json_object_set_new(config, "global_throttle_count", json_integer(cfg.global.count));
	json_object_set_new(config, "global_throttle_period", json_integer(cfg.global.period));
	json_object_set_new(config, "start_delay", json_integer(cfg.start_delay));
	json_expand_security_group(config, "except", cfg.except, 1);
	json_object_set_new(result, "config", config);

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_connthrottle_set)
{
	json_t *result;
	int enabled;
	const char *parv[3];

	REQUIRE_PARAM_BOOLEAN("enabled", enabled);

	if (enabled)
		ct_on(client);
	else
		ct_off(client);

	result = json_object();
	json_object_set_new(result, "success", json_boolean(1));
	json_object_set_new(result, "enabled", json_boolean(!ucounter->disabled));

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_connthrottle_reset)
{
	json_t *result;
	const char *parv[3];

	ct_reset(client);

	result = json_object();
	json_object_set_new(result, "success", json_boolean(1));

	rpc_response(client, request, result);
	json_decref(result);
}

/* ===== IPv6 wider-prefix bucket tracking ===== */

/** Build the masked rawip used as the bucket key for this tier. */
static void ct_make_rawip(Client *client, int tier, char *out)
{
	mask_ipv6_rawip(client->rawip, ct_tier_prefix[tier], out);
}

static uint64_t ct_hash_bucket(const char *masked)
{
	return siphash_raw(masked, 16, siphashkey_ct_buckets) % CT_BUCKET_HASH_SIZE;
}

static ConnThrottleBucket *ct_find_bucket(int tier, const char *masked)
{
	int hash = ct_hash_bucket(masked);
	ConnThrottleBucket *p;

	for (p = ct_bucket_hash[tier][hash]; p; p = p->next)
		if (memcmp(p->rawip, masked, 16) == 0)
			return p;
	return NULL;
}

static ConnThrottleBucket *ct_add_bucket(int tier, const char *masked)
{
	int hash = ct_hash_bucket(masked);
	ConnThrottleBucket *n = safe_alloc(sizeof(ConnThrottleBucket));
	memcpy(n->rawip, masked, 16);
	AddListItem(n, ct_bucket_hash[tier][hash]);
	return n;
}

static void ct_bucket_increment(ConnThrottleBucket *b, ConnThrottleCategory category)
{
	switch (category)
	{
		case CT_CATEGORY_NONE:              break;	/* not classified: no-op */
		case CT_CATEGORY_KNOWN_USERS:       b->known_users++;       break;
		case CT_CATEGORY_EXCEPTED_UNKNOWNS: b->excepted_unknowns++; break;
		case CT_CATEGORY_UNKNOWN_USERS:     b->unknown_users++;     break;
	}
}

static void ct_bucket_decrement(ConnThrottleBucket *b, ConnThrottleCategory category)
{
	switch (category)
	{
		case CT_CATEGORY_NONE:              break;	/* not classified: no-op */
		case CT_CATEGORY_KNOWN_USERS:       b->known_users--;       break;
		case CT_CATEGORY_EXCEPTED_UNKNOWNS: b->excepted_unknowns--; break;
		case CT_CATEGORY_UNKNOWN_USERS:     b->unknown_users--;     break;
	}
	if ((b->known_users < 0) || (b->excepted_unknowns < 0) || (b->unknown_users < 0))
	{
		unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_NEGATIVE_COUNTER", NULL,
		           "[BUG] connthrottle bucket counter went negative: known=$known excepted=$excepted unknown=$unknown",
		           log_data_integer("known", b->known_users),
		           log_data_integer("excepted", b->excepted_unknowns),
		           log_data_integer("unknown", b->unknown_users));
#ifdef DEBUGMODE
		abort();
#endif
	}
}

/** Classify a client into one of CT_CATEGORY_*.
 * Reads client->known_user_cached, the cfg.except SecurityGroup, and TKL
 * exceptions of type maxperip. Does not modify state.
 */
static ConnThrottleCategory ct_classify(Client *client)
{
	if (client->known_user_cached)
		return CT_CATEGORY_KNOWN_USERS;
	if (user_allowed_by_security_group(client, cfg.except) ||
	    find_tkl_exception(TKL_MAXPERIP, client))
		return CT_CATEGORY_EXCEPTED_UNKNOWNS;
	return CT_CATEGORY_UNKNOWN_USERS;
}

/** Bump the bucket counter for this client's category at all 3 tiers. */
static void ct_bucket_bump_client(Client *client, ConnThrottleCategory category)
{
	int tier;
	char masked[16];
	ConnThrottleBucket *b;

	if (!IsIPV6(client) || !client->ip)
		return;

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		ct_make_rawip(client, tier, masked);
		b = ct_find_bucket(tier, masked);
		if (!b)
			b = ct_add_bucket(tier, masked);
		ct_bucket_increment(b, category);
	}
}

/** Decrement the bucket counter for this client's category at all 3 tiers,
 * freeing any bucket whose total reaches 0.
 */
static void ct_bucket_unbump_client(Client *client, ConnThrottleCategory category)
{
	int tier;
	char masked[16];
	ConnThrottleBucket *b;

	if (!IsIPV6(client) || !client->ip)
		return;

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		ct_make_rawip(client, tier, masked);
		b = ct_find_bucket(tier, masked);
		if (!b)
		{
			unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_BUCKET_MISSING", client,
			           "[BUG] connthrottle bucket missing on disconnect for client $client.details");
#ifdef DEBUGMODE
			abort();
#endif
			continue;
		}
		ct_bucket_decrement(b, category);
		if ((b->known_users == 0) && (b->excepted_unknowns == 0) && (b->unknown_users == 0))
		{
			DelListItem(b, ct_bucket_hash[tier][ct_hash_bucket(masked)]);
			safe_free(b);
		}
	}
}

/** HOOKTYPE_ALLOW_CLIENT: classify the client, add to buckets, and
 * reject if the unknown_users count for this client's category exceeds
 * the limit at any tier. Multiple matched allow blocks may invoke this
 * hook for the same client; the cached CT_CATEGORY (non-NONE means
 * "already bucketed") prevents double-counting.
 */
const char *ct_allow_client(Client *client, ConfigItem_allow *aconf)
{
	ConnThrottleCategory category;
	int tier;
	char masked[16];
	ConnThrottleBucket *b;
	int global_limit, effective_limit;

	if (!IsIPV6(client) || !client->ip)
		return NULL;
	if (CT_CATEGORY(client) != CT_CATEGORY_NONE)
		return NULL;

	category = ct_classify(client);
	CT_CATEGORY(client) = category;
	ct_bucket_bump_client(client, category);

	/* Limits fire only on unknown users; known + excepted bypass. */
	if (category != CT_CATEGORY_UNKNOWN_USERS)
		return NULL;

	/* Same dormancy gates as the existing rate-throttle. */
	if (me.local->creationtime + cfg.start_delay > TStime())
		return NULL;
	if (ucounter->disabled)
		return NULL;
	if (still_reputation_gathering())
		return NULL;

	/* effective_limit = max(global tier limit, allow::global-maxperip).
	 * A tier with limit 0 is disabled.
	 * We compare against allow::global-maxperip (not allow::maxperip) because
	 * our buckets count globally (local + remote), so the matched-scope cap
	 * is global-maxperip — same scope-pairing maxperip itself uses.
	 */
	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		global_limit = cfg.ipv6_unknown_users_limit[tier];
		if (global_limit == 0)
			continue;
		effective_limit = global_limit;
		if (aconf && aconf->global_maxperip > effective_limit)
			effective_limit = aconf->global_maxperip;
		ct_make_rawip(client, tier, masked);
		b = ct_find_bucket(tier, masked);
		if (b && (b->unknown_users > effective_limit))
		{
			unreal_log(ULOG_INFO, "connthrottle", "CONNTHROTTLE_IPV6_LIMIT", client,
			    "Client $client.name with IP $client.ip rejected: connthrottle ipv6-unknown-users-limit (cidr-$prefix_len, max $max) exceeded for $prefix_addr/$prefix_len ($unknown_users unknown / $excepted_users excepted / $known_users known)",
			    log_data_string("prefix_addr", format_ipv6_addr(masked)),
			    log_data_integer("prefix_len", ct_tier_prefix[tier]),
			    log_data_integer("max", effective_limit),
			    log_data_integer("unknown_users", b->unknown_users),
			    log_data_integer("excepted_users", b->excepted_unknowns),
			    log_data_integer("known_users", b->known_users));
			return format_ipv6_prefix_reject_message(
			    iConf.reject_message_too_many_new_connections_ipv6_range,
			    masked, ct_tier_prefix[tier]);
		}
	}

	return NULL;
}

/** HOOKTYPE_REMOTE_CONNECT: track remote IPv6 clients in the global buckets.
 * No netmerge skip (unlike ct_rconnect): netmerge'd users are real concurrent
 * presence, and our limits are state-based, not rate-based.
 */
int ct_remote_connect_buckets(Client *client)
{
	ConnThrottleCategory category;

	if (IsULine(client))
		return 0;	/* U-lined service: skip */
	if (!IsIPV6(client) || !client->ip)
		return 0;
	if (CT_CATEGORY(client) != CT_CATEGORY_NONE)
		return 0;

	category = ct_classify(client);
	CT_CATEGORY(client) = category;
	ct_bucket_bump_client(client, category);

	return 0;
}

int ct_quit(Client *client, MessageTag *mtags, const char *comment)
{
	ConnThrottleCategory category = CT_CATEGORY(client);

	if (category == CT_CATEGORY_NONE)
		return 0;

	ct_bucket_unbump_client(client, category);
	CT_CATEGORY(client) = CT_CATEGORY_NONE;
	return 0;
}

/** HOOKTYPE_KNOWN_USER_CACHE_CHANGE: when client->known_user_cached
 * flips, our classification may change. Recompute and update the
 * three bucket counters in place if the category actually changed.
 */
int ct_known_user_cache_change(Client *client)
{
	ConnThrottleCategory old_category, new_category;
	int tier;
	char masked[16];
	ConnThrottleBucket *b;

	if (!IsIPV6(client) || !client->ip)
		return 0;
	old_category = CT_CATEGORY(client);
	if (old_category == CT_CATEGORY_NONE)
		return 0;

	new_category = ct_classify(client);
	if (new_category == old_category)
		return 0;

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		ct_make_rawip(client, tier, masked);
		b = ct_find_bucket(tier, masked);
		if (!b)
		{
			unreal_log(ULOG_ERROR, "connthrottle", "BUG_CT_BUCKET_MISSING", client,
			           "[BUG] connthrottle bucket missing on transition for client $client.details");
#ifdef DEBUGMODE
			abort();
#endif
			continue;
		}
		ct_bucket_decrement(b, old_category);
		ct_bucket_increment(b, new_category);
	}

	CT_CATEGORY(client) = new_category;
	return 0;
}

/** Walk live clients and rebuild the three bucket hash tables.
 * Called from MOD_LOAD; mirrors maxperip's rebuild pattern.
 * Cost is negligible (a few tens of milliseconds even for ~10k clients).
 */
static void ct_buckets_rebuild(void)
{
	Client *client;
	ConnThrottleCategory category;

	list_for_each_entry(client, &client_list, client_node)
	{
		if (!IsUser(client) || IsULine(client))
			continue;	/* only regular IRC users; skip servers, services, pre-registration */
		if (!IsIPV6(client) || !client->ip)
			continue;
		category = ct_classify(client);
		CT_CATEGORY(client) = category;
		ct_bucket_bump_client(client, category);
	}
	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (!IsUser(client) || IsULine(client))
			continue;	/* only regular IRC users; skip servers, services, pre-registration */
		if (!IsIPV6(client) || !client->ip)
			continue;
		category = ct_classify(client);
		CT_CATEGORY(client) = category;
		ct_bucket_bump_client(client, category);
	}
}

/** Free every bucket in every tier hash table, then free the tables themselves. */
static void ct_buckets_free(void)
{
	int tier, i;
	ConnThrottleBucket *p, *next;

	for (tier = 0; tier < CT_NUM_TIERS; tier++)
	{
		if (!ct_bucket_hash[tier])
			continue;
		for (i = 0; i < CT_BUCKET_HASH_SIZE; i++)
		{
			for (p = ct_bucket_hash[tier][i]; p; p = next)
			{
				next = p->next;
				safe_free(p);
			}
			ct_bucket_hash[tier][i] = NULL;
		}
		safe_free(ct_bucket_hash[tier]);
		ct_bucket_hash[tier] = NULL;
	}
}
