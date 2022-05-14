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
	int allowed_unknown_users;		/**< Number of allowed clients - not on except list */
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

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	
	/* Defaults: */
	cfg.local.count = 20; cfg.local.period = 60;
	cfg.global.count = 30; cfg.global.period = 60;
	cfg.start_delay = 180;		/* 3 minutes */
	safe_strdup(cfg.reason, "Throttled: Too many users trying to connect, please wait a while and try again");
	cfg.except = safe_alloc(sizeof(SecurityGroup));
	cfg.except->reputation_score = 24;
	cfg.except->identified = 1;
	cfg.except->webirc = 0;

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, ct_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, ct_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, ucounter, ucounter_free);
	if (!ucounter)
		ucounter = safe_alloc(sizeof(UCounter));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, ct_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, ct_pre_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, ct_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, ct_rconnect);
	CommandAdd(modinfo->handle, MSG_THROTTLE, ct_throttle, MAXPARA, CMD_USER|CMD_SERVER);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "connthrottle_evt", connthrottle_evt, NULL, 1000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, ucounter);
	safe_free(cfg.reason);
	free_security_group(cfg.except);
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
	ConfigEntry *cep, *cepp;

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
	ConfigEntry *cep, *cepp;

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
		unreal_log(ULOG_INFO, "connthrottle", "CONNTHROTLE_REPORT", NULL,
		           "ConnThrottle] Stats for this server past 60 secs: "
		           "Connections rejected: $num_rejected. "
		           "Accepted: $num_accepted_known_users known user(s), "
		           "$num_accepted_sasl SASL, "
		           "$num_accepted_webirc WEBIRC and "
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

	if (user_allowed_by_security_group(client, cfg.except))
		return HOOK_CONTINUE; /* allowed: user is exempt (known user or otherwise) */

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
			unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTLE_ACTIVATED", NULL,
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

	if (user_allowed_by_security_group(client, cfg.except))
	{
		ucounter->allowed_except++;
		return HOOK_CONTINUE; /* allowed: user is exempt (known user or otherwise) */
	}

	/* Allowed NEW user */
	ucounter->allowed_unknown_users++;

	bump_connect_counter(1);

	return 0;
}

int ct_rconnect(Client *client)
{
	int score;

	if (client->uplink && !IsSynched(client->uplink))
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

	if (user_allowed_by_security_group(client, cfg.except))
		return 0; /* user is on except list (known user or otherwise) */

	bump_connect_counter(0);

	return 0;
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
		if (ucounter->disabled)
		{
			sendnotice(client, "Module DISABLED on oper request. To re-enable, type: /THROTTLE ON");
		} else {
			if (still_reputation_gathering())
			{
				sendnotice(client, "Module DISABLED because the 'reputation' module has not gathered enough data yet (set::connthrottle::disabled-when::reputation-gathering).");
			} else
			if (me.local->creationtime + cfg.start_delay > TStime())
			{
				sendnotice(client, "Module DISABLED due to start-delay (set::connthrottle::disabled-when::start-delay), will be enabled in %lld second(s).",
					(long long)((me.local->creationtime + cfg.start_delay) - TStime()));
			} else
			{
				sendnotice(client, "Module ENABLED");
			}
		}
	} else 
	if (!strcasecmp(parv[1], "OFF"))
	{
		if (ucounter->disabled == 1)
		{
			sendnotice(client, "Already OFF");
			return;
		}
		ucounter->disabled = 1;
		unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTLE_MODULE_DISABLED", client,
			   "[ConnThrottle] $client.details DISABLED the connthrottle module.");
	} else
	if (!strcasecmp(parv[1], "ON"))
	{
		if (ucounter->disabled == 0)
		{
			sendnotice(client, "Already ON");
			return;
		}
		unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTLE_MODULE_ENABLED", client,
			   "[ConnThrottle] $client.details ENABLED the connthrottle module.");
		ucounter->disabled = 0;
	} else
	if (!strcasecmp(parv[1], "RESET"))
	{
		memset(ucounter, 0, sizeof(UCounter));
		unreal_log(ULOG_WARNING, "connthrottle", "CONNTHROTLE_RESET", client,
			   "[ConnThrottle] $client.details did a RESET on the statistics/counters.");
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
