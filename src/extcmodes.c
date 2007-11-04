/************************************************************************
 *   IRC - Internet Relay Chat, extcmodes.c
 *   (C) 2003-2007 Bram Matthys (Syzop) and the UnrealIRCd Team
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
#ifdef STRIPBADWORDS
Cmode_t EXTMODE_STRIPBADWORDS = 0L;
#endif

#ifdef JOINTHROTTLE
/* cmode j stuff... */
Cmode_t EXTMODE_JOINTHROTTLE = 0L;
int cmodej_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what);
CmodeParam *cmodej_put_param(CmodeParam *r_in, char *param);
char *cmodej_get_param(CmodeParam *r_in);
char *cmodej_conv_param(char *param_in);
void cmodej_free_param(CmodeParam *r);
CmodeParam *cmodej_dup_struct(CmodeParam *r_in);
int cmodej_sjoin_check(aChannel *chptr, CmodeParam *ourx, CmodeParam *theirx);
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
	
#ifdef JOINTHROTTLE
	/* +j */
	memset(&req, 0, sizeof(req));
	req.paracount = 1;
	req.is_ok = cmodej_is_ok;
	req.flag = 'j';
	req.put_param = cmodej_put_param;
	req.get_param = cmodej_get_param;
	req.conv_param = cmodej_conv_param;
	req.free_param = cmodej_free_param;
	req.dup_struct = cmodej_dup_struct;
	req.sjoin_check = cmodej_sjoin_check;
	CmodeAdd(NULL, req, &EXTMODE_JOINTHROTTLE);
#endif
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

