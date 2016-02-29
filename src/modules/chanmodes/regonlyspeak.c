/*
 * Only registered users can speak UnrealIRCd Module (Channel Mode +M)
 * (C) Copyright 2014 Travis McArthur (Heero) and the UnrealIRCd team
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


ModuleHeader MOD_HEADER(regonlyspeak)
  = {
	"chanmodes/regonlyspeak",
	"4.0",
	"Channel Mode +M",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_REGONLYSPEAK;
static char errMsg[2048];

#define IsRegOnlySpeak(chptr)    (chptr->mode.extmode & EXTCMODE_REGONLYSPEAK)

DLLFUNC int regonlyspeak_can_send (aClient* cptr, aChannel *chptr, char* message, Membership* lp, int notice);
DLLFUNC char * regonlyspeak_part_message (aClient* sptr, aChannel *chptr, char* comment);

MOD_TEST(regonlyspeak)
{
	return MOD_SUCCESS;
}

MOD_INIT(regonlyspeak)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'M';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGONLYSPEAK);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND, 0, regonlyspeak_can_send);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, regonlyspeak_part_message);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(regonlyspeak)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(regonlyspeak)
{
	return MOD_SUCCESS;
}

DLLFUNC char * regonlyspeak_part_message (aClient* sptr, aChannel *chptr, char* comment)
{

	if (IsRegOnlySpeak(chptr) && !IsLoggedIn(sptr) && !ValidatePermissionsForPath("immune:regonly",sptr,NULL,NULL,NULL))
				return NULL;

	return comment;
}

DLLFUNC int regonlyspeak_can_send (aClient* cptr, aChannel *chptr, char* message, Membership* lp, int notice)
{

	if (IsRegOnlySpeak(chptr) && !op_can_override("override:message:regonlyspeak",cptr,chptr,NULL) && !IsLoggedIn(cptr) &&
		    (!lp
		    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
		    CHFL_HALFOP | CHFL_CHANPROT))))
			return CANNOT_SEND_MODREG;

	return HOOK_CONTINUE;
}


