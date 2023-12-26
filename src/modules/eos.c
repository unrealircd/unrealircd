/*
 *   IRC - Internet Relay Chat, src/modules/eos.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_eos);

#define MSG_EOS 	"EOS"	

ModuleHeader MOD_HEADER
  = {
	"eos",
	"5.0",
	"command /eos", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_EOS, cmd_eos, MAXPARA, CMD_SERVER);
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
 * EOS (End Of Sync) command.
 * Type: Broadcast
 * Purpose: Broadcasted over a network if a server is synced (after the users, channels,
 *          etc are introduced). Makes us able to know if a server is linked.
 * History: Added in beta18 (in cvs since 2003-08-11) by Syzop
 */
CMD_FUNC(cmd_eos)
{
	if (!IsServer(client))
		return;
	client->server->flags.synced = 1;
	/* pass it on ^_- */
	sendto_server(client, 0, 0, NULL, ":%s EOS", client->id);

	RunHook(HOOKTYPE_SERVER_SYNCED, client);
}
