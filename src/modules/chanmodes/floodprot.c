/*
 * Channel Mode +f
 * (C) Copyright 2019 Syzop and the UnrealIRCd team
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

ModuleHeader MOD_HEADER(floodprot)
  = {
	"chanmodes/floodprot",
	"5.0",
	"Channel Mode +f",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

#define FLD_CTCP	0 /* c */
#define FLD_JOIN	1 /* j */
#define FLD_KNOCK	2 /* k */
#define FLD_MSG		3 /* m */
#define FLD_NICK	4 /* n */
#define FLD_TEXT	5 /* t */
#define FLD_REPEAT	6 /* r */

#define NUMFLD	7 /* 7 flood types */

/** Configuration settings */
struct {
	unsigned char modef_default_unsettime;
	unsigned char modef_max_unsettime;
	long modef_boot_delay;
} cfg;

#define MODEF_DEFAULT_UNSETTIME		cfg.modef_default_unsettime
#define MODEF_MAX_UNSETTIME		cfg.modef_max_unsettime
#define MODEF_BOOT_DELAY		cfg.modef_boot_delay

typedef struct ChannelFloodProtection ChannelFloodProtection;
typedef struct RemoveChannelModeTimer RemoveChannelModeTimer;

struct RemoveChannelModeTimer {
	struct RemoveChannelModeTimer *prev, *next;
	Channel *chptr;
	char m; /* mode to be removed */
	time_t when; /* scheduled at */
};

typedef struct MemberFlood MemberFlood;
struct MemberFlood {
	unsigned short nmsg;
	unsigned short nmsg_repeat;
	time_t firstmsg;
	uint64_t lastmsg;
	uint64_t prevmsg;
};

/* Maximum timers, iotw: max number of possible actions.
 * Currently this is: CNmMKiRd (8)
 */
#define MAXCHMODEFACTIONS 8

/** Per-channel flood protection settings and counters */
struct ChannelFloodProtection {
	unsigned short	per; /**< setting: per <XX> seconds */
	time_t		timer[NUMFLD]; /**< runtime: timers */
	unsigned short	counter[NUMFLD]; /**< runtime: counters */
	unsigned short	limit[NUMFLD]; /**< setting: limit */
	unsigned char	action[NUMFLD]; /**< setting: action */
	unsigned char	remove_after[NUMFLD]; /**< setting: remove-after <this> minutes */
	unsigned char   timers_running[MAXCHMODEFACTIONS+1]; /**< if for example a '-m' timer is running then this contains 'm' */
};

/* Global variables */
ModDataInfo *mdflood = NULL;
Cmode_t EXTMODE_FLOODLIMIT = 0L;
static int timedban_available = 0; /**< Set to 1 if extbans/timedban module is loaded. */
RemoveChannelModeTimer *removechannelmodetimer_list = NULL;
char *floodprot_msghash_key = NULL;

#define IsFloodLimit(x)	((x)->mode.extmode & EXTMODE_FLOODLIMIT)

/* Forward declarations */
static void init_config(void);
int floodprot_rehash_complete(void);
int floodprot_config_test(ConfigFile *, ConfigEntry *, int, int *);
int floodprot_config_run(ConfigFile *, ConfigEntry *, int);
void floodprottimer_del(Channel *chptr, char mflag);
void floodprottimer_stopchantimers(Channel *chptr);
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r);
static int compare_floodprot_modes(ChannelFloodProtection *a, ChannelFloodProtection *b);
static int do_floodprot(Channel *chptr, int what);
char *channel_modef_string(ChannelFloodProtection *x, char *str);
int  check_for_chan_flood(Client *sptr, Channel *chptr, char *text);
void do_floodprot_action(Channel *chptr, int what, char *text);
void floodprottimer_add(Channel *chptr, char mflag, time_t when);
uint64_t gen_floodprot_msghash(char *text);
int cmodef_is_ok(Client *sptr, Channel *chptr, char mode, char *para, int type, int what);
void *cmodef_put_param(void *r_in, char *param);
char *cmodef_get_param(void *r_in);
char *cmodef_conv_param(char *param_in, Client *cptr);
void cmodef_free_param(void *r);
void *cmodef_dup_struct(void *r_in);
int cmodef_sjoin_check(Channel *chptr, void *ourx, void *theirx);
int floodprot_join(Client *cptr, Client *sptr, Channel *chptr, MessageTag *mtags, char *parv[]);
EVENT(modef_event);
int cmodef_channel_destroy(Channel *chptr, int *should_destroy);
char *floodprot_pre_chanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice);
int floodprot_post_chanmsg(Client *sptr, Channel *chptr, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice);
int floodprot_knock(Client *sptr, Channel *chptr, MessageTag *mtags, char *comment);
int floodprot_local_nickchange(Client *sptr, char *oldnick);
int floodprot_remote_nickchange(Client *cptr, Client *sptr, char *oldnick);
int floodprot_chanmode_del(Channel *chptr, int m);
void memberflood_free(ModData *md);
int floodprot_stats(Client *sptr, char *flag);
void floodprot_free_removechannelmodetimer_list(ModData *m);
void floodprot_free_msghash_key(ModData *m);

MOD_TEST(floodprot)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, floodprot_config_test);
	return MOD_SUCCESS;
}

