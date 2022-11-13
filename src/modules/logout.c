/*
 *   Unreal Internet Relay Chat Daemon, src/modules/logout.c
 *   (C) 2022 Valware and the UnrealIRCd Team
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

CMD_FUNC(cmd_logout);

/* Place includes here */
#define MSG_LOGOUT "LOGOUT"

ModuleHeader MOD_HEADER
  = {
	"logout",	/* Name of module */
	"1.0", /* Version */
	"command /LOGOUT", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
};

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_LOGOUT, cmd_logout, MAXPARA, CMD_USER|CMD_SERVER);
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

CMD_FUNC(cmd_logout)
{
	Client *nickserv = find_user("NickServ", NULL);
	Client *server = find_server(iConf.services_name, NULL);

	if (!IsUser(client)) // we don't want anything to do with pre-connect
		return;
	if (!IsLoggedIn(client))
	{
		sendto_one(client, NULL, "FAIL LOGOUT NOT_LOGGED_IN :You are not logged in");
		return;
	}
	
	/* If the server does not exist, still log them out. Services will see they're not logged in when they burst back */
	if (!server)
	{
		strlcpy(client->user->account, "0", sizeof(client->user->account));
		user_account_login(recv_mtags, client);
		
		/* We are sending this out with SVSLOGIN because this is the current best way to tell other
		* servers including services that the user is no longer logged in.
		*/
		sendto_server(client, 0, 0, NULL, ":%s SVSLOGIN * %s 0", me.id, client->name);
	}
	
	/* If NickServ is online and also part of the services server, ask them*/
	else if (nickserv && nickserv->uplink == server)
		sendto_one(nickserv, recv_mtags, ":%s PRIVMSG %s :LOGOUT", client->id, nickserv->id);

	/* Well, we tried. Can't verify any valid NickServ, sorry
	 * It would be good if the services pseudoserver supported a logout S2S or incoming SVSLOGIN
	 * But seeing as it doesn't, we're forced to rely on messaging a bot which may or may not be called
	 * NickServ.
	 */
	else
		sendto_one(client, NULL, "FAIL LOGOUT NICKSERV_NOT_FOUND :Could not find NickServ.");

}

