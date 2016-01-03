/*
 * Module Data module (command MD)
 * (C) Copyright 2014-.. Bram Matthys and The UnrealIRCd Team
 *
 * This file contains all commands that deal with sending and
 * receiving module data over the network.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER(m_md)
  = {
	"m_md",
	"4.0",
	"command /MD (S2S only)",
	"3.2-b8-1",
	NULL 
    };

CMD_FUNC(m_md);
void _broadcast_md_client(ModDataInfo *mdi, aClient *acptr, ModData *md);
void _broadcast_md_channel(ModDataInfo *mdi, aChannel *chptr, ModData *md);
void _broadcast_md_member(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md);
void _broadcast_md_membership(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md);
void _broadcast_md_client_cmd(aClient *except, aClient *sender, aClient *acptr, char *varname, char *value);
void _broadcast_md_channel_cmd(aClient *except, aClient *sender, aChannel *chptr, char *varname, char *value);
void _broadcast_md_member_cmd(aClient *except, aClient *sender, aChannel *chptr, aClient *acptr, char *varname, char *value);
void _broadcast_md_membership_cmd(aClient *except, aClient *sender, aClient *acptr, aChannel *chptr, char *varname, char *value);
void _send_moddata_client(aClient *srv, aClient *acptr);
void _send_moddata_channel(aClient *srv, aChannel *chptr);
void _send_moddata_members(aClient *srv);
void _broadcast_moddata_client(aClient *acptr);

extern MODVAR ModDataInfo *MDInfo;

MOD_TEST(m_md)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CLIENT, _broadcast_md_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CHANNEL, _broadcast_md_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBER, _broadcast_md_member);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBERSHIP, _broadcast_md_membership);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CLIENT_CMD, _broadcast_md_client_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_CHANNEL_CMD, _broadcast_md_channel_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBER_CMD, _broadcast_md_member_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MD_MEMBERSHIP_CMD, _broadcast_md_membership_cmd);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CLIENT, _send_moddata_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_CHANNEL, _send_moddata_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MODDATA_MEMBERS, _send_moddata_members);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_MODDATA_CLIENT, _broadcast_moddata_client);
	return MOD_SUCCESS;
}

MOD_INIT(m_md)
{
	CommandAdd(modinfo->handle, "MD", m_md, MAXPARA, M_SERVER);
	return MOD_SUCCESS;
}

MOD_LOAD(m_md)
{
	return MOD_SUCCESS;
}


MOD_UNLOAD(m_md)
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
 */
