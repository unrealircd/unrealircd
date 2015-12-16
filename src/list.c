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
#include "mempool.h"
#include <assert.h>
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

/* unless documented otherwise, these are all local-only, except client_list. */
MODVAR struct list_head client_list, lclient_list, server_list, oper_list, unknown_list, global_server_list;

static mp_pool_t *user_pool = NULL;

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

	INIT_LIST_HEAD(&client_list);
	INIT_LIST_HEAD(&lclient_list);
	INIT_LIST_HEAD(&server_list);
	INIT_LIST_HEAD(&oper_list);
	INIT_LIST_HEAD(&unknown_list);
	INIT_LIST_HEAD(&global_server_list);

	user_pool = mp_pool_new(sizeof(anUser), 512 * 1024);
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

	cptr = MyMallocEx(sizeof(aClient));

#ifdef	DEBUGMODE
	if (!from)
		cloc.inuse++;
	else
		crem.inuse++;
#endif

	/* Note:  structure is zero (calloc) */
	cptr->from = from ? from : cptr;	/* 'from' of local client is self! */
	cptr->user = NULL;
	cptr->serv = NULL;
	cptr->srvptr = servr;
	cptr->status = STAT_UNKNOWN;

	INIT_LIST_HEAD(&cptr->client_node);
	INIT_LIST_HEAD(&cptr->client_hash);
	INIT_LIST_HEAD(&cptr->id_hash);

	(void)strcpy(cptr->username, "unknown");
	if (!from)
	{
		/* Local client */
		
		cptr->local = MyMallocEx(sizeof(aLocalClient));
		
		INIT_LIST_HEAD(&cptr->lclient_node);
		INIT_LIST_HEAD(&cptr->special_node);

		cptr->local->since = cptr->local->lasttime =
		cptr->lastnick = cptr->local->firsttime = TStime();
		cptr->local->class = NULL;
		cptr->local->passwd = NULL;
		cptr->local->sockhost[0] = '\0';
		cptr->local->buffer[0] = '\0';
		cptr->local->authfd = -1;
		cptr->fd = -1;

		dbuf_queue_init(&cptr->local->recvQ);
		dbuf_queue_init(&cptr->local->sendQ);
	} else {
		cptr->fd = -256;
	}
	return (cptr);
}

void free_client(aClient *cptr)
{
	if (!list_empty(&cptr->client_node))
		list_del(&cptr->client_node);

	if (MyConnect(cptr))
	{
		if (!list_empty(&cptr->lclient_node))
			list_del(&cptr->lclient_node);
		if (!list_empty(&cptr->special_node))
			list_del(&cptr->special_node);

		RunHook(HOOKTYPE_FREE_CLIENT, cptr);
		if (cptr->local)
		{
			safefree(cptr->local->passwd);
			safefree(cptr->local->error_str);
			if (cptr->local->hostp)
				unreal_free_hostent(cptr->local->hostp);
			
			MyFree(cptr->local);
		}
	}
	
	safefree(cptr->ip);

	MyFree(cptr);
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
		user = mp_pool_get(user_pool);
		memset(user, 0, sizeof(anUser));

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
		strlcpy(user->svid, "0", sizeof(user->svid));
		user->lopt = NULL;
		user->whowas = NULL;
		user->snomask = 0;
		if (cptr->ip)
		{
			/* initially set cptr->user->realhost to IP */
			strlcpy(user->realhost, cptr->ip, sizeof(user->realhost));
		} else {
			*user->realhost = '\0';
		}
		user->virthost = NULL;
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
	if (user->refcnt == 0)
		sendto_realops("[BUG] free_user: ref count for '%s' was already 0!?", user->username);
	else
		--user->refcnt;
	if (user->refcnt == 0)
	{
		RunHook2(HOOKTYPE_FREE_USER, user, cptr);
		if (user->away)
			MyFree(user->away);
		if (user->swhois)
		{
			SWhois *s, *s_next;
			for (s = user->swhois; s; s = s_next)
			{
				s_next = s->next;
				safefree(s->line);
				safefree(s->setby);
				MyFree(s);
			}
		}
		if (user->virthost)
			MyFree(user->virthost);
		if (user->operlogin)
			MyFree(user->operlogin);
		mp_pool_release(user);
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
	list_del(&cptr->client_node);
	if (IsServer(cptr))
	{
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
		|| IsSSLHandshake(cptr)
	)
		IRCstats.unknown--;

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
		safefree(cptr->serv->features.chanmodes[0]);
		safefree(cptr->serv->features.chanmodes[1]);
		safefree(cptr->serv->features.chanmodes[2]);
		safefree(cptr->serv->features.chanmodes[3]);
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
	assert(list_empty(&cptr->client_node));
	assert(list_empty(&cptr->client_hash));
	assert(list_empty(&cptr->id_hash));
	(void)free_client(cptr);
	checklist();
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
	list_add(&cptr->client_node, &client_list);
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

/** Add item to list with a 'priority'.
 * If there are multiple items with the same priority then it will be
 * added as the last item within.
 */
void add_ListItemPrio(ListStructPrio *new, ListStructPrio **list, int priority)
{
	ListStructPrio *x, *last = NULL;
	
	if (!*list)
	{
		/* We are the only item. Easy. */
		*list = new;
		return;
	}
	
	for (x = *list; x; x = x->next)
	{
		last = x;
		if (x->priority >= priority)
			break;
	}

	if (x)
	{
		if (x->prev)
		{
			/* We will insert ourselves just before this item */
			new->prev = x->prev;
			new->next = x;
			x->prev->next = new;
			x->prev = new;
		} else {
			/* We are the new head */
			*list = new;
			new->next = x;
			x->prev = new;
		}
	} else
	{
		/* We are the last item */
		last->next = new;
		new->prev = last;
	}
}

