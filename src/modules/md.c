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
	"unrealircd-5",
    };

CMD_FUNC(cmd_md);
void _broadcast_md_client(ModDataInfo *mdi, Client *client, ModData *md);
void _broadcast_md_channel(ModDataInfo *mdi, Channel *chptr, ModData *md);
void _broadcast_md_member(ModDataInfo *mdi, Channel *chptr, Member *m, ModData *md);
void _broadcast_md_membership(ModDataInfo *mdi, Client *client, Membership *m, ModData *md);
void _broadcast_md_globalvar(ModDataInfo *mdi, ModData *md);
void _broadcast_md_client_cmd(Client *except, Client *sender, Client *client, char *varname, char *value);
void _broadcast_md_channel_cmd(Client *except, Client *sender, Channel *chptr, char *varname, char *value);
void _broadcast_md_member_cmd(Client *except, Client *sender, Channel *chptr, Client *client, char *varname, char *value);
void _broadcast_md_membership_cmd(Client *except, Client *sender, Client *client, Channel *chptr, char *varname, char *value);
void _broadcast_md_globalvar_cmd(Client *except, Client *sender, char *varname, char *value);
void _send_moddata_client(Client *srv, Client *client);
void _send_moddata_channel(Client *srv, Channel *chptr);
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
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CLIENT, _send_moddata_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CHANNEL, _send_moddata_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_MEMBERS, _send_moddata_members);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MODDATA_CLIENT, _broadcast_moddata_client);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, "MD", cmd_md, MAXPARA, CMD_SERVER);
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
	char *type, *objname, *varname, *value;
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
		Channel *chptr = find_channel(objname, NULL);
		md = findmoddata_byname(varname, MODDATATYPE_CHANNEL);
		if (!md || !md->unserialize || !chptr)
			return;
		if (value)
			md->unserialize(value, &moddata_channel(chptr, md));
		else
		{
			if (md->free)
				md->free(&moddata_channel(chptr, md));
			memset(&moddata_channel(chptr, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_channel_cmd(client->direction, client, chptr, varname, value);
	} else
	if (!strcmp(type, "member"))
	{
		Client *target;
		Channel *chptr;
		Member *m;
		char *p;

		/* for member the object name is like '#channel/Syzop' */
		p = strchr(objname, ':');
		if (!p)
			return;
		*p++ = '\0';

		chptr = find_channel(objname, NULL);
		if (!chptr)
			return;

		target = find_person(p, NULL);
		if (!target)
			return;

		m = find_member_link(chptr->members, target);
		if (!m)
			return;

		md = findmoddata_byname(varname, MODDATATYPE_MEMBER);
		if (!md || !md->unserialize)
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
		broadcast_md_member_cmd(client->direction, client, chptr, target, varname, value);
	} else
	if (!strcmp(type, "membership"))
	{
		Client *target;
		Channel *chptr;
		Membership *m;
		char *p;

		/* for membership the object name is like 'Syzop/#channel' */
		p = strchr(objname, ':');
		if (!p)
			return;
		*p++ = '\0';

		target = find_person(objname, NULL);
		if (!target)
			return;

		chptr = find_channel(p, NULL);
		if (!chptr)
			return;

		m = find_membership_link(target->user->channel, chptr);
		if (!m)
			return;

		md = findmoddata_byname(varname, MODDATATYPE_MEMBERSHIP);
		if (!md || !md->unserialize)
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
		broadcast_md_membership_cmd(client->direction, client, target, chptr, varname, value);
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

void _broadcast_md_client_cmd(Client *except, Client *sender, Client *client, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s %s :%s",
			sender->name, "client", ID(client), varname, value);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s %s :%s",
			sender->name, "client", client->name, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s %s",
			sender->name, "client", ID(client), varname);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s %s",
			sender->name, "client", client->name, varname);
	}
}

void _broadcast_md_channel_cmd(Client *except, Client *sender, Channel *chptr, char *varname, char *value)
{
	if (value)
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s :%s",
			sender->name, "channel", chptr->chname, varname, value);
	else
		sendto_server(except, 0, 0, NULL, ":%s MD %s %s %s",
			sender->name, "channel", chptr->chname, varname);
}