MOD_INIT(floodprot)
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmodef_is_ok;
	creq.flag = 'f';
	creq.unset_with_param = 1; /* ah yeah, +f is special! */
	creq.put_param = cmodef_put_param;
	creq.get_param = cmodef_get_param;
	creq.conv_param = cmodef_conv_param;
	creq.free_param = cmodef_free_param;
	creq.dup_struct = cmodef_dup_struct;
	creq.sjoin_check = cmodef_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_FLOODLIMIT);

	init_config();

	LoadPersistentPointer(modinfo, removechannelmodetimer_list, floodprot_free_removechannelmodetimer_list);
	LoadPersistentPointer(modinfo, floodprot_msghash_key, floodprot_free_msghash_key);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "floodprot";
	mreq.type = MODDATATYPE_MEMBERSHIP;
	mreq.free = memberflood_free;
	mdflood = ModDataAdd(modinfo->handle, mreq);
	if (!mdflood)
	        abort();
	if (!floodprot_msghash_key)
	{
		floodprot_msghash_key = MyMallocEx(16);
		siphash_generate_key(floodprot_msghash_key);
	}

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, floodprot_config_run);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, floodprot_pre_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, floodprot_post_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_KNOCK, 0, floodprot_knock);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, floodprot_local_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, floodprot_remote_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_MODECHAR_DEL, 0, floodprot_chanmode_del);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 0, cmodef_channel_destroy);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH_COMPLETE, 0, floodprot_rehash_complete);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, floodprot_stats);
	return MOD_SUCCESS;
}

MOD_LOAD(floodprot)
{
	EventAdd(modinfo->handle, "modef_event", 10, 0, modef_event, NULL);
	floodprot_rehash_complete();
	return MOD_SUCCESS;
}

MOD_UNLOAD(floodprot)
{
	SavePersistentPointer(modinfo, removechannelmodetimer_list);
	return MOD_SUCCESS;
}

int floodprot_rehash_complete(void)
{
	timedban_available = is_module_loaded("extbans/timedban");
	return 0;
}

static void init_config(void)
{
	/* This sets some default values */
	memset(&cfg, 0, sizeof(cfg));
	cfg.modef_default_unsettime = 0;
	cfg.modef_max_unsettime = 60; /* 1 hour seems enough :p */
	cfg.modef_boot_delay = 75;
}

int floodprot_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->ce_varname, "modef-default-unsettime"))
	{
		if (!ce->ce_vardata)
		{
			config_error_empty(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"set", ce->ce_varname);
			errors++;
		} else {
			int v = atoi(ce->ce_vardata);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-default-unsettime: value '%d' out of range (should be 1-255)",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, v);
				errors++;
			}
		}
	} else
	if (!strcmp(ce->ce_varname, "modef-max-unsettime"))
	{
		if (!ce->ce_vardata)
		{
			config_error_empty(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"set", ce->ce_varname);
			errors++;
		} else {
			int v = atoi(ce->ce_vardata);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-max-unsettime: value '%d' out of range (should be 1-255)",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, v);
				errors++;
			}
		}
	} else
	if (!strcmp(ce->ce_varname, "modef-boot-delay"))
	{
		if (!ce->ce_vardata)
		{
			config_error_empty(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
				"set", ce->ce_varname);
			errors++;
		} else {
			long v = config_checkval(ce->ce_vardata, CFG_TIME);
			if ((v < 0) || (v > 600))
			{
				config_error("%s:%i: set::modef-boot-delay: value '%ld' out of range (should be 0-600)",
					ce->ce_fileptr->cf_filename, ce->ce_varlinenum, v);
				errors++;
			}
		}
	} else
	{
		/* Not handled by us */
		return 0;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int floodprot_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->ce_varname, "modef-default-unsettime"))
		cfg.modef_default_unsettime = (unsigned char)atoi(ce->ce_vardata);
	else if (!strcmp(ce->ce_varname, "modef-max-unsettime"))
		cfg.modef_max_unsettime = (unsigned char)atoi(ce->ce_vardata);
	else if (!strcmp(ce->ce_varname, "modef-boot-delay"))
		cfg.modef_boot_delay = config_checkval(ce->ce_vardata, CFG_TIME);
	else
		return 0; /* not handled by us */

	return 1;
}

