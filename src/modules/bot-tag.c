/*
 *   IRC - Internet Relay Chat, src/modules/bot-tag.c
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

/* This implements the message tag that is mentioned in
 * https://ircv3.net/specs/extensions/bot-mode
 * The B mode and 005 is in the modules/usermodes/bot module.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"bot-tag",
	"5.0",
	"bot message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

int bottag_mtag_is_ok(Client *client, const char *name, const char *value);
void mtag_add_bottag(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "bot";
	mtag.is_ok = bottag_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "draft/bot";
	mtag.is_ok = bottag_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_bottag);

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

/** This function verifies if the client sending the mtag is permitted to do so.
 */
int bottag_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client) && (value == NULL))
		return 1; /* OK */

	return 0;
}

void mtag_add_bottag(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;

	if (IsUser(client) && has_user_mode(client, 'B'))
	{
		MessageTag *m;

		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "bot");
		m->value = NULL;
		AddListItem(m, *mtag_list);

		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "draft/bot");
		m->value = NULL;
		AddListItem(m, *mtag_list);
	}
}
