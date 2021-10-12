/* Target flood protection
 * (C)Copyright 2020 Bram Matthys and the UnrealIRCd team
 * License: GPLv2
 */
   
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"targetfloodprot",
	"5.0",
	"Target flood protection (set::anti-flood::target-flood)",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

#define TFP_PRIVMSG	0
#define TFP_NOTICE	1
#define TFP_TAGMSG	2
#define TFP_MAX		3

typedef struct TargetFlood TargetFlood;
struct TargetFlood {
	unsigned short cnt[TFP_MAX];
	time_t t[TFP_MAX];
};

typedef struct TargetFloodConfig TargetFloodConfig;
struct TargetFloodConfig {
	int cnt[TFP_MAX];
	int t[TFP_MAX];
};

/* Forward declarations */
int targetfloodprot_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int targetfloodprot_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
void targetfloodprot_mdata_free(ModData *m);
int targetfloodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, char **msg, char **errmsg, SendType sendtype);
int targetfloodprot_can_send_to_user(Client *client, Client *target, char **text, char **errmsg, SendType sendtype);

/* Global variables */
ModDataInfo *targetfloodprot_client_md = NULL;
ModDataInfo *targetfloodprot_channel_md = NULL;
TargetFloodConfig *channelcfg = NULL;
TargetFloodConfig *privatecfg = NULL;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, targetfloodprot_config_test);
	return MOD_SUCCESS;
}

/** Allocate config and set default configuration */
void targetfloodprot_defaults(void)
{
	channelcfg = safe_alloc(sizeof(TargetFloodConfig));
	privatecfg = safe_alloc(sizeof(TargetFloodConfig));

	/* set::anti-flood::target-flood::channel-privmsg */
	channelcfg->cnt[TFP_PRIVMSG] = 45;
	channelcfg->t[TFP_PRIVMSG] = 5;
	/* set::anti-flood::target-flood::channel-notice */
	channelcfg->cnt[TFP_NOTICE] = 15;
	channelcfg->t[TFP_NOTICE] = 5;
	/* set::anti-flood::target-flood::channel-tagmsg */
	channelcfg->cnt[TFP_TAGMSG] = 15;
	channelcfg->t[TFP_TAGMSG] = 5;

	/* set::anti-flood::target-flood::private-privmsg */
	privatecfg->cnt[TFP_PRIVMSG] = 30;
	privatecfg->t[TFP_PRIVMSG] = 5;
	/* set::anti-flood::target-flood::private-notice */
	privatecfg->cnt[TFP_NOTICE] = 10;
	privatecfg->t[TFP_NOTICE] = 5;
	/* set::anti-flood::target-flood::private-tagmsg */
	privatecfg->cnt[TFP_TAGMSG] = 10;
	privatecfg->t[TFP_TAGMSG] = 5;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, targetfloodprot_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, targetfloodprot_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, targetfloodprot_can_send_to_user);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "targetfloodprot";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = targetfloodprot_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	targetfloodprot_client_md = ModDataAdd(modinfo->handle, mreq);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "targetfloodprot";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = targetfloodprot_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_CHANNEL;
	targetfloodprot_channel_md = ModDataAdd(modinfo->handle, mreq);

	targetfloodprot_defaults();

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

#ifndef CheckNull
 #define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#endif

int targetfloodprot_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET_ANTI_FLOOD)
		return 0;

	/* We are only interrested in set::anti-flood::target-flood.. */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "target-flood"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		CheckNull(cep);

		if (!strcmp(cep->ce_varname, "channel-privmsg") ||
		    !strcmp(cep->ce_varname, "channel-notice") ||
		    !strcmp(cep->ce_varname, "channel-tagmsg") ||
		    !strcmp(cep->ce_varname, "private-privmsg") ||
		    !strcmp(cep->ce_varname, "private-notice") ||
		    !strcmp(cep->ce_varname, "private-tagmsg"))
		{
			int cnt = 0, period = 0;

			if (!config_parse_flood(cep->ce_vardata, &cnt, &period) ||
			    (cnt < 1) || (cnt > 10000) || (period < 1) || (period > 120))
			{
				config_error("%s:%i: set::anti-flood::target-flood::%s error. "
				             "Syntax is '<count>:<period>' (eg 5:60). "
				             "Count must be 1-10000 and period must be 1-120.",
				             cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				             cep->ce_varname);
				errors++;
			}
		} else
		{
			config_error("%s:%i: unknown directive set::anti-flood::target-flood:%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int targetfloodprot_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET_ANTI_FLOOD)
		return 0;

	/* We are only interrested in set::anti-flood::target-flood.. */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "target-flood"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel-privmsg"))
			config_parse_flood(cep->ce_vardata, &channelcfg->cnt[TFP_PRIVMSG], &channelcfg->t[TFP_PRIVMSG]);
		else if (!strcmp(cep->ce_varname, "channel-notice"))
			config_parse_flood(cep->ce_vardata, &channelcfg->cnt[TFP_NOTICE], &channelcfg->t[TFP_NOTICE]);
		else if (!strcmp(cep->ce_varname, "channel-tagmsg"))
			config_parse_flood(cep->ce_vardata, &channelcfg->cnt[TFP_TAGMSG], &channelcfg->t[TFP_TAGMSG]);
		else if (!strcmp(cep->ce_varname, "private-privmsg"))
			config_parse_flood(cep->ce_vardata, &privatecfg->cnt[TFP_PRIVMSG], &privatecfg->t[TFP_PRIVMSG]);
		else if (!strcmp(cep->ce_varname, "private-notice"))
			config_parse_flood(cep->ce_vardata, &privatecfg->cnt[TFP_NOTICE], &privatecfg->t[TFP_NOTICE]);
		else if (!strcmp(cep->ce_varname, "private-tagmsg"))
			config_parse_flood(cep->ce_vardata, &privatecfg->cnt[TFP_TAGMSG], &privatecfg->t[TFP_TAGMSG]);
	}

	return 1;
}

