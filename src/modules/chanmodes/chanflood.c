/*
 * Channel Mode +f
 * (C) Copyright 2005-2006 Bram Matthys and The UnrealIRCd team.
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


ModuleHeader MOD_HEADER(chanflood)
  = {
	"chanflood",
	"$Id$",
	"Channel Mode +f",
	"3.2-b8-1",
	NULL,
    };

typedef struct SChanFloodProt ChanFloodProt;
typedef struct SRemoveFld RemoveFld;

struct SRemoveFld {
	struct SRemoveFld *prev, *next;
	aChannel *chptr;
	char m; /* mode to be removed */
	time_t when; /* scheduled at */
};

struct SChanFloodProt {
	unsigned short	per; /* setting: per <XX> seconds */
	time_t			t[NUMFLD]; /* runtime: timers */
	unsigned short	c[NUMFLD]; /* runtime: counters */
	unsigned short	l[NUMFLD]; /* setting: limit */
	unsigned char	a[NUMFLD]; /* setting: action */
	unsigned char	r[NUMFLD]; /* setting: remove-after <this> minutes */
	unsigned long	timer_flags; /* if a "-m timer" is running this is & MODE_MODERATED etc.. */
};

/* FIXME: note to self: get_param() is not enough for module reloading, need to have an alternative serialize_struct() for like in our case where we want to remember timer settings and all .. (?) */

ModuleInfo *ModInfo = NULL;

Cmode_t EXTMODE_FLOODLIMIT = 0L;

#define IsFloodLimit(x)	((x)->mode.extmode & EXTMODE_FLOODLIMIT)

void chanfloodtimer_del(aChannel *chptr, char mflag, long mbit);
void chanfloodtimer_stopchantimers(aChannel *chptr);

static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r);
static int compare_floodprot_modes(ChanFloodProt *a, ChanFloodProt *b);
static int do_chanflood(aChannel *chptr, int what);
char *channel_modef_string(ChanFloodProt *x);
int  check_for_chan_flood(aClient *sptr, aChannel *chptr);
void do_chanflood_action(aChannel *chptr, int what, char *text);

int cmodef_is_ok(aClient *sptr, aChannel *chptr, char *para, int type, int what);
void *cmodef_put_param(void *r_in, char *param);
char *cmodef_get_param(void *r_in);
char *cmodef_conv_param(char *param_in, aClient *cptr);
void cmodef_free_param();
void *cmodef_dup_struct(void *r_in);
int cmodef_sjoin_check(aChannel *chptr, void *ourx, void *theirx);
int chanflood_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[]);
EVENT(modef_event);
int cmodef_channel_destroy(aChannel *chptr);
char *chanflood_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
int chanflood_post_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
int chanflood_knock(aClient *sptr, aChannel *chptr);
int chanflood_local_nickchange(aClient *sptr, char *oldnick);
int chanflood_remote_nickchange(aClient *cptr, aClient *sptr, char *oldnick);
int chanflood_chanmode_fixme(aChannel *chptr, long mode);

