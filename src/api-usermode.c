/************************************************************************
 *   IRC - Internet Relay Chat, src/api-usermode.c
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

#include "unrealircd.h"

char umodestring[UMODETABLESZ+1];

Umode *Usermode_Table = NULL;
short	 Usermode_highest = 0;

Snomask *Snomask_Table = NULL;
short	 Snomask_highest = 0;

/* cptr->umodes (32 bits): 26 used, 6 free */
long UMODE_INVISIBLE = 0L;     /* makes user invisible */
long UMODE_OPER = 0L;          /* Operator */
long UMODE_WALLOP = 0L;        /* send wallops to them */
long UMODE_REGNICK = 0L;       /* Nick set by services as registered */
long UMODE_SERVNOTICE = 0L;    /* server notices such as kill */
long UMODE_HIDE = 0L;          /* Hide from Nukes */
long UMODE_SECURE = 0L;        /* User is a secure connect */
long UMODE_DEAF = 0L;          /* Deaf */
long UMODE_HIDEOPER = 0L;      /* Hide oper mode */
long UMODE_SETHOST = 0L;       /* Used sethost */
long UMODE_HIDLE = 0L;         /* Hides the idle time of opers */

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
	Usermode_Table = MyMallocEx(sizeof(Umode) * UMODETABLESZ);
	for (i = 0; i < UMODETABLESZ; i++)
	{
		Usermode_Table[i].mode = val;
		val *= 2;
	}
	Usermode_highest = 0;

	Snomask_Table = MyMallocEx(sizeof(Snomask) * UMODETABLESZ);
	val = 1;
	for (i = 0; i < UMODETABLESZ; i++)
	{
		Snomask_Table[i].mode = val;
		val *= 2;
	}
	Snomask_highest = 0;

	/* Set up modes */
	UmodeAdd(NULL, 'i', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_INVISIBLE);
	UmodeAdd(NULL, 'o', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_OPER);
	UmodeAdd(NULL, 'w', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_WALLOP);
	UmodeAdd(NULL, 'r', UMODE_GLOBAL, 0, umode_allow_none, &UMODE_REGNICK);
	UmodeAdd(NULL, 's', UMODE_LOCAL, 0, umode_allow_all, &UMODE_SERVNOTICE);
	UmodeAdd(NULL, 'x', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_HIDE);
	UmodeAdd(NULL, 'z', UMODE_GLOBAL, 0, umode_allow_none, &UMODE_SECURE);
	UmodeAdd(NULL, 'd', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_DEAF);
	UmodeAdd(NULL, 'H', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_HIDEOPER);
	UmodeAdd(NULL, 't', UMODE_GLOBAL, 0, umode_allow_unset, &UMODE_SETHOST);
	UmodeAdd(NULL, 'I', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_HIDLE);
	SnomaskAdd(NULL, 'k', umode_allow_opers, &SNO_KILLS);
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
	SnomaskAdd(NULL, 's', umode_allow_opers, &SNO_SNOTICE);
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

static char previous_umodestring[256];

void umodes_check_for_changes(void)
{
	make_umodestr();
	safestrdup(me.serv->features.usermodes, umodestring);

	if (!*previous_umodestring)
	{
		strlcpy(previous_umodestring, umodestring, sizeof(previous_umodestring));
		return; /* not booted yet. then we are done here. */
	}

	if (*previous_umodestring && strcmp(umodestring, previous_umodestring))
	{
		ircd_log(LOG_ERROR, "User modes changed at runtime: %s -> %s",
			previous_umodestring, umodestring);
		sendto_realops("User modes changed at runtime: %s -> %s",
			previous_umodestring, umodestring);
		/* Broadcast change to all (locally connected) servers */
		sendto_server(&me, 0, 0, NULL, "PROTOCTL USERMODES=%s", umodestring);
	}

	strlcpy(previous_umodestring, umodestring, sizeof(previous_umodestring));
}

/* UmodeAdd:
 * Add a usermode with character 'ch', if global is set to 1 the usermode is global
 * (sent to other servers) otherwise it's a local usermode
 */
