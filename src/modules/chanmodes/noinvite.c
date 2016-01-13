/*
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

ModuleHeader MOD_HEADER(noinvite)
  = {
	"chanmodes/noinvite",
	"4.0",
	"Channel Mode +V",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_NOINVITE;

#define IsNoInvite(chptr)    (chptr->mode.extmode & EXTCMODE_NOINVITE)

DLLFUNC int noinvite_check (aClient *sptr, aChannel *chptr);

MOD_TEST(noinvite)
{
	return MOD_SUCCESS;
}

MOD_INIT(noinvite)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'V';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOINVITE);
	
	HookAdd(modinfo->handle, HOOKTYPE_PRE_KNOCK, 0, noinvite_check);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_INVITE, 0, noinvite_check);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(noinvite)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(noinvite)
{
	return MOD_SUCCESS;
}


DLLFUNC int noinvite_check (aClient *sptr, aChannel *chptr)
{
	if (MyClient(sptr) && IsNoInvite(chptr))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
				    me.name,
				    sptr->name,
				    chptr->chname, "The channel does not allow invites (+V)");
		return HOOK_DENY;
	}

	return HOOK_CONTINUE;
}