DLLFUNC int MOD_INIT(chanflood)(ModuleInfo *modinfo)
{
	CmodeInfo req;
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	memset(&req, 0, sizeof(req));
	ModInfo = modinfo;

	req.paracount = 1;
	req.is_ok = cmodef_is_ok;
	req.flag = 'f';
	req.put_param = cmodef_put_param;
	req.get_param = cmodef_get_param;
	req.conv_param = cmodef_conv_param;
	req.free_param = cmodef_free_param;
	req.dup_struct = cmodef_dup_struct;
	req.sjoin_check = cmodef_sjoin_check;
	CmodeAdd(modinfo->handle, req, &EXTMODE_FLOODLIMIT);

	HookAddPCharEx(modinfo->handle, HOOKTYPE_PRE_CHANMSG, chanflood_pre_chanmsg);
	HookAddEx(modinfo->handle, HOOKTYPE_CHANMSG, chanflood_post_chanmsg);
	HookAddEx(modinfo->handle, HOOKTYPE_KNOCK, chanflood_knock);
	HookAddEx(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, chanflood_local_nickchange);
	HookAddEx(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, chanflood_remote_nickchange);
	HookAddEx(modinfo->handle, HOOKTYPE_MODECHAR_FIXME, chanflood_chanmode_fixme);
	HookAddEx(modinfo->handle, HOOKTYPE_LOCAL_JOIN, chanflood_join);
	HookAddEx(modinfo->handle, HOOKTYPE_REMOTE_JOIN, chanflood_join);
	HookAddEx(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, cmodef_channel_destroy);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(chanflood)(int module_load)
{
	EventAddEx(ModInfo->handle, "modef_event", 10, 0, modef_event, NULL);
	return MOD_SUCCESS;
}


DLLFUNC int MOD_UNLOAD(chanflood)(int module_unload)
{
	sendto_realops("Mod_Unload was called??? Arghhhhhh..");
	return MOD_FAILED;
}

int cmodef_is_ok(aClient *sptr, aChannel *chptr, char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chan_op(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendto_one(sptr, err_str(ERR_NOTFORHALFOPS), me.name, sptr->name, 'f');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		ChanFloodProt newf;
		int xxi, xyi, xzi, hascolon;
		char *xp;
		/* old +f was like +f 10:5 or +f *10:5
		 * new is +f [5c,30j,10t#b]:15
		 * +f 10:5  --> +f [10t]:5
		 * +f *10:5 --> +f [10t#b]:5
		 */
		if (param[0] != '[')
		{
			/* <<OLD +f>> */
		  /* like 1:1 and if its less than 3 chars then ahem.. */
		  if (strlen(param) < 3)
		  	goto invalidsyntax;
		  /* may not contain other chars 
		     than 0123456789: & NULL */
		  hascolon = 0;
		  for (xp = param; *xp; xp++)
		  {
			  if (*xp == ':')
				hascolon++;
			  /* fast alpha check */
			  if (((*xp < '0') || (*xp > '9'))
			      && (*xp != ':')
			      && (*xp != '*'))
				goto invalidsyntax;
			  /* uh oh, not the first char */
			  if (*xp == '*' && (xp != param))
				goto invalidsyntax;
		  }
		  /* We can avoid 2 strchr() and a strrchr() like this
		   * it should be much faster. -- codemastr
		   */
		  if (hascolon != 1)
			goto invalidsyntax;
		  if (*param == '*')
		  {
			  xzi = 1;
			  //                      chptr->mode.kmode = 1;
		  }
		  else
		  {
			  xzi = 0;

			  //                   chptr->mode.kmode = 0;
		  }
		  xp = index(param, ':');
		  *xp = '\0';
		  xxi =
		      atoi((*param ==
		      '*' ? (param + 1) : param));
		  xp++;
		  xyi = atoi(xp);
		  if (xxi > 500 || xyi > 500)
			goto invalidsyntax;
		  xp--;
		  *xp = ':';
		  if ((xxi == 0) || (xyi == 0))
			  goto invalidsyntax;

		  /* ok, we passed */
		  newf.l[FLD_TEXT] = xxi;
		  newf.per = xyi;
		  if (xzi == 1)
		      newf.a[FLD_TEXT] = 'b';
		} else {
			/* NEW +F */
			char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
			int v;
			unsigned short warnings = 0, breakit;
			unsigned char r;

			/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
			strlcpy(xbuf, param, sizeof(xbuf));
			p2 = strchr(xbuf+1, ']');
			if (!p2)
				goto invalidsyntax;
			*p2 = '\0';
			if (*(p2+1) != ':')
				goto invalidsyntax;
			breakit = 0;
			for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
			{
				/* <number><1 letter>[optional: '#'+1 letter] */
				p = x;
				while(isdigit(*p)) { p++; }
				if ((*p == '\0') ||
				    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
				      (*p == 'm') || (*p == 'n') || (*p == 't')))
				{
					if (MyClient(sptr) && *p && (warnings++ < 3))
						sendto_one(sptr, ":%s NOTICE %s :warning: channelmode +f: floodtype '%c' unknown, ignored.",
							me.name, sptr->name, *p);
					continue; /* continue instead of break for forward compatability. */
				}
				c = *p;
				*p = '\0';
				v = atoi(x);
				if ((v < 1) || (v > 999)) /* out of range... */
				{
					if (MyClient(sptr))
					{
						sendto_one(sptr, err_str(ERR_CANNOTCHANGECHANMODE),
							   me.name, sptr->name, 
							   'f', "value should be from 1-999");
						goto invalidsyntax;
					} else
						continue; /* just ignore for remote servers */
				}
				p++;
				a = '\0';
				r = MyClient(sptr) ? MODEF_DEFAULT_UNSETTIME : 0;
				if (*p != '\0')
				{
					if (*p == '#')
					{
						p++;
						a = *p;
						p++;
						if (*p != '\0')
						{
							int tv;
							tv = atoi(p);
							if (tv <= 0)
								tv = 0; /* (ignored) */
							if (tv > (MyClient(sptr) ? MODEF_MAX_UNSETTIME : 255))
								tv = (MyClient(sptr) ? MODEF_MAX_UNSETTIME : 255); /* set to max */
							r = (unsigned char)tv;
						}
					}
				}

				switch(c)
				{
					case 'c':
						newf.l[FLD_CTCP] = v;
						if ((a == 'm') || (a == 'M'))
							newf.a[FLD_CTCP] = a;
						else
							newf.a[FLD_CTCP] = 'C';
						newf.r[FLD_CTCP] = r;
						break;
					case 'j':
						newf.l[FLD_JOIN] = v;
						if (a == 'R')
							newf.a[FLD_JOIN] = a;
						else
							newf.a[FLD_JOIN] = 'i';
						newf.r[FLD_JOIN] = r;
						break;
					case 'k':
						newf.l[FLD_KNOCK] = v;
						newf.a[FLD_KNOCK] = 'K';
						newf.r[FLD_KNOCK] = r;
						break;
					case 'm':
						newf.l[FLD_MSG] = v;
						if (a == 'M')
							newf.a[FLD_MSG] = a;
						else
							newf.a[FLD_MSG] = 'm';
						newf.r[FLD_MSG] = r;
						break;
					case 'n':
						newf.l[FLD_NICK] = v;
						newf.a[FLD_NICK] = 'N';
						newf.r[FLD_NICK] = r;
						break;
					case 't':
						newf.l[FLD_TEXT] = v;
						if (a == 'b')
							newf.a[FLD_TEXT] = a;
						/** newf.r[FLD_TEXT] ** not supported */
						break;
					default:
						// fixme: send uknown character thingy?
						goto invalidsyntax;
				}
			} /* for */
			/* parse 'per' */
			p2++;
			if (*p2 != ':')
				goto invalidsyntax;
			p2++;
			if (!*p2)
				goto invalidsyntax;
			v = atoi(p2);
			if ((v < 1) || (v > 999)) /* 'per' out of range */
			{
				if (MyClient(sptr))
					sendto_one(sptr, err_str(ERR_CANNOTCHANGECHANMODE), 
						   me.name, sptr->name, 'f', 
						   "time range should be 1-999");
				goto invalidsyntax;
			}
			newf.per = v;
			
			/* Is anything turned on? (to stop things like '+f []:15' */
			breakit = 1;
			for (v=0; v < NUMFLD; v++)
				if (newf.l[v])
					breakit=0;
			if (breakit)
				goto invalidsyntax;
			
		} /* if param[0] == '[' */ 
		
		return EX_ALLOW;
invalidsyntax:
		sendto_one(sptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, sptr->name, 'f', "Invalid syntax for MODE +f"); /* FIXME */
		return EX_DENY;
	}

	/* falltrough -- should not be used */
	return EX_DENY;
}

