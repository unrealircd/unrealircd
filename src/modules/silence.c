/*
 *   IRC - Internet Relay Chat, src/modules/silence.c
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

CMD_FUNC(cmd_silence);

#define MSG_SILENCE 	"SILENCE"	

ModuleHeader MOD_HEADER
  = {
	"silence",
	"5.0",
	"command /silence", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* Forward declarations */
int _is_silenced(Client *, Client *);
int _del_silence(Client *sptr, const char *mask);
int _add_silence(Client *sptr, const char *mask, int senderr);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_ADD_SILENCE, _add_silence);
	EfunctionAdd(modinfo->handle, EFUNC_DEL_SILENCE, _del_silence);
	EfunctionAdd(modinfo->handle, EFUNC_IS_SILENCED, _is_silenced);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_SILENCE, cmd_silence, MAXPARA, M_USER);
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
** cmd_silence
** From local client:
**	parv[1] = mask (NULL sends the list)
** From remote client:
**	parv[1] = nick that must be silenced
**      parv[2] = mask
*/

CMD_FUNC(cmd_silence)
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

int _del_silence(Client *sptr, const char *mask)
{
	Link **lp;
	Link *tmp;

	for (lp = &(sptr->user->silence); *lp; lp = &((*lp)->next))
		if (mycmp(mask, (*lp)->value.cp) == 0)
		{
			tmp = *lp;
			*lp = tmp->next;
			safe_free(tmp->value.cp);
			free_link(tmp);
			return 0;
		}
	return -1;
}

int _add_silence(Client *sptr, const char *mask, int senderr)
{
	Link *lp;
	int  cnt = 0;

	for (lp = sptr->user->silence; lp; lp = lp->next)
	{
		if (MyUser(sptr))
			if ((strlen(lp->value.cp) > MAXSILELENGTH) || (++cnt >= SILENCE_LIMIT))
			{
				if (senderr)
					sendnumeric(sptr, ERR_SILELISTFULL, mask);
				return -1;
			}
			else
			{
				if (match_simple(lp->value.cp, mask))
					return -1;
			}
		else if (!mycmp(lp->value.cp, mask))
			return -1;
	}
	lp = make_link();
	memset(lp, 0, sizeof(Link));
	lp->next = sptr->user->silence;
	safe_strdup(lp->value.cp, mask);
	sptr->user->silence = lp;
	return 0;
}

/*
 * is_silenced : Does the actual check wether sptr is allowed
 *               to send a message to acptr.
 *               Both must be registered persons.
 * If sptr is silenced by acptr, his message should not be propagated,
 * but more over, if this is detected on a server not local to sptr
 * the SILENCE mask is sent upstream.
 */
int _is_silenced(Client *sptr, Client *acptr)
{
	Link *lp;
	static char sender[HOSTLEN + NICKLEN + USERLEN + 5];

	if (!acptr->user || !sptr->user || !(lp = acptr->user->silence))
		return 0;

	ircsnprintf(sender, sizeof(sender), "%s!%s@%s", sptr->name, sptr->user->username, GetHost(sptr));

	for (; lp; lp = lp->next)
	{
		if (match_simple(lp->value.cp, sender))
		{
			if (!MyConnect(sptr))
			{
				sendto_one(sptr->direction, NULL, ":%s SILENCE %s :%s",
				    acptr->name, sptr->name, lp->value.cp);
				lp->flags = 1;
			}
			return 1;
		}
	}
	return 0;
}