int cmodef_is_ok(Client *sptr, Channel *chptr, char mode, char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chan_op(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(sptr, ERR_NOTFORHALFOPS, 'f');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		ChannelFloodProtection newf;
		int xxi, xyi, xzi, hascolon;
		char *xp;

		memset(&newf, 0, sizeof(newf));

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
		  xp = strchr(param, ':');
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
		  if ((xxi <= 0) || (xyi <= 0))
			  goto invalidsyntax;

		  /* ok, we passed */
		  newf.limit[FLD_TEXT] = xxi;
		  newf.per = xyi;
		  if (xzi == 1)
		      newf.action[FLD_TEXT] = 'b';
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
				      (*p == 'm') || (*p == 'n') || (*p == 't') ||
				      (*p == 'r')))
				{
					if (MyClient(sptr) && *p && (warnings++ < 3))
						sendnotice(sptr, "warning: channelmode +f: floodtype '%c' unknown, ignored.", *p);
					continue; /* continue instead of break for forward compatability. */
				}
				c = *p;
				*p = '\0';
				v = atoi(x);
				if ((v < 1) || (v > 999)) /* out of range... */
				{
					if (MyClient(sptr))
					{
						sendnumeric(sptr, ERR_CANNOTCHANGECHANMODE,
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
						newf.limit[FLD_CTCP] = v;
						if ((a == 'm') || (a == 'M'))
							newf.action[FLD_CTCP] = a;
						else
							newf.action[FLD_CTCP] = 'C';
						newf.remove_after[FLD_CTCP] = r;
						break;
					case 'j':
						newf.limit[FLD_JOIN] = v;
						if (a == 'R')
							newf.action[FLD_JOIN] = a;
						else
							newf.action[FLD_JOIN] = 'i';
						newf.remove_after[FLD_JOIN] = r;
						break;
					case 'k':
						newf.limit[FLD_KNOCK] = v;
						newf.action[FLD_KNOCK] = 'K';
						newf.remove_after[FLD_KNOCK] = r;
						break;
					case 'm':
						newf.limit[FLD_MSG] = v;
						if (a == 'M')
							newf.action[FLD_MSG] = a;
						else
							newf.action[FLD_MSG] = 'm';
						newf.remove_after[FLD_MSG] = r;
						break;
					case 'n':
						newf.limit[FLD_NICK] = v;
						newf.action[FLD_NICK] = 'N';
						newf.remove_after[FLD_NICK] = r;
						break;
					case 't':
						newf.limit[FLD_TEXT] = v;
						if (a == 'b' || a == 'd')
							newf.action[FLD_TEXT] = a;
						if (timedban_available)
							newf.remove_after[FLD_TEXT] = r;
						break;
					case 'r':
						newf.limit[FLD_REPEAT] = v;
						if (a == 'b' || a == 'd')
							newf.action[FLD_REPEAT] = a;
						if (timedban_available)
							newf.remove_after[FLD_REPEAT] = r;
						break;
					default:
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
					sendnumeric(sptr, ERR_CANNOTCHANGECHANMODE, 'f',
						   "time range should be 1-999");
				goto invalidsyntax;
			}
			newf.per = v;

			/* Is anything turned on? (to stop things like '+f []:15' */
			breakit = 1;
			for (v=0; v < NUMFLD; v++)
				if (newf.limit[v])
					breakit=0;
			if (breakit)
				goto invalidsyntax;

		} /* if param[0] == '[' */

		return EX_ALLOW;
invalidsyntax:
		sendnumeric(sptr, ERR_CANNOTCHANGECHANMODE, 'f', "Invalid syntax for MODE +f");
		return EX_DENY;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void *cmodef_put_param(void *fld_in, char *param)
{
	ChannelFloodProtection *fld = (ChannelFloodProtection *)fld_in;
	char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
	int v;
	unsigned short warnings = 0, breakit;
	unsigned char r;

	strlcpy(xbuf, param, sizeof(xbuf));

	if (!fld)
	{
		/* Need to create one */
		fld = MyMallocEx(sizeof(ChannelFloodProtection));
	}

	/* always reset settings (l, a, r) */
	for (v=0; v < NUMFLD; v++)
	{
		fld->limit[v] = 0;
		fld->action[v] = 0;
		fld->remove_after[v] = 0;
	}

	/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */

	p2 = strchr(xbuf+1, ']');
	if (!p2)
	{
		memset(fld, 0, sizeof(ChannelFloodProtection));
		return fld; /* FAIL */
	}
	*p2 = '\0';
	if (*(p2+1) != ':')
	{
		memset(fld, 0, sizeof(ChannelFloodProtection));
		return fld; /* FAIL */
	}
	breakit = 0;
	for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
	{
		/* <number><1 letter>[optional: '#'+1 letter] */
		p = x;
		while(isdigit(*p)) { p++; }
		if ((*p == '\0') ||
		    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
		      (*p == 'm') || (*p == 'n') || (*p == 't') ||
		      (*p == 'r')))
		{
			/* (unknown type) */
			continue; /* continue instead of break for forward compatability. */
		}
		c = *p;
		*p = '\0';
		v = atoi(x);
		if (v < 1)
			v = 1;
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
				fld->limit[FLD_CTCP] = v;
				if ((a == 'm') || (a == 'M'))
					fld->action[FLD_CTCP] = a;
				else
					fld->action[FLD_CTCP] = 'C';
				fld->remove_after[FLD_CTCP] = r;
				break;
			case 'j':
				fld->limit[FLD_JOIN] = v;
				if (a == 'R')
					fld->action[FLD_JOIN] = a;
				else
					fld->action[FLD_JOIN] = 'i';
				fld->remove_after[FLD_JOIN] = r;
				break;
			case 'k':
				fld->limit[FLD_KNOCK] = v;
				fld->action[FLD_KNOCK] = 'K';
				fld->remove_after[FLD_KNOCK] = r;
				break;
			case 'm':
				fld->limit[FLD_MSG] = v;
				if (a == 'M')
					fld->action[FLD_MSG] = a;
				else
					fld->action[FLD_MSG] = 'm';
				fld->remove_after[FLD_MSG] = r;
				break;
			case 'n':
				fld->limit[FLD_NICK] = v;
				fld->action[FLD_NICK] = 'N';
				fld->remove_after[FLD_NICK] = r;
				break;
			case 't':
				fld->limit[FLD_TEXT] = v;
				if (a == 'b' || a == 'd')
					fld->action[FLD_TEXT] = a;
				if (timedban_available)
					fld->remove_after[FLD_TEXT] = r;
				break;
			case 'r':
				fld->limit[FLD_REPEAT] = v;
				if (a == 'b' || a == 'd')
					fld->action[FLD_REPEAT] = a;
				if (timedban_available)
					fld->remove_after[FLD_REPEAT] = r;
				break;
			default:
				/* NOOP */
				break;
		}
	} /* for */
	/* parse 'per' */
	p2++;
	if (*p2 != ':')
	{
		memset(fld, 0, sizeof(ChannelFloodProtection));
		return fld; /* FAIL */
	}
	p2++;
	if (!*p2)
	{
		memset(fld, 0, sizeof(ChannelFloodProtection));
		return fld; /* FAIL */
	}
	v = atoi(p2);
	if (v < 1)
		v = 1;
	/* if new 'per xxx seconds' is smaller than current 'per' then reset timers/counters (t, c) */
	if (v < fld->per)
	{
		for (v=0; v < NUMFLD; v++)
		{
			fld->timer[v] = 0;
			fld->counter[v] = 0;
		}
	}
	fld->per = v;

	/* Is anything turned on? (to stop things like '+f []:15' */
	breakit = 1;
	for (v=0; v < NUMFLD; v++)
		if (fld->limit[v])
			breakit=0;
	if (breakit)
	{
		memset(fld, 0, sizeof(ChannelFloodProtection));
		return fld; /* FAIL */
	}

	return (void *)fld;
}

