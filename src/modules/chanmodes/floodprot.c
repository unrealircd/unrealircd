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

ModuleHeader MOD_HEADER
  = {
	"chanmodes/floodprot",
	"5.0",
	"Channel Mode +f",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

#define CHFLD_CTCP	0 /* c */
#define CHFLD_JOIN	1 /* j */
#define CHFLD_KNOCK	2 /* k */
#define CHFLD_MSG		3 /* m */
#define CHFLD_NICK	4 /* n */
#define CHFLD_TEXT	5 /* t */
#define CHFLD_REPEAT	6 /* r */

#define NUMFLD	7 /* 7 flood types */

/** Configuration settings */
struct {
	unsigned char modef_default_unsettime;
	unsigned char modef_max_unsettime;
	long modef_boot_delay;
} cfg;

typedef struct FloodType {
	char letter;
	int index;
	char *description;
	char default_action;
	char *actions;
	int timedban_required;
} FloodType;

/* All the floodtypes that are tracked.
 * IMPORTANT: MUST be in alphabetic order!!
 */
FloodType floodtypes[] = {
	{ 'c', CHFLD_CTCP,	"CTCPflood",		'C',	"mM",	0, },
	{ 'j', CHFLD_JOIN,	"joinflood",		'i',	"R",	0, },
	{ 'k', CHFLD_KNOCK,	"knockflood",		'K',	"",	0, },
	{ 'm', CHFLD_MSG,		"msg/noticeflood",	'm',	"M",	0, },
	{ 'n', CHFLD_NICK,	"nickflood",		'N',	"",	0, },
	{ 't', CHFLD_TEXT,	"msg/noticeflood",	'\0',	"bd",	1, },
	{ 'r', CHFLD_REPEAT,	"repeating",		'\0',	"bd",	1, },
};

#define MODEF_DEFAULT_UNSETTIME		cfg.modef_default_unsettime
#define MODEF_MAX_UNSETTIME		cfg.modef_max_unsettime
#define MODEF_BOOT_DELAY		cfg.modef_boot_delay

typedef struct ChannelFloodProtection ChannelFloodProtection;
typedef struct RemoveChannelModeTimer RemoveChannelModeTimer;

struct RemoveChannelModeTimer {
	struct RemoveChannelModeTimer *prev, *next;
	Channel *channel;
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
void floodprottimer_del(Channel *channel, char mflag);
void floodprottimer_stopchantimers(Channel *channel);
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r);
static int compare_floodprot_modes(ChannelFloodProtection *a, ChannelFloodProtection *b);
static int do_floodprot(Channel *channel, Client *client, int what);
char *channel_modef_string(ChannelFloodProtection *x, char *str);
void do_floodprot_action(Channel *channel, int what);
void floodprottimer_add(Channel *channel, char mflag, time_t when);
uint64_t gen_floodprot_msghash(char *text);
int cmodef_is_ok(Client *client, Channel *channel, char mode, char *para, int type, int what);
void *cmodef_put_param(void *r_in, char *param);
char *cmodef_get_param(void *r_in);
char *cmodef_conv_param(char *param_in, Client *client, Channel *channel);
void cmodef_free_param(void *r);
void *cmodef_dup_struct(void *r_in);
int cmodef_sjoin_check(Channel *channel, void *ourx, void *theirx);
int floodprot_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);
EVENT(modef_event);
int cmodef_channel_destroy(Channel *channel, int *should_destroy);
int floodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, char **msg, char **errmsg, SendType sendtype);
int floodprot_post_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, SendType sendtype);
int floodprot_knock(Client *client, Channel *channel, MessageTag *mtags, char *comment);
int floodprot_nickchange(Client *client, MessageTag *mtags, char *oldnick);
int floodprot_chanmode_del(Channel *channel, int m);
void memberflood_free(ModData *md);
int floodprot_stats(Client *client, char *flag);
void floodprot_free_removechannelmodetimer_list(ModData *m);
void floodprot_free_msghash_key(ModData *m);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, floodprot_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
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
		floodprot_msghash_key = safe_alloc(16);
		siphash_generate_key(floodprot_msghash_key);
	}

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, floodprot_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, floodprot_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, floodprot_post_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_KNOCK, 0, floodprot_knock);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, floodprot_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, floodprot_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_MODECHAR_DEL, 0, floodprot_chanmode_del);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 0, cmodef_channel_destroy);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH_COMPLETE, 0, floodprot_rehash_complete);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, floodprot_stats);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "modef_event", modef_event, NULL, 10000, 0);
	floodprot_rehash_complete();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, removechannelmodetimer_list);
	SavePersistentPointer(modinfo, floodprot_msghash_key);
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

