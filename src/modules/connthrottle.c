/*
 * connthrottle - Connection throttler
 * (C) Copyright 2004-2020 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
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
	"unrealircd-5",
    };

typedef struct {
	int count;
	int period;
} ThrottleSetting;

struct cfgstruct {
	/* set::connthrottle::known-users: */
	ThrottleSetting local;
	ThrottleSetting global;
	/* set::connthrottle::new-users: */
	int minimum_reputation_score;
	int sasl_bypass;
	int webirc_bypass;
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
	int allowed_score;		/**< Number of allowed clients of type known-user */
	int allowed_sasl;		/**< Number of allowed clients of type SASL */
	int allowed_webirc;		/**< Number of allowed clients of type WEBIRC */
	int allowed_other;		/**< Number of allowed clients of type other (new) */
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
	cfg.minimum_reputation_score = 24;
	cfg.sasl_bypass = 1;
	cfg.webirc_bypass = 0;

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

#ifndef CheckNull
 #define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#endif
/** Test the set::connthrottle configuration */
int ct_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::connthrottle.. */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "connthrottle"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "known-users"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "minimum-reputation-score"))
				{
					int cnt = atoi(cepp->ce_vardata);
					if (cnt < 1)
					{
						config_error("%s:%i: set::connthrottle::known-users::minimum-reputation-score should be at least 1",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->ce_varname, "sasl-bypass"))
				{
				} else
				if (!strcmp(cepp->ce_varname, "webirc-bypass"))
				{
				} else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
					                     "set::connthrottle::known-users", cepp->ce_varname);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->ce_varname, "new-users"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "local-throttle"))
				{
					int cnt, period;
					if (!config_parse_flood(cepp->ce_vardata, &cnt, &period) ||
					    (cnt < 1) || (cnt > 2000000000) || (period > 2000000000))
					{
						config_error("%s:%i: set::connthrottle::new-users::local-throttle error. "
							     "Syntax is <count>:<period> (eg 6:60), "
							     "and count and period should be non-zero.",
							     cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->ce_varname, "global-throttle"))
				{
					int cnt, period;
					if (!config_parse_flood(cepp->ce_vardata, &cnt, &period) ||
					    (cnt < 1) || (cnt > 2000000000) || (period > 2000000000))
					{
						config_error("%s:%i: set::connthrottle::new-users::global-throttle error. "
							     "Syntax is <count>:<period> (eg 6:60), "
							     "and count and period should be non-zero.",
							     cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
				} else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
					                     "set::connthrottle::new-users", cepp->ce_varname);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->ce_varname, "disabled-when"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				CheckNull(cepp);
				if (!strcmp(cepp->ce_varname, "start-delay"))
				{
					int cnt = config_checkval(cepp->ce_vardata, CFG_TIME);
					if ((cnt < 0) || (cnt > 3600))
					{
						config_error("%s:%i: set::connthrottle::disabled-when::start-delay should be in range 0-3600",
							cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum);
						errors++;
						continue;
					}
				} else
				if (!strcmp(cepp->ce_varname, "reputation-gathering"))
				{
				} else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename, cepp->ce_varlinenum,
					                     "set::connthrottle::disabled-when", cepp->ce_varname);
					errors++;
				}
			}
		} else
		if (!strcmp(cep->ce_varname, "reason"))
		{
			CheckNull(cep);
		} else
		{
			config_error("%s:%i: unknown directive set::connthrottle::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
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
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "connthrottle"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "known-users"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "minimum-reputation-score"))
					cfg.minimum_reputation_score = atoi(cepp->ce_vardata);
				else if (!strcmp(cepp->ce_varname, "sasl-bypass"))
					cfg.sasl_bypass = config_checkval(cepp->ce_vardata, CFG_YESNO);
				else if (!strcmp(cepp->ce_varname, "webirc-bypass"))
					cfg.webirc_bypass = config_checkval(cepp->ce_vardata, CFG_YESNO);
			}
		} else
		if (!strcmp(cep->ce_varname, "new-users"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "local-throttle"))
					config_parse_flood(cepp->ce_vardata, &cfg.local.count, &cfg.local.period);
				else if (!strcmp(cepp->ce_varname, "global-throttle"))
					config_parse_flood(cepp->ce_vardata, &cfg.global.count, &cfg.global.period);
			}
		} else
		if (!strcmp(cep->ce_varname, "disabled-when"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "start-delay"))
					cfg.start_delay = config_checkval(cepp->ce_vardata, CFG_TIME);
				else if (!strcmp(cepp->ce_varname, "reputation-gathering"))
					cfg.reputation_gathering = config_checkval(cepp->ce_vardata, CFG_TIME);
			}
		} else
		if (!strcmp(cep->ce_varname, "reason"))
		{
			safe_free(cfg.reason);
			cfg.reason = safe_alloc(strlen(cep->ce_vardata)+16);
			sprintf(cfg.reason, "Throttled: %s", cep->ce_vardata);
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
		snprintf(buf, sizeof(buf),
		         "[ConnThrottle] Stats for this server past 60 secs: Connections rejected: %d. Accepted: %d known user(s), %d SASL, %d WEBIRC and %d new user(s).",
		         ucounter->rejected_clients,
		         ucounter->allowed_score,
		         ucounter->allowed_sasl,
			 ucounter->allowed_webirc,
		         ucounter->allowed_other);

		sendto_realops("%s", buf);
		ircd_log(LOG_ERROR, "%s", buf);
	}

	/* Reset stats for next message */
	ucounter->rejected_clients = 0;
	ucounter->allowed_score = 0;
	ucounter->allowed_sasl = 0;
	ucounter->allowed_webirc = 0;
	ucounter->allowed_other = 0;

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

	if (me.local->firsttime + cfg.start_delay > TStime())
		return HOOK_CONTINUE; /* no throttle: start delay */

	if (ucounter->disabled)
		return HOOK_CONTINUE; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return HOOK_CONTINUE; /* still gathering reputation data */

	if (cfg.sasl_bypass && IsLoggedIn(client))
	{
		/* Allowed in: user authenticated using SASL */
		return HOOK_CONTINUE;
	}
	
	if (cfg.webirc_bypass && moddata_client_get(client, "webirc"))
	{
		/* Allowed in: user using WEBIRC */
		return HOOK_CONTINUE;
	}

	score = GetReputation(client);
	if (score >= cfg.minimum_reputation_score)
	{
		/* Allowed in: IP has enough reputation ("known user") */
		return HOOK_CONTINUE;
	}

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
			ircd_log(LOG_ERROR, "[ConnThrottle] Connection throttling has been ACTIVATED due to a HIGH CONNECTION RATE.");
			sendto_realops("[ConnThrottle] Connection throttling has been ACTIVATED due to a HIGH CONNECTION RATE.");
			sendto_realops("[ConnThrottle] Users with IP addresses that have not been seen before will be rejected above the set connection rate. Known users can still get in.");
			sendto_realops("[ConnThrottle] For more information see https://www.unrealircd.org/docs/ConnThrottle");
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

	if (me.local->firsttime + cfg.start_delay > TStime())
		return 0; /* no throttle: start delay */

	if (ucounter->disabled)
		return 0; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return 0; /* still gathering reputation data */

	if (cfg.sasl_bypass && IsLoggedIn(client))
	{
		/* Allowed in: user authenticated using SASL */
		ucounter->allowed_sasl++;
		return 0;
	}
	
	if (cfg.webirc_bypass && moddata_client_get(client, "webirc"))
	{
		/* Allowed in: user using WEBIRC */
		ucounter->allowed_webirc++;
		return 0;
	}

	score = GetReputation(client);
	if (score >= cfg.minimum_reputation_score)
	{
		/* Allowed in: IP has enough reputation ("known user") */
		ucounter->allowed_score++;
		return 0;
	}

	/* Allowed NEW user */
	ucounter->allowed_other++;

	bump_connect_counter(1);

	return 0;
}

