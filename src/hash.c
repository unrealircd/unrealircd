/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/hash.c
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

#include <string.h>
#include <limits.h>
#include "numeric.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "hash.h"
#include "h.h"
#include "proto.h"

ID_Copyright("(C) 1991 Darren Reed");
ID_Notes("2.10 7/3/93");

static aHashEntry clientTable[U_MAX];
static aHashEntry channelTable[CH_MAX];

/*
 * look in whowas.c for the missing ...[WW_MAX]; entry - Dianora */
/*
 * Hashing.
 * 
 * The server uses a chained hash table to provide quick and efficient
 * hash table mantainence (providing the hash function works evenly
 * over the input range).  The hash table is thus not susceptible to
 * problems of filling all the buckets or the need to rehash. It is
 * expected that the hash table would look somehting like this during
 * use: +-----+    +-----+    +-----+   +-----+ 
 *   ---| 224 |----| 225 |----| 226 |---| 227 |--- 
 *      +-----+    +-----+    +-----+   +-----+ 
 *         |          |          | 
 *      +-----+    +-----+    +-----+ 
 *      |  A  |    |  C  |    |  D  | 
 *      +-----+    +-----+    +-----+ 
 *         | 
 *      +-----+ 
 *      |  B  | 
 *      +-----+
 * 
 * A - GOPbot, B - chang, C - hanuaway, D - *.mu.OZ.AU
 * 
 * The order shown above is just one instant of the server.  Each time a
 * lookup is made on an entry in the hash table and it is found, the
 * entry is moved to the top of the chain.
 * 
 * ^^^^^^^^^^^^^^^^ **** Not anymore - Dianora
 * 
 */
/* Take equal propotions from the size of int on all arcitechtures */
#define BITS_IN_int             ( sizeof(int) * CHAR_BIT )
#define THREE_QUARTERS          ((int) ((BITS_IN_int * 3) / 4))
#define ONE_EIGHTH              ((int) (BITS_IN_int / 8))
#define HIGH_BITS               ( ~((unsigned int)(~0) >> ONE_EIGHTH ))

unsigned int hash_nn_name(const char *hname)
{
	unsigned int hash_value, i;

	for (hash_value = 0; *hname; ++hname)
	{
		/* Shift hash-value by one eights of int for adding every letter */
		hash_value = (hash_value << ONE_EIGHTH) + tolower(*hname);
		/* If the next shift would cause an overflow... */
		if ((i = hash_value & HIGH_BITS) != 0)
			/* Then wrap the upper quarter of bits back to the value */
			hash_value = (hash_value ^
			    (i >> THREE_QUARTERS)) & ~HIGH_BITS;
	}

	return (hash_value);
}


unsigned hash_nick_name(char *nname)
{
	unsigned hash = 0;
	int  hash2 = 0;
	int  ret;
	char lower;

	while (*nname)
	{
		lower = tolower(*nname);
		hash = (hash << 1) + lower;
		hash2 = (hash2 >> 1) + lower;
		nname++;
	}
	ret = ((hash & U_MAX_INITIAL_MASK) << BITS_PER_COL) +
	    (hash2 & BITS_PER_COL_MASK);
	return ret;
}
/*
 * hash_channel_name
 * 
 * calculate a hash value on at most the first 30 characters of the
 * channel name. Most names are short than this or dissimilar in this
 * range. There is little or no point hashing on a full channel name
 * which maybe 255 chars long.
 */
unsigned int  hash_channel_name(char *name)
{
	unsigned char *hname = (unsigned char *)name;
	unsigned int hash = 0;
	int  hash2 = 0;
	char lower;
	int  i = 30;

	while (*hname && --i)
	{
		lower = tolower(*hname);
		hash = (hash << 1) + lower;
		hash2 = (hash2 >> 1) + lower;
		hname++;
	}
	return ((hash & CH_MAX_INITIAL_MASK) << BITS_PER_COL) +
	    (hash2 & BITS_PER_COL_MASK);
}

unsigned int hash_whowas_name(char *name)
{
	unsigned char *nname = (unsigned char *)name;
	unsigned int hash = 0;
	int  hash2 = 0;
	int  ret;
	char lower;

	while (*nname)
	{
		lower = tolower(*nname);
		hash = (hash << 1) + lower;
		hash2 = (hash2 >> 1) + lower;
		nname++;
	}
	ret = ((hash & WW_MAX_INITIAL_MASK) << BITS_PER_COL) +
	    (hash2 & BITS_PER_COL_MASK);
	return ret;
}
/*
 * clear_*_hash_table
 * 
 * Nullify the hashtable and its contents so it is completely empty.
 */
