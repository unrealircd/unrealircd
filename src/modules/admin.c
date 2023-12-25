/*
 *   IRC - Internet Relay Chat, src/modules/admin.c
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

CMD_FUNC(cmd_admin);

#define MSG_ADMIN 	"ADMIN"	

ModuleHeader MOD_HEADER
  = {
	"admin",
	"5.0",
	"command /admin", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_ADMIN, cmd_admin, MAXPARA, CMD_USER|CMD_SHUN|CMD_VIRUS);
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

/** ADMIN [servername].
 * Shows administrative information about this server to users.
 */
CMD_FUNC(cmd_admin)
{
	ConfigItem_admin *admin;

	if (IsUser(client))
	{
		if (hunt_server(client, recv_mtags, "ADMIN", 1, parc, parv) != HUNTED_ISME)
			return;
	}

	if (!conf_admin_tail)
	{
		sendnumeric(client, ERR_NOADMININFO, me.name);
		return;
	}

	sendnumeric(client, RPL_ADMINME, me.name);

	/* cycle through the list backwards */
	for (admin = conf_admin_tail; admin; admin = admin->prev)
	{
		if (!admin->next)
			sendnumeric(client, RPL_ADMINLOC1, admin->line);
		else if (!admin->next->next)
			sendnumeric(client, RPL_ADMINLOC2, admin->line);
		else
			sendnumeric(client, RPL_ADMINEMAIL, admin->line);
	}
}
