/*
 *   IRC - Internet Relay Chat, src/modules/account-tag.c
 *   (C) 2019 Syzop & The UnrealIRCd Team
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
	"account-tag",
	"5.0",
	"account-tag CAP",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int account_tag_mtag_is_ok(Client *client, const char *name, const char *value);
void mtag_add_account(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "account-tag";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_ACCOUNT_TAG);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "account";
	mtag.is_ok = account_tag_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_account);

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
 * 'account-tag' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow account-tag ONLY from servers and with any syntax.
 */
int account_tag_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_add_account(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsLoggedIn(client))
	{
		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "account");
		safe_strdup(m->value, client->user->account);

		AddListItem(m, *mtag_list);
	}
}
