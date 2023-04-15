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

void add_whowas_to_clist(WhoWas **, WhoWas *);
void del_whowas_from_clist(WhoWas **, WhoWas *);
void add_whowas_to_list(WhoWas **, WhoWas *);
void del_whowas_from_list(WhoWas **, WhoWas *);

WhoWas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
WhoWas MODVAR *WHOWASHASH[WHOWAS_HASH_TABLE_SIZE];

MODVAR int whowas_next = 0;

void free_whowas_fields(WhoWas *e)
{
	safe_free(e->name);
	safe_free(e->hostname);
	safe_free(e->virthost);
	safe_free(e->realname);
	safe_free(e->username);
	safe_free(e->account);
	safe_free(e->ip);
	e->servername = NULL;

	if (e->online)
		del_whowas_from_clist(&(e->online->user->whowas), e);
	del_whowas_from_list(&WHOWASHASH[e->hashv], e);
}

void create_whowas_entry(Client *client, WhoWas *e)
{
	e->hashv = hash_whowas_name(client->name);
	e->logoff = TStime();
	e->umodes = client->umodes;
	safe_strdup(e->name, client->name);
	safe_strdup(e->username, client->user->username);
	safe_strdup(e->hostname, client->user->realhost);
	safe_strdup(e->ip, client->ip);
	if (client->user->virthost)
		safe_strdup(e->virthost, client->user->virthost);
	else
		safe_strdup(e->virthost, "");
	e->servername = client->user->server;
	safe_strdup(e->realname, client->info);
	if (strcmp(client->user->account, "0"))
		safe_strdup(e->account, client->user->account);

	/* Its not string copied, a pointer to the scache hash is copied
	   -Dianora
	 */
	/*  strlcpy(e->servername, client->user->server,HOSTLEN); */
	e->servername = client->user->server;
}

void add_history(Client *client, int online)
{
	WhoWas *new;

	new = &WHOWAS[whowas_next];

	if (new->hashv != -1)
		free_whowas_fields(new);

	create_whowas_entry(client, new);

	if (online)
	{
		new->online = client;
		add_whowas_to_clist(&(client->user->whowas), new);
	} else {
		new->online = NULL;
	}
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

void add_whowas_to_clist(WhoWas ** bucket, WhoWas * whowas)
{
	whowas->cprev = NULL;
	if ((whowas->cnext = *bucket) != NULL)
		whowas->cnext->cprev = whowas;
	*bucket = whowas;
}

void del_whowas_from_clist(WhoWas ** bucket, WhoWas * whowas)
{
	if (whowas->cprev)
		whowas->cprev->cnext = whowas->cnext;
	else
		*bucket = whowas->cnext;
	if (whowas->cnext)
		whowas->cnext->cprev = whowas->cprev;
}

void add_whowas_to_list(WhoWas ** bucket, WhoWas * whowas)
{
	whowas->prev = NULL;
	if ((whowas->next = *bucket) != NULL)
		whowas->next->prev = whowas;
	*bucket = whowas;
}

void del_whowas_from_list(WhoWas ** bucket, WhoWas * whowas)
{
	if (whowas->prev)
		whowas->prev->next = whowas->next;
	else
		*bucket = whowas->next;
	if (whowas->next)
		whowas->next->prev = whowas->prev;
}
