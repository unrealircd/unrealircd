/* IRC, Internet Relay Chat - src/modules/nocodes.c
 * "No Codes", a very simple (but often requested) module written by Syzop.
 * This module will strip messages with bold/underline/reverse if chanmode
 * +S is set, and block them if +c is set.
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

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"nocodes",	/* Name of module */
	"1.3", /* Version */
	"Strip/block color/bold/underline/italic/reverse - by Syzop", /* Short description of module */
	"UnrealIRCd Team", /* Author */
	"unrealircd-6",
};

int nocodes_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, nocodes_can_send_to_channel);
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

static int has_controlcodes(const char *p)
{
	for (; *p; p++)
		if ((*p == '\002') || (*p == '\037') || (*p == '\026') || (*p == '\035')) /* bold, underline, reverse, italic */
			return 1;
	return 0;
}

int nocodes_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	static char retbuf[4096];
	Hook *h;
	int i;

	if (has_channel_mode(channel, 'S'))
	{
		if (!has_controlcodes(*msg))
			return HOOK_CONTINUE;

		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_COLOR);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass +S restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		strlcpy(retbuf, StripControlCodes(*msg), sizeof(retbuf));
		*msg = retbuf;
		return HOOK_CONTINUE;
	} else
	if (has_channel_mode(channel, 'c'))
	{
		if (!has_controlcodes(*msg))
			return HOOK_CONTINUE;

		for (h = Hooks[HOOKTYPE_CAN_BYPASS_CHANNEL_MESSAGE_RESTRICTION]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client, channel, BYPASS_CHANMSG_COLOR);
			if (i == HOOK_ALLOW)
				return HOOK_CONTINUE; /* bypass +c restriction */
			if (i != HOOK_CONTINUE)
				break;
		}

		*errmsg = "Control codes (color/bold/underline/italic/reverse) are not permitted in this channel";
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}
