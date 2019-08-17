/*
 * modules/chanmodes/history - Channel History
 * (C) Copyright 2009-2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER(history)
  = {
	"chanmodes/history",
	"1.0",
	"Channel Mode +H",
	"3.2-b8-1",
	NULL,
    };

typedef struct _config_history_ext ConfigHistoryExt;
struct _config_history_ext {
	int lines;
	long time;
};
struct {
	ConfigHistoryExt playback_on_join; /**< Maximum number of lines & time to playback on-join */
	ConfigHistoryExt max_storage_per_channel; /**< Maximum number of lines & time to record */
} cfg;

typedef struct _historychanmode HistoryChanMode;
struct _historychanmode {
	unsigned int max_lines; /**< Maximum number of messages to record */
	unsigned long max_time; /**< Maximum number of time to record */
};

Cmode_t EXTMODE_HISTORY = 0L;
#define HistoryEnabled(chptr)    (chptr->mode.extmode & EXTMODE_HISTORY)

/* The regular history cleaning (by timer) is spread out
 * a bit, rather than doing ALL channels every T time.
 * HISTORY_SPREAD: how much to spread the "cleaning", eg 1 would be
 *  to clean everything in 1 go, 2 would mean the first event would
 *  clean half of the channels, and the 2nd event would clean the rest.
 *  Obviously more = better to spread the load, but doing a reasonable
 *  amount of work is also benefitial for performance (think: CPU cache).
 * HISTORY_MAX_OFF_SECS: how many seconds may the history be 'off',
 *  that is: how much may we store the history longer than required.
 * The other 2 macros are calculated based on that target.
 */
#define HISTORY_SPREAD	16
#define HISTORY_MAX_OFF_SECS	128
#define HISTORY_CLEAN_PER_LOOP	(CHAN_HASH_TABLE_SIZE/HISTORY_SPREAD)
#define HISTORY_TIMER_EVERY	(HISTORY_MAX_OFF_SECS/HISTORY_SPREAD)

/* Forward declarations */
static void init_config(void);
int history_config_test(ConfigFile *, ConfigEntry *, int, int *);
int history_config_run(ConfigFile *, ConfigEntry *, int);
static int compare_history_modes(HistoryChanMode *a, HistoryChanMode *b);
int history_chanmode_is_ok(aClient *sptr, aChannel *chptr, char mode, char *para, int type, int what);
void *history_chanmode_put_param(void *r_in, char *param);
char *history_chanmode_get_param(void *r_in);
char *history_chanmode_conv_param(char *param, aClient *cptr);
void history_chanmode_free_param(void *r);
void *history_chanmode_dup_struct(void *r_in);
int history_chanmode_sjoin_check(aChannel *chptr, void *ourx, void *theirx);
int history_channel_destroy(aChannel *chptr, int *should_destroy);
int history_chanmsg(aClient *sptr, aChannel *chptr, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice);
int history_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[]);
EVENT(history_clean);

MOD_TEST(history)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, history_config_test);
	return MOD_SUCCESS;
}

MOD_INIT(history)
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
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, history_join);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, history_chanmsg);

	HookAdd(modinfo->handle, HOOKTYPE_CHANNEL_DESTROY, 1000000, history_channel_destroy);
	return MOD_SUCCESS;
}

MOD_LOAD(history)
{
	EventAdd(modinfo->handle, "history_clean", HISTORY_TIMER_EVERY, 0, history_clean, NULL);
	return MOD_SUCCESS;
}

MOD_UNLOAD(history)
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
	char buf[64], *p;

	/* Work on a copy */
	strlcpy(buf, param, sizeof(buf));

	/* Initialize, to be safe */
	*lines = 0;
	*t = 0;

	p = strchr(buf, ':');
	if (!p)
		return 0;

	*p++ = '\0';
	*lines = atoi(buf);
	*t = config_checkval(param, CFG_TIME);

	if (*lines < 1)
		return 0;

	if (*t < 1)
		return 0;

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
int history_chanmode_is_ok(aClient *sptr, aChannel *chptr, char mode, char *param, int type, int what)
{
	if ((type == EXCHK_ACCESS) || (type == EXCHK_ACCESS_ERR))
	{
		if (IsPerson(sptr) && is_chan_op(sptr, chptr))
			return EX_ALLOW;
		if (type == EXCHK_ACCESS_ERR) /* can only be due to being halfop */
			sendnumeric(sptr, ERR_NOTFORHALFOPS, 'H');
		return EX_DENY;
	} else
	if (type == EXCHK_PARAM)
	{
		int lines = 0;
		long t = 0L;

		if (!history_parse_chanmode(param, &lines, &t))
		{
			sendnumeric(sptr, ERR_CANNOTCHANGECHANMODE, 'H', "Invalid syntax for MODE +H. Use +H count:period");
			return EX_DENY;
		}
		/* Don't bother about lines/t limits here, we will auto-convert in .conv_param */

		return EX_ALLOW;
	}

	/* fallthrough -- should not be used */
	return EX_DENY;
}

