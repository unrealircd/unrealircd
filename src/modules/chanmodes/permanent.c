/*
 * UnrealIRCd, src/modules/chanmodes/permanent.c
 * Copyright (c) 2013 William Pitcock <nenolod@dereferenced.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"chanmodes/permanent",
	"4.2",
	"Permanent channel mode (+P)", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

static Cmode_t EXTMODE_PERMANENT = 0L;

static int permanent_channel_destroy(Channel *channel, int *should_destroy)
{
	if (channel->mode.mode & EXTMODE_PERMANENT)
		*should_destroy = 0;
	
	return 0;
}

static int permanent_is_ok(Client *client, Channel *channel, char mode, const char *para, int checkt, int what)
{
	if (!IsOper(client))
	{
		if (checkt == EXCHK_ACCESS_ERR)
			sendnumeric(client, ERR_NOPRIVILEGES);

		return EX_DENY;
	}

	return EX_ALLOW;
}

int permanent_chanmode(Client *client, Channel *channel, MessageTag *mtags, const char *modebuf, const char *parabuf, time_t sendts, int samode, int *destroy_channel)
{
	if (samode == -1)
		return 0; /* SJOIN server-sync, already has its own way of destroying the channel */

	/* Destroy the channel if it was set '(SA)MODE #chan -P' with nobody in it (#4442) */
	if (!(channel->mode.mode & EXTMODE_PERMANENT) && (channel->users <= 0))
	{
		sub1_from_channel(channel);
		*destroy_channel = 1;
	}
	
	return 0;
}

/* This is called on module init, before Server Ready */
MOD_INIT()
{
CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'P';
	req.is_ok = permanent_is_ok;
	CmodeAdd(modinfo->handle, req, &EXTMODE_PERMANENT);

	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, -100000, permanent_channel_destroy);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CHANMODE, 1000000, permanent_chanmode);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CHANMODE, 1000000, permanent_chanmode);

	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