void *cmodef_put_param(void *fld_in, char *param)
{
ChanFloodProt *fld = (ChanFloodProt *)fld_in;
char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
int v;
unsigned short warnings = 0, breakit;
unsigned char r;

	strlcpy(xbuf, param, sizeof(xbuf));

	if (!fld)
	{
		/* Need to create one */
		fld = MyMallocEx(sizeof(ChanFloodProt));
	}

	/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */

	p2 = strchr(xbuf+1, ']');
	if (!p2)
		return NULL;
	*p2 = '\0';
	if (*(p2+1) != ':')
		return NULL;
	breakit = 0;
	for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
	{
		/* <number><1 letter>[optional: '#'+1 letter] */
		p = x;
		while(isdigit(*p)) { p++; }
		if ((*p == '\0') ||
		    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
		      (*p == 'm') || (*p == 'n') || (*p == 't')))
		{
			/* (unknown type) */
			continue; /* continue instead of break for forward compatability. */
		}
		c = *p;
		*p = '\0';
		v = atoi(x);
		if (v < 1)
			return NULL;
		p++;
		a = '\0';
		r = 0;
		if (*p != '\0')
		{
			if (*p == '#')
			{
				p++;
				a = *p;
				p++;
				if (*p != '\0')
				{
					int tv;
					tv = atoi(p);
					if (tv <= 0)
						tv = 0; /* (ignored) */
					r = (unsigned char)tv;
				}
			}
		}

		switch(c)
		{
			case 'c':
				fld->l[FLD_CTCP] = v;
				if ((a == 'm') || (a == 'M'))
					fld->a[FLD_CTCP] = a;
				else
					fld->a[FLD_CTCP] = 'C';
				fld->r[FLD_CTCP] = r;
				break;
			case 'j':
				fld->l[FLD_JOIN] = v;
				if (a == 'R')
					fld->a[FLD_JOIN] = a;
				else
					fld->a[FLD_JOIN] = 'i';
				fld->r[FLD_JOIN] = r;
				break;
			case 'k':
				fld->l[FLD_KNOCK] = v;
				fld->a[FLD_KNOCK] = 'K';
				fld->r[FLD_KNOCK] = r;
				break;
			case 'm':
				fld->l[FLD_MSG] = v;
				if (a == 'M')
					fld->a[FLD_MSG] = a;
				else
					fld->a[FLD_MSG] = 'm';
				fld->r[FLD_MSG] = r;
				break;
			case 'n':
				fld->l[FLD_NICK] = v;
				fld->a[FLD_NICK] = 'N';
				fld->r[FLD_NICK] = r;
				break;
			case 't':
				fld->l[FLD_TEXT] = v;
				if (a == 'b')
					fld->a[FLD_TEXT] = a;
				/** fld->r[FLD_TEXT] ** not supported */
				break;
			default:
				return NULL;
		}
	} /* for */
	/* parse 'per' */
	p2++;
	if (*p2 != ':')
		return NULL;
	p2++;
	if (!*p2)
		return NULL;
	v = atoi(p2);
	if (v < 1)
		return NULL;
	fld->per = v;
	
	/* Is anything turned on? (to stop things like '+f []:15' */
	breakit = 1;
	for (v=0; v < NUMFLD; v++)
		if (fld->l[v])
			breakit=0;
	if (breakit)
		return NULL;

	return (void *)fld;
}

