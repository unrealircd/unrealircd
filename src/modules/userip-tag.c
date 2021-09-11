/*
 *   IRC - Internet Relay Chat, src/modules/userip-tag.c
 *   (C) 2020 Syzop & The UnrealIRCd Team
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

ModuleHeader MOD_HEADER
  = {
	"userip-tag",
	"5.0",
	"userip message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int userip_mtag_is_ok(Client *client, const char *name, const char *value);
int userip_mtag_should_send_to_client(Client *target);
void mtag_add_userip(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "unrealircd.org/userip";
	mtag.is_ok = userip_mtag_is_ok;
	mtag.should_send_to_client = userip_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_userip);

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

/** This function verifies if the client sending
 * 'userip-tag' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow userip-tag ONLY from servers and with any syntax.
 */
int userip_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_add_userip(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsUser(client) && client->ip)
	{
		MessageTag *m = find_mtag(recv_mtags, "unrealircd.org/userip");
		if (m)
		{
			m = duplicate_mtag(m);
		} else {
			char nuh[USERLEN+HOSTLEN+1];

			snprintf(nuh, sizeof(nuh), "%s@%s", client->user->username, GetIP(client));

			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "unrealircd.org/userip");
			safe_strdup(m->value, nuh);
		}
		AddListItem(m, *mtag_list);
	}
}

/** Outgoing filter for this message tag */
int userip_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;
	return 0;
}
