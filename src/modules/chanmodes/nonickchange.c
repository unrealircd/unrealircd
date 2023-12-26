/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/nonickchange.c
 * No nick changes in channel UnrealIRCd Module (Channel Mode +N)
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
	"chanmodes/nonickchange",
	"4.2",
	"Channel Mode +N",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_NONICKCHANGE;

#define IsNoNickChange(channel)    (channel->mode.mode & EXTCMODE_NONICKCHANGE)

int nonickchange_check (Client *client, Channel *channel);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'N';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NONICKCHANGE);
	
	HookAdd(modinfo->handle, HOOKTYPE_CHAN_PERMIT_NICK_CHANGE, 0, nonickchange_check);

	
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

int nonickchange_check (Client *client, Channel *channel)
{
	if (!IsOper(client) && !IsULine(client)
		&& IsNoNickChange(channel)
		&& !check_channel_access(client, channel, "oaq"))
	{
		return HOOK_DENY;
	}

	return HOOK_ALLOW;
}

