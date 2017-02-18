/*
 * Recieve private messages only from SSL/TLS users (User mode +Z)
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

#define IsSecureOnlyMsg(cptr)    (cptr->umodes & UMODE_SECUREONLYMSG)

/* Module header */
ModuleHeader MOD_HEADER(secureonlymsg)
  = {
	"usermodes/secureonlymsg",
	"4.0",
	"User Mode +Z",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long UMODE_SECUREONLYMSG = 0L;

/* Forward declarations */
char *secureonlymsg_pre_usermsg(aClient *sptr, aClient *target, char *text, int notice);
                    
MOD_INIT(secureonlymsg)
{
	UmodeAdd(modinfo->handle, 'Z', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_SECUREONLYMSG);
	
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, 0, secureonlymsg_pre_usermsg);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(secureonlymsg)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(secureonlymsg)
{
	return MOD_SUCCESS;
}

char *secureonlymsg_pre_usermsg(aClient *sptr, aClient *target, char *text, int notice)
{
	if (IsSecureOnlyMsg(target) && !IsServer(sptr) && !IsULine(sptr) && !IsSecure(sptr))
	{
		if (ValidatePermissionsForPath("override:message:secureonly",sptr,target,NULL,text))
			return text; /* TODO: this is actually an override */

		/* A numeric is preferred to indicate the user cannot message.
		 * 492 is ERR_NOCTCP but apparently is also used by some other ircd(s) as a
		 * general "cannot send message" numeric (similar to the generic
		 * ERR_CANNOTSENDTOCHAN for channel messaging).
		 */
		sendto_one(sptr, ":%s 492 %s :Cannot send to user %s (You must be connected via SSL/TLS to message this user)",
			me.name, sptr->name, target->name);

		return NULL; /* Block the message */
	} else
	if (IsSecureOnlyMsg(sptr) && !IsSecure(target) && !IsULine(target))
	{
		if (ValidatePermissionsForPath("override:message:secureonly",sptr,target,NULL,text))
			return text; /* TODO: this is actually an override */
		
		/* Similar to above but in this case we are +Z and are trying to message
		 * an SSL user (who does not have +Z set, note the 'else'). This does not
		 * make sense since they could never message back to us. Better block the
		 * message than leave the user confused.
		 */
		sendto_one(sptr, ":%s 492 %s :Cannot send to user %s (You have user mode +Z set but are not connected via SSL/TLS)",
			me.name, sptr->name, target->name);
	}

	return text;
}
