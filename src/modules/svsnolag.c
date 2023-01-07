/*
 *   IRC - Internet Relay Chat, src/modules/svsnolag.c
 *   (C) 2006 Alex Berezhnyak and djGrrr
 *
 *   Fake lag exception - SVSNOLAG and SVS2NOLAG commands
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

CMD_FUNC(cmd_svsnolag);
CMD_FUNC(cmd_svs2nolag);

#define MSG_SVSNOLAG 	"SVSNOLAG"	
#define MSG_SVS2NOLAG 	"SVS2NOLAG"	

ModuleHeader MOD_HEADER
  = {
	"svsnolag",
	"5.0",
	"commands /svsnolag and /svs2nolag", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSNOLAG, cmd_svsnolag, MAXPARA, CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_SVS2NOLAG, cmd_svs2nolag, MAXPARA, CMD_SERVER);
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

void do_svsnolag(Client *client, int parc, const char *parv[], int show_change)
{
	Client *target;
	char *cmd = show_change ? MSG_SVS2NOLAG : MSG_SVSNOLAG;

	if (!IsSvsCmdOk(client))
		return;

	if (parc < 3)
		return;

	if (!(target = find_user(parv[2], NULL)))
		return;

	if (!MyUser(target))
	{
		sendto_one(target, NULL, ":%s %s %s %s", client->name, cmd, parv[1], parv[2]);
		return;
	}

	if (*parv[1] == '+')
	{
		if (!IsNoFakeLag(target))
		{
			SetNoFakeLag(target);
			if (show_change)
				sendnotice(target, "You are now exempted from fake lag");
		}
	}
	if (*parv[1] == '-')
	{
		if (IsNoFakeLag(target))
		{
			ClearNoFakeLag(target);
			if (show_change)
				sendnotice(target, "You are no longer exempted from fake lag");
		}
	}
}

CMD_FUNC(cmd_svsnolag)
{
	do_svsnolag(client, parc, parv, 0);
}

CMD_FUNC(cmd_svs2nolag)
{
	do_svsnolag(client, parc, parv, 1);
}
