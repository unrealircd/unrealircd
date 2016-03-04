/*
 * Example module for ModData usage
 * NEVER LOAD THIS ON A LIVE SERVER!!
 *
 * (C) Copyright 2014 Bram Matthys and the UnrealIRCd team
 * License: GPLv2
 */

#include "unrealircd.h"

CMD_FUNC(m_mdex);

ModuleHeader MOD_HEADER(m_mdex)
  = {
	"m_mdex",
	"4.0",
	"Command /MDEX",
	"3.2-b8-1",
	NULL 
    };

ModDataInfo *mdex_cli = NULL, *mdex_chan = NULL, *mdex_member = NULL, *mdex_membership = NULL;
void mdex_free(ModData *m);
char *mdex_serialize(ModData *m);
void mdex_unserialize(char *str, ModData *m);

MOD_INIT(m_mdex)
{
ModDataInfo mreq;

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "mdex";
	mreq.free = mdex_free;
	mreq.serialize = mdex_serialize;
	mreq.unserialize = mdex_unserialize;
	mreq.sync = 1;
	mreq.type = MODDATATYPE_CLIENT;
	mdex_cli = ModDataAdd(modinfo->handle, mreq);
	if (!mdex_cli)
	        abort();
	mreq.type = MODDATATYPE_CHANNEL;
	mdex_chan = ModDataAdd(modinfo->handle, mreq);
	if (!mdex_cli)
	        abort();
	mreq.type = MODDATATYPE_MEMBER;
	mdex_member = ModDataAdd(modinfo->handle, mreq);
	if (!mdex_cli)
	        abort();
	mreq.type = MODDATATYPE_MEMBERSHIP;
	mdex_membership = ModDataAdd(modinfo->handle, mreq);
	if (!mdex_cli)
	        abort();

	CommandAdd(modinfo->handle, "MDEX", m_mdex, MAXPARA, M_USER);

	return MOD_SUCCESS;
}

MOD_LOAD(m_mdex)
{
	return MOD_SUCCESS;
}


MOD_UNLOAD(m_mdex)
{
	return MOD_SUCCESS;
}

CMD_FUNC(m_mdex)
{
	char *action, *type, *objname, *varname, *value;
	ModDataInfo *md;

	if (!IsOper(sptr) || (parc < 5) || BadPtr(parv[4]))
		return 0;

	action = parv[1]; /* get / set */
	type = parv[2];
	objname = parv[3];
#ifdef DEBUGMODE
	varname = parv[4];
#else
	varname = "mdex";
#endif
	value = parv[5]; /* may be NULL */

	if (!strcmp(action, "set"))
	{
		if (!strcmp(type, "client"))
		{
			aClient *acptr = find_client(objname, NULL);
			md = findmoddata_byname(varname, MODDATATYPE_CLIENT);
			if (!md || !md->unserialize || !md->free || !acptr)
				return 0;
			if (value)
				md->unserialize(value, &moddata_client(acptr, md));
			else
			{
				md->free(&moddata_client(acptr, md));
				memset(&moddata_client(acptr, md), 0, sizeof(ModData));
			}
			broadcast_md_client(md, acptr, &moddata_client(acptr, md));
		} else
		if (!strcmp(type, "channel"))
		{
			aChannel *chptr = find_channel(objname, NULL);
			md = findmoddata_byname(varname, MODDATATYPE_CHANNEL);
			if (!md || !md->unserialize || !md->free || !chptr)
				return 0;
			if (value)
				md->unserialize(value, &moddata_channel(chptr, md));
			else
			{
				md->free(&moddata_channel(chptr, md));
				memset(&moddata_channel(chptr, md), 0, sizeof(ModData));
			}
			broadcast_md_channel(md, chptr, &moddata_channel(chptr, md));
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
			if (!md || !md->unserialize || !md->free)
				return 0;

			if (value)
				md->unserialize(value, &moddata_member(m, md));
			else
			{
				md->free(&moddata_member(m, md));
				memset(&moddata_member(m, md), 0, sizeof(ModData));
			}
			broadcast_md_member(md, chptr, m, &moddata_member(m, md));
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
			if (!md || !md->unserialize || !md->free)
				return 0;

			if (value)
				md->unserialize(value, &moddata_membership(m, md));
			else
			{
				md->free(&moddata_membership(m, md));
				memset(&moddata_membership(m, md), 0, sizeof(ModData));
			}
			broadcast_md_membership(md, acptr, m, &moddata_membership(m, md));
		}
	} else
	if (!strcmp(action, "get"))
	{
		if (!strcmp(type, "client"))
		{
			aClient *acptr = find_client(objname, NULL);
			char *str;
			
			md = findmoddata_byname(varname, MODDATATYPE_CLIENT);
			if (!md || !md->serialize || !acptr)
				return 0;
			str = md->serialize(&moddata_client(acptr, md));
			if (str)
				sendnotice(sptr, "Value: %s", str ? str : "<null>");
			else
				sendnotice(sptr, "No value set");
		} else
		if (!strcmp(type, "channel"))
		{
			aChannel *chptr = find_channel(objname, NULL);
			char *str;
			
			md = findmoddata_byname(varname, MODDATATYPE_CHANNEL);
			if (!md || !md->serialize || !chptr)
				return 0;
			str = md->serialize(&moddata_channel(chptr, md));
			if (str)
				sendnotice(sptr, "Value: %s", str ? str : "<null>");
			else
				sendnotice(sptr, "No value set");
		} else
		if (!strcmp(type, "member"))
		{
			aClient *acptr;
			aChannel *chptr;
			Member *m;
			char *p, *str;
			
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
			if (!md || !md->serialize)
				return 0;

			str = md->serialize(&moddata_member(m, md));
			if (str)
				sendnotice(sptr, "Value: %s", str ? str : "<null>");
			else
				sendnotice(sptr, "No value set");
		} else
		if (!strcmp(type, "membership"))
		{
			aClient *acptr;
			aChannel *chptr;
			Membership *m;
			char *p, *str;
			
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
			if (!md || !md->serialize)
				return 0;

			str = md->serialize(&moddata_membership(m, md));
			if (str)
				sendnotice(sptr, "Value: %s", str ? str : "<null>");
			else
				sendnotice(sptr, "No value set");
		}
	}
	
	return 0;
}

void mdex_free(ModData *m)
{
	if (m->str)
		MyFree(m->str);
}

char *mdex_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void mdex_unserialize(char *str, ModData *m)
{
	if (m->str)
		MyFree(m->str);
	m->str = strdup(str);
}