/** UnrealIRCd internals: free object. */
void targetfloodprot_mdata_free(ModData *m)
{
	/* we don't have any members to free, so this is easy */
	safe_free(m->ptr);
}

int sendtypetowhat(SendType sendtype)
{
	if (sendtype == SEND_TYPE_PRIVMSG)
		return 0;
	if (sendtype == SEND_TYPE_NOTICE)
		return 1;
	if (sendtype == SEND_TYPE_TAGMSG)
		return 2;
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "sendtypetowhat() for unknown value %d", (int)sendtype);
	abort();
#endif
	return 0; /* otherwise, default to privmsg i guess */
}

int targetfloodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, char **msg, char **errmsg, SendType sendtype)
{
	TargetFlood *flood;
	static char errbuf[256];
	int what;

	/* This is redundant, right? */
	if (!MyUser(client))
		return HOOK_CONTINUE;

	/* IRCOps and U-Lines override */
	if (IsULine(client) || (IsOper(client) && ValidatePermissionsForPath("immune:target-flood",client,NULL,channel,NULL)))
		return HOOK_CONTINUE;

	what = sendtypetowhat(sendtype);

	if (moddata_channel(channel, targetfloodprot_channel_md).ptr == NULL)
	{
		/* Alloc a new entry if it doesn't exist yet */
		moddata_channel(channel, targetfloodprot_channel_md).ptr = safe_alloc(sizeof(TargetFlood));
	}

	flood = (TargetFlood *)moddata_channel(channel, targetfloodprot_channel_md).ptr;

	if ((TStime() - flood->t[what]) >= channelcfg->t[what])
	{
		/* Reset due to moving into a new time slot */
		flood->t[what] = TStime();
		flood->cnt[what] = 1;
		return HOOK_CONTINUE; /* forget about it.. */
	}

	if (flood->cnt[what] >= channelcfg->cnt[what])
	{
		/* Flood detected */
		flood_limit_exceeded_log(client, "target-flood-channel");
		snprintf(errbuf, sizeof(errbuf), "Channel is being flooded. Message not delivered.");
		*errmsg = errbuf;
		return HOOK_DENY;
	}

	flood->cnt[what]++;
	return HOOK_CONTINUE;
}

int targetfloodprot_can_send_to_user(Client *client, Client *target, char **text, char **errmsg, SendType sendtype)
{
	TargetFlood *flood;
	static char errbuf[256];
	int what;

	/* Check if it is our TARGET ('target'), so yeah
	 * be aware that 'client' may be remote client in all the code that follows!
	 */
	if (!MyUser(target))
		return HOOK_CONTINUE;

	/* IRCOps and U-Lines override */
	if (IsULine(client) || (IsOper(client) && ValidatePermissionsForPath("immune:target-flood",client,target,NULL,NULL)))
		return HOOK_CONTINUE;

	what = sendtypetowhat(sendtype);

	if (moddata_local_client(target, targetfloodprot_client_md).ptr == NULL)
	{
		/* Alloc a new entry if it doesn't exist yet */
		moddata_local_client(target, targetfloodprot_client_md).ptr = safe_alloc(sizeof(TargetFlood));
	}

	flood = (TargetFlood *)moddata_local_client(target, targetfloodprot_client_md).ptr;

	if ((TStime() - flood->t[what]) >= privatecfg->t[what])
	{
		/* Reset due to moving into a new time slot */
		flood->t[what] = TStime();
		flood->cnt[what] = 1;
		return HOOK_CONTINUE; /* forget about it.. */
	}

	if (flood->cnt[what] >= privatecfg->cnt[what])
	{
		/* Flood detected */
		flood_limit_exceeded_log(client, "target-flood-user");
		snprintf(errbuf, sizeof(errbuf), "User is being flooded. Message not delivered.");
		*errmsg = errbuf;
		return HOOK_DENY;
	}

	flood->cnt[what]++;
	return HOOK_CONTINUE;
}