char *cmodef_get_param(void *r_in)
{
	ChannelFloodProtection *r = (ChannelFloodProtection *)r_in;
	static char retbuf[512];

	if (!r)
		return NULL;

	channel_modef_string(r, retbuf);
	return retbuf;
}

/** Convert parameter to something proper.
 * NOTE: cptr may be NULL if called for e.g. set::modes-on-join
 */
char *cmodef_conv_param(char *param_in, Client *cptr)
{
	static char retbuf[256];
	char param[256], *p;
	int num, t, fail = 0;
	ChannelFloodProtection newf;
	int xxi, xyi, xzi, hascolon;
	char *xp;
	int localclient = (!cptr || MyClient(cptr)) ? 1 : 0;

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
	  xp = strchr(param, ':');
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
	  if ((xxi <= 0) || (xyi <= 0))
		  return NULL;

	  /* ok, we passed */
	  newf.limit[FLD_TEXT] = xxi;
	  newf.per = xyi;
	  if (xzi == 1)
	      newf.action[FLD_TEXT] = 'b';
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
			      (*p == 'm') || (*p == 'n') || (*p == 't') ||
			      (*p == 'r')))
			{
				/* (unknown type) */
				continue; /* continue instead of break for forward compatability. */
			}
			c = *p;
			*p = '\0';
			v = atoi(x);
			if ((v < 1) || (v > 999)) /* out of range... */
			{
				if (localclient || (v < 1))
					return NULL;
			}
			p++;
			a = '\0';
			r = localclient ? MODEF_DEFAULT_UNSETTIME : 0;
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
						if (tv > (localclient ? MODEF_MAX_UNSETTIME : 255))
							tv = (localclient ? MODEF_MAX_UNSETTIME : 255); /* set to max */
						r = (unsigned char)tv;
					}
				}
			}

			switch(c)
			{
				case 'c':
					newf.limit[FLD_CTCP] = v;
					if ((a == 'm') || (a == 'M'))
						newf.action[FLD_CTCP] = a;
					else
						newf.action[FLD_CTCP] = 'C';
					newf.remove_after[FLD_CTCP] = r;
					break;
				case 'j':
					newf.limit[FLD_JOIN] = v;
					if (a == 'R')
						newf.action[FLD_JOIN] = a;
					else
						newf.action[FLD_JOIN] = 'i';
					newf.remove_after[FLD_JOIN] = r;
					break;
				case 'k':
					newf.limit[FLD_KNOCK] = v;
					newf.action[FLD_KNOCK] = 'K';
					newf.remove_after[FLD_KNOCK] = r;
					break;
				case 'm':
					newf.limit[FLD_MSG] = v;
					if (a == 'M')
						newf.action[FLD_MSG] = a;
					else
						newf.action[FLD_MSG] = 'm';
					newf.remove_after[FLD_MSG] = r;
					break;
				case 'n':
					newf.limit[FLD_NICK] = v;
					newf.action[FLD_NICK] = 'N';
					newf.remove_after[FLD_NICK] = r;
					break;
				case 't':
					newf.limit[FLD_TEXT] = v;
					if (a == 'b' || a == 'd')
						newf.action[FLD_TEXT] = a;
					if (timedban_available)
						newf.remove_after[FLD_TEXT] = r;
					break;
				case 'r':
					newf.limit[FLD_REPEAT] = v;
					if (a == 'b' || a == 'd')
						newf.action[FLD_REPEAT] = a;
					if (timedban_available)
						newf.remove_after[FLD_REPEAT] = r;
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
			if (localclient || (v < 1))
				return NULL;
		}
		newf.per = v;

		/* Is anything turned on? (to stop things like '+f []:15' */
		breakit = 1;
		for (v=0; v < NUMFLD; v++)
			if (newf.limit[v])
				breakit=0;
		if (breakit)
			return NULL;

	} /* if param[0] == '[' */

	channel_modef_string(&newf, retbuf);
	return retbuf;
}

void cmodef_free_param(void *r)
{
	// TODO: consider cancelling timers just to e sure? or maybe in DEBUGMODE?
	MyFree(r);
}

void *cmodef_dup_struct(void *r_in)
{
	ChannelFloodProtection *r = (ChannelFloodProtection *)r_in;
	ChannelFloodProtection *w = MyMallocEx(sizeof(ChannelFloodProtection));

	memcpy(w, r, sizeof(ChannelFloodProtection));
	return (void *)w;
}

