/************************************************************************
 *   Unreal Internet Relay Chat, src/list.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Finland
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

/* -- Jto -- 20 Jun 1990
 * extern void free() fixed as suggested by
 * gruner@informatik.tu-muenchen.de
 */

/* -- Jto -- 03 Jun 1990
 * Added chname initialization...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() to channel.c
 */

/* -- Jto -- 10 May 1990
 * Added #include <sys.h>
 * Changed memset(xx,0,yy) into bzero(xx,yy)
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "proto.h"
#include "numeric.h"
#ifdef	DBMALLOC
#include "malloc.h"
#endif
#include <string.h>
void free_link(Link *);
Link *make_link();
extern ircstats IRCstats;

ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.24 4/20/94");

#ifdef	DEBUGMODE
static struct liststats {
	int  inuse;
} cloc, crem, users, servs, links, classs, aconfs;

#endif

void outofmemory();

MODVAR int  flinks = 0;
MODVAR int  freelinks = 0;
MODVAR Link *freelink = NULL;
MODVAR Member *freemember = NULL;
MODVAR Membership *freemembership = NULL;
MODVAR MembershipL *freemembershipL = NULL;
MODVAR int  numclients = 0;

void initlists(void)
{
#ifdef	DEBUGMODE
	bzero((char *)&cloc, sizeof(cloc));
	bzero((char *)&crem, sizeof(crem));
	bzero((char *)&users, sizeof(users));
	bzero((char *)&servs, sizeof(servs));
	bzero((char *)&links, sizeof(links));
	bzero((char *)&classs, sizeof(classs));
#endif
}

void outofmemory(void)
{
	Debug((DEBUG_FATAL, "Out of memory: restarting server..."));
	restart("Out of Memory");
}


/*
** Create a new aClient structure and set it to initial state.
**
**	from == NULL,	create local client (a client connected
**			to a socket).
**
**	from,	create remote client (behind a socket
**			associated with the client defined by
**			'from'). ('from' is a local client!!).
*/
aClient *make_client(aClient *from, aClient *servr)
{
	aClient *cptr = NULL;
	unsigned size = CLIENT_REMOTE_SIZE;

	/*
	 * Check freelists first to see if we can grab a client without
	 * having to call malloc.
	 */
	if (!from)
		size = CLIENT_LOCAL_SIZE;

	if (!(cptr = (aClient *)MyMalloc(size)))
		outofmemory();
	bzero((char *)cptr, (int)size);

#ifdef	DEBUGMODE
	if (size == CLIENT_LOCAL_SIZE)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr;	/* 'from' of local client is self! */
	cptr->next = NULL;	/* For machines with NON-ZERO NULL pointers >;) */
	cptr->prev = NULL;
	cptr->hnext = NULL;
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->srvptr = servr;
	cptr->status = STAT_UNKNOWN;
	
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	{
		cptr->since = cptr->lasttime =
		    cptr->lastnick = cptr->firsttime = TStime();
		cptr->class = NULL;
		cptr->passwd = NULL;
		cptr->sockhost[0] = '\0';
		cptr->buffer[0] = '\0';
		cptr->authfd = -1;
		cptr->fd = -1;
	} else {
		cptr->fd = -256;
	}
	return (cptr);
}

