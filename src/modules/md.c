/*
 * Module Data module (command MD)
 * (C) Copyright 2014-.. Bram Matthys and The UnrealIRCd Team
 *
 * This file contains all commands that deal with sending and
 * receiving module data over the network.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"md",
	"5.0",
	"command /MD (S2S only)",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

CMD_FUNC(cmd_md);
void _broadcast_md_client(ModDataInfo *mdi, Client *client, ModData *md);
void _broadcast_md_channel(ModDataInfo *mdi, Channel *channel, ModData *md);
void _broadcast_md_member(ModDataInfo *mdi, Channel *channel, Member *m, ModData *md);
void _broadcast_md_membership(ModDataInfo *mdi, Client *client, Membership *m, ModData *md);
void _broadcast_md_globalvar(ModDataInfo *mdi, ModData *md);
void _broadcast_md_client_cmd(Client *except, Client *sender, Client *client, const char *varname, const char *value);
void _broadcast_md_channel_cmd(Client *except, Client *sender, Channel *channel, const char *varname, const char *value);
void _broadcast_md_member_cmd(Client *except, Client *sender, Channel *channel, Client *client, const char *varname, const char *value);
void _broadcast_md_membership_cmd(Client *except, Client *sender, Client *client, Channel *channel, const char *varname, const char *value);
void _broadcast_md_globalvar_cmd(Client *except, Client *sender, const char *varname, const char *value);
void _moddata_add_s2s_mtags(Client *client, MessageTag **mtags);
void _moddata_extract_s2s_mtags(Client *client, MessageTag *mtags);
void _send_moddata_client(Client *srv, Client *client);
void _send_moddata_channel(Client *srv, Channel *channel);
void _send_moddata_members(Client *srv);
void _broadcast_moddata_client(Client *client);

extern MODVAR ModDataInfo *MDInfo;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CLIENT, _broadcast_md_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CHANNEL, _broadcast_md_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBER, _broadcast_md_member);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBERSHIP, _broadcast_md_membership);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_GLOBALVAR, _broadcast_md_globalvar);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CLIENT_CMD, _broadcast_md_client_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CHANNEL_CMD, _broadcast_md_channel_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBER_CMD, _broadcast_md_member_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBERSHIP_CMD, _broadcast_md_membership_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_GLOBALVAR_CMD, _broadcast_md_globalvar_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_MODDATA_ADD_S2S_MTAGS, _moddata_add_s2s_mtags);
	EfunctionAddVoid(modinfo->handle, EFUNC_MODDATA_EXTRACT_S2S_MTAGS, _moddata_extract_s2s_mtags);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CLIENT, _send_moddata_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CHANNEL, _send_moddata_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_MEMBERS, _send_moddata_members);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MODDATA_CLIENT, _broadcast_moddata_client);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, "MD", cmd_md, MAXPARA, CMD_SERVER|CMD_BIGLINES);
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

/** Check if client may write to this MD object */
int md_access_check(Client *client, ModDataInfo *md, Client *target)
{
	if ((client == target) && md->self_write)
		return 1;

	if (MyConnect(target) && !md->remote_write)
	{
		unreal_log(ULOG_WARNING, "md", "REMOTE_MD_WRITE_DENIED", client,
		           "Remote server $client tried to write moddata $moddata_name "
		           "of a client from ours ($target.name) -- attempt BLOCKED",
		           log_data_string("moddata_name", md->name),
		           log_data_client("target", target));
		return 0;
	}

	return 1;
}

/** Set ModData command.
 *  Syntax: MD <type> <object name> <variable name> <value>
 * Example: MD client Syzop sslfp 123456789
 *
 * If <value> is ommitted, the variable is unset & freed.
 *
 * The appropriate module is called to set the data (unserialize) and
 * then the command is broadcasted to all other servers.
 *
 * Technical documentation (if writing services) is available at:
 * https://www.unrealircd.org/docs/Server_protocol:MD_command
 * Module API documentation (if writing an UnrealIRCd module):
 * https://www.unrealircd.org/docs/Dev:Module_Storage
 */
