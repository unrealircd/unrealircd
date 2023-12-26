/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/regonlymsg.c
 * Recieve private messages only from registered users (User mode +R)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

#define IsRegOnlyMsg(client)    (client->umodes & UMODE_REGONLYMSG)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/regonlymsg",
	"4.2",
	"User Mode +R",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_REGONLYMSG = 0L;

/* Forward declarations */
int regonlymsg_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);
                    
MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'R', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_REGONLYMSG);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, regonlymsg_can_send_to_user);
	
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

int regonlymsg_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	if (IsRegOnlyMsg(target) && !IsServer(client) && !IsULine(client) && !IsLoggedIn(client))
	{
		if (ValidatePermissionsForPath("client:override:message:regonlymsg",client,target,NULL,text?*text:NULL))
			return HOOK_CONTINUE; /* bypass this restriction */

		*errmsg = "You must identify to a registered nick to private message this user";
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}