Cmode *CmodeAdd(Module *module, CmodeInfo req, Cmode_t *mode)
{
	short i = 0, j = 0;
	char tmpbuf[512];

	while (i < EXTCMODETABLESZ)
	{
		if (!Channelmode_Table[i].flag)
			break;
		else if (Channelmode_Table[i].flag == req.flag)
		{
			if (Channelmode_Table[i].unloaded)
			{
				Channelmode_Table[i].unloaded = 0;
				break;
			} else {
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
		i++;
	}
	if (i == EXTCMODETABLESZ)
	{
		Debug((DEBUG_DEBUG, "CmodeAdd failed, no space"));
		if (module)
			module->errorcode = MODERR_NOSPACE;
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
	Channelmode_Table[i].owner = module;
	
	for (j = 0; j < EXTCMODETABLESZ; j++)
		if (Channelmode_Table[j].flag)
			if (j > Channelmode_highest)
				Channelmode_highest = j;
	if (module)
	{
		ModuleObject *cmodeobj = MyMallocEx(sizeof(ModuleObject));
		cmodeobj->object.cmode = &Channelmode_Table[i];
		cmodeobj->type = MOBJ_CMODE;
		AddListItem(cmodeobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
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

void unload_extcmode_commit(Cmode *cmode)
{
char tmpbuf[512];
aChannel *chptr;

	if (!cmode)
		return;	
	if (cmode->paracount == 1)
	{
		/* If we don't do this, we will crash anyway.. but then with severe corruption / suckyness */
		ircd_log(LOG_ERROR, "FATAL ERROR: ChannelMode module for chanmode +%c is misbehaving: "
		                    "all chanmode modules with parameters should be tagged PERManent.", cmode->flag);
		abort();
	}

	for (chptr = channel; chptr; chptr = chptr->nextch)
		if (chptr->mode.extmode && cmode->mode)
		{
			/* Unset channel mode and send MODE -<char> to other servers */
			sendto_channel_butserv(chptr, &me, ":%s MODE %s -%c",
				me.name, chptr->chname, cmode->flag);
			sendto_serv_butone(NULL, ":%s MODE %s -%c 0",
				me.name, chptr->chname, cmode->flag);
			chptr->mode.extmode &= ~cmode->mode;
		}	

	cmode->flag = '\0';
	make_cmodestr();
	make_extcmodestr();
	ircsprintf(tmpbuf, CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
			EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	IsupportSetValue(IsupportFind("CHANMODES"), tmpbuf);
}

void CmodeDel(Cmode *cmode)
{
	/* It would be nice if we could abort() here if a parameter module is trying to unload which is extremely dangerous/crashy/disallowed */

	if (loop.ircd_rehashing)
		cmode->unloaded = 1;
	else
		unload_extcmode_commit(cmode);

	if (cmode->owner)
	{
		ModuleObject *cmodeobj;
		for (cmodeobj = cmode->owner->objects; cmodeobj; cmodeobj = cmodeobj->next) {
			if (cmodeobj->type == MOBJ_CMODE && cmodeobj->object.cmode == cmode) {
				DelListItem(cmodeobj, cmode->owner->objects);
				MyFree(cmodeobj);
				break;
			}
		}
		cmode->owner = NULL;
	}
}

void unload_all_unused_extcmodes(void)
{
int i;

	for (i = 0; i < EXTCMODETABLESZ; i++)
		if (Channelmode_Table[i].flag && Channelmode_Table[i].unloaded)
		{
			unload_extcmode_commit(&Channelmode_Table[i]);
		}

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
		n->next = n->prev = NULL; /* safety (required!) */
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

#ifdef JOINTHROTTLE
/*** CHANNEL MODE +j STUFF ******/
int cmodej_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chan_op(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendto_one(sptr, err_str(ERR_NOTFORHALFOPS), me.name, sptr->name, 'j');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		/* Check parameter.. syntax should be X:Y, X should be 1-255, Y should be 1-999 */
		char buf[32], *p;
		int num, t, fail = 0;
		
		strlcpy(buf, para, sizeof(buf));
		p = strchr(buf, ':');
		if (!p)
		{
			fail = 1;
		} else {
			*p++ = '\0';
			num = atoi(buf);
			t = atoi(p);
			if ((num < 1) || (num > 255) || (t < 1) || (t > 999))
				fail = 1;
		}
		if (fail)
		{
			sendnotice(sptr, "Error in setting +j, syntax: +j <num>:<seconds>, where <num> must be 1-255, and <seconds> 1-999");
			return EX_DENY;
		}
		return EX_ALLOW;
	}

	/* falltrough -- should not be used */
	return EX_DENY;
}

CmodeParam *cmodej_put_param(CmodeParam *r_in, char *param)
{
aModejEntry *r = (aModejEntry *)r_in;
char buf[32], *p;
int num, t;

	if (!r)
	{
		/* Need to create one */
		r = (aModejEntry *)malloc(sizeof(aModejEntry));
		memset(r, 0, sizeof(aModejEntry));
		r->flag = 'j';
	}
	strlcpy(buf, param, sizeof(buf));
	p = strchr(buf, ':');
	if (p)
	{
		*p++ = '\0';
		num = atoi(buf);
		t = atoi(p);
		if (num < 1) num = 1;
		if (num > 255) num = 255;
		if (t < 1) t = 1;
		if (t > 999) t = 999;
		r->num = num;
		r->t = t;
	} else {
		r->num = 0;
		r->t = 0;
	}
	return (CmodeParam *)r;
}

char *cmodej_get_param(CmodeParam *r_in)
{
aModejEntry *r = (aModejEntry *)r_in;
static char retbuf[16];

	if (!r)
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "%hu:%hu", r->num, r->t);
	return retbuf;
}

char *cmodej_conv_param(char *param_in)
{
static char retbuf[32];
char param[32], *p;
int num, t, fail = 0;
		
	strlcpy(param, param_in, sizeof(param));
	p = strchr(param, ':');
	if (!p)
		return NULL;
	*p++ = '\0';
	num = atoi(param);
	t = atoi(p);
	if (num < 1)
		num = 1;
	if (num > 255)
		num = 255;
	if (t < 1)
		t = 1;
	if (t > 999)
		t = 999;
	
	snprintf(retbuf, sizeof(retbuf), "%d:%d", num, t);
	return retbuf;
}

void cmodej_free_param(CmodeParam *r)
{
	MyFree(r);
}

CmodeParam *cmodej_dup_struct(CmodeParam *r_in)
{
aModejEntry *r = (aModejEntry *)r_in;
aModejEntry *w = (aModejEntry *)MyMalloc(sizeof(aModejEntry));

	memcpy(w, r, sizeof(aModejEntry));
	w->next = w->prev = NULL;
	return (CmodeParam *)w;
}

int cmodej_sjoin_check(aChannel *chptr, CmodeParam *ourx, CmodeParam *theirx)
{
aModejEntry *our = (aModejEntry *)ourx;
aModejEntry *their = (aModejEntry *)theirx;

	if (our->t != their->t)
	{
		if (our->t > their->t)
			return EXSJ_WEWON;
		else
			return EXSJ_THEYWON;
	}
	else if (our->num != their->num)
	{
		if (our->num > their->num)
			return EXSJ_WEWON;
		else
			return EXSJ_THEYWON;
	} else
		return EXSJ_SAME;
}
#endif /* JOINTHROTTLE */

#endif /* EXTCMODE */