FloodType *find_floodprot_by_letter(char c)
{
	int i;
	for (i=0; i < ARRAY_SIZEOF(floodtypes); i++)
		if (floodtypes[i].letter == c)
			return &floodtypes[i];

	return NULL;
}

FloodType *find_floodprot_by_index(int index)
{
	int i;
	for (i=0; i < ARRAY_SIZEOF(floodtypes); i++)
		if (floodtypes[i].index == index)
			return &floodtypes[i];

	return NULL;
}

int cmodef_is_ok(Client *client, Channel *channel, char mode, char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsUser(client) && is_chan_op(client, channel))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(client, ERR_NOTFORHALFOPS, 'f');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		ChannelFloodProtection newf;
		char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
		int v;
		unsigned short warnings = 0, breakit;
		unsigned char r;
		FloodType *floodtype;
		int index;

		memset(&newf, 0, sizeof(newf));

		/* old +f was like +f 10:5 or +f *10:5 - no longer supported */
		if ((param[0] != '[') || strlen(param) < 3)
			goto invalidsyntax;;

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
			c = *p;
			floodtype = find_floodprot_by_letter(c);
			if (!floodtype)
			{
				if (MyUser(client) && *p && (warnings++ < 3))
					sendnotice(client, "warning: channelmode +f: floodtype '%c' unknown, ignored.", *p);
				continue; /* continue instead of break for forward compatability. */
			}
			*p = '\0';
			v = atoi(x);
			if ((v < 1) || (v > 999)) /* out of range... */
			{
				if (MyUser(client))
				{
					sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'f', "value should be from 1-999");
					goto invalidsyntax;
				} else
					continue; /* just ignore for remote servers */
			}
			p++;
			a = '\0';
			r = MyUser(client) ? MODEF_DEFAULT_UNSETTIME : 0;
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
						if (tv > (MyUser(client) ? MODEF_MAX_UNSETTIME : 255))
							tv = (MyUser(client) ? MODEF_MAX_UNSETTIME : 255); /* set to max */
						r = (unsigned char)tv;
					}
				}
			}

			index = floodtype->index;
			newf.limit[index] = v;
			if (a && strchr(floodtype->actions, a))
				newf.action[index] = a;
			else
				newf.action[index] = floodtype->default_action;
			if (!floodtype->timedban_required || (floodtype->timedban_required && timedban_available))
				newf.remove_after[index] = r;
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
			if (MyUser(client))
				sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'f', "time range should be 1-999");
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

		return EX_ALLOW;
invalidsyntax:
		sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'f', "Invalid syntax for MODE +f");
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
	unsigned short breakit;
	unsigned char r;
	FloodType *floodtype;
	int index;

	strlcpy(xbuf, param, sizeof(xbuf));

	if (!fld)
		fld = safe_alloc(sizeof(ChannelFloodProtection));

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
		goto fail_cmodef_put_param; /* FAIL */
	*p2 = '\0';
	if (*(p2+1) != ':')
		goto fail_cmodef_put_param; /* FAIL */

	breakit = 0;
	for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
	{
		/* <number><1 letter>[optional: '#'+1 letter] */
		p = x;
		while(isdigit(*p)) { p++; }
		c = *p;
		floodtype = find_floodprot_by_letter(c);
		if (!floodtype)
			continue; /* continue instead of break for forward compatability. */
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

		index = floodtype->index;
		fld->limit[index] = v;
		if (a && strchr(floodtype->actions, a))
			fld->action[index] = a;
		else
			fld->action[index] = floodtype->default_action;
		if (!floodtype->timedban_required || (floodtype->timedban_required && timedban_available))
			fld->remove_after[index] = r;
	} /* for */

	/* parse 'per' */
	p2++;
	if (*p2 != ':')
		goto fail_cmodef_put_param; /* FAIL */
	p2++;
	if (!*p2)
		goto fail_cmodef_put_param; /* FAIL */
	v = atoi(p2);
	if (v < 1)
		v = 1;

	/* If new 'per xxx seconds' is smaller than current 'per' then reset timers/counters (t, c) */
	if (v < fld->per)
	{
		int i;
		for (i=0; i < NUMFLD; i++)
		{
			fld->timer[i] = 0;
			fld->counter[i] = 0;
		}
	}
	fld->per = v;

	/* Is anything turned on? (to stop things like '+f []:15' */
	breakit = 1;
	for (v=0; v < NUMFLD; v++)
		if (fld->limit[v])
			breakit=0;
	if (breakit)
		goto fail_cmodef_put_param; /* FAIL */

	return (void *)fld;

