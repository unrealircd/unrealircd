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

/** User modes and their handlers */
Umode *usermodes = NULL;

long UMODE_INVISIBLE = 0L;     /* makes user invisible */
long UMODE_OPER = 0L;          /* Operator */
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

/* Forward declarations */
int umode_hidle_allow(Client *client, int what);
static void unload_usermode_commit(Umode *m);

void umode_init(void)
{
	/* Some built-in modes */
	UmodeAdd(NULL, 'i', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_INVISIBLE);
	UmodeAdd(NULL, 'o', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_OPER);
	UmodeAdd(NULL, 'r', UMODE_GLOBAL, 0, umode_allow_none, &UMODE_REGNICK);
	UmodeAdd(NULL, 's', UMODE_LOCAL, 0, umode_allow_all, &UMODE_SERVNOTICE);
	UmodeAdd(NULL, 'x', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_HIDE);
	UmodeAdd(NULL, 'z', UMODE_GLOBAL, 0, umode_allow_none, &UMODE_SECURE);
	UmodeAdd(NULL, 'd', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_DEAF);
	UmodeAdd(NULL, 'H', UMODE_GLOBAL, 1, umode_allow_opers, &UMODE_HIDEOPER);
	UmodeAdd(NULL, 't', UMODE_GLOBAL, 0, umode_allow_unset, &UMODE_SETHOST);
	UmodeAdd(NULL, 'I', UMODE_GLOBAL, 0, umode_hidle_allow, &UMODE_HIDLE);
}

void make_umodestr(void)
{
	Umode *um;
	char *p = umodestring;

	for (um=usermodes; um; um = um->next)
		if (um->letter)
			*p++ = um->letter;
	*p = '\0';
}

static char previous_umodestring[256];

void umodes_check_for_changes(void)
{
	make_umodestr();
	safe_strdup(me.server->features.usermodes, umodestring);

	if (!*previous_umodestring)
	{
		strlcpy(previous_umodestring, umodestring, sizeof(previous_umodestring));
		return; /* not booted yet. then we are done here. */
	}

	if (*previous_umodestring && strcmp(umodestring, previous_umodestring))
	{
		unreal_log(ULOG_INFO, "mode", "USER_MODES_CHANGED", NULL,
		           "User modes changed at runtime: $old_user_modes -> $new_user_modes",
		           log_data_string("old_user_modes", previous_umodestring),
		           log_data_string("new_user_modes", umodestring));
		/* Broadcast change to all (locally connected) servers */
		sendto_server(NULL, 0, 0, NULL, "PROTOCTL USERMODES=%s", umodestring);
	}

	strlcpy(previous_umodestring, umodestring, sizeof(previous_umodestring));
}

void usermode_add_sorted(Umode *n)
{
	Umode *m;

	if (usermodes == NULL)
	{
		usermodes = n;
		return;
	}

	for (m = usermodes; m; m = m->next)
	{
		if (m->letter == '\0')
			abort();
		if (sort_character_lowercase_before_uppercase(n->letter, m->letter))
		{
			/* Insert us before */
			if (m->prev)
				m->prev->next = n;
			else
				usermodes = n; /* new head */
			n->prev = m->prev;

			n->next = m;
			m->prev = n;
			return;
		}
		if (!m->next)
		{
			/* Append us at end */
			m->next = n;
			n->prev = m;
			return;
		}
	}
}


/* UmodeAdd:
 * Add a usermode with character 'ch', if global is set to 1 the usermode is global
 * (sent to other servers) otherwise it's a local usermode
 */
Umode *UmodeAdd(Module *module, char ch, int global, int unset_on_deoper, int (*allowed)(Client *client, int what), long *mode)
{
	Umode *um;
	int existing = 0;

	for (um=usermodes; um; um = um->next)
	{
		if (um->letter == ch)
		{
			if (um->unloaded)
			{
				um->unloaded = 0;
				existing = 1;
				break;
			} else {
				if (module)
					module->errorcode = MODERR_EXISTS;
				return NULL;
			}
		}
	}

	if (!um)
	{
		/* Not found, create */
		long l, found = 0;
		for (l = 1; l < LONG_MAX/2; l *= 2)
		{
			found = 0;
			for (um=usermodes; um; um = um->next)
			{
				if (um->mode == l)
				{
					found = 1;
					break;
				}
			}
			if (!found)
				break;
		}
		/* If 'found' is still true, then we are out of space */
		if (found)
		{
			unreal_log(ULOG_ERROR, "module", "USER_MODE_OUT_OF_SPACE", NULL,
				   "UmodeAdd: out of space!!!");
			if (module)
				module->errorcode = MODERR_NOSPACE;
			return NULL;
		}
		um = safe_alloc(sizeof(Umode));
		um->letter = ch;
		um->mode = l;
		usermode_add_sorted(um);
	}

	um->letter = ch;
	um->allowed = allowed;
	um->unset_on_deoper = unset_on_deoper;
	make_umodestr();
	AllUmodes |= um->mode;
	if (global)
		SendUmodes |= um->mode;
	*mode = um->mode;
	um->owner = module;
	if (module)
	{
		ModuleObject *umodeobj = safe_alloc(sizeof(ModuleObject));
		umodeobj->object.umode = um;
		umodeobj->type = MOBJ_UMODE;
		AddListItem(umodeobj, module->objects);
		module->errorcode = MODERR_NOERROR;
	}
	return um;
}


