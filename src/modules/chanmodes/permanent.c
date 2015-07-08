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

ModuleHeader MOD_HEADER(permanent)
  = {
        "chanmodes/permanent",
        "$Id$",
        "Permanent channel mode (+P)", 
        "3.2-b8-1",
        NULL 
    };

static Cmode_t EXTMODE_PERMANENT = 0L;

static void permanent_channel_destroy(aChannel *chptr, bool *should_destroy)
{
	if (chptr->mode.extmode & EXTMODE_PERMANENT)
		*should_destroy = false;
}

static int permanent_is_ok(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	if (!IsOper(cptr))
	{
		sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
		return EX_DENY;
	}

	return EX_ALLOW;
}

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(permanent)(ModuleInfo *modinfo)
{
CmodeInfo req;

        MARK_AS_OFFICIAL_MODULE(modinfo);

        memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'P';
	req.is_ok = permanent_is_ok;
	CmodeAdd(modinfo->handle, req, &EXTMODE_PERMANENT);

	HookAddVoid(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 0, permanent_channel_destroy);

        return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(permanent)(int module_load)
{
        return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(permanent)(int module_unload)
{
        return MOD_SUCCESS;
}

