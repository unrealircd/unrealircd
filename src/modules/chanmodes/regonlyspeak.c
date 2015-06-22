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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif


ModuleHeader MOD_HEADER(regonlyspeak)
  = {
	"chanmodes/regonlyspeak",
	"$Id$",
	"Channel Mode +M",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_REGONLYSPEAK;
static char errMsg[2048];

#define IsRegOnlySpeak(chptr)    (chptr->mode.extmode & EXTCMODE_REGONLYSPEAK)

DLLFUNC int regonlyspeak_can_send (aClient* cptr, aChannel *chptr, char* message, Membership* lp, int notice);
DLLFUNC char * regonlyspeak_part_message (aClient* sptr, aChannel *chptr, char* comment);

DLLFUNC int MOD_TEST(regonlyspeak)(ModuleInfo *modinfo)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(regonlyspeak)(ModuleInfo *modinfo)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'M';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGONLYSPEAK);
	
	HookAddEx(modinfo->handle, HOOKTYPE_CAN_SEND, regonlyspeak_can_send);
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, regonlyspeak_part_message);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(regonlyspeak)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(regonlyspeak)(int module_unload)
{
	return MOD_SUCCESS;
}

DLLFUNC char * regonlyspeak_part_message (aClient* sptr, aChannel *chptr, char* comment)
{

	if (IsRegOnlySpeak(chptr) && !IsLoggedIn(sptr) && !IsAnOper(sptr))
				return NULL;

	return comment;
}

DLLFUNC int regonlyspeak_can_send (aClient* cptr, aChannel *chptr, char* message, Membership* lp, int notice)
{

	if (IsRegOnlySpeak(chptr) && !op_can_override(cptr) && !IsLoggedIn(cptr) &&
		    (!lp
		    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
		    CHFL_HALFOP | CHFL_CHANPROT))))
			return CANNOT_SEND_MODREG;

	return HOOK_CONTINUE;
}