void UmodeDel(Umode *umode)
{
	/* Always free the module object */
	if (umode->owner)
	{
		ModuleObject *umodeobj;
		for (umodeobj = umode->owner->objects; umodeobj; umodeobj = umodeobj->next)
		{
			if (umodeobj->type == MOBJ_UMODE && umodeobj->object.umode == umode)
			{
				DelListItem(umodeobj, umode->owner->objects);
				safe_free(umodeobj);
				break;
			}
		}
		umode->owner = NULL;
	}

	/* Whether we can actually (already) free the Umode depends... */

	if (loop.rehashing)
		umode->unloaded = 1;
	else
		unload_usermode_commit(umode);
}

int umode_allow_all(Client *client, int what)
{
	return 1;
}

int umode_allow_unset(Client *client, int what)
{
	if (!MyUser(client))
		return 1;
	if (what == MODE_DEL)
		return 1;
	return 0;
}

int umode_allow_none(Client *client, int what)
{
	if (MyUser(client))
		return 0;
	return 1;
}

int umode_allow_opers(Client *client, int what)
{
	if (MyUser(client))
		return IsOper(client) ? 1 : 0;
	else
		return 1;
}

int umode_hidle_allow(Client *client, int what)
{
	if (!MyUser(client))
		return 1;
	if (iConf.hide_idle_time == HIDE_IDLE_TIME_OPER_USERMODE)
		return IsOper(client) ? 1 : 0;
	if (iConf.hide_idle_time == HIDE_IDLE_TIME_USERMODE)
		return 1;
	return 0; /* if set::hide-idle-time is 'never' or 'always' then +I makes no sense */
}

static void unload_usermode_commit(Umode *um)
{
	Client *client;
	long removed_umode;

	if (!um)
		return;

	removed_umode = um->mode;

	/* First send the -mode regarding all users */
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (MyUser(client) && (client->umodes & removed_umode))
		{
			long oldumode = client->umodes;
			client->umodes &= ~(removed_umode);
			send_umode_out(client, 1, oldumode);
		}
	}

	/* Then unload the mode */
	DelListItem(um, usermodes);
	safe_free(um);
	make_umodestr();
}

void unload_all_unused_umodes(void)
{
	Umode *um, *um_next;

	for (um=usermodes; um; um = um_next)
	{
		um_next = um->next;
		if (um->letter && um->unloaded)
			unload_usermode_commit(um);
	}
}

/**
 * This function removes any oper-only snomasks when the user is no
 * longer an IRC Operator.
 * This used to be a bit more complex but nowadays we just erase all
 * snomasks since all of them are IRCOp-only. Easy.
 */
void remove_all_snomasks(Client *client)
{
	safe_free(client->user->snomask);
	client->umodes &= ~UMODE_SERVNOTICE;
}

/*
 * This function removes any oper-only user modes from the user.
 * You may also want to call remove_all_snomasks(), see above.
 */
void remove_oper_modes(Client *client)
{
	Umode *um;

	for (um = usermodes; um; um = um->next)
	{
		if (um->unset_on_deoper)
			client->umodes &= ~um->mode;
	}

	/* Bit of a hack, since this is a dynamic permission umode */
	if (iConf.hide_idle_time == HIDE_IDLE_TIME_OPER_USERMODE)
		client->umodes &= ~UMODE_HIDLE;
}

void remove_oper_privileges(Client *client, int broadcast_mode_change)
{
	long oldumodes = client->umodes;
	remove_oper_modes(client);
	remove_all_snomasks(client);
	if (broadcast_mode_change && (client->umodes != oldumodes))
		send_umode_out(client, 1, oldumodes);
	if (MyUser(client)) /* only do if it's our client, remote servers will send a SWHOIS cmd */
		swhois_delete(client, "oper", "*", &me, NULL);
}

/** Return long integer mode for a user mode character (eg: 'x' -> 0x10) */
long find_user_mode(char letter)
{
	Umode *um;

	for (um = usermodes; um; um = um->next)
		if ((um->letter == letter) && !um->unloaded)
			return um->mode;

	return 0;
}

/** Returns 1 if user has this user mode set and 0 if not */
int has_user_mode(Client *client, char mode)
{
	long m = find_user_mode(mode);

	if (client->umodes & m)
		return 1; /* Yes, user has this mode */

	return 0; /* Mode does not exist or not set */
}
