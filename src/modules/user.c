/*
 *   IRC - Internet Relay Chat, src/modules/user.c
 *   (C) 2005 The UnrealIRCd Team
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

CMD_FUNC(cmd_user);

#define MSG_USER 	"USER"	

ModuleHeader MOD_HEADER
  = {
	"user",
	"5.0",
	"command /user", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_USER, cmd_user, 4, CMD_UNREGISTERED);
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

/** The USER command, together with NICK this will register a user.
 * As per UnrealIRCd 5 this command is only available to local clients.
 * Intraserver traffic is handled through the UID command.
 *	parv[1] = username
 *	parv[2] = client host name (ignored)
 *	parv[3] = server host name (ignored)
 *	parv[4] = real name / gecos
 *
 * NOTE: Be advised that multiple USER messages are possible,
 *       hence, always check if a certain struct is already allocated... -- Syzop
 */
CMD_FUNC(cmd_user)
{
	const char *username;
	const char *realname;
	char *p;

	if (!MyConnect(client) || IsServer(client))
		return;

	if (MyConnect(client) && (client->local->listener->options & LISTENER_SERVERSONLY))
	{
		exit_client(client, NULL, "This port is for servers only");
		return;
	}

	if ((parc < 5) || BadPtr(parv[4]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "USER");
		return;
	}

	username = parv[1];
	realname = parv[4];
	
	make_user(client);

	/* set::modes-on-connect */
	client->umodes |= get_setting_for_user_number(client, SET_MODES_ON_CONNECT);
	client->user->server = me_hash;
	strlcpy(client->info, realname, sizeof(client->info));
	strlcpy(client->user->username, username, sizeof(client->user->username));

	/* This cuts the username off at @, uh okay.. */
	if ((p = strchr(client->user->username, '@')))
		*p = '\0';

	if (*client->name && is_handshake_finished(client))
	{
		/* NICK and no-spoof already received, now we have USER... */
		if (USE_BAN_VERSION && MyConnect(client))
		{
			sendto_one(client, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
				me.name, client->name);
		}
		register_user(client);
		return;
	}
}
