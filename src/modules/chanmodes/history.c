/*
 * modules/chanmodes/history - Channel History
 * (C) Copyright 2009-2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"chanmodes/history",
	"1.0",
	"Channel Mode +H",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

typedef struct ConfigHistoryExt ConfigHistoryExt;
struct ConfigHistoryExt {
	int lines; /**< number of lines */
	long time; /**< seconds */
};
struct {
	ConfigHistoryExt playback_on_join; /**< Maximum number of lines & time to playback on-join */
	ConfigHistoryExt max_storage_per_channel; /**< Maximum number of lines & time to record */
} cfg;

typedef struct HistoryChanMode HistoryChanMode;
struct HistoryChanMode {
	unsigned int max_lines; /**< Maximum number of messages to record */
	unsigned long max_time; /**< Maximum number of time (in seconds) to record */
};

Cmode_t EXTMODE_HISTORY = 0L;
#define HistoryEnabled(channel)    (channel->mode.extmode & EXTMODE_HISTORY)

/* Forward declarations */
static void init_config(void);
int history_config_test(ConfigFile *, ConfigEntry *, int, int *);
int history_config_run(ConfigFile *, ConfigEntry *, int);
int history_chanmode_change(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
static int compare_history_modes(HistoryChanMode *a, HistoryChanMode *b);
int history_chanmode_is_ok(Client *client, Channel *channel, char mode, char *para, int type, int what);
void *history_chanmode_put_param(void *r_in, char *param);
char *history_chanmode_get_param(void *r_in);
char *history_chanmode_conv_param(char *param, Client *client);
void history_chanmode_free_param(void *r);
void *history_chanmode_dup_struct(void *r_in);
int history_chanmode_sjoin_check(Channel *channel, void *ourx, void *theirx);
int history_channel_destroy(Channel *channel, int *should_destroy);
int history_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice);
int history_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, history_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CmodeInfo creq;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&creq, 0, sizeof(creq));
	creq.paracount = 1;
	creq.is_ok = history_chanmode_is_ok;
	creq.flag = 'H';
	creq.put_param = history_chanmode_put_param;
	creq.get_param = history_chanmode_get_param;
	creq.conv_param = history_chanmode_conv_param;
	creq.free_param = history_chanmode_free_param;
	creq.dup_struct = history_chanmode_dup_struct;
	creq.sjoin_check = history_chanmode_sjoin_check;
	CmodeAdd(modinfo->handle, creq, &EXTMODE_HISTORY);

	init_config();

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, history_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CHANMODE, 0, history_chanmode_change);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CHANMODE, 0, history_chanmode_change);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, history_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, history_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 1000000, history_channel_destroy);
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

static void init_config(void)
{
	/* Set default values */
	memset(&cfg, 0, sizeof(cfg));
	cfg.playback_on_join.lines = 15;
	cfg.playback_on_join.time = 86400;
	cfg.max_storage_per_channel.lines = 200;
	cfg.max_storage_per_channel.time = 86400*7;
}

#define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }

int history_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp, *cep4;
	int on_join_lines=0, maximum_storage_lines=0;
	long on_join_time=0L, maximum_storage_time=0L;

	/* We only care about set::history */
	if ((type != CONFIG_SET) || strcmp(ce->ce_varname, "history"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "playback-on-join"))
				{
					for (cep4 = cepp->ce_entries; cep4; cep4 = cep4->ce_next)
					{
						if (!strcmp(cep4->ce_varname, "lines"))
						{
							int v;
							CheckNull(cep4);
							v = atoi(cep4->ce_vardata);
							if ((v < 1) || (v > 1000000000))
							{
								config_error("%s:%i: set::history::channel::playback-on-join::lines must be between 1 and 1000. "
								             "Recommended values are 10-50. Got: %d.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum, v);
								errors++;
								continue;
							}
							on_join_lines = v;
						} else
						if (!strcmp(cep4->ce_varname, "time"))
						{
							long v;
							CheckNull(cep4);
							v = config_checkval(cep4->ce_vardata, CFG_TIME);
							if (v < 1)
							{
								config_error("%s:%i: set::history::channel::playback-on-join::time must be a positive number.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
								errors++;
								continue;
							}
							on_join_time = v;
						} else
						{
							config_error_unknown(cep4->ce_fileptr->cf_filename,
								cep4->ce_varlinenum, "set::history::channel::playback-on-join", cep4->ce_varname);
							errors++;
						}
					}
				} else
				if (!strcmp(cepp->ce_varname, "max-storage-per-channel"))
				{
					for (cep4 = cepp->ce_entries; cep4; cep4 = cep4->ce_next)
					{
						if (!strcmp(cep4->ce_varname, "lines"))
						{
							int v;
							CheckNull(cep4);
							v = atoi(cep4->ce_vardata);
							if (v < 1)
							{
								config_error("%s:%i: set::history::channel::max-storage-per-channel::lines must be a positive number.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
								errors++;
								continue;
							}
							maximum_storage_lines = v;
						} else
						if (!strcmp(cep4->ce_varname, "time"))
						{
							long v;
							CheckNull(cep4);
							v = config_checkval(cep4->ce_vardata, CFG_TIME);
							if (v < 1)
							{
								config_error("%s:%i: set::history::channel::max-storage-per-channel::time must be a positive number.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
								errors++;
								continue;
							}
							maximum_storage_time = v;
						} else
						{
							config_error_unknown(cep4->ce_fileptr->cf_filename,
								cep4->ce_varlinenum, "set::history::channel::max-storage-per-channel", cep4->ce_varname);
							errors++;
						}
					}
				} else
				{
					config_error_unknown(cepp->ce_fileptr->cf_filename,
						cepp->ce_varlinenum, "set::history::channel", cepp->ce_varname);
					errors++;
				}
			}
		} else {
			config_error_unknown(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "set::history", cep->ce_varname);
			errors++;
		}
	}

	if ((on_join_time && maximum_storage_time) && (on_join_time > maximum_storage_time))
	{
		config_error("set::history::channel::playback-on-join::time cannot be higher than set::history::channel::max-storage-per-channel::time. Either set the playback-on-join::time lower or the maximum::time higher.");
		errors++;
	}
	if ((on_join_lines && maximum_storage_lines) && (on_join_lines > maximum_storage_lines))
	{
		config_error("set::history::channel::playback-on-join::lines cannot be higher than set::history::channel::max-storage-per-channel::lines. Either set the playback-on-join::lines lower or the maximum::lines higher.");
		errors++;
	}
	*errs = errors;
	return errors ? -1 : 1;
}

