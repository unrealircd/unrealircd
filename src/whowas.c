
/************************************************************************
*   IRC - Internet Relay Chat, src/whowas.c
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "h.h"
#include "hash.h"

/* externally defined functions */
unsigned int hash_whowas_name(char *);	/* defined in hash.c */

/* internally defined function */
static void add_whowas_to_clist(aWhowas **, aWhowas *);
static void del_whowas_from_clist(aWhowas **, aWhowas *);
static void add_whowas_to_list(aWhowas **, aWhowas *);
static void del_whowas_from_list(aWhowas **, aWhowas *);

aWhowas WHOWAS[NICKNAMEHISTORYLENGTH];
aWhowas *WHOWASHASH[WW_MAX];

int  whowas_next = 0;
#define AllocCpy(x,y) x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)
#define SafeFree(x) if (x) { MyFree((x)); (x) = NULL; }

void add_history(aClient *cptr, int online)
{
	aWhowas *new;

	new = &WHOWAS[whowas_next];

	if (new->hashv != -1)
	{
		SafeFree(new->name);
		SafeFree(new->hostname);
		SafeFree(new->virthost);
		SafeFree(new->realname);
		SafeFree(new->username);
		SafeFree(new->away);
		new->servername = NULL;

		if (new->online)
			del_whowas_from_clist(&(new->online->whowas), new);
		del_whowas_from_list(&WHOWASHASH[new->hashv], new);
	}
	new->hashv = hash_whowas_name(cptr->name);
	new->logoff = TStime();
	new->umodes = cptr->umodes;
	AllocCpy(new->name, cptr->name);
	AllocCpy(new->username, cptr->user->username);
	AllocCpy(new->hostname, cptr->user->realhost);
	AllocCpy(new->virthost, cptr->user->virthost);
	if (cptr->user->away)
	{
		AllocCpy(new->away, cptr->user->away);
	}
	else
		new->away = NULL;
	new->servername = cptr->user->server;
	AllocCpy(new->realname, cptr->info);

	/* Its not string copied, a pointer to the scache hash is copied
	   -Dianora
	 */
	/*  strncpyzt(new->servername, cptr->user->server,HOSTLEN); */
	new->servername = cptr->user->server;

	if (online)
	{
		new->online = cptr;
		add_whowas_to_clist(&(cptr->whowas), new);
	}
	else
		new->online = NULL;
	add_whowas_to_list(&WHOWASHASH[new->hashv], new);
	whowas_next++;
	if (whowas_next == NICKNAMEHISTORYLENGTH)
		whowas_next = 0;
}

void off_history(aClient *cptr)
{
	aWhowas *temp, *next;

	for (temp = cptr->whowas; temp; temp = next)
	{
		next = temp->cnext;
		temp->online = NULL;
		del_whowas_from_clist(&(cptr->whowas), temp);
	}
}

aClient *get_history(char *nick, time_t timelimit)
{
	aWhowas *temp;
	int  blah;

	timelimit = TStime() - timelimit;
	blah = hash_whowas_name(nick);
	temp = WHOWASHASH[blah];
	for (; temp; temp = temp->next)
	{
		if (mycmp(nick, temp->name))
			continue;
		if (temp->logoff < timelimit)
			continue;
		return temp->online;
	}
	return NULL;
}

void count_whowas_memory(int *wwu, u_long *wwum)
{
	aWhowas *tmp;
	int  i;
	int  u = 0;
	u_long um = 0;
	u_long uam = 0;
	/* count the number of used whowas structs in 'u' */
	/* count up the memory used of whowas structs in um */

	for (i = 0, tmp = &WHOWAS[0]; i < NICKNAMEHISTORYLENGTH; i++, tmp++)
		if (tmp->hashv != -1)
		{
			u++;
			um += sizeof(aWhowas);
			if (tmp->away)
				uam += (strlen(tmp->away) + 1);
		}
	*wwu = u;
	*wwum = um;
	return;
}
/*
** m_whowas
**      parv[0] = sender prefix
**      parv[1] = nickname queried
*/
int  m_whowas(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aWhowas *temp;
	int  cur = 0;
	int  max = -1, found = 0;
	char *p, *nick;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		    me.name, parv[0]);
		return 0;
	}
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr, sptr, ":%s WHOWAS %s %s :%s", 3, parc,
		    parv))
			return 0;

	if (!MyConnect(sptr) && (max > 20))
		max = 20;

	p = (char *)strchr(parv[1], ',');
	if (p)
		*p = '\0';
	nick = parv[1];
	temp = WHOWASHASH[hash_whowas_name(nick)];
	found = 0;
	for (; temp; temp = temp->next)
	{
		if (!mycmp(nick, temp->name))
		{
			sendto_one(sptr, rpl_str(RPL_WHOWASUSER),
			    me.name, parv[0], temp->name,
			    temp->username,
			    (IsOper(sptr) ? temp->hostname :
			    (*temp->virthost !=
			    '\0') ? temp->virthost : temp->hostname),
			    temp->realname);
			sendto_one(sptr, rpl_str(RPL_WHOISSERVER), me.name,
			    parv[0], temp->name, temp->servername,
			    myctime(temp->logoff));
			if (temp->away)
				sendto_one(sptr, rpl_str(RPL_AWAY),
				    me.name, parv[0], temp->name, temp->away);
			cur++;
			found++;
		}
		if (max > 0 && cur >= max)
			break;
	}
	if (!found)
		sendto_one(sptr, err_str(ERR_WASNOSUCHNICK),
		    me.name, parv[0], nick);

	sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
	return 0;
}

void initwhowas()
{
	int  i;

	for (i = 0; i < NICKNAMEHISTORYLENGTH; i++)
	{
		bzero((char *)&WHOWAS[i], sizeof(aWhowas));
		WHOWAS[i].hashv = -1;
	}
	for (i = 0; i < WW_MAX; i++)
		WHOWASHASH[i] = NULL;
}


static void add_whowas_to_clist(aWhowas ** bucket, aWhowas * whowas)
{
	whowas->cprev = NULL;
	if ((whowas->cnext = *bucket) != NULL)
		whowas->cnext->cprev = whowas;
	*bucket = whowas;
}

static void del_whowas_from_clist(aWhowas ** bucket, aWhowas * whowas)
{
	if (whowas->cprev)
		whowas->cprev->cnext = whowas->cnext;
	else
		*bucket = whowas->cnext;
	if (whowas->cnext)
		whowas->cnext->cprev = whowas->cprev;
}

static void add_whowas_to_list(aWhowas ** bucket, aWhowas * whowas)
{
	whowas->prev = NULL;
	if ((whowas->next = *bucket) != NULL)
		whowas->next->prev = whowas;
	*bucket = whowas;
}

static void del_whowas_from_list(aWhowas ** bucket, aWhowas * whowas)
{

	if (whowas->prev)
		whowas->prev->next = whowas->next;
	else
		*bucket = whowas->next;
	if (whowas->next)
		whowas->next->prev = whowas->prev;
}
