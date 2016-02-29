/*
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


ModuleHeader MOD_HEADER(nokick)
  = {
	"chanmodes/nokick",
	"4.0",
	"Channel Mode +Q",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_NOKICK;

#define IsNoKick(chptr)    (chptr->mode.extmode & EXTCMODE_NOKICK)

int nokick_check (aClient* sptr, aClient* who, aChannel *chptr, char* comment, long sptr_flags, long who_flags, char **reject_reason);

MOD_TEST(nokick)
{
	return MOD_SUCCESS;
}

MOD_INIT(nokick)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'Q';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOKICK);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_KICK, 0, nokick_check);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(nokick)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(nokick)
{
	return MOD_SUCCESS;
}

int nokick_check (aClient* sptr, aClient* who, aChannel *chptr, char* comment, long sptr_flags, long who_flags, char **reject_reason)
{
	static char errmsg[256];

	if (MyClient(sptr) && IsNoKick(chptr))
	{
		ircsnprintf(errmsg, sizeof(errmsg), err_str(ERR_CANNOTDOCOMMAND),
				   me.name, sptr->name, "KICK",
				   "channel is +Q");
		*reject_reason = errmsg;
		return EX_DENY; /* Deny, but let opers override if necessary. */
	}

	return EX_ALLOW;
}

