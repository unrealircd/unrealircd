/*
 * Channel Mode +f and +F
 * (C) Copyright 2019-.. Syzop and the UnrealIRCd team
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
	"6.0",
	"Channel Mode +f and +F",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

typedef enum Flood {
	CHFLD_CTCP	= 0,
	CHFLD_JOIN	= 1,
	CHFLD_KNOCK	= 2,
	CHFLD_MSG	= 3,
	CHFLD_NICK	= 4,
	CHFLD_TEXT	= 5,
	CHFLD_REPEAT	= 6,
} Flood;
#define NUMFLD	7 /* 7 flood types */

/** Configuration settings */
struct {
	unsigned char modef_default_unsettime;
	unsigned char modef_max_unsettime;
	long boot_delay;
	long split_delay;
	int modef_alternate_action_percentage_threshold;
	unsigned char modef_alternative_ban_action_unsettime;
	char *default_profile;
} cfg;

typedef struct FloodType {
	char letter;
	Flood index;
	char *description;
	char default_action;
	char *actions;
	char *alternative_ban_action;
	int timedban_required;
} FloodType;

/* All the floodtypes that are tracked.
 * IMPORTANT: the first row MUST be in alphabetic order!!
 */
FloodType floodtypes[] = {
	{ 'c', CHFLD_CTCP,	"CTCPflood",		'C',	"",	NULL,						0, },
	{ 'j', CHFLD_JOIN,	"joinflood",		'i',	"R",	"~security-group:unknown-users",		0, },
	{ 'k', CHFLD_KNOCK,	"knockflood",		'K',	"",	NULL,						0, },
	{ 'm', CHFLD_MSG,	"msg/noticeflood",	'm',	"M",	"~quiet:~security-group:unknown-users",		0, },
	{ 'n', CHFLD_NICK,	"nickflood",		'N',	"",	"~nickchange:~security-group:unknown-users",	0, },
	{ 't', CHFLD_TEXT,	"msg/noticeflood",	'\0',	"bd",	NULL,						1, },
	{ 'r', CHFLD_REPEAT,	"repeating",		'\0',	"bd",	NULL,						1, },
};

#define MODEF_DEFAULT_UNSETTIME		cfg.modef_default_unsettime
#define MODEF_MAX_UNSETTIME		cfg.modef_max_unsettime

typedef struct ChannelFloodProtection ChannelFloodProtection;
typedef struct ChannelFloodProfile ChannelFloodProfile;
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
 * But bumped to 15 because we now have cmode.flood_type_action
 * so there could be more ;).
 */
#define MAXCHMODEFACTIONS 15

/** Per-channel flood protection settings and counters */
struct ChannelFloodProtection {
	unsigned short	per; /**< setting: per <XX> seconds */
	time_t		timer[NUMFLD]; /**< runtime: timers */
	unsigned short	counter[NUMFLD]; /**< runtime: counters */
	unsigned short	counter_unknown_users[NUMFLD]; /**< runtime: counters */
	unsigned short	limit[NUMFLD]; /**< setting: limit */
	unsigned char	action[NUMFLD]; /**< setting: action */
	unsigned char	remove_after[NUMFLD]; /**< setting: remove-after <this> minutes */
	unsigned char   timers_running[MAXCHMODEFACTIONS+1]; /**< if for example a '-m' timer is running then this contains 'm' */
	char *profile;
};

struct ChannelFloodProfile {
	ChannelFloodProfile *prev, *next;
	ChannelFloodProtection settings;
};

/* Global variables */
ModDataInfo *mdflood = NULL;
Cmode_t EXTMODE_FLOODLIMIT = 0L;
Cmode_t EXTMODE_FLOOD_PROFILE = 0L;
static int timedban_available = 1; /**< Set to 1 if extbans/timedban module is loaded. Assumed 1 during config load due to set::modes-on-join race. */
RemoveChannelModeTimer *removechannelmodetimer_list = NULL;
ChannelFloodProfile *channel_flood_profiles = NULL;
char *floodprot_msghash_key = NULL;
long long floodprot_splittime = 0;

#define IsFloodLimit(x)	(((x)->mode.mode & EXTMODE_FLOODLIMIT) || ((x)->mode.mode & EXTMODE_FLOOD_PROFILE) || (cfg.default_profile && GETPARASTRUCT(channel, 'F')))