fail_cmodef_put_param:
	memset(fld, 0, sizeof(ChannelFloodProtection));
	return fld; /* FAIL */
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
 * NOTE: client may be NULL if called for e.g. set::modes-on-join
 */
char *cmodef_conv_param(char *param_in, Client *client, Channel *channel)
{
	static char retbuf[256];
	char param[256];
	ChannelFloodProtection newf;
	int localclient = (!client || MyUser(client)) ? 1 : 0;
	char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
	int v;
	unsigned short breakit;
	unsigned char r;
	FloodType *floodtype;
	int index;

	memset(&newf, 0, sizeof(newf));

	strlcpy(param, param_in, sizeof(param));

	/* old +f was like +f 10:5 or +f *10:5 - no longer supported */
	if (param[0] != '[')
		return NULL;

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
		c = *p;
		floodtype = find_floodprot_by_letter(c);
		if (!floodtype)
			continue; /* continue instead of break for forward compatability. */
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

		index = floodtype->index;
		newf.limit[index] = v;
		if (a && strchr(floodtype->actions, a))
			newf.action[index] = a;
		else
			newf.action[index] = floodtype->default_action;
		if (!floodtype->timedban_required || (floodtype->timedban_required && timedban_available))
			newf.remove_after[index] = r;
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

	channel_modef_string(&newf, retbuf);
	return retbuf;
}

void cmodef_free_param(void *r)
{
	// TODO: consider cancelling timers just to e sure? or maybe in DEBUGMODE?
	safe_free(r);
}

void *cmodef_dup_struct(void *r_in)
{
	ChannelFloodProtection *r = (ChannelFloodProtection *)r_in;
	ChannelFloodProtection *w = safe_alloc(sizeof(ChannelFloodProtection));

	memcpy(w, r, sizeof(ChannelFloodProtection));
	return (void *)w;
}

int cmodef_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	ChannelFloodProtection *our = (ChannelFloodProtection *)ourx;
	ChannelFloodProtection *their = (ChannelFloodProtection *)theirx;
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

int floodprot_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[])
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
	if (IsFloodLimit(channel) &&
	    (MyUser(client) || client->srvptr->serv->flags.synced) &&
	    (client->srvptr->serv->boottime && (TStime() - client->srvptr->serv->boottime >= MODEF_BOOT_DELAY)) &&
	    !IsULine(client))
	{
	    do_floodprot(channel, client, CHFLD_JOIN);
	}
	return 0;
}

int cmodef_cleanup_user2(Client *client)
{
	return 0;
}

int cmodef_channel_destroy(Channel *channel, int *should_destroy)
{
	floodprottimer_stopchantimers(channel);
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
	int i;
	char *p = retbuf;
	FloodType *f;

	*p++ = '[';

	for (i=0; i < ARRAY_SIZEOF(floodtypes); i++)
	{
		f = &floodtypes[i];
		if (x->limit[f->index])
			p = chmodefstrhelper(p, f->letter, f->default_action, x->limit[f->index], x->action[f->index], x->remove_after[f->index]);
	}

	if (*(p - 1) == ',')
		p--;
	*p++ = ']';
	sprintf(p, ":%hd", x->per);
	return retbuf;
}

int floodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, char **msg, char **errmsg, SendType sendtype)
{
	Membership *mb;
	ChannelFloodProtection *chp;
	MemberFlood *memberflood;
	uint64_t msghash;
	unsigned char is_flooding_text=0, is_flooding_repeat=0;
	static char errbuf[256];

	/* This is redundant, right? */
	if (!MyUser(client))
		return HOOK_CONTINUE;

	if (sendtype == SEND_TYPE_TAGMSG)
		return 0; // TODO: some TAGMSG specific limit? (1 of 2)

	if (ValidatePermissionsForPath("channel:override:flood",client,NULL,channel,NULL) || !IsFloodLimit(channel) || is_skochanop(client, channel))
		return HOOK_CONTINUE;

	if (!(mb = find_membership_link(client->user->channel, channel)))
		return HOOK_CONTINUE; /* not in channel */

	chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');

	if (!chp || !(chp->limit[CHFLD_TEXT] || chp->limit[CHFLD_REPEAT]))
		return HOOK_CONTINUE;

	if (moddata_membership(mb, mdflood).ptr == NULL)
	{
		/* Alloc a new entry if it doesn't exist yet */
		moddata_membership(mb, mdflood).ptr = safe_alloc(sizeof(MemberFlood));
	}

	memberflood = (MemberFlood *)moddata_membership(mb, mdflood).ptr;

	if ((TStime() - memberflood->firstmsg) >= chp->per)
	{
		/* Reset due to moving into a new time slot */
		memberflood->firstmsg = TStime();
		memberflood->nmsg = 1;
		memberflood->nmsg_repeat = 1;
		if (chp->limit[CHFLD_REPEAT])
		{
			memberflood->lastmsg = gen_floodprot_msghash(*msg);
			memberflood->prevmsg = 0;
		}
		return HOOK_CONTINUE; /* forget about it.. */
	}

	/* Anti-repeat ('r') */
	if (chp->limit[CHFLD_REPEAT])
	{
		msghash = gen_floodprot_msghash(*msg);
		if (memberflood->lastmsg)
		{
			if ((memberflood->lastmsg == msghash) || (memberflood->prevmsg == msghash))
			{
				memberflood->nmsg_repeat++;
				if (memberflood->nmsg_repeat > chp->limit[CHFLD_REPEAT])
					is_flooding_repeat = 1;
			}
			memberflood->prevmsg = memberflood->lastmsg;
		}
		memberflood->lastmsg = msghash;
	}

	if (chp->limit[CHFLD_TEXT])
	{
		/* increase msgs */
		memberflood->nmsg++;
		if (memberflood->nmsg > chp->limit[CHFLD_TEXT])
			is_flooding_text = 1;
	}

	/* Do we need to take any action? */
	if (is_flooding_text || is_flooding_repeat)
	{
		char mask[256];
		MessageTag *mtags;
		int flood_type;

		/* Repeat takes precedence over text flood */
		if (is_flooding_repeat)
		{
			snprintf(errbuf, sizeof(errbuf), "Flooding (Your last message is too similar to previous ones)");
			flood_type = CHFLD_REPEAT;
		} else
		{
			snprintf(errbuf, sizeof(errbuf), "Flooding (Limit is %i lines per %i seconds)", chp->limit[CHFLD_TEXT], chp->per);
			flood_type = CHFLD_TEXT;
		}

		if (chp->action[flood_type] == 'd')
		{
			/* Drop the message */
			*errmsg = errbuf;
			return HOOK_DENY;
		}

		if (chp->action[flood_type] == 'b')
		{
			/* Ban the user */
			if (timedban_available && (chp->remove_after[flood_type] > 0))
				snprintf(mask, sizeof(mask), "~t:%d:*!*@%s", chp->remove_after[flood_type], GetHost(client));
			else
				snprintf(mask, sizeof(mask), "*!*@%s", GetHost(client));
			if (add_listmode(&channel->banlist, &me, channel, mask) == 0)
			{
				mtags = NULL;
				new_message(&me, NULL, &mtags);
				sendto_server(NULL, 0, 0, mtags, ":%s MODE %s +b %s 0", me.id, channel->chname, mask);
				sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				    ":%s MODE %s +b %s", me.name, channel->chname, mask);
				free_message_tags(mtags);
			} /* else.. ban list is full */
		}
		mtags = NULL;
		kick_user(NULL, channel, &me, client, errbuf);
		*errmsg = errbuf; /* not used, but needs to be set */
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}

int floodprot_post_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, SendType sendtype)
{
	if (!IsFloodLimit(channel) || is_skochanop(client, channel) || IsULine(client))
		return 0;

	if (sendtype == SEND_TYPE_TAGMSG)
		return 0; // TODO: some TAGMSG specific limit? (2 of 2)

	/* HINT: don't be so stupid to reorder the items in the if's below.. you'll break things -- Syzop. */

	do_floodprot(channel, client, CHFLD_MSG);

	if ((text[0] == '\001') && strncmp(text+1, "ACTION ", 7))
		do_floodprot(channel, client, CHFLD_CTCP);

	return 0;
}

