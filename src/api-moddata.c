/************************************************************************
 *   IRC - Internet Relay Chat, src/api-moddata.c
 *   (C) 2003-2019 Bram Matthys (Syzop) and the UnrealIRCd Team
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

MODVAR ModDataInfo *MDInfo = NULL;

MODVAR ModData local_variable_moddata[MODDATA_MAX_LOCAL_VARIABLE];
MODVAR ModData global_variable_moddata[MODDATA_MAX_GLOBAL_VARIABLE];

ModDataInfo *ModDataAdd(Module *module, ModDataInfo req)
{
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
	if (((req.type == MODDATATYPE_LOCAL_VARIABLE) && (slotav >= MODDATA_MAX_LOCAL_VARIABLE)) ||
	    ((req.type == MODDATATYPE_GLOBAL_VARIABLE) && (slotav >= MODDATA_MAX_GLOBAL_VARIABLE)) ||
	    ((req.type == MODDATATYPE_CLIENT) && (slotav >= MODDATA_MAX_CLIENT)) ||
	    ((req.type == MODDATATYPE_LOCAL_CLIENT) && (slotav >= MODDATA_MAX_LOCAL_CLIENT)) ||
	    ((req.type == MODDATATYPE_CHANNEL) && (slotav >= MODDATA_MAX_CHANNEL)) ||
	    ((req.type == MODDATATYPE_MEMBER) && (slotav >= MODDATA_MAX_MEMBER)) ||
	    ((req.type == MODDATATYPE_MEMBERSHIP) && (slotav >= MODDATA_MAX_MEMBERSHIP)))
	{
		unreal_log(ULOG_ERROR, "module", "MOD_DATA_OUT_OF_SPACE", NULL,
		           "ModDataAdd: out of space!!!");
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}

	new_struct = 1;
	m = safe_alloc(sizeof(ModDataInfo));
	safe_strdup(m->name, req.name);
	m->slot = slotav;
	m->type = req.type;
moddataadd_isok:
	m->free = req.free;
	m->serialize = req.serialize;
	m->unserialize = req.unserialize;
	m->sync = req.sync;
	m->remote_write = req.remote_write;
	m->self_write = req.self_write;
	m->owner = module;
	
	if (new_struct)
		AddListItem(m, MDInfo);

	if (module)
	{
		ModuleObject *mobj = safe_alloc(sizeof(ModuleObject));
		mobj->object.moddata = m;
		mobj->type = MOBJ_MODDATA;
		AddListItem(mobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	
	return m;
}

void moddata_free_client(Client *client)
{
	ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_CLIENT)
		{
			if (md->free && moddata_client(client, md).ptr)
				md->free(&moddata_client(client, md));
		}

	memset(client->moddata, 0, sizeof(client->moddata));
}

void moddata_free_local_client(Client *client)
{
	ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_LOCAL_CLIENT)
		{
			if (md->free && moddata_local_client(client, md).ptr)
				md->free(&moddata_local_client(client, md));
		}

	memset(client->moddata, 0, sizeof(client->moddata));
}

void moddata_free_channel(Channel *channel)
{
	ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->type == MODDATATYPE_CHANNEL)
		{
			if (md->free && moddata_channel(channel, md).ptr)
				md->free(&moddata_channel(channel, md));
		}

	memset(channel->moddata, 0, sizeof(channel->moddata));
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
		case MODDATATYPE_LOCAL_VARIABLE:
			if (md->free && moddata_local_variable(md).ptr)
				md->free(&moddata_local_variable(md));
			memset(&moddata_local_variable(md), 0, sizeof(ModData));
			break;
		case MODDATATYPE_GLOBAL_VARIABLE:
			if (md->free && moddata_global_variable(md).ptr)
				md->free(&moddata_global_variable(md));
			memset(&moddata_global_variable(md), 0, sizeof(ModData));
			break;
		case MODDATATYPE_CLIENT:
		{
			Client *client;
			list_for_each_entry(client, &client_list, client_node)
			{
				if (md->free && moddata_client(client, md).ptr)
					md->free(&moddata_client(client, md));
				memset(&moddata_client(client, md), 0, sizeof(ModData));
			}
			list_for_each_entry(client, &unknown_list, lclient_node)
			{
				if (md->free && moddata_client(client, md).ptr)
					md->free(&moddata_client(client, md));
				memset(&moddata_client(client, md), 0, sizeof(ModData));
			}
			break;
		}
		case MODDATATYPE_LOCAL_CLIENT:
		{
			Client *client;
			list_for_each_entry(client, &lclient_list, lclient_node)
			{
				if (md->free && moddata_local_client(client, md).ptr)
					md->free(&moddata_local_client(client, md));
				memset(&moddata_local_client(client, md), 0, sizeof(ModData));
			}
			list_for_each_entry(client, &unknown_list, lclient_node)
			{
				if (md->free && moddata_local_client(client, md).ptr)
					md->free(&moddata_local_client(client, md));
				memset(&moddata_local_client(client, md), 0, sizeof(ModData));
			}
			break;
		}
		case MODDATATYPE_CHANNEL:
		{
			Channel *channel;
			for (channel = channels; channel; channel=channel->nextch)
			{
				if (md->free && moddata_channel(channel, md).ptr)
					md->free(&moddata_channel(channel, md));
				memset(&moddata_channel(channel, md), 0, sizeof(ModData));
			}
			break;
		}
		case MODDATATYPE_MEMBER:
		{
			Channel *channel;
			Member *m;
			for (channel = channels; channel; channel=channel->nextch)
			{
				for (m = channel->members; m; m = m->next)
				{
					if (md->free && moddata_member(m, md).ptr)
						md->free(&moddata_member(m, md));
					memset(&moddata_member(m, md), 0, sizeof(ModData));
				}
			}
			break;
		}
		case MODDATATYPE_MEMBERSHIP:
		{
			Client *client;
			Membership *m;
			list_for_each_entry(client, &lclient_list, lclient_node)
			{
				if (!client->user)
					continue;
				for (m = client->user->channel; m; m = m->next)
				{
					if (md->free && moddata_membership(m, md).ptr)
						md->free(&moddata_membership(m, md));
					memset(&moddata_membership(m, md), 0, sizeof(ModData));
				}
			}
			break;
		}
	}
	
	DelListItem(md, MDInfo);
	safe_free(md->name);
	safe_free(md);
}

void ModDataDel(ModDataInfo *md)
{
	/* Delete the reference to us first */
	if (md->owner)
	{
		ModuleObject *mdobj;
		for (mdobj = md->owner->objects; mdobj; mdobj = mdobj->next)
		{
			if ((mdobj->type == MOBJ_MODDATA) && (mdobj->object.moddata == md))
			{
				DelListItem(mdobj, md->owner->objects);
				safe_free(mdobj);
				break;
			}
		}
		md->owner = NULL;
	}

	if (loop.rehashing)
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
		if (md->unloaded)
			unload_moddata_commit(md);
	}
}

