/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/regonlyspeak.c
 * Only registered users can speak UnrealIRCd Module (Channel Mode +M)
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


ModuleHeader MOD_HEADER
  = {
	"chanmodes/regonlyspeak",
	"4.2",
	"Channel Mode +M",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_REGONLYSPEAK;
static char errMsg[2048];

#define IsRegOnlySpeak(channel)    (channel->mode.mode & EXTCMODE_REGONLYSPEAK)

int regonlyspeak_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
const char *regonlyspeak_part_message (Client *client, Channel *channel, const char *comment);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'M';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_REGONLYSPEAK);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, regonlyspeak_can_send_to_channel);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, regonlyspeak_part_message);

	
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

const char *regonlyspeak_part_message (Client *client, Channel *channel, const char *comment)
{
	if (!comment)
		return NULL;

	if (IsRegOnlySpeak(channel) && !IsLoggedIn(client) && !ValidatePermissionsForPath("channel:override:message:regonlyspeak",client,NULL,NULL,NULL))
		return NULL;

	return comment;
}

int regonlyspeak_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	Hook *h;
	int i;

	if (IsRegOnlySpeak(channel) &&
	    !op_can_override("channel:override:message:regonlyspeak",client,channel,NULL) &&
	    !IsLoggedIn(client) &&
	    !check_channel_access_membership(lp, "vhoaq"))
	{
		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_MODERATED);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass +M restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		*errmsg = "You must have a registered nick (+r) to talk on this channel";
		return HOOK_DENY; /* BLOCK message */
	}

	return HOOK_CONTINUE;
}
