	/************************************************************************
 *   IRC - Internet Relay Chat, ircd/hash.c
 *   Copyright (C) 1991 Darren Reed
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

#include <limits.h>
#include "numeric.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "hash.h"
#include "h.h"

ID_CVS("$Id$");
ID_Copyright("(C) 1991 Darren Reed");
ID_Notes("2.10 7/3/93");

/* Quick & dirty inline version of mycmp for hash-tables -Donwulff */
#define thecmp(str1, str2, where) { \
                                    register char *st1=str1, *st2=str2; \
                                    while (tolower(*st1)==tolower(*st2)) \
                                    { \
                                      if (!*st1) goto where; \
                                      st1++; st2++; \
                                    } \
                                  }

#ifdef	DEBUGMODE
static	aHashEntry	*clientTable = NULL;
static	aHashEntry	*channelTable = NULL;
static	int	clhits, clmiss;
static	int	chhits, chmiss;
int	HASHSIZE = 32003;
int	CHANNELHASHSIZE = 10007;
#else /* DEBUGMODE */
static	aHashEntry	clientTable[HASHSIZE];
static	aHashEntry	channelTable[CHANNELHASHSIZE];
#endif /* DEBUGMODE */

static	aNotify	*notifyTable[NOTIFYHASHSIZE];

/*
 * Hashing.
 *
 *   The server uses a chained hash table to provide quick and efficient
 * hash table mantainence (providing the hash function works evenly over
 * the input range).  The hash table is thus not susceptible to problems
 * of filling all the buckets or the need to rehash.
 *    It is expected that the hash table would look somehting like this
 * during use:
 *                   +-----+    +-----+    +-----+   +-----+
 *                ---| 224 |----| 225 |----| 226 |---| 227 |---
 *                   +-----+    +-----+    +-----+   +-----+
 *                      |          |          |
 *                   +-----+    +-----+    +-----+
 *                   |  A  |    |  C  |    |  D  |
 *                   +-----+    +-----+    +-----+
 *                      |
 *                   +-----+
 *                   |  B  |
 *                   +-----+
 *
 * A - GOPbot, B - chang, C - hanuaway, D - *.mu.OZ.AU
 *
 * The order shown above is just one instant of the server.  Each time a
 * lookup is made on an entry in the hash table and it is found, the entry
 * is moved to the top of the chain.
 */

#ifndef ELFHASH
/*
 * hash_nn_name
 *
 * Well, extensive profiling seems to indicate that the hash-function
 * is a major provider to ircd's CPU-usage after all, so we're using
 * _really_ minimalistic hash-function and make up for it with huge
 * hash-tables. -Donwulff
 */
unsigned int hash_nn_name (hname)
const char *hname;
{
	unsigned int hash_value;

	for ( hash_value = 0; *hname; ++hname)
	    hash_value = ( hash_value << 2 ) ^ tolower(*hname);

	return ( hash_value );
}
#else
/* Take equal propotions from the size of int on all arcitechtures */
#define BITS_IN_int             ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS	        ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH		((int) (BITS_IN_int / 8))
#define HIGH_BITS		( ~((unsigned int)(~0) >> ONE_EIGHTH ))

unsigned int hash_nn_name (hname)
const char *hname;
{
	unsigned int hash_value, i;

	for ( hash_value = 0; *hname; ++hname )
	{
	    /* Shift hash-value by one eights of int for adding every letter */
	    hash_value = ( hash_value << ONE_EIGHTH ) + tolower(*hname);
	    /* If the next shift would cause an overflow... */
	    if (( i = hash_value & HIGH_BITS ) != 0 )
	        /* Then wrap the upper quarter of bits back to the value */
		hash_value = ( hash_value ^ 
		               ( i >> THREE_QUARTERS )) & ~HIGH_BITS;
	}

	return ( hash_value );
}
#endif

/*
 * clear_*_hash_table
 *
 * Nullify the hashtable and its contents so it is completely empty.
 */
void	clear_client_hash_table()
{
#ifdef	DEBUGMODE
	clhits = 0;
	clmiss = 0;
	if (!clientTable)
		clientTable = (aHashEntry *)MyMalloc(HASHSIZE *
						     sizeof(aHashEntry));
#endif /* DEBUGMODE */

	bzero((char *)clientTable, sizeof(aHashEntry) * HASHSIZE);
}

