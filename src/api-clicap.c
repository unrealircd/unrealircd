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
ClientCapability *ClientCapabilityFind(const char *token)
{
	ClientCapability *clicap;

	for (clicap = clicaps; clicap; clicap = clicap->next)
	{
		if (!stricmp(token, clicap->name))
		{
			if (clicap->visible && !clicap->visible())
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
