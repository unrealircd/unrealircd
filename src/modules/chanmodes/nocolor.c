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

ModuleHeader MOD_HEADER
  = {
	"chanmodes/nocolor",
	"4.2",
	"Channel Mode +c",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

Cmode_t EXTCMODE_NOCOLOR;

#define IsNoColor(chptr)    (chptr->mode.extmode & EXTCMODE_NOCOLOR)

char *nocolor_prechanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice);
char *nocolor_prelocalpart(Client *sptr, Channel *chptr, char *comment);
char *nocolor_prelocalquit(Client *sptr, char *comment);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
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
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT_CHAN, 0, nocolor_prelocalpart);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, nocolor_prelocalquit);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
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

char *nocolor_prechanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice)
{
	Hook *h;
	int i;

	if (MyUser(sptr) && IsNoColor(chptr) && IsUsingColor(text))
	{
		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(sptr, chptr, BYPASS_CHANMSG_COLOR);
			if (i == HOOK_ALLOW)
				return text; /* bypass */
			if (i != HOOK_CONTINUE)
				break;
		}

		if (!notice)
		{
			sendnumeric(sptr, ERR_CANNOTSENDTOCHAN, chptr->chname,
			           "Color is not permitted in this channel", chptr->chname);
		}
		return NULL; /* block */
	}

	return text;
}

char *nocolor_prelocalpart(Client *sptr, Channel *chptr, char *comment)
{
	if (!comment)
		return NULL;

	if (MyUser(sptr) && IsNoColor(chptr) && IsUsingColor(comment))
		return NULL;

	return comment;
}

/** Is any channel where the user is in +c? */
static int IsAnyChannelNoColor(Client *sptr)
{
	Membership *lp;

	for (lp = sptr->user->channel; lp; lp = lp->next)
		if (IsNoColor(lp->chptr))
			return 1;
	return 0;
}

char *nocolor_prelocalquit(Client *sptr, char *comment)
{
	if (!comment)
		return NULL;

	if (MyUser(sptr) && !BadPtr(comment) && IsUsingColor(comment) && IsAnyChannelNoColor(sptr))
		return NULL;

	return comment;
}