void	clear_channel_hash_table()
{
#ifdef	DEBUGMODE
	chmiss = 0;
	chhits = 0;
	if (!channelTable)
		channelTable = (aHashEntry *)MyMalloc(CHANNELHASHSIZE *
						     sizeof(aHashEntry));
#endif /* DEBUGMODE */
	bzero((char *)channelTable, sizeof(aHashEntry) * CHANNELHASHSIZE);
}

void	clear_notify_hash_table(void)
{
	bzero((char *)notifyTable, sizeof(notifyTable));
}

/*
 * add_to_client_hash_table
 */
int	add_to_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	int	hashv;
	aClient	*acptr;

	hashv = hash_nn_name(name)%HASHSIZE;

#ifdef DEBUGMODE
	if(acptr=(aClient *)clientTable[hashv].list) {
		while(acptr&&mycmp(acptr->name,name)) acptr=acptr->hnext;
		if(acptr) dumpcore("add_to_client_hash_table %s",name);
	}
	cptr->hnext = (aClient *)clientTable[hashv].list;
	clientTable[hashv].list = (void *)cptr;
	clientTable[hashv].links++;
	clientTable[hashv].hits++;
#else /* DEBUGMODE */
#ifdef INSERTCHECK
	if(acptr=(aClient *)clientTable[hashv]) {
		while(acptr&&mycmp(acptr->name,name)) acptr=acptr->hnext;
		if(acptr) dumpcore("add_to_client_hash_table %s",name);
	}
#endif /* INSERTCHECK */
	cptr->hnext = (aClient *)clientTable[hashv];
	clientTable[hashv] = (void *)cptr;
#endif /* DEBUGMODE */
	return 0;
}

/*
 * add_to_channel_hash_table
 */
int	add_to_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	int	hashv;
	aChannel	*chan;

	hashv = hash_nn_name(name)%CHANNELHASHSIZE;

#ifdef DEBUGMODE
	if(chan=(aChannel *)channelTable[hashv].list) {
		while(chan&&mycmp(chan->chname,name)) chan=chan->hnextch;
		if(chan) dumpcore("add_to_channel_hash_table %s",name);
	}
	chptr->hnextch = (aChannel *)channelTable[hashv].list;
	channelTable[hashv].list = (void *)chptr;
	channelTable[hashv].links++;
	channelTable[hashv].hits++;
#else /* DEBUGMODE */
#ifdef INSERTCHECK
	if(chan=(aChannel *)channelTable[hashv]) {
		while(chan&&mycmp(chan->chname,name)) chan=chan->hnextch;
		if(chan) dumpcore("add_to_channel_hash_table %s",name);
	}
#endif /* INSERTCHECK */
	chptr->hnextch = (aChannel *)channelTable[hashv];
	channelTable[hashv] = (void *)chptr;
#endif /* DEBUGMODE */
	return 0;
}

/*
 * Rough figure of the datastructures for notify:
 *
 * NOTIFY HASH      cptr1
 *   |                |- nick1
 * nick1-|- cptr1     |- nick2
 *   |   |- cptr2                cptr3
 *   |   |- cptr3   cptr2          |- nick1
 *   |                |- nick1
 * nick2-|- cptr2     |- nick2
 *       |- cptr1
 *
 * add-to-notify-hash-table:
 * del-from-notify-hash-table:
 * hash-del-notify-list:
 * hash-check-notify:
 * hash-get-notify:
 */

/*
 * count_watch_memory
 */
void	count_watch_memory(count, memory)
int	*count;
u_long	*memory;
{
	int	i = NOTIFYHASHSIZE;
	aNotify	*anptr;


	while (i--) {
		anptr = notifyTable[i];
		while (anptr) {
			(*count)++;
			(*memory) += sizeof(aNotify)+strlen(anptr->nick);
			anptr = anptr->hnext;
		}
	}
}

/*
 * add_to_notify_hash_table
 */
