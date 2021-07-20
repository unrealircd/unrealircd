/*
 *   IRC - Internet Relay Chat, src/modules/watch-backend.c
 *   (C) 2021 The UnrealIRCd Team
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

#define WATCH_HASH_TABLE_SIZE 32768

#define WATCHES(client) (moddata_local_client(client, watchCounterMD).i)
#define WATCH(client) (moddata_local_client(client, watchListMD).ptr)

ModDataInfo *watchCounterMD;
ModDataInfo *watchListMD;
static Watch *watchTable[WATCH_HASH_TABLE_SIZE];
static int watch_initialized = 0;
static char siphashkey_watch[SIPHASH_KEY_LENGTH];

void dummy_free(ModData *md);
void watch_free(ModData *md);

int add_to_watch_hash_table(char *nick, Client *client, int flags);
int hash_check_watch(Client *client, int reply);
Watch *hash_get_watch(char *nick);
int del_from_watch_hash_table(char *nick, Client *client);
int hash_del_watch_list(Client *client);
uint64_t hash_watch_nick_name(const char *name);

ModuleHeader MOD_HEADER
  = {
	"watch-backend",
	"5.0",
	"backend for /watch", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAdd(modinfo->handle, EFUNC_WATCH_ADD, add_to_watch_hash_table);
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_DEL, del_from_watch_hash_table);
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_DEL_LIST, hash_del_watch_list);
	EfunctionAddPVoid(modinfo->handle, EFUNC_WATCH_GET, TO_PVOIDFUNC(hash_get_watch));
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_CHECK, hash_check_watch);
	return MOD_SUCCESS;
}

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
#warning do proper free
}

/*
 * add_to_watch_hash_table
 */
int add_to_watch_hash_table(char *nick, Client *client, int flags)
{
	unsigned int hashv;
	Watch  *watch;
	Link  *lp;
	
	
	/* Get the right bucket... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right nick (header) in the bucket, or NULL... */
	if ((watch = (Watch *)watchTable[hashv]))
	  while (watch && mycmp(watch->nick, nick))
		 watch = watch->hnext;
	
	/* If found NULL (no header for this nick), make one... */
	if (!watch) {
		watch = (Watch *)safe_alloc(sizeof(Watch)+strlen(nick));
		watch->lasttime = timeofday;
		strcpy(watch->nick, nick);
		
		watch->watch = NULL;
		
		watch->hnext = watchTable[hashv];
		watchTable[hashv] = watch;
	}
	/* Is this client already on the watch-list? */
	if ((lp = watch->watch))
	  while (lp && (lp->value.client != client))
		 lp = lp->next;
	
	/* No it isn't, so add it in the bucket and client addint it */
	if (!lp) {
		lp = watch->watch;
		watch->watch = make_link();
		watch->watch->value.client = client;
		watch->watch->flags = flags;
		watch->watch->next = lp;
		
		lp = make_link();
		lp->next = WATCH(client);
		lp->value.wptr = watch;
		lp->flags = flags;
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
	Watch  *watch;
	Link  *lp;

	/* Get us the right bucket */
	hashv = hash_watch_nick_name(client->name);
	
	/* Find the right header in this bucket */
	if ((watch = (Watch *)watchTable[hashv]))
	  while (watch && mycmp(watch->nick, client->name))
		 watch = watch->hnext;
	if (!watch)
	  return 0;   /* This nick isn't on watch */
	
	/* Update the time of last change to item */
	watch->lasttime = TStime();
	
	/* Send notifies out to everybody on the list in header */
	for (lp = watch->watch; lp; lp = lp->next)
	{
		RunHook4(HOOKTYPE_WATCH_NOTIFICATION, client, watch, lp, reply);
	}
	
	return 0;
}

/*
 * hash_get_watch
 */
Watch *hash_get_watch(char *nick)
{
	unsigned int hashv;
	Watch  *watch;
	
	hashv = hash_watch_nick_name(nick);
	
	if ((watch = (Watch *)watchTable[hashv]))
	  while (watch && mycmp(watch->nick, nick))
		 watch = watch->hnext;
	
	return watch;
}

/*
 * del_from_watch_hash_table
 */
int del_from_watch_hash_table(char *nick, Client *client)
{
	unsigned int hashv;
	Watch  *watch, *nlast = NULL;
	Link  *lp, *last = NULL;

	/* Get the bucket for this nick... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right header, maintaining last-link pointer... */
	if ((watch = (Watch *)watchTable[hashv]))
	  while (watch && mycmp(watch->nick, nick)) {
		  nlast = watch;
		  watch = watch->hnext;
	  }
	if (!watch)
	  return 0;   /* No such watch */
	
	/* Find this client from the list of notifies... with last-ptr. */
	if ((lp = watch->watch))
	  while (lp && (lp->value.client != client)) {
		  last = lp;
		  lp = lp->next;
	  }
	if (!lp)
	  return 0;   /* No such client to watch */
	
	/* Fix the linked list under header, then remove the watch entry */
	if (!last)
	  watch->watch = lp->next;
	else
	  last->next = lp->next;
	free_link(lp);
	
	/* Do the same regarding the links in client-record... */
	last = NULL;
	if ((lp = WATCH(client)))
	  while (lp && (lp->value.wptr != watch)) {
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
	if (!watch->watch) {
		if (!nlast)
		  watchTable[hashv] = watch->hnext;
		else
		  nlast->hnext = watch->hnext;
		safe_free(watch);
	}
	
	/* Update count of notifies on nick */
	WATCHES(client)--;
	
	return 0;
}

/*
 * hash_del_watch_list
 */
int hash_del_watch_list(Client *client)
{
	unsigned int   hashv;
	Watch  *watch;
	Link  *np, *lp, *last;
	
	
	if (!(np = WATCH(client)))
	  return 0;   /* Nothing to do */
	
	WATCH(client) = NULL; /* Break the watch-list for client */
	while (np) {
		/* Find the watch-record from hash-table... */
		watch = np->value.wptr;
		last = NULL;
		for (lp = watch->watch; lp && (lp->value.client != client);
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
			  watch->watch = lp->next;
			else
			  last->next = lp->next;
			free_link(lp);
			
			/*
			 * If this leaves a header without notifies,
			 * remove it. Need to find the last-pointer!
			 */
			if (!watch->watch) {
				Watch  *np2, *nl;
				
				hashv = hash_watch_nick_name(watch->nick);
				
				nl = NULL;
				np2 = watchTable[hashv];
				while (np2 != watch) {
					nl = np2;
					np2 = np2->hnext;
				}
				
				if (nl)
				  nl->hnext = watch->hnext;
				else
				  watchTable[hashv] = watch->hnext;
				safe_free(watch);
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

