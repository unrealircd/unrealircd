/************************************************************************
 *   UnrealIRCd - Unreal Internet Relay Chat Daemon - src/api-clicap.c
 *   (c) 2015- Bram Matthys and The UnrealIRCd team
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

#define ADVERTISEONLYCAPS 16
/* Advertise only caps are not counted anywhere, this only provides space in rehash temporary storage arrays.
 * If exceeded, the caps just won't be stored and will be re-added safely. --k4be
 */

#define MAXCLICAPS ((int)(sizeof(long)*8 - 1 + ADVERTISEONLYCAPS)) /* how many cap bits will fit in `long`? */
static char *old_caps[MAXCLICAPS]; /**< List of old CAP names - used for /rehash */
int old_caps_proto[MAXCLICAPS]; /**< List of old CAP protocol values - used for /rehash */

MODVAR ClientCapability *clicaps = NULL; /* List of client capabilities */

void clicap_init(void)
{
	memset(&old_caps, 0, sizeof(old_caps));
}

/**
 * Returns an clicap handle based on the given token name.
 *
 * @param token The clicap token to search for.
 * @return Returns the handle to the clicap token if it was found,
 *         otherwise NULL is returned.
 */
ClientCapability *ClientCapabilityFindReal(const char *token)
{
	ClientCapability *clicap;

	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (!strcasecmp(token, clicap->name))
			return clicap;
	}

	return NULL;
}

/**
 * Returns an clicap handle based on the given token name.
 *
 * @param token The clicap token to search for.
 * @return Returns the handle to the clicap token if it was found,
 *         otherwise NULL is returned.
 */
ClientCapability *ClientCapabilityFind(const char *token, Client *client)
{
	ClientCapability *clicap;

	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (!strcasecmp(token, clicap->name))
		{
			if (clicap->visible && !clicap->visible(client))
				return NULL; /* hidden */
			return clicap;
		}
	}
	return NULL;
}

/** Find the bit that will be set if 'token' is enabled */
long ClientCapabilityBit(const char *token)
{
	ClientCapability *clicap = ClientCapabilityFindReal(token);

#ifdef DEBUGMODE
	if (!clicap)
	{
		unreal_log(ULOG_WARNING, "main", "BUG_CLIENTCAPABILITYBIT_UNKNOWN_TOKEN", NULL,
		           "[BUG] ClientCapabilityBit() check for unknown token: $token",
		           log_data_string("token", token));
	}
#endif

	return clicap ? clicap->cap : 0L;
}

void SetCapability(Client *client, const char *token)
{
	client->local->caps |= ClientCapabilityBit(token);
}

void ClearCapability(Client *client, const char *token)
{
	client->local->caps &= ~(ClientCapabilityBit(token));
}

long clicap_allocate_cap(void)
{
	long v;
	ClientCapability *clicap;

	/* The first bit (v=1) is used by the "invert" marker */
	for (v=2; v; v <<= 1)
	{
		unsigned char found = 0;
		for (clicap = clicaps; clicap; clicap = clicap->next)
		{
			if (clicap->cap == v)
			{
				found = 1;
				break;
			}
		}
		if (!found)
			return v; /* free bit found */
	}

	return 0;
}

/**
 * Adds a new clicap token.
 *
 * @param module The module which owns this token.
 * @param clicap_request The details of the requested token, handlers, etc.
 * @param cap The assigned capability bit.
 * @return Returns the handle to the new token if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
ClientCapability *ClientCapabilityAdd(Module *module, ClientCapabilityInfo *clicap_request, long *cap)
{
	ClientCapability *clicap;
	int exists = 0;

	if (cap)
		*cap = 0; /* Initialize early */

	clicap = ClientCapabilityFindReal(clicap_request->name);
	if (clicap)
	{
		exists = 1;
		if (clicap->unloaded)
		{
			clicap->unloaded = 0;
		} else {
			if (module)
				module->errorcode = MODERR_EXISTS;
			return NULL;
		}
	} else {
		long v = 0;

		/* Allocate a bit, but only if the module needs it.
		 * (some clicaps are advertise-only and never gets set,
		 *  hence they don't need a bit allocated to them)
		 */
		if (cap != NULL)
		{
			v = clicap_allocate_cap();
			if (v == 0)
			{
				unreal_log(ULOG_ERROR, "module", "CLIENTCAPABILITY_OUT_OF_SPACE", NULL,
				           "ClientCapabilityAdd: out of space!!!");
				if (module)
					module->errorcode = MODERR_NOSPACE;
				return NULL;
			}
		}
		/* New client capability */
		clicap = safe_alloc(sizeof(ClientCapability));
		safe_strdup(clicap->name, clicap_request->name);
		clicap->cap = v;
	}
	/* Add or update the following fields: */
	clicap->owner = module;
	clicap->flags = clicap_request->flags;
	clicap->visible = clicap_request->visible;
	clicap->parameter = clicap_request->parameter;

	if (!exists)
		AddListItem(clicap, clicaps);

	if (clicap->cap && !cap)
		abort(); /* module API call error */

	if (cap)
		*cap = clicap->cap;

	if (module)
	{
		ModuleObject *clicapobj = safe_alloc(sizeof(ModuleObject));
		clicapobj->object.clicap = clicap;
		clicapobj->type = MOBJ_CLICAP;
		AddListItem(clicapobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}

	return clicap;
}

