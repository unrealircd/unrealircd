/*
 *   IRC - Internet Relay Chat, src/modules/svsnoop.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnoop command
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

CMD_FUNC(cmd_svsnoop);

#define MSG_SVSNOOP 	"SVSNOOP"	


ModuleHeader MOD_HEADER
  = {
	"svsnoop",
	"5.0",
	"command /svsnoop", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSNOOP, cmd_svsnoop, MAXPARA, CMD_SERVER);
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

CMD_FUNC(cmd_svsnoop)
{
	Client *acptr;

	if (!(IsULine(client) && parc > 2))
		return;

	if (hunt_server(client, NULL, "SVSNOOP", 1, parc, parv) == HUNTED_ISME)
	{
		if (parv[2][0] == '+')
		{
			SVSNOOP = 1;
			unreal_log(ULOG_INFO, "svsnoop", "SVSNOOP_ENABLED", client,
			           "This server has been placed in NOOP mode (by $client) -- all IRCOp rights disabled");
			list_for_each_entry(acptr, &client_list, client_node)
			{
				if (MyUser(acptr) && IsOper(acptr))
				{
					if (IsOper(acptr))
					{
						irccounts.operators--;
						VERIFY_OPERCOUNT(acptr, "svsnoop");
					}

					if (!list_empty(&acptr->special_node))
						list_del(&acptr->special_node);

					RunHook(HOOKTYPE_LOCAL_OPER, client, 0, NULL, NULL);
					remove_oper_privileges(acptr, 1);
				}
			}
		}
		else
		{
			SVSNOOP = 0;
			unreal_log(ULOG_INFO, "svsnoop", "SVSNOOP_ENABLED", client,
			           "This server is no longer in NOOP mode (by $client) -- IRCOps can oper up again");
		}
	}
}
