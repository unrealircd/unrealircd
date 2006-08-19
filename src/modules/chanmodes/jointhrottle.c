/*
 * Channel Mode +j.
 * (C) Copyright 2005-2006 The UnrealIRCd team.
 */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif


ModuleHeader MOD_HEADER(chmode_j)
  = {
	"chmode_j",
	"$Id$",
	"Channel Mode +j",
	"3.2-b8-1",
	NULL,
    };

ModuleInfo *ModInfo = NULL;

Cmode_t EXTMODE_JOINTHROTTLE = 0L;

int cmodej_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what);
void *cmodej_put_param(void *r_in, char *param);
char *cmodej_get_param(void *r_in);
char *cmodej_conv_param(char *param_in, aClient *sptr);
void cmodej_free_param();
void *cmodej_dup_struct(void *r_in);
int cmodej_sjoin_check(aChannel *chptr, void *ourx, void *theirx);
int chmode_j_can_join(aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[]);
int chmode_j_local_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[]);
static int isjthrottled(aClient *cptr, aChannel *chptr);
static void cmodej_increase_usercounter(aClient *cptr, aChannel *chptr);
EVENT(cmodej_cleanup_structs);
int cmodej_cleanup_user2(aClient *sptr);
int cmodej_channel_destroy(aChannel *chptr);

DLLFUNC int MOD_INIT(chmode_j)(ModuleInfo *modinfo)
{
	CmodeInfo req;
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
	memset(&req, 0, sizeof(req));
	ModInfo = modinfo;

	req.paracount = 1;
	req.is_ok = cmodej_is_ok;
	req.flag = 'j';
	req.put_param = cmodej_put_param;
	req.get_param = cmodej_get_param;
	req.conv_param = cmodej_conv_param;
	req.free_param = cmodej_free_param;
	req.dup_struct = cmodej_dup_struct;
	req.sjoin_check = cmodej_sjoin_check;
	CmodeAdd(modinfo->handle, req, &EXTMODE_JOINTHROTTLE);

	HookAddEx(modinfo->handle, HOOKTYPE_CAN_JOIN, chmode_j_can_join);
	HookAddEx(modinfo->handle, HOOKTYPE_LOCAL_JOIN, chmode_j_local_join);
	HookAddEx(modinfo->handle, HOOKTYPE_CLEANUP_USER2, cmodej_cleanup_user2);
	HookAddEx(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, cmodej_channel_destroy);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(chmode_j)(int module_load)
{
	EventAddEx(ModInfo->handle, "cmodej_cleanup_structs", 60, 0, cmodej_cleanup_structs, NULL);
	return MOD_SUCCESS;
}


DLLFUNC int MOD_UNLOAD(chmode_j)(int module_unload)
{
	sendto_realops("Mod_Unload was called??? Arghhhhhh..");
	return MOD_FAILED;
}

int cmodej_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chan_op(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendto_one(sptr, err_str(ERR_NOTFORHALFOPS), me.name, sptr->name, 'j');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		/* Check parameter.. syntax should be X:Y, X should be 1-255, Y should be 1-999 */
		char buf[32], *p;
		int num, t, fail = 0;
		
		strlcpy(buf, para, sizeof(buf));
		p = strchr(buf, ':');
		if (!p)
		{
			fail = 1;
		} else {
			*p++ = '\0';
			num = atoi(buf);
			t = atoi(p);
			if ((num < 1) || (num > 255) || (t < 1) || (t > 999))
				fail = 1;
		}
		if (fail)
		{
			sendnotice(sptr, "Error in setting +j, syntax: +j <num>:<seconds>, where <num> must be 1-255, and <seconds> 1-999");
			//sendto_one(sptr, err_str(RPL_MODESYNTAX), chptr->chname, 'j', "Syntax: +j <num>:<seconds>, where <... FIXME ;)
			return EX_DENY;
		}
		return EX_ALLOW;
	}

	/* falltrough -- should not be used */
	return EX_DENY;
}

