/*
 * No nick changes in channel UnrealIRCd Module (Channel Mode +N)
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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef _WIN32
#include "version.h"
#endif


ModuleHeader MOD_HEADER(nonickchange)
  = {
	"chanmodes/nonickchange",
	"4.0",
	"Channel Mode +N",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_NONICKCHANGE;

#define IsNoNickChange(chptr)    (chptr->mode.extmode & EXTCMODE_NONICKCHANGE)

DLLFUNC int nonickchange_check (aClient *sptr, aChannel *chptr);

MOD_TEST(nonickchange)
{
	return MOD_SUCCESS;
}

MOD_INIT(nonickchange)
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'N';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NONICKCHANGE);
	
	HookAdd(modinfo->handle, HOOKTYPE_CHAN_PERMIT_NICK_CHANGE, 0, nonickchange_check);

	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(nonickchange)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(nonickchange)
{
	return MOD_SUCCESS;
}

DLLFUNC int nonickchange_check (aClient *sptr, aChannel *chptr)
{
	if (!IsOper(sptr) && !IsULine(sptr)
		&& IsNoNickChange(chptr)
		&& !is_chanownprotop(sptr, chptr))
		return HOOK_DENY;

	return HOOK_ALLOW;
}

