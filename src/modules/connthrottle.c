/*
 * connthrottle - Connection throttler
 * (C) Copyright 2004-2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 * See https://www.unrealircd.org/docs/Connthrottle
 */

#include "unrealircd.h"

#define CONNTHROTTLE_VERSION "1.2"

#ifndef CALLBACKTYPE_REPUTATION_STARTTIME
 #define CALLBACKTYPE_REPUTATION_STARTTIME 5
#endif

ModuleHeader MOD_HEADER(connthrottle)
  = {
	"connthrottle",
	CONNTHROTTLE_VERSION,
	"Connection throttler - by Syzop",
	"3.2-b8-1",
	NULL 
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

struct _ucounter {
	ThrottleCounter local;		/**< Local counter */
	ThrottleCounter global;		/**< Global counter */
	int rejected_clients;		/**< Number of rejected clients this minute */
	int allowed_score;		/**< Number of allowed clients of type known-user */
	int allowed_sasl;		/**< Number of allowed clients of type SASL */
	int allowed_other;		/**< Number of allowed clients of type other (new) */
	char disabled;			/**< Module disabled by oper? */
	int throttling_this_minute;	/**< Did we do any throttling this minute? */
	int throttling_previous_minute;	/**< Did we do any throttling previous minute? */
	int throttling_banner_displayed;/**< Big we-are-now-throttling banner displayed? */
	time_t next_event;		/**< When is next event? (for "last 60 seconds" stats) */
};
static struct _ucounter ucounter;

static char rehash_dump_filename[512];

#define MSG_THROTTLE "THROTTLE"

#define GetReputation(acptr)     (moddata_client_get(acptr, "reputation") ? atoi(moddata_client_get(acptr, "reputation")) : 0)

/* Forward declarations */
int ct_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int ct_config_posttest(int *errs);
int ct_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int ct_pre_lconnect(aClient *sptr);
int ct_lconnect(aClient *);
int ct_rconnect(aClient *);
CMD_FUNC(ct_throttle);
void rehash_dump_settings(void);
void rehash_read_settings(void);
EVENT(connthrottle_evt);

MOD_TEST(connthrottle)
{
	memset(&cfg, 0, sizeof(cfg));
	memset(&ucounter, 0, sizeof(ucounter));
	
	/* Defaults: */
	cfg.local.count = 20; cfg.local.period = 60;
	cfg.global.count = 30; cfg.global.period = 60;
	cfg.start_delay = 180;		/* 3 minutes */
	cfg.reason = strdup("Throttled: Too many users trying to connect, please wait a while and try again");
	cfg.minimum_reputation_score = 24;
	cfg.sasl_bypass = 1;

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, ct_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, ct_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT(connthrottle)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	snprintf(rehash_dump_filename, sizeof(rehash_dump_filename), "%s/connthrottle.tmp", TMPDIR);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, ct_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, ct_pre_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, ct_lconnect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, ct_rconnect);
	CommandAdd(modinfo->handle, MSG_THROTTLE, ct_throttle, MAXPARA, M_USER|M_SERVER);
	return MOD_SUCCESS;
}

