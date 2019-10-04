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
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_USER, cmd_user, 4, CMD_USER|CMD_UNREGISTERED);
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

/*
** cmd_user
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
**
** NOTE: Be advised that multiple USER messages are possible,
**       hence, always check if a certain struct is already allocated... -- Syzop
*/
CMD_FUNC(cmd_user)
{
	char *username;
	char *host;
	char *server;
	char *realname;
	char *umodex = NULL;
	char *virthost = NULL;
	char *ip = NULL;
	char *sstamp = NULL;
	Client *cptr = client->direction; /* Lazyness, since this function should be rewritten anyway */

	// Eh, this is for old remote USER shit (which we currently still use indirectly via do_cmd).
	// TODO: cleanup. Also, hope this is right:
	if (!MyConnect(client) && !IsUnknown(client))
		return;

	if (MyConnect(client) && (client->local->listener->options & LISTENER_SERVERSONLY))
		return exit_client(client, NULL, "This port is for servers only");

	if (parc > 2 && (username = strchr(parv[1], '@')))
		*username = '\0';

	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
			    client->name, get_client_name(cptr, FALSE));
		else
			return;
	}


	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];

	/* This we can remove as soon as all servers have upgraded. */

	if (parc == 6 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[5])) ? "<bad-realname>" : parv[5];
		umodex = NULL;
	}
	else if (parc == 8 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[7])) ? "<bad-realname>" : parv[7];
		umodex = parv[5];
		virthost = parv[6];
	}
	else if (parc == 9 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[8])) ? "<bad-realname>" : parv[8];
		umodex = parv[5];
		virthost = parv[6];
		ip = parv[7];
	}
	else if (parc == 10 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[9])) ? "<bad-realname>" : parv[9];
		umodex = parv[5];
		virthost = parv[6];
		ip = parv[8];
	}
	else
	{
		realname = (BadPtr(parv[4])) ? "<bad-realname>" : parv[4];
	}
	
	make_user(client);

	if (!MyConnect(client))
	{
		if (client->srvptr == NULL)
			sendto_ops("WARNING, User %s introduced as being "
			    "on non-existant server %s.", client->name, server);
		client->user->server = find_or_add(client->srvptr->name);
		strlcpy(client->user->realhost, host, sizeof(client->user->realhost));
		goto user_finish;
	}

	if (!IsUnknown(client))
	{
		sendnumeric(client, ERR_ALREADYREGISTRED);
		return;
	}

	if (!IsServer(cptr))
	{
		/* set::modes-on-connect */
		client->umodes |= CONN_MODES;
	}

	client->user->server = me_hash;
      user_finish:
	if (sstamp != NULL && *sstamp != '*')
		strlcpy(client->user->svid, sstamp, sizeof(client->user->svid));

	strlcpy(client->info, realname, sizeof(client->info));
	strlcpy(client->user->username, username, USERLEN + 1);

	if (*client->name &&
		(IsServer(cptr) || is_handshake_finished(cptr))
           )
		/* NICK and no-spoof already received, now we have USER... */
	{
		if (USE_BAN_VERSION && MyConnect(client))
			sendto_one(client, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
				me.name, client->name);
		if (strlen(username) > USERLEN)
			username[USERLEN] = '\0'; /* cut-off */
		register_user(client, client->name, username, umodex, virthost, ip);
		return;
	}
}