CMD_FUNC(cmd_md)
{
	const char *type, *objname, *varname, *value;
	ModDataInfo *md;

	if (!IsServer(client) || (parc < 4) || BadPtr(parv[3]))
		return;

	type = parv[1];
	objname = parv[2];
	varname = parv[3];
	value = parv[4]; /* may be NULL */

	if (!strcmp(type, "client"))
	{
		Client *target = find_client(objname, NULL);
		md = findmoddata_byname(varname, MODDATATYPE_CLIENT);
		if (!md || !md->unserialize || !target)
			return;

		if (!md_access_check(client, md, target))
			return;

		if (value)
			md->unserialize(value, &moddata_client(target, md));
		else
		{
			if (md->free)
				md->free(&moddata_client(target, md));
			memset(&moddata_client(target, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_client_cmd(client->direction, client, target, varname, value);
	} else
	if (!strcmp(type, "channel"))
	{
		Channel *channel = find_channel(objname);
		md = findmoddata_byname(varname, MODDATATYPE_CHANNEL);
		if (!md || !md->unserialize || !channel)
			return;
		if (value)
			md->unserialize(value, &moddata_channel(channel, md));
		else
		{
			if (md->free)
				md->free(&moddata_channel(channel, md));
			memset(&moddata_channel(channel, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_channel_cmd(client->direction, client, channel, varname, value);
	} else
	if (!strcmp(type, "member"))
	{
		Client *target;
		Channel *channel;
		Member *m;
		char *p;

		/* for member the object name is like '#channel/Syzop' */
		p = strchr(objname, ':');
		if (!p)
			return;
		*p++ = '\0';

		channel = find_channel(objname);
		if (!channel)
			return;

		target = find_user(p, NULL);
		if (!target)
			return;

		m = find_member_link(channel->members, target);
		if (!m)
			return;

		md = findmoddata_byname(varname, MODDATATYPE_MEMBER);
		if (!md || !md->unserialize)
			return;

		if (!md_access_check(client, md, target))
			return;

		if (value)
			md->unserialize(value, &moddata_member(m, md));
		else
		{
			if (md->free)
				md->free(&moddata_member(m, md));
			memset(&moddata_member(m, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_member_cmd(client->direction, client, channel, target, varname, value);
	} else
	if (!strcmp(type, "membership"))
	{
		Client *target;
		Channel *channel;
		Membership *m;
		char *p;

		/* for membership the object name is like 'Syzop/#channel' */
		p = strchr(objname, ':');
		if (!p)
			return;
		*p++ = '\0';

		target = find_user(objname, NULL);
		if (!target)
			return;

		channel = find_channel(p);
		if (!channel)
			return;

		m = find_membership_link(target->user->channel, channel);
		if (!m)
			return;

		md = findmoddata_byname(varname, MODDATATYPE_MEMBERSHIP);
		if (!md || !md->unserialize)
			return;

		if (!md_access_check(client, md, target))
			return;

		if (value)
			md->unserialize(value, &moddata_membership(m, md));
		else
		{
			if (md->free)
				md->free(&moddata_membership(m, md));
			memset(&moddata_membership(m, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_membership_cmd(client->direction, client, target, channel, varname, value);
	} else
	if (!strcmp(type, "globalvar"))
	{
		/* objname is ignored */
		md = findmoddata_byname(varname, MODDATATYPE_GLOBAL_VARIABLE);
		if (!md || !md->unserialize)
			return;
		if (value)
			md->unserialize(value, &moddata_global_variable(md));
		else
		{
			if (md->free)
				md->free(&moddata_global_variable(md));
			memset(&moddata_global_variable(md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_globalvar_cmd(client->direction, client, varname, value);
	}
}

void _broadcast_md_client_cmd(Client *except, Client *sender, Client *client, const char *varname, const char *value)
{
	if (value)
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s :%s",
			sender->id, "client", client->id, varname, value);
	}
	else
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s",
			sender->id, "client", client->id, varname);
	}
}

void _broadcast_md_channel_cmd(Client *except, Client *sender, Channel *channel, const char *varname, const char *value)
{
	if (value)
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s :%s",
			sender->id, "channel", channel->name, varname, value);
	else
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s",
			sender->id, "channel", channel->name, varname);
}

void _broadcast_md_member_cmd(Client *except, Client *sender, Channel *channel, Client *client, const char *varname, const char *value)
{
	if (value)
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s:%s %s :%s",
			sender->id, "member", channel->name, client->id, varname, value);
	}
	else
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s:%s %s",
			sender->id, "member", channel->name, client->id, varname);
	}
}

void _broadcast_md_membership_cmd(Client *except, Client *sender, Client *client, Channel *channel, const char *varname, const char *value)
{
	if (value)
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s:%s %s :%s",
			sender->id, "membership", client->id, channel->name, varname, value);
	}
	else
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s:%s %s",
			sender->id, "membership", client->id, channel->name, varname);
	}
}

void _broadcast_md_globalvar_cmd(Client *except, Client *sender, const char *varname, const char *value)
{
	if (value)
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s :%s",
			sender->id, "globalvar", varname, value);
	}
	else
	{
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s",
			sender->id, "globalvar", varname);
	}
}

/** Send module data update to all servers.
 * @param mdi    Module Data Info structure (which you received from ModDataAdd)
 * @param client The affected client
 * @param md     The ModData. May be NULL for unset.
 */
 
void _broadcast_md_client(ModDataInfo *mdi, Client *client, ModData *md)
{
	const char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_client_cmd(NULL, &me, client, mdi->name, value);
}