MOD_LOAD(connthrottle)
{
	rehash_read_settings();
	EventAddEx(modinfo->handle, "connthrottle_evt", 1, 0, connthrottle_evt, NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(connthrottle)
{
	rehash_dump_settings();
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
			safefree(cfg.reason);
			cfg.reason = MyMalloc(strlen(cep->ce_vardata)+16);
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

	if (ucounter.next_event > TStime())
		return;
	ucounter.next_event = TStime() + 60;

	if (ucounter.rejected_clients)
	{
		snprintf(buf, sizeof(buf),
		         "[ConnThrottle] Stats for this server past 60 secs: Connections rejected: %d. Accepted: %d known user(s), %d SASL and %d new user(s).",
		         ucounter.rejected_clients,
		         ucounter.allowed_score,
		         ucounter.allowed_sasl,
		         ucounter.allowed_other);

		sendto_realops("%s", buf);
		ircd_log(LOG_ERROR, "%s", buf);
	}

	/* Reset stats for next message */
	ucounter.rejected_clients = 0;
	ucounter.allowed_score = 0;
	ucounter.allowed_sasl = 0;
	ucounter.allowed_other = 0;

	ucounter.throttling_previous_minute = ucounter.throttling_this_minute;
	ucounter.throttling_this_minute = 0; /* reset */
	ucounter.throttling_banner_displayed = 0; /* reset */
}

#define THROT_LOCAL 1
#define THROT_GLOBAL 2
int ct_pre_lconnect(aClient *sptr)
{
	int throttle=0;
	int score;

	if (me.local->firsttime + cfg.start_delay > TStime())
		return 0; /* no throttle: start delay */

	if (ucounter.disabled)
		return 0; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return 0; /* still gathering reputation data */

	if (cfg.sasl_bypass && IsLoggedIn(sptr))
	{
		/* Allowed in: user authenticated using SASL */
		return 0;
	}

	score = GetReputation(sptr);
	if (score >= cfg.minimum_reputation_score)
	{
		/* Allowed in: IP has enough reputation ("known user") */
		return 0;
	}

	/* If we reach this then the user is NEW */

	/* +1 global client would reach global limit? */
	if ((TStime() - ucounter.global.t < cfg.global.period) && (ucounter.global.count+1 > cfg.global.count))
		throttle |= THROT_GLOBAL;

	/* +1 local client would reach local limit? */
	if ((TStime() - ucounter.local.t < cfg.local.period) && (ucounter.local.count+1 > cfg.local.count))
		throttle |= THROT_LOCAL;

	if (throttle)
	{
		ucounter.throttling_this_minute = 1;
		ucounter.rejected_clients++;
		/* We send the LARGE banner if throttling was activated */
		if (!ucounter.throttling_previous_minute && !ucounter.throttling_banner_displayed)
		{
			ircd_log(LOG_ERROR, "[ConnThrottle] Connection throttling has been ACTIVATED due to a HIGH CONNECTION RATE.");
			sendto_realops("[ConnThrottle] Connection throttling has been ACTIVATED due to a HIGH CONNECTION RATE.");
			sendto_realops("[ConnThrottle] Users with IP addresses that have not been seen before will be rejected above the set connection rate. Known users can still get in.");
			sendto_realops("[ConnThrottle] For more information see https://www.unrealircd.org/docs/ConnThrottle");
			ucounter.throttling_banner_displayed = 1;
		}
		return exit_client(sptr, sptr, &me, cfg.reason);
	}

	return 0;
}

/** Increase the connect counter(s), nothing else. */
void bump_connect_counter(int local_connect)
{
	if (local_connect)
	{
		/* Bump local connect counter */
		if (TStime() - ucounter.local.t >= cfg.local.period)
		{
			ucounter.local.t = TStime();
			ucounter.local.count = 1;
		} else {
			ucounter.local.count++;
		}
	}

	/* Bump global connect counter */
	if (TStime() - ucounter.global.t >= cfg.global.period)
	{
		ucounter.global.t = TStime();
		ucounter.global.count = 1;
	} else {
		ucounter.global.count++;
	}
}

int ct_lconnect(aClient *sptr)
{
	int score;

	if (me.local->firsttime + cfg.start_delay > TStime())
		return 0; /* no throttle: start delay */

	if (ucounter.disabled)
		return 0; /* protection disabled: allow user */

	if (still_reputation_gathering())
		return 0; /* still gathering reputation data */

	if (cfg.sasl_bypass && IsLoggedIn(sptr))
	{
		/* Allowed in: user authenticated using SASL */
		ucounter.allowed_sasl++;
		return 0;
	}

	score = GetReputation(sptr);
	if (score >= cfg.minimum_reputation_score)
	{
		/* Allowed in: IP has enough reputation ("known user") */
		ucounter.allowed_score++;
		return 0;
	}

	/* Allowed NEW user */
	ucounter.allowed_other++;

	bump_connect_counter(1);

	return 0;
}

int ct_rconnect(aClient *sptr)
{
	int score;

	if (sptr->srvptr && !IsSynched(sptr->srvptr))
		return 0; /* Netmerge: skip */

	if (IsULine(sptr))
		return 0; /* U:lined, such as services: skip */

#if UNREAL_VERSION_TIME >= 201915
	/* On UnrealIRCd 4.2.3+ we can see the boot time (start time)
	 * of the remote server. This way we can apply the
	 * set::disabled-when::start-delay restriction on remote
	 * servers as well.
	 */
	if (sptr->srvptr && sptr->srvptr->serv && sptr->srvptr->serv->boottime &&
	    (TStime() - sptr->srvptr->serv->boottime < cfg.start_delay))
	{
		return 0;
	}
#endif

	score = GetReputation(sptr);
	if (score >= cfg.minimum_reputation_score)
		return 0; /* sufficient reputation: "known-user" */

	bump_connect_counter(0);

	return 0;
}

static void ct_throttle_usage(aClient *sptr)
{
	sendnotice(sptr, "Usage: /THROTTLE [ON|OFF|STATUS|RESET]");
	sendnotice(sptr, " ON:     Enabled protection");
	sendnotice(sptr, " OFF:    Disables protection");
	sendnotice(sptr, " STATUS: Status report");
	sendnotice(sptr, " RESET:  Resets all counters(&more)");
	sendnotice(sptr, "NOTE: All commands only affect this server. Remote servers are not affected.");
}

CMD_FUNC(ct_throttle)
{
	if (!IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES, me.name, sptr->name);
		return 0;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		ct_throttle_usage(sptr);
		return 0;
	}

	if (!strcasecmp(parv[1], "STATS") || !strcasecmp(parv[1], "STATUS"))
	{
		sendnotice(sptr, "STATUS:");
		if (ucounter.disabled)
		{
			sendnotice(sptr, "Module DISABLED on oper request. To re-enable, type: /THROTTLE ON");
		} else {
			if (still_reputation_gathering())
			{
				sendnotice(sptr, "Module DISABLED because the 'reputation' module has not gathered enough data yet (set::connthrottle::disabled-when::reputation-gathering).");
			} else
			if (me.local->firsttime + cfg.start_delay > TStime())
			{
				sendnotice(sptr, "Module DISABLED due to start-delay (set::connthrottle::disabled-when::start-delay), will be enabled in %ld second(s).",
					(me.local->firsttime + cfg.start_delay) - TStime());
			} else
			{
				sendnotice(sptr, "Module ENABLED");
			}
		}
	} else 
	if (!strcasecmp(parv[1], "OFF"))
	{
		if (ucounter.disabled == 1)
		{
			sendnotice(sptr, "Already OFF");
			return 0;
		}
		ucounter.disabled = 1;
		sendto_realops("[connthrottle] %s (%s@%s) DISABLED the connthrottle module.",
			sptr->name, sptr->user->username, sptr->user->realhost);
	} else
	if (!strcasecmp(parv[1], "ON"))
	{
		if (ucounter.disabled == 0)
		{
			sendnotice(sptr, "Already ON");
			return 0;
		}
		sendto_realops("[connthrottle] %s (%s@%s) ENABLED the connthrottle module.",
			sptr->name, sptr->user->username, sptr->user->realhost);
		ucounter.disabled = 0;
	} else
	if (!strcasecmp(parv[1], "RESET"))
	{
		memset(&ucounter, 0, sizeof(ucounter));
		sendto_realops("[connthrottle] %s (%s@%s) did a RESET on the stats/counters!!",
			sptr->name, sptr->user->username, sptr->user->realhost);
	} else
	{
		sendnotice(sptr, "Unknown option '%s'", parv[1]);
		ct_throttle_usage(sptr);
	}
	return 0;
}

void rehash_dump_settings(void)
{
	FILE *fd = fopen(rehash_dump_filename, "w");

	if (!fd)
	{
		config_status("WARNING: could not write to tmp/connthrottle.tmp (%s): "
		             "throttling counts and status will be RESET", strerror(errno));
		return;
	}
	fprintf(fd, "# THROTTLE DUMP v1 == DO NOT EDIT!\n");
	fprintf(fd, "TSME %ld\n", me.local->firsttime);
	fprintf(fd, "TSNOW %ld\n", TStime());
	fprintf(fd, "next_event %ld\n", ucounter.next_event);
	fprintf(fd, "local.count %d\n", ucounter.local.count);
	fprintf(fd, "local.t %ld\n", ucounter.local.t);
	fprintf(fd, "global.count %d\n", ucounter.global.count);
	fprintf(fd, "global.t %ld\n", ucounter.global.t);
	fprintf(fd, "rejected_clients %d\n", ucounter.rejected_clients);
	fprintf(fd, "allowed_score %d\n", ucounter.allowed_score);
	fprintf(fd, "allowed_sasl %d\n", ucounter.allowed_sasl);
	fprintf(fd, "allowed_other %d\n", ucounter.allowed_other);
	fprintf(fd, "disabled %d\n", (int)ucounter.disabled);
	fprintf(fd, "throttling_this_minute %d\n", ucounter.throttling_this_minute);
	fprintf(fd, "throttling_previous_minute %d\n", ucounter.throttling_previous_minute);
	fprintf(fd, "throttling_banner_displayed %d\n", ucounter.throttling_banner_displayed);
	if (fclose(fd))
	{
		/* fclose(/fprintf) error */
		config_status("WARNING: error while writing to tmp/connthrottle.tmp (%s): "
		              "throttling counts and status will be RESET", strerror(errno));
	}
}

/** Helper for rehash_read_settings() to parse connthrottle temp file */
int parse_connthrottle_file(char *str, char **name, char **value)
{
	static char buf[512];
	char *p;

	/* Initialize */
	*name = *value = NULL;
	strlcpy(buf, str, sizeof(buf));

	/* Strtoken */
	p = strchr(buf, ' ');
	if (!p)
		return 0;
	*p++ = '\0';

	/* Success */
	*name = buf;
	*value = p;
	return 1;
}

void rehash_read_settings(void)
{
	FILE *fd = fopen(rehash_dump_filename, "r");
	char buf[512], *name, *value;
	time_t ts;
	int num = 0;

	if (!fd)
		return;

	/* 1. Check header */
	if (!fgets(buf, sizeof(buf), fd) || strncmp(buf, "# THROTTLE DUMP v1 == DO NOT EDIT!", 34))
	{
		config_status("WARNING: tmp/connthrottle.tmp corrupt (I)");
		fclose(fd);
		return;
	}

	/* 2. Check if boottime matches exactly */
	if (!fgets(buf, sizeof(buf), fd) ||
	    !parse_connthrottle_file(buf, &name, &value) ||
	    strcmp(name, "TSME"))
	{
		config_status("WARNING: tmp/connthrottle.tmp corrupt (II)");
		fclose(fd);
		return;
	}	
	ts = atoi(buf+5);
	if (ts != me.local->firsttime) /* Not rehashing, possible restart or die */
	{
		fclose(fd);
#ifdef DEBUGMODE
		config_status("ts!=me.local->firsttime: ts=%ld, me.local->firsttime=%ld",
			ts, me.local->firsttime);
#endif
		unlink("tmp/connthrottle.tmp");
		return;
	}

	/* 3. Now parse the rest */
	while((fgets(buf, sizeof(buf), fd)))
	{
		if (!parse_connthrottle_file(buf, &name, &value))
		{
			config_warn("Corrupt connthrottle temp file. Settings may be lost.");
			continue;
		}

		if (!strcmp(name, "TSNOW"))
		{
			/* Ignored */
		} else
		if (!strcmp(name, "next_event"))
		{	ucounter.next_event = atol(value);
			num++;
		} else
		if (!strcmp(name, "local.count"))
		{	ucounter.local.count = atoi(value);
			num++;
		} else
		if (!strcmp(name, "local.t"))
		{
			ucounter.local.t = atol(value);
			num++;
		} else
		if (!strcmp(name, "global.count"))
		{
			ucounter.global.count = atoi(value);
			num++;
		} else
		if (!strcmp(name, "global.t"))
		{
			ucounter.global.t = atol(value);
			num++;
		} else
		if (!strcmp(name, "rejected_clients"))
		{
			ucounter.rejected_clients = atoi(value);
			num++;
		} else
		if (!strcmp(name, "allowed_score"))
		{
			ucounter.allowed_score = atoi(value);
			num++;
		} else
		if (!strcmp(name, "allowed_sasl"))
		{
			ucounter.allowed_sasl = atoi(value);
			num++;
		} else
		if (!strcmp(name, "allowed_other"))
		{
			ucounter.allowed_other = atoi(value);
			num++;
		} else
		if (!strcmp(name, "disabled"))
		{
			ucounter.disabled = (char)atoi(value);
			num++;
		} else
		if (!strcmp(name, "throttling_this_minute"))
		{
			ucounter.throttling_this_minute = atoi(value);
			num++;
		} else
		if (!strcmp(name, "throttling_previous_minute"))
		{
			ucounter.throttling_previous_minute = atoi(value);
			num++;
		} else
		if (!strcmp(name, "throttling_banner_displayed"))
		{
			ucounter.throttling_banner_displayed = atoi(value);
			num++;
		} else
		{
			config_warn("[BUG] Unknown variable in temporary connthrottle file: %s", name);
		}
	}
	fclose(fd);
	#define EXPECT_VAR_COUNT 13
	if (num != EXPECT_VAR_COUNT)
	{
		config_status("[connthrottle] WARNING: Only %d variables read but expected %d: "
		              "some information may have been lost during the rehash!",
		              num, EXPECT_VAR_COUNT);
	}
}