/* Forward declarations */
static void init_config(void);
int floodprot_rehash_complete(void);
int floodprot_config_test_set_block(ConfigFile *, ConfigEntry *, int, int *);
int floodprot_config_run_set_block(ConfigFile *, ConfigEntry *, int);
int floodprot_config_test_antiflood_block(ConfigFile *, ConfigEntry *, int, int *);
int floodprot_config_run_antiflood_block(ConfigFile *, ConfigEntry *, int);
void floodprottimer_del(Channel *channel, ChannelFloodProtection *fld, char mflag);
void floodprottimer_stopchantimers(Channel *channel);
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r);
static int compare_floodprot_modes(ChannelFloodProtection *a, ChannelFloodProtection *b);
static int do_floodprot(Channel *channel, Client *client, int what);
char *channel_modef_string(ChannelFloodProtection *x, char *str);
void do_floodprot_action(Channel *channel, int what);
void floodprottimer_add(Channel *channel, ChannelFloodProtection *fld, char mflag, time_t when);
uint64_t gen_floodprot_msghash(const char *text);
int cmodef_is_ok(Client *client, Channel *channel, char mode, const char *para, int type, int what);
void *cmodef_put_param(void *r_in, const char *param);
const char *cmodef_get_param(void *r_in);
const char *cmodef_conv_param(const char *param_in, Client *client, Channel *channel);
int cmodef_free_param(void *r, int soft);
void *cmodef_dup_struct(void *r_in);
int cmodef_sjoin_check(Channel *channel, void *ourx, void *theirx);
int cmodef_profile_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what);
void *cmodef_profile_put_param(void *r_in, const char *param);
const char *cmodef_profile_get_param(void *r_in);
const char *cmodef_profile_conv_param(const char *param_in, Client *client, Channel *channel);
int cmodef_profile_sjoin_check(Channel *channel, void *ourx, void *theirx);
int floodprot_join(Client *client, Channel *channel, MessageTag *mtags);
EVENT(modef_event);
int cmodef_channel_create(Channel *channel);
int cmodef_channel_destroy(Channel *channel, int *should_destroy);
int floodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype);
int floodprot_post_chanmsg(Client *client, Channel *channel, int sendflags, const char *prefix, const char *target, MessageTag *mtags, const char *text, SendType sendtype);
int floodprot_knock(Client *client, Channel *channel, MessageTag *mtags, const char *comment);
int floodprot_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int floodprot_chanmode_del(Channel *channel, int m);
void memberflood_free(ModData *md);
int floodprot_stats(Client *client, const char *flag);
void floodprot_free_removechannelmodetimer_list(ModData *m);
void floodprot_free_msghash_key(ModData *m);
CMD_OVERRIDE_FUNC(floodprot_override_mode);
ChannelFloodProtection *get_channel_flood_profile(const char *name);
int parse_channel_mode_flood(const char *param, ChannelFloodProtection *fld, int strict, Client *client, const char **error_out);
int parse_channel_mode_flood_failed(const char **error_out, ChannelFloodProtection *fld, FORMAT_STRING(const char *fmt), ...) __attribute__((format(printf,3,4)));
int floodprot_server_quit(Client *client, MessageTag *mtags);
void inherit_settings(ChannelFloodProtection *from, ChannelFloodProtection *to);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, floodprot_config_test_set_block);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, floodprot_config_test_antiflood_block);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	LoadPersistentLongLong(modinfo, floodprot_splittime);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmodef_is_ok;
	creq.letter = 'f';
	creq.unset_with_param = 1; /* ah yeah, +f is special! */
	creq.put_param = cmodef_put_param;
	creq.get_param = cmodef_get_param;
	creq.conv_param = cmodef_conv_param;
	creq.free_param = cmodef_free_param;
	creq.dup_struct = cmodef_dup_struct;
	creq.sjoin_check = cmodef_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_FLOODLIMIT);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = cmodef_profile_is_ok;
	creq.letter = 'F';
	creq.put_param = cmodef_profile_put_param;
	creq.get_param = cmodef_profile_get_param;
	creq.conv_param = cmodef_profile_conv_param;
	creq.free_param = cmodef_free_param; // +f & +F uses same code
	creq.dup_struct = cmodef_dup_struct; // +f & +F uses same code
	creq.sjoin_check = cmodef_profile_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_FLOOD_PROFILE);

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

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, floodprot_config_run_set_block);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, floodprot_config_run_antiflood_block);
	HookAdd(modinfo->handle, HOOKTYPE_CAN_SEND_TO_CHANNEL, 0, floodprot_can_send_to_channel);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, floodprot_post_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_KNOCK, 0, floodprot_knock);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, floodprot_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, floodprot_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_MODECHAR_DEL, 0, floodprot_chanmode_del);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, floodprot_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_CREATE, 0, cmodef_channel_create);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 0, cmodef_channel_destroy);
	HookAdd(modinfo->handle, HOOKTYPE_REHASH_COMPLETE, 0, floodprot_rehash_complete);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, floodprot_stats);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_QUIT, 0, floodprot_server_quit);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "modef_event", modef_event, NULL, 10000, 0);
	CommandOverrideAdd(modinfo->handle, "MODE", 0, floodprot_override_mode);
	floodprot_rehash_complete();
	return MOD_SUCCESS;
}

void free_channel_flood_profile(ChannelFloodProfile *f)
{
	safe_free(f->settings.profile);
	safe_free(f);
}

void free_channel_flood_profiles(void)
{
	ChannelFloodProfile *f, *f_next;

	for (f = channel_flood_profiles; f; f = f_next)
	{
		f_next = f->next;
		DelListItem(f, channel_flood_profiles);
		free_channel_flood_profile(f);
	}
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, removechannelmodetimer_list);
	SavePersistentPointer(modinfo, floodprot_msghash_key);
	SavePersistentLongLong(modinfo, floodprot_splittime);

	free_channel_flood_profiles();

	return MOD_SUCCESS;
}

int floodprot_rehash_complete(void)
{
	timedban_available = is_module_loaded("extbans/timedban");
	return 0;
}

/** Set a new channel anti flood profile.
 * Caller MUST ensure that the 'value' is valid, eg by calling
 * parse_channel_mode_flood() or is_ok() prior.
 */
static void set_channel_flood_profile(const char *name, const char *value)
{
	ChannelFloodProfile *f;

	for (f = channel_flood_profiles; f; f = f->next)
		if (!strcasecmp(f->settings.profile, name))
			break;
	if (!f)
	{
		f = safe_alloc(sizeof(ChannelFloodProfile));
		AddListItem(f, channel_flood_profiles);
	}

	safe_strdup(f->settings.profile, name);
	cmodef_put_param(&f->settings, value);
}

static void init_default_channel_flood_profiles(void)
{
	ChannelFloodProfile *f;

	f = safe_alloc(sizeof(ChannelFloodProfile));
	cmodef_put_param(&f->settings, "[10j#R10,30m#M10,7c#C15,5n#N15,10k#K15]:15");
	safe_strdup(f->settings.profile, "very-strict");
	AddListItem(f, channel_flood_profiles);

	f = safe_alloc(sizeof(ChannelFloodProfile));
	cmodef_put_param(&f->settings, "[15j#R10,40m#M10,7c#C15,8n#N15,10k#K15]:15");
	safe_strdup(f->settings.profile, "strict");
	AddListItem(f, channel_flood_profiles);

	f = safe_alloc(sizeof(ChannelFloodProfile));
	cmodef_put_param(&f->settings, "[30j#R10,40m#M10,7c#C15,8n#N15,10k#K15]:15");
	safe_strdup(f->settings.profile, "normal");
	AddListItem(f, channel_flood_profiles);

	f = safe_alloc(sizeof(ChannelFloodProfile));
	cmodef_put_param(&f->settings, "[45j#R10,60m#M10,7c#C15,10n#N15,10k#K15]:15");
	safe_strdup(f->settings.profile, "relaxed");
	AddListItem(f, channel_flood_profiles);

	f = safe_alloc(sizeof(ChannelFloodProfile));
	cmodef_put_param(&f->settings, "[60j#R10,90m#M10,7c#C15,10n#N15,10k#K15]:15");
	safe_strdup(f->settings.profile, "very-relaxed");
	AddListItem(f, channel_flood_profiles);

	f = safe_alloc(sizeof(ChannelFloodProfile));
	safe_strdup(f->settings.profile, "off");
	AddListItem(f, channel_flood_profiles);
}

static void init_config(void)
{
	/* This sets some default values */
	memset(&cfg, 0, sizeof(cfg));
	cfg.modef_default_unsettime = 0;
	cfg.modef_max_unsettime = 60; /* 1 hour seems enough :p */
	cfg.boot_delay = 75;
	cfg.split_delay = 75;
	cfg.modef_alternate_action_percentage_threshold = 75; /* 75% */
	cfg.modef_alternative_ban_action_unsettime = 15; /* 15min */
	init_default_channel_flood_profiles();
}

