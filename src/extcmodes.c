/************************************************************************
 *   IRC - Internet Relay Chat, s_unreal.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"
#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

#ifdef EXTCMODE

extern char cmodestring[512];

extern void make_cmodestr(void);

char extchmstr[4][64];

aExtCMtable *ExtCMode_Table = NULL;
unsigned short ExtCMode_highest = 0;

void make_extcmodestr()
{
char *p;
int i;
	
	extchmstr[0][0] = extchmstr[1][0] = extchmstr[2][0] = extchmstr[3][0] = '\0';
	
	/* type 1: lists (like b/e) */
	/* [NOT IMPLEMENTED IN EXTCMODES] */

	/* type 2: 1 par to set/unset */
	/* [NOT IMPLEMENTED] */

	/* type 3: 1 param to set, 0 params to unset */
	p = extchmstr[2];
	for (i=0; i <= ExtCMode_highest; i++)
		if (ExtCMode_Table[i].paracount && ExtCMode_Table[i].flag)
			*p++ = ExtCMode_Table[i].flag;
	*p = '\0';
	
	/* type 4: paramless modes */
	p = extchmstr[3];
	for (i=0; i <= ExtCMode_highest; i++)
		if (!ExtCMode_Table[i].paracount && ExtCMode_Table[i].flag)
			*p++ = ExtCMode_Table[i].flag;
	*p = '\0';
	printf("dect: %s/%s/%s/%s\n", extchmstr[0], extchmstr[1], extchmstr[2], extchmstr[3]);
}


void	extcmode_init(void)
{
	ExtCMode val = 1;
	int	i;
	ExtCMode_Table = (aExtCMtable *)MyMalloc(sizeof(aExtCMtable) * EXTCMODETABLESZ);
	bzero(ExtCMode_Table, sizeof(aExtCMtable) * EXTCMODETABLESZ);
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		ExtCMode_Table[i].mode = val;
		val *= 2;
	}
	ExtCMode_highest = 0;
	memset(&extchmstr, 0, sizeof(extchmstr));
}

ExtCMode	extcmode_get(aExtCMtable *req)
{
	short	 i = 0, j = 0;
	ExtCMode tmp;
	
	while (i < EXTCMODETABLESZ)
	{
		if (!ExtCMode_Table[i].flag)
			break;
		i++;
	}
	if (i == EXTCMODETABLESZ)
	{
		Debug((DEBUG_DEBUG, "extcmode_get failed, no space"));
		return 0;
	}

	tmp = ExtCMode_Table[i].mode;
	memcpy(&ExtCMode_Table[i], req, sizeof(aExtCMtable));
	ExtCMode_Table[i].mode = tmp;
	Debug((DEBUG_DEBUG, "extcmode_get(flag = '%c') returning %04x",
		req->flag, ExtCMode_Table[i].mode));
	/* Update extended channel mode table highest */
	for (j = 0; j < EXTCMODETABLESZ; j++)
		if (ExtCMode_Table[j].flag)
			if (j > ExtCMode_highest)
				ExtCMode_highest = j;
	make_cmodestr();
	make_extcmodestr();
	return (ExtCMode_Table[i].mode);
}


int	extcmode_delete(char ch)
{
	int i = 0;
	Debug((DEBUG_DEBUG, "extcmode_delete %c", ch));
	
	/* TODO: remove from all channels */
	
	while (i < EXTCMODETABLESZ)
	{
		if (ExtCMode_Table[i].flag == ch)
		{
			ExtCMode_Table[i].flag = '\0';
			return 1;
		}	
		i++;
	}
	return -1;
}

/** searches in chptr extmode parameters and returns entry or NULL. */
aExtCMtableParam *extcmode_get_struct(aExtCMtableParam *p, char ch)
{

	while(p)
	{
		if (p->flag == ch)
			return p;
		p = p->next;
	}
	return NULL;
}

/* bit inefficient :/ */
aExtCMtableParam *extcmode_duplicate_paramlist(aExtCMtableParam *lst)
{
int i;
aExtCMtable *tbl;
aExtCMtableParam *head = NULL, *n;

	while(lst)
	{
		tbl = NULL;
		for (i=0; i <= ExtCMode_highest; i++)
		{
			if (ExtCMode_Table[i].flag == lst->flag)
			{
				tbl = &ExtCMode_Table[i]; /* & ? */
				break;
			}
		}
		n = tbl->dup_struct(lst);
		if (head)
		{
			AddListItem(n, head);
		} else {
			head = n;
		}
		lst = lst->next;
	}
	return head;
}

void extcmode_free_paramlist(aExtCMtableParam *lst)
{
aExtCMtableParam *n; /* prolly not needed but I'm afraid of aliasing and stuff :p */

	while(lst)
	{
		n = lst;
		DelListItem(n, lst);
	}
}

int extcmode_default_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return 1;
	return 0;
}

int extcmode_default_requirehalfop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) &&
	    (is_chan_op(cptr, chptr) || is_half_op(cptr, chptr)))
		return 1;
	return 0;
}

#endif /* EXTCMODE */
