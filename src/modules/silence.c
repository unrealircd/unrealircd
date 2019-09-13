/*
 *   IRC - Internet Relay Chat, src/modules/m_silence.c
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

CMD_FUNC(m_silence);

#define MSG_SILENCE 	"SILENCE"	

ModuleHeader MOD_HEADER
  = {
	"silence",
	"5.0",
	"command /silence", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SILENCE, m_silence, MAXPARA, M_USER);
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
** m_silence
** From local client:
**	parv[1] = mask (NULL sends the list)
** From remote client:
**	parv[1] = nick that must be silenced
**      parv[2] = mask
*/

CMD_FUNC(m_silence)
{
	Link *lp;
	Client *acptr;
	char c, *cp;

	acptr = sptr;

	if (MyUser(sptr))
	{
		if (parc < 2 || *parv[1] == '\0'
		    || (acptr = find_person(parv[1], NULL)))
		{
			if (acptr != sptr)
				return 0;
			for (lp = acptr->user->silence; lp; lp = lp->next)
				sendnumeric(sptr, RPL_SILELIST, acptr->name, lp->value.cp);
			sendnumeric(sptr, RPL_ENDOFSILELIST);
			return 0;
		}
		cp = parv[1];
		c = *cp;
		if (c == '-' || c == '+')
			cp++;
		else if (!(strchr(cp, '@') || strchr(cp, '.') ||
		    strchr(cp, '!') || strchr(cp, '*')))
		{
			sendnumeric(sptr, ERR_NOSUCHNICK, parv[1]);
			return -1;
		}
		else
			c = '+';
		cp = pretty_mask(cp);
		if ((c == '-' && !del_silence(sptr, cp)) ||
		    (c != '-' && !add_silence(sptr, cp, 1)))
		{
			sendto_prefix_one(sptr, sptr, NULL, ":%s SILENCE %c%s",
			    sptr->name, c, cp);
			if (c == '-')
				sendto_server(NULL, 0, 0, NULL, ":%s SILENCE * -%s",
				    sptr->name, cp);
		}
	}
	else if (parc < 3 || *parv[2] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS,
		    "SILENCE");
		return -1;
	}
	else if ((c = *parv[2]) == '-' || (acptr = find_person(parv[1], NULL)))
	{
		if (c == '-')
		{
			if (!del_silence(sptr, parv[2] + 1))
				sendto_server(cptr, 0, 0, NULL, ":%s SILENCE %s :%s",
				    sptr->name, parv[1], parv[2]);
		}
		else
		{
			(void)add_silence(sptr, parv[2], 1);
			if (!MyUser(acptr))
				sendto_one(acptr, NULL, ":%s SILENCE %s :%s",
				    sptr->name, parv[1], parv[2]);
		}
	}
	else
	{
		sendnumeric(sptr, ERR_NOSUCHNICK, parv[1]);
		return -1;
	}
	return 0;
}