char *cmodef_get_param(void *r_in)
{
ChanFloodProt *r = (ChanFloodProt *)r_in;
static char retbuf[512];

	if (!r)
		return NULL;

	strcpy(retbuf, channel_modef_string(r)); /* safe: buffer is large enough */
	return retbuf;
}

char *cmodef_conv_param(char *param_in, aClient *cptr)
{
static char retbuf[256];
char param[256], *p;
int num, t, fail = 0;
ChanFloodProt newf;
int xxi, xyi, xzi, hascolon;
char *xp;

	memset(&newf, 0, sizeof(newf));
		
	strlcpy(param, param_in, sizeof(param));

	/* old +f was like +f 10:5 or +f *10:5
	 * new is +f [5c,30j,10t#b]:15
	 * +f 10:5  --> +f [10t]:5
	 * +f *10:5 --> +f [10t#b]:5
	 */
	if (param[0] != '[')
	{
		/* <<OLD +f>> */
	  /* like 1:1 and if its less than 3 chars then ahem.. */
	  if (strlen(param) < 3)
	  	return NULL;
	  /* may not contain other chars 
	     than 0123456789: & NULL */
	  hascolon = 0;
	  for (xp = param; *xp; xp++)
	  {
		  if (*xp == ':')
			hascolon++;
		  /* fast alpha check */
		  if (((*xp < '0') || (*xp > '9'))
		      && (*xp != ':')
		      && (*xp != '*'))
			return NULL;
		  /* uh oh, not the first char */
		  if (*xp == '*' && (xp != param))
			return NULL;
	  }
	  /* We can avoid 2 strchr() and a strrchr() like this
	   * it should be much faster. -- codemastr
	   */
	  if (hascolon != 1)
		return NULL;
	  if (*param == '*')
	  {
		  xzi = 1;
		  //                      chptr->mode.kmode = 1;
	  }
	  else
	  {
		  xzi = 0;

		  //                   chptr->mode.kmode = 0;
	  }
	  xp = index(param, ':');
	  *xp = '\0';
	  xxi =
	      atoi((*param ==
	      '*' ? (param + 1) : param));
	  xp++;
	  xyi = atoi(xp);
	  if (xxi > 500 || xyi > 500)
		return NULL;
	  xp--;
	  *xp = ':';
	  if ((xxi == 0) || (xyi == 0))
		  return NULL;

	  /* ok, we passed */
	  newf.l[FLD_TEXT] = xxi;
	  newf.per = xyi;
	  if (xzi == 1)
	      newf.a[FLD_TEXT] = 'b';
	} else {
		/* NEW +F */
		char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
		int v;
		unsigned short warnings = 0, breakit;
		unsigned char r;

		/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
		strlcpy(xbuf, param, sizeof(xbuf));
		p2 = strchr(xbuf+1, ']');
		if (!p2)
			return NULL;
		*p2 = '\0';
		if (*(p2+1) != ':')
			return NULL;
		breakit = 0;
		for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
		{
			/* <number><1 letter>[optional: '#'+1 letter] */
			p = x;
			while(isdigit(*p)) { p++; }
			if ((*p == '\0') ||
			    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
			      (*p == 'm') || (*p == 'n') || (*p == 't')))
			{
				/* (unknown type) */
				continue; /* continue instead of break for forward compatability. */
			}
			c = *p;
			*p = '\0';
			v = atoi(x);
			if ((v < 1) || (v > 999)) /* out of range... */
			{
				if (MyClient(cptr) || (v < 1))
					return NULL;
			}
			p++;
			a = '\0';
			r = MyClient(cptr) ? MODEF_DEFAULT_UNSETTIME : 0;
			if (*p != '\0')
			{
				if (*p == '#')
				{
					p++;
					a = *p;
					p++;
					if (*p != '\0')
					{
						int tv;
						tv = atoi(p);
						if (tv <= 0)
							tv = 0; /* (ignored) */
						if (tv > (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255))
							tv = (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255); /* set to max */
						r = (unsigned char)tv;
					}
				}
			}

			switch(c)
			{
				case 'c':
					newf.l[FLD_CTCP] = v;
					if ((a == 'm') || (a == 'M'))
						newf.a[FLD_CTCP] = a;
					else
						newf.a[FLD_CTCP] = 'C';
					newf.r[FLD_CTCP] = r;
					break;
				case 'j':
					newf.l[FLD_JOIN] = v;
					if (a == 'R')
						newf.a[FLD_JOIN] = a;
					else
						newf.a[FLD_JOIN] = 'i';
					newf.r[FLD_JOIN] = r;
					break;
				case 'k':
					newf.l[FLD_KNOCK] = v;
					newf.a[FLD_KNOCK] = 'K';
					newf.r[FLD_KNOCK] = r;
					break;
				case 'm':
					newf.l[FLD_MSG] = v;
					if (a == 'M')
						newf.a[FLD_MSG] = a;
					else
						newf.a[FLD_MSG] = 'm';
					newf.r[FLD_MSG] = r;
					break;
				case 'n':
					newf.l[FLD_NICK] = v;
					newf.a[FLD_NICK] = 'N';
					newf.r[FLD_NICK] = r;
					break;
				case 't':
					newf.l[FLD_TEXT] = v;
					if (a == 'b')
						newf.a[FLD_TEXT] = a;
					/** newf.r[FLD_TEXT] ** not supported */
					break;
				default:
					return NULL;
			}
		} /* for */
		/* parse 'per' */
		p2++;
		if (*p2 != ':')
			return NULL;
		p2++;
		if (!*p2)
			return NULL;
		v = atoi(p2);
		if ((v < 1) || (v > 999)) /* 'per' out of range */
		{
			if (MyClient(cptr) || (v < 1))
				return NULL;
		}
		newf.per = v;
		
		/* Is anything turned on? (to stop things like '+f []:15' */
		breakit = 1;
		for (v=0; v < NUMFLD; v++)
			if (newf.l[v])
				breakit=0;
		if (breakit)
			return NULL;
		
	} /* if param[0] == '[' */ 

	strlcpy(retbuf, channel_modef_string(&newf), sizeof(retbuf));
	return retbuf;
}

