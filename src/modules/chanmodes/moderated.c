/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/moderated.c
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

/* Global variables */
Cmode_t EXTCMODE_MODERATED;

/* Forward declarations */
int moderated_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
const char *moderated_pre_local_part(Client *client, Channel *channel, const char *text);
int moderated_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg);

/* Macros */
#define IsModerated(channel)    (channel->mode.mode & EXTCMODE_MODERATED)

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
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SET_TOPIC, 0, moderated_can_set_topic);

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

int moderated_can_send_to_channel(Client *client, Channel *channel, Membership *m, const char **msg, const char **errmsg, SendType sendtype)
{
	if (IsModerated(channel) && (!m || !check_channel_access_membership(m, "vhoaq")) &&
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
	if (IsModerated(channel) && !check_channel_access(client, channel, "v") && !check_channel_access(client, channel, "h"))
		return NULL;
	return text;
}

int moderated_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg)
{
	static char errmsg_buf[NICKLEN+256];

	/* Channel is +m but user is not +vhoaq: reject the topic change */
	if (has_channel_mode(channel, 'm') && !check_channel_access(client, channel, "vhoaq"))
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "Voice (+v) or higher is required in order to change the topic on %s (channel is +m)", channel->name);
		buildnumeric(errmsg_buf, sizeof(errmsg_buf), client, ERR_CANNOTDOCOMMAND, "TOPIC", buf);
		*errmsg = errmsg_buf;
		return EX_DENY;
	}

	return EX_ALLOW;
}
