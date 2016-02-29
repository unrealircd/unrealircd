/*
 * Block Color UnrealIRCd Module (Channel Mode +c)
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

#include "unrealircd.h"

CMD_FUNC(nocolor);

ModuleHeader MOD_HEADER(nocolor)
  = {
	"chanmodes/nocolor",
	"4.0",
	"Channel Mode +c",
	"3.2-b8-1",
	NULL 
    };

Cmode_t EXTCMODE_NOCOLOR;

#define IsNoColor(chptr)    (chptr->mode.extmode & EXTCMODE_NOCOLOR)

DLLFUNC char *nocolor_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
DLLFUNC char *nocolor_prelocalpart(aClient *sptr, aChannel *chptr, char *comment);
DLLFUNC char *nocolor_prelocalquit(aClient *sptr, char *comment);

MOD_TEST(nocolor)
{
	return MOD_SUCCESS;
}

MOD_INIT(nocolor)
{
CmodeInfo req;

	/* Channel mode */
	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.flag = 'c';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOCOLOR);
	
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, nocolor_prechanmsg);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, nocolor_prelocalpart);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, nocolor_prelocalquit);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(nocolor)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(nocolor)
{
	return MOD_SUCCESS;
}

static int IsUsingColor(char *s)
{
        if (!s)
                return 0;

        for (; *s; s++)
                if (*s == 3 || *s == 27 || *s == 4 || *s == 22) /* mirc color, ansi, rgb, reverse */
                        return 1;

        return 0;
}

DLLFUNC char *nocolor_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
        if (MyClient(sptr) && IsNoColor(chptr) && IsUsingColor(text))
        {
                if (!notice)
                {
                        sendto_one(sptr, err_str(ERR_CANNOTSENDTOCHAN),
                                   me.name, sptr->name, chptr->chname,
                                   "Color is not permitted in this channel", chptr->chname);
                }

                return NULL;
        }
        return text;
}

DLLFUNC char *nocolor_prelocalpart(aClient *sptr, aChannel *chptr, char *comment)
{
        if (MyClient(sptr) && IsNoColor(chptr) && IsUsingColor(comment))
                return NULL;

        return comment;
}

/** Is any channel where the user is in +c? */
static int IsAnyChannelNoColor(aClient *sptr)
{
Membership *lp;

	for (lp = sptr->user->channel; lp; lp = lp->next)
		if (IsNoColor(lp->chptr))
			return 1;
	return 0;
}

DLLFUNC char *nocolor_prelocalquit(aClient *sptr, char *comment)
{
        if (MyClient(sptr) && !BadPtr(comment) && IsUsingColor(comment) && IsAnyChannelNoColor(sptr))
                return NULL;

        return comment;
}
