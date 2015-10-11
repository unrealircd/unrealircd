/*
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

#define IsRegOnlyMsg(cptr)    (cptr->umodes & UMODE_REGONLYMSG)

/* Module header */
ModuleHeader MOD_HEADER(regonlymsg)
  = {
	"usermodes/regonlymsg",
	"4.0",
	"User Mode +R",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long UMODE_REGONLYMSG = 0L;

/* Forward declarations */
char *regonlymsg_pre_usermsg(aClient *sptr, aClient *target, char *text, int notice);
                    
MOD_INIT(regonlymsg)
{
	UmodeAdd(modinfo->handle, 'R', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_REGONLYMSG);
	
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, 0, regonlymsg_pre_usermsg);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(regonlymsg)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(regonlymsg)
{
	return MOD_SUCCESS;
}

char *regonlymsg_pre_usermsg(aClient *sptr, aClient *target, char *text, int notice)
{
	if (IsRegOnlyMsg(target) && !IsServer(sptr) && !IsULine(sptr) && !IsLoggedIn(sptr))
	{
		if (ValidatePermissionsForPath("override:message:regonly",sptr,target,NULL,text))
			return text; /* TODO: this is actually an override */

		sendto_one(sptr, err_str(ERR_NONONREG), me.name, sptr->name, target->name);

		return NULL; /* Block the message */
	}

	return text;
}
