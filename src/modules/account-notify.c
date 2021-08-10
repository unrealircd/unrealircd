/*
 *   IRC - Internet Relay Chat, src/modules/account-notify.c
 *   (C) 2012-2020 The UnrealIRCd Team
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
	"account-notify",	/* Name of module */
	"5.0", 			/* Version */
	"account-notify CAP",	/* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_ACCOUNT_NOTIFY = 0L;

/* Forward declarations */
int account_notify_account_login(Client *client, MessageTag *mtags);

MOD_INIT()
{
	ClientCapabilityInfo c;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&c, 0, sizeof(c));
	c.name = "account-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_ACCOUNT_NOTIFY);

	HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, account_notify_account_login);

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

int account_notify_account_login(Client *client, MessageTag *recv_mtags)
{
	MessageTag *mtags = NULL;
	new_message(client, recv_mtags, &mtags);
	sendto_local_common_channels(client, client,
				     CAP_ACCOUNT_NOTIFY, mtags,
				     ":%s ACCOUNT %s",
				     client->name,
				     IsLoggedIn(client) ? client->user->account : "*");
	free_message_tags(mtags);
	return 0;
}
