/*
 * Disallow knocks UnrealIRCd Module (Channel Mode +K)
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

DLLFUNC CMD_FUNC(adminonly);

ModuleHeader MOD_HEADER(adminonly)
  = {
	"chanmodes/adminonly",
	"$Id$",
	"Channel Mode +V",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_ADMINONLY;

#define IsAdminOnly(chptr)    (chptr->mode.extmode & EXTCMODE_ADMINONLY)

DLLFUNC int adminonly_require_admin(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what);
DLLFUNC int adminonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[]);
DLLFUNC int adminonly_topic_allow (aClient *sptr, aChannel *chptr);
DLLFUNC int adminonly_check_ban(aClient *cptr, aChannel *chptr);

DLLFUNC int MOD_TEST(adminonly)(ModuleInfo *modinfo)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(adminonly)(ModuleInfo *modinfo)
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'A';
	req.is_ok = adminonly_require_admin;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_ADMINONLY);
	
	HookAddEx(modinfo->handle, HOOKTYPE_CAN_JOIN, adminonly_check);
	HookAddEx(modinfo->handle, HOOKTYPE_OPER_INVITE_BAN, adminonly_check_ban);
	HookAddEx(modinfo->handle, HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEl, adminonly_topic_allow);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(noctcp)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(noctcp)(int module_unload)
{
	return MOD_SUCCESS;
}

DLLFUNC int adminonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[])
{
	if ((chptr->mode.extmode & EXTCMODE_ADMINONLY) && !IsSkoAdmin(cptr))
		return ERR_ADMONLY;
	return 0;
}


DLLFUNC int adminonly_check_ban(aClient *cptr, aChannel *chptr)
{

	 if ((chptr->mode.extmode & EXTCMODE_ADMINONLY) && IsAnOper(cptr) && !IsNetAdmin(cptr) && !IsSAdmin(cptr))
		 return HOOK_DENY;

	 return HOOK_CONTINUE;
}

DLLFUNC int adminonly_topic_allow (aClient *sptr, aChannel *chptr)
{
	if (chptr->mode.extmode & EXTCMODE_ADMINONLY && !IsAdmin(sptr))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

DLLFUNC int adminonly_require_admin(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	if (!MyClient(cptr) || IsSkoAdmin(cptr))
		return EX_ALLOW;


	sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE),
					me.client.name, cptr->name, 'A', "You are not an Admin");
	return EX_DENY;
}

