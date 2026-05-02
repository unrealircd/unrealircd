#include "unrealircd.h"

/* this should go into include/numeric.h (was there at one point of time) */

#define RPL_WHOISKEYVALUE    760
#define RPL_KEYVALUE         761
#define RPL_KEYNOTSET    766
#define RPL_METADATASUBOK    770
#define RPL_METADATAUNSUBOK  771
#define RPL_METADATASUBS     772
#define RPL_METADATASYNCLATER 774

#define STR_RPL_WHOISKEYVALUE		/* 760 */	"%s %s %s :%s"
#define STR_RPL_KEYVALUE			/* 761 */	"%s %s %s :%s"
#define STR_RPL_KEYNOTSET			/* 766 */	"%s %s :key not set"
#define STR_RPL_METADATASUBOK		/* 770 */	":%s"
#define STR_RPL_METADATAUNSUBOK		/* 771 */	":%s"
#define STR_RPL_METADATASUBS		/* 772 */	":%s"
#define STR_RPL_METADATASYNCLATER	/* 774 */	"%s %s"

/* sendnumeric() which allows message tags (BATCH in our case) */

#define sendnumeric_mtags(to, mtags, numeric, ...) sendnumericfmt_tags(to, mtags, numeric, STR_ ## numeric, ##__VA_ARGS__)

void vsendto_one(Client *to, MessageTag *mtags, const char *pattern, va_list vl); /* no prototype from send.c */

void sendnumericfmt_tags (Client *to, MessageTag *mtags, int numeric, FORMAT_STRING(const char *pattern), ...) {
	va_list vl;
	char realpattern[512];

	snprintf(realpattern, sizeof(realpattern), ":%s %.3d %s %s", me.name, numeric, to->name[0] ? to->name : "*", pattern);

	va_start(vl, pattern);
	vsendto_one(to, mtags, realpattern, vl);
	va_end(vl);
}

/* actual METADATA code */

/* get or set for perms */
#define MODE_SET 0
#define MODE_GET 1

#define WATCH_EVENT_METADATA	3000 /* core uses 0..8, we hope no other module will try 3000 */

#define MYCONF "metadata"

#define CHECKPARAMSCNT_OR_DIE(count, return) \
{ \
	if (parc < count+1 || BadPtr(parv[count])) \
	{ \
		sendnumeric(client, ERR_NEEDMOREPARAMS, "METADATA"); \
		return; \
	} \
}

/* target "*" is always the user issuing the command */

#define PROCESS_TARGET_OR_DIE(target, user, channel, return) \
{ \
	char *channame; \
	channame = strchr(target, '#'); \
	if (channame) \
	{ \
		channel = find_channel(channame); \
		if (!channel) \
		{ \
			sendto_one(client, NULL, ":%s FAIL METADATA INVALID_TARGET %s :invalid metadata target", me.name, channame); \
			return; \
		} \
	} else \
	{ \
		if (strcmp(target, "*")) \
		{ \
			user = hash_find_nickatserver(target, NULL); \
			if (!user) \
			{ \
				sendto_one(client, NULL, ":%s FAIL METADATA INVALID_TARGET %s :invalid metadata target", me.name, target); \
				return; \
			} \
		} else \
		{ \
			user = client; \
		} \
	} \
}

#define FOR_EACH_KEY(keyindex, parc, parv) while(keyindex++, keyindex < parc && !BadPtr(key = parv[keyindex]))
#define IsSendable(x)		(DBufLength(&x->local->sendQ) < 2048)
#define CHECKREGISTERED_OR_DIE(client, return) \
{ \
	if (!IsUser(client)) \
	{ \
		sendnumeric(client, ERR_NOTREGISTERED); \
		return; \
	} \
}
#define USER_METADATA(client) moddata_client(client, metadataUser).ptr
#define CHANNEL_METADATA(channel) moddata_channel(channel, metadataChannel).ptr

#define MAKE_BATCH(client, batch, mtags, target) do { \
	if (HasCapability(client, "batch")) { \
		generate_batch_id(batch); \
		sendto_one(client, NULL, ":%s BATCH +%s metadata %s", me.name, batch, target); \
		mtags = safe_alloc(sizeof(MessageTag)); \
		mtags->name = strdup("batch"); \
		mtags->value = strdup(batch); \
	} \
} while(0)

#define FINISH_BATCH(client, batch, mtags) do { \
	if (*batch) \
		sendto_one(client, NULL, ":%s BATCH -%s", me.name, batch); \
	if (mtags) \
		free_message_tags(mtags); \
} while(0)

struct metadata {
	char *name;
	char *value;
	struct metadata *next;
};

struct metadata_subscriptions {
	char *name;
	struct metadata_subscriptions *next;
};

struct metadata_moddata_user {
	struct metadata *metadata;
	struct metadata_subscriptions *subs;
	struct metadata_unsynced *us;
};

struct metadata_unsynced { /* we're listing users (UIDs) or channels that should be synced but were not */
	char *id;
	char *key;
	struct metadata_unsynced *next;
};

struct metadata_monitor_s {
	Client *changer;
	const char *key; /* set to NULL to send all metadata */
	const char *value;
};

CMD_FUNC(cmd_metadata);
CMD_FUNC(cmd_metadata_remote);
CMD_FUNC(cmd_metadata_local);
EVENT(metadata_queue_evt);
const char *metadata_cap_param(Client *client);
int metadata_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int metadata_configposttest(int *errs);
int metadata_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
int metadata_server_sync(Client *client);
int metadata_join(Client *client, Channel *channel, MessageTag *mtags);
int metadata_user_registered(Client *client);
void metadata_user_free(ModData *md);
void metadata_channel_free(ModData *md);
void metadata_free(struct metadata *metadata);
void metadata_free_subs(struct metadata_subscriptions *subs);
int metadata_is_subscribed(Client *user, const char *key);
const char *metadata_get_user_key_value(Client *user, const char *key);
const char *metadata_get_channel_key_value(Channel *channel, const char *key);
void user_metadata_changed(Client *user, const char *key, const char *value, Client *changer);
void channel_metadata_changed(Channel *channel, const char *key, const char *value, Client *changer);
void metadata_free_list(struct metadata *metadata, const char *whose, Client *client, MessageTag *mtags);
struct metadata_moddata_user *metadata_prepare_user_moddata(Client *user);
void metadata_set_channel(Channel *channel, const char *key, const char *value, Client *client);
void metadata_set_user(Client *user, const char *key, const char *value, Client *client);
void metadata_send_channel(Channel *channel, const char *key, Client *client, MessageTag *mtags);
void metadata_send_user(Client *user, const char *key, Client *client, MessageTag *mtags);
int metadata_subscribe(const char *key, Client *client, int remove, MessageTag *mtags);
void metadata_clear_channel(Channel *channel, Client *client, MessageTag *mtags);
void metadata_clear_user(Client *user, Client *client, MessageTag *mtags);
void metadata_send_subscribtions(Client *client);
void metadata_send_all_for_channel(Channel *channel, Client *client);
void metadata_send_all_for_user(Client *user, Client *client);
void metadata_send_pending(Client *client);
void metadata_sync_user(Client *client, Client *target, MessageTag *mtags, int create_batch);
void metadata_sync_channel(Client *client, Channel *channel);
int metadata_key_valid(const char *key);
int metadata_check_perms(Client *user, Channel *channel, Client *client, const char *key, int mode);
void metadata_send_change(Client *client, MessageTag *mtags, const char *who, const char *key, const char *value, Client *changer);
int metadata_notify_or_queue(Client *client, MessageTag *mtags, Client *who, Channel *chan, const char *key, const char *value, Client *changer);
void metadata_notify_monitored(Client *client, Client *monitored, Client *changer, const char *key, const char *value);
int metadata_is_monitoring(Client *watcher, Client *watched);