int floodprot_config_test_set_block(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->name, "modef-default-unsettime"))
	{
		if (!ce->value)
		{
			config_error_empty(ce->file->filename, ce->line_number,
				"set", ce->name);
			errors++;
		} else {
			int v = atoi(ce->value);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-default-unsettime: value '%d' out of range (should be 1-255)",
					ce->file->filename, ce->line_number, v);
				errors++;
			}
		}
	} else
	if (!strcmp(ce->name, "modef-max-unsettime"))
	{
		if (!ce->value)
		{
			config_error_empty(ce->file->filename, ce->line_number,
				"set", ce->name);
			errors++;
		} else {
			int v = atoi(ce->value);
			if ((v <= 0) || (v > 255))
			{
				config_error("%s:%i: set::modef-max-unsettime: value '%d' out of range (should be 1-255)",
					ce->file->filename, ce->line_number, v);
				errors++;
			}
		}
	} else
	if (!strcmp(ce->name, "modef-boot-delay"))
	{
		config_error("%s:%i: set::modef-boot-delay is now called set::anti-flood::channel::boot-delay. "
		             "See https://www.unrealircd.org/docs/Channel_anti-flood_settings#config",
		             ce->file->filename, ce->line_number);
		errors++;
	} else
	{
		/* Not handled by us */
		return 0;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int floodprot_config_run_set_block(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->name, "modef-default-unsettime"))
		cfg.modef_default_unsettime = (unsigned char)atoi(ce->value);
	else if (!strcmp(ce->name, "modef-max-unsettime"))
		cfg.modef_max_unsettime = (unsigned char)atoi(ce->value);
	else
		return 0; /* not handled by us */

	return 1;
}

/** Check if 'str' is a flood profile name
 */
int valid_flood_profile_name(const char *str)
{
	if (strlen(str) > 24)
		return 0;
	for (; *str; str++)
		if (!islower(*str) && !isdigit(*str) && !strchr("_-", *str))
			return 0;
	return 1;
}

int floodprot_config_test_antiflood_block(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	/* We only deal with set::anti-flood::channel */
	if ((type != CONFIG_SET_ANTI_FLOOD) || strcmp(ce->parent->name, "channel"))
		return 0;

	for (; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "default-profile"))
		{
			if (!ce->value)
			{
				config_error_noname(ce->file->filename, ce->line_number,
				                    "set::anti-flood::channel::default-profile");
				errors++;
				continue;
			}
		} else
		if (!strcmp(ce->name, "boot-delay") || !strcmp(ce->name, "split-delay"))
		{
			if (!ce->value)
			{
				config_error_empty(ce->file->filename, ce->line_number,
					"set", ce->name);
				errors++;
			} else {
				long v = config_checkval(ce->value, CFG_TIME);
				if ((v < 0) || (v > 600))
				{
					config_error("%s:%i: set::anti-flood::channel::%s: value '%ld' out of range (should be 0-600)",
						ce->file->filename, ce->line_number,
						ce->name,
						v);
					errors++;
				}
			}
		} else
		if (!strcmp(ce->name, "profile"))
		{
			if (!ce->value)
			{
				config_error_noname(ce->file->filename, ce->line_number,
				                    "set::anti-flood::channel::profile");
				errors++;
				continue;
			}
			if (!valid_flood_profile_name(ce->value))
			{
				config_error("%s:%i: set::anti-flood::channel: profile '%s' name is invalid. "
				             "Name can be 24 characters max and may only contain characters a-z, 0-9, _ and -",
				             ce->file->filename, ce->line_number, ce->value);
				errors++;
				continue;
			}
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "flood-mode"))
				{
					ChannelFloodProtection fld;
					const char *err;

					if (!cep->value)
					{
						config_error("%s:%i: set::anti-flood::channel::profile %s::flood-mode has no value",
						             cep->file->filename, cep->line_number, ce->value);
						errors++;
						continue;
					}
					memset(&fld, 0, sizeof(fld));
					if (!parse_channel_mode_flood(cep->value, &fld, 1, NULL, &err))
					{
						config_error("%s:%i: set::anti-flood::channel::profile %s::flood-mode: %s",
						             cep->file->filename, cep->line_number,
						             ce->value,
						             cep->value);
						errors++;
					} else if (!BadPtr(err))
					{
						config_warn("%s:%i: set::anti-flood::channel::profile %s::flood-mode: %s",
						             cep->file->filename, cep->line_number,
						             ce->value,
						             err);
					}
					if (fld.limit[CHFLD_TEXT] || fld.limit[CHFLD_REPEAT])
					{
						config_error("%s:%i: set::anti-flood::channel::profile %s::flood-mode: "
						             "subtypes 't' and 'r' are not supported for +F profiles at the moment.",
						             cep->file->filename, cep->line_number,
						             ce->value);
						errors++;
					}
				} else {
					config_error_unknown(cep->file->filename, cep->line_number,
							     "set::anti-flood::channel::profile", cep->name);
					errors++;
				}
			}
		} else
		{
			config_error_unknown(ce->file->filename, ce->line_number,
			                     "set::anti-flood::channel", ce->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -2 : 2;
}

int floodprot_config_run_antiflood_block(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	/* We only deal with set::anti-flood::channel */
	if ((type != CONFIG_SET_ANTI_FLOOD) || strcmp(ce->parent->name, "channel"))
		return 0;

	for (; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "default-profile"))
		{
			safe_strdup(cfg.default_profile, ce->value);
		} else
		if (!strcmp(ce->name, "boot-delay"))
		{
			cfg.boot_delay = config_checkval(ce->value, CFG_TIME);
		} else
		if (!strcmp(ce->name, "split-delay"))
		{
			cfg.split_delay = config_checkval(ce->value, CFG_TIME);
		} else
		if (!strcmp(ce->name, "profile"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "flood-mode"))
					set_channel_flood_profile(ce->value, cep->value);
			}
		}
	}
	return 2;
}

FloodType *find_floodprot_by_letter(char c)
{
	int i;
	for (i=0; i < ARRAY_SIZEOF(floodtypes); i++)
		if (floodtypes[i].letter == c)
			return &floodtypes[i];

	return NULL;
}

FloodType *find_floodprot_by_index(Flood index)
{
	int i;
	for (i=0; i < ARRAY_SIZEOF(floodtypes); i++)
		if (floodtypes[i].index == index)
			return &floodtypes[i];

	return NULL;
}

ChannelFloodProtection *get_channel_flood_profile(const char *name)
{
	ChannelFloodProfile *f;

	for (f = channel_flood_profiles; f; f = f->next)
		if (!strcasecmp(f->settings.profile, name))
			return &f->settings;

	return NULL;
}