/** Convert channel parameter to something proper.
 * NOTE: cptr may be NULL if called for e.g. set::modes-playback-on-join
 */
char *history_chanmode_conv_param(char *param, aClient *cptr)
{
	static char buf[64];
	int lines = 0;
	long t = 0L;

	if (!history_parse_chanmode(param, &lines, &t))
		return NULL;

	snprintf(buf, sizeof(buf), "%d:%ld", lines, t);
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
		h = MyMallocEx(sizeof(HistoryChanMode));
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

	/* Should we make the time value more readable (eg: 3600 -> 1h)
	 * or should we keep it easily parseable by machines/scripts.
	 * I'm leaning towards the latter. That also means there is no
	 * confusion with regards to the meaning of 'm' (minute or month)
	 * and different IRCd implementations.
	 */
	snprintf(buf, sizeof(buf), "%d:%ld", h->max_lines, h->max_time);
	return buf;
}

/** Free channel mode */
void history_chanmode_free_param(void *r)
{
	MyFree(r);
}

/** Duplicate the channel mode +H settings */
void *history_chanmode_dup_struct(void *r_in)
{
	HistoryChanMode *r = (HistoryChanMode *)r_in;
	HistoryChanMode *w = MyMallocEx(sizeof(HistoryChanMode));

	memcpy(w, r, sizeof(HistoryChanMode));
	return (void *)w;
}

/** If two servers with an identical creation time stamp connect,
 * we have to deal with merging the settings on different sides
 * (if they differ at all). That's what we do here.
 */
int history_chanmode_sjoin_check(aChannel *chptr, void *ourx, void *theirx)
{
	HistoryChanMode *our = (HistoryChanMode *)ourx;
	HistoryChanMode *their = (HistoryChanMode *)theirx;

	if ((our->max_lines == their->max_lines) && (our->max_time == their->max_time))
		return EXSJ_SAME;

	our->max_lines = MAX(our->max_lines, their->max_lines);
	our->max_time = MAX(our->max_time, their->max_time);

	return EXSJ_MERGE;
}

/** Channel is destroyed (or is it?) */
int history_channel_destroy(aChannel *chptr, int *should_destroy)
{
	if (*should_destroy == 0)
		return 0; /* channel will not be destroyed */

	history_destroy(chptr->chname);

	return 0;
}

int history_chanmsg(aClient *sptr, aChannel *chptr, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, int notice)
{
	char buf[512];
	char source[64];
	HistoryChanMode *settings;

	if (!HistoryEnabled(chptr))
		return 0;

	/* Filter out CTCP / CTCP REPLY */
	if ((*text == '\001') && strncmp(text+1, "ACTION", 6))
		return 0;

	/* Lazy: if any prefix is addressed (eg: @#channel) then don't record it.
	 * This so we don't have to check privileges during history playback etc.
	 */
	if (prefix)
		return 0;

	if (IsPerson(sptr))
		snprintf(source, sizeof(source), "%s!%s@%s", sptr->name, sptr->user->username, GetHost(sptr));
	else
		strlcpy(source, sptr->name, sizeof(source));

	snprintf(buf, sizeof(buf), ":%s %s %s :%s",
		source,
		notice ? "NOTICE" : "PRIVMSG",
		chptr->chname,
		text);

	history_add(chptr->chname, mtags, buf);
	settings = (HistoryChanMode *)GETPARASTRUCT(chptr, 'H');
	history_del(chptr->chname, settings->max_lines, settings->max_time);

	return 0;
}

int history_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *parv[])
{
	if (!HistoryEnabled(chptr))
		return 0;

	if (MyClient(sptr))
	{
		HistoryChanMode *settings = (HistoryChanMode *)GETPARASTRUCT(chptr, 'H');
		history_del(chptr->chname, settings->max_lines, settings->max_time);
		history_request(sptr, chptr->chname, NULL);
	}

	return 0;
}

/** Periodically clean the history.
 * Instead of doing all channels in 1 go, we do a limited number
 * of channels each call, hence the 'static int' and the do { } while
 * rather than a regular for loop.
 */
EVENT(history_clean)
{
	static int hashnum = 0;
	int loopcnt = 0;
	aChannel *chptr;

	do
	{
		for (chptr = hash_get_chan_bucket(hashnum); chptr; chptr = chptr->hnextch)
		{
			if (HistoryEnabled(chptr))
			{
				HistoryChanMode *settings = (HistoryChanMode *)GETPARASTRUCT(chptr, 'H');
				if (settings)
					history_del(chptr->chname, settings->max_lines, settings->max_time);
			}
		}
		hashnum++;
		if (hashnum >= CHAN_HASH_TABLE_SIZE)
			hashnum = 0;
	} while(loopcnt++ < HISTORY_CLEAN_PER_LOOP);
}