int history_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp, *cep4;

	if ((type != CONFIG_SET) || strcmp(ce->ce_varname, "history"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "channel"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				if (!strcmp(cepp->ce_varname, "playback-on-join"))
				{
					for (cep4 = cepp->ce_entries; cep4; cep4 = cep4->ce_next)
					{
						if (!strcmp(cep4->ce_varname, "lines"))
						{
							cfg.playback_on_join.lines = atoi(cep4->ce_vardata);
						} else
						if (!strcmp(cep4->ce_varname, "time"))
						{
							cfg.playback_on_join.time = config_checkval(cep4->ce_vardata, CFG_TIME);
						}
					}
				} else
				if (!strcmp(cepp->ce_varname, "max-storage-per-channel"))
				{
					for (cep4 = cepp->ce_entries; cep4; cep4 = cep4->ce_next)
					{
						if (!strcmp(cep4->ce_varname, "lines"))
						{
							cfg.max_storage_per_channel.lines = atoi(cep4->ce_vardata);
						} else
						if (!strcmp(cep4->ce_varname, "time"))
						{
							cfg.max_storage_per_channel.time = config_checkval(cep4->ce_vardata, CFG_TIME);
						}
					}
				}
			}
		}
	}

	return 0; /* Retval 0 = trick so other modules can see the same configuration */
}

/** Helper function for .is_ok(), .conv_param() and .put_param().
 * @param param: The mode parameter.
 * @param lines: The number of lines (the X in +H X:Y)
 * @param t:     The time value (the Y in +H X:Y)
  */
int history_parse_chanmode(char *param, int *lines, long *t)
{
	char buf[64], *p, *q;
	char contains_non_digit = 0;

	/* Work on a copy */
	strlcpy(buf, param, sizeof(buf));

	/* Initialize, to be safe */
	*lines = 0;
	*t = 0;

	p = strchr(buf, ':');
	if (!p)
		return 0;

	/* Parse lines */
	*p++ = '\0';
	*lines = atoi(buf);

	/* Parse time value */
	/* If it is all digits then it is in minutes */
	for (q=p; *q; q++)
	{
		if (!isdigit(*q))
		{
			contains_non_digit = 1;
			break;
		}
	}
	if (contains_non_digit)
		*t = config_checkval(p, CFG_TIME);
	else
		*t = atoi(p) * 60;

	/* Sanity checking... */
	if (*lines < 1)
		return 0;

	if (*t < 60)
		return 0;

	/* Check imposed configuration limits... */
	if (*lines > cfg.max_storage_per_channel.lines)
		*lines = cfg.max_storage_per_channel.lines;

	if (*t > cfg.max_storage_per_channel.time)
		*t = cfg.max_storage_per_channel.time;

	return 1;
}

/** Channel Mode +H check:
 * Does the user have rights to add/remove this channel mode?
 * Is the supplied mode parameter ok?
 */
int history_chanmode_is_ok(Client *client, Channel *channel, char mode, char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsUser(client) && is_chan_op(client, channel))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(client, ERR_NOTFORHALFOPS, 'H');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		int lines = 0;
		long t = 0L;

		if (!history_parse_chanmode(param, &lines, &t))
		{
			sendnumeric(client, ERR_CANNOTCHANGECHANMODE, 'H', "Invalid syntax for MODE +H. Use +H lines:period. The period must be in minutes (eg: 10) or a time value (eg: 1h).");
			return EX_DENY;
		}
		/* Don't bother about lines/t limits here, we will auto-convert in .conv_param */

		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