int	add_to_notify_hash_table(nick, cptr)
char	*nick;
aClient	*cptr;
{
	int	hashv;
	aNotify	*anptr;
	Link	*lp;


	/* Get the right bucket... */
	hashv = hash_nn_name(nick)%NOTIFYHASHSIZE;

	/* Find the right nick (header) in the bucket, or NULL... */
	if ((anptr = (aNotify *)notifyTable[hashv]))
		while (anptr && mycmp(anptr->nick, nick))
			anptr = anptr->hnext;

	/* If found NULL (no header for this nick), make one... */
	if (!anptr) {
		anptr = (aNotify *)MyMalloc(sizeof(aNotify)+strlen(nick));
		anptr->lasttime = 0;
		strcpy(anptr->nick, nick);

		anptr->notify = NULL;

		anptr->hnext = notifyTable[hashv];
		notifyTable[hashv] = anptr;
	}

	/* Is this client already on the notify-list? */
	if ((lp = anptr->notify))
		while (lp && (lp->value.cptr != cptr))
			lp = lp->next;

	/* No it isn't, so add it in the bucket and client addint it */
	if (!lp) {
		lp = anptr->notify;
		anptr->notify = make_link();
		anptr->notify->value.cptr = cptr;
		anptr->notify->next = lp;

		lp = make_link();
		lp->next = cptr->notify;
		lp->value.nptr = anptr;
		cptr->notify = lp;
		cptr->notifies++;
	}

	return 0;
}

/*
 * hash_check_notify
 */
int	hash_check_notify(cptr, reply)
aClient	*cptr;
int	reply;
{
	int	hashv;
	aNotify	*anptr;
	Link	*lp;


	/* Get us the right bucket */
	hashv = hash_nn_name(cptr->name)%NOTIFYHASHSIZE;

	/* Find the right header in this bucket */
	if ((anptr = (aNotify *)notifyTable[hashv]))
		while (anptr && mycmp(anptr->nick, cptr->name))
			anptr = anptr->hnext;
	if (!anptr)
		return 0;	/* This nick isn't on notify */

	/* Update the time of last change to item */
	anptr->lasttime = time(NULL);

	/* Send notifies out to everybody on the list in header */
	for (lp = anptr->notify; lp; lp = lp->next)
		sendto_one(lp->value.cptr, rpl_str(reply), me.name,
			lp->value.cptr->name, cptr->name,
			(IsPerson(cptr)?cptr->user->username:"<N/A>"),
			(IsPerson(cptr)?
			 (IsHidden(cptr) ? cptr->user->virthost : cptr->user->realhost) :"<N/A>"),
			anptr->lasttime, cptr->info);

	return 0;
}

/*
 * hash_get_notify
 */
aNotify	*hash_get_notify(name)
char	*name;
{
	int	hashv;
	aNotify	*anptr;


	hashv = hash_nn_name(name)%NOTIFYHASHSIZE;

	if ((anptr = (aNotify *)notifyTable[hashv]))
		while (anptr && mycmp(anptr->nick, name))
			anptr = anptr->hnext;

	return anptr;
}

/*
 * del_from_notify_hash_table
 */
int	del_from_notify_hash_table(nick, cptr)
char	*nick;
aClient	*cptr;
{
	int	hashv;
	aNotify	*anptr, *nlast = NULL;
	Link	*lp, *last = NULL;


	/* Get the bucket for this nick... */
	hashv = hash_nn_name(nick)%NOTIFYHASHSIZE;

	/* Find the right header, maintaining last-link pointer... */
	if ((anptr = (aNotify *)notifyTable[hashv]))
		while (anptr && mycmp(anptr->nick, nick)) {
			nlast = anptr;
			anptr = anptr->hnext;
		}
	if (!anptr)
		return 0;	/* No such notify */

	/* Find this client from the list of notifies... with last-ptr. */
	if ((lp = anptr->notify))
		while (lp && (lp->value.cptr != cptr)) {
			last = lp;
			lp = lp->next;
		}
	if (!lp)
		return 0;	/* No such client to notify */

	/* Fix the linked list under header, then remove the notify entry */
	if (!last)
		anptr->notify = lp->next;
	else
		last->next = lp->next;
	free_link(lp);

	/* Do the same regarding the links in client-record... */
	last = NULL;
	if ((lp = cptr->notify))
		while (lp && (lp->value.nptr != anptr)) {
			last = lp;
			lp = lp->next;
		}

	/*
	 * Give error on the odd case... probobly not even neccessary
	 *
	 * No error checking in ircd is unneccessary ;) -Cabal95
	 */
	if (!lp)
		sendto_ops("WATCH debug error: del_from_notify_hash_table "
			   "found a watch entry with no client "
			   "counterpoint processing nick %s on client %s!",
			   nick, cptr->user);
	else {
		if (!last) /* First one matched */
			cptr->notify = lp->next;
		else
			last->next = lp->next;
		free_link(lp);
	}

	/* In case this header is now empty of notices, remove it */
	if (!anptr->notify) {
		if (!nlast)
			notifyTable[hashv] = anptr->hnext;
		else
			nlast->hnext = anptr->hnext;
		MyFree(anptr);
	}

	/* Update count of notifies on nick */
	cptr->notifies--;

	return 0;
}