void _broadcast_md_channel(ModDataInfo *mdi, Channel *channel, ModData *md)
{
	const char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_channel_cmd(NULL, &me, channel, mdi->name, value);
}

void _broadcast_md_member(ModDataInfo *mdi, Channel *channel, Member *m, ModData *md)
{
	const char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_member_cmd(NULL, &me, channel, m->client, mdi->name, value);
}

void _broadcast_md_membership(ModDataInfo *mdi, Client *client, Membership *m, ModData *md)
{
	const char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_membership_cmd(NULL, &me, client, m->channel, mdi->name, value);
}

void _broadcast_md_globalvar(ModDataInfo *mdi, ModData *md)
{
	const char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_globalvar_cmd(NULL, &me, mdi->name, value);
}

/** Send all moddata attached to client 'client' to remote server 'srv' (if the module wants this), called by .. */
void _send_moddata_client(Client *srv, Client *client)
{
	ModDataInfo *mdi;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CLIENT) && mdi->sync && mdi->serialize)
		{
			const char *value = mdi->serialize(&moddata_client(client, mdi));
			if (value)
				sendto_one(srv, NULL, ":%s MD %s %s %s :%s",
					me.id, "client", client->id, mdi->name, value);
		}
	}
}

/** Enhance the command with moddata message tags, so we can send
 * traffic like @s2s-md/certfp=xxxxx UID ....
 */
void _moddata_add_s2s_mtags(Client *client, MessageTag **mtags_list)
{
	ModDataInfo *mdi;
	char name[128];

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CLIENT) && (mdi->sync == MODDATA_SYNC_EARLY) && mdi->serialize)
		{
			MessageTag *m;
			const char *value = mdi->serialize(&moddata_client(client, mdi));
			if (!value)
				continue;
			snprintf(name, sizeof(name), "s2s-md/%s", mdi->name);

			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, name);
			safe_strdup(m->value, value);
			AddListItem(m, *mtags_list);
		}
	}
}

/** Extract the s2s-md/<moddataname> tags again from an incoming command,
 * eg @s2s-md/certfp=xxxxx UID ....
 */
void _moddata_extract_s2s_mtags(Client *client, MessageTag *mtags)
{
	MessageTag *m;
	ModDataInfo *md;

	for (m = mtags; m; m = m->next)
	{
		if (!strncmp(m->name, "s2s-md/", 7))
		{
			char *varname = m->name + 7;
			char *value = m->value;

			if (!value)
				continue;

			md = findmoddata_byname(varname, MODDATATYPE_CLIENT);
			if (!md || !md->unserialize)
				continue;

			if (!md_access_check(client, md, client))
				return;

			md->unserialize(value, &moddata_client(client, md));
		}
	}
}

/** Send all moddata attached to channel 'channel' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_channel(Client *srv, Channel *channel)
{
	ModDataInfo *mdi;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CHANNEL) && mdi->sync && mdi->serialize)
		{
			const char *value = mdi->serialize(&moddata_channel(channel, mdi));
			if (value)
				sendto_one(srv, NULL, ":%s MD %s %s %s :%s",
					me.id, "channel", channel->name, mdi->name, value);
		}
	}
}

/** Send all moddata attached to member & memberships for 'channel' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_members(Client *srv)
{
	ModDataInfo *mdi;
	Channel *channel;
	Client *client;

	for (channel = channels; channel; channel = channel->nextch)
	{
		Member *m;
		for (m = channel->members; m; m = m->next)
		{
			client = m->client;
			if (client->direction == srv)
				continue; /* from srv's direction */
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBER) && mdi->sync && mdi->serialize)
				{
					const char *value = mdi->serialize(&moddata_member(m, mdi));
					if (value)
						sendto_one(srv, NULL, ":%s MD %s %s:%s %s :%s",
							me.id, "member", channel->name, client->id, mdi->name, value);
				}
			}
		}
	}

	list_for_each_entry(client, &client_list, client_node)
	{
		Membership *m;
		if (!IsUser(client) || !client->user)
			continue;

		if (client->direction == srv)
			continue; /* from srv's direction */

		for (m = client->user->channel; m; m = m->next)
		{
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBERSHIP) && mdi->sync && mdi->serialize)
				{
					const char *value = mdi->serialize(&moddata_membership(m, mdi));
					if (value)
						sendto_one(srv, NULL, ":%s MD %s %s:%s %s :%s",
							me.id, "membership", client->id, m->channel->name, mdi->name, value);
				}
			}
		}
	}
}

/** Broadcast moddata attached to client 'client' to all servers. */
void _broadcast_moddata_client(Client *client)
{
	Client *acptr;

	list_for_each_entry(acptr, &server_list, special_node)
	{
		send_moddata_client(acptr, client);
	}
}
