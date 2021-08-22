/*
 *   IRC - Internet Relay Chat, src/modules/metadata.c
 *   (C) 2021 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* get or set for perms */
#define MODE_SET 0
#define MODE_GET 1

#define MYCONF "metadata"

#ifdef PREFIX_AQ
#define HOP_OR_MORE (CHFL_HALFOP | CHFL_CHANOP | CHFL_CHANADMIN | CHFL_CHANOWNER)
#else
#define HOP_OR_MORE (CHFL_HALFOP | CHFL_CHANOP)
#endif

#define CHECKPARAMSCNT_OR_DIE(count, return) { if(parc < count+1 || BadPtr(parv[count])) { sendnumeric(client, ERR_NEEDMOREPARAMS, "METADATA"); return; } }

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
			sendnumeric(client, ERR_NOSUCHNICK, channame); \
			return; \
		} \
	} else \
	{ \
		if (strcmp(target, "*")) \
		{ \
			user = hash_find_nickatserver(target, NULL); \
			if (!user) \
			{ \
				sendnumeric(client, ERR_NOSUCHNICK, target); \
				return; \
			} \
		} else \
		{ \
			user = client; \
		} \
	} \
}

#define FOR_EACH_KEY() while(keyindex++, key = parv[keyindex], (!BadPtr(key) && keyindex < parc))
#define IsSendable(x)		(DBufLength(&x->local->sendQ) < 2048)
#define CHECKREGISTERED_OR_DIE(client, return)	{ if(!IsUser(client)){ sendnumeric(client, ERR_NOTREGISTERED); return; } }

struct metadata {
	char *name;
	char *value;
	struct metadata *next;
};

struct subscriptions {
	char *name;
	struct subscriptions *next;
};

struct moddata_user {
	struct metadata *metadata;
	struct subscriptions *subs;
	struct unsynced *us;
};

struct unsynced { /* we're listing users (nicknames) that should be synced but were not */
	char *name;
	char *key;
	struct unsynced *next;
};

CMD_FUNC(cmd_metadata);
CMD_FUNC(cmd_metadata_remote);
CMD_FUNC(cmd_metadata_local);
EVENT(metadata_queue_evt);
char *metadata_cap_param(Client *client);
char *metadata_isupport_param(void);
int metadata_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int metadata_configposttest(int *errs);
int metadata_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
/* hooks */
int metadata_server_sync(Client *client);
int metadata_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);
int metadata_user_registered(Client *client);

void metadata_user_free(ModData *md);
void metadata_channel_free(ModData *md);
void sendnumeric(Client *to, int numeric, ...);
void free_metadata(struct metadata *metadata);
void free_subs(struct subscriptions *subs);
int is_subscribed(Client *user, char *key);
char *get_user_key_value(Client *user, char *key);
char *get_channel_key_value(Channel *channel, char *key);
void user_metadata_changed(Client *user, char *key, char *value, Client *changer);
void channel_metadata_changed(Channel *channel, char *key, char *value, Client *changer);
void metadata_free_list(struct metadata *metadata, char *whose, Client *client);
struct moddata_user *prepare_user_moddata(Client *user);
void metadata_set_channel(Channel *channel, char *key, char *value, Client *client);
void metadata_set_user(Client *user, char *key, char *value, Client *client);
void metadata_send_channel(Channel *channel, char *key, Client *client);
void metadata_send_user(Client *user, char *key, Client *client);
int metadata_subscribe(char *key, Client *client, int remove);
void metadata_clear_channel(Channel *channel, Client *client);
void metadata_clear_user(Client *user, Client *client);
void metadata_send_subscribtions(Client *client);
void metadata_send_all_for_channel(Channel *channel, Client *client);
void metadata_send_all_for_user(Client *user, Client *client);
void metadata_sync(Client *client);
int key_valid(char *key);
int check_perms(Client *user, Channel *channel, Client *client, char *key, int mode);
void send_change(Client *client, char *who, char *key, char *value, Client *changer);
int notify_or_queue(Client *client, char *who, char *key, char *value, Client *changer);

ModDataInfo *metadataUser;
ModDataInfo *metadataChannel;
long CAP_METADATA = 0L;
long CAP_METADATA_NOTIFY = 0L;

struct settings {
	int max_user_metadata;
	int max_channel_metadata;
	int max_subscriptions;
	int enable_debug;
} metadata_settings;

