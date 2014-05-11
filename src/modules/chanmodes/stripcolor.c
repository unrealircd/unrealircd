/*
 * Strip Color UnrealIRCd Module (Channel Mode +S)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

DLLFUNC CMD_FUNC(stripcolor);

ModuleHeader MOD_HEADER(stripcolor)
  = {
	"chanmodes/stripcolor",
	"$Id$",
	"Channel Mode +S",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_STRIPCOLOR;

#define IsStripColor(chptr)    (chptr->mode.extmode & EXTCMODE_STRIPCOLOR)

DLLFUNC char *stripcolor_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
DLLFUNC char *stripcolor_prelocalpart(aClient *sptr, aChannel *chptr, char *comment);
DLLFUNC char *stripcolor_prelocalquit(aClient *sptr, char *comment);

DLLFUNC int MOD_TEST(stripcolor)(ModuleInfo *modinfo)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(stripcolor)(ModuleInfo *modinfo)
{
CmodeInfo req;

	/* Channel mode */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'S';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_STRIPCOLOR);
	
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_CHANMSG, stripcolor_prechanmsg);
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, stripcolor_prelocalpart);
	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, stripcolor_prelocalquit);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(stripcolor)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(stripcolor)(int module_unload)
{
	return MOD_SUCCESS;
}

DLLFUNC char *stripcolor_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
	if (MyClient(sptr) && IsStripColor(chptr))
		text = StripColors(text);

	return text;
}

DLLFUNC char *stripcolor_prelocalpart(aClient *sptr, aChannel *chptr, char *comment)
{
	if (MyClient(sptr) && IsStripColor(chptr))
		comment = StripColors(comment);

	return comment;
}

/** Is any channel where the user is in +S? */
static int IsAnyChannelStripColor(aClient *sptr)
{
Membership *lp;

	for (lp = sptr->user->channel; lp; lp = lp->next)
		if (IsStripColor(lp->chptr))
			return 1;
	return 0;
}


DLLFUNC char *stripcolor_prelocalquit(aClient *sptr, char *comment)
{
	if (MyClient(sptr) && !BadPtr(comment) && IsAnyChannelStripColor(sptr))
		comment = StripColors(comment);
        
	return comment;
}

