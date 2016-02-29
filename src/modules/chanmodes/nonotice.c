/*
 * Disallow notices in channel UnrealIRCd Module (Channel Mode +T)
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

ModuleHeader MOD_HEADER(nonotice)
  = {
	"chanmodes/nonotice",
	"4.0",
	"Channel Mode +T",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_NONOTICE;

#define IsNoNotice(chptr)    (chptr->mode.extmode & EXTCMODE_NONOTICE)

int nonotice_check_can_send(aClient *cptr, aChannel *chptr, char *msgtext, Membership *lp, int notice);

MOD_TEST(nonotice)
{
	return MOD_SUCCESS;
}

MOD_INIT(nonotice)
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'T';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NONOTICE);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND, 0, nonotice_check_can_send);

	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(nonotice)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(nonotice)
{
	return MOD_SUCCESS;
}

int nonotice_check_can_send(aClient *cptr, aChannel *chptr, char *msgtext, Membership *lp, int notice)
{
	if (notice && IsNoNotice(chptr) &&
	   (!lp || !(lp->flags & (CHFL_CHANOP | CHFL_CHANOWNER | CHFL_CHANPROT))))
		return CANNOT_SEND_NONOTICE;

	return HOOK_CONTINUE;
}