int ct_rconnect(Client *client)
{
	int score;

	if (client->srvptr && !IsSynched(client->srvptr))
		return 0; /* Netmerge: skip */

	if (IsULine(client))
		return 0; /* U:lined, such as services: skip */

#if UNREAL_VERSION_TIME >= 201915
	/* On UnrealIRCd 4.2.3+ we can see the boot time (start time)
	 * of the remote server. This way we can apply the
	 * set::disabled-when::start-delay restriction on remote
	 * servers as well.
	 */
	if (client->srvptr && client->srvptr->serv && client->srvptr->serv->boottime &&
	    (TStime() - client->srvptr->serv->boottime < cfg.start_delay))
	{
		return 0;
	}
#endif

	score = GetReputation(client);
	if (score >= cfg.minimum_reputation_score)
		return 0; /* sufficient reputation: "known-user" */

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
			if (me.local->firsttime + cfg.start_delay > TStime())
			{
				sendnotice(client, "Module DISABLED due to start-delay (set::connthrottle::disabled-when::start-delay), will be enabled in %lld second(s).",
					(long long)((me.local->firsttime + cfg.start_delay) - TStime()));
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
		sendto_realops("[connthrottle] %s (%s@%s) DISABLED the connthrottle module.",
			client->name, client->user->username, client->user->realhost);
	} else
	if (!strcasecmp(parv[1], "ON"))
	{
		if (ucounter->disabled == 0)
		{
			sendnotice(client, "Already ON");
			return;
		}
		sendto_realops("[connthrottle] %s (%s@%s) ENABLED the connthrottle module.",
			client->name, client->user->username, client->user->realhost);
		ucounter->disabled = 0;
	} else
	if (!strcasecmp(parv[1], "RESET"))
	{
		memset(ucounter, 0, sizeof(UCounter));
		sendto_realops("[connthrottle] %s (%s@%s) did a RESET on the stats/counters!!",
			client->name, client->user->username, client->user->realhost);
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