int cmodef_sjoin_check(Channel *chptr, void *ourx, void *theirx)
{
	ChannelFloodProtection *our = (ChannelFloodProtection *)ourx;
	ChannelFloodProtection *their = (ChannelFloodProtection *)theirx;
	char *x;
	int i;

	if (compare_floodprot_modes(our, their) == 0)
		return EXSJ_SAME;

	our->per = MAX(our->per, their->per);
	for (i=0; i < NUMFLD; i++)
	{
		our->limit[i] = MAX(our->limit[i], their->limit[i]);
		our->action[i] = MAX(our->action[i], their->action[i]);
		our->remove_after[i] = MAX(our->remove_after[i], their->remove_after[i]);
	}

	return EXSJ_MERGE;
}

int floodprot_join(Client *cptr, Client *sptr, Channel *chptr, MessageTag *mtags, char *parv[])
{
	/* I'll explain this only once:
	 * 1. if channel is +f
	 * 2. local client OR synced server
	 * 3. server uptime more than XX seconds (if this information is available)
	 * 4. is not a uline
	 * 5. then, increase floodcounter
	 * 6. if we reached the limit AND only if source was a local client.. do the action (+i).
	 * Nr 6 is done because otherwise you would have a noticeflood with 'joinflood detected'
	 * from all servers.
	 */
	if (IsFloodLimit(chptr) &&
	    (MyClient(sptr) || sptr->srvptr->serv->flags.synced) &&
	    (sptr->srvptr->serv->boottime && (TStime() - sptr->srvptr->serv->boottime >= MODEF_BOOT_DELAY)) &&
	    !IsULine(sptr) &&
	    do_floodprot(chptr, FLD_JOIN) &&
	    MyClient(sptr))
	{
		do_floodprot_action(chptr, FLD_JOIN, "join");
	}
	return 0;
}

int cmodef_cleanup_user2(Client *sptr)
{
	return 0;
}

int cmodef_channel_destroy(Channel *chptr, int *should_destroy)
{
	floodprottimer_stopchantimers(chptr);
	return 0;
}