#if UNREAL_VERSION_TIME < 202346
int metadata_monitor_connect(Client *client);
int metadata_monitor_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int metadata_monitor_notification(Client *client, Watch *watch, Link *lp, int event);
CMD_OVERRIDE_FUNC(metadata_overridemonitor);

struct metadata_monitor_s metadata_monitor_data;

#else
int metadata_monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data);
int metadata_monitor_online(Client *watcher, Client *client, int online);
int metadata_watch_add(char *nick, Client *client, int flags);
#endif

ModDataInfo *metadataUser;
ModDataInfo *metadataChannel;
long CAP_METADATA = 0L;
long CAP_METADATA_NOTIFY = 0L;

struct metadata_settings_s {
	int max_user_metadata;
	int max_channel_metadata;
	int max_subscriptions;
	int max_value_bytes;
} metadata_settings;

ModuleHeader MOD_HEADER = {
	"third/metadata-2",
	"6.0",
	"draft/metadata-2 and draft/metadata-notify-2 cap",
	"k4be",
	"unrealircd-6"
};

/*
metadata {
	max-user-metadata 10;
	max-channel-metadata 10;
	max-subscriptions 10;
	max-value-bytes 300;
};
*/

int metadata_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
	ConfigEntry *cep;
	int errors = 0;
	int i;
	
	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, MYCONF))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
		{
			config_error("%s:%i: blank %s item", cep->file->filename, cep->line_number, MYCONF);
			errors++;
			continue;
		}

		if (!cep->value || !strlen(cep->value))
		{
			config_error("%s:%i: %s::%s must be non-empty", cep->file->filename, cep->line_number, MYCONF, cep->name);
			errors++;
			continue;
		}
	
		if (!strcmp(cep->name, "max-user-metadata"))
		{
			for (i = 0; cep->value[i]; i++)
			{
				if (!isdigit(cep->value[i]))
				{
					config_error("%s:%i: %s::%s must be an integer between 1 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
					errors++;
					break;
				}
			}
			if (!errors && (atoi(cep->value) < 1 || atoi(cep->value) > 100))
			{
				config_error("%s:%i: %s::%s must be an integer between 1 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}

		if (!strcmp(cep->name, "max-channel-metadata"))
		{
			for (i = 0; cep->value[i]; i++)
			{
				if (!isdigit(cep->value[i]))
				{
					config_error("%s:%i: %s::%s must be an integer between 0 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
					errors++;
					break;
				}
			}
			if (!errors && (atoi(cep->value) < 0 || atoi(cep->value) > 100))
			{
				config_error("%s:%i: %s::%s must be an integer between 0 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}

		if (!strcmp(cep->name, "max-subscriptions"))
		{
			for (i = 0; cep->value[i]; i++)
			{
				if (!isdigit(cep->value[i]))
				{
					config_error("%s:%i: %s::%s must be an integer between 1 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
					errors++;
					break;
				}
			}
			if (!errors && (atoi(cep->value) < 1 || atoi(cep->value) > 100))
			{
				config_error("%s:%i: %s::%s must be an integer between 1 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}


		if (!strcmp(cep->name, "max-value-bytes"))
		{
			for (i = 0; cep->value[i]; i++)
			{
				if (!isdigit(cep->value[i]))
				{
					config_error("%s:%i: %s::%s must be an integer between 1 and 400", cep->file->filename, cep->line_number, MYCONF, cep->name);
					errors++;
					break;
				}
			}
			if (!errors && (atoi(cep->value) < 1 || atoi(cep->value) > 400))
			{
				config_error("%s:%i: %s::%s must be an integer between 1 and 400", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}

		config_warn("%s:%i: unknown item %s::%s", cep->file->filename, cep->line_number, MYCONF, cep->name);
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int metadata_configposttest(int *errs) {
	/* null the settings to avoid keeping old value if none is set in config */
	metadata_settings.max_user_metadata = 0;
	metadata_settings.max_channel_metadata = 0;
	metadata_settings.max_subscriptions = 0;
	metadata_settings.max_value_bytes = 0;
	return 1;
}

int metadata_configrun(ConfigFile *cf, ConfigEntry *ce, int type) {
	ConfigEntry *cep;

	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, MYCONF))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;

		if (!strcmp(cep->name, "max-user-metadata"))
		{
			metadata_settings.max_user_metadata = atoi(cep->value);
			continue;
		}

		if (!strcmp(cep->name, "max-channel-metadata"))
		{
			metadata_settings.max_channel_metadata = atoi(cep->value);
			continue;
		}

		if (!strcmp(cep->name, "max-subscriptions"))
		{
			metadata_settings.max_subscriptions = atoi(cep->value);
			continue;
		}

		if (!strcmp(cep->name, "max-value-bytes"))
		{
			metadata_settings.max_value_bytes = atoi(cep->value);
			continue;
		}
	}
	return 1;
}

MOD_TEST(){
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, metadata_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, metadata_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT() {
	ClientCapabilityInfo cap;
	ClientCapability *c;
	ModDataInfo mreq;
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	MARK_AS_GLOBAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/metadata-2";
	cap.parameter = metadata_cap_param;
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_METADATA);
	
	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/metadata-notify-2"; /* for old client compatibility */
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_METADATA_NOTIFY);
	
	CommandAdd(modinfo->handle, "METADATA", cmd_metadata, MAXPARA, CMD_USER|CMD_SERVER|CMD_UNREGISTERED);
	
	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_CLIENT;
	mreq.name = "metadata_user",
	mreq.free = metadata_user_free;
	metadataUser = ModDataAdd(modinfo->handle, mreq);
	if (!metadataUser)
	{
		config_error("[%s] Failed to request metadata_user moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	
	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_CHANNEL;
	mreq.name = "metadata_channel",
	mreq.free = metadata_channel_free;
	metadataChannel = ModDataAdd(modinfo->handle, mreq);
	if (!metadataChannel)
	{
		config_error("[%s] Failed to request metadata_channel moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_SYNC, 0, metadata_server_sync);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, -2, metadata_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, -2, metadata_join);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, metadata_user_registered);
#if UNREAL_VERSION_TIME < 202346
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, metadata_monitor_connect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, metadata_monitor_connect);
	HookAdd(modinfo->handle, HOOKTYPE_POST_LOCAL_NICKCHANGE, 0, metadata_monitor_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_REMOTE_NICKCHANGE, 0, metadata_monitor_post_nickchange);
	CommandOverrideAdd(modinfo->handle, "MONITOR", 0, metadata_overridemonitor);
#else
	HookAdd(modinfo->handle, HOOKTYPE_MONITOR_NOTIFICATION, 0, metadata_monitor_online);
	HookAdd(modinfo->handle, HOOKTYPE_WATCH_ADD, 0, metadata_watch_add);
#endif
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, metadata_configrun);
	
	return MOD_SUCCESS;
}

MOD_LOAD() {
	/* setting default values if not configured */
	if (metadata_settings.max_user_metadata == 0)
		metadata_settings.max_user_metadata = 10;
	if (metadata_settings.max_channel_metadata == 0)
		metadata_settings.max_channel_metadata = 10;
	if (metadata_settings.max_subscriptions == 0)
		metadata_settings.max_subscriptions = 10;
	if (metadata_settings.max_value_bytes == 0)
		metadata_settings.max_value_bytes = 300;

	EventAdd(modinfo->handle, "metadata_queue", metadata_queue_evt, NULL, 1500, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD() {
	return MOD_SUCCESS;
}

const char *metadata_cap_param(Client *client)
{
	static char buf[80];
	ircsnprintf(buf, sizeof(buf), "before-connect,max-subs=%d,max-keys=%d,max-value-bytes=%d",
		metadata_settings.max_subscriptions, metadata_settings.max_user_metadata, metadata_settings.max_value_bytes);
	return buf;
}

void metadata_free(struct metadata *metadata)
{
	safe_free(metadata->name);
	safe_free(metadata->value);
	safe_free(metadata);
}

void metadata_free_subs(struct metadata_subscriptions *subs)
{
	safe_free(subs->name);
	safe_free(subs);
}

int metadata_is_subscribed(Client *user, const char *key)
{
	struct metadata_moddata_user *moddata = USER_METADATA(user);
	if (!moddata)
		return 0;
	struct metadata_subscriptions *subs;
	for (subs = moddata->subs; subs; subs = subs->next)
	{
		if (!strcasecmp(subs->name, key))
			return 1;
	}
	return 0;
}

const char *metadata_get_user_key_value(Client *user, const char *key)
{
	struct metadata_moddata_user *moddata = USER_METADATA(user);
	struct metadata *metadata = NULL;
	if (!moddata)
		return NULL;
	for (metadata = moddata->metadata; metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
			return metadata->value;
	}
	return NULL;
}

const char *metadata_get_channel_key_value(Channel *channel, const char *key)
{
	struct metadata *metadata;
	for (metadata = CHANNEL_METADATA(channel); metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
			return metadata->value;
	}
	return NULL;
}

/* returns 1 if something remains to sync */
int metadata_notify_or_queue(Client *client, MessageTag *mtags, Client *who, Channel *chan, const char *key, const char *value, Client *changer)
{
	int trylater = 0;
	if (!who && !chan)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_notify_or_queue called with null who and channel!");
		return 0;
	}
	if (!key)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_notify_or_queue called with null key!");
		return 0;
	}
	if (!client)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_notify_or_queue called with null client!");
		return 0;
	}

	struct metadata_moddata_user *moddata = USER_METADATA(client);
	if (!moddata)
		moddata = metadata_prepare_user_moddata(client);
	struct metadata_unsynced **us = &moddata->us;

	if (IsSendable(client))
	{
		const char *nick_or_channel;
		if (chan)
			nick_or_channel = chan->name;
		else
			nick_or_channel = who->name;
		metadata_send_change(client, mtags, nick_or_channel, key, value, changer);
	} else
	{ /* store for the SYNC */
		const char *uid_or_channel;
		if (chan)
			uid_or_channel = chan->name;
		else
			uid_or_channel = who->id;
			
		trylater = 1;
		while (*us)
			us = &(*us)->next; /* find last list element */
		*us = safe_alloc(sizeof(struct metadata_unsynced));
		(*us)->id = strdup(uid_or_channel);
		(*us)->key = strdup(key);
		(*us)->next = NULL;
	}
	return trylater;
}

void metadata_send_change(Client *client, MessageTag *mtags, const char *who, const char *key, const char *value, Client *changer)
{
	char *sender = NULL;
	if (!key)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_send_change called with null key!");
		return;
	}
	if (!who)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_send_change called with null who!");
		return;
	}
	if (!client)
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", changer, "metadata_send_change called with null client!");
		return;
	}
	if (changer)
	{
		if (IsServer(client))
			sender = changer->id;
		else
			sender = changer->name;
	}
	if (!sender)
		sender = me.name;
	if (changer && IsUser(changer) && MyUser(client))
	{
		/* Always send value parameter (empty if NULL) */
		sendto_one(client, mtags, ":%s!%s@%s METADATA %s %s %s :%s", sender, changer->user->username, GetHost(changer), who, key, "*", value ? value : "");
	} else
	{ /* sending S2S (sender is id) or receiving S2S (sender is servername) */
		/* Always send value parameter (empty if NULL) */
		sendto_one(client, mtags, ":%s METADATA %s %s %s :%s", sender, who, key, "*", value ? value : "");
	}
}

/* used for broadcasting changes to subscribed users and linked servers */
void user_metadata_changed(Client *user, const char *key, const char *value, Client *changer){
	Client *acptr;
	if (!user || !key)
		return; /* sanity check */
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if (!IsUser(acptr) || !IsUser(user))
			continue;
		if (!metadata_is_subscribed(acptr, key))
			continue;
		if (acptr == user && changer == user)
			continue; /* clients don't receive notifications for their own changes (spec: "excluding changes they make themselves") */
		if (acptr == user || has_common_channels(user, acptr))
			metadata_notify_or_queue(acptr, NULL, user, NULL, key, value, changer);
	}

	{ /* propagate to linked servers, excluding the source */
		const char *sender_id = (changer && changer != &me) ? changer->id : me.id;
		sendto_server(changer, 0, 0, NULL, ":%s METADATA %s %s * :%s",
			sender_id, user->id, key, value ? value : "");
	}
	/* notifications for MONITOR */
#if UNREAL_VERSION_TIME < 202346
	metadata_monitor_data.changer = changer;
	metadata_monitor_data.key = key;
	metadata_monitor_data.value = value;
	watch_check(user, WATCH_EVENT_METADATA, metadata_monitor_notification);
	memset(&metadata_monitor_data, 0, sizeof(metadata_monitor_data));
#else
	struct metadata_monitor_s metadata_mond = { .changer = changer, .key = key, .value = value };
	watch_check(user, WATCH_EVENT_METADATA, &metadata_mond, metadata_monitor_notification);
#endif
}

void channel_metadata_changed(Channel *channel, const char *key, const char *value, Client *changer)
{
	Client *acptr;
	if (!channel || !key)
		return; /* sanity check */
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if (metadata_is_subscribed(acptr, key) && IsMember(acptr, channel))
			metadata_send_change(acptr, NULL, channel->name, key, value, changer);
	}
	
	{ /* propagate to linked servers, excluding the source */
		const char *sender_id = (changer && changer != &me) ? changer->id : me.id;
		sendto_server(changer, 0, 0, NULL, ":%s METADATA %s %s * :%s",
			sender_id, channel->name, key, value ? value : "");
	}
}

void metadata_free_list(struct metadata *metadata, const char *whose, Client *client, MessageTag *mtags)
{
	struct metadata *prev_metadata = metadata;
	char *name;
	while(metadata)
	{
		name = metadata->name;
		safe_free(metadata->value);
		metadata = metadata->next;
		safe_free(prev_metadata);
		prev_metadata = metadata;
		if(client && whose && *whose)
		{ /* send out the data being removed, unless we're unloading the module */
			sendnumeric_mtags(client, mtags, RPL_KEYVALUE, whose, name, "*", "");
			if(*whose == '#')
				channel_metadata_changed(find_channel(whose), name, NULL, client);
			else
				user_metadata_changed(hash_find_nickatserver(whose, NULL), name, NULL, client);
		}
		safe_free(name);
	}
}

void metadata_channel_free(ModData *md)
{
	if (!md->ptr)
		return; /* was not set */
	struct metadata *metadata = md->ptr;
	metadata_free_list(metadata, NULL, NULL, NULL);
}

void metadata_user_free(ModData *md)
{
	struct metadata_moddata_user *moddata = md->ptr;
	if (!moddata)
		return; /* was not set */
	struct metadata_subscriptions *sub = moddata->subs;
	struct metadata_subscriptions *prev_sub = sub;
	struct metadata_unsynced *us = moddata->us;
	struct metadata_unsynced *prev_us;
	while (sub)
	{
		safe_free(sub->name);
		sub = sub->next;
		safe_free(prev_sub);
		prev_sub = sub;
	}
	struct metadata *metadata = moddata->metadata;
	metadata_free_list(metadata, NULL, NULL, NULL);
	while (us)
	{
		safe_free(us->id);
		safe_free(us->key);
		prev_us = us;
		us = us->next;
		safe_free(prev_us);
	}
	safe_free(moddata);
}

struct metadata_moddata_user *metadata_prepare_user_moddata(Client *user)
{
	USER_METADATA(user) = safe_alloc(sizeof(struct metadata_moddata_user));
	struct metadata_moddata_user *ptr = USER_METADATA(user);
	ptr->metadata = NULL;
	ptr->subs = NULL;
	ptr->us = NULL;
	return ptr;
}

void metadata_set_user(Client *user, const char *key, const char *value, Client *client)
{
	int changed = 0;
	Client *target;
	char *target_name;
	int removed = 0;
	int set = 0;
	int count = 0;

	if (user)
	{
		target = user;
		target_name = user->name;
	} else
	{
		target = client;
		target_name = "*";
	}
		
	struct metadata_moddata_user *moddata = USER_METADATA(target);
	if (!moddata) /* first call for this user */
		moddata = metadata_prepare_user_moddata(target);
	struct metadata **metadata = &moddata->metadata;
	struct metadata *prev;

	if (BadPtr(value) || strlen(value) == 0)
	{ /* unset */
		value = NULL; /* just to make sure */
		removed = 0;
		while (*metadata)
		{
			if (!strcasecmp(key, (*metadata)->name))
				break;
			metadata = &(*metadata)->next;
		}
		if (*metadata)
		{
			prev = *metadata;
			*metadata = prev->next;
			metadata_free(prev);
			removed = 1;
			changed = 1;
		}
		if (!removed)
		{
			if (client)
				sendto_one(client, NULL, ":%s FAIL METADATA KEY_NOT_SET %s %s :key not set", me.name, target_name, key); /* not set so can't remove */
			return;
		}
	} else
	{ /* set */
		while (*metadata)
		{
			if (!strcasecmp(key, (*metadata)->name))
			{
				set = 1;
				if (strcmp(value, (*metadata)->value))
				{
					safe_free((*metadata)->value);
					(*metadata)->value = strdup(value);
					changed = 1;
				}
			}
			metadata = &(*metadata)->next;
			count++;
		}
		if (!set)
		{
			if (!client || count < metadata_settings.max_user_metadata)
			{ /* add new entry for user */
				*metadata = safe_alloc(sizeof(struct metadata));
				(*metadata)->next = NULL;
				(*metadata)->name = strdup(key);
				(*metadata)->value = strdup(value);
				changed = 1;
			} else
			{ /* no more allowed */
				if (client)
					sendto_one(client, NULL, ":%s FAIL METADATA LIMIT_REACHED %s :metadata limit reached", me.name, target_name);
			}
		}
		if (!changed)
			return;
	}

	if (!IsServer(client) && MyConnect(client))
	{
		if (BadPtr(value))
			sendnumeric(client, RPL_KEYNOTSET, (*target_name)?target_name:"*", key); /* ok but empty */
		else
			sendnumeric(client, RPL_KEYVALUE, (*target_name)?target_name:"*", key, "*", value?value:""); /* all OK */
	}
	if (changed && (client == &me || IsUser(client) || IsServer(client)))
		user_metadata_changed(target, key, value, client);
}

void metadata_set_channel(Channel *channel, const char *key, const char *value, Client *client)
{
	int changed = 0;
	int set = 0;
	int count = 0;
	struct metadata **metadata = (struct metadata **)&CHANNEL_METADATA(channel);
	struct metadata *prev;

	if(BadPtr(value) || strlen(value) == 0)
	{ /* unset */
		value = NULL; /* just to make sure */
		int removed = 0;
		while (*metadata)
		{
			if (!strcasecmp(key, (*metadata)->name))
				break;
			metadata = &(*metadata)->next;
		}
		if (*metadata)
		{
			prev = *metadata;
			*metadata = prev->next;
			metadata_free(prev);
			removed = 1;
			changed = 1;
		}
		if (!removed)
		{
			if (client)
				sendto_one(client, NULL, ":%s FAIL METADATA KEY_NOT_SET %s %s :key not set", me.name, channel->name, key); /* not set so can't remove */
			return;
		}
	} else { /* set */
		while (*metadata)
		{
			if (!strcasecmp(key, (*metadata)->name))
			{
				set = 1;
				if (strcmp(value, (*metadata)->value))
				{
					safe_free((*metadata)->value);
					(*metadata)->value = strdup(value);
					changed = 1;
				}
			}
			metadata = &(*metadata)->next;
			count++;
		}
		if (!set)
		{
			if (!client || count < metadata_settings.max_channel_metadata)
			{ /* add new entry for user */
				*metadata = safe_alloc(sizeof(struct metadata));
				(*metadata)->next = NULL;
				(*metadata)->name = strdup(key);
				(*metadata)->value = strdup(value);
				changed = 1;
			} else
			{ /* no more allowed */
				if (client)
					sendto_one(client, NULL, ":%s FAIL METADATA LIMIT_REACHED %s :metadata limit reached", me.name, channel->name);
			}
		}
		if (!changed)
			return;
	}
	if (IsUser(client) && MyUser(client))
	{
		if (BadPtr(value))
			sendnumeric(client, RPL_KEYNOTSET, channel->name, key); /* ok but empty */
		else
			sendnumeric(client, RPL_KEYVALUE, channel->name, key, "*", value?value:""); /* all OK */
	}
	if (changed && (IsUser(client) || IsServer(client)))
		channel_metadata_changed(channel, key, value, client);
}

int metadata_subscribe(const char *key, Client *client, int remove, MessageTag *mtags)
{
	struct metadata_moddata_user *moddata = USER_METADATA(client);
	struct metadata_subscriptions **subs;
	struct metadata_subscriptions *prev_subs;
	int found = 0;
	int count = 0;
	const char *value;
	unsigned int hashnum;
	Channel *channel;
	Client *acptr;

	if (!client)
		return 0;
	
	if (!moddata) /* first call for this user */
		moddata = metadata_prepare_user_moddata(client);
	subs = &moddata->subs;
	while (*subs)
	{
		count++;
		if (!strcasecmp(key, (*subs)->name))
		{
			found = 1;
			if (remove)
			{
				prev_subs = *subs;
				*subs = prev_subs->next;
				metadata_free_subs(prev_subs);
			}
			break;
		}
		subs = &(*subs)->next;
	}
	if (!remove && !found)
	{
		if (count < metadata_settings.max_subscriptions)
		{
			*subs = safe_alloc(sizeof(struct metadata_subscriptions));
			(*subs)->next = NULL;
			(*subs)->name = strdup(key);
		} else
		{ /* no more allowed */
			sendto_one(client, NULL, ":%s FAIL METADATA TOO_MANY_SUBS %s :too many subscriptions", me.name, key);
			return 0;
		}
	}
	if (!remove)
	{
		sendnumeric(client, RPL_METADATASUBOK, key);
		if(!IsUser(client))
			return 1; /* unregistered user is not getting any keys yet */
		/* we have to send out all subscribed data now */
		list_for_each_entry(acptr, &client_list, client_node)
		{
			if (!IsUser(acptr))
				continue;
			value = NULL;
			if (has_common_channels(acptr, client) || metadata_is_monitoring(client, acptr))
				value = metadata_get_user_key_value(acptr, key);
			if (value)
				metadata_notify_or_queue(client, mtags, acptr, NULL, key, value, NULL);
		}
		for (hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++)
		{
			for (channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch)
			{
				if (IsMember(client, channel))
				{
					value = metadata_get_channel_key_value(channel, key);
					if (value)
						metadata_notify_or_queue(client, mtags, NULL, channel, key, value, NULL);
				}
			}
		}
	} else
	{
		sendnumeric(client, RPL_METADATAUNSUBOK, key);	
	}
	return 1;
}

void metadata_send_channel(Channel *channel, const char *key, Client *client, MessageTag *mtags)
{
	struct metadata *metadata;
	int found = 0;
	char batch[BATCHLEN+1] = "";
	int parent_mtags = !!mtags;
	if (!parent_mtags)
		MAKE_BATCH(client, batch, mtags, channel->name);
	for (metadata = CHANNEL_METADATA(channel); metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
		{
			found = 1;
			sendnumeric_mtags(client, mtags, RPL_KEYVALUE, channel->name, key, "*", metadata->value);
			break;
		}
	}
	if (!found)
		sendnumeric_mtags(client, mtags, RPL_KEYNOTSET, channel->name, key);
	if (!parent_mtags)
		FINISH_BATCH(client, batch, mtags);
}

void metadata_send_user(Client *user, const char *key, Client *client, MessageTag *mtags)
{
	if (!user)
		user = client;
	struct metadata_moddata_user *moddata = USER_METADATA(user);
	struct metadata *metadata = NULL;
	char batch[BATCHLEN+1] = "";
	int parent_mtags = !!mtags;
	if (!parent_mtags)
		MAKE_BATCH(client, batch, mtags, user->name);
	if (moddata)
		metadata = moddata->metadata;
	int found = 0;
	for ( ; metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
		{
			found = 1;
			sendnumeric_mtags(client, mtags, RPL_KEYVALUE, user->name, key, "*", metadata->value);
			break;
		}
	}
	if (!found)
		sendnumeric_mtags(client, mtags, RPL_KEYNOTSET, user->name, key);
	if (!parent_mtags)
		FINISH_BATCH(client, batch, mtags);
}

void metadata_clear_channel(Channel *channel, Client *client, MessageTag *mtags)
{
	struct metadata *metadata = CHANNEL_METADATA(channel);
	metadata_free_list(metadata, channel->name, client, mtags);
	CHANNEL_METADATA(channel) = NULL;
}

void metadata_clear_user(Client *user, Client *client, MessageTag *mtags)
{
	if (!user)
		user = client;
	struct metadata_moddata_user *moddata = USER_METADATA(user);
	struct metadata *metadata = NULL;
	if (!moddata)
		return; /* nothing to delete */
	metadata = moddata->metadata;
	metadata_free_list(metadata, user->name, client, mtags);
	moddata->metadata = NULL;
}

void metadata_send_subscribtions(Client *client)
{
	struct metadata_subscriptions *subs;
	struct metadata_moddata_user *moddata = USER_METADATA(client);
	char batch[BATCHLEN+1] = "";
	MessageTag *mtags = NULL;

	if (!moddata || !moddata->subs)
		return; /* no subscriptions: no replies */

	/* Create metadata-subs batch */
	if (HasCapability(client, "batch")) {
		generate_batch_id(batch);
		sendto_one(client, NULL, ":%s BATCH +%s metadata-subs", me.name, batch);
		mtags = safe_alloc(sizeof(MessageTag));
		mtags->name = strdup("batch");
		mtags->value = strdup(batch);
	}

	for (subs = moddata->subs; subs; subs = subs->next)
		sendnumeric_mtags(client, mtags, RPL_METADATASUBS, subs->name);

	FINISH_BATCH(client, batch, mtags);
}

void metadata_send_all_for_channel(Channel *channel, Client *client)
{
	struct metadata *metadata;
	char batch[BATCHLEN+1] = "";
	MessageTag *mtags = NULL;
	MAKE_BATCH(client, batch, mtags, channel->name);
	for (metadata = CHANNEL_METADATA(channel); metadata; metadata = metadata->next)
		sendnumeric_mtags(client, mtags, RPL_KEYVALUE, channel->name, metadata->name, "*", metadata->value);
	FINISH_BATCH(client, batch, mtags);
}

void metadata_send_all_for_user(Client *user, Client *client)
{
	struct metadata *metadata;
	char batch[BATCHLEN+1] = "";
	MessageTag *mtags = NULL;
	if (!user)
		user = client;
	struct metadata_moddata_user *moddata = USER_METADATA(user);
	MAKE_BATCH(client, batch, mtags, user->name);
	if (moddata) {
		for (metadata = moddata->metadata; metadata; metadata = metadata->next)
			sendnumeric_mtags(client, mtags, RPL_KEYVALUE, user->name, metadata->name, "*", metadata->value);
	}
	FINISH_BATCH(client, batch, mtags);
}

int metadata_key_valid(const char *key)
{
	/* Empty string is invalid */
	if (!key || !*key)
		return 0;

	for( ; *key; key++)
	{
		if(*key >= 'a' && *key <= 'z')
			continue;
		if(*key >= '0' && *key <= '9')
			continue;
		if(*key == '_' || *key == '.' || *key == '/' || *key == '-')
			continue;
		return 0;
	}
	return 1;
}

int metadata_check_perms(Client *user, Channel *channel, Client *client, const char *key, int mode)
{ /* either user or channel should be NULL */
	if (!IsUser(client) && channel) /* ignore channel metadata requests for unregistered users */
		return 0;
	if ((user == client) || (!user && !channel)) /* specified target is "*" or own nick */
		return 1;
	if (IsOper(client) && mode == MODE_GET)
		return 1; /* allow ircops to view everything */
	if (channel)
	{
		/* The only requirement for GET is to be in the channel */
		if ((mode == MODE_GET) && IsMember(client, channel))
			return 1;
		/* Otherwise, +hoaq */
		if (check_channel_access(client, channel, "hoaq"))
			return 1;
	} else if (user)
	{
		if (mode == MODE_SET)
		{
			if (user == client)
				return 1;
		} else if (mode == MODE_GET)
		{
			if(has_common_channels(user, client))
				return 1;
		}
		
	}
	if (key)
		sendto_one(client, NULL, ":%s FAIL METADATA KEY_NO_PERMISSION %s %s :permission denied", me.name, user?user->name:channel->name, key);
	return 0;
}

/* METADATA <Target> <Subcommand> [<Param 1> ... [<Param n>]] */
CMD_FUNC(cmd_metadata_local)
{
	Channel *channel = NULL;
	Client *user = NULL;
	const char *target;
	const char *cmd;
	const char *key;
	const char *value = NULL;
	int keyindex = 3-1;
	char *channame;
	MessageTag *batch_mtags = NULL;
	char batch[BATCHLEN+1] = "";
	
	CHECKPARAMSCNT_OR_DIE(2, return);

	target = parv[1];
	cmd = parv[2];

	if (!strcasecmp(cmd, "GET"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		MAKE_BATCH(client, batch, batch_mtags, channel ? channel->name : user->name);
		FOR_EACH_KEY(keyindex, parc, parv)
		{
			if (!metadata_check_perms(user, channel, client, NULL, MODE_GET))
			{
				sendto_one(client, batch_mtags, ":%s FAIL METADATA KEY_NO_PERMISSION %s %s :permission denied",
					me.name, channel ? channel->name : user->name, key);
				continue;
			}
			if (!metadata_key_valid(key))
			{
				sendto_one(client, batch_mtags, ":%s FAIL METADATA KEY_INVALID %s :invalid key", me.name, key);
				continue;
			}
			if (channel)
				metadata_send_channel(channel, key, client, batch_mtags);
			else
				metadata_send_user(user, key, client, batch_mtags);
		}
		FINISH_BATCH(client, batch, batch_mtags);
	} else if (!strcasecmp(cmd, "LIST"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		if (!metadata_check_perms(user, channel, client, NULL, MODE_GET))
		{
			const char *tname = channel ? channel->name : user->name;
			char perm_batch[BATCHLEN+1] = "";
			MessageTag *perm_mtags = NULL;
			MAKE_BATCH(client, perm_batch, perm_mtags, tname);
			sendto_one(client, perm_mtags, ":%s FAIL METADATA KEY_NO_PERMISSION %s * :permission denied",
				me.name, tname);
			FINISH_BATCH(client, perm_batch, perm_mtags);
		} else {
			if (channel)
				metadata_send_all_for_channel(channel, client);
			else
				metadata_send_all_for_user(user, client);
		}
	} else if (!strcasecmp(cmd, "SET"))
	{
		CHECKPARAMSCNT_OR_DIE(3, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		key = parv[3];
		if (!metadata_check_perms(user, channel, client, key, MODE_SET))
			return;
		if (parc > 3 && !BadPtr(parv[4]))
			value = parv[4];

		/* validity checks */
		if (!metadata_key_valid(key))
		{
			sendto_one(client, NULL, ":%s FAIL METADATA KEY_INVALID %s :invalid key", me.name,  key);
			return;
		}

		if (value && strlen(value) > metadata_settings.max_value_bytes)
		{
			sendto_one(client, NULL, ":%s FAIL METADATA VALUE_INVALID :value is too long or not UTF8", me.name);
			return;
		}

		if (value && !unrl_utf8_validate(value, NULL))
		{
			sendto_one(client, NULL, ":%s FAIL METADATA VALUE_INVALID :value is too long or not UTF8", me.name);
			return;
		}
		
		/* proceed with SET */
		if (channel)
			metadata_set_channel(channel, key, value, client);
		else
			metadata_set_user(user, key, value, client);
	} else if (!strcasecmp(cmd, "CLEAR"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		if (metadata_check_perms(user, channel, client, "*", MODE_SET))
		{
			char clear_batch[BATCHLEN+1] = "";
			MessageTag *clear_mtags = NULL;
			MAKE_BATCH(client, clear_batch, clear_mtags, channel ? channel->name : user->name);
			if (channel)
				metadata_clear_channel(channel, client, clear_mtags);
			else
				metadata_clear_user(user, client, clear_mtags);
			FINISH_BATCH(client, clear_batch, clear_mtags);
		}
	} else if (!strcasecmp(cmd, "SUB"))
	{
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		FOR_EACH_KEY(keyindex, parc, parv)
		{
			if(metadata_key_valid(key))
			{
				/* Stop processing if subscription limit is reached */
				if (!metadata_subscribe(key, client, 0, NULL))
					break;
			} else
			{
				sendto_one(client, NULL, ":%s FAIL METADATA KEY_INVALID %s :invalid key", me.name,  key);
				continue;
			}
		}
	} else if (!strcasecmp(cmd, "UNSUB"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		FOR_EACH_KEY(keyindex, parc, parv)
		{
			if(metadata_key_valid(key))
			{
				metadata_subscribe(key, client, 1, NULL);
			} else
			{
				sendto_one(client, NULL, ":%s FAIL METADATA KEY_INVALID %s :invalid key", me.name,  key);
				continue;
			}
		}
	} else if (!strcasecmp(cmd, "SUBS"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		metadata_send_subscribtions(client);
	} else if (!strcasecmp(cmd, "SYNC"))
	{ /* the client requested re-sending of all subbed metadata */
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		if (channel)
			metadata_sync_channel(client, channel);
		else
			metadata_sync_user(client, user, NULL, 1);
	} else
	{
		sendto_one(client, NULL, ":%s FAIL METADATA SUBCOMMAND_INVALID %s :invalid subcommand", me.name,  cmd);
	}
}

/* format of S2S is same as the event: ":origin METADATA <client/channel> <key name> *[ :<key value>]" */
CMD_FUNC(cmd_metadata_remote)
{ /* handling data from linked server */
	Channel *channel = NULL;
	Client *user = NULL;
	const char *target;
	const char *key;
	const char *value;
	const char *channame;

	if (parc < 4 || BadPtr(parv[3]))
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", client, "METADATA S2S: not enough args from $sender",
			log_data_string("sender", client->name));
		return;
	}
	if (parc < 5 || BadPtr(parv[4]))
	{
		value = NULL;
	}
	else
	{
		value = parv[4];
	}

	/* parv[3] "visibility" always present - we ignore it for now */
	target = parv[1];
	key = parv[2];
	channame = strchr(target, '#');

	if (!*target || !strcmp(target, "*") || !metadata_key_valid(key))
	{
		unreal_log(ULOG_DEBUG, "metadata", "METADATA_DEBUG", client, "METADATA S2S: bad metadata target $target or key $key from $sender",
			log_data_string("target", target),
			log_data_string("key", key),
			log_data_string("sender", client->name));
		return;
	}
	PROCESS_TARGET_OR_DIE(target, user, channel, return);

	if(channel)
	{
		metadata_set_channel(channel, key, value, client);
	} else
	{
		metadata_set_user(user, key, value, client);
	}
}

#if UNREAL_VERSION >= 0x06020000
CMD_FUNC(cmd_metadata)
{
	if (client != &me && MyConnect(client) && !IsServer(client))
		cmd_metadata_local(clictx, client, recv_mtags, parc, parv);
	else
		cmd_metadata_remote(clictx, client, recv_mtags, parc, parv);
}
#else
CMD_FUNC(cmd_metadata)
{
	if (client != &me && MyConnect(client) && !IsServer(client))
		cmd_metadata_local(client, recv_mtags, parc, parv);
	else
		cmd_metadata_remote(client, recv_mtags, parc, parv);
}
#endif /* UNREAL_VERSION */

int metadata_server_sync(Client *client)
{ /* we send all our data to the server that was just linked */
	Client *acptr;
	struct metadata_moddata_user *moddata;
	struct metadata *metadata;
	unsigned int  hashnum;
	Channel *channel;
	
	list_for_each_entry(acptr, &client_list, client_node)
	{ /* send out users (all on our side of the link) */
		moddata = USER_METADATA(acptr);
		if(!moddata)
			continue;
		for (metadata = moddata->metadata; metadata; metadata = metadata->next)
			metadata_send_change(client, NULL, acptr->name, metadata->name, metadata->value, &me);
	}

	for (hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++)
	{ /* send out channels */
		for(channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch)
		{
			for(metadata = CHANNEL_METADATA(channel); metadata; metadata = metadata->next)
				metadata_send_change(client, NULL, channel->name, metadata->name, metadata->value, &me);
		}
	}
	return 0;
}

static int has_common_channels_except(Client *c1, Client *c2, Channel *except)
{
	Membership *lp;
	for (lp = c1->user->channel; lp; lp = lp->next)
	{
		if (lp->channel == except)
			continue;
		if (IsMember(c2, lp->channel) && user_can_see_member(c1, c2, lp->channel))
			return 1;
	}
	return 0;
}

int metadata_join(Client *client, Channel *channel, MessageTag *join_mtags)
{
	Client *acptr;
	Member *cm;
	const char *value;
	struct metadata_unsynced *prev_us;
	struct metadata_unsynced *us;
	Membership *lp;
	struct metadata_subscriptions *subs;
	struct metadata *metadata;
	char batch[BATCHLEN+1] = "";
	MessageTag *batch_mtags = NULL;

	struct metadata_moddata_user *moddata = USER_METADATA(client);
	if(!moddata)
		return 0; /* the user is both not subscribed to anything and has no own data */
	for (metadata = moddata->metadata; metadata; metadata = metadata->next)
	{ /* if joining user has metadata, let's notify all subscribers */
		list_for_each_entry(acptr, &lclient_list, lclient_node)
		{
			if(IsMember(acptr, channel) && metadata_is_subscribed(acptr, metadata->name))
				metadata_notify_or_queue(acptr, NULL, client, NULL, metadata->name, metadata->value, NULL);
		}
	}
	if (!MyConnect(client))
		return 0;
	/* if joining user connection is local, let's send notifications to it */
	MAKE_BATCH(client, batch, batch_mtags, channel->name);
	for (subs = moddata->subs; subs; subs = subs->next)
	{
		value = metadata_get_channel_key_value(channel, subs->name); /* notify joining user about channel metadata */
		if(value)
			metadata_notify_or_queue(client, batch_mtags, NULL, channel, subs->name, value, NULL);
		for (cm = channel->members; cm; cm = cm->next)
		{ /* notify joining user about other channel members' metadata */
			acptr = cm->client;
			if (acptr == client)
				continue; /* ignore own data */
			if (has_common_channels_except(acptr, client, channel))
				continue; /* already seen elsewhere */
			value = metadata_get_user_key_value(acptr, subs->name);
			if (value)
				metadata_notify_or_queue(client, batch_mtags, acptr, NULL, subs->name, value, NULL);
		}
	}
	FINISH_BATCH(client, batch, batch_mtags);
	return 0;
}

void metadata_send_pending(Client *client)
{
	struct metadata_moddata_user *my_moddata = USER_METADATA(client);
	if (!my_moddata)
		return; /* nothing queued */
	struct metadata_unsynced *us = my_moddata->us;
	struct metadata_unsynced *prev_us;

	while (us)
	{
		Client *acptr = NULL;
		Channel *channel = NULL;
		int do_send = 0;
		char *who = NULL;

		if (!IsSendable(client))
			break;
		if (*us->id == '#')
		{
			channel = find_channel(us->id);
			if (channel && IsMember(client, channel)) {
				do_send = 1;
				who = us->id;
			}
		} else
		{
			acptr = find_client(us->id, NULL);
			if (acptr && has_common_channels(acptr, client)) { /* if not, the user has vanished since or one of us parted the channel */
				do_send = 1;
				who = acptr->name;
			}
		}

		if (do_send)
		{
			struct metadata *metadata;
			if (acptr)
			{
				struct metadata_moddata_user *umd = USER_METADATA(acptr);
				metadata = umd ? umd->metadata : NULL;
			} else
			{
				metadata = (struct metadata *)CHANNEL_METADATA(channel);
			}
			for ( ; metadata; metadata = metadata->next)
			{
				if (!strcasecmp(us->key, metadata->name))
				{
					if (metadata->value)
						metadata_send_change(client, NULL, who, us->key, metadata->value, NULL);
					break;
				}
			}
		}
		/* now remove the processed entry */
		prev_us = us;
		us = us->next;
		safe_free(prev_us->id);
		safe_free(prev_us->key);
		safe_free(prev_us);
		my_moddata->us = us; /* we're always removing the first list item */
	}
}

int metadata_user_registered(Client *client)
{
	struct metadata *metadata;
	struct metadata_moddata_user *moddata = USER_METADATA(client);

	/* Send registration burst to this client if they negotiated draft/metadata-2 */
	if (MyUser(client) && HasCapabilityFast(client, CAP_METADATA))
	{
		char batch[BATCHLEN+1] = "";
		MessageTag *mtags = NULL;
		labeled_response_inhibit = 1;
		MAKE_BATCH(client, batch, mtags, client->name);
		if (moddata)
		{
			for (metadata = moddata->metadata; metadata; metadata = metadata->next)
				metadata_send_change(client, mtags, client->name, metadata->name, metadata->value, NULL);
		}
		FINISH_BATCH(client, batch, mtags);
		labeled_response_inhibit = 0;
	}

	/* Propagate to linked servers */
	if (moddata)
	{
		for (metadata = moddata->metadata; metadata; metadata = metadata->next)
		{
			sendto_server(client, 0, 0, NULL, ":%s METADATA %s %s * :%s",
				client->id, client->id, metadata->name, metadata->value ? metadata->value : "");
		}
	}

	return HOOK_CONTINUE;
}

void metadata_sync_user(Client *client, Client *target, MessageTag *mtags, int create_batch) {
	char batch[BATCHLEN+1] = "";
	struct metadata *metadata;
	int parent_mtags = 0;
	
	if (mtags)
		parent_mtags = 1;

	struct metadata_moddata_user *moddata = USER_METADATA(target);

	if (!parent_mtags && create_batch)
		MAKE_BATCH(client, batch, mtags, target->name);

	if (moddata) { /* the user is either subscribed to something (this is not interesting to us) or has some own data */
		for (metadata = moddata->metadata; metadata; metadata = metadata->next)
		{
			if(metadata_is_subscribed(client, metadata->name))
				metadata_notify_or_queue(client, mtags, target, NULL, metadata->name, metadata->value, NULL);
		}
	}

	if (!parent_mtags)
		FINISH_BATCH(client, batch, mtags);
}

void metadata_sync_channel(Client *client, Channel *channel) {
	MessageTag *mtags = NULL;
	char batch[BATCHLEN+1] = "";
	Member *cm;
	struct metadata_subscriptions *subs;
	const char *value;
	struct metadata_moddata_user *moddata = USER_METADATA(client);

	MAKE_BATCH(client, batch, mtags, channel->name);

	if (moddata)
	{
		for (subs = moddata->subs; subs; subs = subs->next)
		{
			value = metadata_get_channel_key_value(channel, subs->name); /* channel metadata notification */
			if(value)
				metadata_notify_or_queue(client, mtags, NULL, channel, subs->name, value, NULL);
		}
		for (cm = channel->members; cm; cm = cm->next) /* notify about all channel members' metadata (including the query source) */
			metadata_sync_user(client, cm->client, mtags, 0);
	}

	FINISH_BATCH(client, batch, mtags);
}

EVENT(metadata_queue_evt)
{ /* let's check every 1.5 seconds whether we have something to send */
	Client *acptr;
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if (!IsUser(acptr))
			continue;
		metadata_send_pending(acptr);
	}
}

void metadata_notify_monitored(Client *client, Client *monitored, Client *changer, const char *key, const char *value)
{

	if (!HasCapabilityFast(client, CAP_METADATA_NOTIFY))
		return;
	if (has_common_channels(client, monitored))
		return; /* already notified */
	if (!key)
		metadata_sync_user(client, monitored, NULL, 0);
	else
	{
		if (metadata_is_subscribed(client, key))
			metadata_notify_or_queue(client, NULL, monitored, NULL, key, value, changer);
	}
}

int metadata_is_monitoring(Client *watcher, Client *watched)
{
	Link *lp;
	Watch *watch = watch_get(watched->name);

	if (!watch)
		return 0;	 /* This nick isn't on watch */
	
	for (lp = watch->watch; lp; lp = lp->next)
	{
		if (lp->value.client == watcher && (lp->flags & WATCH_FLAG_TYPE_MONITOR))
			return 1;
	}
	
	return 0;
}

#if UNREAL_VERSION_TIME < 202346

int metadata_monitor_connect(Client *client) {
	watch_check(client, WATCH_EVENT_ONLINE, metadata_monitor_notification);
	return 0;
}

int metadata_monitor_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	if (!smycmp(client->name, oldnick)) // new nick is same as old one, maybe the case changed
		return 0;

	watch_check(client, WATCH_EVENT_ONLINE, metadata_monitor_notification);
	return 0;
}

int metadata_monitor_notification(Client *client, Watch *watch, Link *lp, int event)
{
	if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
		return 0;
	if (!HasCapabilityFast(lp->value.client, CAP_METADATA_NOTIFY))
		return 0;

	switch (event)
	{
		case WATCH_EVENT_ONLINE:
		case WATCH_EVENT_METADATA:
			metadata_notify_monitored(lp->value.client, client, metadata_monitor_data.changer, metadata_monitor_data.key, metadata_monitor_data.value);
			break;
		default:
			break; /* may be handled by other modules */
	}
	
	return 0;
}

#define WATCH(client) (moddata_local_client(client, watchListMD).ptr)

CMD_OVERRIDE_FUNC(metadata_overridemonitor)
{
	char request[BUFSIZE];
	char *s, *p = NULL;
	Link *lp;
	Client *monitored;

	CallCommandOverride(ovr, client, recv_mtags, parc, parv);

	/* subset of MONITOR command handling as we only need the + action */
	if (!MyUser(client))
		return;
	if (parc < 3 || BadPtr(parv[2]) || *parv[1] != '+')
		return;

	ModDataInfo *watchListMD = findmoddata_byname("watchList", MODDATATYPE_LOCAL_CLIENT);
	
	if (!watchListMD)
		return;
	strlcpy(request, parv[2], sizeof(request));
	for (s = strtoken(&p, request, ","); s; s = strtoken(&p, NULL, ","))
	{
		lp = WATCH(client);
		while (lp)
		{
			if (strcmp(lp->value.wptr->nick, s)) /* checking if original MONITOR command succeeded in adding this entry */
			{
				lp = lp->next;
				continue;
			}
			monitored = find_client(s, NULL);
			if (!monitored || has_common_channels(client, monitored))
			{
				lp = lp->next;
				continue;
			}
			metadata_sync_user(client, monitored, NULL, 1);
			lp = lp->next;
		}
	}
}

#else /* UNREAL_VERSION_TIME < 202346 */

int metadata_monitor_notification(Client *client, Watch *watch, Link *lp, int event, void *data)
{
	struct metadata_monitor_s *mond = data;
	if (!(lp->flags & WATCH_FLAG_TYPE_MONITOR))
		return 0;

	if (event == WATCH_EVENT_METADATA) /* for now we don't have any other event anyway */
		metadata_notify_monitored(lp->value.client, client, mond->changer, mond->key, mond->value);
	
	return 0;
}

int metadata_monitor_online(Client *watcher, Client *client, int online)
{
	if (!online)
		return 0;
	metadata_notify_monitored(watcher, client, NULL, NULL, NULL);
	return 0;
}

int metadata_watch_add(char *nick, Client *client, int flags)
{
	Client *monitored;
	if (!(flags & WATCH_FLAG_TYPE_MONITOR))
		return 0;

	monitored = find_client(nick, NULL);
	if (monitored)
		metadata_notify_monitored(client, monitored, NULL, NULL, NULL);
	return 0;
}

#endif /* UNREAL_VERSION_TIME < 202346 */

