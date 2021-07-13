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

ModuleHeader MOD_HEADER
  = {
	"chanmodes/operonly",
	"4.2",
	"Channel Mode +O",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

Cmode_t EXTCMODE_OPERONLY;

int operonly_require_oper(Client *client, Channel *channel, char mode, char *para, int checkt, int what);
int operonly_check (Client *client, Channel *channel, char *key, char *parv[]);
int operonly_topic_allow (Client *client, Channel *channel);
int operonly_check_ban(Client *client, Channel *channel);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
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

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int operonly_check (Client *client, Channel *channel, char *key, char *parv[])
{
	if ((channel->mode.extmode & EXTCMODE_OPERONLY) && !ValidatePermissionsForPath("channel:operonly:join",client,NULL,channel,NULL))
		return ERR_OPERONLY;
	return 0;
}

int operonly_check_ban(Client *client, Channel *channel)
{
	 if ((channel->mode.extmode & EXTCMODE_OPERONLY) &&
		    !ValidatePermissionsForPath("channel:operonly:ban",client,NULL,NULL,NULL))
		 return HOOK_DENY;

	 return HOOK_CONTINUE;
}

int operonly_topic_allow (Client *client, Channel *channel)
{
	if (channel->mode.extmode & EXTCMODE_OPERONLY && !ValidatePermissionsForPath("channel:operonly:topic",client,NULL,channel,NULL))
		return HOOK_DENY;

	return HOOK_CONTINUE;
}

int operonly_require_oper(Client *client, Channel *channel, char mode, char *para, int checkt, int what)
{
	if (!MyUser(client) || ValidatePermissionsForPath("channel:operonly:set",client,NULL,channel,NULL))
		return EX_ALLOW;

	if (checkt == EXCHK_ACCESS_ERR)
		sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'O', "You are not an IRC operator");

	return EX_DENY;
}