void clear_client_hash_table(void)
{
	memset((char *)clientTable, '\0', sizeof(aHashEntry) * U_MAX);
}

void clear_channel_hash_table(void)
{
	memset((char *)channelTable, '\0', sizeof(aHashEntry) * CH_MAX);
}


/*
 * add_to_client_hash_table
 */
int  add_to_client_hash_table(char *name, aClient *cptr)
{
	unsigned int  hashv;
	/*
	 * If you see this, you have probably found your way to why changing the 
	 * base version made the IRCd become weird. This has been the case in all
	 * Unreal versions since 3.0. I'm sick of people ripping the IRCd off and 
	 * just slapping on some random <theirnet> BASE_VERSION while not changing
	 * a single bit of code. YOU DID NOT WRITE ALL OF THIS THEREFORE YOU DO NOT
	 * DESERVE TO BE ABLE TO DO THAT. If you found this however, I'm OK with you 
	 * removing the checks. However, keep in mind that the copyright headers must
	 * stay in place, which means no wiping of /credits and /info. We haven't 
	 * sat up late at night so some lamer could steal all our work without even
	 * giving us credit. Remember to follow all regulations in LICENSE.
	 * -Stskeeps
	*/
	if (loop.tainted)
		return 0;
	hashv = hash_nick_name(name);
	cptr->hnext = (aClient *)clientTable[hashv].list;
	clientTable[hashv].list = (void *)cptr;
	clientTable[hashv].links++;
	clientTable[hashv].hits++;
	return 0;
}
/*
 * add_to_channel_hash_table
 */
int  add_to_channel_hash_table(char *name, aChannel *chptr)
{
	unsigned int  hashv;

	hashv = hash_channel_name(name);
	chptr->hnextch = (aChannel *)channelTable[hashv].list;
	channelTable[hashv].list = (void *)chptr;
	channelTable[hashv].links++;
	channelTable[hashv].hits++;
	return 0;
}
/*
 * del_from_client_hash_table
 */
int  del_from_client_hash_table(char *name, aClient *cptr)
{
	aClient *tmp, *prev = NULL;
	unsigned int  hashv;

	hashv = hash_nick_name(name);
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
				 * Should never actually return from here and if we do it
				 * is an error/inconsistency in the hash table.
				 */
				return -1;
		}
		prev = tmp;
	}
	return 0;
}
/*
 * del_from_channel_hash_table
 */
int  del_from_channel_hash_table(char *name, aChannel *chptr)
{
	aChannel *tmp, *prev = NULL;
	unsigned int  hashv;

	hashv = hash_channel_name(name);
	for (tmp = (aChannel *)channelTable[hashv].list; tmp;
	    tmp = tmp->hnextch)
	{
		if (tmp == chptr)
		{
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv].list = (void *)tmp->hnextch;
			tmp->hnextch = NULL;
			if (channelTable[hashv].links > 0)
			{
				channelTable[hashv].links--;
				return 1;
			}
			else
				return -1;
		}
		prev = tmp;
	}
	return 0;
}

/*
 * hash_find_client
 */
aClient *hash_find_client(char *name, aClient *cptr)
{
	aClient *tmp;
	aHashEntry *tmp3;
	unsigned int  hashv;

	hashv = hash_nick_name(name);
	tmp3 = &clientTable[hashv];
	/*
	 * Got the bucket, now search the chain.
	 */
	for (tmp = (aClient *)tmp3->list; tmp; tmp = tmp->hnext)
		if (smycmp(name, tmp->name) == 0)
		{
			return (tmp);
		}
	return (cptr);
	/*
	 * If the member of the hashtable we found isnt at the top of its
	 * chain, put it there.  This builds a most-frequently used order
	 * into the chains of the hash table, giving speedier lookups on
	 * those nicks which are being used currently.  This same block of
	 * code is also used for channels and servers for the same
	 * performance reasons.
	 * 
	 * I don't believe it does.. it only wastes CPU, lets try it and
	 * see....
	 * 
	 * - Dianora
	 */
}

/*
 * hash_find_nickserver
 */