Umode *UmodeAdd(Module *module, char ch, int global, int unset_on_deoper, int (*allowed)(Client *sptr, int what), long *mode)
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
		Usermode_Table[i].unset_on_deoper = unset_on_deoper;
		Debug((DEBUG_DEBUG, "UmodeAdd(%c) returning %04lx",
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
		Client *cptr;
		list_for_each_entry(cptr, &client_list, client_node)
		{
			long oldumode = 0;
			if (!IsUser(cptr))
				continue;
			oldumode = cptr->umodes;
			cptr->umodes &= ~umode->mode;
			if (MyUser(cptr))
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

Snomask *SnomaskAdd(Module *module, char ch, int (*allowed)(Client *sptr, int what), long *mode)
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
		Client *cptr;

		list_for_each_entry(cptr, &lclient_list, lclient_node)
		{
			long oldsno;
			if (!cptr || !IsUser(cptr))
				continue;
			oldsno = cptr->user->snomask;
			cptr->user->snomask &= ~sno->mode;
			if (oldsno != cptr->user->snomask)
				sendnumeric(cptr, RPL_SNOMASK, get_snostr(cptr->user->snomask));
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

int umode_allow_all(Client *sptr, int what)
{
	return 1;
}

int umode_allow_unset(Client *sptr, int what)
{
	if (!MyUser(sptr))
		return 1;
	if (what == MODE_DEL)
		return 1;
	return 0;
}

int umode_allow_none(Client *sptr, int what)
{
	if (MyUser(sptr))
		return 0;
	return 1;
}

int umode_allow_opers(Client *sptr, int what)
{
	if (MyUser(sptr))
		return IsOper(sptr) ? 1 : 0;
	else
		return 1;
}

void unload_all_unused_umodes(void)
{
	long removed_umode = 0;
	int i;
	Client *cptr;
	for (i = 0; i < UMODETABLESZ; i++)
	{
		if (Usermode_Table[i].unloaded)
			removed_umode |= Usermode_Table[i].mode;
	}
	if (!removed_umode) /* Nothing was unloaded */
		return;
	list_for_each_entry(cptr, &lclient_list, lclient_node)
	{
		long oldumode = 0;
		if (!IsUser(cptr))
			continue;
		oldumode = cptr->umodes;
		cptr->umodes &= ~(removed_umode);
		if (MyUser(cptr))
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

void unload_all_unused_snomasks(void)
{
	Client *cptr;
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

	list_for_each_entry(cptr, &lclient_list, lclient_node)
	{
		long oldsno;
		if (!cptr || !IsUser(cptr))
			continue;
		oldsno = cptr->user->snomask;
		cptr->user->snomask &= ~(removed_sno);
		if (oldsno != cptr->user->snomask)
			sendnumeric(cptr, RPL_SNOMASK, get_snostr(cptr->user->snomask));
	}
}

/**
 * This function removes any oper-only snomasks when the user is no
 * longer an IRC Operator.
 * This used to be a bit more complex but nowadays we just erase all
 * snomasks since all of them are IRCOp-only. Easy.
 */
void remove_oper_snomasks(Client *sptr)
{
	sptr->user->snomask = 0;
}

/*
 * This function removes any oper-only user modes from the user.
 * You may also want to call remove_oper_snomasks(), see above.
 */
void remove_oper_modes(Client *sptr)
{
int i;

	for (i = 0; i <= Usermode_highest; i++)
	{
		if (!Usermode_Table[i].flag)
			continue;
		if (Usermode_Table[i].unset_on_deoper)
			sptr->umodes &= ~Usermode_Table[i].mode;
	}
}

void remove_oper_privileges(Client *sptr, int broadcast_mode_change)
{
	long oldumodes = sptr->umodes;
	remove_oper_modes(sptr);
	remove_oper_snomasks(sptr);
	if (broadcast_mode_change && (sptr->umodes != oldumodes))
		send_umode_out(sptr, sptr, oldumodes);
	if (MyUser(sptr)) /* only do if it's our client, remote servers will send a SWHOIS cmd */
		swhois_delete(sptr, "oper", "*", &me, NULL);
}

/** Return long integer mode for a user mode character (eg: 'x' -> 0x10) */
long find_user_mode(char flag)
{
	int i;

	for (i = 0; i < UMODETABLESZ; i++)
	{
		if ((Usermode_Table[i].flag == flag) && !(Usermode_Table[i].unloaded))
			return Usermode_Table[i].mode;
	}
	return 0;
}

/** Returns 1 if user has this user mode set and 0 if not */
int has_user_mode(Client *acptr, char mode)
{
	long m = find_user_mode(mode);

	if (acptr->umodes & m)
		return 1; /* Yes, user has this mode */

	return 0; /* Mode does not exist or not set */
}
