/*
 *   IRC - Internet Relay Chat, src/modules/watch.c
 *   (C) 2005 The UnrealIRCd Team
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

#define MSG_WATCH 	"WATCH"
#define WATCH_HASH_TABLE_SIZE 32768

#define WATCHES(client) (moddata_local_client(client, watchCounterMD).i)
#define WATCH(client) (moddata_local_client(client, watchListMD).ptr)

ModDataInfo *watchCounterMD;
ModDataInfo *watchListMD;
static Watch *watchTable[WATCH_HASH_TABLE_SIZE];
static int watch_initialized = 0;
static char siphashkey_watch[SIPHASH_KEY_LENGTH];

CMD_FUNC(cmd_watch);
void dummy_free(ModData *md);
void watch_free(ModData *md);
int watch_user_quit(Client *client, MessageTag *mtags, char *comment);
int watch_away(Client *client, MessageTag *mtags, char *reason, int already_as_away);
int watch_nickchange(Client *client, MessageTag *mtags, char *newnick);
int watch_post_nickchange(Client *client, MessageTag *mtags);
int watch_user_connect(Client *client);

int add_to_watch_hash_table(char *nick, Client *client, int awaynotify);
int hash_check_watch(Client *client, int reply);
Watch  *hash_get_watch(char *nick);
int del_from_watch_hash_table(char *nick, Client *client);
int   hash_del_watch_list(Client *client);
uint64_t hash_watch_nick_name(const char *name);
void  count_watch_memory(int *count, u_long *memory);

ModuleHeader MOD_HEADER
  = {
	"watch",
	"5.0",
	"command /watch", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{	
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	if (!watch_initialized)
	{
		memset(watchTable, 0, sizeof(watchTable));
		siphash_generate_key(siphashkey_watch);
		watch_initialized = 1;
	}
	
	CommandAdd(modinfo->handle, MSG_WATCH, cmd_watch, 1, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, watch_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, watch_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_AWAY, 0, watch_away);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, watch_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, watch_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_LOCAL_NICKCHANGE, 0, watch_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_REMOTE_NICKCHANGE, 0, watch_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, watch_user_connect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, watch_user_connect);

	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.name = "watchCount",
	mreq.free = dummy_free;
	watchCounterMD = ModDataAdd(modinfo->handle, mreq);
	if (!watchCounterMD)
	{
		config_error("[%s] Failed to request user watchCount moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	memset(&mreq, 0 , sizeof(mreq));
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	mreq.name = "watchList",
	mreq.free = watch_free;
	watchListMD = ModDataAdd(modinfo->handle, mreq);
	if (!watchListMD)
	{
		config_error("[%s] Failed to request user watchList moddata: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void dummy_free(ModData *md)
{
}

void watch_free(ModData *md)
{
}

/*
 * RPL_NOWON	- Online at the moment (Successfully added to WATCH-list)
 * RPL_NOWOFF	- Offline at the moement (Successfully added to WATCH-list)
 * RPL_WATCHOFF	- Successfully removed from WATCH-list.
 * ERR_TOOMANYWATCH - Take a guess :>  Too many WATCH entries.
 */
static void show_watch(Client *client, char *name, int rpl1, int rpl2, int awaynotify)
{
	Client *target;

	if ((target = find_person(name, NULL)))
	{
		if (awaynotify && target->user->away)
		{
			sendnumeric(client, RPL_NOWISAWAY,
			    target->name, target->user->username,
			    IsHidden(target) ? target->user->virthost : target->user->
			    realhost, target->user->lastaway);
			return;
		}
		
		sendnumeric(client, rpl1,
		    target->name, target->user->username,
		    IsHidden(target) ? target->user->virthost : target->user->
		    realhost, target->lastnick);
	}
	else
	{
		sendnumeric(client, rpl2, name, "*", "*", 0L);
	}
}

static char buf[BUFSIZE];

/*
 * cmd_watch
 */
