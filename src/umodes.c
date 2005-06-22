/************************************************************************
 *   IRC - Internet Relay Chat, umodes.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

char umodestring[UMODETABLESZ+1];

Umode *Usermode_Table = NULL;
short	 Usermode_highest = 0;

Snomask *Snomask_Table = NULL;
short	 Snomask_highest = 0;

/* cptr->umodes (32 bits): 26 used, 6 free */
long UMODE_INVISIBLE = 0L;     /* makes user invisible */
long UMODE_OPER = 0L;          /* Operator */
long UMODE_WALLOP = 0L;        /* send wallops to them */
long UMODE_FAILOP = 0L;        /* Shows some global messages */
long UMODE_HELPOP = 0L;        /* Help system operator */
long UMODE_REGNICK = 0L;       /* Nick set by services as registered */
long UMODE_SADMIN = 0L;        /* Services Admin */
long UMODE_ADMIN = 0L;         /* Admin */
long UMODE_SERVNOTICE = 0L;    /* server notices such as kill */
long UMODE_LOCOP = 0L;         /* Local operator -- SRB */
long UMODE_RGSTRONLY = 0L;     /* Only reg nick message */
long UMODE_WEBTV = 0L;         /* WebTV Client */
long UMODE_SERVICES = 0L;      /* services */
long UMODE_HIDE = 0L;          /* Hide from Nukes */
long UMODE_NETADMIN = 0L;      /* Network Admin */
long UMODE_COADMIN = 0L;       /* Co Admin */
long UMODE_WHOIS = 0L;         /* gets notice on /whois */
long UMODE_KIX = 0L;           /* usermode +q */
long UMODE_BOT = 0L;           /* User is a bot */
long UMODE_SECURE = 0L;        /* User is a secure connect */
long UMODE_VICTIM = 0L;        /* Intentional Victim */
long UMODE_DEAF = 0L;          /* Deaf */
long UMODE_HIDEOPER = 0L;      /* Hide oper mode */
long UMODE_SETHOST = 0L;       /* Used sethost */
long UMODE_STRIPBADWORDS = 0L; /* Strip badwords */
long UMODE_HIDEWHOIS = 0L;     /* Hides channels in /whois */
long UMODE_NOCTCP = 0L;	       /* Blocks ctcp (except dcc and action) */

long SNO_KILLS = 0L;
long SNO_CLIENT = 0L;
long SNO_FLOOD = 0L;
long SNO_FCLIENT = 0L;
long SNO_JUNK = 0L;
long SNO_VHOST = 0L;
long SNO_EYES = 0L;
long SNO_TKL = 0L;
long SNO_NICKCHANGE = 0L;
long SNO_FNICKCHANGE = 0L;
long SNO_QLINE = 0L;
long SNO_SPAMF = 0L;
long SNO_SNOTICE = 0L;
long SNO_OPER = 0L;

long AllUmodes;		/* All umodes */
long SendUmodes;	/* All umodes which are sent to other servers (global umodes) */