ModuleHeader MOD_HEADER = {
	"metadata",
	"6.0",
	"draft/metadata and draft/metadata-notify-2 cap",
	"UnrealIRCd Team",
	"unrealircd-6"
};

/*
metadata {
	max-user-metadata 10;
	max-channel-metadata 10;
	max-subscriptions 10;
	enable-debug 0;
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
			if (!errors && (atoi(cep->value) < 0 || atoi(cep->value) > 100))
			{
				config_error("%s:%i: %s::%s must be an integer between 1 and 100", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
			}
			continue;
		}

		if (!strcmp(cep->name, "enable-debug"))
		{
			if (strlen(cep->value) != 1 || (cep->value[0] != '0' && cep->value[0] != '1'))
			{
				config_error("%s:%i: %s::%s must be 0 or 1", cep->file->filename, cep->line_number, MYCONF, cep->name);
				errors++;
				break;
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

		if (!strcmp(cep->name, "enable-debug"))
		{
			metadata_settings.enable_debug = atoi(cep->value);
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
	cap.name = "draft/metadata";
	cap.parameter = metadata_cap_param;
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_METADATA);
	
	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/metadata-notify-2"; /* for irccloud compatibility */
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

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, metadata_configrun);
	
	return MOD_SUCCESS;
}

MOD_LOAD() {
	ISupportAdd(modinfo->handle, "METADATA", metadata_isupport_param());
	/* setting default values if not configured */
	if(metadata_settings.max_user_metadata == 0)
		metadata_settings.max_user_metadata = 10;
	if(metadata_settings.max_channel_metadata == 0)
		metadata_settings.max_channel_metadata = 10;
	if(metadata_settings.max_subscriptions == 0)
		metadata_settings.max_subscriptions = 10;

	EventAdd(modinfo->handle, "metadata_queue", metadata_queue_evt, NULL, 1500, 0);

	return MOD_SUCCESS;
}

MOD_UNLOAD() {
	return MOD_SUCCESS;
}

char *metadata_cap_param(Client *client)
{
	static char buf[20];
	ircsnprintf(buf, sizeof(buf), "maxsub=%d", metadata_settings.max_subscriptions);
	return buf;
}

char *metadata_isupport_param(void)
{
	static char buf[20];
	ircsnprintf(buf, sizeof(buf), "%d", metadata_settings.max_user_metadata);
	return buf;
}

void free_metadata(struct metadata *metadata)
{
	safe_free(metadata->name);
	safe_free(metadata->value);
	safe_free(metadata);
}

void free_subs(struct subscriptions *subs)
{
	safe_free(subs->name);
	safe_free(subs);
}

int is_subscribed(Client *user, char *key)
{
	struct moddata_user *moddata = moddata_client(user, metadataUser).ptr;
	if (!moddata)
		return 0;
	struct subscriptions *subs;
	for (subs = moddata->subs; subs; subs = subs->next)
	{
		if (!strcasecmp(subs->name, key))
			return 1;
	}
	return 0;
}

