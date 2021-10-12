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
typedef struct cfgstruct cfgstruct;
struct cfgstruct {
	ConfigHistoryExt playback_on_join; /**< Maximum number of lines & time to playback on-join */
	ConfigHistoryExt max_storage_per_channel_registered; /**< Maximum number of lines & time to record for +r channels*/
	ConfigHistoryExt max_storage_per_channel_unregistered; /**< Maximum number of lines & time to record for -r channels */
};

typedef struct HistoryChanMode HistoryChanMode;
struct HistoryChanMode {
	unsigned int max_lines; /**< Maximum number of messages to record */
	unsigned long max_time; /**< Maximum number of time (in seconds) to record */
};

/* Global variables */
Cmode_t EXTMODE_HISTORY = 0L;
static cfgstruct cfg;
static cfgstruct test;

#define HistoryEnabled(channel)    (channel->mode.extmode & EXTMODE_HISTORY)

/* Forward declarations */
static void init_config(cfgstruct *cfg);
int history_config_test(ConfigFile *, ConfigEntry *, int, int *);
int history_config_posttest(int *);
int history_config_run(ConfigFile *, ConfigEntry *, int);
int history_chanmode_change(Client *client, Channel *channel, MessageTag *mtags, char *modebuf, char *parabuf, time_t sendts, int samode);
static int compare_history_modes(HistoryChanMode *a, HistoryChanMode *b);
int history_chanmode_is_ok(Client *client, Channel *channel, char mode, char *para, int type, int what);
void *history_chanmode_put_param(void *r_in, char *param);
char *history_chanmode_get_param(void *r_in);
char *history_chanmode_conv_param(char *param, Client *client, Channel *channel);
void history_chanmode_free_param(void *r);
void *history_chanmode_dup_struct(void *r_in);
int history_chanmode_sjoin_check(Channel *channel, void *ourx, void *theirx);
int history_channel_destroy(Channel *channel, int *should_destroy);
int history_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, SendType sendtype);
int history_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[]);
CMD_OVERRIDE_FUNC(override_mode);

