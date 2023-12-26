/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/stripcolor.c
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

#include "unrealircd.h"

CMD_FUNC(stripcolor);

ModuleHeader MOD_HEADER
  = {
	"chanmodes/stripcolor",
	"4.2",
	"Channel Mode +S",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_STRIPCOLOR;

#define IsStripColor(channel)    (channel->mode.mode & EXTCMODE_STRIPCOLOR)

int stripcolor_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
const char *stripcolor_prelocalpart(Client *client, Channel *channel, const char *comment);
const char *stripcolor_prelocalquit(Client *client, const char *comment);

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
	req.letter = 'S';
	req.is_ok = extcmode_default_requirechop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_STRIPCOLOR);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, stripcolor_can_send_to_channel);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, stripcolor_prelocalpart);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT_CHAN, 0, stripcolor_prelocalpart);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_QUIT, 0, stripcolor_prelocalquit);
	
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

int stripcolor_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	Hook *h;
	int i;

	if (MyUser(client) && IsStripColor(channel))
	{
		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_COLOR);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass this restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		*msg = StripColors(*msg);
	}

	return HOOK_CONTINUE;
}

const char *stripcolor_prelocalpart(Client *client, Channel *channel, const char *comment)
{
	if (!comment)
		return NULL;

	if (MyUser(client) && IsStripColor(channel))
		comment = StripColors(comment);

	return comment;
}

/** Is any channel where the user is in +S? */
static int IsAnyChannelStripColor(Client *client)
{
	Membership *lp;

	for (lp = client->user->channel; lp; lp = lp->next)
		if (IsStripColor(lp->channel))
			return 1;
	return 0;
}


const char *stripcolor_prelocalquit(Client *client, const char *comment)
{
	if (!comment)
		return NULL;

	if (MyUser(client) && !BadPtr(comment) && IsAnyChannelStripColor(client))
		comment = StripColors(comment);
        
	return comment;
}