void cmodef_free_param(void *r)
{
	// FIXME: cancel timers just to e sure? or maybe in DEBUGMODE?
	MyFree(r);
}

void *cmodef_dup_struct(void *r_in)
{
ChanFloodProt *r = (ChanFloodProt *)r_in;
ChanFloodProt *w = (ChanFloodProt *)MyMalloc(sizeof(ChanFloodProt));

	memcpy(w, r, sizeof(ChanFloodProt));
	return (void *)w;
}

int cmodef_sjoin_check(aChannel *chptr, void *ourx, void *theirx)
{

ChanFloodProt *our = (ChanFloodProt *)ourx;
ChanFloodProt *their = (ChanFloodProt *)theirx;
char *x;
int i;

	if (compare_floodprot_modes(our, their) == 0)
		return EXSJ_SAME;
	
	our->per = MAX(our->per, their->per);
	for (i=0; i < NUMFLD; i++)
	{
		our->l[i] = MAX(our->l[i], their->l[i]);
		our->a[i] = MAX(our->a[i], their->a[i]);
		our->r[i] = MAX(our->r[i], their->r[i]);
	}
	
	return EXSJ_MERGE;
}

int chanflood_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[])
{
	/* I'll explain this only once:
	 * 1. if channel is +f
	 * 2. local client OR synced server
	 * 3. then, increase floodcounter
	 * 4. if we reached the limit AND only if source was a local client.. do the action (+i).
	 * Nr 4 is done because otherwise you would have a noticeflood with 'joinflood detected'
	 * from all servers.
	 */
	if (IsFloodLimit(chptr) && (MyClient(sptr) || sptr->srvptr->serv->flags.synced) && 
	    !IsULine(sptr) && do_chanflood(chptr, FLD_JOIN) && MyClient(sptr))
	{
		do_chanflood_action(chptr, FLD_JOIN, "join");
	}
	return 0;
}

int cmodef_cleanup_user2(aClient *sptr)
{
	return 0;
}

int cmodef_channel_destroy(aChannel *chptr)
{
	chanfloodtimer_stopchantimers(chptr);
	return 0;
}

/* [just a helper for channel_modef_string()] */
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r)
{
char *p;
char tmpbuf[16], *p2 = tmpbuf;

	ircsprintf(buf, "%hd", l);
	p = buf + strlen(buf);
	*p++ = t;
	if (a && ((a != tdef) || r))
	{
		*p++ = '#';
		*p++ = a;
		if (r)
		{
			sprintf(tmpbuf, "%hd", (short)r);
			while ((*p = *p2++))
				p++;
		}
	}
	*p++ = ',';
	return p;
}

