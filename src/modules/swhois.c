/*
 *   IRC - Internet Relay Chat, src/modules/swhois.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SWHOIS command
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

CMD_FUNC(cmd_swhois);

#define MSG_SWHOIS 	"SWHOIS"	

ModuleHeader MOD_HEADER
  = {
	"swhois",
	"5.0",
	"command /swhois", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SWHOIS, cmd_swhois, MAXPARA, CMD_SERVER);
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
/** SWHOIS: add or delete additional whois titles to user.
 * Old syntax:
 * parv[1] = nickname
 * parv[2] = new swhois
 * New syntax (since July 2015, by Syzop):
 * parv[1] = nickname
 * parv[2] = + or -
 * parv[3] = added-by tag
 * parv[4] = priority
 * parv[5] = swhois
 */
CMD_FUNC(cmd_swhois)
{
	Client *target;
	char tag[HOSTLEN+1];
	char swhois[SWHOISLEN+1];
	int add;
	int priority = 0;

	*tag = *swhois = '\0';

	if (parc < 3)
		return;

	target = find_user(parv[1], NULL);
	if (!target)
		return;

	if ((parc > 5) && !BadPtr(parv[5]))
	{
		/* New syntax */
		add = (*parv[2] == '+') ? 1 : 0;
		strlcpy(tag, parv[3], sizeof(tag));
		priority = atoi(parv[4]);
		strlcpy(swhois, parv[5], sizeof(swhois));
	} else {
		/* Old syntax */
		strlcpy(tag, client->name, sizeof(tag));
		if (BadPtr(parv[2]))
		{
			/* Delete. Hmmmm. Let's just delete anything with that tag. */
			strcpy(swhois, "*");
			add = 0;
		} else {
			/* Add */
			add = 1;
			strlcpy(swhois, parv[2], sizeof(swhois));
		}
	}

	if (add)
		swhois_add(target, tag, priority, swhois, client, client);
	else
		swhois_delete(target, tag, swhois, client, client);
}
