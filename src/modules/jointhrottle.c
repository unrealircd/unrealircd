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

/* Default settings for set::anti-flood::join-flood block: */
#define JOINTHROTTLE_DEFAULT_COUNT 3
#define JOINTHROTTLE_DEFAULT_TIME 90

ModuleHeader MOD_HEADER(jointhrottle)
  = {
	"jointhrottle",
	"4.0",
	"Join flood protection (set::anti-flood::join-flood)",
	"3.2-b8-1",
	NULL,
    };

ModuleInfo *ModInfo = NULL;

ModDataInfo *jointhrottle_md; /* Module Data structure which we acquire */

struct {
	unsigned short num;
	unsigned short t;
} cfg;

typedef struct JFlood aJFlood;

struct JFlood {
	aJFlood *prev, *next;
	char chname[CHANNELLEN+1];
	time_t firstjoin;
	unsigned short numjoins;
};

/* Forward declarations */
int jointhrottle_config_test(ConfigFile *, ConfigEntry *, int, int *);
int jointhrottle_config_run(ConfigFile *, ConfigEntry *, int);
void jointhrottle_md_free(ModData *m);
int jointhrottle_can_join(aClient *sptr, aChannel *chptr, char *key, char *parv[]);
int jointhrottle_local_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[]);
static int isjthrottled(aClient *cptr, aChannel *chptr);
static void jointhrottle_increase_usercounter(aClient *cptr, aChannel *chptr);
EVENT(jointhrottle_cleanup_structs);
aJFlood *jointhrottle_addentry(aClient *cptr, aChannel *chptr);

MOD_TEST(jointhrottle)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, jointhrottle_config_test);
	return MOD_SUCCESS;
}

MOD_INIT(jointhrottle)
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
	mreq.type = MODDATATYPE_CLIENT;
	jointhrottle_md = ModDataAdd(modinfo->handle, mreq);
	if (!jointhrottle_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, jointhrottle_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_JOIN, 0, jointhrottle_can_join);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, jointhrottle_local_join);
	
	cfg.t = JOINTHROTTLE_DEFAULT_TIME;
	cfg.num = JOINTHROTTLE_DEFAULT_COUNT;
	return MOD_SUCCESS;
}

