/*
 *   IRC - Internet Relay Chat, src/modules/cap/msgid.c
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

ModuleHeader MOD_HEADER(msgid)
  = {
	"msgid",
	"4.2",
	"msgid CAP",
	"3.2-b8-1",
	NULL 
	};

/* Variables */
long CAP_ACCOUNT_TAG = 0L;

int msgid_mtag_is_ok(aClient *acptr, char *name, char *value);

MOD_INIT(msgid)
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MessageTagHandlerInfo mtag;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "msgid";
	mtag.is_ok = msgid_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	return MOD_SUCCESS;
}

MOD_LOAD(msgid)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(msgid)
{
	return MOD_SUCCESS;
}

/** This function verifies if the client sending
 * 'msgid' is permitted to do so and uses a permitted
 * syntax.
 * We simply allow msgid ONLY from servers and with any syntax.
 */
int msgid_mtag_is_ok(aClient *acptr, char *name, char *value)
{
	if (IsServer(acptr))
		return 1;

	return 0;
}

// TODO: move from m_message (and elsewhere?) to here, some hook call or something
