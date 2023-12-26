/*
 *   IRC - Internet Relay Chat, src/modules/locops.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_locops);

#define MSG_LOCOPS 	"LOCOPS"	

ModuleHeader MOD_HEADER
  = {
	"locops",
	"5.0",
	"command /locops", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_LOCOPS, cmd_locops, 1, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

/*
** cmd_locops (write to opers who are +g currently online *this* server)
**      parv[1] = message text
*/
CMD_FUNC(cmd_locops)
{
	const char *message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "LOCOPS");
		return;
	}
	if (MyUser(client) && !ValidatePermissionsForPath("chat:locops",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}
	sendto_umode(UMODE_OPER, "from %s: %s", client->name, message);
}