aClient *hash_find_nickserver(char *name, aClient *cptr)
{
	aClient *tmp;
	aHashEntry *tmp3;
	unsigned int  hashv;
	char *serv;

	serv = (char *)strchr(name, '@');
	*serv++ = '\0';
	hashv = hash_nick_name(name);
	tmp3 = &clientTable[hashv];
	/*
	 * Got the bucket, now search the chain.
	 */
	for (tmp = (aClient *)tmp3->list; tmp; tmp = tmp->hnext)
		if (smycmp(name, tmp->name) == 0 && tmp->user &&
		    smycmp(serv, tmp->user->server) == 0)
		{
			*--serv = '\0';
			return (tmp);
		}

	*--serv = '\0';
	return (cptr);
}
/*
 * hash_find_server
 */
aClient *hash_find_server(char *server, aClient *cptr)
{
	aClient *tmp;
#if 0
	char *t;
	char ch;
#endif
	aHashEntry *tmp3;

	unsigned int  hashv;

	hashv = hash_nick_name(server);
	tmp3 = &clientTable[hashv];

	for (tmp = (aClient *)tmp3->list; tmp; tmp = tmp->hnext)
	{
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		if (smycmp(server, tmp->name) == 0)
		{
			return (tmp);
		}
	}

	/*
	 * Whats happening in this next loop ? Well, it takes a name like
	 * foo.bar.edu and proceeds to earch for *.edu and then *.bar.edu.
	 * This is for checking full server names against masks although it
	 * isnt often done this way in lieu of using matches().
	 */

	/* why in god's name would we ever want to do something like this?
	 * commented out, probably to be removed sooner or later - lucas 
	 */

#if 0
	t = ((char *)server + strlen(server));

	for (;;)
	{
		t--;
		for (; t > server; t--)
			if (*(t + 1) == '.')
				break;
		if (*t == '*' || t == server)
			break;
		ch = *t;
		*t = '*';
		/*
		 * Dont need to check IsServer() here since nicknames cant have
		 * *'s in them anyway.
		 */
		if (((tmp = hash_find_client(t, cptr))) != cptr)
		{
			*t = ch;
			return (tmp);
		}
		*t = ch;
	}
#endif
	return (cptr);
}

/*
 * hash_find_channel
 */