CMD_FUNC(m_md)
{
	char *type, *objname, *varname, *value;
	ModDataInfo *md;

	if (!IsServer(sptr) || (parc < 4) || BadPtr(parv[3]))
		return 0;
	
	type = parv[1];
	objname = parv[2];
	varname = parv[3];
	value = parv[4]; /* may be NULL */
	
	if (!strcmp(type, "client"))
	{
		aClient *acptr = find_client(objname, NULL);
		md = findmoddata_byname(varname, MODDATATYPE_CLIENT);
		if (!md || !md->unserialize || !acptr)
			return 0;
		if (value)
			md->unserialize(value, &moddata_client(acptr, md));
		else
		{
			if (md->free)
				md->free(&moddata_client(acptr, md));
			memset(&moddata_client(acptr, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_client_cmd(cptr, sptr, acptr, varname, value);
	} else
	if (!strcmp(type, "channel"))
	{
		aChannel *chptr = find_channel(objname, NULL);
		md = findmoddata_byname(varname, MODDATATYPE_CHANNEL);
		if (!md || !md->unserialize || !chptr)
			return 0;
		if (value)
			md->unserialize(value, &moddata_channel(chptr, md));
		else
		{
			if (md->free)
				md->free(&moddata_channel(chptr, md));
			memset(&moddata_channel(chptr, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_channel_cmd(cptr, sptr, chptr, varname, value);
	} else
	if (!strcmp(type, "member"))
	{
		aClient *acptr;
		aChannel *chptr;
		Member *m;
		char *p;
		
		/* for member the object name is like '#channel/Syzop' */
		p = strchr(objname, ':');
		if (!p)
			return 0;
		*p++ = '\0';

		chptr = find_channel(objname, NULL);
		if (!chptr)
			return 0;
		
		acptr = find_person(p, NULL);
		if (!acptr)
			return 0;

		m = find_member_link(chptr->members, acptr);
		if (!m)
			return 0;
		
		md = findmoddata_byname(varname, MODDATATYPE_MEMBER);
		if (!md || !md->unserialize)
			return 0;

		if (value)
			md->unserialize(value, &moddata_member(m, md));
		else
		{
			if (md->free)
				md->free(&moddata_member(m, md));
			memset(&moddata_member(m, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_member_cmd(cptr, sptr, chptr, acptr, varname, value);
	} else
	if (!strcmp(type, "membership"))
	{
		aClient *acptr;
		aChannel *chptr;
		Membership *m;
		char *p;
		
		/* for membership the object name is like 'Syzop/#channel' */
		p = strchr(objname, ':');
		if (!p)
			return 0;
		*p++ = '\0';

		acptr = find_person(objname, NULL);
		if (!acptr)
			return 0;
		
		chptr = find_channel(p, NULL);
		if (!chptr)
			return 0;

		m = find_membership_link(acptr->user->channel, chptr);
		if (!m)
			return 0;
		
		md = findmoddata_byname(varname, MODDATATYPE_MEMBERSHIP);
		if (!md || !md->unserialize)
			return 0;

		if (value)
			md->unserialize(value, &moddata_membership(m, md));
		else
		{
			if (md->free)
				md->free(&moddata_membership(m, md));
			memset(&moddata_membership(m, md), 0, sizeof(ModData));
		}
		/* Pass on to other servers */
		broadcast_md_membership_cmd(cptr, sptr, acptr, chptr, varname, value);
	}
	return 0;
}

void _broadcast_md_client_cmd(aClient *except, aClient *sender, aClient *acptr, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s %s :%s",
			sender->name, "client", ID(acptr), varname, value);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s %s :%s",
			sender->name, "client", acptr->name, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s %s",
			sender->name, "client", ID(acptr), varname);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s %s",
			sender->name, "client", acptr->name, varname);
	}
}

void _broadcast_md_channel_cmd(aClient *except, aClient *sender, aChannel *chptr, char *varname, char *value)
{
	if (value)
		sendto_server(except, 0, 0, ":%s MD %s %s %s :%s",
			sender->name, "channel", chptr->chname, varname, value);
	else
		sendto_server(except, 0, 0, ":%s MD %s %s %s",
			sender->name, "channel", chptr->chname, varname);
}

void _broadcast_md_member_cmd(aClient *except, aClient *sender, aChannel *chptr, aClient *acptr, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s:%s %s :%s",
			sender->name, "member", chptr->chname, ID(acptr), varname, value);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s:%s %s :%s",
			sender->name, "member", chptr->chname, acptr->name, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s:%s %s",
			sender->name, "member", chptr->chname, ID(acptr), varname);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s:%s %s",
			sender->name, "member", chptr->chname, acptr->name, varname);
	}
}

void _broadcast_md_membership_cmd(aClient *except, aClient *sender, aClient *acptr, aChannel *chptr, char *varname, char *value)
{
	if (value)
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s:%s %s :%s",
			sender->name, "membership", ID(acptr), chptr->chname, varname, value);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s:%s %s :%s",
			sender->name, "membership", acptr->name, chptr->chname, varname, value);
	}
	else
	{
		sendto_server(except, PROTO_SID, 0, ":%s MD %s %s:%s %s",
			sender->name, "membership", ID(acptr), chptr->chname, varname);
		sendto_server(except, 0, PROTO_SID, ":%s MD %s %s:%s %s",
			sender->name, "membership", acptr->name, chptr->chname, varname);
	}
}