void _broadcast_md_member_cmd(Client *except, Client *sender, Channel *chptr, Client *client, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s:%s %s :%s",
			sender->name, "member", chptr->chname, ID(client), varname, value);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s:%s %s :%s",
			sender->name, "member", chptr->chname, client->name, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s:%s %s",
			sender->name, "member", chptr->chname, ID(client), varname);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s:%s %s",
			sender->name, "member", chptr->chname, client->name, varname);
	}
}

void _broadcast_md_membership_cmd(Client *except, Client *sender, Client *client, Channel *chptr, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s:%s %s :%s",
			sender->name, "membership", ID(client), chptr->chname, varname, value);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s:%s %s :%s",
			sender->name, "membership", client->name, chptr->chname, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s:%s %s",
			sender->name, "membership", ID(client), chptr->chname, varname);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s:%s %s",
			sender->name, "membership", client->name, chptr->chname, varname);
	}
}

void _broadcast_md_globalvar_cmd(Client *except, Client *sender, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s :%s",
			sender->name, "globalvar", varname, value);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s :%s",
			sender->name, "globalvar", varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, NULL, ":%s MD %s %s",
			sender->name, "globalvar", varname);
		sendto_server(except, 0, PROTO_SID, NULL, ":%s MD %s %s",
			sender->name, "globalvar", varname);
	}
}

/** Send module data update to all servers.
 * @param mdi    Module Data Info structure (which you received from ModDataAdd)
 * @param client The affected client
 * @param md     The ModData. May be NULL for unset.
 */
 
void _broadcast_md_client(ModDataInfo *mdi, Client *client, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_client_cmd(NULL, &me, client, mdi->name, value);
}

void _broadcast_md_channel(ModDataInfo *mdi, Channel *chptr, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_channel_cmd(NULL, &me, chptr, mdi->name, value);
}

void _broadcast_md_member(ModDataInfo *mdi, Channel *chptr, Member *m, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_member_cmd(NULL, &me, chptr, m->client, mdi->name, value);
}

void _broadcast_md_membership(ModDataInfo *mdi, Client *client, Membership *m, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_membership_cmd(NULL, &me, client, m->chptr, mdi->name, value);
}

void _broadcast_md_globalvar(ModDataInfo *mdi, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;

	broadcast_md_globalvar_cmd(NULL, &me, mdi->name, value);
}

/** Send all moddata attached to client 'client' to remote server 'srv' (if the module wants this), called by .. */
void _send_moddata_client(Client *srv, Client *client)
{
	ModDataInfo *mdi;
	char *user = CHECKPROTO(srv, PROTO_SID) ? ID(client) : client->name;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CLIENT) && mdi->sync && mdi->serialize)
		{
			char *value = mdi->serialize(&moddata_client(client, mdi));
			if (value)
				sendto_one(srv, NULL, ":%s MD %s %s %s :%s",
					me.name, "client", user, mdi->name, value);
		}
	}
}

/** Send all moddata attached to channel 'chptr' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_channel(Client *srv, Channel *chptr)
{
	ModDataInfo *mdi;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CHANNEL) && mdi->sync && mdi->serialize)
		{
			char *value = mdi->serialize(&moddata_channel(chptr, mdi));
			if (value)
				sendto_one(srv, NULL, ":%s MD %s %s %s :%s",
					me.name, "channel", chptr->chname, mdi->name, value);
		}
	}
}

/** Send all moddata attached to member & memberships for 'chptr' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_members(Client *srv)
{
	ModDataInfo *mdi;
	Channel *chptr;
	Client *client;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		Member *m;
		for (m = chptr->members; m; m = m->next)
		{
			char *user = CHECKPROTO(srv, PROTO_SID) ? ID(m->client) : m->client->name;

			if (m->client->direction == srv)
				continue; /* from srv's direction */
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBER) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_member(m, mdi));
					if (value)
						sendto_one(srv, NULL, ":%s MD %s %s:%s %s :%s",
							me.name, "member", chptr->chname, user, mdi->name, value);
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
			char *user = CHECKPROTO(srv, PROTO_SID) ? ID(client) : client->name;

			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBERSHIP) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_membership(m, mdi));
					if (value)
						sendto_one(srv, NULL, ":%s MD %s %s:%s %s :%s",
							me.name, "membership", user, m->chptr->chname, mdi->name, value);
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
