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

#include "unrealircd.h"

// FIXME: move this to cmd_whowas,
// Consider making add_history an efunc? Or via a hook?
// Some users may not want to load cmd_whowas at all.

/* internally defined function */
static void add_whowas_to_clist(WhoWas **, WhoWas *);
static void del_whowas_from_clist(WhoWas **, WhoWas *);
static void add_whowas_to_list(WhoWas **, WhoWas *);
static void del_whowas_from_list(WhoWas **, WhoWas *);

WhoWas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
WhoWas MODVAR *WHOWASHASH[WHOWAS_HASH_TABLE_SIZE];

MODVAR int whowas_next = 0;

void add_history(Client *client, int online)
{
	WhoWas *new;

	new = &WHOWAS[whowas_next];

	if (new->hashv != -1)
	{
		safe_free(new->name);
		safe_free(new->hostname);
		safe_free(new->virthost);
		safe_free(new->realname);
		safe_free(new->username);
		safe_free(new->account);
		new->servername = NULL;

		if (new->online)
			del_whowas_from_clist(&(new->online->user->whowas), new);
		del_whowas_from_list(&WHOWASHASH[new->hashv], new);
	}
	new->hashv = hash_whowas_name(client->name);
	new->logoff = TStime();
	new->umodes = client->umodes;
	safe_strdup(new->name, client->name);
	safe_strdup(new->username, client->user->username);
	safe_strdup(new->hostname, client->user->realhost);
	safe_strdup(new->ip, client->ip);
	if (client->user->virthost)
		safe_strdup(new->virthost, client->user->virthost);
	else
		safe_strdup(new->virthost, "");
	new->servername = client->user->server;
	safe_strdup(new->realname, client->info);
	if (strcmp(client->user->account, "0"))
		safe_strdup(new->account, client->user->account);

	/* Its not string copied, a pointer to the scache hash is copied
	   -Dianora
	 */
	/*  strlcpy(new->servername, client->user->server,HOSTLEN); */
	new->servername = client->user->server;

	if (online)
	{
		new->online = client;
		add_whowas_to_clist(&(client->user->whowas), new);
	}
	else
		new->online = NULL;
	add_whowas_to_list(&WHOWASHASH[new->hashv], new);
	whowas_next++;
	if (whowas_next == NICKNAMEHISTORYLENGTH)
		whowas_next = 0;
}

void off_history(Client *client)
{
	WhoWas *temp, *next;

	for (temp = client->user->whowas; temp; temp = next)
	{
		next = temp->cnext;
		temp->online = NULL;
		del_whowas_from_clist(&(client->user->whowas), temp);
	}
}

Client *get_history(const char *nick, time_t timelimit)
{
	WhoWas *temp;
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
	WhoWas *tmp;
	int  i;
	int  u = 0;
	u_long um = 0;
	/* count the number of used whowas structs in 'u' */
	/* count up the memory used of whowas structs in um */

	for (i = 0, tmp = &WHOWAS[0]; i < NICKNAMEHISTORYLENGTH; i++, tmp++)
		if (tmp->hashv != -1)
		{
			u++;
			um += sizeof(WhoWas);
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
		memset(&WHOWAS[i], 0, sizeof(WhoWas));
		WHOWAS[i].hashv = -1;
	}
	for (i = 0; i < WHOWAS_HASH_TABLE_SIZE; i++)
		WHOWASHASH[i] = NULL;
}

static void add_whowas_to_clist(WhoWas ** bucket, WhoWas * whowas)
{
	whowas->cprev = NULL;
	if ((whowas->cnext = *bucket) != NULL)
		whowas->cnext->cprev = whowas;
	*bucket = whowas;
}

static void del_whowas_from_clist(WhoWas ** bucket, WhoWas * whowas)
{
	if (whowas->cprev)
		whowas->cprev->cnext = whowas->cnext;
	else
		*bucket = whowas->cnext;
	if (whowas->cnext)
		whowas->cnext->cprev = whowas->cprev;
}

static void add_whowas_to_list(WhoWas ** bucket, WhoWas * whowas)
{
	whowas->prev = NULL;
	if ((whowas->next = *bucket) != NULL)
		whowas->next->prev = whowas;
	*bucket = whowas;
}

static void del_whowas_from_list(WhoWas ** bucket, WhoWas * whowas)
{
	if (whowas->prev)
		whowas->prev->next = whowas->next;
	else
		*bucket = whowas->next;
	if (whowas->next)
		whowas->next->prev = whowas->prev;
}