/* [just a helper for channel_modef_string()] */
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r)
{
char *p;
char tmpbuf[16], *p2 = tmpbuf;

	sprintf(buf, "%hd", l);
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

/** returns the channelmode +f string (ie: '[5k,40j]:10').
 * 'retbuf' is suggested to be of size 512, which is more than X times the maximum (for safety).
 */
char *channel_modef_string(ChannelFloodProtection *x, char *retbuf)
{
	char *p = retbuf;

	*p++ = '[';

	/* (alphabetized) */
	if (x->limit[FLD_CTCP])
		p = chmodefstrhelper(p, 'c', 'C', x->limit[FLD_CTCP], x->action[FLD_CTCP], x->remove_after[FLD_CTCP]);
	if (x->limit[FLD_JOIN])
		p = chmodefstrhelper(p, 'j', 'i', x->limit[FLD_JOIN], x->action[FLD_JOIN], x->remove_after[FLD_JOIN]);
	if (x->limit[FLD_KNOCK])
		p = chmodefstrhelper(p, 'k', 'K', x->limit[FLD_KNOCK], x->action[FLD_KNOCK], x->remove_after[FLD_KNOCK]);
	if (x->limit[FLD_MSG])
		p = chmodefstrhelper(p, 'm', 'm', x->limit[FLD_MSG], x->action[FLD_MSG], x->remove_after[FLD_MSG]);
	if (x->limit[FLD_NICK])
		p = chmodefstrhelper(p, 'n', 'N', x->limit[FLD_NICK], x->action[FLD_NICK], x->remove_after[FLD_NICK]);
	if (x->limit[FLD_TEXT])
		p = chmodefstrhelper(p, 't', '\0', x->limit[FLD_TEXT], x->action[FLD_TEXT], x->remove_after[FLD_TEXT]);
	if (x->limit[FLD_REPEAT])
		p = chmodefstrhelper(p, 'r', '\0', x->limit[FLD_REPEAT], x->action[FLD_REPEAT], x->remove_after[FLD_REPEAT]);

	if (*(p - 1) == ',')
		p--;
	*p++ = ']';
	sprintf(p, ":%hd", x->per);
	return retbuf;
}

char *floodprot_pre_chanmsg(Client *sptr, Channel *chptr, MessageTag *mtags, char *text, int notice)
{
	if (MyClient(sptr) && (check_for_chan_flood(sptr, chptr, text) == 1))
		return NULL; /* don't send it */
	return text;
}

int floodprot_post_chanmsg(Client *sptr, Channel *chptr, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice)
{
	if (!IsFloodLimit(chptr) || is_skochanop(sptr, chptr) || IsULine(sptr))
		return 0;

	/* HINT: don't be so stupid to reorder the items in the if's below.. you'll break things -- Syzop. */

	if (do_floodprot(chptr, FLD_MSG) && MyClient(sptr))
		do_floodprot_action(chptr, FLD_MSG, "msg/notice");

	if ((text[0] == '\001') && strncmp(text+1, "ACTION ", 7) &&
	    do_floodprot(chptr, FLD_CTCP) && MyClient(sptr))
	{
		do_floodprot_action(chptr, FLD_CTCP, "CTCP");
	}

	return 0;
}

int floodprot_knock(Client *sptr, Channel *chptr, MessageTag *mtags, char *comment)
{
	if (IsFloodLimit(chptr) && !IsULine(sptr) && do_floodprot(chptr, FLD_KNOCK) && MyClient(sptr))
		do_floodprot_action(chptr, FLD_KNOCK, "knock");
	return 0;
}

static int gotnickchange(Client *sptr)
{
Membership *mp;

	for (mp = sptr->user->channel; mp; mp = mp->next)
	{
		Channel *chptr = mp->chptr;
		if (chptr && IsFloodLimit(chptr) &&
		    !(mp->flags & (CHFL_CHANOP|CHFL_VOICE|CHFL_CHANOWNER|CHFL_HALFOP|CHFL_CHANADMIN)) &&
		    do_floodprot(chptr, FLD_NICK) && MyClient(sptr))
		{
			do_floodprot_action(chptr, FLD_NICK, "nick");
		}
	}
	return 0;
}

int floodprot_local_nickchange(Client *sptr, char *oldnick)
{
	if (IsULine(sptr))
		return 0;
	return gotnickchange(sptr);
}

int floodprot_remote_nickchange(Client *cptr, Client *sptr, char *oldnick)
{
	if (IsULine(sptr))
		return 0;
	return gotnickchange(sptr);
}

int floodprot_chanmode_del(Channel *chptr, int modechar)
{
	ChannelFloodProtection *chp;

	if (!IsFloodLimit(chptr))
		return 0;

	chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');
	if (!chp)
		return 0;

	/* reset joinflood on -i, reset msgflood on -m, etc.. */
	switch(modechar)
	{
		case 'C':
			chp->counter[FLD_CTCP] = 0;
			break;
		case 'N':
			chp->counter[FLD_NICK] = 0;
			break;
		case 'm':
			chp->counter[FLD_MSG] = 0;
			chp->counter[FLD_CTCP] = 0;
			break;
		case 'K':
			chp->counter[FLD_KNOCK] = 0;
			break;
		case 'i':
			chp->counter[FLD_JOIN] = 0;
			break;
		case 'M':
			chp->counter[FLD_MSG] = 0;
			chp->counter[FLD_CTCP] = 0;
			break;
		case 'R':
			chp->counter[FLD_JOIN] = 0;
			break;
		default:
			break;
	}
	floodprottimer_del(chptr, modechar);
	return 0;
}

int check_for_chan_flood(Client *sptr, Channel *chptr, char *text)
{
	MembershipL *lp;
	ChannelFloodProtection *chp;
	MemberFlood *memberflood;
	uint64_t msghash;
	unsigned char is_flooding_text=0, is_flooding_repeat=0;

	if (ValidatePermissionsForPath("channel:override:flood",sptr,NULL,chptr,NULL) || !IsFloodLimit(chptr) || is_skochanop(sptr, chptr))
		return 0;

	if (!(lp = (MembershipL *)find_membership_link(sptr->user->channel, chptr)))
		return 0; /* not in channel */

	chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');

	if (!chp || !(chp->limit[FLD_TEXT] || chp->limit[FLD_REPEAT]))
		return 0;

	if (moddata_membership(lp, mdflood).ptr == NULL)
	{
		/* Alloc a new entry if it doesn't exist yet */
		moddata_membership(lp, mdflood).ptr = MyMallocEx(sizeof(MemberFlood));
	}

	memberflood = (MemberFlood *)moddata_membership(lp, mdflood).ptr;

	/* Anti-repeat ('r') */
	if (chp->limit[FLD_REPEAT])
	{
		/* if current - firstmsgtime >= mode.per, then reset,
		 * if nummsg > mode.msgs then kick/ban
		 */
		if ((TStime() - memberflood->firstmsg) >= chp->per)
		{
			/* reset */
			memberflood->firstmsg = TStime();
			memberflood->nmsg = 1;
			memberflood->nmsg_repeat = 1;
			memberflood->lastmsg = gen_floodprot_msghash(text);
			memberflood->prevmsg = 0;
			return 0; /* forget about it.. */
		}

		msghash = gen_floodprot_msghash(text);
		if (memberflood->lastmsg)
		{
			if ((memberflood->lastmsg == msghash) || (memberflood->prevmsg == msghash))
			{
				memberflood->nmsg_repeat++;
				if (memberflood->nmsg_repeat > chp->limit[FLD_REPEAT])
					is_flooding_repeat = 1;
			}
			memberflood->prevmsg = memberflood->lastmsg;
		}
		memberflood->lastmsg = msghash;
	}

	if (chp->limit[FLD_TEXT])
	{
		/* increase msgs */
		memberflood->nmsg++;
		if (memberflood->nmsg > chp->limit[FLD_TEXT])
			is_flooding_text = 1;
	}

	/* Do we need to take any action? */
	if (is_flooding_text || is_flooding_repeat)
	{
		char comment[256], mask[256];
		MessageTag *mtags;
		int flood_type;

		/* Repeat takes precedence over text flood */
		if (is_flooding_repeat)
		{
			snprintf(comment, sizeof(comment), "Flooding (Your last message is too similar to previous ones)");
			flood_type = FLD_REPEAT;
		} else
		{
			snprintf(comment, sizeof(comment), "Flooding (Limit is %i lines per %i seconds)", chp->limit[FLD_TEXT], chp->per);
			flood_type = FLD_TEXT;
		}

		if (chp->action[flood_type] == 'd')
		{
			/* Drop the message */
			sendnumeric(sptr, ERR_CANNOTSENDTOCHAN, chptr->chname, comment, chptr->chname);
			return 1;
		}

		if (chp->action[flood_type] == 'b')
		{
			/* Ban the user */
			if (timedban_available && (chp->remove_after[flood_type] > 0))
				snprintf(mask, sizeof(mask), "~t:%d:*!*@%s", chp->remove_after[flood_type], GetHost(sptr));
			else
				snprintf(mask, sizeof(mask), "*!*@%s", GetHost(sptr));
			if (add_listmode(&chptr->banlist, &me, chptr, mask) == 0)
			{
				mtags = NULL;
				new_message(&me, NULL, &mtags);
				sendto_server(&me, 0, 0, mtags, ":%s MODE %s +b %s 0",
				    me.name, chptr->chname, mask);
				sendto_channel(chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				    ":%s MODE %s +b %s", me.name, chptr->chname, mask);
				free_message_tags(mtags);
			} /* else.. ban list is full */
		}
		mtags = NULL;
		new_message(&me, NULL, &mtags);
		sendto_channel(chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags,
		    ":%s KICK %s %s :%s", me.name,
		    chptr->chname, sptr->name, comment);
		sendto_server(NULL, 0, 0, mtags, ":%s KICK %s %s :%s",
		   me.name, chptr->chname, sptr->name, comment);
		free_message_tags(mtags);
		remove_user_from_channel(sptr, chptr);
		return 1;
	}
	return 0;
}

RemoveChannelModeTimer *floodprottimer_find(Channel *chptr, char mflag)
{
	RemoveChannelModeTimer *e;

	for (e=removechannelmodetimer_list; e; e=e->next)
	{
		if ((e->chptr == chptr) && (e->m == mflag))
			return e;
	}
	return NULL;
}

/** strcat-like */
void strccat(char *s, char c)
{
	for (; *s; s++);
	*s++ = c;
	*s++ = '\0';
}

/*
 * Adds a "remove channelmode set by +f" timer.
 * chptr	Channel
 * mflag	Mode flag, eg 'C'
 * when		when it should be removed
 * NOTES:
 * - This function takes care of overwriting of any previous timer
 *   for the same modechar.
 * - The function takes care of chptr->mode.floodprot->timers_running,
 *   do not modify it yourself.
 * - chptr->mode.floodprot is asumed to be non-NULL.
 */
void floodprottimer_add(Channel *chptr, char mflag, time_t when)
{
	RemoveChannelModeTimer *e = NULL;
	unsigned char add=1;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');

	if (strchr(chp->timers_running, mflag))
	{
		/* Already exists... */
		e = floodprottimer_find(chptr, mflag);
		if (e)
			add = 0;
	}

	if (!strchr(chp->timers_running, mflag))
	{
		char *p;
		if (strlen(chp->timers_running)+1 >= sizeof(chp->timers_running))
		{
			sendto_realops_and_log("floodprottimer_add: too many timers running for %s (%s)!!!",
				chptr->chname, chp->timers_running);
			return;
		}
		strccat(chp->timers_running, mflag); /* bounds already checked ^^ */
	}

	if (add)
		e = MyMallocEx(sizeof(RemoveChannelModeTimer));

	e->chptr = chptr;
	e->m = mflag;
	e->when = when;

	if (add)
		AddListItem(e, removechannelmodetimer_list);
}

void floodprottimer_del(Channel *chptr, char mflag)
{
	RemoveChannelModeTimer *e;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');

	if (chp && !strchr(chp->timers_running, mflag))
		return; /* nothing to remove.. */
	e = floodprottimer_find(chptr, mflag);
	if (!e)
		return;

	DelListItem(e, removechannelmodetimer_list);
	MyFree(e);

	if (chp)
        {
                char newtf[MAXCHMODEFACTIONS+1];
                char *i, *o;
                for (i=chp->timers_running, o=newtf; *i; i++)
                        if (*i != mflag)
                                *o++ = *i;
                *o = '\0';
                strcpy(chp->timers_running, newtf); /* always shorter (or equal) */
        }
}

EVENT(modef_event)
{
	RemoveChannelModeTimer *e, *e_next;
	time_t now;

	now = TStime();

	for (e = removechannelmodetimer_list; e; e = e_next)
	{
		e_next = e->next;
		if (e->when <= now)
		{
			/* Remove chanmode... */
			long mode = 0;
			Cmode_t extmode = 0;
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c EXPIRED", e->chptr->chname, e->m);
#endif
			mode = get_mode_bitbychar(e->m);
			if (mode == 0)
			        extmode = get_extmode_bitbychar(e->m);

			if ((mode && (e->chptr->mode.mode & mode)) ||
			    (extmode && (e->chptr->mode.extmode & extmode)))
			{
				MessageTag *mtags = NULL;

				new_message(&me, NULL, &mtags);
				sendto_server(&me, 0, 0, mtags, ":%s MODE %s -%c 0", me.name, e->chptr->chname, e->m);
				sendto_channel(e->chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s MODE %s -%c",
				               me.name, e->chptr->chname, e->m);
				free_message_tags(mtags);

				e->chptr->mode.mode &= ~mode;
				e->chptr->mode.extmode &= ~extmode;
			}

			/* And delete... */
			DelListItem(e, removechannelmodetimer_list);
			MyFree(e);
		} else {
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c about %d seconds",
				e->chptr->chname, e->m, e->when - now);
#endif
		}
	}
}