void unload_clicap_commit(ClientCapability *clicap)
{
	/* This is an unusual operation, I think we should log it. */
	unreal_log(ULOG_INFO, "module", "UNLOAD_CLICAP", NULL,
	           "Unloading client capability '$token'",
	           log_data_string("token", clicap->name));

	/* NOTE: Stripping the CAP from local clients is done
	 * in clicap_check_for_changes(), so not here.
	 */

	/* A message tag handler may depend on us, remove it */
	/* NOTE: This assumes there is a 0:1 or 1:1 relationship between
	 *       the two, but in theory there could be multiple message tags
	 *       introduced by 1 capability. Ah well, we'll cross that
	 *       bridge when we come to it ;)
	 */
	if (clicap->mtag_handler)
		clicap->mtag_handler->clicap_handler = NULL;

	/* Destroy the capability */
	DelListItem(clicap, clicaps);
	safe_free(clicap->name);
	safe_free(clicap);
}
/**
 * Removes the specified clicap token.
 *
 * @param clicap The token to remove.
 */
void ClientCapabilityDel(ClientCapability *clicap)
{
	if (clicap->owner)
	{
		ModuleObject *mobj;
		for (mobj = clicap->owner->objects; mobj; mobj = mobj->next) {
			if (mobj->type == MOBJ_CLICAP && mobj->object.clicap == clicap) {
				DelListItem(mobj, clicap->owner->objects);
				safe_free(mobj);
				break;
			}
		}
		clicap->owner = NULL;
	}

	if (loop.rehashing)
		clicap->unloaded = 1;
	else
		unload_clicap_commit(clicap);
}

void unload_all_unused_caps(void)
{
	ClientCapability *clicap, *clicap_next;

	for (clicap = clicaps; clicap; clicap = clicap_next)
	{
		clicap_next = clicap->next;
		if (clicap->unloaded)
			unload_clicap_commit(clicap);
	}
}

/** Called before REHASH. This saves the list of cap names and protocol values */
void clicap_pre_rehash(void)
{
	ClientCapability *clicap;
	int i = 0;

	for (i=0; i < MAXCLICAPS; i++)
	{
		safe_free(old_caps[i]);
		old_caps_proto[i] = 0;
	}

	for (i=0, clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (i == MAXCLICAPS)
		{
			unreal_log(ULOG_ERROR, "module", "BUG_TOO_MANY_CLIENTCAPABILITIES", NULL,
			           "[BUG] clicap_pre_rehash: More than $count caps loaded - this should never happen",
			           log_data_integer("count", MAXCLICAPS));
			break;
		}
		safe_strdup(old_caps[i], clicap->name);
		old_caps_proto[i] = clicap->cap;
		i++;
	}
}

/** Clear 'proto' protocol for all users */
void clear_cap_for_users(long cap)
{
	Client *client;

	if (cap == 0)
		return;

	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		client->local->caps &= ~cap;
	}
	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		client->local->caps &= ~cap;
	}
}

/** Called after REHASH. This will deal with:
 * 1. Clearing flags for caps that are deleted
 * 2. Sending any CAP DEL
 * 3. Sending any CAP NEW
 */
void clicap_check_for_changes(void)
{
	ClientCapability *clicap;
	char *name;
	int i;
	int found;

	if (!loop.rehashing)
		return; /* First boot */

	/* Let's deal with CAP DEL first:
	 * Go through the old caps and see what's missing now.
	 */
	for (i = 0; i < MAXCLICAPS && old_caps[i]; i++)
	{
		name = old_caps[i];
		found = 0;
		for (clicap = clicaps; clicap; clicap = clicap->next)
		{
			if (!strcmp(clicap->name, name))
			{
				found = 1;
				break;
			}
		}
		if (!found)
		{
			/* Broadcast CAP DEL to local users */
			send_cap_notify(0, name);
			clear_cap_for_users(old_caps_proto[i]);
		}
	}

	/* Now deal with CAP ADD:
	 * Go through the new caps and see if it was missing from old caps.
	 */
	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		name = clicap->name;
		found = 0;
		for (i = 0; i < MAXCLICAPS && old_caps[i]; i++)
		{
			if (!strcmp(old_caps[i], name))
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			/* Broadcast CAP NEW to local users */
			send_cap_notify(1, name);
		}
	}

	/* Now free the old caps. */
	for (i = 0; i < MAXCLICAPS && old_caps[i]; i++)
		safe_free(old_caps[i]);
}
