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
	if (!IsUser(client)) // we don't want anything to do with pre-connect
		return;
	if (!IsLoggedIn(client))
	{
		sendto_one(client, NULL, "FAIL LOGOUT NOT_LOGGED_IN :You are not logged in");
		return;
	}

	strlcpy(client->user->account, "0", sizeof(client->user->account));
	user_account_login(recv_mtags, client);
	
	/* We are sending this out with SVSLOGIN because this is the current best way to tell other
	 * servers including services that the user is no longer logged in.
	 */
	sendto_server(client, 0, 0, NULL, ":%s SVSLOGIN * %s 0", me.id, client->name);
}
