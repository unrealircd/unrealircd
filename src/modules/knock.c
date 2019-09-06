/*
 *   IRC - Internet Relay Chat, src/modules/m_knock.c
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

CMD_FUNC(m_knock);

#define MSG_KNOCK 	"KNOCK"	

ModuleHeader MOD_HEADER(knock)
  = {
	"knock",
	"5.0",
	"command /knock", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(knock)
{
	CommandAdd(modinfo->handle, MSG_KNOCK, m_knock, 2, M_USER);
	IsupportAdd(modinfo->handle, "KNOCK", NULL);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(knock)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(knock)
{
	return MOD_SUCCESS;
}

/*
** m_knock
**	parv[1] - channel
**	parv[2] - reason
**
** Coded by Stskeeps
** Additional bugfixes/ideas by codemastr
** (C) codemastr & Stskeeps
** 
*/
CMD_FUNC(m_knock)
{
	aChannel *chptr;
	Hook *h;
	int i = 0;
	MessageTag *mtags = NULL;

	if (IsServer(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "KNOCK");
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);

	/* bugfix for /knock PRv Please? */
	if (*parv[1] != '#')
	{
		sendnumeric(sptr, ERR_CANNOTKNOCK,
		    parv[1], "Remember to use a # prefix in channel name");

		return 0;
	}
	if (!(chptr = find_channel(parv[1], NULL)))
	{
		sendnumeric(sptr, ERR_CANNOTKNOCK, parv[1], "Channel does not exist!");
		return 0;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(sptr, chptr) == 1)
	{
		sendnumeric(sptr, ERR_CANNOTKNOCK, chptr->chname, "You're already there!");
		return 0;
	}

	if (!(chptr->mode.mode & MODE_INVITEONLY))
	{
		sendnumeric(sptr, ERR_CANNOTKNOCK, chptr->chname, "Channel is not invite only!");
		return 0;
	}

	if (is_banned(sptr, chptr, BANCHK_JOIN, NULL, NULL))
	{
		sendnumeric(sptr, ERR_CANNOTKNOCK, chptr->chname, "You're banned!");
		return 0;
	}

	for (h = Hooks[HOOKTYPE_PRE_KNOCK]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(sptr,chptr);
		if (i == HOOK_DENY || i == HOOK_ALLOW)
			break;
	}

	if (i == HOOK_DENY)
		return 0;

	if (MyClient(sptr) && !ValidatePermissionsForPath("immune:knock-flood",sptr,NULL,NULL,NULL))
	{
		if ((sptr->user->flood.knock_t + KNOCK_PERIOD) <= timeofday)
		{
			sptr->user->flood.knock_c = 0;
			sptr->user->flood.knock_t = timeofday;
		}
		if (sptr->user->flood.knock_c <= KNOCK_COUNT)
			sptr->user->flood.knock_c++;
		if (sptr->user->flood.knock_c > KNOCK_COUNT)
		{
			sendnumeric(sptr, ERR_CANNOTKNOCK, parv[1],
			    "You are KNOCK flooding");
			return 0;
		}
	}

	new_message(&me, NULL, &mtags);
	sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
	               0, SEND_ALL, mtags,
	               ":%s NOTICE @%s :[Knock] by %s!%s@%s (%s)",
	               me.name, chptr->chname,
	               sptr->name, sptr->user->username, GetHost(sptr),
	               parv[2] ? parv[2] : "no reason specified");

	sendnotice(sptr, "Knocked on %s", chptr->chname);

        RunHook4(HOOKTYPE_KNOCK, sptr, chptr, mtags, parv[2]);

	free_message_tags(mtags);
	return 0;
}
