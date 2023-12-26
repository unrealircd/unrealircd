/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/private.c
 * Channel Mode +p
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
	"chanmodes/private",
	"6.0",
	"Channel Mode +p",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_PRIVATE;

#define IsPrivate(channel)    (channel->mode.mode & EXTCMODE_PRIVATE)

int private_modechar_add(Channel *channel, int modechar);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'p';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_PRIVATE);

	HookAdd(modinfo->handle, HOOKTYPE_MODECHAR_ADD, 0, private_modechar_add);

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

/** This clears channel mode +p when +s gets set */
int private_modechar_add(Channel *channel, int modechar)
{
	if (modechar == 's')
	{
		channel->mode.mode &= ~EXTCMODE_PRIVATE;
	}
	return 0;
}
