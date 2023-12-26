/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/noctcp.c
 * Block user-to-user CTCP UnrealIRCd Module (User Mode +T)
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
	"usermodes/noctcp",
	"4.2",
	"User Mode +T",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

long UMODE_NOCTCP = 0L;

#define IsNoCTCP(client)    (client->umodes & UMODE_NOCTCP)

int noctcp_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
CmodeInfo req;

	UmodeAdd(modinfo->handle, 'T', UMODE_GLOBAL, 0, NULL, &UMODE_NOCTCP);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_USER, 0, noctcp_can_send_to_user);
	
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

	if ((*s == '\001') && strncmp(&s[1], "ACTION ", 7) && strncmp(&s[1], "DCC ", 4))
		return 1;

	return 0;
}

int noctcp_can_send_to_user(Client *client, Client *target, const char **text, const char **errmsg, SendType sendtype)
{
	if (MyUser(client) && (sendtype == SEND_TYPE_PRIVMSG) &&
	    IsNoCTCP(target) && !IsOper(client) && IsACTCP(*text))
	{
		*errmsg = "User does not accept CTCPs";
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}