/** returns the channelmode +f string (ie: '[5k,40j]:10') */
char *channel_modef_string(ChanFloodProt *x)
{
static char retbuf[512]; /* overkill :p */
char *p = retbuf;
	*p++ = '[';

	/* (alphabetized) */
	if (x->l[FLD_CTCP])
		p = chmodefstrhelper(p, 'c', 'C', x->l[FLD_CTCP], x->a[FLD_CTCP], x->r[FLD_CTCP]);
	if (x->l[FLD_JOIN])
		p = chmodefstrhelper(p, 'j', 'i', x->l[FLD_JOIN], x->a[FLD_JOIN], x->r[FLD_JOIN]);
	if (x->l[FLD_KNOCK])
		p = chmodefstrhelper(p, 'k', 'K', x->l[FLD_KNOCK], x->a[FLD_KNOCK], x->r[FLD_KNOCK]);
	if (x->l[FLD_MSG])
		p = chmodefstrhelper(p, 'm', 'm', x->l[FLD_MSG], x->a[FLD_MSG], x->r[FLD_MSG]);
	if (x->l[FLD_NICK])
		p = chmodefstrhelper(p, 'n', 'N', x->l[FLD_NICK], x->a[FLD_NICK], x->r[FLD_NICK]);
	if (x->l[FLD_TEXT])
		p = chmodefstrhelper(p, 't', '\0', x->l[FLD_TEXT], x->a[FLD_TEXT], x->r[FLD_TEXT]);

	if (*(p - 1) == ',')
		p--;
	*p++ = ']';
	ircsprintf(p, ":%hd", x->per);
	return retbuf;
}

char *chanflood_pre_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
	if (MyClient(sptr) && (check_for_chan_flood(sptr, chptr) == 1))
		return NULL; /* don't send it */
	return text;
}

int chanflood_post_chanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
	if (!IsFloodLimit(chptr) || is_skochanop(sptr, chptr) || IsULine(sptr))
		return 0;

	/* HINT: don't be so stupid to reorder the items in the if's below.. you'll break things -- Syzop. */
	
	if (do_chanflood(chptr, FLD_MSG) && MyClient(sptr))
		do_chanflood_action(chptr, FLD_MSG, "msg/notice");
				
	if ((text[0] == '\001') && strncmp(text+1, "ACTION ", 7) &&
	    do_chanflood(chptr, FLD_CTCP) && MyClient(sptr))
	{
		do_chanflood_action(chptr, FLD_CTCP, "CTCP");
	}

	return 0;
}

#if 0
int chanflood_remotejoin(aClient *cptr, aClient *acptr, aChannel *chptr, char *parv[])
{
	if (IsFloodLimit(chptr) && acptr->serv->flags.synced && !IsULine(acptr)) /* hope that's correctly copied? acptr/cptr fun */
		do_chanflood(chptr, FLD_JOIN);
	return 0;
}
#endif

int chanflood_knock(aClient *sptr, aChannel *chptr)
{
	if (IsFloodLimit(chptr) && !IsULine(sptr) && do_chanflood(chptr, FLD_KNOCK) && MyClient(sptr))
		do_chanflood_action(chptr, FLD_KNOCK, "knock");
	return 0;
}

static int gotnickchange(aClient *sptr)
{
Membership *mp;

	for (mp = sptr->user->channel; mp; mp = mp->next)
	{
		aChannel *chptr = mp->chptr;
		if (chptr && IsFloodLimit(chptr) &&
		    !(mp->flags & (CHFL_CHANOP|CHFL_VOICE|CHFL_CHANOWNER|CHFL_HALFOP|CHFL_CHANPROT)) &&
		    do_chanflood(chptr, FLD_NICK) && MyClient(sptr))
		{
			do_chanflood_action(chptr, FLD_NICK, "nick");
		}
	}
	return 0;
}

int chanflood_local_nickchange(aClient *sptr, char *oldnick)
{
	if (IsULine(sptr))
		return 0;
	return gotnickchange(sptr);
}

int chanflood_remote_nickchange(aClient *cptr, aClient *sptr, char *oldnick)
{
	if (IsULine(sptr))
		return 0;
	return gotnickchange(sptr);
}

int chanflood_chanmode_fixme(aChannel *chptr, long modetype)
{
ChanFloodProt *chp;

	if (IsFloodLimit(chptr))
		return 0;
	
	chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');
#ifdef DEBUGMODE
	if (!chp)
		abort();
#endif

	/* reset joinflood on -i, reset msgflood on -m, etc.. */
	switch(modetype)
	{
		case MODE_NOCTCP:
			chp->c[FLD_CTCP] = 0;
			chanfloodtimer_del(chptr, 'C', MODE_NOCTCP);
			break;
		case MODE_NONICKCHANGE:
			chp->c[FLD_NICK] = 0;
			chanfloodtimer_del(chptr, 'N', MODE_NONICKCHANGE);
			break;
		case MODE_MODERATED:
			chp->c[FLD_MSG] = 0;
			chp->c[FLD_CTCP] = 0;
			chanfloodtimer_del(chptr, 'm', MODE_MODERATED);
			break;
		case MODE_NOKNOCK:
			chp->c[FLD_KNOCK] = 0;
			chanfloodtimer_del(chptr, 'K', MODE_NOKNOCK);
			break;
		case MODE_INVITEONLY:
			chp->c[FLD_JOIN] = 0;
			chanfloodtimer_del(chptr, 'i', MODE_INVITEONLY);
			break;
		case MODE_MODREG:
			chp->c[FLD_MSG] = 0;
			chp->c[FLD_CTCP] = 0;
			chanfloodtimer_del(chptr, 'M', MODE_MODREG);
			break;
		case MODE_RGSTRONLY:
			chp->c[FLD_JOIN] = 0;
			chanfloodtimer_del(chptr, 'R', MODE_RGSTRONLY);
			break;
		default:
			break;
	}
	return 0;
}