char *get_user_key_value(Client *user, char *key)
{
	struct moddata_user *moddata = moddata_client(user, metadataUser).ptr;
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

char *get_channel_key_value(Channel *channel, char *key)
{
	struct metadata *metadata;
	for (metadata = moddata_channel(channel, metadataChannel).ptr; metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
			return metadata->value;
	}
	return NULL;
}

/* returns 1 if something remains to sync */
int notify_or_queue(Client *client, char *who, char *key, char *value, Client *changer)
{
	int trylater = 0;
	if (!who)
	{
		sendto_snomask(SNO_JUNK, "notify_or_queue called with null who!");
		return 0;
	}
	if (!key)
	{
		sendto_snomask(SNO_JUNK, "notify_or_queue called with null key!");
		return 0;
	}
	if (!client)
	{
		sendto_snomask(SNO_JUNK, "notify_or_queue called with null client!");
		return 0;
	}

	struct moddata_user *moddata = moddata_client(client, metadataUser).ptr;
	if (!moddata)
		moddata = prepare_user_moddata(client);
	struct unsynced **us = &moddata->us;

	if (IsSendable(client))
	{
		send_change(client, who, key, value, changer);
	} else
	{ /* store for the SYNC */
		trylater = 1;
		while (*us)
			us = &(*us)->next; /* find last list element */
		*us = safe_alloc(sizeof(struct unsynced));
		(*us)->name = strdup(who);
		(*us)->key = strdup(key);
		(*us)->next = NULL;
	}
	return trylater;
}

void send_change(Client *client, char *who, char *key, char *value, Client *changer)
{
	char *sender = NULL;
	if (!key)
	{
		sendto_snomask(SNO_JUNK, "send_change called with null key!");
		return;
	}
	if (!who)
	{
		sendto_snomask(SNO_JUNK, "send_change called with null who!");
		return;
	}
	if (!client)
	{
		sendto_snomask(SNO_JUNK, "send_change called with null client!");
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
		if (!value)
			sendto_one(client, NULL, ":%s!%s@%s METADATA %s %s %s", sender, changer->user->username, GetHost(changer), who, key, "*");
		else
			sendto_one(client, NULL, ":%s!%s@%s METADATA %s %s %s :%s", sender, changer->user->username, GetHost(changer), who, key, "*", value);
	} else
	{ /* sending S2S (sender is id) or receiving S2S (sender is servername) */
		if (!value)
			sendto_one(client, NULL, ":%s METADATA %s %s %s", sender, who, key, "*");
		else
			sendto_one(client, NULL, ":%s METADATA %s %s %s :%s", sender, who, key, "*", value);
	}
}

/* used for broadcasting changes to subscribed users and linked servers */
void user_metadata_changed(Client *user, char *key, char *value, Client *changer){
	Client *acptr;
	if (!user || !key)
		return; /* sanity check */
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if(IsUser(acptr) && IsUser(user) && is_subscribed(acptr, key) && has_common_channels(user, acptr))
			notify_or_queue(acptr, user->name, key, value, changer);
	}

	list_for_each_entry(acptr, &server_list, special_node)
	{ /* notifications for linked servers, TODO change to sendto_server */
		if (acptr == &me)
			continue;
		send_change(acptr, user->name, key, value, changer);
	}
}

void channel_metadata_changed(Channel *channel, char *key, char *value, Client *changer)
{
	Client *acptr;
	if (!channel || !key)
		return; /* sanity check */
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if (is_subscribed(acptr, key) && IsMember(acptr, channel))
			send_change(acptr, channel->name, key, value, changer);
	}
	
	list_for_each_entry(acptr, &server_list, special_node)
	{ /* notifications for linked servers, TODO change to sendto_server */
		if(acptr == &me)
			continue;
		send_change(acptr, channel->name, key, value, changer);
	}
}

void metadata_free_list(struct metadata *metadata, char *whose, Client *client)
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
			sendnumeric(client, RPL_KEYVALUE, whose, name, "*", "");
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
	metadata_free_list(metadata, NULL, NULL);
}

void metadata_user_free(ModData *md)
{
	struct moddata_user *moddata = md->ptr;
	if (!moddata)
		return; /* was not set */
	struct subscriptions *sub = moddata->subs;
	struct subscriptions *prev_sub = sub;
	struct unsynced *us = moddata->us;
	struct unsynced *prev_us;
	while (sub)
	{
		safe_free(sub->name);
		sub = sub->next;
		safe_free(prev_sub);
		prev_sub = sub;
	}
	struct metadata *metadata = moddata->metadata;
	metadata_free_list(metadata, NULL, NULL);
	while (us)
	{
		safe_free(us->name);
		safe_free(us->key);
		prev_us = us;
		us = us->next;
		safe_free(prev_us);
	}
	safe_free(moddata);
}

struct moddata_user *prepare_user_moddata(Client *user)
{
	moddata_client(user, metadataUser).ptr = safe_alloc(sizeof(struct moddata_user));
	struct moddata_user *ptr = moddata_client(user, metadataUser).ptr;
	ptr->metadata = NULL;
	ptr->subs = NULL;
	return ptr;
}

void metadata_set_user(Client *user, char *key, char *value, Client *client)
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
		
	struct moddata_user *moddata = moddata_client(target, metadataUser).ptr;
	if (!moddata) /* first call for this user */
		moddata = prepare_user_moddata(target);
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
			free_metadata(prev);
			removed = 1;
			changed = 1;
		}
		if (!removed)
		{
			if(client) sendnumeric(client, ERR_KEYNOTSET, target_name, key); // not set so can't remove
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
					sendnumeric(client, ERR_METADATALIMIT, target_name);
				return;
			}
		}
	}
	if (!IsServer(client) && MyConnect(client))
		sendnumeric(client, RPL_KEYVALUE, target_name, key, "*", value?value:""); /* all OK */
	if (changed && (client == &me || IsUser(client) || IsServer(client)))
		user_metadata_changed(target, key, value, client);
}