void	umode_init(void)
{
	long val = 1;
	int	i;
	Usermode_Table = MyMalloc(sizeof(Umode) * UMODETABLESZ);
	bzero(Usermode_Table, sizeof(Umode) * UMODETABLESZ);
	for (i = 0; i < UMODETABLESZ; i++)
	{
		Usermode_Table[i].mode = val;
		val *= 2;
	}
	Usermode_highest = 0;

	Snomask_Table = MyMalloc(sizeof(Snomask) * UMODETABLESZ);
	bzero(Snomask_Table, sizeof(Snomask) * UMODETABLESZ);
	val = 1;
	for (i = 0; i < UMODETABLESZ; i++)
	{
		Snomask_Table[i].mode = val;
		val *= 2;
	}
	Snomask_highest = 0;

	/* Set up modes */
	/* 2004-02-11: note: TODO: 'umode_allow_opers' is in most cases
	 * not completely correct since even opers shouldn't be allowed
	 * to set most of these flags (eg locop trying to set +N),
	 * it is currently handled by the m_umode() routine however,
	 * but it would be better if we get rid of that and switch
	 * completely to this umode->allowed system :). -- Syzop.
	 */
	UmodeAdd(NULL, 'i', UMODE_GLOBAL, NULL, &UMODE_INVISIBLE);
	UmodeAdd(NULL, 'o', UMODE_GLOBAL, umode_allow_opers, &UMODE_OPER);
	UmodeAdd(NULL, 'w', UMODE_GLOBAL, NULL, &UMODE_WALLOP);
	UmodeAdd(NULL, 'g', UMODE_GLOBAL, umode_allow_opers, &UMODE_FAILOP);
	UmodeAdd(NULL, 'h', UMODE_GLOBAL, NULL, &UMODE_HELPOP);
	UmodeAdd(NULL, 'r', UMODE_GLOBAL, NULL, &UMODE_REGNICK);
	UmodeAdd(NULL, 'a', UMODE_GLOBAL, umode_allow_opers, &UMODE_SADMIN);
	UmodeAdd(NULL, 'A', UMODE_GLOBAL, umode_allow_opers, &UMODE_ADMIN);
	UmodeAdd(NULL, 's', UMODE_LOCAL, NULL, &UMODE_SERVNOTICE);
	UmodeAdd(NULL, 'O', UMODE_LOCAL, umode_allow_opers, &UMODE_LOCOP);
	UmodeAdd(NULL, 'R', UMODE_GLOBAL, NULL, &UMODE_RGSTRONLY);
	UmodeAdd(NULL, 'T', UMODE_GLOBAL, NULL, &UMODE_NOCTCP);
	UmodeAdd(NULL, 'V', UMODE_GLOBAL, NULL, &UMODE_WEBTV);
	UmodeAdd(NULL, 'S', UMODE_GLOBAL, umode_allow_opers, &UMODE_SERVICES);
	UmodeAdd(NULL, 'x', UMODE_GLOBAL, NULL, &UMODE_HIDE);
	UmodeAdd(NULL, 'N', UMODE_GLOBAL, umode_allow_opers, &UMODE_NETADMIN);
	UmodeAdd(NULL, 'C', UMODE_GLOBAL, umode_allow_opers, &UMODE_COADMIN);
	UmodeAdd(NULL, 'W', UMODE_GLOBAL, NULL, &UMODE_WHOIS);
	UmodeAdd(NULL, 'q', UMODE_GLOBAL, umode_allow_opers, &UMODE_KIX);
	UmodeAdd(NULL, 'B', UMODE_GLOBAL, NULL, &UMODE_BOT);
	UmodeAdd(NULL, 'z', UMODE_GLOBAL, NULL, &UMODE_SECURE);
	UmodeAdd(NULL, 'v', UMODE_GLOBAL, umode_allow_opers, &UMODE_VICTIM);
	UmodeAdd(NULL, 'd', UMODE_GLOBAL, NULL, &UMODE_DEAF);
	UmodeAdd(NULL, 'H', UMODE_GLOBAL, umode_allow_opers, &UMODE_HIDEOPER);
	UmodeAdd(NULL, 't', UMODE_GLOBAL, NULL, &UMODE_SETHOST);
	UmodeAdd(NULL, 'G', UMODE_GLOBAL, NULL, &UMODE_STRIPBADWORDS);
	UmodeAdd(NULL, 'p', UMODE_GLOBAL, NULL, &UMODE_HIDEWHOIS);
	SnomaskAdd(NULL, 'k', umode_allow_all, &SNO_KILLS);
	SnomaskAdd(NULL, 'c', umode_allow_opers, &SNO_CLIENT);
	SnomaskAdd(NULL, 'f', umode_allow_opers, &SNO_FLOOD);
	SnomaskAdd(NULL, 'F', umode_allow_opers, &SNO_FCLIENT);
	SnomaskAdd(NULL, 'j', umode_allow_opers, &SNO_JUNK);
	SnomaskAdd(NULL, 'v', umode_allow_opers, &SNO_VHOST);
	SnomaskAdd(NULL, 'e', umode_allow_opers, &SNO_EYES);
	SnomaskAdd(NULL, 'G', umode_allow_opers, &SNO_TKL);
	SnomaskAdd(NULL, 'n', umode_allow_opers, &SNO_NICKCHANGE);
	SnomaskAdd(NULL, 'N', umode_allow_opers, &SNO_FNICKCHANGE);
	SnomaskAdd(NULL, 'q', umode_allow_opers, &SNO_QLINE);
	SnomaskAdd(NULL, 'S', umode_allow_opers, &SNO_SPAMF);
	SnomaskAdd(NULL, 's', umode_allow_all, &SNO_SNOTICE);
	SnomaskAdd(NULL, 'o', umode_allow_opers, &SNO_OPER);
}

void make_umodestr(void)
{
	int i;
	char *m;

	m = umodestring;
	for (i = 0; i <= Usermode_highest; i++)
	{
		if (Usermode_Table[i].flag)
			*m++ = Usermode_Table[i].flag;
	}
	*m = '\0';
}