void floodprottimer_stopchantimers(Channel *chptr)
{
	RemoveChannelModeTimer *e, *e_next;

	for (e = removechannelmodetimer_list; e; e = e_next)
	{
		e_next = e->next;
		if (e->chptr == chptr)
		{
			DelListItem(e, removechannelmodetimer_list);
			MyFree(e);
		}
	}
}

int do_floodprot(Channel *chptr, int what)
{
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');

	if (!chp || !chp->limit[what]) /* no +f or not restricted */
		return 0;
	if (TStime() - chp->timer[what] >= chp->per)
	{
		chp->timer[what] = TStime();
		chp->counter[what] = 1;
	} else
	{
		chp->counter[what]++;
		if ((chp->counter[what] > chp->limit[what]) &&
		    (TStime() - chp->timer[what] < chp->per))
		{
			/* reset it too (makes it easier for chanops to handle the situation) */
			/*
			 *XXchp->timer[what] = TStime();
			 *XXchp->counter[what] = 1;
			 *
			 * BAD.. there are some situations where we might 'miss' a flood
			 * because of this. The reset has been moved to -i,-m,-N,-C,etc.
			*/
			return 1; /* flood detected! */
		}
	}
	return 0;
}

void do_floodprot_action(Channel *chptr, int what, char *text)
{
	char m;
	int mode = 0;
	Cmode_t extmode = 0;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(chptr, 'f');

	m = chp->action[what];
	if (!m)
		return;

	/* For drop action we don't actually have to do anything here, but we still have to prevent Unreal
	 * from setting chmode +d (which is useless against floods anyways) =]
	 */
	if (chp->action[what] == 'd')
		return;

	mode = get_mode_bitbychar(m);
	if (mode == 0)
		extmode = get_extmode_bitbychar(m);

	if (!mode && !extmode)
		return;

	if (!(mode && (chptr->mode.mode & mode)) &&
		!(extmode && (chptr->mode.extmode & extmode)))
	{
		char comment[512], target[CHANNELLEN + 8];
		MessageTag *mtags;

		/* First the notice to the chanops */
		mtags = NULL;
		new_message(&me, NULL, &mtags);
		ircsnprintf(comment, sizeof(comment), "*** Channel %sflood detected (limit is %d per %d seconds), setting mode +%c",
			text, chp->limit[what], chp->per, m);
		ircsnprintf(target, sizeof(target), "%%%s", chptr->chname);
		sendto_channel(chptr, &me, NULL, PREFIX_HALFOP|PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		               0, SEND_ALL, mtags,
		               ":%s NOTICE %s :%s", me.name, target, comment);
		free_message_tags(mtags);

		/* Then the MODE broadcast */
		mtags = NULL;
		new_message(&me, NULL, &mtags);
		sendto_server(&me, 0, 0, mtags, ":%s MODE %s +%c 0", me.name, chptr->chname, m);
		sendto_channel(chptr, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +%c", me.name, chptr->chname, m);
		free_message_tags(mtags);

		/* Actually set the mode internally */
		chptr->mode.mode |= mode;
		chptr->mode.extmode |= extmode;

		/* Add remove-chanmode timer */
		if (chp->remove_after[what])
		{
			floodprottimer_add(chptr, m, TStime() + ((long)chp->remove_after[what] * 60) - 5);
			/* (since the floodprot timer event is called every 10s, we do -5 here so the accurancy will
			 *  be -5..+5, without it it would be 0..+10.)
			 */
		}
	}
}

uint64_t gen_floodprot_msghash(char *text)
{
	int i;
	int is_ctcp, is_action;
	char *plaintext;
	size_t len;
	SHA256_CTX ckctx;
	unsigned char binaryhash[SHA256_DIGEST_LENGTH];
	static char msghash[256];

	is_ctcp = is_action = 0;
	// Remove any control chars (colours/bold/CTCP/etc) and convert it to lowercase before hashing it
	if (text[0] == '\001')
	{
		if (!strncmp(text + 1, "ACTION ", 7))
			is_action = 1;
		else
			is_ctcp = 1;
	}
	plaintext = (char *)StripControlCodes(text);
	for (i = 0; plaintext[i]; i++)
	{
		// Don't need to bother with non-printables and various symbols and numbers
		if (plaintext[i] > 64)
			plaintext[i] = tolower(plaintext[i]);
	}
	if (is_ctcp || is_action)
	{
		// Remove the \001 chars around the message
		if((len = strlen(plaintext)) && plaintext[len - 1] == '\001')
			plaintext[len - 1] = '\0';
		plaintext++;
		if(is_action)
			plaintext += 7;
	}

	return siphash(text, floodprot_msghash_key);
}

// FIXME: REMARK: make sure you can only do a +f/-f once (latest in line wins).

/* Checks if 2 ChannelFloodProtection modes (chmode +f) are different.
 * This is a bit more complicated than 1 simple memcmp(a,b,..) because
 * counters are also stored in this struct so we have to do
 * it manually :( -- Syzop.
 */
static int compare_floodprot_modes(ChannelFloodProtection *a, ChannelFloodProtection *b)
{
	if (memcmp(a->limit, b->limit, sizeof(a->limit)) ||
	    memcmp(a->action, b->action, sizeof(a->action)) ||
	    memcmp(a->remove_after, b->remove_after, sizeof(a->remove_after)))
		return 1;
	else
		return 0;
}

void memberflood_free(ModData *md)
{
	MemberFlood *memberflood;
	/* We don't have any struct members (anymore) that need freeing */
	safefree(md->ptr);
}

int floodprot_stats(Client *sptr, char *flag)
{
	sendtxtnumeric(sptr, "modef-default-unsettime: %hd", (unsigned short)MODEF_DEFAULT_UNSETTIME);
	sendtxtnumeric(sptr, "modef-max-unsettime: %hd", (unsigned short)MODEF_MAX_UNSETTIME);
	return 0;
}

/** Admin unloading the floodprot module for good. Bad. */
void floodprot_free_removechannelmodetimer_list(ModData *m)
{
	RemoveChannelModeTimer *e, *e_next;

	for (e=removechannelmodetimer_list; e; e=e_next)
	{
		e_next = e->next;
		MyFree(e);
	}
}

void floodprot_free_msghash_key(ModData *m)
{
	safefree(floodprot_msghash_key);
}
