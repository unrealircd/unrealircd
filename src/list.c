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
#include "numeric.h"
#ifdef	DBMALLOC
#include "malloc.h"
#endif
void free_link PROTO((Link *));
Link *make_link PROTO(());
extern ircstats IRCstats;

ID_CVS("$Id$");
ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.24 4/20/94");

#ifdef	DEBUGMODE
static struct liststats {
	int  inuse;
} cloc, crem, users, servs, links, classs, aconfs;

#endif

void outofmemory();

int  flinks = 0;
int  freelinks = 0;
Link *freelink = NULL;


int  numclients = 0;

void initlists()
{
#ifdef	DEBUGMODE
	bzero((char *)&cloc, sizeof(cloc));
	bzero((char *)&crem, sizeof(crem));
	bzero((char *)&users, sizeof(users));
	bzero((char *)&servs, sizeof(servs));
	bzero((char *)&links, sizeof(links));
	bzero((char *)&classs, sizeof(classs));
	bzero((char *)&aconfs, sizeof(aconfs));
#endif
}

void outofmemory()
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
aClient *make_client(from, servr)
	aClient *from, *servr;
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
	cptr->fd = -1;
	(void)strcpy(cptr->username, "unknown");
	if (size == CLIENT_LOCAL_SIZE)
	{
		cptr->since = cptr->lasttime =
		    cptr->lastnick = cptr->firsttime = TStime();
		cptr->confs = NULL;
		cptr->sockhost[0] = '\0';
		cptr->buffer[0] = '\0';
		cptr->authfd = -1;
#ifdef CRYPTOIRCD
		cptr->cryptinfo = NULL;
#endif
#ifdef SOCKSPORT
		cptr->socksfd = -1;
#endif
	}
	return (cptr);
}

void free_client(cptr)
	aClient *cptr;
{
#ifdef CRYPTOIRCD
	if (cptr->cryptinfo)
		MyFree((char *)cptr->cryptinfo);
#endif
	MyFree((char *)cptr);
}

/*
** 'make_user' add's an User information block to a client
** if it was not previously allocated.
*/
anUser *make_user(cptr)
	aClient *cptr;
{
	anUser *user;

	user = cptr->user;
	if (!user)
	{
		user = (anUser *)MyMalloc(sizeof(anUser));
#ifdef	DEBUGMODE
		users.inuse++;
#endif
		user->swhois = NULL;
		user->away = NULL;
		user->refcnt = 1;
		user->joined = 0;
		user->channel = NULL;
		user->invited = NULL;
		user->silence = NULL;
		user->server = NULL;
		user->virthost = MyMalloc(2);
		*user->virthost = '\0';
		cptr->user = user;
	}
	return user;
}

