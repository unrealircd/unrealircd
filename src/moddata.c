/************************************************************************
 *   IRC - Internet Relay Chat, moddata.c
 *   (C) 2003-2014 Bram Matthys (Syzop) and the UnrealIRCd Team
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"
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

#ifndef MAX
#define MAX(x,y)   ((x) > (y) ? (x) : (y))
#endif

ModDataInfo *MDInfo = NULL;

void moddata_init(void)
{
	/* all zero already? */
}

ModDataInfo *ModDataAdd(Module *module, ModDataInfo req)
{
	short i = 0, j = 0;
	int paraslot = -1;
	char tmpbuf[512];
	int slotav = 0; /* highest available slot */
	ModDataInfo *m;
	int new_struct = 0;
	
	/* Hunt for highest available slot */
	for (m = MDInfo; m ; m = m->next)
		if (m->type == req.type)
		{
			/* Does an entry already exist with this name? */
			if (!strcmp(m->name, req.name))
			{
				/* If old module is unloading (so reloading), then OK to take this slot */
				if (m->unloaded)
				{
					slotav = m->slot;
					m->unloaded = 0;
					goto moddataadd_isok;
				}
				/* Otherwise, name collision */
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
			/* Update next available slot */
			slotav = MAX(slotav, m->slot+1);
		}

	/* Now check if we are within bounds (if we really have a free slot available) */
	if (((req.type == MODDATATYPE_CLIENT) && (slotav >= MODDATA_MAX_CLIENT)) ||
	    ((req.type == MODDATATYPE_CHANNEL) && (slotav >= MODDATA_MAX_CHANNEL)) ||
	    ((req.type == MODDATATYPE_MEMBER) && (slotav >= MODDATA_MAX_MEMBER)) ||
	    ((req.type == MODDATATYPE_MEMBERSHIP) && (slotav >= MODDATA_MAX_MEMBERSHIP)))
	{
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}

	new_struct = 1;
	m = MyMallocEx(sizeof(ModDataInfo));
	m->name = strdup(req.name);
	m->slot = slotav;
	m->type = req.type;
moddataadd_isok:
	m->free = req.free;
	m->serialize = req.serialize;
	m->unserialize = req.unserialize;
	m->sync = req.sync;
	m->owner = module;
	
	if (new_struct)
		AddListItem(m, MDInfo);

	if (module)
	{
		ModuleObject *mobj = MyMallocEx(sizeof(ModuleObject));
		mobj->object.moddata = m;
		mobj->type = MOBJ_MODDATA;
		AddListItem(mobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	
	return m;
}

void moddata_free_client(aClient *acptr)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_CLIENT)
		{
			if (md->free && moddata_client(acptr, md).ptr)
				md->free(&moddata_client(acptr, md));
		}

	memset(acptr->moddata, 0, sizeof(acptr->moddata));
}

void moddata_free_channel(aChannel *chptr)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_CHANNEL)
		{
			if (md->free && moddata_channel(chptr, md).ptr)
				md->free(&moddata_channel(chptr, md));
		}

	memset(chptr->moddata, 0, sizeof(chptr->moddata));
}

void moddata_free_member(Member *m)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_MEMBER)
		{
			if (md->free && moddata_member(m, md).ptr)
				md->free(&moddata_member(m, md));
		}

	memset(m->moddata, 0, sizeof(m->moddata));
}

void moddata_free_membership(Membership *m)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_MEMBERSHIP)
		{
			if (md->free && moddata_membership(m, md).ptr)
				md->free(&moddata_membership(m, md));
		}

	memset(m->moddata, 0, sizeof(m->moddata));
}

/** Actually free all the ModData from all objects */
void unload_moddata_commit(ModDataInfo *md)
{
	switch(md->type)
	{
		case MODDATATYPE_CLIENT:
		{
			aClient *acptr;
			list_for_each_entry(acptr, &lclient_list, lclient_node)
			{
				if (md->free && moddata_client(acptr, md).ptr)
					md->free(moddata_client(acptr, md).ptr);
				memset(&moddata_client(acptr, md), 0, sizeof(ModData));
			}
			break;
		}
		case MODDATATYPE_CHANNEL:
		{
			aChannel *chptr;
			for (chptr = channel; chptr; chptr=chptr->nextch)
			{
				if (md->free && moddata_channel(chptr, md).ptr)
					md->free(moddata_channel(chptr, md).ptr);
				memset(&moddata_channel(chptr, md), 0, sizeof(ModData));
			}
			break;
		}
		case MODDATATYPE_MEMBER:
		{
			aChannel *chptr;
			Member *m;
			for (chptr = channel; chptr; chptr=chptr->nextch)
			{
				for (m = chptr->members; m; m = m->next)
				{
					if (md->free && moddata_member(m, md).ptr)
						md->free(moddata_member(m, md).ptr);
					memset(&moddata_member(m, md), 0, sizeof(ModData));
				}
			}
			break;
		}
		case MODDATATYPE_MEMBERSHIP:
		{
			aClient *acptr;
			Membership *m;
			list_for_each_entry(acptr, &lclient_list, lclient_node)
			{
				if (!acptr->user)
					continue;
				for (m = acptr->user->channel; m; m = m->next)
				{
					if (md->free && moddata_membership(m, md).ptr)
						md->free(moddata_membership(m, md).ptr);
					memset(&moddata_membership(m, md), 0, sizeof(ModData));
				}
			}
			break;
		}
	}
	
	DelListItem(md, MDInfo);
	MyFree(md->name);
	MyFree(MDInfo);
}

