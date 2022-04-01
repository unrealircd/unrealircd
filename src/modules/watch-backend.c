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

static Watch **watchTable = NULL;
static char *siphashkey_watch = NULL;

void dummy_free(ModData *md);
void watch_free(ModData *md);

int watch_backend_user_quit(Client *client, MessageTag *mtags, const char *comment);
int _watch_add(char *nick, Client *client, int flags);
int _watch_check(Client *client, int event, int (*watch_notify)(Client *client, Watch *watch, Link *lp, int event));
Watch *_watch_get(char *nick);
int _watch_del(char *nick, Client *client, int flags);
int _watch_del_list(Client *client, int flags);
uint64_t hash_watch_nick_name(const char *name);

ModuleHeader MOD_HEADER
= {
	"watch-backend",
	"6.0.3",
	"backend for /WATCH",
	"UnrealIRCd Team",
	"unrealircd-6",
};

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAdd(modinfo->handle, EFUNC_WATCH_ADD, _watch_add);
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_DEL, _watch_del);
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_DEL_LIST, _watch_del_list);
	EfunctionAddPVoid(modinfo->handle, EFUNC_WATCH_GET, TO_PVOIDFUNC(_watch_get));
	EfunctionAdd(modinfo->handle, EFUNC_WATCH_CHECK, _watch_check);
	return MOD_SUCCESS;
}

void watch_generic_free(ModData *m)
{
	safe_free(m->ptr);
}

MOD_INIT()
{	
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM_RELOADABLE, 1); /* or do a complex memory freeing algorithm instead */

	LoadPersistentPointer(modinfo, siphashkey_watch, watch_generic_free);
	if (siphashkey_watch == NULL)
	{
		siphashkey_watch = safe_alloc(SIPHASH_KEY_LENGTH);
		siphash_generate_key(siphashkey_watch);
	}
	LoadPersistentPointer(modinfo, watchTable, watch_generic_free);
	if (watchTable == NULL)
		watchTable = safe_alloc(sizeof(Watch) * WATCH_HASH_TABLE_SIZE);

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

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, watch_backend_user_quit);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, siphashkey_watch);
	SavePersistentPointer(modinfo, watchTable);
	return MOD_SUCCESS;
}

void dummy_free(ModData *md)
{
}

void watch_free(ModData *md)
{
	/* it should have been never requested to free as the module is PERM */
	if (md)
	{
		unreal_log(ULOG_ERROR, "watch-backend", "BUG_WATCH_FREE_MEMORY_LEAK", NULL,
		           "[BUG] watchList moddata was not freed -- memory leak!");
	}
}

int watch_backend_user_quit(Client *client, MessageTag *mtags, const char *comment)
{
	/* Clean out list and watch structures -Donwulff */
	watch_del_list(client, 0);
	return 0;
}

/*
 * _watch_add
 */
