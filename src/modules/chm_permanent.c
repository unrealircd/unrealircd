/*
 * UnrealIRCd, src/modules/chm_permanent.c
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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "m_cap.h"

ModuleHeader MOD_HEADER(chm_permanent)
  = {
        "chm_permanent",
        "$Id$",
        "Permanent channel mode (+P)", 
        "3.2-b8-1",
        NULL 
    };

static Cmode_t EXTMODE_PERMANENT = 0L;

static void chm_permanent_channel_destroy(aChannel *chptr, bool *should_destroy)
{
	if (chptr->mode.extmode & EXTMODE_PERMANENT)
		*should_destroy = false;
}

static int chm_permanent_is_ok(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsOper(cptr))
		return EX_ALLOW;

	return EX_DENY;
}

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(chm_permanent)(ModuleInfo *modinfo)
{
	CmodeInfo chm_permanent = { };

        MARK_AS_OFFICIAL_MODULE(modinfo);

	chm_permanent.paracount = 0;
	chm_permanent.flag = 'P';
	chm_permanent.is_ok = chm_permanent_is_ok;
	CmodeAdd(modinfo->handle, chm_permanent, &EXTMODE_PERMANENT);

	HookAddVoidEx(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, chm_permanent_channel_destroy);

        return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(chm_permanent)(int module_load)
{
        return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(chm_permanent)(int module_unload)
{
        return MOD_SUCCESS;
}

