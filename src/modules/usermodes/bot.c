/*
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

#define WHOIS_BOT_STRING ":%s 335 %s %s :is a \2Bot\2 on %s"

/* Module header */
ModuleHeader MOD_HEADER(bot)
  = {
	"usermodes/bot",
	"4.0",
	"User Mode +B",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long UMODE_BOT = 0L;

/* Forward declarations */
int bot_whois(aClient *sptr, aClient *acptr);
int bot_who_status(aClient *sptr, aClient *acptr, aChannel *chptr, Member *cm, char *status, int cansee);
int bot_umode_change(aClient *sptr, long oldmode, long newmode);

MOD_TEST(bot)
{
	return MOD_SUCCESS;
}

MOD_INIT(bot)
{
	UmodeAdd(modinfo->handle, 'B', UMODE_GLOBAL, 0, NULL, &UMODE_BOT);
	
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, bot_whois);
	HookAdd(modinfo->handle, HOOKTYPE_WHO_STATUS, 0, bot_who_status);
	HookAdd(modinfo->handle, HOOKTYPE_UMODE_CHANGE, 0, bot_umode_change);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(bot)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(bot)
{
	return MOD_SUCCESS;
}

int bot_whois(aClient *sptr, aClient *acptr)
{
	if (IsBot(acptr))
		sendto_one(sptr, WHOIS_BOT_STRING, me.name, sptr->name, acptr->name, ircnetwork);

	return 0;
}

int bot_who_status(aClient *sptr, aClient *acptr, aChannel *chptr, Member *cm, char *status, int cansee)
{
	if (IsBot(acptr))
		return 'B';
	
	return 0;
}

int bot_umode_change(aClient *sptr, long oldmode, long newmode)
{
	if ((newmode & UMODE_BOT) && !(oldmode & UMODE_BOT) && MyClient(sptr))
	{
		/* now +B */
		char *parv[2];
		parv[0] = sptr->name;
		parv[1] = NULL;
		(void)do_cmd(sptr, sptr, "BOTMOTD", 1, parv);
	}

	return 0;
}
