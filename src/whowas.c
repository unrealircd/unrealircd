/************************************************************************
 *   IRC - Internet Relay Chat, ircd/whowas.c
 *   Copyright (C) 1990 Markku Savela
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

/*
 * --- avalon --- 6th April 1992
 * rewritten to scrap linked lists and use a table of structures which
 * is referenced like a circular loop. Should be faster and more efficient.
 */

#ifndef lint
static  char sccsid[] = "@(#)whowas.c	2.16 08 Nov 1993 (C) 1988 Markku Savela";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "whowas.h"
#include "h.h"

ID_CVS("$Id$");

static	aName	was[NICKNAMEHISTORYLENGTH];
static	int	ww_index = 0;

void	add_history(cptr)
Reg1	aClient	*cptr;
{
	aName	ntmp;
	Reg2	aName	*np = &ntmp, *np2;
	Link	*lp;

	strncpyzt(np->ww_nick, cptr->name, NICKLEN+1);
	strncpyzt(np->ww_info, cptr->info, REALLEN+1);
	np->ww_user = cptr->user;
	np->ww_logout = TStime();
	np->ww_online = (cptr->from != NULL) ? cptr : NULL;
	np->ww_user->refcnt++;

	np2 = &was[ww_index];
	if (np2->ww_user)
		free_user(np2->ww_user, np2->ww_online);
	/*
	 * New whowas handling, we keep a list of what whowas entries
	 * are "used" by a client, in its cptr structure.  This means
	 * that when we overwrite a whowas entry, we have to remove the
	 * relative pointer in the client. -Cabal95
	 */
	if (np2->ww_online) {
		Link	*last = NULL;

		for (lp = np2->ww_online->history; lp;
		     last = lp, lp = lp->next)
			if (lp->value.whowas == np2)
				break;

		if (lp) {	/* Sanity check, never trust anything */
			if (last)
				last->next = lp->next;
			else
				np2->ww_online->history = lp->next;

			free_link(lp);
		}
	}

	bcopy((char *)&ntmp, (char *)np2, sizeof(aName));

	/*
	 * Add this whowas entry into the clients history list
	 */
	lp = make_link();
	lp->value.whowas = np2;
	lp->next = cptr->history;
	cptr->history = lp;

	ww_index++;
	if (ww_index >= NICKNAMEHISTORYLENGTH)
		ww_index = 0;
	return;
}

/*
** get_history
**      Return the current client that was using the given
**      nickname within the timelimit. Returns NULL, if no
**      one found...
*/
aClient *get_history(nick, timelimit)
char    *nick;
time_t  timelimit;
{
        Reg1    aName   *wp, *wp2;
        Reg2    int     i = 0;

	if (ww_index == 0)
	  wp = wp2 = &was[NICKNAMEHISTORYLENGTH - 1];
	  else
	  wp = wp2 = &was[ww_index - 1];
        timelimit = TStime()-timelimit;

	do {
		if (!mycmp(nick, wp->ww_nick) && wp->ww_logout >= timelimit)
			break;
		if (wp == was)
		{
		  i = 1;
		  wp = &was[NICKNAMEHISTORYLENGTH - 1];
		}
		else
		  wp--;
	} while (wp != wp2);

	if (wp != wp2 || !i)
		return (wp->ww_online);
	return (NULL);
}

void	off_history(cptr)
Reg3	aClient	*cptr;
{
	Reg1	Link	*lp;
	Reg2	Link	*next;


	for (lp = cptr->history; lp; lp = next) {
		next = lp->next;
		lp->value.whowas->ww_online = NULL;
		free_link(lp);
	}

	cptr->history = NULL;

	return;
}

void	initwhowas()
{
	Reg1	int	i;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		bzero((char *)&was[i], sizeof(aName));
	return;
}


/*
** m_whowas
**	parv[0] = sender prefix
**	parv[1] = nickname queried
*/
int	m_whowas(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
	Reg1	aName	*wp, *wp2 = NULL;
	Reg2	int	j = 0;
	Reg3	anUser	*up = NULL;
	int	max = -1;
	char	*p, *nick, *s;

 	if (parc < 2)
	    {
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
			   me.name, parv[0]);
		return 0;
	    }
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr,sptr,":%s WHOWAS %s %s :%s", 3,parc,parv))
			return 0;

	for (s = parv[1]; (nick = strtoken(&p, s, ",")); s = NULL)
	    {
		wp = wp2 = &was[ww_index - 1];

		do {
			if (wp < was)
				wp = &was[NICKNAMEHISTORYLENGTH - 1];
			if (mycmp(nick, wp->ww_nick) == 0)
			    {
				up = wp->ww_user;
				sendto_one(sptr, rpl_str(RPL_WHOWASUSER),
					   me.name, parv[0], wp->ww_nick,
					   up->username,
				(IsOper(sptr) ? 
				 up->realhost     :
				  (up->virthost[0] != '\0') ?
				    up->virthost : up->realhost), 
					    wp->ww_info);
				sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
					   me.name, parv[0], wp->ww_nick,
					   up->server, myctime(wp->ww_logout));
				if (up->away)
					sendto_one(sptr, rpl_str(RPL_AWAY),
						   me.name, parv[0],
						   wp->ww_nick, up->away);
				j++;
			    }
			if (max > 0 && j >= max)
				break;
			wp--;
		} while (wp != wp2);

		if (up == NULL)
			sendto_one(sptr, err_str(ERR_WASNOSUCHNICK),
				   me.name, parv[0], nick);
		up=NULL;

		if (p)
			p[-1] = ',';
	    }
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
	return 0;
    }


void	count_whowas_memory(wwu, wwa, wwam)
int	*wwu, *wwa;
u_long	*wwam;
{
	Reg1	anUser	*tmp;
	Reg2	int	i, j;
	int	u = 0, a = 0;
	u_long	am = 0;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
		if ((tmp = was[i].ww_user))
			if (!was[i].ww_online)
			    {
				for (j = 0; j < i; j++)
					if (was[j].ww_user == tmp)
						break;
				if (j < i)
					continue;
				u++;
				if (tmp->away)
				    {
					a++;
					am += (strlen(tmp->away)+1);
				    }
			    }
	*wwu = u;
	*wwa = a;
	*wwam = am;

	return;
}
