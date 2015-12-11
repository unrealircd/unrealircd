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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

extern char cmodestring[512];

/* Channel parameter to slot# mapping */
MODVAR unsigned char param_to_slot_mapping[256];

extern void make_cmodestr(void);

char extchmstr[4][64];

Cmode *Channelmode_Table = NULL;
unsigned short Channelmode_highest = 0;

Cmode *ParamTable[MAXPARAMMODES+1];

void make_extcmodestr()
{
char *p;
int i;
	
	extchmstr[0][0] = extchmstr[1][0] = extchmstr[2][0] = extchmstr[3][0] = '\0';
	
	/* type 1: lists (like b/e) */
	/* [NOT IMPLEMENTED IN EXTCMODES] */

	/* type 2: 1 par to set/unset (has .unset_with_param) */
	p = extchmstr[1];
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].paracount && Channelmode_Table[i].flag &&
		    Channelmode_Table[i].unset_with_param)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';

	/* type 3: 1 param to set, 0 params to unset (does not have .unset_with_param) */
	p = extchmstr[2];
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].paracount && Channelmode_Table[i].flag &&
		    !Channelmode_Table[i].unset_with_param)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
	
	/* type 4: paramless modes */
	p = extchmstr[3];
	for (i=0; i <= Channelmode_highest; i++)
		if (!Channelmode_Table[i].paracount && Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
	*p = '\0';
}

static char previous_chanmodes[256];

void extcmodes_check_for_changes(void)
{
	char chanmodes[256];
	Isupport *isup;
	
	make_cmodestr();
	make_extcmodestr();
	ircsnprintf(chanmodes, sizeof(chanmodes), CHPAR1 "%s," CHPAR2 "%s," CHPAR3 "%s," CHPAR4 "%s",
		EXPAR1, EXPAR2, EXPAR3, EXPAR4);
	
	isup = IsupportFind("CHANMODES");
	if (!isup)
	{
		strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
		return; /* not booted yet. then we are done here. */
	}
	
	IsupportSetValue(isup, chanmodes);
	
	if (strcmp(chanmodes, previous_chanmodes))
	{
		ircd_log(LOG_ERROR, "Channel modes changed at runtime: %s -> %s",
			previous_chanmodes, chanmodes);
		sendto_realops("Channel modes changed at runtime: %s -> %s",
			previous_chanmodes, chanmodes);
		/* Broadcast change to all (locally connected) servers */
		sendto_server(&me, 0, 0, "PROTOCTL CHANMODES=%s", chanmodes);
	}

	strlcpy(previous_chanmodes, chanmodes, sizeof(previous_chanmodes));
}

void	extcmode_init(void)
{
	Cmode_t val = 1;
	int	i;
	Channelmode_Table = MyMallocEx(sizeof(Cmode) * EXTCMODETABLESZ);
	for (i = 0; i < EXTCMODETABLESZ; i++)
	{
		Channelmode_Table[i].mode = val;
		val *= 2;
	}
	Channelmode_highest = 0;
	memset(&extchmstr, 0, sizeof(extchmstr));
	memset(&param_to_slot_mapping, 0, sizeof(param_to_slot_mapping));
	*previous_chanmodes = '\0';
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

Cmode *CmodeAdd(Module *module, CmodeInfo req, Cmode_t *mode)
{
	short i = 0, j = 0;
	int paraslot = -1;
	int existing = 0;

	while (i < EXTCMODETABLESZ)
	{
		if (!Channelmode_Table[i].flag)
			break;
		else if (Channelmode_Table[i].flag == req.flag)
		{
			if (Channelmode_Table[i].unloaded)
			{
				Channelmode_Table[i].unloaded = 0;
				existing = 1;
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

	if (req.paracount == 1)
	{
		if (existing)
		{
			/* Re-use parameter slot of the module with the same modechar that is unloading */
			paraslot = Channelmode_Table[i].slot;
		}
		else
		{
			/* Allocate a new one */
			for (paraslot = 0; ParamTable[paraslot]; paraslot++)
			{
				if (paraslot == MAXPARAMMODES - 1)
				{
					Debug((DEBUG_DEBUG, "CmodeAdd failed, no space for parameter"));
					if (module)
						module->errorcode = MODERR_NOSPACE;
					return NULL;
				}
			}
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
	Channelmode_Table[i].local = req.local;
	Channelmode_Table[i].unset_with_param = req.unset_with_param;
	Channelmode_Table[i].owner = module;
	Channelmode_Table[i].unloaded = 0;
	
	for (j = 0; j < EXTCMODETABLESZ; j++)
		if (Channelmode_Table[j].flag)
			if (j > Channelmode_highest)
				Channelmode_highest = j;

        if (Channelmode_Table[i].paracount == 1)
                extcmode_para_addslot(&Channelmode_Table[i], paraslot);
                
	if (module)
	{
		ModuleObject *cmodeobj = MyMallocEx(sizeof(ModuleObject));
		cmodeobj->object.cmode = &Channelmode_Table[i];
		cmodeobj->type = MOBJ_CMODE;
		AddListItem(cmodeobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return &(Channelmode_Table[i]);
}

void unload_extcmode_commit(Cmode *cmode)
{
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
			sendto_server(NULL, 0, 0, ":%s MODE %s -%c 0",
				me.name, chptr->chname, cmode->flag);
			chptr->mode.extmode &= ~cmode->mode;
		}	

	cmode->flag = '\0';
}

void CmodeDel(Cmode *cmode)
{
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
		if (!inx)
			continue; /* not set */
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

int extcmode_default_requirechop(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	if (IsPerson(cptr) && is_chan_op(cptr, chptr))
		return EX_ALLOW;
	if (checkt == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
		sendto_one(cptr, err_str(ERR_NOTFORHALFOPS), me.name, cptr->name, mode);
	return EX_DENY;
}

int extcmode_default_requirehalfop(aClient *cptr, aChannel *chptr, char mode, char *para, int checkt, int what)
{
	if (IsPerson(cptr) &&
	    (is_chan_op(cptr, chptr) || is_half_op(cptr, chptr)))
		return EX_ALLOW;
	return EX_DENY;
}

