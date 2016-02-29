/*
 * Registered users only UnrealIRCd Module (Channel Mode +R)
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


ModuleHeader MOD_HEADER(regonly)
  = {
	"chanmodes/regonly",
	"4.0",
	"Channel Mode +R",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_REGONLY;

#define IsRegOnly(chptr)    (chptr->mode.extmode & EXTCMODE_REGONLY)

DLLFUNC int regonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[]);


MOD_TEST(regonly)
{
	return MOD_SUCCESS;
}

MOD_INIT(regonly)
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'R';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGONLY);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, regonly_check);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(regonly)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(regonly)
{
	return MOD_SUCCESS;
}

DLLFUNC int regonly_check (aClient *cptr, aChannel *chptr, char *key, char *parv[])
{
	if (IsRegOnly(chptr) && !IsLoggedIn(cptr))
		return ERR_NEEDREGGEDNICK;
	return 0;
}

