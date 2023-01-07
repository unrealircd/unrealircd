/*
 *   Unreal Internet Relay Chat Daemon, src/modules/svskill.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_svskill);

#define MSG_SVSKILL	"SVSKILL"

ModuleHeader MOD_HEADER
  = {
	"svskill",	/* Name of module */
	"5.0", /* Version */
	"command /svskill", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };


/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSKILL, cmd_svskill, MAXPARA, CMD_SERVER|CMD_USER);
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

/*
** cmd_svskill
**	parv[1] = client
**	parv[2] = kill message
*/
CMD_FUNC(cmd_svskill)
{
	MessageTag *mtags = NULL;
	Client *target;
	const char *comment = "SVS Killed";
	int n;

	if (parc < 2)
		return;
	if (parc > 3)
		return;
	if (parc == 3)
		comment = parv[2];

	if (!IsSvsCmdOk(client))
		return;

	if (!(target = find_user(parv[1], NULL)))
		return;

	/* for new_message() we use target here, makes sense for the exit_client, right? */
	new_message(target, recv_mtags, &mtags);
	sendto_server(client, 0, 0, mtags, ":%s SVSKILL %s :%s", client->id, target->id, comment);
	SetKilled(target);
	exit_client(target, mtags, comment);
	free_message_tags(mtags);
}
