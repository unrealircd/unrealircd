/*
 *   IRC - Internet Relay Chat, src/modules/extended-isupport.c
 *   (C) 2025 Valware & The UnrealIRCd Team
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

/* One include for all */
#include "unrealircd.h"

/* Forward declarations */
CMD_FUNC(cmd_isupport);

/* Variables */
long CAP_EXTISUPPORT = 0L;

ModuleHeader MOD_HEADER = {
    "extended-isupport", /* Name of module */
    "5.0", /* Version */
    "Implements IRCv3 draft/extended-isupport", /* Short description of module */
    "UnrealIRCd Team", /* Author */
    "unrealircd-6", /* Version of UnrealIRCd */
};

// Module initialization
MOD_INIT()
{
	ClientCapabilityInfo cap;
	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&cap, 0, sizeof(cap));
	cap.name = "draft/extended-isupport";
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_EXTISUPPORT);

	CommandAdd(modinfo->handle, "ISUPPORT", cmd_isupport, 0, CMD_USER | CMD_UNREGISTERED);

	return MOD_SUCCESS;
}

// Module load
MOD_LOAD()
{
	return MOD_SUCCESS;
}

// Module unload
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

CMD_FUNC(cmd_isupport)
{
	if (!MyConnect(client))
		return;
	send_isupport(client);
}