void *cmodej_put_param(void *r_in, char *param)
{
aModejEntry *r = (aModejEntry *)r_in;
char buf[32], *p;
int num, t;

	if (!r)
	{
		/* Need to create one */
		r = (aModejEntry *)MyMallocEx(sizeof(aModejEntry));
	}
	strlcpy(buf, param, sizeof(buf));
	p = strchr(buf, ':');
	if (p)
	{
		*p++ = '\0';
		num = atoi(buf);
		t = atoi(p);
		if (num < 1) num = 1;
		if (num > 255) num = 255;
		if (t < 1) t = 1;
		if (t > 999) t = 999;
		r->num = num;
		r->t = t;
	} else {
		r->num = 0;
		r->t = 0;
	}
	return (void *)r;
}

char *cmodej_get_param(void *r_in)
{
aModejEntry *r = (aModejEntry *)r_in;
static char retbuf[16];

	if (!r)
		return NULL;

	snprintf(retbuf, sizeof(retbuf), "%hu:%hu", r->num, r->t);
	return retbuf;
}

char *cmodej_conv_param(char *param_in, aClient *sptr)
{
static char retbuf[32];
char param[32], *p;
int num, t, fail = 0;
		
	strlcpy(param, param_in, sizeof(param));
	p = strchr(param, ':');
	if (!p)
		return NULL;
	*p++ = '\0';
	num = atoi(param);
	t = atoi(p);
	if (num < 1)
		num = 1;
	if (num > 255)
		num = 255;
	if (t < 1)
		t = 1;
	if (t > 999)
		t = 999;
	
	snprintf(retbuf, sizeof(retbuf), "%d:%d", num, t);
	return retbuf;
}

void cmodej_free_param(void *r)
{
	MyFree(r);
}

void *cmodej_dup_struct(void *r_in)
{
aModejEntry *r = (aModejEntry *)r_in;
aModejEntry *w = (aModejEntry *)MyMalloc(sizeof(aModejEntry));

	memcpy(w, r, sizeof(aModejEntry));
	return (void *)w;
}

int cmodej_sjoin_check(aChannel *chptr, void *ourx, void *theirx)
{
aModejEntry *our = (aModejEntry *)ourx;
aModejEntry *their = (aModejEntry *)theirx;

	if (our->t != their->t)
	{
		if (our->t > their->t)
			return EXSJ_WEWON;
		else
			return EXSJ_THEYWON;
	}
	else if (our->num != their->num)
	{
		if (our->num > their->num)
			return EXSJ_WEWON;
		else
			return EXSJ_THEYWON;
	} else
		return EXSJ_SAME;
}

static int isjthrottled(aClient *cptr, aChannel *chptr)
{
aModejEntry *m;
aJFlood *e;
int num=0, t=0;

	if (!MyClient(cptr))
		return 0;

	m = (aModejEntry *)GETPARASTRUCT(chptr, 'j');
	if (!m) return 0; /* caller made an error? */
	num = m->num;
	t = m->t;

#ifdef DEBUGMODE
	if (!num || !t)
		abort();
#endif

	/* Grab user<->chan entry.. */
	for (e = cptr->user->jflood; e; e=e->next_u)
		if (e->chptr == chptr)
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

static void cmodej_increase_usercounter(aClient *cptr, aChannel *chptr)
{
aModejEntry *m;
aJFlood *e;
int num=0, t=0;

	if (!MyClient(cptr))
		return;
		
	m = (aModejEntry *)GETPARASTRUCT(chptr, 'j');
	if (!m) return;
	num = m->num;
	t = m->t;

#ifdef DEBUGMODE
	if (!num || !t)
		abort();
#endif

	/* Grab user<->chan entry.. */
	for (e = cptr->user->jflood; e; e=e->next_u)
		if (e->chptr == chptr)
			break;
	
	if (!e)
	{
		/* Allocate one */
		e = cmodej_addentry(cptr, chptr);
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

int chmode_j_can_join(aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[])
{
	if (!IsAnOper(sptr) &&
	    (chptr->mode.extmode & EXTMODE_JOINTHROTTLE) && isjthrottled(sptr, chptr))
		return ERR_TOOMANYJOINS;
	return 0;
}


int chmode_j_local_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[])
{
	cmodej_increase_usercounter(cptr, chptr);
	return 0;
}

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
//CmodeParam *cmp;
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
			t = ((aModejEntry *)GETPARASTRUCT(chptr, 'j'))->t;
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

int cmodej_cleanup_user2(aClient *sptr)
{
	cmodej_deluserentries(sptr);
	return 0;
}

int cmodej_channel_destroy(aChannel *chptr)
{
	cmodej_delchannelentries(chptr);
	return 0;
}
