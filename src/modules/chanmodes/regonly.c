/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/regonly.c
 * Registered users only UnrealIRCd Module (Channel Mode +R)
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


ModuleHeader MOD_HEADER
  = {
	"chanmodes/regonly",
	"4.2",
	"Channel Mode +R",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_REGONLY;

#define IsRegOnly(channel)    (channel->mode.mode & EXTCMODE_REGONLY)

int regonly_check(Client *client, Channel *channel, const char *key, char **errmsg);


MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'R';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGONLY);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, regonly_check);

	
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

int regonly_check (Client *client, Channel *channel, const char *key, char **errmsg)
{
	if (IsRegOnly(channel) && !IsLoggedIn(client))
	{
		*errmsg = STR_ERR_NEEDREGGEDNICK;
		return ERR_NEEDREGGEDNICK;
	}
	return 0;
}

