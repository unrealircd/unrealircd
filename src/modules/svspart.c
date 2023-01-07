/*
 *   Unreal Internet Relay Chat Daemon, src/modules/svspart.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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

CMD_FUNC(cmd_svspart);

#define MSG_SVSPART       "SVSPART"

ModuleHeader MOD_HEADER
  = {
	"svspart",	/* Name of module */
	"5.0", /* Version */
	"command /svspart", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSPART, cmd_svspart, 3, CMD_USER|CMD_SERVER);
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

/* cmd_svspart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
  Modified for PART by Stskeeps
	parv[1] - nick to make part
	parv[2] - channel(s) to part
	parv[3] - comment
*/
CMD_FUNC(cmd_svspart)
{
	Client *target;
	const char *comment = (parc > 3 && parv[3] ? parv[3] : NULL);
	if (!IsSvsCmdOk(client))
		return;

	if (parc < 3 || !(target = find_user(parv[1], NULL))) 
		return;

	if (MyUser(target))
	{
		parv[0] = target->name;
		parv[1] = parv[2];
		parv[2] = comment;
		parv[3] = NULL;
		do_cmd(target, NULL, "PART", comment ? 3 : 2, parv);
		/* NOTE: target may be killed now by spamfilter due to the part reason */
	}
	else
	{
		if (comment)
			sendto_one(target, NULL, ":%s SVSPART %s %s :%s", client->name,
			    parv[1], parv[2], parv[3]);
		else
			sendto_one(target, NULL, ":%s SVSPART %s %s", client->name,
			    parv[1], parv[2]);
	}
}