aServer *make_server(cptr)
	aClient *cptr;
{
	aServer *serv = cptr->serv;

	if (!serv)
	{
		serv = (aServer *)MyMalloc(sizeof(aServer));
#ifdef	DEBUGMODE
		servs.inuse++;
#endif
		serv->user = NULL;
		serv->nexts = NULL;
		*serv->by = '\0';
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
void free_user(user, cptr)
	anUser *user;
	aClient *cptr;
{
	if (--user->refcnt <= 0)
	{
		if (user->away)
			MyFree((char *)user->away);
		if (user->swhois)
			MyFree((char *)user->swhois);
		if (user->virthost)
			MyFree((char *)user->virthost);
		/*
		 * sanity check
		 */
		if (user->joined || user->refcnt < 0 ||
		    user->invited || user->channel)
#ifdef DEBUGMODE
			dumpcore("%#x user (%s!%s@%s) %#x %#x %#x %d %d",
			    cptr, cptr ? cptr->name : "<noname>",
			    user->username, user->realhost, user,
			    user->invited, user->channel, user->joined,
			    user->refcnt);
#else
			sendto_ops("* %#x user (%s!%s@%s) %#x %#x %#x %d %d *",
			    cptr, cptr ? cptr->name : "<noname>",
			    user->username, user->realhost, user,
			    user->invited, user->channel, user->joined,
			    user->refcnt);
#endif
		MyFree((char *)user);
#ifdef	DEBUGMODE
		users.inuse--;
#endif
	}
}

/*
 * taken the code from ExitOneClient() for this and placed it here.
 * - avalon
 */
void remove_client_from_list(cptr)
	aClient *cptr;
{
	if (IsServer(cptr))
		IRCstats.servers--;
	if (IsClient(cptr))
	{
		if (IsInvisible(cptr))
			IRCstats.invisible--;
		if (IsOper(cptr))
			IRCstats.operators--;
		IRCstats.clients--;
	}
	if (IsUnknown(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
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
void add_client_to_list(cptr)
	aClient *cptr;
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
Link *find_user_link(lp, ptr)
	Link *lp;
	aClient *ptr;
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

Link *find_channel_link(lp, ptr)
	Link *lp;
	aChannel *ptr;
{
	if (ptr)
		while (lp)
		{
			if (lp->value.chptr == ptr)
				return (lp);
			lp = lp->next;
		}
	return NULL;
}



/*
 * Look for a match in a list of strings. Go through the list, and run
 * match() on it. Side effect: if found, this link is moved to the top of
 * the list.
 */
int  find_str_match_link(lp, str)
	Link **lp;		/* Two **'s, since we might modify the original *lp */
	char *str;
{
	Link **head = lp;
	if (!str || !lp)
		return 0;
	if (lp && *lp)
	{
		if (!match((*lp)->value.cp, str))
			return 1;
		for (; (*lp)->next; *lp = (*lp)->next)
			if (!match((*lp)->next->value.cp, str))
			{
				Link *temp = (*lp)->next;
				*lp = (*lp)->next->next;
				temp->next = *head;
				*head = temp;
				return 1;
			}
		return 0;
	}
	return 0;
}

void free_str_list(lp)
	Link *lp;
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

Link *make_link()
{
	Link *lp, *lp1;
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

void free_link(lp)
	Link *lp;
{
	lp->next = freelink;
	freelink = lp;
	freelinks++;

#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

Ban *make_ban()
{
	Ban *lp;

	lp = (Ban *) MyMalloc(sizeof(Ban));
#ifdef	DEBUGMODE
	links.inuse++;
#endif
	return lp;
}

void free_ban(lp)
	Ban *lp;
{
	MyFree((char *)lp);
#ifdef	DEBUGMODE
	links.inuse--;
#endif
}

aClass *make_class()
{
	aClass *tmp;

	tmp = (aClass *)MyMalloc(sizeof(aClass));
#ifdef	DEBUGMODE
	classs.inuse++;
#endif
	return tmp;
}

void free_class(tmp)
	aClass *tmp;
{
	MyFree((char *)tmp);
#ifdef	DEBUGMODE
	classs.inuse--;
#endif
}

aSqlineItem *make_sqline()
{
	aSqlineItem *asqline;

	asqline = (struct SqlineItem *)MyMalloc(sizeof(aSqlineItem));
	asqline->next = NULL;
	asqline->sqline = asqline->reason = NULL;

	return (asqline);
}

aConfItem *make_conf()
{
	aConfItem *aconf;

	aconf = (struct ConfItem *)MyMalloc(sizeof(aConfItem));
#ifdef	DEBUGMODE
	aconfs.inuse++;
#endif
	bzero((char *)&aconf->ipnum, sizeof(struct IN_ADDR));
	aconf->next = NULL;
	aconf->host = aconf->passwd = aconf->name = NULL;
	aconf->status = CONF_ILLEGAL;
	aconf->clients = 0;
	aconf->port = 0;
	aconf->hold = 0;
	Class(aconf) = 0;
	return (aconf);
}

void delist_conf(aconf)
	aConfItem *aconf;
{
	if (aconf == conf)
		conf = conf->next;
	else
	{
		aConfItem *bconf;

		for (bconf = conf; aconf != bconf->next; bconf = bconf->next)
			;
		bconf->next = aconf->next;
	}
	aconf->next = NULL;
}

void free_sqline(asqline)
	aSqlineItem *asqline;
{
	del_queries((char *)asqline);
	MyFree(asqline->sqline);
	MyFree(asqline->reason);
	MyFree((char *)asqline);
	return;
}

void free_conf(aconf)
	aConfItem *aconf;
{
	del_queries((char *)aconf);
	MyFree(aconf->host);
	if (aconf->passwd)
		bzero(aconf->passwd, strlen(aconf->passwd));
	MyFree(aconf->passwd);
	MyFree(aconf->name);
	MyFree((char *)aconf);
#ifdef	DEBUGMODE
	aconfs.inuse--;
#endif
	return;
}

#ifdef	DEBUGMODE
void send_listinfo(cptr, name)
	aClient *cptr;
	char *name;
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
	inuse += classs.inuse,
	    sendto_one(cptr, ":%s %d %s :Confs: inuse: %d(%d)",
	    me.name, RPL_STATSDEBUG, name, aconfs.inuse,
	    tmp = aconfs.inuse * sizeof(aConfItem));
	mem += tmp;
	inuse += aconfs.inuse,
	    sendto_one(cptr, ":%s %d %s :Totals: inuse %d %d",
	    me.name, RPL_STATSDEBUG, name, inuse, mem);
}

#endif
