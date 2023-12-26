/* 
 * IRC - Internet Relay Chat, src/modules/usermodes/bot.c
 * Bot user mode (User mode +B)
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

#define IsBot(cptr)    (cptr->umodes & UMODE_BOT)

/* Module header */
ModuleHeader MOD_HEADER
  = {
	"usermodes/bot",
	"4.2",
	"User Mode +B",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long UMODE_BOT = 0L;

/* Forward declarations */
int bot_whois(Client *client, Client *acptr, NameValuePrioList **list);
int bot_who_status(Client *client, Client *acptr, Channel *channel, Member *cm, const char *status, int cansee);
int bot_umode_change(Client *client, long oldmode, long newmode);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	UmodeAdd(modinfo->handle, 'B', UMODE_GLOBAL, 0, NULL, &UMODE_BOT);
	ISupportAdd(modinfo->handle, "BOT", "B");
	
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, bot_whois);
	HookAdd(modinfo->handle, HOOKTYPE_WHO_STATUS, 0, bot_who_status);
	HookAdd(modinfo->handle, HOOKTYPE_UMODE_CHANGE, 0, bot_umode_change);
	
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

int bot_whois(Client *client, Client *target, NameValuePrioList **list)
{
	char buf[512];

	if (!IsBot(target))
		return 0;

	if (whois_get_policy(client, target, "bot") == WHOIS_CONFIG_DETAILS_NONE)
		return 0;

	add_nvplist_numeric(list, 0, "bot", client, RPL_WHOISBOT, target->name, NETWORK_NAME);

	return 0;
}

int bot_who_status(Client *client, Client *target, Channel *channel, Member *cm, const char *status, int cansee)
{
	if (IsBot(target))
		return 'B';
	
	return 0;
}

int bot_umode_change(Client *client, long oldmode, long newmode)
{
	if ((newmode & UMODE_BOT) && !(oldmode & UMODE_BOT) && MyUser(client))
	{
		/* now +B */
		const char *parv[2];
		parv[0] = client->name;
		parv[1] = NULL;
		do_cmd(client, NULL, "BOTMOTD", 1, parv);
	}

	return 0;
}
