/************************************************************************
 *   Unreal Internet Relay Chat Daemo, src/aln.c
 *   (C) 2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *   Copyright (C) 2000 Lucas Madar [bahamut team]
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

#ifndef STANDALONE
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#endif

#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#ifndef STANDALONE
#include "h.h"
#include "proto.h"
ID_Copyright("(C) Carsten Munk 2000");
#endif

static inline char *int_to_base64(long);
static inline long base64_to_int(char *);


Link *Servers = NULL;

char *base64enc(long i)
{
	if (i < 0)
		return ("0");
	return int_to_base64(i);
}

long base64dec(char *b64)
{
	if (b64)
		return base64_to_int(b64);
	else
		return 0;
}

int  numeric_collides(long numeric)
{
	Link *lp;

	if (!numeric)
		return 0;

	for (lp = Servers; lp; lp = lp->next)
		if (numeric == lp->value.cptr->serv->numeric)
			return 1;
	return 0;
}

void add_server_to_table(aClient *what)
{
	Link *ptr;

	if (IsServer(what) || IsMe(what))
	{
		ptr = make_link();
		ptr->value.cptr = what;
		ptr->flags = what->serv->numeric;
		ptr->next = Servers;
		Servers = ptr;
	}
}

void remove_server_from_table(aClient *what)
{
	Link **curr;
	Link *tmp;
	Link *lp = Servers;

	for (; lp && (lp->value.cptr == what); lp = lp->next);
	for (;;)
	{
		for (curr = &Servers; (tmp = *curr); curr = &tmp->next)
			if (tmp->value.cptr == what)
			{
				*curr = tmp->next;
				free_link(tmp);
				break;
			}
		if (lp)
			break;
	}
}

aClient *find_server_by_numeric(long value)
{
	Link *lp;

	for (lp = Servers; lp; lp = lp->next)
		if (lp->value.cptr->serv->numeric == value)
			return (lp->value.cptr);
	return NULL;
}

aClient *find_server_by_base64(char *b64)
{
	if (b64)
		return find_server_by_numeric(base64dec(b64));
	else
		return NULL;
}

char *find_server_id(aClient *which)
{
	return (base64enc(which->serv->numeric));
}

aClient *find_server_quick_search(char *name)
{
	Link *lp;

	for (lp = Servers; lp; lp = lp->next)
		if (!match(name, lp->value.cptr->name))
			return (lp->value.cptr);
	return NULL;
}


aClient *find_server_quick_straight(char *name)
{
	Link *lp;

	for (lp = Servers; lp; lp = lp->next)
		if (!strcmp(name, lp->value.cptr->name))
			return (lp->value.cptr);
	return NULL;
}



aClient *find_server_quickx(char *name, aClient *cptr)
{
	if (name)
		cptr = (aClient *)find_server_quick_search(name);
	return cptr;
}


aClient *find_server_b64_or_real(char *name)
{
	Link *lp;
	long  namebase64;
	
	if (!name)
		return NULL;
	
	if (strlen(name) < 3)
	{
		namebase64 = base64dec(name);	
		for (lp = Servers; lp; lp = lp->next)
			if (lp->value.cptr->serv->numeric == namebase64)
				return (lp->value.cptr);
	}
	else
		return find_server_quick_straight(name);
	return NULL;
	
}

/* ':' and '#' and '&' and '+' and '@' must never be in this table. */
/* these tables must NEVER CHANGE! >) */
char int6_to_base64_map[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D',
	    'E', 'F',
	'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	    'U', 'V',
	'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
	    'k', 'l',
	'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	    '{', '}'
};

char base64_to_int6_map[] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
	-1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, -1, -1, -1, -1, -1,
	-1, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
	51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, -1, 63, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

static inline char *int_to_base64(long val)
{
	/* 32/6 == max 6 bytes for representation, 
	 * +1 for the null, +1 for byte boundaries 
	 */
	static char base64buf[8];
	long i = 7;

	base64buf[i] = '\0';

	/* Temporary debugging code.. remove before 2038 ;p.
	 * This might happen in case of 64bit longs (opteron/ia64),
	 * if the value is then too large it can easily lead to
	 * a buffer underflow and thus to a crash. -- Syzop
	 */
	if (val > 2147483647L)
	{
		abort();
	}

	do
	{
		base64buf[--i] = int6_to_base64_map[val & 63];
	}
	while (val >>= 6);

	return base64buf + i;
}

static inline long base64_to_int(char *b64)
{
	int v = base64_to_int6_map[(u_char)*b64++];

	if (!b64)
		return 0;
		
	while (*b64)
	{
		v <<= 6;
		v += base64_to_int6_map[(u_char)*b64++];
	}

	return v;
}


void ns_stats(aClient *cptr)
{
	Link *lp;
	aClient *sptr;
	for (lp = Servers; lp; lp = lp->next)
	{
		sptr = lp->value.cptr;
		sendto_one(cptr,
		    ":%s NOTICE %s :*** server=%s numeric=%i b64=%s [%s]", me.name,
		    cptr->name, sptr->name, sptr->serv->numeric,
		    find_server_id(sptr), find_server_b64_or_real(find_server_id(sptr)) == sptr ? "SANE" : "INSANE");
	}
}