/** Helper function for parse_channel_mode_flood() */
int parse_channel_mode_flood_failed(const char **error_out, ChannelFloodProtection *fld, const char *fmt, ...)
{
	static char retbuf[512];
	int v;

	va_list vl;
	va_start(vl, fmt);
	vsnprintf(retbuf, sizeof(retbuf), fmt, vl);
	va_end(vl);

	/* Zero out all settings */
	for (v=0; v < NUMFLD; v++)
	{
		fld->limit[v] = 0;
		fld->action[v] = 0;
		fld->remove_after[v] = 0;
	}

	if (error_out)
		*error_out = retbuf;

	return 0;
}

int floodprot_valid_alternate_action(char action, FloodType *floodtype)
{
	Cmode *cm;

	/* Built-in actions */
	if (strchr(floodtype->actions, action))
		return 1;

	cm = find_channel_mode_handler(action);
	if (cm && cm->flood_type_action == floodtype->letter)
		return 1;

	return 0;
}

/** Parse channel mode +f string.
 * @param param		The parameter string to parse
 * @param fld		The setting struct to fill, this MAY already contain data.
 * @param strict	If set to 1 then reject invalid setting, used for ex .is_ok().
 *			If set to 0 then do your best to make something out of it,
 *			and skip invalid stuff for forward-compatibility, eg for .put_param().
 * @param client	The client requesting the mode change, can be NULL
 *			(used for local vs remote things, not for sending errors)
 * @param error		Used for returning the error or warning string, can be NULL.
 * @retval 1 On success, although there could still be a warning stored in *error_out.
 * @retval 0 On failure, the error will be in *error_out
 */
int parse_channel_mode_flood(const char *param, ChannelFloodProtection *fld, int strict, Client *client, const char **error_out)
{
	static char retbuf[512];
	char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
	int v;
	unsigned short breakit;
	unsigned char r;
	FloodType *floodtype;
	Flood index;
	char localclient = (client && MyUser(client)) ? 1 : 0;
	char warn_unknown_flood_type[32];

	*warn_unknown_flood_type = '\0';
	if (error_out)
		*error_out = NULL;

	/* always reset settings (l, a, r) */
	for (v=0; v < NUMFLD; v++)
	{
		fld->limit[v] = 0;
		fld->action[v] = 0;
		fld->remove_after[v] = 0;
	}

	strlcpy(xbuf, param, sizeof(xbuf));

	if (*xbuf != '[')
		return parse_channel_mode_flood_failed(error_out, fld, "Invalid format (brackets missing)");

	/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
	p2 = strchr(xbuf+1, ']');
	if (!p2)
		return parse_channel_mode_flood_failed(error_out, fld, "Invalid format (brackets missing)");
	*p2 = '\0';
	if (*(p2+1) != ':')
		return parse_channel_mode_flood_failed(error_out, fld, "Invalid format (:XX period missing)");

	breakit = 0;
	for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
	{
		/* <number><1 letter>[optional: '#'+1 letter] */
		p = x;
		while(isdigit(*p)) { p++; }

		/* letter */
		c = *p;
		floodtype = find_floodprot_by_letter(c);
		if (!floodtype)
		{
			strlcat_letter(warn_unknown_flood_type, c, sizeof(warn_unknown_flood_type));
			continue; /* continue instead of break for forward compatability. */
		}
		*p = '\0';

		/* floodcount (number) */
		v = atoi(x);
		if (strict)
		{
			if ((v < 1) || (v > 999))
				return parse_channel_mode_flood_failed(error_out, fld, "Flood count for '%c' must be 1-999 (got %d)", c, v);
		}
		if (v < 1)
			v = 1;
		if (v > 999)
			v = 999;
		p++;
		a = '\0';

		/* Removal */
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
					if (tv > 255)
						tv = 255; /* always max limit, as it is a char */
					if (strict && localclient && (tv > MODEF_MAX_UNSETTIME))
						tv = MODEF_MAX_UNSETTIME;
					r = (unsigned char)tv;
				}
			}
		}

		index = floodtype->index;
		fld->limit[index] = v;
		if (a && floodprot_valid_alternate_action(a, floodtype))
			fld->action[index] = a;
		else
			fld->action[index] = floodtype->default_action;
		if (!floodtype->timedban_required || (floodtype->timedban_required && timedban_available))
			fld->remove_after[index] = r;
	} /* for */

	/* parse 'per' */
	p2++;
	if (*p2 != ':')
		return parse_channel_mode_flood_failed(error_out, fld, "Invalid format (:XX period missing)");
	p2++;
	if (!*p2)
		return parse_channel_mode_flood_failed(error_out, fld, "Invalid format (:XX period missing)");
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
			fld->counter_unknown_users[i] = 0;
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
		/* Nothing is turned on.. */
		if (*warn_unknown_flood_type)
			return parse_channel_mode_flood_failed(error_out, fld, "Unknown flood type(s) '%s'", warn_unknown_flood_type);
		return parse_channel_mode_flood_failed(error_out, fld, "None of the floodtypes set");
	}

	/* Finally, this is a warning only */
	if (*warn_unknown_flood_type && error_out)
	{
		snprintf(retbuf, sizeof(retbuf), "Unknown flood type(s) '%s'", warn_unknown_flood_type);
		*error_out = retbuf;
	}

	return 1;
}

int cmodef_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsUser(client) && check_channel_access(client, channel, "oaq"))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(client, ERR_NOTFORHALFOPS, 'f');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		ChannelFloodProtection fld;
		const char *err;

		memset(&fld, 0, sizeof(fld));
		if (!parse_channel_mode_flood(param, &fld, 1, client, &err))
		{
			sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'f', err);
			return EX_DENY;
		} else if (err)
		{
			sendnotice(client, "WARNING: Channel mode +f: %s", err);
			/* fallthrough */
		}
		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void *cmodef_put_param(void *fld_in, const char *param)
{
	ChannelFloodProtection *fld = (ChannelFloodProtection *)fld_in;
	int v;

	if (!fld)
		fld = safe_alloc(sizeof(ChannelFloodProtection));

	parse_channel_mode_flood(param, fld, 0, NULL, NULL);

	return fld;
}

const char *cmodef_get_param(void *r_in)
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
const char *cmodef_conv_param(const char *param_in, Client *client, Channel *channel)
{
	static char retbuf[256];
	ChannelFloodProtection fld;
	const char *err;

	memset(&fld, 0, sizeof(fld));
	if (!parse_channel_mode_flood(param_in, &fld, 0, client, &err))
		return NULL;

	*retbuf = '\0';
	channel_modef_string(&fld, retbuf);
	return retbuf;
}