void metadata_set_channel(Channel *channel, char *key, char *value, Client *client)
{
	int changed = 0;
	int set = 0;
	int count = 0;
	struct metadata **metadata = (struct metadata **)&moddata_channel(channel, metadataChannel).ptr;
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
			free_metadata(prev);
			removed = 1;
			changed = 1;
		}
		if (!removed)
		{
			if (client)
				sendnumeric(client, ERR_KEYNOTSET, channel->name, key); /* not set so can't remove */
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
					sendnumeric(client, ERR_METADATALIMIT, channel->name);
				return;
			}
		}
	}
	if (IsUser(client) && MyUser(client))
		sendnumeric(client, RPL_KEYVALUE, channel->name, key, "*", value?value:""); /* all OK */
	if (changed && (IsUser(client) || IsServer(client)))
		channel_metadata_changed(channel, key, value, client);
}

int metadata_subscribe(char *key, Client *client, int remove)
{
	struct moddata_user *moddata = moddata_client(client, metadataUser).ptr;
	struct subscriptions **subs;
	struct subscriptions *prev_subs;
	int found = 0;
	int count = 0;
	int trylater = 0;
	char *value;
	unsigned int hashnum;
	Channel *channel;
	Client *acptr;
	if (!client)
		return 0;
	
	if (!moddata) /* first call for this user */
		moddata = prepare_user_moddata(client);
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
				free_subs(prev_subs);
			}
			break;
		}
		subs = &(*subs)->next;
	}
	if (!remove && !found)
	{
		if (count < metadata_settings.max_subscriptions)
		{
			*subs = safe_alloc(sizeof(struct subscriptions));
			(*subs)->next = NULL;
			(*subs)->name = strdup(key);
		} else
		{ /* no more allowed */
			sendnumeric(client, ERR_METADATATOOMANYSUBS, key);
			return 0;
		}
	}
	if (!remove)
	{
		sendnumeric(client, RPL_METADATASUBOK, key);
		if(!IsUser(client))
			return 0; /* unregistered user is not getting any keys yet */
		/* we have to send out all subscribed data now */
		trylater = 0;
		list_for_each_entry(acptr, &client_list, client_node)
		{
			value = NULL;
			if (IsUser(client) && IsUser(acptr) && has_common_channels(acptr, client))
				value = get_user_key_value(acptr, key);
			if (value)
				trylater |= notify_or_queue(client, acptr->name, key, value, NULL);
		}
		for (hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++)
		{
			for (channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch)
			{
				if (IsMember(client, channel))
				{
					value = get_channel_key_value(channel, key);
					if (value)
						trylater |= notify_or_queue(client, channel->name, key, value, NULL);
				}
			}
		}
		if (trylater)
			return 1;
	} else
	{
		sendnumeric(client, RPL_METADATAUNSUBOK, key);	
	}
	return 0;
}

void metadata_send_channel(Channel *channel, char *key, Client *client)
{
	struct metadata *metadata;
	int found = 0;
	for (metadata = moddata_channel(channel, metadataChannel).ptr; metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
		{
			found = 1;
			sendnumeric(client, RPL_KEYVALUE, channel->name, key, "*", metadata->value);
			break;
		}
	}
	if (!found)
		sendnumeric(client, ERR_NOMATCHINGKEY, channel->name, key);
}

void metadata_send_user(Client *user, char *key, Client *client)
{
	if (!user)
		user = client;
	struct moddata_user *moddata = moddata_client(user, metadataUser).ptr;
	struct metadata *metadata = NULL;
	if (moddata)
		metadata = moddata->metadata;
	int found = 0;
	for ( ; metadata; metadata = metadata->next)
	{
		if (!strcasecmp(key, metadata->name))
		{
			found = 1;
			sendnumeric(client, RPL_KEYVALUE, user->name, key, "*", metadata->value);
			break;
		}
	}
	if (!found)
		sendnumeric(client, ERR_NOMATCHINGKEY, user->name, key);
}