/** Send module data update to all servers.
 * @param mdi   Module Data Info structure (which you received from ModDataAdd)
 * @param acptr The affected client
 * @param md    The ModData. May be NULL for unset.
 */
 
void _broadcast_md_client(ModDataInfo *mdi, aClient *acptr, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;
	
	broadcast_md_client_cmd(NULL, &me, acptr, mdi->name, value);
}

void _broadcast_md_channel(ModDataInfo *mdi, aChannel *chptr, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;
	
	broadcast_md_channel_cmd(NULL, &me, chptr, mdi->name, value);
}

void _broadcast_md_member(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;
	
	broadcast_md_member_cmd(NULL, &me, chptr, m->cptr, mdi->name, value);
}

void _broadcast_md_membership(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md)
{
	char *value = md ? mdi->serialize(md) : NULL;
	
	broadcast_md_membership_cmd(NULL, &me, acptr, m->chptr, mdi->name, value);
}

/** Send all moddata attached to client 'acptr' to remote server 'srv' (if the module wants this), called by .. */
void _send_moddata_client(aClient *srv, aClient *acptr)
{
	ModDataInfo *mdi;
	char *user = CHECKPROTO(srv, PROTO_SID) ? ID(acptr) : acptr->name;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CLIENT) && mdi->sync && mdi->serialize)
		{
			char *value = mdi->serialize(&moddata_client(acptr, mdi));
			if (value)
				sendto_one(srv, ":%s MD %s %s %s :%s",
					me.name, "client", user, mdi->name, value);
		}
	}
}

/** Send all moddata attached to channel 'chptr' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_channel(aClient *srv, aChannel *chptr)
{
	ModDataInfo *mdi;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CHANNEL) && mdi->sync && mdi->serialize)
		{
			char *value = mdi->serialize(&moddata_channel(chptr, mdi));
			if (value)
				sendto_one(srv, ":%s MD %s %s %s :%s",
					me.name, "channel", chptr->chname, mdi->name, value);
		}
	}
}

/** Send all moddata attached to member & memberships for 'chptr' to remote server 'srv' (if the module wants this), called by SJOIN */
void _send_moddata_members(aClient *srv)
{
	ModDataInfo *mdi;
	aChannel *chptr;
	aClient *acptr;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		Member *m;
		for (m = chptr->members; m; m = m->next)
		{
			char *user = CHECKPROTO(srv, PROTO_SID) ? ID(m->cptr) : m->cptr->name;
			
			if (m->cptr->from == srv)
				continue; /* from srv's direction */
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBER) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_member(m, mdi));
					if (value)
						sendto_one(srv, ":%s MD %s %s:%s %s :%s",
							me.name, "member", chptr->chname, user, mdi->name, value);
				}
			}
		}
	}
	
	list_for_each_entry(acptr, &client_list, client_node)
	{
		Membership *m;
		if (!IsPerson(acptr) || !acptr->user)
			continue;

		if (acptr->from == srv)
			continue; /* from srv's direction */

		for (m = acptr->user->channel; m; m = m->next)
		{
			char *user = CHECKPROTO(srv, PROTO_SID) ? ID(acptr) : acptr->name;

			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBERSHIP) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_membership(m, mdi));
					if (value)
						sendto_one(srv, ":%s MD %s %s:%s %s :%s",
							me.name, "membership", user, m->chptr->chname, mdi->name, value);
				}
			}
		}
	}
}

/** Broadcast moddata attached to client 'acptr' to all servers. */
void _broadcast_moddata_client(aClient *acptr)
{
	aClient *cptr;

	list_for_each_entry(cptr, &server_list, special_node)
	{
		send_moddata_client(cptr, acptr);
	}
}
