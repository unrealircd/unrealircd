/*
 *   IRC - Internet Relay Chat, src/modules/react-tag.c
 *   (C) 2021 Syzop & The UnrealIRCd Team
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

/* This implements https://ircv3.net/specs/client-tags/react */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"react-tag",
	"5.0",
	"+react client tag",
	"UnrealIRCd Team",
	"unrealircd-5",
	};

int reacttag_mtag_is_ok(Client *client, char *name, char *value);
void mtag_add_reacttag(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

#if 0
	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "+react";
	mtag.is_ok = reacttag_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);
#endif

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "+draft/react";
	mtag.is_ok = reacttag_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_reacttag);

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

int reacttag_mtag_is_ok(Client *client, char *name, char *value)
{
	/* "This specification doesn’t define any restrictions on what can be
	 * sent as the reaction value." */
	return 1; /* OK */
}

void mtag_add_reacttag(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, char *signature)
{
	MessageTag *m;

	if (IsUser(client))
	{
#if 0
		m = find_mtag(recv_mtags, "+react");
		if (m)
		{
			m = duplicate_mtag(m);
			AddListItem(m, *mtag_list);
		}
#endif
		m = find_mtag(recv_mtags, "+draft/react");
		if (m)
		{
			m = duplicate_mtag(m);
			AddListItem(m, *mtag_list);
		}
	}
}