int cmodef_free_param(void *r, int soft)
{
	ChannelFloodProtection *fld = (ChannelFloodProtection *)r;

	if (!fld)
		return 0;

	if (soft && fld->profile && cfg.default_profile)
	{
		/* Resist freeing */
		if (strcmp(fld->profile, cfg.default_profile))
		{
			/* But reset */
			ChannelFloodProtection *base = get_channel_flood_profile(cfg.default_profile);
			if (!base)
				base = get_channel_flood_profile("normal"); /* fallback, always exists */
			inherit_settings(base, fld);
			safe_strdup(fld->profile, base->profile);
		}
		return 1; /* NO FREE */
	} else
	{
		// TODO: consider cancelling timers just to be sure? or maybe in DEBUGMODE?
		safe_free(fld->profile);
		safe_free(r);
	}
	return 0;
}

void *cmodef_dup_struct(void *r_in)
{
	ChannelFloodProtection *r = (ChannelFloodProtection *)r_in;
	ChannelFloodProtection *w = safe_alloc(sizeof(ChannelFloodProtection));

	/* We can copy most members in a lazy way... */
	memcpy(w, r, sizeof(ChannelFloodProtection));
	/* ... except this one. */
	w->profile = raw_strdup(r->profile);
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

void floodprot_show_profiles(Client *client)
{
	ChannelFloodProfile *fld;
	char buf[512];
	int padding;
	int max_length = 0;

	sendnotice(client, "List of available flood profiles for +F:");
	for (fld = channel_flood_profiles; fld; fld = fld->next)
	{
		int n = strlen(fld->settings.profile);
		if (n > max_length)
			max_length = n;
	}

	for (fld = channel_flood_profiles; fld; fld = fld->next)
	{
		*buf = '\0';
		channel_modef_string(&fld->settings, buf);
		padding = max_length - strlen(fld->settings.profile);
		sendnotice(client, " %*s%s: %s",
		           padding, "",
		           fld->settings.profile,
		           buf);
	}
	sendnotice(client, "See also https://www.unrealircd.org/docs/Channel_anti-flood_settings");
}

int cmodef_profile_is_ok(Client *client, Channel *channel, char mode, const char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsUser(client) && check_channel_access(client, channel, "oaq"))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(client, ERR_NOTFORHALFOPS, 'f');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		if (get_channel_flood_profile(param))
			return EX_ALLOW;
		sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'F', "Invalid flood profile specified for +F");
		floodprot_show_profiles(client);
		return EX_DENY;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

void inherit_settings(ChannelFloodProtection *from, ChannelFloodProtection *to)
{
	int i;

	/* If new 'per xxx seconds' is smaller than current 'per' then reset timers/counters (t, c) */
	if (from->per < to->per)
	{
		for (i=0; i < NUMFLD; i++)
		{
			to->timer[i] = 0;
			to->counter[i] = 0;
			to->counter_unknown_users[i] = 0;
		}
	}

	/* inherit settings (limit/action/remove_after) */
	for (i=0; i < NUMFLD; i++)
	{
		to->limit[i] = from->limit[i];
		to->action[i] = from->action[i];
		to->remove_after[i] = from->remove_after[i];
	}
	to->per = from->per;
}

void *cmodef_profile_put_param(void *fld_in, const char *param)
{
	ChannelFloodProtection *fld = (ChannelFloodProtection *)fld_in;
	ChannelFloodProtection *base;
	int v;

	if (!fld)
		fld = safe_alloc(sizeof(ChannelFloodProtection));

	base = get_channel_flood_profile(param);
	if (!base)
		base = get_channel_flood_profile("normal"); /* fallback, always exists */

	safe_strdup(fld->profile, param);
	inherit_settings(base, fld);

	return (void *)fld;
}

const char *cmodef_profile_get_param(void *r_in)
{
	ChannelFloodProtection *r = (ChannelFloodProtection *)r_in;
	static char retbuf[512];

	if (!r)
		return NULL;

	strlcpy(retbuf, r->profile ? r->profile : "???", sizeof(retbuf));
	return retbuf;
}

/** Convert parameter to something proper.
 * NOTE: client may be NULL if called for e.g. set::modes-on-join
 */
const char *cmodef_profile_conv_param(const char *param_in, Client *client, Channel *channel)
{
	static char retbuf[256];
	ChannelFloodProtection *fld;

	fld = get_channel_flood_profile(param_in);
	if (!fld)
		return NULL;

	strlcpy(retbuf, fld->profile, sizeof(retbuf));

	return retbuf;
}

int cmodef_profile_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	ChannelFloodProtection *our = (ChannelFloodProtection *)ourx;
	ChannelFloodProtection *their = (ChannelFloodProtection *)theirx;
	int i;

	if (!strcmp(our->profile, their->profile))
		return EXSJ_SAME;

	if (strcmp(our->profile, their->profile) < 0)
		return EXSJ_THEYWON;

	return EXSJ_WEWON;
}

int floodprot_join(Client *client, Channel *channel, MessageTag *mtags)
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
	    (MyUser(client) || client->uplink->server->flags.synced) &&
	    (client->uplink->server->boottime && (TStime() - client->uplink->server->boottime >= cfg.boot_delay)) &&
	    (TStime() - floodprot_splittime >= cfg.split_delay) &&
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

/** Install a default +F profile, if set::anti-flood::channel::default-profile is set */
int cmodef_channel_create(Channel *channel)
{
	ChannelFloodProtection *base;
	ChannelFloodProtection *fld;

	if (!cfg.default_profile)
		return 0;

	base = get_channel_flood_profile(cfg.default_profile);
	if (!base)
		base = get_channel_flood_profile("normal"); /* fallback, always exists */

	GETPARASTRUCT(channel, 'F') = fld = safe_alloc(sizeof(ChannelFloodProtection));
	inherit_settings(base, fld);
	safe_strdup(fld->profile, base->profile);

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

ChannelFloodProtection *get_channel_flood_settings(Channel *channel, int what)
{
	ChannelFloodProtection *fld;

	if (channel->mode.mode & EXTMODE_FLOODLIMIT)
	{
		fld = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');
		if (fld->action[what])
			return fld;
	}

	fld = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'F');
	if (fld && fld->action[what])
		return fld;

	return NULL;
}