/*
 * hash_del_notify_list
 */
int	hash_del_notify_list(cptr)
aClient	*cptr;
{
	int	hashv;
	aNotify	*anptr;
	Link	*np, *lp, *last;


	if (!(np = cptr->notify))
		return 0;	/* Nothing to do */

	cptr->notify = NULL;	/* Break the notify-list for client */
	while (np) {
		/* Find the notify-record from hash-table... */
		anptr = np->value.nptr;
		last = NULL;
		for (lp = anptr->notify; lp && (lp->value.cptr != cptr);
		     lp = lp->next)
			last = lp;

		/* Not found, another "worst case" debug error */
		if (!lp)
			sendto_ops("WATCH Debug error: hash_del_notify_list "
				   "found a WATCH entry with no table "
				   "counterpoint processing client %s!",
				   cptr->name);
		else {
			/* Fix the notify-list and remove entry */
			if (!last)
				anptr->notify = lp->next;
			else
				last->next = lp->next;
			free_link(lp);

			/*
			 * If this leaves a header without notifies,
			 * remove it. Need to find the last-pointer!
			 */
			if (!anptr->notify) {
				aNotify	*np2, *nl;

				hashv = hash_nn_name(anptr->nick)%NOTIFYHASHSIZE;

				nl = NULL;
				np2 = notifyTable[hashv];
				while (np2 != anptr) {
					nl = np2;
					np2 = np2->hnext;
				}

				if (nl)
					nl->hnext = anptr->hnext;
				else
					notifyTable[hashv] = anptr->hnext;
				MyFree(anptr);
			}
		}

		lp = np;	/* Save last pointer processed */
		np = np->next;	/* Jump to the next pointer */
		free_link(lp);	/* Free the previous */
	}

	cptr->notifies = 0;

	return 0;
}


/*
 * del_from_client_hash_table
 */
int	del_from_client_hash_table(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	aClient	*tmp, *prev = NULL;
	Reg2	int	hashv;

	hashv = hash_nn_name(name)%HASHSIZE;
#ifdef DEBUGMODE
	for (tmp = (aClient *)clientTable[hashv].list; tmp; tmp = tmp->hnext)
	    {
		if (tmp == cptr)
		    {
			if (prev)
				prev->hnext = tmp->hnext;
			else
				clientTable[hashv].list = (void *)tmp->hnext;
			tmp->hnext = NULL;
			if (clientTable[hashv].links > 0)
			    {
				clientTable[hashv].links--;
				return 1;
			    } 
			else
				/*
				 * Should never actually return from here and
				 * if we do it is an error/inconsistency in the
				 * hash table.
				 */
				return -1;
			return 0; /* Found, we can return -Donwulff */
		}
		prev = tmp;
	}
#else /* DEBUGMODE */
	for (tmp = (aClient *)clientTable[hashv]; tmp; tmp = tmp->hnext) {
		if (tmp == cptr) {
			if (prev)
				prev->hnext = tmp->hnext;
			else
				clientTable[hashv] = (void *)tmp->hnext;
			tmp->hnext = NULL;
			return 0; /* Found, we can return -Donwulff */
		}
		prev = tmp;
	    }
#endif /* DEBUGMODE */
	return 0;
}

/*
 * del_from_channel_hash_table
 */
