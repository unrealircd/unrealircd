/*
 * Module Data module (command MD)
 * (C) Copyright 2014 Bram Matthys and The UnrealIRCd Team
 *
 * This is used to synch. module data over the network
 * The send_md_* functions are used by SJOIN and modules can also call these
 * functions themselves when they alter a value and want to broadcast it.
 */
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_md(aClient *cptr, aClient *sptr, int parc, char *parv[]);
void _send_md_client(ModDataInfo *mdi, aClient *acptr, ModData *md);
void _send_md_channel(ModDataInfo *mdi, aChannel *chptr, ModData *md);
void _send_md_member(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md);
void _send_md_membership(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md);

ModuleHeader MOD_HEADER(m_md)
  = {
	"m_md",
	"$Id$",
	"command /MD (S2S only)",
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_TEST(m_md)(ModuleInfo *modinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MD_CLIENT, _send_md_client);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MD_CHANNEL, _send_md_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MD_MEMBER, _send_md_member);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_MD_MEMBERSHIP, _send_md_membership);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(m_md)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, "MD", m_md, MAXPARA, M_SERVER);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_md)(int module_load)
{
	return MOD_SUCCESS;
}


DLLFUNC int MOD_UNLOAD(m_md)(int module_unload)
{
	return MOD_SUCCESS;
}

/** Set ModData command.
 *  Syntax: MD <type> <object name> <variable name> <value>
 * Example: MD client Syzop sslfp 123456789
 *
 * If <value> is ommitted, the variable is unset & freed.
 *
 * The appropriate module is called to set the data (unserialize).
 * When the command is received it is broadcasted further to all other servers.
 */
DLLFUNC int m_md(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
char *type, *objname, *varname, *value;
ModDataInfo *md;

	if (!IsServer(sptr) || (parc < 4) || BadPtr(parv[3]))
		return 0;
	
	type = parv[1];
	objname = parv[2];
	varname = parv[3];
	value = parv[4]; /* may be NULL */
	
	/* Pass on to other servers */
	if (value)
		sendto_server(cptr, 0, 0, ":%s MD %s %s %s :%s",
			sptr->name, type, objname, varname, value);
	else
		sendto_server(cptr, 0, 0, ":%s MD %s %s %s",
			sptr->name, type, objname, varname);

	/* Now interpret it ourselves.. */
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
	}
	return 0;
}

/** Send module data update to all servers.
 * @param mdi   Module Data Info structure (which you received from ModDataAdd)
 * @param acptr The affected client
 * @param md    The ModData. May be NULL for unset.
 */
 
void _send_md_client(ModDataInfo *mdi, aClient *acptr, ModData *md)
{
char *value = md ? mdi->serialize(md) : NULL;
	
	if (value)
		sendto_server(NULL, 0, 0, ":%s MD %s %s %s :%s",
			me.client.name, "client", acptr->name, mdi->name, value);
	else
		sendto_server(NULL, 0, 0, ":%s MD %s %s %s",
			me.client.name, "client", acptr->name, mdi->name);
}

void _send_md_channel(ModDataInfo *mdi, aChannel *chptr, ModData *md)
{
char *value = md ? mdi->serialize(md) : NULL;
	
	if (value)
		sendto_server(NULL, 0, 0, ":%s MD %s %s %s :%s",
			me.client.name, "channel", chptr->chname, mdi->name, value);
	else
		sendto_server(NULL, 0, 0, ":%s MD %s %s %s",
			me.client.name, "channel", chptr->chname, mdi->name);
}

void _send_md_member(ModDataInfo *mdi, aChannel *chptr, Member *m, ModData *md)
{
char *value = md ? mdi->serialize(md) : NULL;
	
	if (value)
		sendto_server(NULL, 0, 0, ":%s MD %s %s:%s %s :%s",
			me.client.name, "member", chptr->chname, m->cptr->name, mdi->name, value);
	else
		sendto_server(NULL, 0, 0, ":%s MD %s %s:%s %s",
			me.client.name, "member", chptr->chname, m->cptr->name, mdi->name);
}

void _send_md_membership(ModDataInfo *mdi, aClient *acptr, Membership *m, ModData *md)
{
char *value = md ? mdi->serialize(md) : NULL;
	
	if (value)
		sendto_server(NULL, 0, 0, ":%s MD %s %s:%s %s :%s",
			me.client.name, "membership", acptr->name, m->chptr->chname, mdi->name, value);
	else
		sendto_server(NULL, 0, 0, ":%s MD %s %s:%s %s",
			me.client.name, "membership", acptr->name, m->chptr->chname, mdi->name);
}
