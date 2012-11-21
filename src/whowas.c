
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
#include "proto.h"
#include "msg.h"
#include <string.h>

/* externally defined functions */
unsigned int hash_whowas_name(char *);	/* defined in hash.c */

/* internally defined function */
static void add_whowas_to_clist(aWhowas **, aWhowas *);
static void del_whowas_from_clist(aWhowas **, aWhowas *);
static void add_whowas_to_list(aWhowas **, aWhowas *);
static void del_whowas_from_list(aWhowas **, aWhowas *);

aWhowas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
aWhowas MODVAR *WHOWASHASH[WW_MAX];

MODVAR int  whowas_next = 0;
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
		new->servername = NULL;

		if (new->online)
			del_whowas_from_clist(&(new->online->user->whowas), new);
		del_whowas_from_list(&WHOWASHASH[new->hashv], new);
	}
	new->hashv = hash_whowas_name(cptr->name);
	new->logoff = TStime();
	new->umodes = cptr->umodes;
	AllocCpy(new->name, cptr->name);
	AllocCpy(new->username, cptr->user->username);
	AllocCpy(new->hostname, cptr->user->realhost);
	if (cptr->user->virthost)
	{
		AllocCpy(new->virthost, cptr->user->virthost);
	}
	else
		new->virthost = strdup("");
	new->servername = cptr->user->server;
	AllocCpy(new->realname, cptr->info);

	/* Its not string copied, a pointer to the scache hash is copied
	   -Dianora
	 */
	/*  strlcpy(new->servername, cptr->user->server,HOSTLEN); */
	new->servername = cptr->user->server;

	if (online)
	{
		new->online = cptr;
		add_whowas_to_clist(&(cptr->user->whowas), new);
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

	for (temp = cptr->user->whowas; temp; temp = next)
	{
		next = temp->cnext;
		temp->online = NULL;
		del_whowas_from_clist(&(cptr->user->whowas), temp);
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
	/* count the number of used whowas structs in 'u' */
	/* count up the memory used of whowas structs in um */

	for (i = 0, tmp = &WHOWAS[0]; i < NICKNAMEHISTORYLENGTH; i++, tmp++)
		if (tmp->hashv != -1)
		{
			u++;
			um += sizeof(aWhowas);
		}
	*wwu = u;
	*wwum = um;
	return;
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
