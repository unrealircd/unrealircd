/*
 * Prevents you from being kicked (User mode +q)
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

#define IsNokick(cptr)    (cptr->umodes & UMODE_NOKICK)

/* Module header */
ModuleHeader MOD_HEADER(nokick)
  = {
	"usermodes/nokick",
	"$Id$",
	"User Mode +q",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long UMODE_NOKICK = 0L;

/* Forward declarations */
int nokick_can_kick(aClient *sptr, aClient *target, aChannel *chptr,
                    char *comment, long sptr_flags, long target_flags, char **reject_reason);

DLLFUNC int MOD_TEST(nokick)(ModuleInfo *modinfo)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(nokick)(ModuleInfo *modinfo)
{
	UmodeAdd(modinfo->handle, 'q', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_NOKICK); // TODO: limit more!!
	
	HookAddEx(modinfo->handle, HOOKTYPE_CAN_KICK, nokick_can_kick);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(nokick)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(nokick)(int module_unload)
{
	return MOD_SUCCESS;
}

int nokick_can_kick(aClient *sptr, aClient *target, aChannel *chptr, char *comment,
                    long sptr_flags, long target_flags, char **reject_reason)
{
	static char errmsg[NICKLEN+32];

	if (IsNokick(target) && !IsULine(sptr) && MyClient(sptr) && !OperClass_evaluateACLPath("override:kick:nokick",sptr,target,chptr,NULL))
	{
		ircsnprintf(errmsg, sizeof(errmsg), err_str(ERR_CANNOTDOCOMMAND),
				   me.name, sptr->name, "KICK",
				   "user is unkickable (user mode +q)");

		*reject_reason = errmsg;

		sendnotice(target,
			"*** umode q: %s tried to kick you from channel %s (%s)",
			sptr->name, chptr->chname, comment);
		
		return EX_ALWAYS_DENY;
	}

	return EX_ALLOW;
}