/* umode_get:
 * Add a usermode with character 'ch', if global is set to 1 the usermode is global
 * (sent to other servers) otherwise it's a local usermode
 */
Umode *UmodeAdd(Module *module, char ch, int global, int (*allowed)(aClient *sptr, int what), long *mode)
{
	short	 i = 0;
	short	 j = 0;
	short 	 save = -1;
	while (i < UMODETABLESZ)
	{
		if (!Usermode_Table[i].flag && save == -1)
			save = i;
		else if (Usermode_Table[i].flag == ch)
		{
			if (Usermode_Table[i].unloaded)
			{
				save = i;
				Usermode_Table[i].unloaded = 0;
				break;
			}
			else
			{
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
		i++;
	}
	i = save;
	if (i != UMODETABLESZ)
	{
		Usermode_Table[i].flag = ch;
		Usermode_Table[i].allowed = allowed;
		Debug((DEBUG_DEBUG, "umode_get(%c) returning %04x",
			ch, Usermode_Table[i].mode));
		/* Update usermode table highest */
		for (j = 0; j < UMODETABLESZ; j++)
			if (Usermode_Table[i].flag)
				if (i > Usermode_highest)
					Usermode_highest = i;
		make_umodestr();
		AllUmodes |= Usermode_Table[i].mode;
		if (global)
			SendUmodes |= Usermode_Table[i].mode;
		*mode = Usermode_Table[i].mode;
		Usermode_Table[i].owner = module;
		if (module)
		{
			ModuleObject *umodeobj = MyMallocEx(sizeof(ModuleObject));
			umodeobj->object.umode = &(Usermode_Table[i]);
			umodeobj->type = MOBJ_UMODE;
			AddListItem(umodeobj, module->objects);
			module->errorcode = MODERR_NOERROR;
		}
		return &(Usermode_Table[i]);
	}
	else
	{
		Debug((DEBUG_DEBUG, "UmodeAdd failed, no space"));
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}
}


void UmodeDel(Umode *umode)
{
	if (loop.ircd_rehashing)
		umode->unloaded = 1;
	else	
	{
		aClient *cptr;
		for (cptr = client; cptr; cptr = cptr->next)
		{
			long oldumode = 0;
			if (!IsPerson(cptr))
				continue;
			oldumode = cptr->umodes;
			cptr->umodes &= ~umode->mode;
			if (MyClient(cptr))
				send_umode_out(cptr, cptr, oldumode);
		}
		umode->flag = '\0';
		AllUmodes &= ~(umode->mode);
		SendUmodes &= ~(umode->mode);
		make_umodestr();
	}

	if (umode->owner) {
		ModuleObject *umodeobj;
		for (umodeobj = umode->owner->objects; umodeobj; umodeobj = umodeobj->next) {
			if (umodeobj->type == MOBJ_UMODE && umodeobj->object.umode == umode) {
				DelListItem(umodeobj, umode->owner->objects);
				MyFree(umodeobj);
				break;
			}
		}
		umode->owner = NULL;
	}
	return;
}

Snomask *SnomaskAdd(Module *module, char ch, int (*allowed)(aClient *sptr, int what), long *mode)
{
	short	 i = 0;
	short	 j = 0;
	short 	 save = -1;
	while (i < UMODETABLESZ)
	{
		if (!Snomask_Table[i].flag && save == -1)
			save = i;
		else if (Snomask_Table[i].flag == ch)
		{
			if (Snomask_Table[i].unloaded)
			{
				save = i;
				Snomask_Table[i].unloaded = 0;
				break;
			}
			else
			{
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
		i++;
	}
	i = save;
	if (i != UMODETABLESZ)
	{
		Snomask_Table[i].flag = ch;
		Snomask_Table[i].allowed = allowed;
		/* Update usermode table highest */
		for (j = 0; j < UMODETABLESZ; j++)
			if (Snomask_Table[i].flag)
				if (i > Snomask_highest)
					Snomask_highest = i;
		*mode = Snomask_Table[i].mode;
		Snomask_Table[i].owner = module;
		if (module)
		{
			ModuleObject *snoobj = MyMallocEx(sizeof(ModuleObject));
			snoobj->object.snomask = &(Snomask_Table[i]);
			snoobj->type = MOBJ_SNOMASK;
			AddListItem(snoobj, module->objects);
			module->errorcode = MODERR_NOERROR;
		}
		return &(Snomask_Table[i]);
	}
	else
	{
		Debug((DEBUG_DEBUG, "SnomaskAdd failed, no space"));
		*mode = 0;
		if (module)
			module->errorcode = MODERR_NOSPACE;
		return NULL;
	}
}

void SnomaskDel(Snomask *sno)
{
	if (loop.ircd_rehashing)
		sno->unloaded = 1;
	else	
	{
		int i;
		for (i = 0; i <= LastSlot; i++)
		{
			aClient *cptr = local[i];
			long oldsno;
			if (!cptr || !IsPerson(cptr))
				continue;
			oldsno = cptr->user->snomask;
			cptr->user->snomask &= ~sno->mode;
			if (oldsno != cptr->user->snomask)
				sendto_one(cptr, rpl_str(RPL_SNOMASK), me.name,
					cptr->name, get_snostr(cptr->user->snomask));
		}
		sno->flag = '\0';
	}
	if (sno->owner) {
		ModuleObject *snoobj;
		for (snoobj = sno->owner->objects; snoobj; snoobj = snoobj->next) {
			if (snoobj->type == MOBJ_SNOMASK && snoobj->object.snomask == sno) {
				DelListItem(snoobj, sno->owner->objects);
				MyFree(snoobj);
				break;
			}
		}
		sno->owner = NULL;
	}
	return;
}

int umode_allow_all(aClient *sptr, int what)
{
	return 1;
}

int umode_allow_opers(aClient *sptr, int what)
{
	if (MyClient(sptr))
		return IsAnOper(sptr) ? 1 : 0;
	else
		return 1;
}

void unload_all_unused_umodes()
{
	long removed_umode = 0;
	int i;
	aClient *cptr;
	for (i = 0; i < UMODETABLESZ; i++)
	{
		if (Usermode_Table[i].unloaded)
			removed_umode |= Usermode_Table[i].mode;
	}
	if (!removed_umode) /* Nothing was unloaded */
		return;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		long oldumode = 0;
		if (!IsPerson(cptr))
			continue;
		oldumode = cptr->umodes;
		cptr->umodes &= ~(removed_umode);
		if (MyClient(cptr))
			send_umode_out(cptr, cptr, oldumode);
	}
	for (i = 0; i < UMODETABLESZ; i++)
	{
		if (Usermode_Table[i].unloaded)
		{
			AllUmodes &= ~(Usermode_Table[i].mode);
			SendUmodes &= ~(Usermode_Table[i].mode);
			Usermode_Table[i].flag = '\0';
			Usermode_Table[i].unloaded = 0;
		}
	}
	make_umodestr();
}

void unload_all_unused_snomasks()
{
	long removed_sno = 0;
	int i;

	for (i = 0; i < UMODETABLESZ; i++)
	{
		if (Snomask_Table[i].unloaded)
		{
			removed_sno |= Snomask_Table[i].mode;
			Snomask_Table[i].flag = '\0';
			Snomask_Table[i].unloaded = 0;
		}
	}
	if (!removed_sno) /* Nothing was unloaded */
		return;
	for (i = 0; i <= LastSlot; i++)
	{
		aClient *cptr = local[i];
		long oldsno;
		if (!cptr || !IsPerson(cptr))
			continue;
		oldsno = cptr->user->snomask;
		cptr->user->snomask &= ~(removed_sno);
		if (oldsno != cptr->user->snomask)
			sendto_one(cptr, rpl_str(RPL_SNOMASK), me.name,
				cptr->name, get_snostr(cptr->user->snomask));
		
	}
}

long umode_get(char ch, int options, int (*allowed)(aClient *sptr, int what))
{
	long flag;
	if (UmodeAdd(NULL, ch, options, allowed, &flag))
		return flag;
	return 0;
}

int umode_delete(char ch, long val)
{
	int i;
	for (i = 0; i < UMODETABLESZ; i++)
	{
		if (Usermode_Table[i].flag == ch && Usermode_Table[i].mode == val)
		{
			UmodeDel(&Usermode_Table[i]);
			return 1;
		}
	}
	return -1;
}

/* Simply non-perfect function to remove all oper-snomasks, 
 * it's at least better than manually doing a .. &= ~SNO_BLAH everywhere.
 */
void remove_oper_snomasks(aClient *sptr)
{
int i;
	for (i = 0; i <= Snomask_highest; i++)
	{
		if (!Snomask_Table[i].flag)
			continue;
		if (Snomask_Table[i].allowed == umode_allow_opers)
			sptr->user->snomask &= ~Snomask_Table[i].mode;
	}
}