int	del_from_channel_hash_table(name, chptr)
char	*name;
aChannel	*chptr;
{
	Reg1	aChannel	*tmp, *prev = NULL;
	Reg2	int	hashv;

	hashv = hash_nn_name(name)%CHANNELHASHSIZE;
#ifdef DEBUGMODE
	for (tmp = (aChannel *)channelTable[hashv].list; tmp;
	     tmp = tmp->hnextch)
	    {
		if (tmp == chptr)
		    {
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv].list=(void *)tmp->hnextch;
			tmp->hnextch = NULL;
			if (channelTable[hashv].links > 0)
			    {
				channelTable[hashv].links--;
				return 1;
			    }
			else
				return -1;
			return 0; /* Found, we can return -Donwulff */
		    }
		prev = tmp;
	    }
#else /* DEBUGMODE */
	for (tmp = (aChannel *)channelTable[hashv]; tmp; tmp = tmp->hnextch) {
		if (tmp == chptr) {
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv]=(void *)tmp->hnextch;
			tmp->hnextch = NULL;
			return 0; /* Found, we can return -Donwulff */
		}
		prev = tmp;
	}
#endif /* DEBUGMODE */
	return 0;
}


/*
 * hash_get_chan_bucket
 */
aChannel	*hash_get_chan_bucket(hashv)
int	hashv;
{
	if (hashv > CHANNELHASHSIZE)
		return NULL;
#ifdef DEBUGMODE
	return (aChannel *)channelTable[hashv].list;
#else
	return (aChannel *)channelTable[hashv];
#endif
}


/*
 * hash_find_client
 */