void ModDataDel(ModDataInfo *md)
{

	if (md->owner)
	{
		ModuleObject *mdobj;
		for (mdobj = md->owner->objects; mdobj; mdobj = mdobj->next)
		{
			if ((mdobj->type == MOBJ_MODDATA) && (mdobj->object.moddata == md))
			{
				DelListItem(mdobj, md->owner->objects);
				MyFree(mdobj);
				break;
			}
		}
		md->owner = NULL;
	}

	if (loop.ircd_rehashing)
		md->unloaded = 1;
	else
		unload_moddata_commit(md);
}

void unload_all_unused_moddata(void)
{
ModDataInfo *md, *md_next;

	for (md = MDInfo; md; md = md_next)
	{
		md_next = md->next;
		if (md->owner)
			abort(); /* shouldn't happen */
		unload_moddata_commit(md);
	}
}

ModDataInfo *findmoddata_byname(char *name, ModDataType type)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if ((md->type == type) && !strcmp(name, md->name))
			return md;

	return NULL;
}

// TODO: move below to efuncs

/** Send all moddata attached to client 'acptr' to remote server 'srv' (if the module wants this), called by .. */
void send_moddata_client(aClient *srv, aClient *acptr)
{
ModDataInfo *mdi;

	for (mdi = MDInfo; mdi; mdi = mdi->next)
	{
		if ((mdi->type == MODDATATYPE_CLIENT) && mdi->sync && mdi->serialize)
		{
			char *value = mdi->serialize(&moddata_client(acptr, mdi));
			if (value)
				sendto_one(srv, ":%s MD %s %s %s :%s",
					me.name, "client", acptr->name, mdi->name, value);
		}
	}
}

/** Broadcast moddata attached to client 'acptr' to all servers. */
void broadcast_moddata_client(aClient *acptr)
{
	aClient *cptr;

	list_for_each_entry(cptr, &server_list, special_node)
	{
		send_moddata_client(cptr, acptr);
	}
}

/** Send all moddata attached to channel 'chptr' to remote server 'srv' (if the module wants this), called by SJOIN */
void send_moddata_channel(aClient *srv, aChannel *chptr)
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
void send_moddata_members(aClient *srv)
{
ModDataInfo *mdi;
aChannel *chptr;
aClient *acptr;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		Member *m;
		for (m = chptr->members; m; m = m->next)
		{
			if (m->cptr->from == srv)
				continue; /* from srv's direction */
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBER) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_member(m, mdi));
					if (value)
						sendto_one(srv, ":%s MD %s %s:%s %s :%s",
							me.name, "member", chptr->chname, m->cptr->name, mdi->name, value);
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
			for (mdi = MDInfo; mdi; mdi = mdi->next)
			{
				if ((mdi->type == MODDATATYPE_MEMBERSHIP) && mdi->sync && mdi->serialize)
				{
					char *value = mdi->serialize(&moddata_membership(m, mdi));
					if (value)
						sendto_one(srv, ":%s MD %s %s:%s %s :%s",
							me.name, "membership", acptr->name, m->chptr->chname, mdi->name, value);
				}
			}
		}
	}
}

/** Set ModData for client (via variable name, string value) */
int moddata_client_set(aClient *acptr, char *varname, char *value)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_CLIENT);

	if (!md)
		return 0;

	if (value)
	{
		/* SET */
		md->unserialize(value, &moddata_client(acptr, md));
	}
	else
	{
		/* UNSET */
		md->free(&moddata_client(acptr, md));
		memset(&moddata_client(acptr, md), 0, sizeof(ModData));
	}

	/* If 'sync' field is set and the user is not in pre-registered state then
	 * broadcast the new setting.
	 */
	if (md->sync && IsPerson(acptr))
	{
		if (value)
			sendto_server(NULL, 0, 0, ":%s MD %s %s %s :%s",
				me.name, "client", acptr->name, md->name, value); /* set */
		else
			sendto_server(NULL, 0, 0, ":%s MD %s %s %s",
				me.name, "client", acptr->name, md->name); /* unset */
	}
	return 1;
}

/** Get ModData for client (via variable name) */
char *moddata_client_get(aClient *acptr, char *varname)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_CLIENT);

	if (!md)
		return NULL;

	return md->serialize(&moddata_client(acptr, md)); /* can be NULL */
}
