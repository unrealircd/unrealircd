/*
 *   IRC - Internet Relay Chat, src/modules/sdesc.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

CMD_FUNC(cmd_sdesc);

#define MSG_SDESC 	"SDESC"	/* sdesc */

ModuleHeader MOD_HEADER
  = {
	"sdesc",	/* Name of module */
	"5.0", /* Version */
	"command /sdesc", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SDESC, cmd_sdesc, 1, CMD_USER);
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

/* cmd_sdesc - 15/05/1999 - Stskeeps
 *  :prefix SDESC
 *  parv[1] - description
 *  D: Sets server info if you are Server Admin (ONLINE)
*/

CMD_FUNC(cmd_sdesc)
{
	if (!ValidatePermissionsForPath("server:description",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}
	
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SDESC");
		return;
	}

	if (strlen(parv[1]) > REALLEN)
	{
		if (MyConnect(client))
		{
			sendnotice(client, "*** /SDESC Error: \"Server info\" may maximum be %i characters of length",
				REALLEN);
			return;
		}
		parv[1][REALLEN] = '\0';
	}

	ircsnprintf(client->srvptr->info, sizeof(client->srvptr->info), "%s", parv[1]);

	sendto_server(client, 0, 0, NULL, ":%s SDESC :%s", client->name, parv[1]);

	sendto_ops("Server description for %s is now '%s' (changed by %s)",
		client->srvptr->name, client->srvptr->info, client->name);
}
