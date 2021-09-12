/*
 * Jointhrottle (set::anti-flood::join-flood).
 * (C) Copyright 2005-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * This was PREVIOUSLY channel mode +j but has been moved to the
 * set::anti-flood::join-flood block instead since people rarely need
 * to tweak this per-channel and it's nice to have this on by default.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"jointhrottle",
	"5.0",
	"Join flood protection (set::anti-flood::join-flood)",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

ModuleInfo *ModInfo = NULL;

ModDataInfo *jointhrottle_md; /* Module Data structure which we acquire */

typedef struct JoinFlood JoinFlood;

struct JoinFlood {
	JoinFlood *prev, *next;
	char name[CHANNELLEN+1];
	time_t firstjoin;
	unsigned short numjoins;
};

/* Forward declarations */
void jointhrottle_md_free(ModData *m);
int jointhrottle_can_join(Client *client, Channel *channel, const char *key, char **errmsg);
int jointhrottle_local_join(Client *client, Channel *channel, MessageTag *mtags);
static int isjthrottled(Client *client, Channel *channel);
static void jointhrottle_increase_usercounter(Client *client, Channel *channel);
EVENT(jointhrottle_cleanup_structs);
JoinFlood *jointhrottle_addentry(Client *client, Channel *channel);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModInfo = modinfo;

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "jointhrottle";
	mreq.free = jointhrottle_md_free;
	mreq.serialize = NULL; /* not supported */
	mreq.unserialize = NULL; /* not supported */
	mreq.sync = 0;
	mreq.type = MODDATATYPE_LOCAL_CLIENT;
	jointhrottle_md = ModDataAdd(modinfo->handle, mreq);
	if (!jointhrottle_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, jointhrottle_can_join);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, jointhrottle_local_join);
	
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(ModInfo->handle, "jointhrottle_cleanup_structs", jointhrottle_cleanup_structs, NULL, 60000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_FAILED;
}

static int isjthrottled(Client *client, Channel *channel)
{
	JoinFlood *e;
	FloodSettings *settings = get_floodsettings_for_user(client, FLD_JOIN);

	if (!MyUser(client))
		return 0;

	/* Grab user<->chan entry.. */
	for (e = moddata_local_client(client, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->name, channel->name))
			break;
	
	if (!e)
		return 0; /* Not present, so cannot be throttled */

	/* Ok... now the actual check:
	 * if ([timer valid] && [one more join would exceed num])
	 */
	if (((TStime() - e->firstjoin) < settings->period[FLD_JOIN]) &&
	    (e->numjoins >= settings->limit[FLD_JOIN]))
		return 1; /* Throttled */

	return 0;
}

static void jointhrottle_increase_usercounter(Client *client, Channel *channel)
{
	JoinFlood *e;

	if (!MyUser(client))
		return;
		
	/* Grab user<->chan entry.. */
	for (e = moddata_local_client(client, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->name, channel->name))
			break;
	
	if (!e)
	{
		/* Allocate one */
		e = jointhrottle_addentry(client, channel);
		e->firstjoin = TStime();
		e->numjoins = 1;
	} else
	if ((TStime() - e->firstjoin) < iConf.floodsettings->period[FLD_JOIN]) /* still valid? */
	{
		e->numjoins++;
	} else {
		/* reset :p */
		e->firstjoin = TStime();
		e->numjoins = 1;
	}
}

int jointhrottle_can_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	if (!ValidatePermissionsForPath("immune:join-flood",client,NULL,channel,NULL) && isjthrottled(client, channel))
	{
		*errmsg = STR_ERR_TOOMANYJOINS;
		return ERR_TOOMANYJOINS;
	}
	return 0;
}


int jointhrottle_local_join(Client *client, Channel *channel, MessageTag *mtags)
{
	jointhrottle_increase_usercounter(client, channel);
	return 0;
}

/** Adds a JoinFlood entry to user & channel and returns entry.
 * NOTE: Does not check for already-existing-entry
 */
JoinFlood *jointhrottle_addentry(Client *client, Channel *channel)
{
	JoinFlood *e;

#ifdef DEBUGMODE
	if (!IsUser(client))
		abort();

	for (e=moddata_local_client(client, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->name, channel->name))
			abort(); /* already exists -- should never happen */
#endif

	e = safe_alloc(sizeof(JoinFlood));
	strlcpy(e->name, channel->name, sizeof(e->name));

	/* Insert our new entry as (new) head */
	if (moddata_local_client(client, jointhrottle_md).ptr)
	{
		JoinFlood *current_head = moddata_local_client(client, jointhrottle_md).ptr;
		current_head->prev = e;
		e->next = current_head;
	}
	moddata_local_client(client, jointhrottle_md).ptr = e;

	return e;
}

/** Regularly cleans up user/chan structs */
EVENT(jointhrottle_cleanup_structs)
{
	Client *client;
	JoinFlood *jf, *jf_next;
	
	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (!MyUser(client))
			continue; /* only (local) persons.. */

		for (jf = moddata_local_client(client, jointhrottle_md).ptr; jf; jf = jf_next)
		{
			jf_next = jf->next;
			
			if (jf->firstjoin + iConf.floodsettings->period[FLD_JOIN] > TStime())
				continue; /* still valid entry */
			if (moddata_local_client(client, jointhrottle_md).ptr == jf)
			{
				/* change head */
				moddata_local_client(client, jointhrottle_md).ptr = jf->next; /* could be set to NULL now */
				if (jf->next)
					jf->next->prev = NULL;
			} else {
				/* change non-head entries */
				jf->prev->next = jf->next; /* could be set to NULL now */
				if (jf->next)
					jf->next->prev = jf->prev;
			}
			safe_free(jf);
		}
	}
}

void jointhrottle_md_free(ModData *m)
{
	JoinFlood *j, *j_next;

	if (!m->ptr)
		return;

	for (j = m->ptr; j; j = j_next)
	{
		j_next = j->next;
		safe_free(j);
	}	

	m->ptr = NULL;
}
