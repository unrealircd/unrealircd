/*
 *   Unreal Internet Relay Chat Daemon, src/modules/lag.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

CMD_FUNC(cmd_lag);

/* Place includes here */
#define MSG_LAG         "LAG"   /* Lag detect */

ModuleHeader MOD_HEADER
  = {
	"lag",	/* Name of module */
	"5.0", /* Version */
	"command /lag", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_LAG, cmd_lag, MAXPARA, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/* cmd_lag (lag measure) - Stskeeps
 * parv[1] = server to query
*/

CMD_FUNC(cmd_lag)
{
	if (!ValidatePermissionsForPath("server:info:lag",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "LAG");
		return;
	}

	if (*parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "LAG");
		return;
	}

	if (hunt_server(client, recv_mtags, "LAG", 1, parc, parv) == HUNTED_NOSUCH)
		return;

	sendnotice(client, "Lag reply -- %s %s %lld", me.name, parv[1], (long long)TStime());
}
