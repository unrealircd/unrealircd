/*
 *   Unreal Internet Relay Chat Daemon, src/modules/sreply.c
 *   (C) 2022 Valware and the UnrealIRCd Team
 * 
 *   Allows services to send Standard Replies in response to non-privmsg commands:
 *   https://ircv3.net/specs/extensions/standard-replies
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

CMD_FUNC(cmd_sreply);

/* Place includes here */
#define MSG_SREPLY       "SREPLY"

ModuleHeader MOD_HEADER
  = {
	"sreply",	/* Name of module */
	"1.0", /* Version */
	"Server command SREPLY", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SREPLY, cmd_sreply, 3, CMD_SERVER);
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

/**
 * cmd_sreply
 * @param parv[1]		Nick|UID
 * @param parv[2]		"F", "W" or "N" for FAIL, WARN and NOTE.
 * @param parv[3]		The rest of the message
*/
CMD_FUNC(cmd_sreply)
{
	Client *target;

	if ((parc < 4) || !(target = find_user(parv[1], NULL)))
		return;

	if (MyUser(target))
	{
		if (!strcmp(parv[2],"F"))
			sendto_one(target, recv_mtags, "FAIL %s", parv[3]);

		else if (!strcmp(parv[2],"W"))
			sendto_one(target, recv_mtags, "WARN %s", parv[3]);

		else if (!strcmp(parv[2],"N"))
			sendto_one(target, recv_mtags, "NOTE %s", parv[3]);

		else // error
			return;
	}
	else
		sendto_server(client, 0, 0, recv_mtags, ":%s %s %s %s %s", client->name, MSG_SREPLY, parv[1], parv[2], parv[3]);
}