void metadata_clear_channel(Channel *channel, Client *client)
{
	struct metadata *metadata = moddata_channel(channel, metadataChannel).ptr;
	metadata_free_list(metadata, channel->name, client);
	moddata_channel(channel, metadataChannel).ptr = NULL;
}

void metadata_clear_user(Client *user, Client *client)
{
	if (!user)
		user = client;
	struct moddata_user *moddata = moddata_client(user, metadataUser).ptr;
	struct metadata *metadata = NULL;
	if (!moddata)
		return; /* nothing to delete */
	metadata = moddata->metadata;
	metadata_free_list(metadata, user->name, client);
	moddata->metadata = NULL;
}

void metadata_send_subscribtions(Client *client)
{
	struct subscriptions *subs;
	struct moddata_user *moddata = moddata_client(client, metadataUser).ptr;
	if (!moddata)
		return;
	for (subs = moddata->subs; subs; subs = subs->next)
		sendnumeric(client, RPL_METADATASUBS, subs->name);
}

void metadata_send_all_for_channel(Channel *channel, Client *client)
{
	struct metadata *metadata;
	for (metadata = moddata_channel(channel, metadataChannel).ptr; metadata; metadata = metadata->next)
		sendnumeric(client, RPL_KEYVALUE, channel->name, metadata->name, "*", metadata->value);
}

void metadata_send_all_for_user(Client *user, Client *client)
{
	struct metadata *metadata;
	if (!user)
		user = client;
	struct moddata_user *moddata = moddata_client(user, metadataUser).ptr;
	if (!moddata)
		return;
	for (metadata = moddata->metadata; metadata; metadata = metadata->next)
		sendnumeric(client, RPL_KEYVALUE, user->name, metadata->name, "*", metadata->value);
}

int key_valid(char *key)
{
	for( ; *key; key++)
	{
		if(*key >= 'a' && *key <= 'z')
			continue;
		if(*key >= 'A' && *key <= 'Z')
			continue;
		if(*key >= '0' && *key <= '9')
			continue;
		if(*key == '_' || *key == '.' || *key == ':' || *key == '-')
			continue;
		return 0;
	}
	return 1;
}

