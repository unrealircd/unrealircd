/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/inviteonly.c
 * Channel Mode +i
 * (C) Copyright 2021 Syzop and the UnrealIRCd team
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
	"chanmodes/inviteonly",
	"6.0",
	"Channel Mode +i",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_INVITE_ONLY;

#define IsInviteOnly(channel)    (channel->mode.mode & EXTCMODE_INVITE_ONLY)

int inviteonly_can_join(Client *client, Channel *channel, const char *key, char **errmsg);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'i';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_INVITE_ONLY);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, inviteonly_can_join);

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

int inviteonly_can_join (Client *client, Channel *channel, const char *key, char **errmsg)
{
	if (IsInviteOnly(channel))
	{
		if (is_invited(client, channel))
			return 0;
		if (find_invex(channel, client))
			return 0;
		*errmsg = STR_ERR_INVITEONLYCHAN;
		return ERR_INVITEONLYCHAN;
	}
	return 0;
}
