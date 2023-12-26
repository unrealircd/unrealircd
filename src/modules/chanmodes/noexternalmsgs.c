/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/noexternalmsgs.c
 * Channel Mode +n
 * (C) Copyright 2021 Syzop and the UnrealIRCd team
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
	"chanmodes/noexternalmsgs",
	"6.0",
	"Channel Mode +n",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_NO_EXTERNAL_MESSAGES;

#define IsNoExternalMessages(channel)    (channel->mode.mode & EXTCMODE_NO_EXTERNAL_MESSAGES)

int noexternalmsgs_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'n';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NO_EXTERNAL_MESSAGES);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, noexternalmsgs_can_send_to_channel);

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

int noexternalmsgs_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	if (IsNoExternalMessages(channel) && !IsMember(client,channel))
	{
		/* Channel does not accept external messages (+n).
		 * Reject, unless HOOKTYPE_CAN_BYPASS_NO_EXTERNAL_MSGS tells otherwise.
		 */
		Hook *h;
		int i;

		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_EXTERNAL);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass +n restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		*errmsg = "No external channel messages";
		return HOOK_DENY; /* BLOCK message */
	}

	return HOOK_CONTINUE;
}
