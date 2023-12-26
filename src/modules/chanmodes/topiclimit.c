/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/topiclimit.c
 * Channel Mode +t
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
	"chanmodes/topiclimit",
	"6.0",
	"Channel Mode +t",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
Cmode_t EXTCMODE_TOPIC_LIMIT;

/* Forward declarations */
int topiclimit_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg);

/* Macros */
#define IsTopicLimit(channel)    (channel->mode.mode & EXTCMODE_TOPIC_LIMIT)

MOD_INIT()
{
	CmodeInfo req;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 't';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_TOPIC_LIMIT);

	HookAdd(modinfo->handle, HOOKTYPE_CAN_SET_TOPIC, 0, topiclimit_can_set_topic);
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

int topiclimit_can_set_topic(Client *client, Channel *channel, const char *topic, const char **errmsg)
{
	static char errmsg_buf[NICKLEN+256];

	if (has_channel_mode(channel, 't') &&
	    !check_channel_access(client, channel, "hoaq") &&
	    !IsULine(client) &&
	    !IsServer(client))
	{
		buildnumeric(errmsg_buf, sizeof(errmsg_buf), client, ERR_CHANOPRIVSNEEDED, channel->name);
		*errmsg = errmsg_buf;
		return EX_DENY;
	}

	return EX_ALLOW;
}