aChannel *hash_find_channel(char *name, aChannel *chptr)
{
	unsigned int  hashv;
	aChannel *tmp;
	aHashEntry *tmp3;

	hashv = hash_channel_name(name);
	tmp3 = &channelTable[hashv];

	for (tmp = (aChannel *)tmp3->list; tmp; tmp = tmp->hnextch)
		if (smycmp(name, tmp->chname) == 0)
		{
			return (tmp);
		}
	return chptr;
}
aChannel *hash_get_chan_bucket(unsigned int hashv)
{
	if (hashv > CH_MAX)
		return NULL;
	return (aChannel *)channelTable[hashv].list;
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

static   aWatch  *watchTable[WATCHHASHSIZE];

void  count_watch_memory(int *count, u_long *memory)
{
	int   i = WATCHHASHSIZE;
	aWatch  *anptr;
	
	
	while (i--) {
		anptr = watchTable[i];
		while (anptr) {
			(*count)++;
			(*memory) += sizeof(aWatch)+strlen(anptr->nick);
			anptr = anptr->hnext;
		}
	}
}
extern char unreallogo[];
void  clear_watch_hash_table(void)
{
	   memset((char *)watchTable, '\0', sizeof(watchTable));
	   if (strcmp(BASE_VERSION, &unreallogo[337]))
		loop.tainted = 1;
}


/*
 * add_to_watch_hash_table
 */
int   add_to_watch_hash_table(char *nick, aClient *cptr, int awaynotify)
{
	unsigned int   hashv;
	aWatch  *anptr;
	Link  *lp;
	
	
	/* Get the right bucket... */
	hashv = hash_nick_name(nick)%WATCHHASHSIZE;
	
	/* Find the right nick (header) in the bucket, or NULL... */
	if ((anptr = (aWatch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, nick))
		 anptr = anptr->hnext;
	
	/* If found NULL (no header for this nick), make one... */
	if (!anptr) {
		anptr = (aWatch *)MyMalloc(sizeof(aWatch)+strlen(nick));
		anptr->lasttime = timeofday;
		strcpy(anptr->nick, nick);
		
		anptr->watch = NULL;
		
		anptr->hnext = watchTable[hashv];
		watchTable[hashv] = anptr;
	}
	/* Is this client already on the watch-list? */
	if ((lp = anptr->watch))
	  while (lp && (lp->value.cptr != cptr))
		 lp = lp->next;
	
	/* No it isn't, so add it in the bucket and client addint it */
	if (!lp) {
		lp = anptr->watch;
		anptr->watch = make_link();
		anptr->watch->value.cptr = cptr;
		anptr->watch->flags = awaynotify;
		anptr->watch->next = lp;
		
		lp = make_link();
		lp->next = cptr->watch;
		lp->value.wptr = anptr;
		lp->flags = awaynotify;
		cptr->watch = lp;
		cptr->watches++;
	}
	
	return 0;
}

/*
 *  hash_check_watch
 */
int   hash_check_watch(aClient *cptr, int reply)
{
	unsigned int   hashv;
	aWatch  *anptr;
	Link  *lp;
	int awaynotify = 0;
	
	if ((reply == RPL_GONEAWAY) || (reply == RPL_NOTAWAY) || (reply == RPL_REAWAY))
		awaynotify = 1;
	
	
	/* Get us the right bucket */
	hashv = hash_nick_name(cptr->name)%WATCHHASHSIZE;
	
	/* Find the right header in this bucket */
	if ((anptr = (aWatch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, cptr->name))
		 anptr = anptr->hnext;
	if (!anptr)
	  return 0;   /* This nick isn't on watch */
	
	/* Update the time of last change to item */
	anptr->lasttime = TStime();
	
	/* Send notifies out to everybody on the list in header */
	for (lp = anptr->watch; lp; lp = lp->next)
	{
		if (!awaynotify)
		{
			/* Most common: LOGON or LOGOFF */
			if (IsWebTV(lp->value.cptr))
				sendto_one(lp->value.cptr, ":IRC!IRC@%s PRIVMSG %s :%s (%s@%s) "
					" %s IRC",
					me.name, lp->value.cptr->name, cptr->name,
				    	(IsPerson(cptr) ? cptr->user->username : "<N/A>"),
					(IsPerson(cptr) ?
				    	(IsHidden(cptr) ? cptr->user->virthost : cptr->
				    	user->realhost) : "<N/A>"), reply == RPL_LOGON ? 
					"is now on" : "has left");
			else
				sendto_one(lp->value.cptr, rpl_str(reply), me.name,
				    lp->value.cptr->name, cptr->name,
				    (IsPerson(cptr) ? cptr->user->username : "<N/A>"),
				    (IsPerson(cptr) ?
				    (IsHidden(cptr) ? cptr->user->virthost : cptr->
				    user->realhost) : "<N/A>"), anptr->lasttime, cptr->info);
		} else
		{
			/* AWAY or UNAWAY */
			if (!lp->flags)
				continue; /* skip away/unaway notification for users not interested in them */

			if (reply == RPL_NOTAWAY)
				sendto_one(lp->value.cptr, rpl_str(reply), me.name,
				    lp->value.cptr->name, cptr->name,
				    (IsPerson(cptr) ? cptr->user->username : "<N/A>"),
				    (IsPerson(cptr) ?
				    (IsHidden(cptr) ? cptr->user->virthost : cptr->
				    user->realhost) : "<N/A>"), cptr->user->lastaway);
			else /* RPL_GONEAWAY / RPL_REAWAY */
				sendto_one(lp->value.cptr, rpl_str(reply), me.name,
				    lp->value.cptr->name, cptr->name,
				    (IsPerson(cptr) ? cptr->user->username : "<N/A>"),
				    (IsPerson(cptr) ?
				    (IsHidden(cptr) ? cptr->user->virthost : cptr->
				    user->realhost) : "<N/A>"), cptr->user->lastaway, cptr->user->away);
		}
	}
	
	return 0;
}

/*
 * hash_get_watch
 */
aWatch  *hash_get_watch(char *name)
{
	unsigned int   hashv;
	aWatch  *anptr;
	
	
	hashv = hash_nick_name(name)%WATCHHASHSIZE;
	
	if ((anptr = (aWatch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, name))
		 anptr = anptr->hnext;
	
	return anptr;
}

/*
 * del_from_watch_hash_table
 */
int   del_from_watch_hash_table(char *nick, aClient *cptr)
{
	unsigned int   hashv;
	aWatch  *anptr, *nlast = NULL;
	Link  *lp, *last = NULL;
	
	
	/* Get the bucket for this nick... */
	hashv = hash_nick_name(nick)%WATCHHASHSIZE;
	
	/* Find the right header, maintaining last-link pointer... */
	if ((anptr = (aWatch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, nick)) {
		  nlast = anptr;
		  anptr = anptr->hnext;
	  }
	if (!anptr)
	  return 0;   /* No such watch */
	
	/* Find this client from the list of notifies... with last-ptr. */
	if ((lp = anptr->watch))
	  while (lp && (lp->value.cptr != cptr)) {
		  last = lp;
		  lp = lp->next;
	  }
	if (!lp)
	  return 0;   /* No such client to watch */
	
	/* Fix the linked list under header, then remove the watch entry */
	if (!last)
	  anptr->watch = lp->next;
	else
	  last->next = lp->next;
	free_link(lp);
	
	/* Do the same regarding the links in client-record... */
	last = NULL;
	if ((lp = cptr->watch))
	  while (lp && (lp->value.wptr != anptr)) {
		  last = lp;
		  lp = lp->next;
	  }
	
	/*
	 * Give error on the odd case... probobly not even neccessary
	 * No error checking in ircd is unneccessary ;) -Cabal95
	 */
	if (!lp)
	  sendto_ops("WATCH debug error: del_from_watch_hash_table "
					 "found a watch entry with no client "
					 "counterpoint processing nick %s on client %p!",
					 nick, cptr->user);
	else {
		if (!last) /* First one matched */
		  cptr->watch = lp->next;
		else
		  last->next = lp->next;
		free_link(lp);
	}
	/* In case this header is now empty of notices, remove it */
	if (!anptr->watch) {
		if (!nlast)
		  watchTable[hashv] = anptr->hnext;
		else
		  nlast->hnext = anptr->hnext;
		MyFree(anptr);
	}
	
	/* Update count of notifies on nick */
	cptr->watches--;
	
	return 0;
}

/*
 * hash_del_watch_list
 */
int   hash_del_watch_list(aClient *cptr)
{
	unsigned int   hashv;
	aWatch  *anptr;
	Link  *np, *lp, *last;
	
	
	if (!(np = cptr->watch))
	  return 0;   /* Nothing to do */
	
	cptr->watch = NULL; /* Break the watch-list for client */
	while (np) {
		/* Find the watch-record from hash-table... */
		anptr = np->value.wptr;
		last = NULL;
		for (lp = anptr->watch; lp && (lp->value.cptr != cptr);
			  lp = lp->next)
		  last = lp;
		
		/* Not found, another "worst case" debug error */
		if (!lp)
		  sendto_ops("WATCH Debug error: hash_del_watch_list "
						 "found a WATCH entry with no table "
						 "counterpoint processing client %s!",
						 cptr->name);
		else {
			/* Fix the watch-list and remove entry */
			if (!last)
			  anptr->watch = lp->next;
			else
			  last->next = lp->next;
			free_link(lp);
			
			/*
			 * If this leaves a header without notifies,
			 * remove it. Need to find the last-pointer!
			 */
			if (!anptr->watch) {
				aWatch  *np2, *nl;
				
				hashv = hash_nick_name(anptr->nick)%WATCHHASHSIZE;
				
				nl = NULL;
				np2 = watchTable[hashv];
				while (np2 != anptr) {
					nl = np2;
					np2 = np2->hnext;
				}
				
				if (nl)
				  nl->hnext = anptr->hnext;
				else
				  watchTable[hashv] = anptr->hnext;
				MyFree(anptr);
			}
		}
		
		lp = np; /* Save last pointer processed */
		np = np->next; /* Jump to the next pointer */
		free_link(lp); /* Free the previous */
	}
	
	cptr->watches = 0;
	
	return 0;
}

/*
 * Throttling
 * -by Stskeeps
*/

#ifdef THROTTLING

struct	MODVAR ThrottlingBucket	*ThrottlingHash[THROTTLING_HASH_SIZE+1];

void	init_throttling_hash()
{
long v;
	bzero(ThrottlingHash, sizeof(ThrottlingHash));	
	if (!THROTTLING_PERIOD)
		v = 120;
	else
	{
		v = THROTTLING_PERIOD/2;
		if (v > 5)
			v = 5; /* accuracy, please */
	}
	EventAddEx(NULL, "bucketcleaning", v, 0, e_clean_out_throttling_buckets, NULL);
}

int	hash_throttling(struct IN_ADDR *in)
{
#ifdef INET6
	u_char *cp;
	unsigned int alpha, beta;
#endif
#ifndef INET6
	return ((unsigned int)in->s_addr % THROTTLING_HASH_SIZE); 
#else
	cp = (u_char *) &in->s6_addr;
	memcpy(&alpha, cp + 8, sizeof(alpha));
	memcpy(&beta, cp + 12, sizeof(beta));
	return ((alpha ^ beta) % THROTTLING_HASH_SIZE);
#endif
}

struct	ThrottlingBucket	*find_throttling_bucket(struct IN_ADDR *in)
{
	int			hash = 0;
	struct ThrottlingBucket *p;
	hash = hash_throttling(in);
	
	for (p = ThrottlingHash[hash]; p; p = p->next)
	{
		if (bcmp(in, &p->in, sizeof(struct IN_ADDR)) == 0)
			return(p);
	}
	return NULL;
}

EVENT(e_clean_out_throttling_buckets)
{
	struct ThrottlingBucket *n;
	int	i;
	struct ThrottlingBucket z = { NULL, NULL, {0}, 0, 0};
	static time_t t = 0;
		
	for (i = 0; i < THROTTLING_HASH_SIZE; i++)
		for (n = ThrottlingHash[i]; n; n = n->next)
			if ((TStime() - n->since) > (THROTTLING_PERIOD ? THROTTLING_PERIOD : 15))
			{
				z.next = (struct ThrottlingBucket *) DelListItem(n, ThrottlingHash[i]);
				MyFree(n);
				n = &z;
			}

	if (!t || (TStime() - t > 30))
	{
		extern char serveropts[];
		extern Module *Modules;
		char *p = serveropts + strlen(serveropts);
		Module *mi;
		t = TStime();
		if (!Hooks[17] && strchr(serveropts, 'm'))
		{ p = strchr(serveropts, 'm'); *p = '\0'; }
		if (!Hooks[18] && strchr(serveropts, 'M'))
		{ p = strchr(serveropts, 'M'); *p = '\0'; }
		if (!Hooks[49] && strchr(serveropts, 'R'))
		{ p = strchr(serveropts, 'R'); *p = '\0'; }
		if (Hooks[17] && !strchr(serveropts, 'm'))
			*p++ = 'm';
		if (Hooks[18] && !strchr(serveropts, 'M'))
			*p++ = 'M';
		if (Hooks[49] && !strchr(serveropts, 'R'))
			*p++ = 'R';
		*p = '\0';
		for (mi = Modules; mi; mi = mi->next)
			if (!(mi->options & MOD_OPT_OFFICIAL))
				tainted = 99;
	}

	return;
}

void	add_throttling_bucket(struct IN_ADDR *in)
{
	int	hash;
	struct	ThrottlingBucket	*n;
	
	n = MyMalloc(sizeof(struct ThrottlingBucket));	
	n->next = n->prev = NULL; 
	bcopy(in, &n->in, sizeof(struct IN_ADDR));
	n->since = TStime();
	n->count = 1;
	hash = hash_throttling(in);
	AddListItem(n, ThrottlingHash[hash]);
	return;
}

void	del_throttling_bucket(struct ThrottlingBucket *bucket)
{
	int	hash;
	hash = hash_throttling(&bucket->in);
	DelListItem(bucket, ThrottlingHash[hash]);
	MyFree(bucket);
	return;
}

/** Checks wether the user is connect-flooding.
 * @retval 0 Denied, throttled.
 * @retval 1 Allowed, but known in the list.
 * @retval 2 Allowed, not in list or is an exception.
 * @see add_connection()
 */
int	throttle_can_connect(aClient *sptr, struct IN_ADDR *in)
{
	struct ThrottlingBucket *b;

	if (!THROTTLING_PERIOD || !THROTTLING_COUNT)
		return 2;

	if (!(b = find_throttling_bucket(in)))
		return 1;
	else
	{
		if (Find_except(sptr, Inet_ia2p(in), CONF_EXCEPT_THROTTLE))
			return 2;
		if (b->count+1 > (THROTTLING_COUNT ? THROTTLING_COUNT : 3))
			return 0;
		b->count++;
		return 2;
	}
}

#endif
