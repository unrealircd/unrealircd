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

MODVAR ClientCapability *clicaps = NULL; /* List of client capabilities */

void clicap_init(void)
{
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
		if (!stricmp(token, clicap->name))
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
ClientCapability *ClientCapabilityFind(const char *token, aClient *sptr)
{
	ClientCapability *clicap;

	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (!stricmp(token, clicap->name))
		{
			if (clicap->visible && !clicap->visible(sptr))
				return NULL; /* hidden */
			return clicap;
		}
	}
	return NULL;
}

/**
 * Adds a new clicap token.
 *
 * @param module The module which owns this token.
 * @param token  The name of the token to create.
 * @param value  The value of the token (NULL indicates no value).
 * @return Returns the handle to the new token if successful, otherwise NULL.
 *         The module's error code contains specific information about the
 *         error.
 */
ClientCapability *ClientCapabilityAdd(Module *module, ClientCapability *clicap_request)
{
	ClientCapability *clicap;
	char *c;

	if (ClientCapabilityFindReal(clicap_request->name))
	{
		if (module)
			module->errorcode = MODERR_EXISTS;
		return NULL;
	}

	clicap = MyMallocEx(sizeof(ClientCapability));
	clicap->owner = module;
	clicap->name = strdup(clicap_request->name);
	clicap->cap = clicap_request->cap;
	clicap->flags = clicap_request->flags;
	clicap->visible = clicap_request->visible;
	clicap->parameter = clicap_request->parameter;

	AddListItem(clicap, clicaps);

	if (module)
	{
		ModuleObject *clicapobj = MyMallocEx(sizeof(ModuleObject));
		clicapobj->object.clicap = clicap;
		clicapobj->type = MOBJ_CLICAP;
		AddListItem(clicapobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return clicap;
}

/**
 * Removes the specified clicap token.
 *
 * @param clicap The token to remove.
 */
void ClientCapabilityDel(ClientCapability *clicap)
{
	DelListItem(clicap, clicaps);
	safefree(clicap->name);
	MyFree(clicap);
}

#define MAXCLICAPS 64
static char *old_caps[MAXCLICAPS]; /**< List of old CAP names - used for /rehash */
int old_caps_proto[MAXCLICAPS]; /**< List of old CAP protocol values - used for /rehash */

/** Called before REHASH. This saves the list of cap names and protocol values */
void clicap_pre_rehash(void)
{
	ClientCapability *clicap;
	int i = 0;

	memset(&old_caps, 0, sizeof(old_caps));

	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (i == MAXCLICAPS)
		{
			ircd_log(LOG_ERROR, "More than %d caps loaded - what???", MAXCLICAPS);
			break;
		}
		old_caps[i] = strdup(clicap->name);
		old_caps_proto[i] = clicap->cap;
		i++;
	}
}

/** Clear 'proto' protocol for all users */
void clear_cap_for_users(int proto)
{
	aClient *acptr;

	if (proto == 0)
		return;

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		acptr->local->proto &= ~proto;
	}
}

/** Called after REHASH. This will deal with:
 * 1. Clearing flags for caps that are deleted
 * 2. Sending any CAP DEL
 * 3. Sending any CAP NEW
 */
void clicap_post_rehash(void)
{
	ClientCapability *clicap;
	char *name;
	int i;
	int found;

	if (!loop.ircd_rehashing)
		return; /* First boot */

	/* Let's deal with CAP DEL first:
	 * Go through the old caps and see what's missing now.
	 */
	for (i = 0; old_caps[i]; i++)
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
		for (i = 0; old_caps[i]; i++)
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
}