MOD_TEST()
{
	init_config(&test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, history_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, history_config_posttest);

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

	init_config(&cfg);

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
	CommandOverrideAdd(modinfo->handle, "MODE", override_mode);
	CommandOverrideAdd(modinfo->handle, "SVSMODE", override_mode);
	CommandOverrideAdd(modinfo->handle, "SVS2MODE", override_mode);
	CommandOverrideAdd(modinfo->handle, "SAMODE", override_mode);
	CommandOverrideAdd(modinfo->handle, "SJOIN", override_mode);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

static void init_config(cfgstruct *cfg)
{
	/* Set default values */
	memset(cfg, 0, sizeof(cfgstruct));
	cfg->playback_on_join.lines = 15;
	cfg->playback_on_join.time = 86400;
	cfg->max_storage_per_channel_unregistered.lines = 200;
	cfg->max_storage_per_channel_unregistered.time = 86400*31;
	cfg->max_storage_per_channel_registered.lines = 5000;
	cfg->max_storage_per_channel_registered.time = 86400*31;
}

#define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }

int history_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp, *cep4, *cep5;
	int on_join_lines=0, maximum_storage_lines_registered=0, maximum_storage_lines_unregistered=0;
	long on_join_time=0L, maximum_storage_time_registered=0L, maximum_storage_time_unregistered=0L;

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
							if ((v < 0) || (v > 1000))
							{
								config_error("%s:%i: set::history::channel::playback-on-join::lines must be between 0 and 1000. "
								             "Recommended values are 10-50. Got: %d.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum, v);
								errors++;
								continue;
							}
							test.playback_on_join.lines = v;
						} else
						if (!strcmp(cep4->ce_varname, "time"))
						{
							long v;
							CheckNull(cep4);
							v = config_checkval(cep4->ce_vardata, CFG_TIME);
							if (v < 0)
							{
								config_error("%s:%i: set::history::channel::playback-on-join::time must be zero or more.",
								             cep4->ce_fileptr->cf_filename, cep4->ce_varlinenum);
								errors++;
								continue;
							}
							test.playback_on_join.time = v;
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
						if (!strcmp(cep4->ce_varname, "registered"))
						{
							for (cep5 = cep4->ce_entries; cep5; cep5 = cep5->ce_next)
							{
								if (!strcmp(cep5->ce_varname, "lines"))
								{
									int v;
									CheckNull(cep5);
									v = atoi(cep5->ce_vardata);
									if (v < 1)
									{
										config_error("%s:%i: set::history::channel::max-storage-per-channel::registered::lines must be a positive number.",
											     cep5->ce_fileptr->cf_filename, cep5->ce_varlinenum);
										errors++;
										continue;
									}
									test.max_storage_per_channel_registered.lines = v;
								} else
								if (!strcmp(cep5->ce_varname, "time"))
								{
									long v;
									CheckNull(cep5);
									v = config_checkval(cep5->ce_vardata, CFG_TIME);
									if (v < 1)
									{
										config_error("%s:%i: set::history::channel::max-storage-per-channel::registered::time must be a positive number.",
											     cep5->ce_fileptr->cf_filename, cep5->ce_varlinenum);
										errors++;
										continue;
									}
									test.max_storage_per_channel_registered.time = v;
								} else
								{
									config_error_unknown(cep5->ce_fileptr->cf_filename,
										cep5->ce_varlinenum, "set::history::channel::max-storage-per-channel::registered", cep5->ce_varname);
									errors++;
								}
							}
						} else
						if (!strcmp(cep4->ce_varname, "unregistered"))
						{
							for (cep5 = cep4->ce_entries; cep5; cep5 = cep5->ce_next)
							{
								if (!strcmp(cep5->ce_varname, "lines"))
								{
									int v;
									CheckNull(cep5);
									v = atoi(cep5->ce_vardata);
									if (v < 1)
									{
										config_error("%s:%i: set::history::channel::max-storage-per-channel::unregistered::lines must be a positive number.",
											     cep5->ce_fileptr->cf_filename, cep5->ce_varlinenum);
										errors++;
										continue;
									}
									test.max_storage_per_channel_unregistered.lines = v;
								} else
								if (!strcmp(cep5->ce_varname, "time"))
								{
									long v;
									CheckNull(cep5);
									v = config_checkval(cep5->ce_vardata, CFG_TIME);
									if (v < 1)
									{
										config_error("%s:%i: set::history::channel::max-storage-per-channel::unregistered::time must be a positive number.",
											     cep5->ce_fileptr->cf_filename, cep5->ce_varlinenum);
										errors++;
										continue;
									}
									test.max_storage_per_channel_unregistered.time = v;
								} else
								{
									config_error_unknown(cep5->ce_fileptr->cf_filename,
										cep5->ce_varlinenum, "set::history::channel::max-storage-per-channel::unregistered", cep5->ce_varname);
									errors++;
								}
							}
						} else
						{
							config_error_unknown(cep->ce_fileptr->cf_filename,
								cep->ce_varlinenum, "set::history::max-storage-per-channel", cep->ce_varname);
							errors++;
						}
					}
				} else
				{
					/* hmm.. I don't like this method. but I just quickly copied it from CONFIG_ALLOW for now... */
					int used = 0;
					Hook *h;
					for (h = Hooks[HOOKTYPE_CONFIGTEST]; h; h = h->next)
					{
						int value, errs = 0;
						if (h->owner && !(h->owner->flags & MODFLAG_TESTING)
							&& !(h->owner->options & MOD_OPT_PERM))
							continue;
						value = (*(h->func.intfunc))(cf, cepp, CONFIG_SET_HISTORY_CHANNEL, &errs);
						if (value == 2)
							used = 1;
						if (value == 1)
						{
							used = 1;
							break;
						}
						if (value == -1)
						{
							used = 1;
							errors += errs;
							break;
						}
						if (value == -2)
						{
							used = 1;
							errors += errs;
						}
					}
					if (!used)
					{
						config_error_unknown(cepp->ce_fileptr->cf_filename,
							cepp->ce_varlinenum, "set::history::channel", cepp->ce_varname);
						errors++;
					}
				}
			}
		} else {
			config_error_unknown(cep->ce_fileptr->cf_filename,
				cep->ce_varlinenum, "set::history", cep->ce_varname);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int history_config_posttest(int *errs)
{
	int errors = 0;

	/* We could check here for on join lines / on join time being bigger than max storage but..
	 * not really important.
	 */

	*errs = errors;
	return errors ? -1 : 1;
}

int history_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp, *cep4, *cep5;

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
						if (!strcmp(cep4->ce_varname, "registered"))
						{
							for (cep5 = cep4->ce_entries; cep5; cep5 = cep5->ce_next)
							{
								if (!strcmp(cep5->ce_varname, "lines"))
								{
									cfg.max_storage_per_channel_registered.lines = atoi(cep5->ce_vardata);
								} else
								if (!strcmp(cep5->ce_varname, "time"))
								{
									cfg.max_storage_per_channel_registered.time = config_checkval(cep5->ce_vardata, CFG_TIME);
								}
							}
						} else
						if (!strcmp(cep4->ce_varname, "unregistered"))
						{
							for (cep5 = cep4->ce_entries; cep5; cep5 = cep5->ce_next)
							{
								if (!strcmp(cep5->ce_varname, "lines"))
								{
									cfg.max_storage_per_channel_unregistered.lines = atoi(cep5->ce_vardata);
								} else
								if (!strcmp(cep5->ce_varname, "time"))
								{
									cfg.max_storage_per_channel_unregistered.time = config_checkval(cep5->ce_vardata, CFG_TIME);
								}
							}
						}
					}
				} else
				{
					Hook *h;
					for (h = Hooks[HOOKTYPE_CONFIGRUN]; h; h = h->next)
					{
						int value = (*(h->func.intfunc))(cf, cepp, CONFIG_SET_HISTORY_CHANNEL);
						if (value == 1)
							break;
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
int history_parse_chanmode(Channel *channel, char *param, int *lines, long *t)
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
	if (!channel || has_channel_mode(channel, 'r'))
	{
		if (*lines > cfg.max_storage_per_channel_registered.lines)
			*lines = cfg.max_storage_per_channel_registered.lines;

		if (*t > cfg.max_storage_per_channel_registered.time)
			*t = cfg.max_storage_per_channel_registered.time;
	} else {
		if (*lines > cfg.max_storage_per_channel_unregistered.lines)
			*lines = cfg.max_storage_per_channel_unregistered.lines;

		if (*t > cfg.max_storage_per_channel_unregistered.time)
			*t = cfg.max_storage_per_channel_unregistered.time;
	}
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

		if (!history_parse_chanmode(channel, param, &lines, &t))
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

static void history_chanmode_helper(char *buf, size_t bufsize, int lines, long t)
{
	if ((t % 86400) == 0)
	{
		/* Can be represented in full days, eg "1d" */
		snprintf(buf, bufsize, "%d:%ldd", lines, t / 86400);
	} else
	if ((t % 3600) == 0)
	{
		/* Can be represented in hours, eg "8h" */
		snprintf(buf, bufsize, "%d:%ldh", lines, t / 3600);
	} else
	{
		/* Otherwise, stick to minutes */
		snprintf(buf, bufsize, "%d:%ldm", lines, t / 60);
	}
}

/** Convert channel parameter to something proper.
 * NOTE: client may be NULL if called for e.g. set::modes-playback-on-join
 */
char *history_chanmode_conv_param(char *param, Client *client, Channel *channel)
{
	static char buf[64];
	int lines = 0;
	long t = 0L;

	if (!history_parse_chanmode(channel, param, &lines, &t))
		return NULL;

	history_chanmode_helper(buf, sizeof(buf), lines, t);
	return buf;
}

/** Store the +H x:y channel mode */
void *history_chanmode_put_param(void *mode_in, char *param)
{
	HistoryChanMode *h = (HistoryChanMode *)mode_in;
	int lines = 0;
	long t = 0L;

	if (!history_parse_chanmode(NULL, param, &lines, &t))
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

	history_chanmode_helper(buf, sizeof(buf), h->max_lines, h->max_time);
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

int history_chanmsg(Client *client, Channel *channel, int sendflags, int prefix, char *target, MessageTag *mtags, char *text, SendType sendtype)
{
	char buf[512];
	char source[64];
	HistoryChanMode *settings;

	if (!HistoryEnabled(channel))
		return 0;

	/* Filter out CTCP / CTCP REPLY */
	if ((*text == '\001') && strncmp(text+1, "ACTION", 6))
		return 0;

	/* Filter out TAGMSG */
	if (sendtype == SEND_TYPE_TAGMSG)
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
		sendtype_to_cmd(sendtype),
		channel->chname,
		text);

	history_add(channel->chname, mtags, buf);

	return 0;
}

int history_join(Client *client, Channel *channel, MessageTag *mtags, char *parv[])
{
	/* Only for +H channels */
	if (!HistoryEnabled(channel) || !cfg.playback_on_join.lines || !cfg.playback_on_join.time)
		return 0;

	/* No history-on-join for clients that implement CHATHISTORY,
	 * they will pull history themselves if they need it.
	 */
	if (HasCapability(client, "draft/chathistory") || HasCapability(client, "chathistory"))
		return 0;

	if (MyUser(client) && can_receive_history(client))
	{
		HistoryFilter filter;
		HistoryResult *r;
		memset(&filter, 0, sizeof(filter));
		filter.cmd = HFC_SIMPLE;
		filter.last_lines = cfg.playback_on_join.lines;
		filter.last_seconds = cfg.playback_on_join.time;
		r = history_request(channel->chname, &filter);
		if (r)
		{
			history_send_result(client, r);
			free_history_result(r);
		}
	}

	return 0;
}

/** Check if a channel went from +r to -r and adjust +H if needed.
 * This does not only override "MODE" but also "SAMODE", "SJOIN" and more.
 */
CMD_OVERRIDE_FUNC(override_mode)
{
	Channel *channel;
	int had_r = 0;

	/* We only bother checking for this corner case if the -r
	 * comes from a server directly linked to us, this normally
	 * means: we are the server that services are linked to.
	 */
	if ((IsServer(client) && client->local) ||
	    (IsUser(client) && client->srvptr && client->srvptr->local))
	{
		/* Now check if the channel is currently +r */
		if ((parc >= 2) && !BadPtr(parv[1]) && ((channel = find_channel(parv[1], NULL))) &&
		    has_channel_mode(channel, 'r'))
		{
			had_r = 1;
		}
	}
	CallCommandOverride(ovr, client, recv_mtags, parc, parv);

	/* If..
	 * - channel was +r
	 * - re-lookup the channel and check that it still
	 *   exists (as it may have been destroyed)
	 * - and is now -r
	 * - and has +H set
	 * then...
	 */
	if (had_r &&
	    ((channel = find_channel(parv[1], NULL))) &&
	    !has_channel_mode(channel, 'r') &&
	    HistoryEnabled(channel))
	{
		/* Check if limit is higher than allowed for unregistered channels */
		HistoryChanMode *settings = (HistoryChanMode *)GETPARASTRUCT(channel, 'H');
		int changed = 0;

		if (!settings)
			return; /* Weird */

		if (settings->max_lines > cfg.max_storage_per_channel_unregistered.lines)
		{
			settings->max_lines = cfg.max_storage_per_channel_unregistered.lines;
			changed = 1;
		}

		if (settings->max_time > cfg.max_storage_per_channel_unregistered.time)
		{
			settings->max_time = cfg.max_storage_per_channel_unregistered.time;
			changed = 1;
		}

		if (changed)
		{
			MessageTag *mtags = NULL;
			char *params = history_chanmode_get_param(settings);

			if (!params)
				return; /* Weird */

			strlcpy(modebuf, "+H", sizeof(modebuf));
			strlcpy(parabuf, params, sizeof(modebuf));

			new_message(&me, NULL, &mtags);

			sendto_channel(channel, &me, &me, 0, 0, SEND_LOCAL, mtags,
				       ":%s MODE %s %s %s",
				       me.name, channel->chname, modebuf, parabuf);
			sendto_server(NULL, 0, 0, mtags, ":%s MODE %s %s %s %lld",
				me.id, channel->chname, modebuf, parabuf,
				(long long)channel->creationtime);

			/* Activate this hook just like cmd_mode.c */
			RunHook7(HOOKTYPE_REMOTE_CHANMODE, &me, channel, mtags, modebuf, parabuf, 0, 0);

			free_message_tags(mtags);

			*modebuf = *parabuf = '\0';
		}
	}
}
