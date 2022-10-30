/*
 *   IRC - Internet Relay Chat, src/modules/tline.c
 *   (C) 2022 Noisytoot & The UnrealIRCd Team
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

#define MSG_TLINE "TLINE"

CMD_FUNC(cmd_tline);

ModuleHeader MOD_HEADER
  = {
	"tline",
	"1.0",
	"TLINE command to show amount of clients matching a server ban mask",
	"UnrealIRCd team",
	"unrealircd-6",
	};

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_TLINE, cmd_tline, 1, CMD_OPER);
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
** cmd_tline
**	parv[1] = mask to test
*/
CMD_FUNC(cmd_tline)
{
	Client *acptr;
	int matching_lclients = 0;
	int matching_clients = 0;

	if ((parc < 1) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, MSG_TLINE);
		return;
	}

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (match_user(parv[1], acptr, MATCH_CHECK_REAL))
		{
			if (MyUser(acptr))
				matching_lclients++;
			matching_clients++;
		}
	}

	sendnotice(client,
	    "*** TLINE: Users matching mask '%s': global: %d/%d (%.2f%%), local: %d/%d (%.2f%%).",
	    parv[1], matching_clients, irccounts.clients,
	    (double)matching_clients / irccounts.clients * 100,
	    matching_lclients, irccounts.me_clients,
	    (double)matching_lclients / irccounts.me_clients * 100);
}