int floodprot_knock(Client *client, Channel *channel, MessageTag *mtags, char *comment)
{
	if (IsFloodLimit(channel) && !IsULine(client))
		do_floodprot(channel, client, CHFLD_KNOCK);
	return 0;
}

int floodprot_nickchange(Client *client, MessageTag *mtags, char *oldnick)
{
	Membership *mp;

	if (IsULine(client))
		return 0;

	for (mp = client->user->channel; mp; mp = mp->next)
	{
		Channel *channel = mp->channel;
		if (channel && IsFloodLimit(channel) &&
		    !(mp->flags & (CHFL_CHANOP|CHFL_VOICE|CHFL_CHANOWNER|CHFL_HALFOP|CHFL_CHANADMIN)))
		{
			do_floodprot(channel, client, CHFLD_NICK);
		}
	}
	return 0;
}

int floodprot_chanmode_del(Channel *channel, int modechar)
{
	ChannelFloodProtection *chp;

	if (!IsFloodLimit(channel))
		return 0;

	chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');
	if (!chp)
		return 0;

	/* reset joinflood on -i, reset msgflood on -m, etc.. */
	switch(modechar)
	{
		case 'C':
			chp->counter[CHFLD_CTCP] = 0;
			break;
		case 'N':
			chp->counter[CHFLD_NICK] = 0;
			break;
		case 'm':
			chp->counter[CHFLD_MSG] = 0;
			chp->counter[CHFLD_CTCP] = 0;
			break;
		case 'K':
			chp->counter[CHFLD_KNOCK] = 0;
			break;
		case 'i':
			chp->counter[CHFLD_JOIN] = 0;
			break;
		case 'M':
			chp->counter[CHFLD_MSG] = 0;
			chp->counter[CHFLD_CTCP] = 0;
			break;
		case 'R':
			chp->counter[CHFLD_JOIN] = 0;
			break;
		default:
			break;
	}
	floodprottimer_del(channel, modechar);
	return 0;
}

RemoveChannelModeTimer *floodprottimer_find(Channel *channel, char mflag)
{
	RemoveChannelModeTimer *e;

	for (e=removechannelmodetimer_list; e; e=e->next)
	{
		if ((e->channel == channel) && (e->m == mflag))
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
 * channel	Channel
 * mflag	Mode flag, eg 'C'
 * when		when it should be removed
 * NOTES:
 * - This function takes care of overwriting of any previous timer
 *   for the same modechar.
 * - The function takes care of channel->mode.floodprot->timers_running,
 *   do not modify it yourself.
 * - channel->mode.floodprot is asumed to be non-NULL.
 */
void floodprottimer_add(Channel *channel, char mflag, time_t when)
{
	RemoveChannelModeTimer *e = NULL;
	unsigned char add=1;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');

	if (strchr(chp->timers_running, mflag))
	{
		/* Already exists... */
		e = floodprottimer_find(channel, mflag);
		if (e)
			add = 0;
	}

	if (!strchr(chp->timers_running, mflag))
	{
		if (strlen(chp->timers_running)+1 >= sizeof(chp->timers_running))
		{
			sendto_realops_and_log("floodprottimer_add: too many timers running for %s (%s)!!!",
				channel->chname, chp->timers_running);
			return;
		}
		strccat(chp->timers_running, mflag); /* bounds already checked ^^ */
	}

	if (add)
		e = safe_alloc(sizeof(RemoveChannelModeTimer));

	e->channel = channel;
	e->m = mflag;
	e->when = when;

	if (add)
		AddListItem(e, removechannelmodetimer_list);
}

void floodprottimer_del(Channel *channel, char mflag)
{
	RemoveChannelModeTimer *e;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');

	if (chp && !strchr(chp->timers_running, mflag))
		return; /* nothing to remove.. */
	e = floodprottimer_find(channel, mflag);
	if (!e)
		return;

	DelListItem(e, removechannelmodetimer_list);
	safe_free(e);

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
			sendto_realops("modef_event: chan %s mode -%c EXPIRED", e->channel->chname, e->m);
#endif
			mode = get_mode_bitbychar(e->m);
			if (mode == 0)
			        extmode = get_extmode_bitbychar(e->m);

			if ((mode && (e->channel->mode.mode & mode)) ||
			    (extmode && (e->channel->mode.extmode & extmode)))
			{
				MessageTag *mtags = NULL;

				new_message(&me, NULL, &mtags);
				sendto_server(NULL, 0, 0, mtags, ":%s MODE %s -%c 0", me.id, e->channel->chname, e->m);
				sendto_channel(e->channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s MODE %s -%c",
				               me.name, e->channel->chname, e->m);
				free_message_tags(mtags);

				e->channel->mode.mode &= ~mode;
				e->channel->mode.extmode &= ~extmode;
			}

			/* And delete... */
			DelListItem(e, removechannelmodetimer_list);
			safe_free(e);
		} else {
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c about %d seconds",
				e->channel->chname, e->m, e->when - now);
#endif
		}
	}
}

