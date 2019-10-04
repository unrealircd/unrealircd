/*
 *   IRC - Internet Relay Chat, src/modules/svsfline.c
 *   (C) 2004-present The UnrealIRCd Team
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

CMD_FUNC(cmd_svsfline);

#define MSG_SVSFLINE 	"SVSFLINE"	

ModuleHeader MOD_HEADER
  = {
	"svsfline",
	"5.0",
	"command /svsfline", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSFLINE, cmd_svsfline, MAXPARA, CMD_SERVER);
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

CMD_FUNC(cmd_svsfline)
{
	if (parc < 2)
		return;

	switch (*parv[1])
	{
		/* Allow non-U-Lines to send ONLY SVSFLINE +, but don't send it out
		 * unless it is from a U-Line -- codemastr
		 */
		case '+':
		{
			if (parc < 4)
				return;

			if (!Find_deny_dcc(parv[2]))
				DCCdeny_add(parv[2], parv[3], DCCDENY_HARD, CONF_BAN_TYPE_AKILL);

			if (IsULine(sptr))
			{
				sendto_server(sptr, 0, 0, NULL, ":%s SVSFLINE + %s :%s",
				    sptr->name, parv[2], parv[3]);
			}

			break;
		}

		case '-':
		{
			ConfigItem_deny_dcc *deny;

			if (!IsULine(sptr))
				return;

			if (parc < 3)
				return;

			if (!(deny = Find_deny_dcc(parv[2])))
				break;

			DCCdeny_del(deny);

			sendto_server(sptr, 0, 0, NULL, ":%s SVSFLINE %s", sptr->name, parv[2]);

			break;
		}

		case '*':
		{
			if (!IsULine(sptr))
				return;

			dcc_wipe_services();

			sendto_server(sptr, 0, 0, NULL, ":%s SVSFLINE *", sptr->name);

			break;
		}
	}
}
