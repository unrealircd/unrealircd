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

MODVAR ModDataInfo *MDInfo[HIGHESTMODDATATYPE+1] = { NULL };

MODVAR ModData local_variable_moddata[MODDATA_MAX_LOCAL_VARIABLE];
MODVAR ModData global_variable_moddata[MODDATA_MAX_GLOBAL_VARIABLE];

struct moddatatypelimit {
	ModDataType type;
	char *type_name;
	int limit;
	char *limit_name;
};

struct moddatatypelimit moddatatypelimits[] =
{
	{ MODDATATYPE_LOCAL_VARIABLE, "MODDATATYPE_LOCAL_VARIABLE", MODDATA_MAX_LOCAL_VARIABLE, "MODDATA_MAX_LOCAL_VARIABLE" },
	{ MODDATATYPE_GLOBAL_VARIABLE, "MODDATATYPE_GLOBAL_VARIABLE", MODDATA_MAX_GLOBAL_VARIABLE, "MODDATA_MAX_GLOBAL_VARIABLE" },
	{ MODDATATYPE_CLIENT, "MODDATATYPE_CLIENT", MODDATA_MAX_CLIENT, "MODDATA_MAX_CLIENT" },
	{ MODDATATYPE_LOCAL_CLIENT, "MODDATATYPE_LOCAL_CLIENT", MODDATA_MAX_LOCAL_CLIENT, "MODDATA_MAX_LOCAL_CLIENT" },
	{ MODDATATYPE_CHANNEL, "MODDATATYPE_CHANNEL", MODDATA_MAX_CHANNEL, "MODDATA_MAX_CHANNEL" },
	{ MODDATATYPE_MEMBER, "MODDATATYPE_MEMBER", MODDATA_MAX_MEMBER, "MODDATA_MAX_MEMBER" },
	{ MODDATATYPE_MEMBERSHIP, "MODDATATYPE_MEMBERSHIP", MODDATATYPE_MEMBERSHIP, "MODDATATYPE_MAX_MEMBERSHIP" },
	{ 0, NULL, 0, NULL },
};

int exceeds_moddatatype_limit(int type, int slot)
{
	int i;

	for (i = 0; moddatatypelimits[i].type; i++)
	{
		if (moddatatypelimits[i].type == type)
		{
			if (slot >= moddatatypelimits[i].limit)
			{
				unreal_log(ULOG_ERROR, "module", "MOD_DATA_OUT_OF_SPACE", NULL,
					   "ModDataAdd: out of space! Your $mod_data_type limit of $limit is reached. "
					   "Perhaps you have many third party modules loaded?\n"
					   "If you need more space then you could open include/config.h and "
					   "raise $mod_data_type_limit_name. You may also want to raise the other limits "
					   "there, just to be sure. After changing that file, you will have to "
					   "recompile (make clean; make install) and restart the IRCd.",
					   log_data_string("mod_data_type", moddatatypelimits[i].type_name),
					   log_data_string("mod_data_type_limit_name", moddatatypelimits[i].limit_name),
					   log_data_integer("limit", moddatatypelimits[i].limit));
				return 1;
			}
			return 0;
		}
	}

	/* If we reach here then we were called with an unknown moddatatype, which
	 * should be impossible. This could happen when f.e. ModDataType had a new
	 * type added but it was not added in the moddatatypelimits[] table above.
	 */
	abort();
	return 0;
}

void moddatatype_dump(Client *client)
{
	int i;
	ModDataInfo *m;
	int position;

	for (i = 0; moddatatypelimits[i].type; i++)
	{
#ifdef DEBUGMODE
		sendtxtnumeric(client, "=== %s ===", moddatatypelimits[i].type_name);
#endif
		for (position = 0, m = MDInfo[moddatatypelimits[i].type]; m ; m = m->next, position++)
		{
#ifdef DEBUGMODE
			sendtxtnumeric(client, "Position %d: %s",
			               position, m->name);
#endif
		}
		sendtxtnumeric(client, "%s has %d of %d slot(s) in use",
		               moddatatypelimits[i].type_name, position, moddatatypelimits[i].limit);
	}
}

