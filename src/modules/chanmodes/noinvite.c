/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/noinvite.c
 * Disallow invites UnrealIRCd Module (Channel Mode +V)
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

CMD_FUNC(noinvite);

ModuleHeader MOD_HEADER
  = {
	"chanmodes/noinvite",
	"4.2",
	"Channel Mode +V",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_NOINVITE;

#define IsNoInvite(channel)    (channel->mode.mode & EXTCMODE_NOINVITE)

int noinvite_pre_knock(Client *client, Channel *channel, const char **reason);
int noinvite_pre_invite(Client *client, Client *target, Channel *channel, int *override);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'V';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOINVITE);
	
	HookAdd(modinfo->handle, HOOKTYPE_PRE_KNOCK, 0, noinvite_pre_knock);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_INVITE, 0, noinvite_pre_invite);
	
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


int noinvite_pre_knock(Client *client, Channel *channel, const char **reason)
{
	if (MyUser(client) && IsNoInvite(channel))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, channel->name,
		            "The channel does not allow invites (+V)");
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}

int noinvite_pre_invite(Client *client, Client *target, Channel *channel, int *override)
{
	if (MyUser(client) && IsNoInvite(channel))
	{
		if (ValidatePermissionsForPath("channel:override:invite:noinvite",client,NULL,channel,NULL) && client == target)
		{
			*override = 1;
		} else {
			sendnumeric(client, ERR_NOINVITE, channel->name);
			return HOOK_DENY;
		}
	}

	return HOOK_CONTINUE;
}