/** Convert channel parameter to something proper.
 * NOTE: client may be NULL if called for e.g. set::modes-playback-on-join
 */
char *history_chanmode_conv_param(char *param, Client *client)
{
	static char buf[64];
	int lines = 0;
	long t = 0L;

	if (!history_parse_chanmode(param, &lines, &t))
		return NULL;

	snprintf(buf, sizeof(buf), "%d:%ldm", lines, t / 60);
	return buf;
}

/** Store the +H x:y channel mode */
void *history_chanmode_put_param(void *mode_in, char *param)
{
	HistoryChanMode *h = (HistoryChanMode *)mode_in;
	int lines = 0;
	long t = 0L;

	if (!history_parse_chanmode(param, &lines, &t))
		return NULL;

	if (!h)
	{
		/* Need to create one */
		h = safe_alloc(sizeof(HistoryChanMode));
	}

	h->max_lines = lines;
	h->max_time = t;

	return (void *)h;
}

/** Retrieve the +H settings (the X:Y string) */
char *history_chanmode_get_param(void *h_in)
{
	HistoryChanMode *h = (HistoryChanMode *)h_in;
	static char buf[64];

	if (!h_in)
		return NULL;

	/* For now we convert the time to minutes for displaying purposes
	 * and show it as eg 5:10m.
	 * In a later release we can have a go at converting to '1h', '1d'
	 * and such, but not before most people run 5.0.2+ as otherwise you
	 * get desyncs in channel history retention times.
	 */
	snprintf(buf, sizeof(buf), "%d:%ldm", h->max_lines, h->max_time / 60);
	return buf;
}

/** Free channel mode */
void history_chanmode_free_param(void *r)
{
	safe_free(r);
}

/** Duplicate the channel mode +H settings */
void *history_chanmode_dup_struct(void *r_in)
{
	HistoryChanMode *r = (HistoryChanMode *)r_in;
	HistoryChanMode *w = safe_alloc(sizeof(HistoryChanMode));

	memcpy(w, r, sizeof(HistoryChanMode));
	return (void *)w;
}

/** If two servers with an identical creation time stamp connect,
 * we have to deal with merging the settings on different sides
 * (if they differ at all). That's what we do here.
 */
int history_chanmode_sjoin_check(Channel *channel, void *ourx, void *theirx)
{
	HistoryChanMode *our = (HistoryChanMode *)ourx;
	HistoryChanMode *their = (HistoryChanMode *)theirx;

	if ((our->max_lines == their->max_lines) && (our->max_time == their->max_time))
		return EXSJ_SAME;

	our->max_lines = MAX(our->max_lines, their->max_lines);
	our->max_time = MAX(our->max_time, their->max_time);

	return EXSJ_MERGE;
}

/** On channel mode change, communicate the +H limits to the history backend layer */
int history_chanmode_change(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode)
{
	HistoryChanMode *settings;

	/* Did anything change, with regards to channel mode H ? */
	if (!strchr(modebuf, 'H'))
		return 0;

	/* If so, grab the settings, and communicate them */
	settings = (HistoryChanMode *)GETPARASTRUCT(channel, 'H');
	if (settings)
		history_set_limit(channel->chname, settings->max_lines, settings->max_time);
	else
		history_destroy(channel->chname);

	return 0;
}

/** Channel is destroyed (or is it?) */
int history_channel_destroy(Channel *channel, int *should_destroy)
{
	if (*should_destroy == 0)
		return 0; /* channel will not be destroyed */

	history_destroy(channel->chname);

	return 0;
}

int history_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice)
{
	char buf[512];
	char source[64];
	HistoryChanMode *settings;

	if (!HistoryEnabled(channel))
		return 0;

	/* Filter out CTCP / CTCP REPLY */
	if ((*text == '\001') && strncmp(text+1, "ACTION", 6))
		return 0;

	/* Lazy: if any prefix is addressed (eg: @#channel) then don't record it.
	 * This so we don't have to check privileges during history playback etc.
	 */
	if (prefix)
		return 0;

	if (IsUser(client))
		snprintf(source, sizeof(source), "%s!%s@%s", client->name, client->user->username, GetHost(client));
	else
		strlcpy(source, client->name, sizeof(source));

	snprintf(buf, sizeof(buf), ":%s %s %s :%s",
		source,
		notice ? "NOTICE" : "PRIVMSG",
		channel->chname,
		text);

	history_add(channel->chname, mtags, buf);

	return 0;
}

int history_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[])
{
	if (!HistoryEnabled(channel))
		return 0;

	if (MyUser(client))
		history_request(client, channel->chname, NULL);

	return 0;
}
