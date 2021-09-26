/*
 *   Unreal Internet Relay Chat Daemon, src/modules/sendumode.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

CMD_FUNC(cmd_sendumode);

/* Place includes here */
#define MSG_SENDUMODE   "SENDUMODE"
#define MSG_SMO         "SMO"

ModuleHeader MOD_HEADER
  = {
	"sendumode",	/* Name of module */
	"5.0", /* Version */
	"command /sendumode", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SENDUMODE, cmd_sendumode, MAXPARA, CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_SMO, cmd_sendumode, MAXPARA, CMD_SERVER);
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

/** SENDUMODE - Send to usermode command (S2S traffic only).
 * parv[1] = target user modes
 * parv[2] = message text
 * For example:
 * :server SENDUMODE o :Serious problem: blablabla
 */
CMD_FUNC(cmd_sendumode)
{
	MessageTag *mtags = NULL;
	Client *acptr;
	const char *message;
	const char *p;
	int i;
	long umode_s = 0;

	message = (parc > 3) ? parv[3] : parv[2];

	if (parc < 3)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SENDUMODE");
		return;
	}

	new_message(client, recv_mtags, &mtags);

	sendto_server(client, 0, 0, mtags, ":%s SENDUMODE %s :%s", client->id, parv[1], message);

	umode_s = set_usermode(parv[1]);

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->umodes & umode_s)
			sendto_one(acptr, mtags, ":%s NOTICE %s :%s", client->name, acptr->name, message);
	}

	free_message_tags(mtags);
}
