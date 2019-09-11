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
	"4.2",
	"Channel Mode +O",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

Cmode_t EXTCMODE_OPERONLY;

int operonly_require_oper(Client *cptr, Channel *chptr, char mode, char *para, int checkt, int what);
int operonly_check (Client *cptr, Channel *chptr, char *key, char *parv[]);
int operonly_topic_allow (Client *sptr, Channel *chptr);
int operonly_check_ban(Client *cptr, Channel *chptr);

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

int operonly_check (Client *cptr, Channel *chptr, char *key, char *parv[])
{
	if ((chptr->mode.extmode & EXTCMODE_OPERONLY) && !ValidatePermissionsForPath("channel:operonly:join",cptr,NULL,chptr,NULL))
		return ERR_OPERONLY;
	return 0;
}

int operonly_check_ban(Client *cptr, Channel *chptr)
{
	 if ((chptr->mode.extmode & EXTCMODE_OPERONLY) &&
		    !ValidatePermissionsForPath("channel:operonly:ban",cptr,NULL,NULL,NULL))
		 return HOOK_DENY;

	 return HOOK_CONTINUE;
}

int operonly_topic_allow (Client *sptr, Channel *chptr)
{
	if (chptr->mode.extmode & EXTCMODE_OPERONLY && !ValidatePermissionsForPath("channel:operonly:topic",sptr,NULL,chptr,NULL))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

int operonly_require_oper(Client *cptr, Channel *chptr, char mode, char *para, int checkt, int what)
{
	if (!MyUser(cptr) || ValidatePermissionsForPath("channel:operonly:set",cptr,NULL,chptr,NULL))
		return EX_ALLOW;

	if (checkt == EXCHK_ACCESS_ERR)
		sendnumeric(cptr, ERR_CANNOTCHANGECHANMODE, 'O', "You are not an IRC operator");

	return EX_DENY;
}