ModDataInfo *findmoddata_byname(const char *name, ModDataType type)
{
ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if ((md->type == type) && !strcmp(name, md->name))
			return md;

	return NULL;
}

int module_has_moddata(Module *mod)
{
	ModDataInfo *md;

	for (md = MDInfo; md; md = md->next)
		if (md->owner == mod)
			return 1;

	return 0;
}

/** Set ModData for client (via variable name, string value) */
int moddata_client_set(Client *client, const char *varname, const char *value)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_CLIENT);

	if (!md)
		return 0;

	if (value)
	{
		/* SET */
		md->unserialize(value, &moddata_client(client, md));
	}
	else
	{
		/* UNSET */
		md->free(&moddata_client(client, md));
		memset(&moddata_client(client, md), 0, sizeof(ModData));
	}

	/* If 'sync' field is set and the client is not in pre-registered
	 * state then broadcast the new setting.
	 */
	if (md->sync && (IsUser(client) || IsServer(client) || IsMe(client)))
		broadcast_md_client_cmd(NULL, &me, client, md->name, value);

	return 1;
}

/** Get ModData for client (via variable name) */
const char *moddata_client_get(Client *client, const char *varname)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_CLIENT);

	if (!md)
		return NULL;

	return md->serialize(&moddata_client(client, md)); /* can be NULL */
}

/** Get ModData for client (via variable name) */
ModData *moddata_client_get_raw(Client *client, const char *varname)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_CLIENT);

	if (!md)
		return NULL;

	return &moddata_client(client, md); /* can be NULL */
}

/** Set ModData for LocalClient (via variable name, string value) */
int moddata_local_client_set(Client *client, const char *varname, const char *value)
{
	ModDataInfo *md;

	if (!MyConnect(client))
		abort();

	md = findmoddata_byname(varname, MODDATATYPE_LOCAL_CLIENT);

	if (!md)
		return 0;

	if (value)
	{
		/* SET */
		md->unserialize(value, &moddata_local_client(client, md));
	}
	else
	{
		/* UNSET */
		md->free(&moddata_local_client(client, md));
		memset(&moddata_local_client(client, md), 0, sizeof(ModData));
	}

	/* If 'sync' field is set and the client is not in pre-registered
	 * state then broadcast the new setting.
	 */
	if (md->sync && (IsUser(client) || IsServer(client) || IsMe(client)))
		broadcast_md_client_cmd(NULL, &me, client, md->name, value);

	return 1;
}

/** Get ModData for LocalClient (via variable name) */
const char *moddata_local_client_get(Client *client, const char *varname)
{
	ModDataInfo *md;

	if (!MyConnect(client))
		abort();

	md = findmoddata_byname(varname, MODDATATYPE_LOCAL_CLIENT);

	if (!md)
		return NULL;

	return md->serialize(&moddata_local_client(client, md)); /* can be NULL */
}

/** Set local variable moddata (via variable name, string value) */
int moddata_local_variable_set(const char *varname, const char *value)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_LOCAL_VARIABLE);

	if (!md)
		return 0;

	if (value)
	{
		/* SET */
		md->unserialize(value, &moddata_local_variable(md));
	}
	else
	{
		/* UNSET */
		md->free(&moddata_local_variable(md));
		memset(&moddata_local_variable(md), 0, sizeof(ModData));
	}

	return 1;
}

/** Set global variable moddata (via variable name, string value) */
int moddata_global_variable_set(const char *varname, const char *value)
{
	ModDataInfo *md;

	md = findmoddata_byname(varname, MODDATATYPE_GLOBAL_VARIABLE);

	if (!md)
		return 0;

	if (value)
	{
		/* SET */
		md->unserialize(value, &moddata_global_variable(md));
	}
	else
	{
		/* UNSET */
		md->free(&moddata_global_variable(md));
		memset(&moddata_global_variable(md), 0, sizeof(ModData));
	}

	if (md->sync)
		broadcast_md_globalvar_cmd(NULL, &me, md->name, value);

	return 1;
}

/* The rest of the MD related functions, the send/receive functions,
 * are in src/modules/md.c
 */
