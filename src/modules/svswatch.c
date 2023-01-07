/*
 *   Unreal Internet Relay Chat Daemon, src/modules/svswatch.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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

CMD_FUNC(cmd_svswatch);

/* Place includes here */
#define MSG_SVSWATCH       "SVSWATCH"

ModuleHeader MOD_HEADER
  = {
	"svswatch",	/* Name of module */
	"5.0", /* Version */
	"command /svswatch", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSWATCH, cmd_svswatch, MAXPARA, CMD_SERVER);
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

/* cmd_svswatch() - written by Syzop, suggested by Griever.
 * parv[1] - target nick
 * parv[2] - parameters
 */
CMD_FUNC(cmd_svswatch)
{
	Client *target;

	if (!IsSvsCmdOk(client))
		return;

	if (parc < 3 || BadPtr(parv[2]) || !(target = find_user(parv[1], NULL)))
		return;

	if (MyUser(target))
	{
		parv[0] = target->name;
		parv[1] = parv[2];
		parv[2] = NULL;
		do_cmd(target, NULL, "WATCH", 2, parv);
	}
	else
		sendto_one(target, NULL, ":%s SVSWATCH %s :%s", client->name, parv[1], parv[2]);
}
