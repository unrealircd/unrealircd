/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/nokick.c
 * No kicks in channel UnrealIRCd Module (Channel Mode +Q)
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
	"chanmodes/nokick",
	"4.2",
	"Channel Mode +Q",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_NOKICK;

#define IsNoKick(channel)    (channel->mode.mode & EXTCMODE_NOKICK)

int nokick_check (Client *client, Client *target, Channel *channel, const char *comment, const char *client_member_modes, const char *target_member_modes, const char **reject_reason);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'Q';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOKICK);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_KICK, 0, nokick_check);

	
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

int nokick_check (Client *client, Client *target, Channel *channel, const char *comment, const char *client_member_modes, const char *target_member_modes, const char **reject_reason)
{
	static char errmsg[256];

	if (MyUser(client) && IsNoKick(channel))
	{
		ircsnprintf(errmsg, sizeof(errmsg), ":%s %d %s %s :%s",
		            me.name, ERR_CANNOTDOCOMMAND, client->name,
		            "KICK", "channel is +Q");
		*reject_reason = errmsg;
		return EX_DENY; /* Deny, but let opers override if necessary. */
	}

	return EX_ALLOW;
}

