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

Cmode *Channelmode_Table = NULL;
unsigned short Channelmode_highest = 0;

Cmode_t EXTMODE_NONOTICE = 0L;

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
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].paracount && Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
	
	/* type 4: paramless modes */
	p = extchmstr[3];
	for (i=0; i <= Channelmode_highest; i++)
		if (!Channelmode_Table[i].paracount && Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
}

static void load_extendedchanmodes(void)
{
	CmodeInfo req;
	memset(&req, 0, sizeof(req));
	
	req.paracount = 0;
	req.is_ok = extcmode_default_requirechop;
	req.flag = 'T';
	CmodeAdd(NULL, req, &EXTMODE_NONOTICE);
}

void	extcmode_init(void)
{
	Cmode_t val = 1;
	int	i;
	Channelmode_Table = (Cmode *)MyMalloc(sizeof(Cmode) * EXTCMODETABLESZ);
	bzero(Channelmode_Table, sizeof(Cmode) * EXTCMODETABLESZ);
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		Channelmode_Table[i].mode = val;
		val *= 2;
	}
	Channelmode_highest = 0;
	memset(&extchmstr, 0, sizeof(extchmstr));

	/* And load the build-in extended chanmodes... */
	load_extendedchanmodes();
}

Cmode *CmodeAdd(Module *reserved, CmodeInfo req, Cmode_t *mode)
{
	short i = 0, j = 0;

	while (i < EXTCMODETABLESZ)
	{
		if (!Channelmode_Table[i].flag)
			break;
		i++;
	}
	if (i == EXTCMODETABLESZ)
	{
		Debug((DEBUG_DEBUG, "CmodeAdd failed, no space"));
		if (reserved)
			reserved->errorcode = MODERR_NOSPACE;
		return NULL;
	}
	*mode = Channelmode_Table[i].mode;
	/* Update extended channel mode table highest */
	Channelmode_Table[i].flag = req.flag;
	Channelmode_Table[i].paracount = req.paracount;
	Channelmode_Table[i].is_ok = req.is_ok;
	Channelmode_Table[i].put_param = req.put_param;
	Channelmode_Table[i].get_param = req.get_param;
	Channelmode_Table[i].conv_param = req.conv_param;
	Channelmode_Table[i].free_param = req.free_param;
	Channelmode_Table[i].dup_struct = req.dup_struct;
	Channelmode_Table[i].sjoin_check = req.sjoin_check;
	for (j = 0; j < EXTCMODETABLESZ; j++)
		if (Channelmode_Table[j].flag)
			if (j > Channelmode_highest)
				Channelmode_highest = j;
	make_cmodestr();
	make_extcmodestr();
	return &(Channelmode_Table[i]);
}

void CmodeDel(Cmode *cmode)
{
	/* TODO: remove from all channel */
	if (cmode)
		cmode->flag = '\0';
	/* Not unloadable, so module object support is not needed (yet) */
}

/** searches in chptr extmode parameters and returns entry or NULL. */
CmodeParam *extcmode_get_struct(CmodeParam *p, char ch)
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
CmodeParam *extcmode_duplicate_paramlist(CmodeParam *lst)
{
	int i;
	Cmode *tbl;
	CmodeParam *head = NULL, *n;

	while(lst)
	{
		tbl = NULL;
		for (i=0; i <= Channelmode_highest; i++)
		{
			if (Channelmode_Table[i].flag == lst->flag)
			{
				tbl = &Channelmode_Table[i]; /* & ? */
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

void extcmode_free_paramlist(CmodeParam *lst)
{
	CmodeParam *n;
	int i;
	Cmode *tbl;

	while(lst)
	{
		/* first remove it from the list... */
		n = lst;
		DelListItem(n, lst);
		/* then hunt for the param free function and let it free */
		tbl = NULL;
		for (i=0; i <= Channelmode_highest; i++)
		{
			if (Channelmode_Table[i].flag == n->flag)
			{
				tbl = &Channelmode_Table[i]; /* & ? */
				break;
			}
		}
		tbl->free_param(n);
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