int floodprot_can_send_to_channel(Client *client, Channel *channel, Membership *lp, const char **msg, const char **errmsg, SendType sendtype)
{
	Membership *mb;
	ChannelFloodProtection *fld;
	MemberFlood *memberflood;
	uint64_t msghash;
	unsigned char is_flooding_text=0, is_flooding_repeat=0;
	static char errbuf[256];

	/* This is redundant, right? */
	if (!MyUser(client))
		return HOOK_CONTINUE;

	if (sendtype == SEND_TYPE_TAGMSG)
		return 0; // TODO: some TAGMSG specific limit? (1 of 2)

	if (ValidatePermissionsForPath("channel:override:flood",client,NULL,channel,NULL) || !IsFloodLimit(channel) || check_channel_access(client, channel, "hoaq"))
		return HOOK_CONTINUE;

	if (!(mb = find_membership_link(client->user->channel, channel)))
		return HOOK_CONTINUE; /* not in channel */

	/* config test rejects having 't' in +F and 'r' in +f or vice versa,
	 * otherwise we would be screwed here :D.
	 */
	fld = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');

	if (!fld || !(fld->limit[CHFLD_TEXT] || fld->limit[CHFLD_REPEAT]))
		return HOOK_CONTINUE;

	if (moddata_membership(mb, mdflood).ptr == NULL)
	{
		/* Alloc a new entry if it doesn't exist yet */
		moddata_membership(mb, mdflood).ptr = safe_alloc(sizeof(MemberFlood));
	}

	memberflood = (MemberFlood *)moddata_membership(mb, mdflood).ptr;

	if ((TStime() - memberflood->firstmsg) >= fld->per)
	{
		/* Reset due to moving into a new time slot */
		memberflood->firstmsg = TStime();
		memberflood->nmsg = 1;
		memberflood->nmsg_repeat = 1;
		if (fld->limit[CHFLD_REPEAT])
		{
			memberflood->lastmsg = gen_floodprot_msghash(*msg);
			memberflood->prevmsg = 0;
		}
		return HOOK_CONTINUE; /* forget about it.. */
	}

	/* Anti-repeat ('r') */
	if (fld->limit[CHFLD_REPEAT])
	{
		msghash = gen_floodprot_msghash(*msg);
		if (memberflood->lastmsg)
		{
			if ((memberflood->lastmsg == msghash) || (memberflood->prevmsg == msghash))
			{
				memberflood->nmsg_repeat++;
				if (memberflood->nmsg_repeat > fld->limit[CHFLD_REPEAT])
					is_flooding_repeat = 1;
			}
			memberflood->prevmsg = memberflood->lastmsg;
		}
		memberflood->lastmsg = msghash;
	}

	if (fld->limit[CHFLD_TEXT])
	{
		/* increase msgs */
		memberflood->nmsg++;
		if (memberflood->nmsg > fld->limit[CHFLD_TEXT])
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
			snprintf(errbuf, sizeof(errbuf), "Flooding (Limit is %i lines per %i seconds)", fld->limit[CHFLD_TEXT], fld->per);
			flood_type = CHFLD_TEXT;
		}

		if (fld->action[flood_type] == 'd')
		{
			/* Drop the message */
			*errmsg = errbuf;
			return HOOK_DENY;
		}

		if (fld->action[flood_type] == 'b')
		{
			/* Ban the user */
			if (timedban_available && (fld->remove_after[flood_type] > 0))
			{
				if (iConf.named_extended_bans)
					snprintf(mask, sizeof(mask), "~time:%d:*!*@%s", fld->remove_after[flood_type], GetHost(client));
				else
					snprintf(mask, sizeof(mask), "~t:%d:*!*@%s", fld->remove_after[flood_type], GetHost(client));
			} else {
				snprintf(mask, sizeof(mask), "*!*@%s", GetHost(client));
			}
			if (add_listmode(&channel->banlist, &me, channel, mask) == 0)
			{
				mtags = NULL;
				new_message(&me, NULL, &mtags);
				sendto_server(NULL, 0, 0, mtags, ":%s MODE %s +b %s 0", me.id, channel->name, mask);
				sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				    ":%s MODE %s +b %s", me.name, channel->name, mask);
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

int floodprot_post_chanmsg(Client *client, Channel *channel, int sendflags, const char *prefix, const char *target, MessageTag *mtags, const char *text, SendType sendtype)
{
	if (!IsFloodLimit(channel) || check_channel_access(client, channel, "hoaq") || IsULine(client))
		return 0;

	if (sendtype == SEND_TYPE_TAGMSG)
		return 0; // TODO: some TAGMSG specific limit? (2 of 2)

	/* HINT: don't be so stupid to reorder the items in the if's below.. you'll break things -- Syzop. */

	do_floodprot(channel, client, CHFLD_MSG);

	if ((text[0] == '\001') && strncmp(text+1, "ACTION ", 7))
		do_floodprot(channel, client, CHFLD_CTCP);

	return 0;
}

int floodprot_knock(Client *client, Channel *channel, MessageTag *mtags, const char *comment)
{
	if (IsFloodLimit(channel) && !IsULine(client))
		do_floodprot(channel, client, CHFLD_KNOCK);
	return 0;
}

int floodprot_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	Membership *mp;

	/* Ignore u-lines, as usual */
	if (IsULine(client))
		return 0;

	/* Don't count forced nick changes, eg from NickServ */
	if (find_mtag(mtags, "unrealircd.org/issued-by"))
		return 0;

	for (mp = client->user->channel; mp; mp = mp->next)
	{
		Channel *channel = mp->channel;
		if (channel && IsFloodLimit(channel) && !check_channel_access_membership(mp, "vhoaq"))
		{
			do_floodprot(channel, client, CHFLD_NICK);
		}
	}
	return 0;
}

void floodprot_chanmode_del_helper(ChannelFloodProtection *fld, char modechar)
{
	/* reset joinflood on -i, reset msgflood on -m, etc.. */
	switch(modechar)
	{
		case 'C':
			fld->counter[CHFLD_CTCP] = 0;
			fld->counter_unknown_users[CHFLD_CTCP] = 0;
			break;
		case 'N':
			fld->counter[CHFLD_NICK] = 0;
			fld->counter_unknown_users[CHFLD_NICK] = 0;
			break;
		case 'm':
			fld->counter[CHFLD_MSG] = 0;
			fld->counter[CHFLD_CTCP] = 0;
			fld->counter_unknown_users[CHFLD_MSG] = 0;
			fld->counter_unknown_users[CHFLD_CTCP] = 0;
			break;
		case 'K':
			fld->counter[CHFLD_KNOCK] = 0;
			fld->counter_unknown_users[CHFLD_KNOCK] = 0;
			break;
		case 'i':
			fld->counter[CHFLD_JOIN] = 0;
			fld->counter_unknown_users[CHFLD_JOIN] = 0;
			break;
		case 'M':
			fld->counter[CHFLD_MSG] = 0;
			fld->counter[CHFLD_CTCP] = 0;
			fld->counter_unknown_users[CHFLD_MSG] = 0;
			fld->counter_unknown_users[CHFLD_CTCP] = 0;
			break;
		case 'R':
			fld->counter[CHFLD_JOIN] = 0;
			fld->counter_unknown_users[CHFLD_JOIN] = 0;
			break;
		default:
			break;
	}
}

int floodprot_chanmode_del(Channel *channel, int modechar)
{
	ChannelFloodProtection *fld;

	if (!IsFloodLimit(channel))
		return 0;

	fld = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');
	if (fld)
	{
		floodprot_chanmode_del_helper(fld, modechar);
		floodprottimer_del(channel, fld, modechar);
	}

	fld = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'F');
	if (fld)
	{
		floodprot_chanmode_del_helper(fld, modechar);
		floodprottimer_del(channel, fld, modechar);
	}

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
void floodprottimer_add(Channel *channel, ChannelFloodProtection *fld, char mflag, time_t when)
{
	RemoveChannelModeTimer *e = NULL;
	unsigned char add=1;

	if (strchr(fld->timers_running, mflag))
	{
		/* Already exists... */
		e = floodprottimer_find(channel, mflag);
		if (e)
			add = 0;
	}

	if (!strchr(fld->timers_running, mflag))
	{
		if (strlen(fld->timers_running)+1 >= sizeof(fld->timers_running))
		{
			unreal_log(ULOG_WARNING, "flood", "BUG_FLOODPROTTIMER_ADD", NULL,
			           "[BUG] floodprottimer_add: too many timers running for $channel ($timers_running)",
			           log_data_channel("channel", channel),
			           log_data_string("timers_running", fld->timers_running));
			return;
		}
		strccat(fld->timers_running, mflag); /* bounds already checked ^^ */
	}

	if (add)
		e = safe_alloc(sizeof(RemoveChannelModeTimer));

	e->channel = channel;
	e->m = mflag;
	e->when = when;

	if (add)
		AddListItem(e, removechannelmodetimer_list);
}

void floodprottimer_del(Channel *channel, ChannelFloodProtection *fld, char mflag)
{
	RemoveChannelModeTimer *e;

	if (fld && !strchr(fld->timers_running, mflag))
		return; /* nothing to remove.. */
	e = floodprottimer_find(channel, mflag);
	if (!e)
		return;

	DelListItem(e, removechannelmodetimer_list);
	safe_free(e);

	if (fld)
        {
                char newtf[MAXCHMODEFACTIONS+1];
                char *i, *o;
                for (i=fld->timers_running, o=newtf; *i; i++)
                        if (*i != mflag)
                                *o++ = *i;
                *o = '\0';
                strcpy(fld->timers_running, newtf); /* always shorter (or equal) */
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
			Cmode_t extmode = get_extmode_bitbychar(e->m);

			if (extmode && (e->channel->mode.mode & extmode))
			{
				MessageTag *mtags = NULL;

				new_message(&me, NULL, &mtags);
				sendto_server(NULL, 0, 0, mtags, ":%s MODE %s -%c 0", me.id, e->channel->name, e->m);
				sendto_channel(e->channel, &me, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s MODE %s -%c",
				               me.name, e->channel->name, e->m);
				free_message_tags(mtags);
				e->channel->mode.mode &= ~extmode;
			}

			/* And delete... */
			DelListItem(e, removechannelmodetimer_list);
			safe_free(e);
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
	ChannelFloodProtection *fld = get_channel_flood_settings(channel, what);
	char unknown_user;

	if (!fld)
		return 0; /* no +f active */

	unknown_user = user_allowed_by_security_group_name(client, "known-users") ? 0 : 1;

	if (fld->limit[what])
	{
		if (TStime() - fld->timer[what] >= fld->per)
		{
			/* reset */
			fld->timer[what] = TStime();
			fld->counter[what] = 1;
			fld->counter_unknown_users[what] = unknown_user;
		} else
		{
			fld->counter[what]++;

			if (unknown_user)
				fld->counter_unknown_users[what]++;

			if ((fld->counter[what] > fld->limit[what]) &&
			    (TStime() - fld->timer[what] < fld->per))
			{
				if (MyUser(client))
					do_floodprot_action(channel, what);
				return 1; /* flood detected! */
			}
		}
	}
	return 0;
}

/** Helper for do_floodprot_action() - standard action +i/+R/etc.. */
void do_floodprot_action_standard(Channel *channel, int what, FloodType *floodtype, Cmode_t extmode, char m)
{
	ChannelFloodProtection *fld = get_channel_flood_settings(channel, what);
	char comment[512], target[CHANNELLEN + 8];
	MessageTag *mtags;
	const char *text = floodtype->description;

	/* First the notice to the chanops */
	mtags = NULL;
	new_message(&me, NULL, &mtags);
	ircsnprintf(comment, sizeof(comment), "*** Channel %s detected (limit is %d per %d seconds), setting mode +%c",
		text, fld->limit[what], fld->per, m);
	ircsnprintf(target, sizeof(target), "%%%s", channel->name);
	sendto_channel(channel, &me, NULL, "ho",
		       0, SEND_ALL, mtags,
		       ":%s NOTICE %s :%s", me.name, target, comment);
	free_message_tags(mtags);

	/* Then the MODE broadcast */
	mtags = NULL;
	new_message(&me, NULL, &mtags);
	sendto_server(NULL, 0, 0, mtags, ":%s MODE %s +%c 0", me.id, channel->name, m);
	sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +%c", me.name, channel->name, m);
	free_message_tags(mtags);

	/* Actually set the mode internally */
	channel->mode.mode |= extmode;

	/* Add remove-chanmode timer */
	if (fld->remove_after[what])
	{
		floodprottimer_add(channel, fld, m, TStime() + ((long)fld->remove_after[what] * 60) - 5);
		/* (since the floodprot timer event is called every 10s, we do -5 here so the accurancy will
		 *  be -5..+5, without it it would be 0..+10.)
		 */
	}
}

/** Helper for do_floodprot_action() - alternative action like +b ~security-group:unknown-users */
int do_floodprot_action_alternative(Channel *channel, int what, FloodType *floodtype)
{
	ChannelFloodProtection *fld = get_channel_flood_settings(channel, what);
	char ban[512];
	char comment[512], target[CHANNELLEN + 8];
	MessageTag *mtags;
	const char *text = floodtype->description;

	snprintf(ban, sizeof(ban), "~time:%d:%s",
	         fld->remove_after[what] ? fld->remove_after[what] : cfg.modef_alternative_ban_action_unsettime,
	         floodtype->alternative_ban_action);

	/* Add the ban internally */
	if (add_listmode(&channel->banlist, &me, channel, ban) == -1)
		return 0; /* ban list full (or ban already exists) */

	/* First the notice to the chanops */
	mtags = NULL;
	new_message(&me, NULL, &mtags);
	ircsnprintf(comment, sizeof(comment),
	            "*** Channel %s detected (limit is %d per %d seconds), "
	            "mostly caused by 'unknown-users', setting mode +b %s",
		text, fld->limit[what], fld->per, ban);
	ircsnprintf(target, sizeof(target), "%%%s", channel->name);
	sendto_channel(channel, &me, NULL, "ho",
		       0, SEND_ALL, mtags,
		       ":%s NOTICE %s :%s", me.name, target, comment);
	free_message_tags(mtags);

	/* Then the MODE broadcast */
	mtags = NULL;
	new_message(&me, NULL, &mtags);
	sendto_server(NULL, 0, 0, mtags, ":%s MODE %s +b %s 0", me.id, channel->name, ban);
	sendto_channel(channel, &me, NULL, 0, 0, SEND_LOCAL, mtags, ":%s MODE %s +b %s", me.name, channel->name, ban);
	free_message_tags(mtags);

	return 1;
}


void do_floodprot_action(Channel *channel, int what)
{
	Cmode_t extmode = 0;
	ChannelFloodProtection *fld = get_channel_flood_settings(channel, what);
	FloodType *floodtype = find_floodprot_by_index(what);
	char ban_exists;
	double perc;
	char m;

	if (!fld || !floodtype)
		return;

	/* For drop action we don't actually have to do anything here, but we still have to prevent Unreal
	 * from setting chmode +d (which is useless against floods anyways) =]
	 */
	if (fld->action[what] == 'd')
		return;

	m = fld->action[what];
	if (!m)
		return;

	extmode = get_extmode_bitbychar(m);
	if (!extmode)
		return;

	if (extmode && (channel->mode.mode & extmode))
		return; /* channel mode is already set, so nothing to do */

	/* Do we have other options, instead of setting the channel +i/etc ? */
	if (floodtype->alternative_ban_action)
	{
		/* The 'ban_exists' assignments and checks below are carefully ordered and checked.
		 * Don't try to "optimize" this code by rearranging or scratching certain checks
		 * or assignments! -- Syzop
		 */
		ban_exists = ban_exists_ignore_time(channel->banlist, floodtype->alternative_ban_action);

		if (!ban_exists)
		{
			/* Calculate the percentage of unknown-users that is responsible for the action trigger */
			perc = ((double)fld->counter_unknown_users[what] / (double)fld->counter[what])*100;
			if (perc >= cfg.modef_alternate_action_percentage_threshold)
			{
				/* ACTION: We need to add the ban (+b) */
				ban_exists = do_floodprot_action_alternative(channel, what, floodtype);
			}
		}

		/* Now recheck, before we fallback to do_floodprot_action_standard() below,
		 * taking into account that all unknown-users are banned now
		 * or will be banned (eg: some actions still go through because of lag
		 * between servers).
		 */
		if (ban_exists && (fld->counter[what] - fld->counter_unknown_users[what] <= fld->limit[what]))
			return; /* flood limit not reached by known-users group */
	}

	/* ACTION: We need to set the channel mode */
	do_floodprot_action_standard(channel, what, floodtype, extmode, m);
}

uint64_t gen_floodprot_msghash(const char *text)
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
		if ((len = strlen(plaintext)) && plaintext[len - 1] == '\001')
			plaintext[len - 1] = '\0';
		plaintext++;
		if (is_action)
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

int floodprot_stats(Client *client, const char *flag)
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

CMD_OVERRIDE_FUNC(floodprot_override_mode)
{
	if (MyUser(client) && (parc == 3) &&
	    (parv[1][0] == '#') &&
	    (!strcasecmp(parv[2], "f") || !strcasecmp(parv[2], "+f")))
	{
		/* Query (not set!) request for #channel */
		Channel *channel = find_channel(parv[1]);
		ChannelFloodProtection *profile, *advanced;
		char buf[512];

		if (!channel)
		{
			sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
			return;
		}

		advanced = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'f');
		profile = (ChannelFloodProtection *)GETPARASTRUCT(channel, 'F');
		if (!advanced && !profile)
		{
			sendnotice(client, "No channel mode +f/+F is active on %s", channel->name);
		} else
		if (advanced && !profile)
		{
			channel_modef_string(advanced, buf);
			sendnotice(client, "Channel '%s' has effective flood setting '%s' (custom settings via +f)",
			           channel->name, buf);
		} else
		if (profile && !advanced)
		{
			channel_modef_string(profile, buf);
			sendnotice(client, "Channel '%s' has effective flood setting '%s' (flood profile '%s')",
			           channel->name, buf, profile->profile);
		} else {
			/* Both +f and +F are set */
			int v;
			ChannelFloodProtection mix;
			FloodType *t;
			char overridden[64];
			*overridden = '\0';
			memcpy(&mix, profile, sizeof(mix));
			for (v=0; v < NUMFLD; v++)
			{
				if ((advanced->limit[v]>0) && (mix.limit[v]>0))
				{
					mix.limit[v] = 0;
					mix.action[v] = 0;
					t = find_floodprot_by_index(v);
					if (t)
						strlcat_letter(overridden, t->letter, sizeof(overridden));
				}
			}
			channel_modef_string(&mix, buf);
			if (*overridden)
			{
				sendnotice(client, "Channel '%s' uses flood profile '%s', without action(s) '%s' as they are overridden by +f.",
					   channel->name, profile->profile, overridden);
				sendnotice(client, "Effective flood setting via +F: '%s'", buf);
			} else {
				sendnotice(client, "Channel '%s' has effective flood setting '%s' (flood profile '%s')",
					   channel->name, buf, profile->profile);
			}
			channel_modef_string(advanced, buf);
			sendnotice(client, "Plus flood setting via +f: '%s'", buf);
		}
		sendnotice(client, "-");
		floodprot_show_profiles(client);
		return;
	}

	CALL_NEXT_COMMAND_OVERRIDE();
}

int floodprot_server_quit(Client *client, MessageTag *mtags)
{
	if (!IsULine(client))
		floodprot_splittime = TStime();
	return 0;
}

// TODO: if flood profiles change during REHASH (or otherwise) they are not re-applied to channels

// TODO: handle mismatch of flood profiles between servers

// TODO: perhaps an option to turn off +F in the IRCd without turning off +f ? maybe as a multi-option.
