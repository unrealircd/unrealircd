/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/servicebot.c
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

#define IsServiceBot(client)    (client->umodes & UMODE_SERVICEBOT)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/servicebot",
	"4.2",
	"User Mode +S",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_SERVICEBOT = 0L;

/* Forward declarations */
int servicebot_can_kick(Client *client, Client *target, Channel *channel,
                    const char *comment, const char *client_member_modes, const char *target_member_modes, const char **reject_reason);
int servicebot_mode_deop(Client *client, Client *target, Channel *channel,
                    u_int what, int modechar, const char *client_access, const char *target_access, const char **reject_reason);
int servicebot_pre_kill(Client *client, Client *target, const char *reason);
int servicebot_whois(Client *requester, Client *acptr, NameValuePrioList **list);
int servicebot_see_channel_in_whois(Client *client, Client *target, Channel *channel);
                    
MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'S', UMODE_GLOBAL, 1, umode_allow_none, &UMODE_SERVICEBOT);
	
	HookAdd(modinfo->handle, HOOKTYPE_CAN_KICK, 0, servicebot_can_kick);
	HookAdd(modinfo->handle, HOOKTYPE_MODE_DEOP, 0, servicebot_mode_deop);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_KILL, 0, servicebot_pre_kill);
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, servicebot_whois);
	HookAdd(modinfo->handle, HOOKTYPE_SEE_CHANNEL_IN_WHOIS, 0, servicebot_see_channel_in_whois);
	
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

int servicebot_can_kick(Client *client, Client *target, Channel *channel, const char *comment,
                    const char *client_member_modes, const char *target_member_modes, const char **reject_reason)
{
	static char errmsg[NICKLEN+256];

	if (MyUser(client) && !IsULine(client) && IsServiceBot(target))
	{
		char errmsg2[NICKLEN+32];
		snprintf(errmsg2, sizeof(errmsg2), "%s is a Service Bot", target->name);
		
		snprintf(errmsg, sizeof(errmsg), ":%s %d %s %s :%s",
		         me.name, ERR_CANNOTDOCOMMAND, client->name, "KICK", errmsg2);

		*reject_reason = errmsg;

		return EX_DENY;
	}

	return EX_ALLOW;
}

int servicebot_mode_deop(Client *client, Client *target, Channel *channel,
                    u_int what, int modechar, const char *client_access, const char *target_access, const char **reject_reason)
{
	static char errmsg[NICKLEN+256];
	
	if (IsServiceBot(target) && MyUser(client) && !ValidatePermissionsForPath("services:servicebot:deop",client,target,channel,NULL) && (what == MODE_DEL))
	{
		snprintf(errmsg, sizeof(errmsg), ":%s %d %s %c :%s is a Service Bot",
			me.name, ERR_CANNOTCHANGECHANMODE, client->name, (char)modechar, target->name);
		
		*reject_reason = errmsg;
		
		return EX_DENY;
	}
	
	return EX_ALLOW;
}

int servicebot_pre_kill(Client *client, Client *target, const char *reason)
{
	if (IsServiceBot(target) && !(ValidatePermissionsForPath("services:servicebot:kill",client,target,NULL,NULL) || IsULine(client)))
	{
		sendnumeric(client, ERR_KILLDENY, target->name);
		return EX_ALWAYS_DENY;
	}
	return EX_ALLOW;
}

int servicebot_whois(Client *client, Client *target, NameValuePrioList **list)
{
	int hideoper = (IsHideOper(target) && (client != target) && !IsOper(client)) ? 1 : 0;

	if (IsServiceBot(target) && !hideoper &&
	    (whois_get_policy(client, target, "services") > WHOIS_CONFIG_DETAILS_NONE))
	{
		add_nvplist_numeric(list, 0, "services", client, RPL_WHOISOPERATOR, target->name, "a Network Service");
	}

	return 0;
}

/* This hides the servicebot, even if you are in the same channel, unless oper overriding */
int servicebot_see_channel_in_whois(Client *client, Client *target, Channel *channel)
{
	if (IsServiceBot(target))
		return EX_DENY;
	
	return EX_ALLOW;
}