/** Register module data storage.
 * This allows a module to attach custom data to clients, channels,
 * memberships, members, or other objects.
 * @param module	The module registering the moddata
 * @param req		The ModDataInfo request struct (name, type, callbacks, etc.)
 * @returns The ModDataInfo pointer, or NULL on failure.
 * @note Call this from MOD_INIT().
 */
ModDataInfo *ModDataAdd(Module *module, ModDataInfo req)
{
	int slotav = 0; /* highest available slot */
	ModDataInfo *m;
	int new_struct = 0;

	/* This would be a rather weird error on the module coder part */
	if ((req.type < 1) || (req.type > HIGHESTMODDATATYPE))
		abort();

	/* Hunt for highest available slot */
	for (m = MDInfo[req.type]; m ; m = m->next)
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

	if (exceeds_moddatatype_limit(req.type, slotav))
	{
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
	m->priority = req.priority;
	
	if (new_struct)
		AddListItemPrio(m, MDInfo[req.type], m->priority);

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

	for (md = MDInfo[MODDATATYPE_CLIENT]; md; md = md->next)
		if (md->free && moddata_client(client, md).ptr)
			md->free(&moddata_client(client, md));

	memset(client->moddata, 0, sizeof(client->moddata));
}

void moddata_free_local_client(Client *client)
{
	ModDataInfo *md;

	for (md = MDInfo[MODDATATYPE_LOCAL_CLIENT]; md; md = md->next)
		if (md->free && moddata_local_client(client, md).ptr)
			md->free(&moddata_local_client(client, md));

	memset(client->moddata, 0, sizeof(client->moddata));
}

void moddata_free_channel(Channel *channel)
{
	ModDataInfo *md;

	for (md = MDInfo[MODDATATYPE_CHANNEL]; md; md = md->next)
		if (md->free && moddata_channel(channel, md).ptr)
			md->free(&moddata_channel(channel, md));

	memset(channel->moddata, 0, sizeof(channel->moddata));
}

void moddata_free_member(Member *m)
{
	ModDataInfo *md;

	for (md = MDInfo[MODDATATYPE_MEMBER]; md; md = md->next)
		if (md->free && moddata_member(m, md).ptr)
			md->free(&moddata_member(m, md));

	memset(m->moddata, 0, sizeof(m->moddata));
}

void moddata_free_membership(Membership *m)
{
	ModDataInfo *md;

	for (md = MDInfo[MODDATATYPE_MEMBERSHIP]; md; md = md->next)
		if (md->free && moddata_membership(m, md).ptr)
			md->free(&moddata_membership(m, md));

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
	
	DelListItem(md, MDInfo[md->type]);
	safe_free(md->name);
	safe_free(md);
}

/** Delete module data storage.
 * @param md	The ModDataInfo to delete
 * @note Modules do not need to call this function,
 *       it is done automatically on module unload.
 */
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
		/* Module is unloading, so owner will be gone: */
		md->owner = NULL;
	}

	if (loop.rehashing)
	{
		/* Since the module is unloaded, these are not safe anymore either: */
		md->free = NULL;
		md->serialize = NULL;
		md->unserialize = NULL;
		md->unloaded = 1;
	} else {
		/* We need the free function and the like,
		 * and then completely destroy 'md'.
		 */
		unload_moddata_commit(md);
	}
}

void unload_all_unused_moddata(void)
{
	ModDataInfo *md, *md_next;
	int i;

	for (i = 1; i <= HIGHESTMODDATATYPE; i++)
	{
		for (md = MDInfo[i]; md; md = md_next)
		{
			md_next = md->next;
			if (md->unloaded)
			{
				//config_status("UNLOADING: md %s (owner %p, type %d, slot %d)",
				//	md->name, md->owner, md->type, md->slot);
				unload_moddata_commit(md);
			} else {
				//config_status("loaded: md %s (owner %p, type %d, slot %d)",
				//	md->name, md->owner, md->type, md->slot);
			}
		}
	}
}

ModDataInfo *findmoddata_byname(const char *name, ModDataType type)
{
	ModDataInfo *md;

	for (md = MDInfo[type]; md; md = md->next)
		if (!strcmp(name, md->name))
			return md;

	return NULL;
}

int module_has_moddata(Module *mod)
{
	ModDataInfo *md;
	int i;

	for (i = 1; i <= HIGHESTMODDATATYPE; i++)
		for (md = MDInfo[i]; md; md = md->next)
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