int  check_for_chan_flood(aClient *sptr, aChannel *chptr)
{
	Membership *lp;
	MembershipL *lp2;
	int c_limit, t_limit, banthem;
	ChanFloodProt *chp;

	if (!MyClient(sptr) || !IsFloodLimit(chptr) || IsOper(sptr) || IsULine(sptr) || is_skochanop(sptr, chptr))
		return 0;

	if (!(lp = find_membership_link(sptr->user->channel, chptr)))
		return 0; /* not in channel */

	lp2 = (MembershipL *) lp;
	
	chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');

#ifdef NEWCHFLOODPROT
	if (!chp || !chp->l[FLD_TEXT])
		return 0;
	c_limit = chp->l[FLD_TEXT];
	t_limit = chp->per;
	banthem = (chp->a[FLD_TEXT] == 'b') ? 1 : 0;
#else
	if ((chptr->mode.msgs < 1) || (chptr->mode.per < 1))
		return 0;
	c_limit = chptr->mode.msgs;
	t_limit = chptr->mode.per;
	banthem = chptr->mode.kmode;
#endif
	/* if current - firstmsgtime >= mode.per, then reset,
	 * if nummsg > mode.msgs then kick/ban
	 */
	Debug((DEBUG_ERROR, "Checking for flood +f: firstmsg=%d (%ds ago), new nmsgs: %d, limit is: %d:%d",
		lp2->flood.firstmsg, TStime() - lp2->flood.firstmsg, lp2->flood.nmsg + 1,
		c_limit, t_limit));
	if ((TStime() - lp2->flood.firstmsg) >= t_limit)
	{
		/* reset */
		lp2->flood.firstmsg = TStime();
		lp2->flood.nmsg = 1;
		return 0; /* forget about it.. */
	}

	/* increase msgs */
	lp2->flood.nmsg++;

	if ((lp2->flood.nmsg) > c_limit)
	{
		char comment[1024], mask[1024];
		ircsprintf(comment,
		    "Flooding (Limit is %i lines per %i seconds)",
		    c_limit, t_limit);
		if (banthem)
		{		/* ban. */
			ircsprintf(mask, "*!*@%s", GetHost(sptr));
			add_listmode(&chptr->banlist, &me, chptr, mask);
			sendto_serv_butone(&me, ":%s MODE %s +b %s 0",
			    me.name, chptr->chname, mask);
			sendto_channel_butserv(chptr, &me,
			    ":%s MODE %s +b %s", me.name, chptr->chname, mask);
		}
		sendto_channel_butserv(chptr, &me,
		    ":%s KICK %s %s :%s", me.name,
		    chptr->chname, sptr->name, comment);
		sendto_serv_butone_token(NULL, me.name,
			MSG_KICK, TOK_KICK, 
			"%s %s :%s",
		   chptr->chname, sptr->name, comment);
		remove_user_from_channel(sptr, chptr);
		return 1;
	}
	return 0;
}

MODVAR RemoveFld *removefld_list = NULL;

RemoveFld *chanfloodtimer_find(aChannel *chptr, char mflag)
{
RemoveFld *e;

	for (e=removefld_list; e; e=e->next)
	{
		if ((e->chptr == chptr) && (e->m == mflag))
			return e;
	}
	return NULL;
}

/*
 * Adds a "remove channelmode set by +f" timer.
 * chptr	Channel
 * mflag	Mode flag, eg 'C'
 * mbit		Mode bitflag, eg MODE_NOCTCP
 * when		when it should be removed
 * NOTES:
 * - This function takes care of overwriting of any previous timer
 *   for the same modechar.
 * - The function takes care of chptr->mode.floodprot->timer_flags,
 *   do not modify it yourself.
 * - chptr->mode.floodprot is asumed to be non-NULL.
 */
void chanfloodtimer_add(aChannel *chptr, char mflag, long mbit, time_t when)
{
RemoveFld *e = NULL;
unsigned char add=1;
ChanFloodProt *chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');

	if (chp->timer_flags & mbit)
	{
		/* Already exists... */
		e = chanfloodtimer_find(chptr, mflag);
		if (e)
			add = 0;
	}

	if (add)
		e = MyMallocEx(sizeof(RemoveFld));

	e->chptr = chptr;
	e->m = mflag;
	e->when = when;

	if (add)
		AddListItem(e, removefld_list);

	chp->timer_flags |= mbit;
}

void chanfloodtimer_del(aChannel *chptr, char mflag, long mbit)
{
RemoveFld *e;
ChanFloodProt *chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');

	if (chp && !(chp->timer_flags & mbit))
		return; /* nothing to remove.. */
	e = chanfloodtimer_find(chptr, mflag);
	if (!e)
		return;

	DelListItem(e, removefld_list);

	if (chp)
		chp->timer_flags &= ~mbit;
}