void floodprottimer_stopchantimers(Channel *channel)
{
	RemoveChannelModeTimer *e, *e_next;

	for (e = removechannelmodetimer_list; e; e = e_next)
	{
		e_next = e->next;
		if (e->channel == channel)
		{
			DelListItem(e, removechannelmodetimer_list);
			safe_free(e);
		}
	}
}

int do_floodprot(Channel *channel, Client *client, int what)
{
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');

	if (!chp)
		return 0; /* no +f active */

	if (chp->limit[what])
	{
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
				if (MyUser(client))
					do_floodprot_action(channel, what);
				return 1; /* flood detected! */
			}
		}
	}
	return 0;
}

void do_floodprot_action(Channel *channel, int what)
{
	char m;
	int mode = 0;
	Cmode_t extmode = 0;
	ChannelFloodProtection *chp = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');
	FloodType *floodtype = find_floodprot_by_index(what);
	char *text;

	if (!floodtype)
		return;
	text = floodtype->description;

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

	if (!(mode && (channel->mode.mode & mode)) &&
		!(extmode && (channel->mode.extmode & extmode)))
	{
		char comment[512], target[CHANNELLEN + 8];
		MessageTag *mtags;

		/* First the notice to the chanops */
		mtags = NULL;
		new_message(&me, NULL, &mtags);
		ircsnprintf(comment, sizeof(comment), "*** Channel %s detected (limit is %d per %d seconds), setting mode +%c",
			text, chp->limit[what], chp->per, m);
		ircsnprintf(target, sizeof(target), "%%%s", channel->chname);
		sendto_channel(channel, &me, NULL, PREFIX_HALFOP|PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		               0, SEND_ALL, mtags,
		               ":%s NOTICE %s :%s", me.name, target, comment);
		free_message_tags(mtags);

		/* Then the MODE broadcast */
		mtags = NULL;
		new_message(&me, NULL, &mtags);
		sendto_server(NULL, 0, 0, mtags, ":%s MODE %s +%c 0", me.id, channel->chname, m);
		sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +%c", me.name, channel->chname, m);
		free_message_tags(mtags);

		/* Actually set the mode internally */
		channel->mode.mode |= mode;
		channel->mode.extmode |= extmode;

		/* Add remove-chanmode timer */
		if (chp->remove_after[what])
		{
			floodprottimer_add(channel, m, TStime() + ((long)chp->remove_after[what] * 60) - 5);
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
	/* We don't have any struct members (anymore) that need freeing */
	safe_free(md->ptr);
}

int floodprot_stats(Client *client, char *flag)
{
	if (*flag != 'S')
		return 0;

	sendtxtnumeric(client, "modef-default-unsettime: %hd", (unsigned short)MODEF_DEFAULT_UNSETTIME);
	sendtxtnumeric(client, "modef-max-unsettime: %hd", (unsigned short)MODEF_MAX_UNSETTIME);
	return 1;
}

/** Admin unloading the floodprot module for good. Bad. */
void floodprot_free_removechannelmodetimer_list(ModData *m)
{
	RemoveChannelModeTimer *e, *e_next;

	for (e=removechannelmodetimer_list; e; e=e_next)
	{
		e_next = e->next;
		safe_free(e);
	}
}

void floodprot_free_msghash_key(ModData *m)
{
	safe_free(floodprot_msghash_key);
}