CMD_FUNC(cmd_watch)
{
	Client *target;
	char *s, **pav = parv, *user;
	char *p = NULL, *def = "l";
	int awaynotify = 0;
	int did_l=0, did_s=0;

	if (!MyUser(client))
		return;

	if (parc < 2)
	{
		/*
		 * Default to 'l' - list who's currently online
		 */
		parc = 2;
		parv[1] = def;
	}

	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
	{
		if ((user = strchr(s, '!')))
			*user++ = '\0';	/* Not used */
			
		if (!strcmp(s, "A") && WATCH_AWAY_NOTIFICATION)
			awaynotify = 1;

		/*
		 * Prefix of "+", they want to add a name to their WATCH
		 * list.
		 */
		if (*s == '+')
		{
			if (!*(s+1))
				continue;
			if (do_nick_name(s + 1))
			{
				if (WATCHES(client) >= MAXWATCH)
				{
					sendnumeric(client, ERR_TOOMANYWATCH, s + 1);
					continue;
				}

				add_to_watch_hash_table(s + 1, client, awaynotify);
			}

			show_watch(client, s + 1, RPL_NOWON, RPL_NOWOFF, awaynotify);
			continue;
		}

		/*
		 * Prefix of "-", coward wants to remove somebody from their
		 * WATCH list.  So do it. :-)
		 */
		if (*s == '-')
		{
			if (!*(s+1))
				continue;
			del_from_watch_hash_table(s + 1, client);
			show_watch(client, s + 1, RPL_WATCHOFF, RPL_WATCHOFF, 0);

			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			hash_del_watch_list(client);

			continue;
		}

		/*
		 * Now comes the fun stuff, "S" or "s" returns a status report of
		 * their WATCH list.  I imagine this could be CPU intensive if its
		 * done alot, perhaps an auto-lag on this?
		 */
		if ((*s == 'S' || *s == 's') && !did_s)
		{
			Link *lp;
			Watch *anptr;
			int  count = 0;
			
			did_s = 1;
			
			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on.
			 */
			anptr = hash_get_watch(client->name);
			if (anptr)
				for (lp = anptr->watch, count = 1;
				    (lp = lp->next); count++)
					;
			sendnumeric(client, RPL_WATCHSTAT, WATCHES(client), count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			if ((lp = WATCH(client)) == NULL)
			{
				sendnumeric(client, RPL_ENDOFWATCHLIST, *s);
				continue;
			}
			*buf = '\0';
			strlcpy(buf, lp->value.wptr->nick, sizeof buf);
			count =
			    strlen(client->name) + strlen(me.name) + 10 +
			    strlen(buf);
			while ((lp = lp->next))
			{
				if (count + strlen(lp->value.wptr->nick) + 1 >
				    BUFSIZE - 2)
				{
					sendnumeric(client, RPL_WATCHLIST, buf);
					*buf = '\0';
					count = strlen(client->name) + strlen(me.name) + 10;
				}
				strcat(buf, " ");
				strcat(buf, lp->value.wptr->nick);
				count += (strlen(lp->value.wptr->nick) + 1);
			}
			sendnumeric(client, RPL_WATCHLIST, buf);

			sendnumeric(client, RPL_ENDOFWATCHLIST, *s);
			continue;
		}

		/*
		 * Well that was fun, NOT.  Now they want a list of everybody in
		 * their WATCH list AND if they are online or offline? Sheesh,
		 * greedy arn't we?
		 */
		if ((*s == 'L' || *s == 'l') && !did_l)
		{
			Link *lp = WATCH(client);

			did_l = 1;

			while (lp)
			{
				if ((target = find_person(lp->value.wptr->nick, NULL)))
				{
					sendnumeric(client, RPL_NOWON, target->name,
					    target->user->username,
					    IsHidden(target) ? target->user->
					    virthost : target->user->realhost,
					    target->lastnick);
				}
				/*
				 * But actually, only show them offline if its a capital
				 * 'L' (full list wanted).
				 */
				else if (isupper(*s))
					sendnumeric(client, RPL_NOWOFF,
					    lp->value.wptr->nick, "*", "*",
					    lp->value.wptr->lasttime);
				lp = lp->next;
			}

			sendnumeric(client, RPL_ENDOFWATCHLIST, *s);

			continue;
		}

		/*
		 * Hmm.. unknown prefix character.. Ignore it. :-)
		 */
	}
}

int watch_user_quit(Client *client, MessageTag *mtags, char *comment)
{
	if (IsUser(client))
		hash_check_watch(client, RPL_LOGOFF);

	if (MyConnect(client))
		/* Clean out list and watch structures -Donwulff */
		hash_del_watch_list(client);

	return 0;
}

int watch_away(Client *client, MessageTag *mtags, char *reason, int already_as_away)
{
	if (reason)
		hash_check_watch(client, already_as_away ? RPL_REAWAY : RPL_GONEAWAY);
	else
		hash_check_watch(client, RPL_NOTAWAY);

	return 0;
}

int watch_nickchange(Client *client, MessageTag *mtags, char *newnick)
{
	hash_check_watch(client, RPL_LOGOFF);

	return 0;
}

int watch_post_nickchange(Client *client, MessageTag *mtags)
{
	hash_check_watch(client, RPL_LOGON);

	return 0;
}

int watch_user_connect(Client *client)
{
	hash_check_watch(client, RPL_LOGON);

	return 0;
}

/*
 * add_to_watch_hash_table
 */
int add_to_watch_hash_table(char *nick, Client *client, int awaynotify)
{
	unsigned int hashv;
	Watch  *anptr;
	Link  *lp;
	
	
	/* Get the right bucket... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right nick (header) in the bucket, or NULL... */
	if ((anptr = (Watch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, nick))
		 anptr = anptr->hnext;
	
	/* If found NULL (no header for this nick), make one... */
	if (!anptr) {
		anptr = (Watch *)safe_alloc(sizeof(Watch)+strlen(nick));
		anptr->lasttime = timeofday;
		strcpy(anptr->nick, nick);
		
		anptr->watch = NULL;
		
		anptr->hnext = watchTable[hashv];
		watchTable[hashv] = anptr;
	}
	/* Is this client already on the watch-list? */
	if ((lp = anptr->watch))
	  while (lp && (lp->value.client != client))
		 lp = lp->next;
	
	/* No it isn't, so add it in the bucket and client addint it */
	if (!lp) {
		lp = anptr->watch;
		anptr->watch = make_link();
		anptr->watch->value.client = client;
		anptr->watch->flags = awaynotify;
		anptr->watch->next = lp;
		
		lp = make_link();
		lp->next = WATCH(client);
		lp->value.wptr = anptr;
		lp->flags = awaynotify;
		WATCH(client) = lp;
		WATCHES(client)++;
	}
	
	return 0;
}

/*
 *  hash_check_watch
 */
int hash_check_watch(Client *client, int reply)
{
	unsigned int hashv;
	Watch  *anptr;
	Link  *lp;
	int awaynotify = 0;
	
	if ((reply == RPL_GONEAWAY) || (reply == RPL_NOTAWAY) || (reply == RPL_REAWAY))
		awaynotify = 1;

	/* Get us the right bucket */
	hashv = hash_watch_nick_name(client->name);
	
	/* Find the right header in this bucket */
	if ((anptr = (Watch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, client->name))
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
			sendnumeric(lp->value.client, reply,
			    client->name,
			    (IsUser(client) ? client->user->username : "<N/A>"),
			    (IsUser(client) ?
			    (IsHidden(client) ? client->user->virthost : client->
			    user->realhost) : "<N/A>"), anptr->lasttime, client->info);
		}
		else
		{
			/* AWAY or UNAWAY */
			if (!lp->flags)
				continue; /* skip away/unaway notification for users not interested in them */

			if (reply == RPL_NOTAWAY)
				sendnumeric(lp->value.client, reply,
				    client->name,
				    (IsUser(client) ? client->user->username : "<N/A>"),
				    (IsUser(client) ?
				    (IsHidden(client) ? client->user->virthost : client->
				    user->realhost) : "<N/A>"), client->user->lastaway);
			else /* RPL_GONEAWAY / RPL_REAWAY */
				sendnumeric(lp->value.client, reply,
				    client->name,
				    (IsUser(client) ? client->user->username : "<N/A>"),
				    (IsUser(client) ?
				    (IsHidden(client) ? client->user->virthost : client->
				    user->realhost) : "<N/A>"), client->user->lastaway, client->user->away);
		}
	}
	
	return 0;
}

/*
 * hash_get_watch
 */
Watch  *hash_get_watch(char *nick)
{
	unsigned int hashv;
	Watch  *anptr;
	
	hashv = hash_watch_nick_name(nick);
	
	if ((anptr = (Watch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, nick))
		 anptr = anptr->hnext;
	
	return anptr;
}

/*
 * del_from_watch_hash_table
 */
int del_from_watch_hash_table(char *nick, Client *client)
{
	unsigned int hashv;
	Watch  *anptr, *nlast = NULL;
	Link  *lp, *last = NULL;

	/* Get the bucket for this nick... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right header, maintaining last-link pointer... */
	if ((anptr = (Watch *)watchTable[hashv]))
	  while (anptr && mycmp(anptr->nick, nick)) {
		  nlast = anptr;
		  anptr = anptr->hnext;
	  }
	if (!anptr)
	  return 0;   /* No such watch */
	
	/* Find this client from the list of notifies... with last-ptr. */
	if ((lp = anptr->watch))
	  while (lp && (lp->value.client != client)) {
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
	if ((lp = WATCH(client)))
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
					 nick, client->user);
	else {
		if (!last) /* First one matched */
		  WATCH(client) = lp->next;
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
		safe_free(anptr);
	}
	
	/* Update count of notifies on nick */
	WATCHES(client)--;
	
	return 0;
}

/*
 * hash_del_watch_list
 */
int   hash_del_watch_list(Client *client)
{
	unsigned int   hashv;
	Watch  *anptr;
	Link  *np, *lp, *last;
	
	
	if (!(np = WATCH(client)))
	  return 0;   /* Nothing to do */
	
	WATCH(client) = NULL; /* Break the watch-list for client */
	while (np) {
		/* Find the watch-record from hash-table... */
		anptr = np->value.wptr;
		last = NULL;
		for (lp = anptr->watch; lp && (lp->value.client != client);
			  lp = lp->next)
		  last = lp;
		
		/* Not found, another "worst case" debug error */
		if (!lp)
		  sendto_ops("WATCH Debug error: hash_del_watch_list "
						 "found a WATCH entry with no table "
						 "counterpoint processing client %s!",
						 client->name);
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
				Watch  *np2, *nl;
				
				hashv = hash_watch_nick_name(anptr->nick);
				
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
				safe_free(anptr);
			}
		}
		
		lp = np; /* Save last pointer processed */
		np = np->next; /* Jump to the next pointer */
		free_link(lp); /* Free the previous */
	}
	
	WATCHES(client) = 0;
	
	return 0;
}

uint64_t hash_watch_nick_name(const char *name)
{
	return siphash_nocase(name, siphashkey_watch) % WATCH_HASH_TABLE_SIZE;
}

void  count_watch_memory(int *count, u_long *memory)
{
	int i = WATCH_HASH_TABLE_SIZE;
	Watch *anptr;

	while (i--)
	{
		anptr = watchTable[i];
		while (anptr)
		{
			(*count)++;
			(*memory) += sizeof(Watch)+strlen(anptr->nick);
			anptr = anptr->hnext;
		}
	}
}