long get_chanbitbychar(char m)
{
aCtab *tab = &cFlagTab[0];
	while(tab->mode != 0x0)
	{
		if (tab->flag == m)
			return tab->mode;
		tab++;;
	}
	return 0;
}

EVENT(modef_event)
{
RemoveFld *e = removefld_list;
time_t now;
long mode;

	now = TStime();
	
	while(e)
	{
		if (e->when <= now)
		{
			/* Remove chanmode... */
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c EXPIRED", e->chptr->chname, e->m);
#endif
			mode = get_chanbitbychar(e->m);
			if (e->chptr->mode.mode & mode)
			{
				sendto_serv_butone(&me, ":%s MODE %s -%c 0", me.name, e->chptr->chname, e->m);
				sendto_channel_butserv(e->chptr, &me, ":%s MODE %s -%c", me.name, e->chptr->chname, e->m);
				e->chptr->mode.mode &= ~mode;
			}
			
			/* And delete... */
			e = (RemoveFld *)DelListItem(e, removefld_list);
		} else {
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c about %d seconds",
				e->chptr->chname, e->m, e->when - now);
#endif
			e = e->next;
		}
	}
}

void chanfloodtimer_stopchantimers(aChannel *chptr)
{
RemoveFld *e = removefld_list;
	while(e)
	{
		if (e->chptr == chptr)
			e = (RemoveFld *)DelListItem(e, removefld_list);
		else
			e = e->next;
	}
}



int do_chanflood(aChannel *chptr, int what)
{
ChanFloodProt *chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');

	if (!chp || !chp->l[what]) /* no +f or not restricted */
		return 0;
	if (TStime() - chp->t[what] >= chp->per)
	{
		chp->t[what] = TStime();
		chp->c[what] = 1;
	} else
	{
		chp->c[what]++;
		if ((chp->c[what] > chp->l[what]) &&
		    (TStime() - chp->t[what] < chp->per))
		{
			/* reset it too (makes it easier for chanops to handle the situation) */
			/*
			 *XXchp->t[what] = TStime();
			 *XXchp->c[what] = 1;
			 * 
			 * BAD.. there are some situations where we might 'miss' a flood
			 * because of this. The reset has been moved to -i,-m,-N,-C,etc.
			*/
			return 1; /* flood detected! */
		}
	}
	return 0;
}

void do_chanflood_action(aChannel *chptr, int what, char *text)
{
long modeflag = 0;
aCtab *tab = &cFlagTab[0];
char m;
ChanFloodProt *chp = (ChanFloodProt *)GETPARASTRUCT(chptr, 'f');

	m = chp->a[what];
	if (!m)
		return;

	/* [TODO: add extended channel mode support] */
	
	while(tab->mode != 0x0)
	{
		if (tab->flag == m)
		{
			modeflag = tab->mode;
			break;
		}
		tab++;
	}

	if (!modeflag)
		return;
		
	if (!(chptr->mode.mode & modeflag))
	{
		char comment[1024], target[CHANNELLEN + 8];
		ircsprintf(comment, "*** Channel %sflood detected (limit is %d per %d seconds), setting mode +%c",
			text, chp->l[what], chp->per, m);
		ircsprintf(target, "%%%s", chptr->chname);
		sendto_channelprefix_butone_tok(NULL, &me, chptr,
			PREFIX_HALFOP|PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
			MSG_NOTICE, TOK_NOTICE, target, comment, 0);
		sendto_serv_butone(&me, ":%s MODE %s +%c 0", me.name, chptr->chname, m);
		sendto_channel_butserv(chptr, &me, ":%s MODE %s +%c", me.name, chptr->chname, m);
		chptr->mode.mode |= modeflag;
		if (chp->r[what]) /* Add remove-chanmode timer... */
		{
			chanfloodtimer_add(chptr, m, modeflag, TStime() + ((long)chp->r[what] * 60) - 5);
			/* (since the chanflood timer event is called every 10s, we do -5 here so the accurancy will
			 *  be -5..+5, without it it would be 0..+10.)
			 */
		}
	}
}

// FIXME: REMARK: make sure you can only do a +f/-f once (latest in line wins).

/* Checks if 2 ChanFloodProt modes (chmode +f) are different.
 * This is a bit more complicated than 1 simple memcmp(a,b,..) because
 * counters are also stored in this struct so we have to do
 * it manually :( -- Syzop.
 */
static int compare_floodprot_modes(ChanFloodProt *a, ChanFloodProt *b)
{
	if (memcmp(a->l, b->l, sizeof(a->l)) ||
	    memcmp(a->a, b->a, sizeof(a->a)) ||
	    memcmp(a->r, b->r, sizeof(a->r)))
		return 1;
	else
		return 0;
}
