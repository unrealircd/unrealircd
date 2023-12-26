/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/isregistered.c
 * Channel Mode +r
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
	"chanmodes/isregistered",
	"6.0",
	"Channel Mode +r",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_REGISTERED;

#define IsRegisteredChannel(channel)    (channel->mode.mode & EXTCMODE_REGISTERED)

int isregistered_chanmode_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'r';
	req.is_ok = isregistered_chanmode_is_ok;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGISTERED);

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

int isregistered_chanmode_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
{
	if (!IsServer(client) && !IsULine(client))
	{
		if (type == EXCHK_ACCESS_ERR)
			sendnumeric(client, ERR_ONLYSERVERSCANCHANGE, channel->name);
		return EX_ALWAYS_DENY;
	}
	return EX_ALLOW;
}
