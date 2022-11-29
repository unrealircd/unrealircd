/*
 *   IRC - Internet Relay Chat, src/modules/server-time.c
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
	"server-time",
	"5.0",
	"server-time CAP",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Variables */
long CAP_SERVER_TIME = 0L;

int server_time_mtag_is_ok(Client *client, const char *name, const char *value);
void mtag_add_or_inherit_time(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "server-time";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_SERVER_TIME);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "time";
	mtag.is_ok = server_time_mtag_is_ok;
	mtag.clicap_handler = c;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_or_inherit_time);

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
 * 'server-time' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow server-time ONLY from servers.
 */
int server_time_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client) && !BadPtr(value) && HasCapability(client,"server-time"))
		return 1;

	return 0;
}

void mtag_add_or_inherit_time(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m = find_mtag(recv_mtags, "time");
	if (m)
	{
		m = duplicate_mtag(m);
	} else
	{
		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "time");
		safe_strdup(m->value, timestamp_iso8601_now());
	}
	AddListItem(m, *mtag_list);
}