aClient	*hash_find_client(name, cptr)
char	*name;
aClient	*cptr;
{
	Reg1	aClient	*tmp;
	Reg2	aClient	*prv = NULL;
#ifdef DEBUGMODE
	Reg3	aHashEntry	*tmp3;
#endif /* DEBUGMODE */
	int	hashv;

	hashv = hash_nn_name(name)%HASHSIZE;

	/*
	 * Got the bucket, now search the chain.
	 */
#ifdef  DEBUGMODE
tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
		thecmp(name, tmp->name, c_move_to_top);
	clmiss++;
	return (cptr);
c_move_to_top:
	clhits++;
	/*
	 * If the member of the hashtable we found isnt at the top of its
	 * chain, put it there.  This builds a most-frequently used order into
	 * the chains of the hash table, giving speadier lookups on those nicks
	 * which are being used currently.  This same block of code is also
	 * used for channels and servers for the same performance reasons.
	 */
	if (prv)
	    {
		aClient *tmp2;

		tmp2 = (aClient *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	    }
#else /* DEBUGMODE */
	for (tmp = (aClient *)clientTable[hashv]; tmp; prv = tmp, tmp = tmp->hnext)
		thecmp(name, tmp->name, c_move_to_top);
	return (cptr);
c_move_to_top:
	if (prv) {
		aClient *tmp2;

		tmp2 = (aClient *)clientTable[hashv];
		clientTable[hashv] = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	}
#endif /* DEBUGMODE */
	return (tmp);
}

/*
 * hash_find_nickserver
 */
aClient	*hash_find_nickserver(name, cptr)
char	*name;
aClient *cptr;
{
	Reg1	aClient	*tmp;
	Reg2	aClient	*prv = NULL;
#ifdef DEBUGMODE
	Reg3	aHashEntry	*tmp3;
#endif /* DEBUGMODE */
	int	hashv;
	char	*serv;

	serv = index(name, '@');
	*serv++ = '\0';
	hashv = hash_nn_name(name)%HASHSIZE;

	/*
	 * Got the bucket, now search the chain.
	 */
#ifdef  DEBUGMODE
tmp3 = &clientTable[hashv];
	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
		if (tmp->user &&
                    mycmp(name, tmp->name) == 0 &&
		    mycmp(serv, tmp->user->server) == 0)
			goto c_move_to_top;

	clmiss++;
/*	*--serv = '\0'; */ /* This code has no function, perhaps meant @? */
	return (cptr);     /* For now, just remarking it out... -Donwulff */

c_move_to_top:
	clhits++;
	if (prv)
	    {
		aClient *tmp2;

		tmp2 = (aClient *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	    }
#else /* DEBUGMODE */
	for (tmp = (aClient *)clientTable[hashv]; tmp; prv = tmp, tmp = tmp->hnext)
		if (tmp->user &&
		    mycmp(name, tmp->name) == 0 &&
		    mycmp(serv, tmp->user->server) == 0)
			goto c_move_to_top;
/*	*--serv = '\0'; */ /* This code has no function, perhaps meant @? */
	return (cptr);     /* For now, just remarking it out... -Donwulff */

c_move_to_top:
	if (prv) {
		aClient *tmp2;

		tmp2 = (aClient *)clientTable[hashv];
		clientTable[hashv] = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	}
#endif /* DEBUGMODE */
/*	*--serv = '\0'; */ /* This code has no function, perhaps meant @? */
	return (tmp);      /* For now, just remarking it out... -Donwulff */
}

/*
 * hash_find_server
 */
aClient	*hash_find_server(server, cptr)
char	*server;
aClient *cptr;
{
	Reg1	aClient	*tmp, *prv = NULL;
	Reg2	char	*t;
	Reg3	char	ch;
#ifdef DEBUGMODE
	aHashEntry	*tmp3;
#endif /* DEBUGMODE */

	int hashv;

	hashv = hash_nn_name(server)%HASHSIZE;

#ifdef  DEBUGMODE
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnext)
	    {
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		thecmp(server, tmp->name, s_move_to_top);
	    }
#else /* DEBUGMODE */
	for (tmp = (aClient *)clientTable[hashv]; tmp; prv = tmp, tmp = tmp->hnext) {
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		thecmp(server, tmp->name, s_move_to_top);
	}
#endif /* DEBUGMODE */
	t = ((char *)server + strlen(server));
	/*
	 * Whats happening in this next loop ? Well, it takes a name like
	 * foo.bar.edu and proceeds to search for *.edu and then *.bar.edu.
	 * This is for checking full server names against masks although
	 * it isnt often done this way in lieu of using match().
	 */
	for (;;)
	    {
		t--;
		for (; t > server; t--)
			if (*(t+1) == '.')
				break;
		if (t <= server || *t == '*')
			break;
		ch = *t;
		*t = '*';
		/*
	 	 * Dont need to check IsServer() here since nicknames cant
		 *have *'s in them anyway.
		 */
		if (((tmp = hash_find_client(t, cptr))) != cptr)
		    {
			*t = ch;
			return (tmp);
		    }
		*t = ch;
	    }
#ifdef	DEBUGMODE
	clmiss++;
	return (cptr);
s_move_to_top:
	clhits++;

	if (prv)
	    {
		aClient *tmp2;

		tmp2 = (aClient *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnext = tmp->hnext;
		tmp->hnext = tmp2;
	    }
#else /* DEBUGMODE */
	return (cptr);
s_move_to_top:
	if (prv) {
        	aClient *tmp2;

		tmp2 = (aClient *)clientTable[hashv];
	        clientTable[hashv] = (void *)tmp;
	        prv->hnext = tmp->hnext;
	        tmp->hnext = tmp2;
	}
#endif /* DEBUGMODE */
	return (tmp);
}

/*
 * hash_find_channel
 */
aChannel	*hash_find_channel(name, chptr)
char	*name;
aChannel *chptr;
{
	int	hashv;
	Reg1    aChannel	*tmp;
	Reg2    aChannel	*prv = NULL;
#ifdef DEBUGMODE
	aHashEntry	*tmp3;
#endif /* DEBUGMODE */
	hashv = hash_nn_name(name)%CHANNELHASHSIZE;

#ifdef  DEBUGMODE
	tmp3 = &channelTable[hashv];

	for (tmp = (aChannel *)tmp3->list; tmp; prv = tmp, tmp = tmp->hnextch)
		thecmp(name, tmp->chname, c_move_to_top);
	chmiss++;
	return chptr;
c_move_to_top:
	chhits++;
	if (prv)
	    {
		register aChannel *tmp2;

		tmp2 = (aChannel *)tmp3->list;
		tmp3->list = (void *)tmp;
		prv->hnextch = tmp->hnextch;
		tmp->hnextch = tmp2;
	    }
#else /* DEBUGMODE */
	for (tmp = (aChannel *)channelTable[hashv]; tmp; prv = tmp, tmp = tmp->hnextch)
		thecmp(name, tmp->chname, c_move_to_top);
	return chptr;
c_move_to_top:
	if(prv) {
		register	aChannel	*tmp2;

		tmp2 = (aChannel *)channelTable[hashv];
		channelTable[hashv] = (void *)tmp;
		prv->hnextch = tmp->hnextch;
		tmp->hnextch = tmp2;
	}
#endif /* DEBUGMODE */
	return (tmp);
}

/*
 * NOTE: this command is not supposed to be an offical part of the ircd
 *       protocol.  It is simply here to help debug and to monitor the
 *       performance of the hash functions and table, enabling a better
 *       algorithm to be sought if this one becomes troublesome.
 *       -avalon
 */

int	m_hash(cptr, sptr, parc, parv)
aClient	*cptr, *sptr;
int	parc;
char	*parv[];
{
#ifndef DEBUGMODE
	return 0;
#else
	register	int	l, i;
	register	aHashEntry	*tab;
	int	deepest = 0, deeplink = 0, showlist = 0, tothits = 0;
	int	mosthit = 0, mosthits = 0, used = 0, used_now = 0, totlink = 0;
	int	link_pop[10], size = HASHSIZE;
	char	ch;
	aHashEntry	*table;

	if(!IsPrivileged(sptr)) return 0;

	if (parc > 1) {
		ch = *parv[1];
		if (islower(ch))
			table = clientTable;
		else {
			table = channelTable;
			size = CHANNELHASHSIZE;
		}
		if (ch == 'L' || ch == 'l')
			showlist = 1;
	} else {
		ch = '\0';
		table = clientTable;
	}

	for (i = 0; i < 10; i++)
		link_pop[i] = 0;
	for (i = 0; i < size; i++) {
		tab = &table[i];
		l = tab->links;
		if (showlist)
		    sendto_one(sptr,
			   "NOTICE %s :Hash Entry:%6d Hits:%7d Links:%6d",
			   parv[0], i, tab->hits, l);
		if (l > 0) {
			if (l < 10)
				link_pop[l]++;
			else
				link_pop[9]++;
			used_now++;
			totlink += l;
			if (l > deepest) {
				deepest = l;
				deeplink = i;
			}
		}
		else
			link_pop[0]++;
		l = tab->hits;
		if (l) {
			used++;
			tothits += l;
			if (l > mosthits) {
				mosthits = l;
				mosthit = i;
			}
		}
	}
	switch((int)ch)
	{
	case 'V' : case 'v' :
	    {
		register	aClient	*acptr;
		int	bad = 0, listlength = 0;

		for (acptr = client; acptr; acptr = acptr->next) {
			if (hash_find_client(acptr->name,acptr) != acptr) {
				if (ch == 'V')
				sendto_one(sptr, "NOTICE %s :Bad hash for %s",
					   parv[0], acptr->name);
				bad++;
			}
			listlength++;
		}
		sendto_one(sptr,"NOTICE %s :List Length: %d Bad Hashes: %d",
			   parv[0], listlength, bad);
	    }
	case 'P' : case 'p' :
		for (i = 0; i < 10; i++)
			sendto_one(sptr,"NOTICE %s :Entires with %d links : %d",
			parv[0], i, link_pop[i]);
		return (0);
	case 'r' :
	    {
		Reg1	aClient	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Client List.", parv[0]);
		clear_client_hash_table();
		for (acptr = client; acptr; acptr = acptr->next)
			(void)add_to_client_hash_table(acptr->name, acptr);
		break;
	    }
	case 'R' :
	    {
		Reg1	aChannel	*acptr;

		sendto_one(sptr,"NOTICE %s :Rehashing Channel List.", parv[0]);
		clear_channel_hash_table();
		for (acptr = channel; acptr; acptr = acptr->nextch)
			(void)add_to_channel_hash_table(acptr->chname, acptr);
		break;
	    }
	case 'H' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
                       hash_nn_name(parv[2])%CHANNELHASHSIZE);
		return (0);
	case 'h' :
		if (parc > 2)
			sendto_one(sptr,"NOTICE %s :%s hash to entry %d",
				   parv[0], parv[2],
                       hash_nn_name(parv[2])%HASHSIZE);
		return (0);
/* Quick hack for getting memory statistics from list.c -Donwulff */
	case 'm' :
		send_listinfo(sptr, parv[0]);
		return(0);
	case 'n' :
	    {
		aClient	*tmp;
		int	max;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % HASHSIZE;
		if (parc > 3)
			max = atoi(parv[3]) % HASHSIZE;
		else
			max = l;
		for (;l <= max; l++)
			for (i = 0, tmp = (aClient *)clientTable[l].list; tmp;
			     i++, tmp = tmp->hnext)
			    {
				if (parv[1][2] == '1' && tmp != tmp->from)
					continue;
				sendto_one(sptr,"NOTICE %s :Node: %d #%d %s",
					   parv[0], l, i, tmp->name);
			    }
		return (0);
	    }
	case 'N' :
	    {
		aChannel *tmp;
		int	max;

		if (parc <= 2)
			return (0);
		l = atoi(parv[2]) % CHANNELHASHSIZE;
		if (parc > 3)
			max = atoi(parv[3]) % CHANNELHASHSIZE;
		else
			max = l;
		for (;l <= max; l++)
			for (i = 0, tmp = (aChannel *)channelTable[l].list; tmp;
			     i++, tmp = tmp->hnextch)
				sendto_one(sptr,"NOTICE %s :Node: %d #%d %s",
					   parv[0], l, i, tmp->chname);
		return (0);
	    }
	case 'z' :
	    {
		Reg1	aClient	*acptr;

		if (parc <= 2)
			return 0;
		l = atoi(parv[2]);
		if (l < 256)
			return 0;
		(void)MyFree((char *)clientTable);
		clientTable = (aHashEntry *)MyMalloc(sizeof(aHashEntry) * l);
		HASHSIZE = l;
		clear_client_hash_table();
		for (acptr = client; acptr; acptr = acptr->next)
		    {
			acptr->hnext = NULL;
			(void)add_to_client_hash_table(acptr->name, acptr);
		    }
		sendto_one(sptr, "NOTICE %s :HASHSIZE now %d", parv[0], l);
		break;
	    }
	case 'Z' :
	    {
		Reg1	aChannel	*acptr;

		if (parc <= 2)
			return 0;
		l = atoi(parv[2]);
		if (l < 256)
			return 0;
		(void)MyFree((char *)channelTable);
		channelTable = (aHashEntry *)MyMalloc(sizeof(aHashEntry) * l);
		CHANNELHASHSIZE = l;
		clear_channel_hash_table();
		for (acptr = channel; acptr; acptr = acptr->nextch)
		    {
			acptr->hnextch = NULL;
			(void)add_to_channel_hash_table(acptr->chname, acptr);
		    }
		sendto_one(sptr, "NOTICE %s :CHANNELHASHSIZE now %d",
			   parv[0], l);
		break;
	    }
	default :
		break;
	}
	sendto_one(sptr,"NOTICE %s :Entries Hashed: %d NonEmpty: %d of %d",
		   parv[0], totlink, used_now, size);
	if (!used_now)
		used_now = 1;
    sendto_one(sptr,"NOTICE %s :Hash Ratio (av. depth): %f %%Full: %f",
		  parv[0], (float)((1.0 * totlink) / (1.0 * used_now)),
		  (float)((1.0 * used_now) / (1.0 * size)));
	sendto_one(sptr,"NOTICE %s :Deepest Link: %d Links: %d",
		   parv[0], deeplink, deepest);
	if (!used)
		used = 1;
	sendto_one(sptr,"NOTICE %s :Total Hits: %d Unhit: %d Av Hits: %f",
		   parv[0], tothits, size-used,
		   (float)((1.0 * tothits) / (1.0 * used)));
	sendto_one(sptr,"NOTICE %s :Entry Most Hit: %d Hits: %d",
		   parv[0], mosthit, mosthits);
	sendto_one(sptr,"NOTICE %s :Client hits %d miss %d",
		   parv[0], clhits, clmiss);
	sendto_one(sptr,"NOTICE %s :Channel hits %d miss %d",
		   parv[0], chhits, chmiss);
	return 0;
#endif /* DEBUGMODE */
}
