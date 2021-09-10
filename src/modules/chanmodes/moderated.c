/*
 * Channel Mode +m
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
	"chanmodes/moderated",
	"6.0",
	"Channel Mode +m",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_MODERATED;

#define IsModerated(channel)    (channel->mode.mode & EXTCMODE_MODERATED)

int moderated_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
const char *moderated_pre_local_part(Client *client, Channel *channel, const char *text);

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'm';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_MODERATED);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, moderated_can_send_to_channel);
	HookAddConstString(modinfo->handle, HOOKTYPE_PRE_LOCAL_PART, 0, moderated_pre_local_part);

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

int moderated_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	if (IsModerated(channel) && (!lp || !(lp->flags & CHFL_OVERLAP)) &&
	    !op_can_override("channel:override:message:moderated",client,channel,NULL))
	{
		Hook *h;
		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			int i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_MODERATED);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass +m restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		*errmsg = "You need voice (+v)";
		return HOOK_DENY; /* BLOCK message */
	}

	return HOOK_CONTINUE;
}

/** Remove PART reason too if the channel is +m, -t, and user not +vhoaq */
const char *moderated_pre_local_part(Client *client, Channel *channel, const char *text)
{
	if (IsModerated(channel) && !has_voice(client, channel) && !is_half_op(client, channel))
		return NULL;
	return text;
}
