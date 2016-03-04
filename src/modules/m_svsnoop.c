/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnoop.c
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

CMD_FUNC(m_svsnoop);

#define MSG_SVSNOOP 	"SVSNOOP"	


ModuleHeader MOD_HEADER(m_svsnoop)
  = {
	"m_svsnoop",
	"4.0",
	"command /svsnoop", 
	"3.2-b8-1",
	NULL
    };

MOD_INIT(m_svsnoop)
{
	CommandAdd(modinfo->handle, MSG_SVSNOOP, m_svsnoop, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svsnoop)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svsnoop)
{
	return MOD_SUCCESS;
}

CMD_FUNC(m_svsnoop)
{
	aClient *acptr;

	if (!(IsULine(sptr) && parc > 2))
		return 0;

	if (hunt_server(cptr, sptr, ":%s SVSNOOP %s :%s", 1, parc, parv) == HUNTED_ISME)
	{
		if (parv[2][0] == '+')
		{
			SVSNOOP = 1;
			sendto_ops("This server has been placed in NOOP mode");
			list_for_each_entry(acptr, &client_list, client_node)
			{
				if (MyClient(acptr) && IsOper(acptr))
				{
					if (IsOper(acptr))
					{
						IRCstats.operators--;
						VERIFY_OPERCOUNT(acptr, "svsnoop");
					}

					if (!list_empty(&acptr->special_node))
						list_del(&acptr->special_node);

					remove_oper_privileges(acptr, 1);
					RunHook2(HOOKTYPE_LOCAL_OPER, acptr, 0);
				}
			}
		}
		else
		{
			SVSNOOP = 0;
			sendto_ops("This server is no longer in NOOP mode");
		}
	}
	return 0;
}