void free_client(aClient *cptr)
{
	if (MyConnect(cptr))
	{
		if (cptr->passwd)
			MyFree((char *)cptr->passwd);
		if (cptr->error_str)
			MyFree(cptr->error_str);
#ifdef ZIP_LINKS
		if (cptr->zip)
			zip_free(cptr);
#endif
		if (cptr->hostp)
			unreal_free_hostent(cptr->hostp);
	}
	MyFree((char *)cptr);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
anUser *make_user(aClient *cptr)
{
	anUser *user;

	user = cptr->user;
	if (!user)
	{
		user = (anUser *)MyMallocEx(sizeof(anUser));
#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->swhois = NULL;
		user->away = NULL;
#ifdef NO_FLOOD_AWAY
		user->flood.away_t = 0;
		user->flood.away_c = 0;
#endif
		user->refcnt = 1;
		user->joined = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->silence = NULL;
		user->server = NULL;
		user->servicestamp = 0;
		user->lopt = NULL;
		user->whowas = NULL;
		user->snomask = 0;
		*user->realhost = '\0';
		user->virthost = NULL;
		user->ip_str = NULL;
		cptr->user = user;		
	}
	return user;
}

aServer *make_server(aClient *cptr)
{

	aServer *serv = cptr->serv;

	if (!serv)
	{
		serv = (aServer *)MyMallocEx(sizeof(aServer));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		serv->user = NULL;
		*serv->by = '\0';
		serv->numeric = 0;
		serv->users = 0;
		serv->up = NULL;
		cptr->serv = serv;
	}
	return cptr->serv;
}

/*
** free_user
**	Decrease user reference count by one and realease block,
**	if count reaches 0
*/
void free_user(anUser *user, aClient *cptr)
{
	if (--user->refcnt <= 0)
	{
		if (user->away)
			MyFree(user->away);
		if (user->swhois)
			MyFree(user->swhois);
		if (user->virthost)
			MyFree(user->virthost);
		if (user->ip_str)
			MyFree(user->ip_str);
		if (user->operlogin)
			MyFree(user->operlogin);
		/*
		 * sanity check
		 */
		if (user->joined || user->refcnt < 0 ||
		    user->invited || user->channel)
			sendto_realops("* %p user (%s!%s@%s) %p %p %p %d %d *",
			    cptr, cptr ? cptr->name : "<noname>",
			    user->username, user->realhost, user,
			    user->invited, user->channel, user->joined,
			    user->refcnt);
		MyFree(user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	}
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(aClient *cptr)
{
	if (IsServer(cptr))
	{
		remove_server_from_table(cptr);
		IRCstats.servers--;
	}
	if (IsClient(cptr))
	{
		if (IsInvisible(cptr))
		{
			IRCstats.invisible--;
		}
		if (IsOper(cptr) && !IsHideOper(cptr))
		{
			IRCstats.operators--;
			VERIFY_OPERCOUNT(cptr, "rmvlist");
		}
		IRCstats.clients--;
		if (cptr->srvptr && cptr->srvptr->serv)
			cptr->srvptr->serv->users--;
	}
	if (IsUnknown(cptr) || IsConnecting(cptr) || IsHandshake(cptr)
#ifdef USE_SSL
		|| IsSSLHandshake(cptr)
#endif
	)
		IRCstats.unknown--;
	checklist();
	if (cptr->prev)
		cptr->prev->next = cptr->next;
	else
	{
		client = cptr->next;
		if (client)
			client->prev = NULL;
	}
	if (cptr->next)
		cptr->next->prev = cptr->prev;
	if (IsPerson(cptr))	/* Only persons can have been added before */
	{
		add_history(cptr, 0);
		off_history(cptr);	/* Remove all pointers to cptr */
	}
	
	if (cptr->user)
		(void)free_user(cptr->user, cptr);
	if (cptr->serv)
	{
		if (cptr->serv->user)
			free_user(cptr->serv->user, cptr);
		MyFree((char *)cptr->serv);
#ifdef	DEBUGMODE
		servs.inuse--;
#endif
	}
#ifdef	DEBUGMODE
	if (cptr->fd == -2)
		cloc.inuse--;
	else
		crem.inuse--;
#endif
	(void)free_client(cptr);
	numclients--;
	return;
}

/*
 * although only a small routine, it appears in a number of places
 * as a collection of a few lines...functions like this *should* be
 * in this file, shouldnt they ?  after all, this is list.c, isnt it ?
 * -avalon
 */
void add_client_to_list(aClient *cptr)
{
	/*
	 * since we always insert new clients to the top of the list,
	 * this should mean the "me" is the bottom most item in the list.
	 */
	cptr->next = client;
	client = cptr;
	if (cptr->next)
		cptr->next->prev = cptr;
	return;
}

/*
 * Look for ptr in the linked listed pointed to by link.
 */
Link *find_user_link(Link *lp, aClient *ptr)
{
	if (ptr)
		while (lp)
		{
			if (lp->value.cptr == ptr)
				return (lp);
			lp = lp->next;
		}
	return NULL;
}

/* Based on find_str_link() from bahamut -- codemastr */
int find_str_match_link(Link *lp, char *charptr)
{
	if (!charptr)
		return 0;
	for (; lp; lp = lp->next) {
		if(!match(lp->value.cp, charptr))
			return 1;
	}
	return 0;
}

void free_str_list(Link *lp)
{
	Link *next;


	while (lp)
	{
		next = lp->next;
		MyFree((char *)lp->value.cp);
		free_link(lp);
		lp = next;
	}

	return;
}


#define	LINKSIZE	(4072/sizeof(Link))

Link *make_link(void)
{
	Link *lp;
	int  i;

	/* "caching" slab-allocator... ie. we're allocating one pages
	   (hopefully - upped to the Linux default, not dbuf.c) worth of 
	   link-structures at time to avoid all the malloc overhead.
	   All links left free from this process or separately freed 
	   by a call to free_link() are moved over to freelink-list.
	   Impact? Let's see... -Donwulff */
	/* Impact is a huge memory leak -Stskeeps
	   hope this implementation works a little bit better */
	if (freelink == NULL)
	{
		for (i = 1; i <= LINKSIZE; i++)
		{
			lp = (Link *)MyMalloc(sizeof(Link));
			lp->next = freelink;
			freelink = lp;
		}
		freelinks = freelinks + LINKSIZE;
		lp = freelink;
		freelink = lp->next;
		freelinks--;
	}
	else
	{
		lp = freelink;
		freelink = freelink->next;
		freelinks--;
	}
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void free_link(Link *lp)
{
	lp->next = freelink;
	freelink = lp;
	freelinks++;

#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

Ban *make_ban(void)
{
	Ban *lp;

	lp = (Ban *) MyMalloc(sizeof(Ban));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void free_ban(Ban *lp)
{
	MyFree((char *)lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

aClass *make_class(void)
{
	aClass *tmp;

	tmp = (aClass *)MyMalloc(sizeof(aClass));
#ifdef	DEBUGMODE
	classs.inuse++;
#endif
	return tmp;
}

void free_class(aClass *tmp)
{
	MyFree((char *)tmp);
#ifdef	DEBUGMODE
	classs.inuse--;
#endif
}

#ifdef	DEBUGMODE
void send_listinfo(aClient *cptr, char *name)
{
	int  inuse = 0, mem = 0, tmp = 0;

	sendto_one(cptr, ":%s %d %s :Local: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, inuse += cloc.inuse,
	    tmp = cloc.inuse * CLIENT_LOCAL_SIZE);
	mem += tmp;
	sendto_one(cptr, ":%s %d %s :Remote: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name,
	    crem.inuse, tmp = crem.inuse * CLIENT_REMOTE_SIZE);
	mem += tmp;
	inuse += crem.inuse;
	sendto_one(cptr, ":%s %d %s :Users: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, users.inuse,
	    tmp = users.inuse * sizeof(anUser));
	mem += tmp;
	inuse += users.inuse,
	    sendto_one(cptr, ":%s %d %s :Servs: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, servs.inuse,
	    tmp = servs.inuse * sizeof(aServer));
	mem += tmp;
	inuse += servs.inuse,
	    sendto_one(cptr, ":%s %d %s :Links: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, links.inuse,
	    tmp = links.inuse * sizeof(Link));
	mem += tmp;
	inuse += links.inuse,
	    sendto_one(cptr, ":%s %d %s :Classes: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, classs.inuse,
	    tmp = classs.inuse * sizeof(aClass));
	mem += tmp;
	inuse += aconfs.inuse,
	    sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
	    me.name, RPL_STATSDEBUG, name, inuse, mem);
}

#endif

void add_ListItem(ListStruct *item, ListStruct **list) {
	item->next = *list;
	item->prev = NULL;
	if (*list)
		(*list)->prev = item;
	*list = item;
}

ListStruct *del_ListItem(ListStruct *item, ListStruct **list) {
	ListStruct *l, *ret;

	for (l = *list; l; l = l->next) {
		if (l == item) {
			ret = item->next;
			if (l->prev)
				l->prev->next = l->next;
			else
				*list = l->next;
			if (l->next)
				l->next->prev = l->prev;
			return ret;
		}
	}
	return NULL;
}

#ifdef JOINTHROTTLE
/** Adds a aJFlood entry to user & channel and returns entry.
 * NOTE: Does not check for already-existing-entry
 */
aJFlood *cmodej_addentry(aClient *cptr, aChannel *chptr)
{
aJFlood *e;

#ifdef DEBUGMODE
	if (!IsPerson(cptr))
		abort();

	for (e=cptr->user->jflood; e; e=e->next_u)
		if (e->chptr == chptr)
			abort();

	for (e=chptr->jflood; e; e=e->next_c)
		if (e->cptr == cptr)
			abort();
#endif

	e = MyMallocEx(sizeof(aJFlood));
	e->cptr = cptr;
	e->chptr = chptr;
	e->prev_u = e->prev_c = NULL;
	e->next_u = cptr->user->jflood;
	e->next_c = chptr->jflood;
	if (cptr->user->jflood)
		cptr->user->jflood->prev_u = e;
	if (chptr->jflood)
		chptr->jflood->prev_c = e;
	cptr->user->jflood = chptr->jflood = e;

	return e;
}

/** Removes an individual entry from list and frees it.
 */
void cmodej_delentry(aJFlood *e)
{
	/* remove from user.. */
	if (e->prev_u)
		e->prev_u->next_u = e->next_u;
	else
		e->cptr->user->jflood = e->next_u; /* new head */
	if (e->next_u)
		e->next_u->prev_u = e->prev_u;

	/* remove from channel.. */
	if (e->prev_c)
		e->prev_c->next_c = e->next_c;
	else
		e->chptr->jflood = e->next_c; /* new head */
	if (e->next_c)
		e->next_c->prev_c = e->prev_c;

	/* actually free it */
	MyFree(e);
}

/** Removes all entries belonging to user from all lists and free them. */
void cmodej_deluserentries(aClient *cptr)
{
aJFlood *e, *e_next;

	for (e=cptr->user->jflood; e; e=e_next)
	{
		e_next = e->next_u;

		/* remove from channel.. */
		if (e->prev_c)
			e->prev_c->next_c = e->next_c;
		else
			e->chptr->jflood = e->next_c; /* new head */
		if (e->next_c)
			e->next_c->prev_c = e->prev_c;

		/* actually free it */
		MyFree(e);
	}
	cptr->user->jflood = NULL;
}

/** Removes all entries belonging to channel from all lists and free them. */
void cmodej_delchannelentries(aChannel *chptr)
{
aJFlood *e, *e_next;

	for (e=chptr->jflood; e; e=e_next)
	{
		e_next = e->next_c;
		
		/* remove from user.. */
		if (e->prev_u)
			e->prev_u->next_u = e->next_u;
		else
			e->cptr->user->jflood = e->next_u; /* new head */
		if (e->next_u)
			e->next_u->prev_u = e->prev_u;

		/* actually free it */
		MyFree(e);
	}
	chptr->jflood = NULL;
}

/** Regulary cleans up cmode-j user/chan structs */
EVENT(cmodej_cleanup_structs)
{
aJFlood *e, *e_next;
int i;
aClient *cptr;
aChannel *chptr;
int t;
CmodeParam *cmp;
#ifdef DEBUGMODE
int freed=0;
#endif

	for (chptr = channel; chptr; chptr=chptr->nextch)
	{
		if (!chptr->jflood)
			continue;
		t=0;
		/* t will be kept at 0 if not found or if mode not set,
		 * but DO still check since there are entries left as indicated by ->jflood!
		 */
		if (chptr->mode.extmode & EXTMODE_JOINTHROTTLE)
		{
			for (cmp = chptr->mode.extmodeparam; cmp; cmp=cmp->next)
				if (cmp->flag == 'j')
					t = ((aModejEntry *)cmp)->t;
		}

		for (e = chptr->jflood; e; e = e_next)
		{
			e_next = e->next_c;
			
			if (e->firstjoin + t < TStime())
			{
				cmodej_delentry(e);
#ifdef DEBUGMODE
				freed++;
#endif
			}
		}
	}

#ifdef DEBUGMODE
	if (freed)
		ircd_log(LOG_ERROR, "cmodej_cleanup_structs: %d entries freed [%d bytes]", freed, freed * sizeof(aJFlood));
#endif
}

#endif		
	
