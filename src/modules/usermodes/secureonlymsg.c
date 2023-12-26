/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/secureonlymsg.c
 * Recieve private messages only from TLS users (User mode +Z)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * Idea from "Stealth" <stealth@x-tab.org>
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

#define IsSecureOnlyMsg(client)    (client->umodes & UMODE_SECUREONLYMSG)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/secureonlymsg",
	"4.2",
	"User Mode +Z",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_SECUREONLYMSG = 0L;

/* Forward declarations */
int secureonlymsg_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);
                    
MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'Z', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_SECUREONLYMSG);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, secureonlymsg_can_send_to_user);
	
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

int secureonlymsg_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	if (IsSecureOnlyMsg(target) && !IsServer(client) && !IsULine(client) && !IsSecureConnect(client))
	{
		if (ValidatePermissionsForPath("client:override:message:secureonlymsg",client,target,NULL,text?*text:NULL))
			return HOOK_CONTINUE; /* bypass this restriction */

		*errmsg = "You must be connected via TLS to message this user";
		return HOOK_DENY;
	} else
	if (IsSecureOnlyMsg(client) && !IsSecureConnect(target) && !IsULine(target))
	{
		if (ValidatePermissionsForPath("client:override:message:secureonlymsg",client,target,NULL,text?*text:NULL))
			return HOOK_CONTINUE; /* bypass this restriction */
		
		/* Similar to above but in this case we are +Z and are trying to message
		 * a secure user (who does not have +Z set, note the 'else'). This does not
		 * make sense since they could never message back to us. Better block the
		 * message than leave the user confused.
		 */
		*errmsg = "Recipient is not connected via TLS and you are +Z";
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}
