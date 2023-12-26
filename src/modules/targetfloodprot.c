/*
 * IRC - Internet Relay Chat, src/modules/targetfloodprot.c
 * Target flood protection
 * (C)Copyright 2020 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */
   
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"targetfloodprot",
	"5.0",
	"Target flood protection (set::anti-flood::target-flood)",
	"UnrealIRCd Team",
	"unrealircd-6",
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
int targetfloodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
int targetfloodprot_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);

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
	safe_free(channelcfg);
	safe_free(privatecfg);
	return MOD_SUCCESS;
}

int targetfloodprot_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET_ANTI_FLOOD)
		return 0;

	/* We are only interrested in set::anti-flood::target-flood.. */
	if (!ce || !ce->name || strcmp(ce->name, "target-flood"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		CheckNull(cep);

		if (!strcmp(cep->name, "channel-privmsg") ||
		    !strcmp(cep->name, "channel-notice") ||
		    !strcmp(cep->name, "channel-tagmsg") ||
		    !strcmp(cep->name, "private-privmsg") ||
		    !strcmp(cep->name, "private-notice") ||
		    !strcmp(cep->name, "private-tagmsg"))
		{
			int cnt = 0, period = 0;

			if (!config_parse_flood(cep->value, &cnt, &period) ||
			    (cnt < 1) || (cnt > 10000) || (period < 1) || (period > 120))
			{
				config_error("%s:%i: set::anti-flood::target-flood::%s error. "
				             "Syntax is '<count>:<period>' (eg 5:60). "
				             "Count must be 1-10000 and period must be 1-120.",
				             cep->file->filename, cep->line_number,
				             cep->name);
				errors++;
			}
		} else
		{
			config_error("%s:%i: unknown directive set::anti-flood::target-flood:%s",
				cep->file->filename, cep->line_number, cep->name);
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
	if (!ce || !ce->name || strcmp(ce->name, "target-flood"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "channel-privmsg"))
			config_parse_flood(cep->value, &channelcfg->cnt[TFP_PRIVMSG], &channelcfg->t[TFP_PRIVMSG]);
		else if (!strcmp(cep->name, "channel-notice"))
			config_parse_flood(cep->value, &channelcfg->cnt[TFP_NOTICE], &channelcfg->t[TFP_NOTICE]);
		else if (!strcmp(cep->name, "channel-tagmsg"))
			config_parse_flood(cep->value, &channelcfg->cnt[TFP_TAGMSG], &channelcfg->t[TFP_TAGMSG]);
		else if (!strcmp(cep->name, "private-privmsg"))
			config_parse_flood(cep->value, &privatecfg->cnt[TFP_PRIVMSG], &privatecfg->t[TFP_PRIVMSG]);
		else if (!strcmp(cep->name, "private-notice"))
			config_parse_flood(cep->value, &privatecfg->cnt[TFP_NOTICE], &privatecfg->t[TFP_NOTICE]);
		else if (!strcmp(cep->name, "private-tagmsg"))
			config_parse_flood(cep->value, &privatecfg->cnt[TFP_TAGMSG], &privatecfg->t[TFP_TAGMSG]);
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
	unreal_log(ULOG_ERROR, "flood", "BUG_SENDTYPETOWHAT_UNKNOWN_VALUE", NULL,
	           "[BUG] sendtypetowhat() called for unknown sendtype $send_type",
	           log_data_integer("send_type", sendtype));
	abort();
#endif
	return 0; /* otherwise, default to privmsg i guess */
}

int targetfloodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	TargetFlood *flood;
	static char errbuf[256];
	int what;

	/* This is redundant, right? */
	if (!MyUser(client))
		return HOOK_CONTINUE;

	/* U-Lines, servers and IRCOps override */
	if (IsULine(client) || !IsUser(client) || (IsOper(client) && ValidatePermissionsForPath("immune:target-flood",client,NULL,channel,NULL)))
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
		unreal_log(ULOG_INFO, "flood", "FLOOD_BLOCKED", client,
			   "Flood blocked ($flood_type) from $client.details [$client.ip] to $channel",
			   log_data_string("flood_type", "target-flood-channel"),
			   log_data_channel("channel", channel));
		snprintf(errbuf, sizeof(errbuf), "Channel is being flooded. Message not delivered.");
		*errmsg = errbuf;
		return HOOK_DENY;
	}

	flood->cnt[what]++;
	return HOOK_CONTINUE;
}

int targetfloodprot_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	TargetFlood *flood;
	static char errbuf[256];
	int what;

	/* Check if it is our TARGET ('target'), so yeah
	 * be aware that 'client' may be remote client in all the code that follows!
	 */
	if (!MyUser(target))
		return HOOK_CONTINUE;

	/* U-Lines, servers and IRCOps override */
	if (IsULine(client) || !IsUser(client) || (IsOper(client) && ValidatePermissionsForPath("immune:target-flood",client,target,NULL,NULL)))
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
		unreal_log(ULOG_INFO, "flood", "FLOOD_BLOCKED", client,
			   "Flood blocked ($flood_type) from $client.details [$client.ip] to $target",
			   log_data_string("flood_type", "target-flood-user"),
			   log_data_client("target", target));
		snprintf(errbuf, sizeof(errbuf), "User is being flooded. Message not delivered.");
		*errmsg = errbuf;
		return HOOK_DENY;
	}

	flood->cnt[what]++;
	return HOOK_CONTINUE;
}