int _watch_add(char *nick, Client *client, int flags)
{
	unsigned int hashv;
	Watch *watch;
	Link *lp;
	
	
	/* Get the right bucket... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right nick (header) in the bucket, or NULL... */
	if ((watch = watchTable[hashv]))
		while (watch && mycmp(watch->nick, nick))
		 watch = watch->hnext;
	
	/* If found NULL (no header for this nick), make one... */
	if (!watch) {
		watch = safe_alloc(sizeof(Watch)+strlen(nick));
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
 *	_watch_check
 */
int _watch_check(Client *client, int event, int (*watch_notify)(Client *client, Watch *watch, Link *lp, int event))
{
	unsigned int hashv;
	Watch *watch;
	Link *lp;

	/* Get us the right bucket */
	hashv = hash_watch_nick_name(client->name);
	
	/* Find the right header in this bucket */
	if ((watch = watchTable[hashv]))
		while (watch && mycmp(watch->nick, client->name))
		 watch = watch->hnext;
	if (!watch)
		return 0;	 /* This nick isn't on watch */
	
	/* Update the time of last change to item */
	watch->lasttime = TStime();
	
	/* Send notifies out to everybody on the list in header */
	for (lp = watch->watch; lp; lp = lp->next)
	{
		watch_notify(client, watch, lp, event);
	}
	
	return 0;
}

/*
 * _watch_get
 */
Watch *_watch_get(char *nick)
{
	unsigned int hashv;
	Watch *watch;
	
	hashv = hash_watch_nick_name(nick);
	
	if ((watch = watchTable[hashv]))
		while (watch && mycmp(watch->nick, nick))
		 watch = watch->hnext;
	
	return watch;
}

/*
 * _watch_del
 */
int _watch_del(char *nick, Client *client, int flags)
{
	unsigned int hashv;
	Watch **watch, *wprev;
	Link **lp, *prev;

	/* Get the bucket for this nick... */
	hashv = hash_watch_nick_name(nick);
	
	/* Find the right header, maintaining last-link pointer... */
	watch = (Watch **)&watchTable[hashv];
	while (*watch && mycmp((*watch)->nick, nick))
		watch = &(*watch)->hnext;
	if (!*watch)
		return 0;	 /* No such watch */
	
	/* Find this client from the list of notifies... with last-ptr. */
	lp = &(*watch)->watch;
	while (*lp)
	{
		if ((*lp)->value.client == client && ((*lp)->flags & flags) == flags)
			break;
		lp = &(*lp)->next;
	}
	if (!*lp)
		return 0;	 /* No such client to watch */
	
	/* Fix the linked list under header, then remove the watch entry */
	prev = *lp;
	*lp = prev->next;
	free_link(prev);
	
	/* Do the same regarding the links in client-record... */
	lp = (Link **)&WATCH(client);
	while (*lp && ((*lp)->value.wptr != *watch))
		lp = &(*lp)->next;
	
	/*
	 * Give error on the odd case... probobly not even neccessary
	 * No error checking in ircd is unneccessary ;) -Cabal95
	 */
	if (!*lp)
	{
		unreal_log(ULOG_WARNING, "watch", "BUG_WATCH_DEL", client,
		           "[BUG] watch_del found a watch entry with no client counterpoint, "
		           "while processing nick $nick on client $client.details",
		           log_data_string("nick", nick));
	} else {
		prev = *lp;
		*lp = prev->next;
		free_link(prev);
	}
	/* In case this header is now empty of notices, remove it */
	if (!(*watch)->watch) {
		wprev = *watch;
		*watch = wprev->hnext;
		safe_free(wprev);
	}
	
	/* Update count of notifies on nick */
	WATCHES(client)--;
	
	return 0;
}

/*
 * _watch_del_list
 */
int _watch_del_list(Client *client, int flags)
{
	unsigned int hashv;
	Watch *watch;
	Link **np, **lp, *prev;
	
	np = (Link **)&WATCH(client);
	
	while (*np) {
		if (((*np)->flags & flags) != flags)
		{
			/* this entry is not fitting requested flags */
			np = &(*np)->next;
			continue;
		}
		
		WATCHES(client)--;
		
		/* Find the watch-record from hash-table... */
		watch = (*np)->value.wptr;
		lp = &(watch->watch);
		while (*lp && ((*lp)->value.client != client))
			lp = &(*lp)->next;

		/* Not found, another "worst case" debug error */
		if (!*lp)
		{
			unreal_log(ULOG_WARNING, "watch", "BUG_WATCH_DEL_LIST", client,
				   "[BUG] watch_del_list found a watch entry with no table counterpoint, "
				   "while processing client $client.details");
		} else {
			/* Fix the watch-list and remove entry */
			Link *prev = *lp;
			*lp = prev->next;
			free_link(prev);
			
			/*
			 * If this leaves a header without notifies,
			 * remove it. Need to find the last-pointer!
			 */
			if (!watch->watch) {
				Watch **np2, *wprev;
				
				hashv = hash_watch_nick_name(watch->nick);
				
				np2 = &watchTable[hashv];
				while (*np2 && *np2 != watch)
					np2 = &(*np2)->hnext;

				*np2 = watch->hnext;

				safe_free(watch);
			}
		}
		
		prev = *np; /* Save last pointer processed */
		*np = prev->next; /* Jump to the next pointer */
		free_link(prev); /* Free the previous */
	}
	
	if (!flags)
		WATCHES(client) = 0;
	
	return 0;
}

uint64_t hash_watch_nick_name(const char *name)
{
	return siphash_nocase(name, siphashkey_watch) % WATCH_HASH_TABLE_SIZE;
}