int check_perms(Client *user, Channel *channel, Client *client, char *key, int mode)
{ /* either user or channel should be NULL */
	Membership *lp;
	if ((user == client) || (!user && !channel)) /* specified target is "*" or own nick */
		return 1;
	if (IsOper(client) && mode == MODE_GET)
		return 1; /* allow ircops to view everything */
	if (channel)
	{
		if ((lp = find_membership_link(client->user->channel, channel)) && ((lp->flags & HOP_OR_MORE) || (mode == MODE_GET)))
			return 1; /* allow setting channel metadata if we're halfop or more, and getting when we're just on this channel */
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
		sendnumeric(client, ERR_KEYNOPERMISSION, user?user->name:channel->name, key);
	return 0;
}

/* METADATA <Target> <Subcommand> [<Param 1> ... [<Param n>]] */
CMD_FUNC(cmd_metadata_local)
{
	Channel *channel = NULL;
	Client *user = NULL;
	
	char buf[1024] = "";
	int i;
	int trylater;
	
	if (metadata_settings.enable_debug)
	{
		for (i=1; i<parc; i++)
		{
			if (!BadPtr(parv[i]))
				strlcat(buf, parv[i], 1024);
			strlcat(buf, " ", 1024);
		}
		sendto_snomask(SNO_JUNK, "Received METADATA, sender %s, params: %s", client->name, buf);
	}
	
	CHECKPARAMSCNT_OR_DIE(2, return);
	char *target = parv[1];
	char *cmd = parv[2];
	char *key;
	int keyindex = 3-1;
	char *value = NULL;
	char *channame;
	
	if (!strcasecmp(cmd, "GET"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		FOR_EACH_KEY()
		{
			if (check_perms(user, channel, client, key, MODE_GET))
			{
				if (!key_valid(key))
				{
					sendnumeric(client, ERR_KEYINVALID, key);
					continue;
				}
				if (channel)
					metadata_send_channel(channel, key, client);
				else
					metadata_send_user(user, key, client);
			}
		}
	} else if (!strcasecmp(cmd, "LIST"))
	{ /* we're just not sending anything if there are no permissions */
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		if (check_perms(user, channel, client, NULL, MODE_GET))
		{
			if (channel)
				metadata_send_all_for_channel(channel, client);
			else
				metadata_send_all_for_user(user, client);
		}
		sendnumeric(client, RPL_METADATAEND);
	} else if (!strcasecmp(cmd, "SET"))
	{
		CHECKPARAMSCNT_OR_DIE(3, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		key = parv[3];
		if (!check_perms(user, channel, client, key, MODE_SET))
			return;
		if (parc > 3 && !BadPtr(parv[4]))
			value = parv[4];

		if (!key_valid(key))
		{
			sendnumeric(client, ERR_KEYINVALID, key);
			return;
		}

		if (channel)
			metadata_set_channel(channel, key, value, client);
		else
			metadata_set_user(user, key, value, client);
	} else if (!strcasecmp(cmd, "CLEAR"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		if (check_perms(user, channel, client, "*", MODE_SET))
		{
			if (channel)
				metadata_clear_channel(channel, client);
			else
				metadata_clear_user(user, client);
		}
		sendnumeric(client, RPL_METADATAEND);
	} else if (!strcasecmp(cmd, "SUB"))
	{
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		trylater = 0;
		FOR_EACH_KEY()
		{
			if(key_valid(key))
			{
				if(metadata_subscribe(key, client, 0))
					trylater = 1;
			} else
			{
				sendnumeric(client, ERR_KEYINVALID, key);
				continue;
			}
		}
		sendnumeric(client, RPL_METADATAEND);
	} else if (!strcasecmp(cmd, "UNSUB"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		CHECKPARAMSCNT_OR_DIE(3, return);
		int subok = 0;
		FOR_EACH_KEY()
		{
			if(key_valid(key))
			{
				metadata_subscribe(key, client, 1);
			} else
			{
				sendnumeric(client, ERR_KEYINVALID, key);
				continue;
			}
		}
		sendnumeric(client, RPL_METADATAEND);
	} else if (!strcasecmp(cmd, "SUBS"))
	{
		CHECKREGISTERED_OR_DIE(client, return);
		metadata_send_subscribtions(client);
		sendnumeric(client, RPL_METADATAEND);
	} else if (!strcasecmp(cmd, "SYNC"))
	{ /* the SYNC command is ignored, as we're using events to send out the queue - only validate the params */
		CHECKREGISTERED_OR_DIE(client, return);
		PROCESS_TARGET_OR_DIE(target, user, channel, return);
	} else
	{
		sendnumeric(client, ERR_METADATAINVALIDSUBCOMMAND, cmd);
	}
}

/* format of S2S is same as the event: ":origin METADATA <client/channel> <key name> *[ :<key value>]" */
CMD_FUNC(cmd_metadata_remote)
{ /* handling data from linked server */
	Channel *channel = NULL;
	Client *user = NULL;
	char *target= parv[1];
	char *key;
	char *value;
	char *channame = strchr(target, '#');
	
	char buf[1024] = "";
	int i;
	if (metadata_settings.enable_debug)
	{
		for (i=1; i<parc; i++)
		{
			if (!BadPtr(parv[i]))
				strlcat(buf, parv[i], 1024);
			strlcat(buf, " ", 1024);
		}
		sendto_snomask(SNO_JUNK, "Received remote METADATA, sender: %s, params: %s", client->name, buf);
	}
	
	if (parc < 5 || BadPtr(parv[4]))
	{
		if (parc == 4 && !BadPtr(parv[3]))
		{
			value = NULL;
		} else
		{
			sendto_snomask(SNO_JUNK, "METADATA not enough args from %s", client->name);
			return;
		}
	} else
	{
		value = parv[4];
	}
	target = parv[1];
	key = parv[2];
	channame = strchr(target, '#');

	if (!*target || !strcmp(target, "*") || !key_valid(key))
	{
		sendto_snomask(SNO_JUNK, "Bad metadata target %s or key %s from %s", target, key, client->name);
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

CMD_FUNC(cmd_metadata)
{
	if (client != &me && MyConnect(client) && !IsServer(client))
		cmd_metadata_local(client, recv_mtags, parc, parv);
	else
		cmd_metadata_remote(client, recv_mtags, parc, parv);
}

int metadata_server_sync(Client *client)
{ /* we send all our data to the server that was just linked */
	Client *acptr;
	struct moddata_user *moddata;
	struct metadata *metadata;
	unsigned int  hashnum;
	Channel *channel;
	
	list_for_each_entry(acptr, &client_list, client_node)
	{ /* send out users (all on our side of the link) */
		moddata = moddata_client(acptr, metadataUser).ptr;
		if(!moddata)
			continue;
		for (metadata = moddata->metadata; metadata; metadata = metadata->next)
			send_change(client, acptr->name, metadata->name, metadata->value, &me);
	}

	for (hashnum = 0; hashnum < CHAN_HASH_TABLE_SIZE; hashnum++)
	{ /* send out channels */
		for(channel = hash_get_chan_bucket(hashnum); channel; channel = channel->hnextch)
		{
			for(metadata = moddata_channel(channel, metadataChannel).ptr; metadata; metadata = metadata->next)
				send_change(client, channel->name, metadata->name, metadata->value, &me);
		}
	}
	return 0;
}

int metadata_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[])
{
	Client *acptr;
	Member *cm;
	char *value;
	struct unsynced *prev_us;
	struct unsynced *us;
	Membership *lp;
	struct subscriptions *subs;
	struct metadata *metadata;

	struct moddata_user *moddata = moddata_client(client, metadataUser).ptr;
	if(!moddata)
		return 0; /* the user is both not subscribed to anything and has no own data */
	for (metadata = moddata->metadata; metadata; metadata = metadata->next)
	{ /* if joining user has metadata, let's notify all subscribers */
		list_for_each_entry(acptr, &lclient_list, lclient_node)
		{
			if(IsMember(acptr, channel) && is_subscribed(acptr, metadata->name))
				notify_or_queue(acptr, client->name, metadata->name, metadata->value, NULL);
		}
	}
	for (subs = moddata->subs; subs; subs = subs->next)
	{
		value = get_channel_key_value(channel, subs->name); /* notify joining user about channel metadata */
		if(value)
			notify_or_queue(client, channel->name, subs->name, value, NULL);
		for (cm = channel->members; cm; cm = cm->next)
		{ /* notify joining user about other channel members' metadata, TODO check if we already see this user elsewhere */
			acptr = cm->client;
			if (acptr == client)
				continue; /* ignore own data */
			value = get_user_key_value(acptr, subs->name);
			if (value)
				notify_or_queue(client, acptr->name, subs->name, value, NULL);
		}
	}
	return 0;
}

void metadata_sync(Client *client)
{
	Client *acptr;
	Channel *channel = NULL;

	struct moddata_user *my_moddata = moddata_client(client, metadataUser).ptr;
	if(!my_moddata)
		return; /* nothing queued */
	struct unsynced *us = my_moddata->us;
	struct unsynced *prev_us;
	
	while (us)
	{
		if (!IsSendable(client))
			break;
		acptr = hash_find_nickatserver(us->name, NULL);
		if (acptr && has_common_channels(acptr, client))
		{ /* if not, the user has vanished since or one of us parted the channel */
			struct moddata_user *moddata = moddata_client(acptr, metadataUser).ptr;
			if (moddata)
			{
				struct metadata *metadata = moddata->metadata;
				while (metadata)
				{
					if (!strcasecmp(us->key, metadata->name))
					{ /* has it */
						char *value = get_user_key_value(acptr, us->key);
						if(value)
							send_change(client, us->name, us->key, value, NULL);
					}
					metadata = metadata->next;
				}
			}
		}
		/* now remove the processed entry */
		prev_us = us;
		us = us->next;
		safe_free(prev_us->name);
		safe_free(prev_us);
		my_moddata->us = us; /* we're always removing the first list item */
	}
}

int metadata_user_registered(Client *client)
{	/*	if we have any metadata set at this point, let's broadcast it to other servers and users */
	struct metadata *metadata;
	struct moddata_user *moddata = moddata_client(client, metadataUser).ptr;
	if(!moddata)
		return HOOK_CONTINUE;
	for (metadata = moddata->metadata; metadata; metadata = metadata->next)
		user_metadata_changed(client, metadata->name, metadata->value, client);
	return HOOK_CONTINUE;
}

EVENT(metadata_queue_evt)
{ /* let's check every 1.5 seconds whether we have something to send */
	Client *acptr;
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{ /* notifications for local subscribers */
		if(!IsUser(acptr)) continue;
		metadata_sync(acptr);
	}
}

