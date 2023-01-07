/*
 *   IRC - Internet Relay Chat, src/modules/svslusers.c
 *   (C) 2002 codemastr [Dominick Meglio] (codemastr@unrealircd.com)
 *
 *   SVSLUSERS command, allows remote setting of local and global max user count
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

CMD_FUNC(cmd_svslusers);

#define MSG_SVSLUSERS 	"SVSLUSERS"	

ModuleHeader MOD_HEADER
  = {
	"svslusers",
	"5.0",
	"command /svslusers", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSLUSERS, cmd_svslusers, MAXPARA, CMD_SERVER);
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
** cmd_svslusers
**      parv[1] = server to update
**      parv[2] = max global users
**      parv[3] = max local users
**      If -1 is specified for either number, it is ignored and the current count
**      is kept.
*/
CMD_FUNC(cmd_svslusers)
{
        if (!IsSvsCmdOk(client) || parc < 4)
		return;  
        if (hunt_server(client, NULL, "SVSLUSERS", 1, parc, parv) == HUNTED_ISME)
        {
		int temp;
		temp = atoi(parv[2]);
		if (temp >= 0)
			irccounts.global_max = temp;
		temp = atoi(parv[3]);
		if (temp >= 0) 
			irccounts.me_max = temp;
        }
}