MOD_LOAD(jointhrottle)
{
	EventAddEx(ModInfo->handle, "jointhrottle_cleanup_structs", 60, 0, jointhrottle_cleanup_structs, NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(jointhrottle)
{
	return MOD_FAILED;
}

int jointhrottle_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	int cnt=0, period=0;

	if (type != CONFIG_SET_ANTI_FLOOD)
		return 0;

	if (strcmp(ce->ce_varname, "join-flood"))
		return 0; /* otherwise not interested */

	if (!ce->ce_vardata || !config_parse_flood(ce->ce_vardata, &cnt, &period) ||
		(cnt < 1) || (cnt > 255) || (period < 5))
	{
		config_error("%s:%i: set::anti-flood::join-flood. Syntax is '<count>:<period>' (eg 3:90), "
					 "count should be 1-255, period should be greater than 4",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int jointhrottle_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	int cnt=0, period=0;

	if (type != CONFIG_SET_ANTI_FLOOD)
		return 0;

	if (strcmp(ce->ce_varname, "join-flood"))
		return 0; /* otherwise not interested */
	
	config_parse_flood(ce->ce_vardata, &cnt, &period);
	
	cfg.t = period;
	cfg.num = cnt;
	
	return 0;
}

static int isjthrottled(aClient *cptr, aChannel *chptr)
{
aJFlood *e;
int num=cfg.num, t=cfg.t;

	if (!MyClient(cptr))
		return 0;

	/* Grab user<->chan entry.. */
	for (e = moddata_client(cptr, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->chname, chptr->chname))
			break;
	
	if (!e)
		return 0; /* Not present, so cannot be throttled */

	/* Ok... now the actual check:
	 * if ([timer valid] && [one more join would exceed num])
	 */
	if (((TStime() - e->firstjoin) < t) && (e->numjoins == num))
		return 1; /* Throttled */

	return 0;
}

static void jointhrottle_increase_usercounter(aClient *cptr, aChannel *chptr)
{
aJFlood *e;
int num=cfg.num, t=cfg.t;

	if (!MyClient(cptr))
		return;
		
	/* Grab user<->chan entry.. */
	for (e = moddata_client(cptr, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->chname, chptr->chname))
			break;
	
	if (!e)
	{
		/* Allocate one */
		e = jointhrottle_addentry(cptr, chptr);
		e->firstjoin = TStime();
		e->numjoins = 1;
	} else
	if ((TStime() - e->firstjoin) < t) /* still valid? */
	{
		e->numjoins++;
	} else {
		/* reset :p */
		e->firstjoin = TStime();
		e->numjoins = 1;
	}
}

int jointhrottle_can_join(aClient *sptr, aChannel *chptr, char *key, char *parv[])
{
	if (!ValidatePermissionsForPath("immune:jointhrottle",sptr,NULL,chptr,NULL) && isjthrottled(sptr, chptr))
		return ERR_TOOMANYJOINS;
	return 0;
}


int jointhrottle_local_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[])
{
	jointhrottle_increase_usercounter(cptr, chptr);
	return 0;
}

/** Adds a aJFlood entry to user & channel and returns entry.
 * NOTE: Does not check for already-existing-entry
 */
aJFlood *jointhrottle_addentry(aClient *cptr, aChannel *chptr)
{
aJFlood *e;

#ifdef DEBUGMODE
	if (!IsPerson(cptr))
		abort();

	for (e=moddata_client(cptr, jointhrottle_md).ptr; e; e=e->next)
		if (!strcasecmp(e->chname, chptr->chname))
			abort(); /* already exists -- should never happen */
#endif

	e = MyMallocEx(sizeof(aJFlood));
	strlcpy(e->chname, chptr->chname, sizeof(e->chname));

	/* Insert our new entry as (new) head */
	if (moddata_client(cptr, jointhrottle_md).ptr)
	{
		aJFlood *current_head = moddata_client(cptr, jointhrottle_md).ptr;
		current_head->prev = e;
		e->next = current_head;
	}
	moddata_client(cptr, jointhrottle_md).ptr = e;

	return e;
}

/** Regularly cleans up user/chan structs */
EVENT(jointhrottle_cleanup_structs)
{
aClient *acptr;
aChannel *chptr;
aJFlood *jf, *jf_next;
int t = cfg.t;
	
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (!MyClient(acptr))
			continue; /* only (local) persons.. */

		for (jf = moddata_client(acptr, jointhrottle_md).ptr; jf; jf = jf_next)
		{
			jf_next = jf->next;
			
			chptr = find_channel(jf->chname, NULL);
			/* Now check if chptr is valid and if a flood still applies, if so we skip,
			 * in all other cases we free the (no longer useful) entry.
			 */
			if (jf->firstjoin + t > TStime())
				continue; /* still valid entry */
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "jointhrottle_cleanup_structs(): freeing %s/%s (%ld[%ld], %d)",
				acptr->name, jf->chname, jf->firstjoin, (long)(TStime() - jf->firstjoin), t);
#endif
			if (moddata_client(acptr, jointhrottle_md).ptr == jf)
			{
				/* change head */
				moddata_client(acptr, jointhrottle_md).ptr = jf->next; /* could be set to NULL now */
				if (jf->next)
					jf->next->prev = NULL;
			} else {
				/* change non-head entries */
				jf->prev->next = jf->next; /* could be set to NULL now */
				if (jf->next)
					jf->next->prev = jf->prev;
			}
			MyFree(jf);
		}
	}
}

void jointhrottle_md_free(ModData *m)
{
aJFlood *j, *j_next;

	if (!m->ptr)
		return;

	for (j = m->ptr; j; j = j_next)
	{
		j_next = j->next;
		MyFree(j);
	}	

	m->ptr = NULL;
}
