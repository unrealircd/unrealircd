/* 
 * IRC - Internet Relay Chat, src/modules/chanmodes/noctcp.c
 * Block CTCP UnrealIRCd Module (Channel Mode +C)
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

CMD_FUNC(noctcp);

ModuleHeader MOD_HEADER
  = {
	"chanmodes/noctcp",
	"4.2",
	"Channel Mode +C",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

Cmode_t EXTCMODE_NOCTCP;

#define IsNoCTCP(channel)    (channel->mode.mode & EXTCMODE_NOCTCP)

int noctcp_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
CmodeInfo req;

	memset(&req, 0, sizeof(req));
	req.paracount = 0;
	req.letter = 'C';
	req.is_ok = extcmode_default_requirehalfop;
	CmodeAdd(modinfo->handle, req, &EXTCMODE_NOCTCP);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, noctcp_can_send_to_channel);
	
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

static int IsACTCP(const char *s)
{
	if (!s)
		return 0;

	if ((*s == '\001') && strncmp(&s[1], "ACTION ", 7))
		return 1;

	return 0;
}

int noctcp_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	if (IsNoCTCP(channel) && IsACTCP(*msg))
	{
		*errmsg = "CTCPs are not permitted in this channel";
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}
