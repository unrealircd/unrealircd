/*
 *   IRC - Internet Relay Chat, src/modules/knock.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_knock);

#define MSG_KNOCK 	"KNOCK"	

ModuleHeader MOD_HEADER
  = {
	"knock",
	"5.0",
	"command /knock", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_KNOCK, cmd_knock, 2, CMD_USER);
	ISupportAdd(modinfo->handle, "KNOCK", NULL);
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

/*
** cmd_knock
**	parv[1] - channel
**	parv[2] - reason
**
** Coded by Stskeeps
** Additional bugfixes/ideas by codemastr
** (C) codemastr & Stskeeps
** 
*/
CMD_FUNC(cmd_knock)
{
	Channel *chptr;
	Hook *h;
	int i = 0;
	MessageTag *mtags = NULL;

	if (IsServer(client))
		return;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KNOCK");
		return;
	}

	if (MyConnect(client))
		clean_channelname(parv[1]);

	/* bugfix for /knock PRv Please? */
	if (*parv[1] != '#')
	{
		sendnumeric(client, ERR_CANNOTKNOCK,
		    parv[1], "Remember to use a # prefix in channel name");

		return;
	}
	if (!(chptr = find_channel(parv[1], NULL)))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, parv[1], "Channel does not exist!");
		return;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(client, chptr) == 1)
	{
		sendnumeric(client, ERR_CANNOTKNOCK, chptr->chname, "You're already there!");
		return;
	}

	if (!(chptr->mode.mode & MODE_INVITEONLY))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, chptr->chname, "Channel is not invite only!");
		return;
	}

	if (is_banned(client, chptr, BANCHK_JOIN, NULL, NULL))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, chptr->chname, "You're banned!");
		return;
	}

	for (h = Hooks[HOOKTYPE_PRE_KNOCK]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client,chptr);
		if (i == HOOK_DENY || i == HOOK_ALLOW)
			break;
	}

	if (i == HOOK_DENY)
		return;

	if (MyUser(client) && !ValidatePermissionsForPath("immune:knock-flood",client,NULL,NULL,NULL))
	{
		if ((client->user->flood.knock_t + KNOCK_PERIOD) <= timeofday)
		{
			client->user->flood.knock_c = 0;
			client->user->flood.knock_t = timeofday;
		}
		if (client->user->flood.knock_c <= KNOCK_COUNT)
			client->user->flood.knock_c++;
		if (client->user->flood.knock_c > KNOCK_COUNT)
		{
			sendnumeric(client, ERR_CANNOTKNOCK, parv[1],
			    "You are KNOCK flooding");
			return;
		}
	}

	new_message(&me, NULL, &mtags);
	sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
	               0, SEND_ALL, mtags,
	               ":%s NOTICE @%s :[Knock] by %s!%s@%s (%s)",
	               me.name, chptr->chname,
	               client->name, client->user->username, GetHost(client),
	               parv[2] ? parv[2] : "no reason specified");

	sendnotice(client, "Knocked on %s", chptr->chname);

        RunHook4(HOOKTYPE_KNOCK, client, chptr, mtags, parv[2]);

	free_message_tags(mtags);
}
