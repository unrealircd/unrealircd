/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/nokick.c
 * Prevents you from being kicked (User mode +q)
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

#define IsNokick(client)    (client->umodes & UMODE_NOKICK)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/nokick",
	"4.2",
	"User Mode +q",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_NOKICK = 0L;

/* Forward declarations */
int umode_allow_unkickable_oper(Client *client, int what);
int nokick_can_kick(Client *client, Client *target, Channel *channel,
                    const char *comment, const char *client_member_modes, const char *target_member_modes, const char **reject_reason);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'q', UMODE_GLOBAL, 1, umode_allow_unkickable_oper, &UMODE_NOKICK);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_KICK, 0, nokick_can_kick);
	
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

int umode_allow_unkickable_oper(Client *client, int what)
{
	if (MyUser(client))
	{
		if (IsOper(client) && ValidatePermissionsForPath("self:unkickablemode",client,NULL,NULL,NULL))
			return 1;
		return 0;
	}
	/* Always allow remotes: */
	return 1;
}

int nokick_can_kick(Client *client, Client *target, Channel *channel, const char *comment,
                    const char *client_member_modes, const char *target_member_modes, const char **reject_reason)
{
	static char errmsg[NICKLEN+256];

	if (IsNokick(target) && !IsULine(client) && MyUser(client) && !ValidatePermissionsForPath("channel:override:kick:nokick",client,target,channel,NULL))
	{
		ircsnprintf(errmsg, sizeof(errmsg), ":%s %d %s %s :%s",
		            me.name, ERR_CANNOTDOCOMMAND, client->name, "KICK",
				   "user is unkickable (user mode +q)");

		*reject_reason = errmsg;

		sendnotice(target,
			"*** umode q: %s tried to kick you from channel %s (%s)",
			client->name, channel->name, comment);
		
		return EX_ALWAYS_DENY;
	}

	return EX_ALLOW;
}
