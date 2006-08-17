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

/* Channel parameter to slot# mapping */
unsigned char param_to_slot_mapping[256];

extern void make_cmodestr(void);

char extchmstr[4][64];

Cmode *Channelmode_Table = NULL;
unsigned short Channelmode_highest = 0;

Cmode *ParamTable[MAXPARAMMODES+1];

Cmode_t EXTMODE_NONOTICE = 0L;
#ifdef STRIPBADWORDS
Cmode_t EXTMODE_STRIPBADWORDS = 0L;
#endif

int extcmode_cmodeT_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what);
#ifdef STRIPBADWORDS
int extcmode_cmodeG_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what);
#endif

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
	req.is_ok = extcmode_cmodeT_requirechop;
	req.flag = 'T';
	CmodeAdd(NULL, req, &EXTMODE_NONOTICE);
#ifdef STRIPBADWORDS
	req.flag = 'G';
	req.is_ok = extcmode_cmodeG_requirechop;
	CmodeAdd(NULL, req, &EXTMODE_STRIPBADWORDS);
#endif
}

void	extcmode_init(void)
{
	Cmode_t val = 1;
	int	i;
	Channelmode_Table = (Cmode *)MyMalloc(sizeof(Cmode) * EXTCMODETABLESZ);
	bzero(Channelmode_Table, sizeof(Cmode) * EXTCMODETABLESZ);
	
	memset(&ParamTable, 0, sizeof(ParamTable));
	
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		Channelmode_Table[i].mode = val;
		val *= 2;
	}
	Channelmode_highest = 0;
	memset(&extchmstr, 0, sizeof(extchmstr));
	memset(&param_to_slot_mapping, 0, sizeof(param_to_slot_mapping));

	/* And load the build-in extended chanmodes... */
	load_extendedchanmodes();
}

/* Update letter->slot mapping and slot->handler mapping */
void extcmode_para_addslot(Cmode *c, int slot)
{
	if ((slot < 0) || (slot > MAXPARAMMODES))
		abort();
	c->slot = slot;
	ParamTable[slot] = c;
	param_to_slot_mapping[c->flag] = slot;
}

Cmode *CmodeAdd(Module *reserved, CmodeInfo req, Cmode_t *mode)
{
	short i = 0, j = 0;
	int paraslot = -1;
	char tmpbuf[512];

	while (i < EXTCMODETABLESZ)
	{
		if (!Channelmode_Table[i].flag)
			break;
		else if (Channelmode_Table[i].flag == req.flag)
		{
			if (reserved)
				reserved->errorcode = MODERR_EXISTS;
			return NULL;
		}
		i++;
	}
	if (i == EXTCMODETABLESZ)
	{
		Debug((DEBUG_DEBUG, "CmodeAdd failed, no space"));
		if (reserved)
			reserved->errorcode = MODERR_NOSPACE;
		return NULL;
	}
	
	if (req.paracount == 1)
	{
		for (paraslot = 0; ParamTable[paraslot]; paraslot++)
			if (paraslot == MAXPARAMMODES - 1)
			{
				Debug((DEBUG_DEBUG, "CmodeAdd failed, no space for parameter"));
				if (reserved)
					reserved->errorcode = MODERR_NOSPACE;
				return NULL;
			}
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

	if (Channelmode_Table[i].paracount == 1)
		extcmode_para_addslot(&Channelmode_Table[i], paraslot);

	if (reserved)
		reserved->errorcode = MODERR_NOERROR;
	if (loop.ircd_booted)
	{
		make_cmodestr();
		make_extcmodestr();
		ircsprintf(tmpbuf, CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
			EXPAR1, EXPAR2, EXPAR3, EXPAR4);
		IsupportSetValue(IsupportFind("CHANMODES"), tmpbuf);
	}
	return &(Channelmode_Table[i]);
}

void CmodeDel(Cmode *cmode)
{
	char tmpbuf[512];
	/* TODO: remove from all channel */
	if (cmode)
		cmode->flag = '\0';
	make_cmodestr();
	make_extcmodestr();
	/* Not unloadable, so module object support is not needed (yet) */
	ircsprintf(tmpbuf, CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
			EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	IsupportSetValue(IsupportFind("CHANMODES"), tmpbuf);
}

void extcmode_duplicate_paramlist(void **xi, void **xo)
{
	int i;
	Cmode *handler;
	void *inx;

	for (i = 0; i < MAXPARAMMODES; i++)
	{
		handler = CMP_GETHANDLERBYSLOT(i);
		if (!handler)
			continue; /* nothing there.. */
		inx = xi[handler->slot]; /* paramter data of input is here */
		xo[handler->slot] = handler->dup_struct(inx); /* call dup_struct with that input and set the output param to that */
	}
}

void extcmode_free_paramlist(void **ar)
{
int i;
Cmode *handler;

	for (i = 0; i < MAXPARAMMODES; i++)
	{
		handler = GETPARAMHANDLERBYSLOT(i);
		if (!handler)
			continue; /* nothing here... */
		handler->free_param(ar[handler->slot]);
		ar[handler->slot] = NULL;
	}
}

char *cm_getparameter(aChannel *chptr, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCT(chptr, mode));
}

void cm_putparameter(aChannel *chptr, char mode, char *str)
{
	GETPARASTRUCT(chptr, mode) = GETPARAMHANDLERBYLETTER(mode)->put_param(GETPARASTRUCT(chptr, mode), str);
}

void cm_freeparameter(aChannel *chptr, char mode)
{
	GETPARAMHANDLERBYLETTER(mode)->free_param(GETPARASTRUCT(chptr, mode));
	GETPARASTRUCT(chptr, mode) = NULL;
}

char *cm_getparameter_ex(void **p, char mode)
{
	return GETPARAMHANDLERBYLETTER(mode)->get_param(GETPARASTRUCTEX(p, mode));
}

void cm_putparameter_ex(void **p, char mode, char *str)
{
	GETPARASTRUCTEX(p, mode) = GETPARAMHANDLERBYLETTER(mode)->put_param(GETPARASTRUCTEX(p, mode), str);
}

void cm_freeparameter_ex(void **p, char mode, char *str)
{
	GETPARAMHANDLERBYLETTER(mode)->free_param(GETPARASTRUCTEX(p, mode));
	GETPARASTRUCTEX(p, mode) = NULL;
}


/* Ok this is my mistake @ EXCHK_ACCESS_ERR error msg:
 * the is_ok() thing does not know which mode it belongs to,
 * this is normally redundant information of course but in
 * case of a default handler like these, it's required to
 * know which setting of mode failed (the mode char).
 * I just return '?' for now, better than nothing.
 * TO SUMMARIZE: Do not use extcmode_default_requirechop for new modules :p.
 * Obviously in Unreal3.3* we should fix this. -- Syzop
 */

int extcmode_default_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendto_one(cptr, err_str(ERR_NOTFORHALFOPS), me.name, cptr->name, '?');
	return EX_DENY;
}

int extcmode_default_requirehalfop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) &&
	    (is_chan_op(cptr, chptr) || is_half_op(cptr, chptr)))
		return EX_ALLOW;
	return EX_DENY;
}

int extcmode_cmodeT_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendto_one(cptr, err_str(ERR_NOTFORHALFOPS), me.name, cptr->name, 'T');
	return EX_DENY;
}

int extcmode_cmodeG_requirechop(aClient *cptr, aChannel *chptr, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendto_one(cptr, err_str(ERR_NOTFORHALFOPS), me.name, cptr->name, 'G');
	return EX_DENY;
}

#endif /* EXTCMODE */
