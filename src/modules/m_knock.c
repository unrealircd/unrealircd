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

ModuleHeader MOD_HEADER(m_knock)
  = {
	"m_knock",
	"4.0",
	"command /knock", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_knock)
{
	CommandAdd(modinfo->handle, MSG_KNOCK, m_knock, 2, M_USER|M_ANNOUNCE);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_knock)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_knock)
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

	if (IsServer(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "KNOCK");
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);

	/* bugfix for /knock PRv Please? */
	if (*parv[1] != '#')
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name,
		    parv[1], "Remember to use a # prefix in channel name");

		return 0;
	}
	if (!(chptr = find_channel(parv[1], NullChn)))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, parv[1], "Channel does not exist!");
		return 0;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(sptr, chptr) == 1)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "You're already there!");
		return 0;
	}

	if (!(chptr->mode.mode & MODE_INVITEONLY))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "Channel is not invite only!");
		return 0;
	}

	if (is_banned(sptr, chptr, BANCHK_JOIN))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, chptr->chname, "You're banned!");
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


	sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		":%s NOTICE @%s :[Knock] by %s!%s@%s (%s)", me.name, chptr->chname,
		sptr->name, sptr->user->username, GetHost(sptr),
		parv[2] ? parv[2] : "no reason specified");

	sendnotice(sptr, "Knocked on %s", chptr->chname);

        RunHook2(HOOKTYPE_KNOCK, sptr, chptr);
	return 0;
}
