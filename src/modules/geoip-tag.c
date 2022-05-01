/*
 *   IRC - Internet Relay Chat, src/modules/geoip-tag.c
 *   (C) 2022 westor, Syzop and The UnrealIRCd Team
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
	"geoip-tag",
	"6.0",
	"geoip message tag",
	"UnrealIRCd Team",
	"unrealircd-6",
	};

/* Forward declarations */
int geoip_mtag_is_ok(Client *client, const char *name, const char *value);
int geoip_mtag_should_send_to_client(Client *target);
void mtag_add_geoip(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature);

MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "unrealircd.org/geoip";
	mtag.is_ok = geoip_mtag_is_ok;
	mtag.should_send_to_client = geoip_mtag_should_send_to_client;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	HookAddVoid(modinfo->handle, HOOKTYPE_NEW_MESSAGE, 0, mtag_add_geoip);

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
 * 'geoip-tag' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow geoip-tag ONLY from servers and with any syntax.
 */
int geoip_mtag_is_ok(Client *client, const char *name, const char *value)
{
	if (IsServer(client))
		return 1;

	return 0;
}

void mtag_add_geoip(Client *client, MessageTag *recv_mtags, MessageTag **mtag_list, const char *signature)
{
	MessageTag *m;
	
	GeoIPResult *geoip;

	if (IsUser(client) && ((geoip = geoip_client(client))))
	{
		MessageTag *m = find_mtag(recv_mtags, "unrealircd.org/geoip");
		if (m)
		{
			m = duplicate_mtag(m);
		} else {
			m = safe_alloc(sizeof(MessageTag));
			safe_strdup(m->name, "unrealircd.org/geoip");
			safe_strdup(m->value, geoip->country_code);
		}
		AddListItem(m, *mtag_list);
	}
}

/** Outgoing filter for this message tag */
int geoip_mtag_should_send_to_client(Client *target)
{
	if (IsServer(target) || IsOper(target))
		return 1;

	return 0;
}
