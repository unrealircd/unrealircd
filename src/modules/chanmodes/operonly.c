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

#include "unrealircd.h"

CMD_FUNC(operonly);

ModuleHeader MOD_HEADER(operonly)
  = {
	"chanmodes/operonly",
	"4.0",
	"Channel Mode +O",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_OPERONLY;

DLLFUNC int operonly_require_oper(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what);
DLLFUNC int operonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[]);
DLLFUNC int operonly_topic_allow (aClient *sptr, aChannel *chptr);
DLLFUNC int operonly_check_ban(aClient *cptr, aChannel *chptr);

MOD_TEST(operonly)
{
	return MOD_SUCCESS;
}

MOD_INIT(operonly)
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'O';
	req.is_ok = operonly_require_oper;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_OPERONLY);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, operonly_check);
	HookAdd(modinfo->handle, HOOKTYPE_OPER_INVITE_BAN, 0, operonly_check_ban);
	HookAdd(modinfo->handle, HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL, 0, operonly_topic_allow);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(noctcp)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(noctcp)
{
	return MOD_SUCCESS;
}

DLLFUNC int operonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[])
{
	if ((chptr->mode.extmode & EXTCMODE_OPERONLY) && !ValidatePermissionsForPath("channel:operonly",cptr,NULL,chptr,NULL))
		return ERR_OPERONLY;
	return 0;
}

DLLFUNC int operonly_check_ban(aClient *cptr, aChannel *chptr)
{
	 if ((chptr->mode.extmode & EXTCMODE_OPERONLY) &&
		    !ValidatePermissionsForPath("override:ban:operonly",cptr,NULL,NULL,NULL))
		 return HOOK_DENY;

	 return HOOK_CONTINUE;
}

DLLFUNC int operonly_topic_allow (aClient *sptr, aChannel *chptr)
{
	if (chptr->mode.extmode & EXTCMODE_OPERONLY && !ValidatePermissionsForPath("channel:operonly:topic",sptr,NULL,chptr,NULL))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

DLLFUNC int operonly_require_oper(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	if (!MyClient(cptr) || ValidatePermissionsForPath("channel:operonly",cptr,NULL,chptr,NULL))
		return EX_ALLOW;


	sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE),
					me.name, cptr->name, 'O', "You are not an IRC operator");
	return EX_DENY;
}

