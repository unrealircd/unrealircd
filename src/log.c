/************************************************************************
 * IRC - Internet Relay Chat, src/api-channelmode.c
 * (C) 2021 Bram Matthys (Syzop) and the UnrealIRCd Team
 *
 * See file AUTHORS in IRC package for additional names of
 * the programmers.
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

/** @file
 * @brief The logging API
 */

#define UNREAL_LOGGER_CODE
#include "unrealircd.h"

// TODO: Make configurable at compile time (runtime won't do, as we haven't read the config file)
#define show_event_console 0

/* Variables */
Log *logs[NUM_LOG_DESTINATIONS] = { NULL, NULL, NULL, NULL, NULL };
Log *temp_logs[NUM_LOG_DESTINATIONS] = { NULL, NULL, NULL, NULL, NULL };
static int snomask_num_destinations = 0;

static char snomasks_in_use[257] = { '\0' };
static char snomasks_in_use_testing[257] = { '\0' };

/* Forward declarations */
int log_sources_match(LogSource *logsource, LogLevel loglevel, const char *subsystem, const char *event_id, int matched_already);
void do_unreal_log_internal(LogLevel loglevel, const char *subsystem, const char *event_id, Client *client, int expand_msg, const char *msg, va_list vl);
void log_blocks_switchover(void);

/** Calculate expansion of a JSON string thanks to double escaping.
 * orig => JSON => IRC
 *    " => \"   => \\"
 *    \ => \\   => \\\\
 */
int json_dump_string_length(const char *s)
{
	int len = 0;
	for (; *s; s++)
	{
		if (*s == '\\')
			len += 4;
		else if (*s == '"')
			len += 3;
		else
			len++;
	}
	return len;
}

/** Convert a regular string value to a JSON string.
 * In UnrealIRCd, this must be used instead of json_string()
 * as we may use non-UTF8 sequences. Also, this takes care
 * of using json_null() if the string was NULL, which is
 * usually what we want as well.
 * @param s	Input string
 * @returns a json string value or json null value.
 */
json_t *json_string_unreal(const char *s)
{
	char buf1[512], buf2[512];
	char *verified_s;
	const char *stripped;

	if (s == NULL)
		return json_null();

	stripped = StripControlCodesEx(s, buf1, sizeof(buf1), UNRL_STRIP_LOW_ASCII|UNRL_STRIP_KEEP_LF);
	verified_s = unrl_utf8_make_valid(buf1, buf2, sizeof(buf2), 0);

	return json_string(verified_s);
}

const char *json_object_get_string(json_t *j, const char *name)
{
	json_t *v = json_object_get(j, name);
	return v ? json_string_value(v) : NULL;
}

#define json_string __BAD___DO__NOT__USE__JSON__STRING__PLZ

json_t *json_timestamp(time_t v)
{
	const char *ts = timestamp_iso8601(v);
	if (ts)
		return json_string_unreal(ts);
	return json_null();
}

LogType log_type_stringtoval(const char *str)
{
	if (!strcmp(str, "json"))
		return LOG_TYPE_JSON;
	if (!strcmp(str, "text"))
		return LOG_TYPE_TEXT;
	return LOG_TYPE_INVALID;
}

const char *log_type_valtostring(LogType v)
{
	switch(v)
	{
		case LOG_TYPE_TEXT:
			return "text";
		case LOG_TYPE_JSON:
			return "json";
		default:
			return NULL;
	}
}

/***** CONFIGURATION ******/

LogSource *add_log_source(const char *str)
{
	LogSource *ls;
	char buf[256];
	char *p;
	LogLevel loglevel = ULOG_INVALID;
	char *subsystem = NULL;
	char *event_id = NULL;
	int negative = 0;

	if (*str == '!')
	{
		negative = 1;
		strlcpy(buf, str+1, sizeof(buf));
	} else
	{
		strlcpy(buf, str, sizeof(buf));
	}

	p = strchr(buf, '.');
	if (p)
		*p++ = '\0';

	loglevel = log_level_stringtoval(buf);
	if (loglevel == ULOG_INVALID)
	{
		if (isupper(*buf))
			event_id = buf;
		else
			subsystem = buf;
	}
	if (p)
	{
		if (isupper(*p))
		{
			event_id = p;
		} else
		if (loglevel == ULOG_INVALID)
		{
			loglevel = log_level_stringtoval(p);
			if ((loglevel == ULOG_INVALID) && !subsystem)
				subsystem = p;
		} else if (!subsystem)
		{
			subsystem = p;
		}
	}
	ls = safe_alloc(sizeof(LogSource));
	ls->loglevel = loglevel;
	ls->negative = negative;
	if (!BadPtr(subsystem))
		strlcpy(ls->subsystem, subsystem, sizeof(ls->subsystem));
	if (!BadPtr(event_id))
		strlcpy(ls->event_id, event_id, sizeof(ls->event_id));

	return ls;
}

void free_log_source(LogSource *l)
{
	safe_free(l);
}

void free_log_sources(LogSource *l)
{
	LogSource *l_next;
	for (; l; l = l_next)
	{
		l_next = l->next;
		free_log_source(l);
	}
}

int config_test_log(ConfigFile *conf, ConfigEntry *block)
{
	int errors = 0;
	int any_sources = 0;
	ConfigEntry *ce, *cep, *cepp;
	int destinations = 0;

	for (ce = block->items; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "source"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				/* TODO: Validate the sources lightly for formatting issues */
				any_sources = 1;
			}
		} else
		if (!strcmp(ce->name, "destination"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "snomask"))
				{
					destinations++;
					snomask_num_destinations++;
					/* We need to validate the parameter here as well */
					if (!cep->value)
					{
						config_error_blank(cep->file->filename, cep->line_number, "set::logging::snomask");
						errors++;
					} else
					if ((strlen(cep->value) != 1) || !(islower(cep->value[0]) || isupper(cep->value[0])))
					{
						config_error("%s:%d: snomask must be a single letter",
							cep->file->filename, cep->line_number);
						errors++;
					} else {
						strlcat(snomasks_in_use_testing, cep->value, sizeof(snomasks_in_use_testing));
					}
				} else
				if (!strcmp(cep->name, "channel"))
				{
					destinations++;
					/* We need to validate the parameter here as well */
					if (!cep->value)
					{
						config_error_blank(cep->file->filename, cep->line_number, "set::logging::channel");
						errors++;
					} else
					if (!valid_channelname(cep->value))
					{
						config_error("%s:%d: Invalid channel name '%s'",
							cep->file->filename, cep->line_number, cep->value);
						errors++;
					}
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcmp(cepp->name, "color"))
							;
						else if (!strcmp(cepp->name, "show-event"))
							;
						else if (!strcmp(cepp->name, "json-message-tag"))
							;
						else if (!strcmp(cepp->name, "oper-only"))
							;
						else
						{
							config_error_unknown(cepp->file->filename, cepp->line_number, "log::destination::channel", cepp->name);
							errors++;
						}
					}
				} else
				if (!strcmp(cep->name, "file"))
				{
					destinations++;
					if (!cep->value)
					{
						config_error_blank(cep->file->filename, cep->line_number, "set::logging::file");
						errors++;
						continue;
					}
					convert_to_absolute_path(&cep->value, LOGDIR);
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcmp(cepp->name, "type"))
						{
							if (!cepp->value)
							{
								config_error_empty(cepp->file->filename,
									cepp->line_number, "log", cepp->name);
								errors++;
								continue;
							}
							if (!log_type_stringtoval(cepp->value))
							{
								config_error("%s:%i: unknown log type '%s'",
									cepp->file->filename, cepp->line_number,
									cepp->value);
								errors++;
							}
						} else
						if (!strcmp(cepp->name, "maxsize"))
						{
							if (!cepp->value)
							{
								config_error_empty(cepp->file->filename,
									cepp->line_number, "log", cepp->name);
								errors++;
							}
						} else
						{
							config_error_unknown(cepp->file->filename, cepp->line_number, "log::destination::file", cepp->name);
							errors++;
						}
					}
				} else
				if (!strcmp(cep->name, "remote"))
				{
					destinations++;
				} else
				if (!strcmp(cep->name, "syslog"))
				{
					destinations++;
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcmp(cepp->name, "type"))
						{
							if (!cepp->value)
							{
								config_error_empty(cepp->file->filename,
									cepp->line_number, "log", cepp->name);
								errors++;
								continue;
							}
							if (!log_type_stringtoval(cepp->value))
							{
								config_error("%s:%i: unknown log type '%s'",
									cepp->file->filename, cepp->line_number,
									cepp->value);
								errors++;
							}
						} else
						{
							config_error_unknown(cepp->file->filename, cepp->line_number, "log::destination::syslog", cepp->name);
							errors++;
						}
					}
				} else
				{
					config_error_unknownopt(cep->file->filename, cep->line_number, "log::destination", cep->name);
					errors++;
					continue;
				}
			}
		} else
		{
			config_error_unknown(ce->file->filename, ce->line_number, "log", ce->name);
			errors++;
		}
	}

	if (!any_sources && !destinations)
	{
		unreal_log(ULOG_ERROR, "config", "CONFIG_OLD_LOG_BLOCK", NULL,
		           "$config_file:$line_number: Your log block contains no sources and no destinations.\n"
		           "The log block changed between UnrealIRCd 5 and UnrealIRCd 6, "
		           "see https://www.unrealircd.org/docs/FAQ#old-log-block on how "
		           "to convert it to the new syntax.",
		           log_data_string("config_file", block->file->filename),
		           log_data_integer("line_number", block->line_number));
		errors++;
		return errors;
	}

	if (!any_sources)
	{
		config_error("%s:%d: log block contains no sources. Old log block perhaps?",
			block->file->filename, block->line_number);
		errors++;
	}
	if (destinations == 0)
	{
		config_error("%s:%d: log block contains no destinations. Old log block perhaps?",
			block->file->filename, block->line_number);
		errors++;
	}
	if (destinations > 1)
	{
		config_error("%s:%d: log block contains multiple destinations. This is not support... YET!",
			block->file->filename, block->line_number);
		errors++;
	}
	return errors;
}

int config_run_log(ConfigFile *conf, ConfigEntry *block)
{
	ConfigEntry *ce, *cep, *cepp;
	LogSource *sources = NULL;
	int type;

	/* If we later allow multiple destination entries later,
	 * then we need to 'clone' sources or work with reference counts.
	 */

	/* First, gather the source... */
	for (ce = block->items; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "source"))
		{
			LogSource *s;
			for (cep = ce->items; cep; cep = cep->next)
			{
				s = add_log_source(cep->name);
				AddListItem(s, sources);
			}
		}
	}

	/* Now deal with destinations... */
	for (ce = block->items; ce; ce = ce->next)
	{
		if (!strcmp(ce->name, "destination"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "snomask"))
				{
					Log *log = safe_alloc(sizeof(Log));
					strlcpy(log->destination, cep->value, sizeof(log->destination)); /* destination is the snomask */
					strlcat(snomasks_in_use, cep->value, sizeof(snomasks_in_use));
					log->sources = sources;
					if (!strcmp(cep->value, "s"))
						AddListItem(log, temp_logs[LOG_DEST_OPER]);
					else
						AddListItem(log, temp_logs[LOG_DEST_SNOMASK]);
				} else
				if (!strcmp(cep->name, "channel"))
				{
					Log *log = safe_alloc(sizeof(Log));
					strlcpy(log->destination, cep->value, sizeof(log->destination)); /* destination is the channel */
					log->sources = sources;
					AddListItem(log, temp_logs[LOG_DEST_CHANNEL]);
					/* set defaults */
					log->color = tempiConf.server_notice_colors;
					log->show_event = tempiConf.server_notice_show_event;
					log->json_message_tag = 1;
					log->oper_only = 1;
					/* now parse options (if any) */
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcmp(cepp->name, "color"))
							log->color = config_checkval(cepp->value, CFG_YESNO);
						else if (!strcmp(cepp->name, "show-event"))
							log->show_event = config_checkval(cepp->value, CFG_YESNO);
						else if (!strcmp(cepp->name, "json-message-tag"))
							log->json_message_tag = config_checkval(cepp->value, CFG_YESNO);
						else if (!strcmp(cepp->name, "oper-only"))
							log->oper_only = config_checkval(cepp->value, CFG_YESNO);
					}
				} else
				if (!strcmp(cep->name, "remote"))
				{
					Log *log = safe_alloc(sizeof(Log));
					/* destination stays empty */
					log->sources = sources;
					AddListItem(log, temp_logs[LOG_DEST_REMOTE]);
				} else
				if (!strcmp(cep->name, "file") || !strcmp(cep->name, "syslog"))
				{
					Log *log;
					/* First check if already exists... yeah this is a bit late
					 * and ideally would have been done in config_test but...
					 * that would have been lots of work for a (hopefully) rare case.
					 */
					for (log = temp_logs[LOG_DEST_DISK]; log; log = log->next)
					{
						if ((log->file && !strcmp(log->file, cep->value)) ||
						    (log->filefmt && !strcmp(log->filefmt, cep->value)))
						{
							config_warn("%s:%d: Ignoring duplicate log block for file '%s'. "
							            "You cannot have multiple log blocks logging to the same file.",
							            cep->file->filename, cep->line_number,
							            cep->value);
							free_log_sources(sources);
							return 0;
						}
					}
					log = safe_alloc(sizeof(Log));
					log->sources = sources;
					log->logfd = -1;
					log->type = LOG_TYPE_TEXT; /* default */
					if (!strcmp(cep->name, "syslog"))
						safe_strdup(log->file, "syslog");
					else if (strchr(cep->value, '%'))
						safe_strdup(log->filefmt, cep->value);
					else
						safe_strdup(log->file, cep->value);
					for (cepp = cep->items; cepp; cepp = cepp->next)
					{
						if (!strcmp(cepp->name, "maxsize"))
						{
							log->maxsize = config_checkval(cepp->value,CFG_SIZE);
						}
						else if (!strcmp(cepp->name, "type"))
						{
							log->type = log_type_stringtoval(cepp->value);
						}
					}
					AddListItem(log, temp_logs[LOG_DEST_DISK]);
				}
			}
		}
	}

	return 0;
}




/***** RUNTIME *****/

void json_expand_client_security_groups(json_t *parent, Client *client)
{
	SecurityGroup *s;
	json_t *child = json_array();
	json_object_set_new(parent, "security-groups", child);

	/* We put known-users or unknown-users at the beginning.
	 * The latter is special and doesn't actually exist
	 * in the linked list, hence the special code here,
	 * and again later in the for loop to skip it.
	 */
	if (user_allowed_by_security_group_name(client, "known-users"))
		json_array_append_new(child, json_string_unreal("known-users"));
	else
		json_array_append_new(child, json_string_unreal("unknown-users"));

	for (s = securitygroups; s; s = s->next)
		if (strcmp(s->name, "known-users") && user_allowed_by_security_group(client, s))
			json_array_append_new(child, json_string_unreal(s->name));
}

void json_expand_client(json_t *j, const char *key, Client *client, int detail)
{
	char buf[BUFSIZE+1];
	json_t *child;
	json_t *user = NULL;

	if (key)
	{
		child = json_object();
		json_object_set_new(j, key, child);
	} else {
		child = j;
	}

	/* First the information that is available for ALL client types: */

	json_object_set_new(child, "name", json_string_unreal(client->name));
	json_object_set_new(child, "id", json_string_unreal(client->id));

	/* hostname is available for all, it just depends a bit on whether it is DNS or IP */
	if (client->user && *client->user->realhost)
		json_object_set_new(child, "hostname", json_string_unreal(client->user->realhost));
	else if (client->local && *client->local->sockhost)
		json_object_set_new(child, "hostname", json_string_unreal(client->local->sockhost));
	else
		json_object_set_new(child, "hostname", json_string_unreal(GetIP(client)));

	/* same for ip, is there for all (well, some services pseudo-users may not have one) */
	json_object_set_new(child, "ip", json_string_unreal(client->ip));
	if (client->local && client->local->listener)
		json_object_set_new(child, "server_port", json_integer(client->local->listener->port));
	if (client->local && client->local->port)
		json_object_set_new(child, "client_port", json_integer(client->local->port));

	/* client.details is always available: it is nick!user@host, nick@host, server@host
	 * server@ip, or just server.
	 */
	if (client->user)
	{
		snprintf(buf, sizeof(buf), "%s!%s@%s", client->name, client->user->username, client->user->realhost);
		json_object_set_new(child, "details", json_string_unreal(buf));
	} else if (client->ip) {
		if (*client->name)
			snprintf(buf, sizeof(buf), "%s@%s", client->name, client->ip);
		else
			snprintf(buf, sizeof(buf), "[%s]", client->ip);
		json_object_set_new(child, "details", json_string_unreal(buf));
	} else {
		json_object_set_new(child, "details", json_string_unreal(client->name));
	}

	if (client->local && client->local->creationtime)
		json_object_set_new(child, "connected_since", json_timestamp(client->local->creationtime));

	if (client->local && client->local->idle_since)
		json_object_set_new(child, "idle_since", json_timestamp(client->local->idle_since));

	if (client->user)
	{
		char buf[512];
		const char *str;
		/* client.user */
		user = json_object();
		json_object_set_new(child, "user", user);

		json_object_set_new(user, "username", json_string_unreal(client->user->username));
		if (!BadPtr(client->info))
			json_object_set_new(user, "realname", json_string_unreal(client->info));
		if (has_user_mode(client, 'x') && client->user->virthost && strcmp(client->user->virthost, client->user->realhost))
			json_object_set_new(user, "vhost", json_string_unreal(client->user->virthost));
		if (*client->user->cloakedhost)
			json_object_set_new(user, "cloakedhost", json_string_unreal(client->user->cloakedhost));
		if (client->uplink)
			json_object_set_new(user, "servername", json_string_unreal(client->uplink->name));
		if (IsLoggedIn(client))
			json_object_set_new(user, "account", json_string_unreal(client->user->account));
		json_object_set_new(user, "reputation", json_integer(GetReputation(client)));
		json_expand_client_security_groups(user, client);

		/* user modes and snomasks */
		get_usermode_string_r(client, buf, sizeof(buf));
		json_object_set_new(user, "modes", json_string_unreal(buf+1));
		if (client->user->snomask)
			json_object_set_new(user, "snomasks", json_string_unreal(client->user->snomask));

		/* if oper then we can possibly expand a bit more */
		str = get_operlogin(client);
		if (str)
			json_object_set_new(user, "operlogin", json_string_unreal(str));
		str = get_operclass(client);
		if (str)
			json_object_set_new(user, "operclass", json_string_unreal(str));
		if (client->user->channel)
		{
			Membership *m;
			int cnt = 0;
			int len = 0;
			json_t *channels = json_array();
			json_object_set_new(user, "channels", channels);
			for (m = client->user->channel; m; m = m->next)
			{
				len += json_dump_string_length(m->channel->name);
				if (len > 384)
				{
					/* Truncated */
					json_array_append_new(channels, json_string_unreal("..."));
					break;
				}
				json_array_append_new(channels, json_string_unreal(m->channel->name));
			}
		}
		RunHook(HOOKTYPE_JSON_EXPAND_CLIENT_USER, client, detail, child, user);
	} else
	if (IsMe(client))
	{
		json_t *server = json_object();
		json_t *features;

		/* client.server */
		json_object_set_new(child, "server", server);

		if (!BadPtr(client->info))
			json_object_set_new(server, "info", json_string_unreal(client->info));
		json_object_set_new(server, "num_users", json_integer(client->server->users));
		json_object_set_new(server, "boot_time", json_timestamp(client->server->boottime));
	} else
	if (IsServer(client) && client->server)
	{
		/* client.server */

		/* Whenever a server is expanded, which is rare,
		 * we should probably expand as much as info as possible:
		 */
		json_t *server = json_object();
		json_t *features;

		/* client.server */
		json_object_set_new(child, "server", server);
		if (!BadPtr(client->info))
			json_object_set_new(server, "info", json_string_unreal(client->info));
		if (client->uplink)
			json_object_set_new(server, "uplink", json_string_unreal(client->uplink->name));
		json_object_set_new(server, "num_users", json_integer(client->server->users));
		json_object_set_new(server, "boot_time", json_timestamp(client->server->boottime));
		json_object_set_new(server, "synced", json_boolean(client->server->flags.synced));

		/* client.server.features */
		features = json_object();
		json_object_set_new(server, "features", features);
		if (!BadPtr(client->server->features.software))
			json_object_set_new(features, "software", json_string_unreal(client->server->features.software));
		json_object_set_new(features, "protocol", json_integer(client->server->features.protocol));
		if (!BadPtr(client->server->features.usermodes))
			json_object_set_new(features, "usermodes", json_string_unreal(client->server->features.usermodes));
		if (!BadPtr(client->server->features.chanmodes[0]))
		{
			/* client.server.features.chanmodes (array) */
			int i;
			json_t *chanmodes = json_array();
			json_object_set_new(features, "chanmodes", chanmodes);
			for (i=0; i < 4; i++)
				json_array_append_new(chanmodes, json_string_unreal(client->server->features.chanmodes[i]));
		}
		if (!BadPtr(client->server->features.nickchars))
			json_object_set_new(features, "nick_character_sets", json_string_unreal(client->server->features.nickchars));
		RunHook(HOOKTYPE_JSON_EXPAND_CLIENT_SERVER, client, detail, child, server);
	}
	RunHook(HOOKTYPE_JSON_EXPAND_CLIENT, client, detail, child);
}

void json_expand_channel(json_t *j, const char *key, Channel *channel, int detail)
{
	char mode1[512], mode2[512], modes[512];
	json_t *child;

	if (key)
	{
		child = json_object();
		json_object_set_new(j, key, child);
	} else {
		child = j;
	}

	json_object_set_new(child, "name", json_string_unreal(channel->name));
	json_object_set_new(child, "creation_time", json_timestamp(channel->creationtime));
	json_object_set_new(child, "num_users", json_integer(channel->users));
	if (channel->topic)
	{
		json_object_set_new(child, "topic", json_string_unreal(channel->topic));
		json_object_set_new(child, "topic_set_by", json_string_unreal(channel->topic_nick));
		json_object_set_new(child, "topic_set_at", json_timestamp(channel->topic_time));
	}

	/* Add "mode" too */
	channel_modes(NULL, mode1, mode2, sizeof(mode1), sizeof(mode2), channel, 0);
	if (*mode2)
	{
		snprintf(modes, sizeof(modes), "%s %s", mode1+1, mode2);
		json_object_set_new(child, "modes", json_string_unreal(modes));
	} else {
		json_object_set_new(child, "modes", json_string_unreal(mode1+1));
	}

	// Possibly later: If detail is set to 1 then expand more...
	RunHook(HOOKTYPE_JSON_EXPAND_CHANNEL, channel, detail, child);
}

const char *timestamp_iso8601_now(void)
{
	struct timeval t;
	struct tm *tm;
	time_t sec;
	static char buf[64];

	gettimeofday(&t, NULL);
	sec = t.tv_sec;
	tm = gmtime(&sec);

	snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec,
		(int)(t.tv_usec / 1000));

	return buf;
}

const char *timestamp_iso8601(time_t v)
{
	struct tm *tm;
	static char buf[64];

	if (v == 0)
		return NULL;

	tm = gmtime(&v);

	if (tm == NULL)
		return NULL;

	snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
		tm->tm_year + 1900,
		tm->tm_mon + 1,
		tm->tm_mday,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec,
		0);

	return buf;
}

LogData *log_data_string(const char *key, const char *str)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_STRING;
	safe_strdup(d->key, key);
	safe_strdup(d->value.string, str);
	return d;
}

LogData *log_data_char(const char *key, const char c)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_STRING;
	safe_strdup(d->key, key);
	d->value.string = safe_alloc(2);
	d->value.string[0] = c;
	d->value.string[1] = '\0';
	return d;
}

LogData *log_data_integer(const char *key, int64_t integer)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_INTEGER;
	safe_strdup(d->key, key);
	d->value.integer = integer;
	return d;
}

LogData *log_data_timestamp(const char *key, time_t ts)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_STRING;
	safe_strdup(d->key, key);
	safe_strdup(d->value.string, timestamp_iso8601(ts));
	return d;
}

LogData *log_data_client(const char *key, Client *client)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_CLIENT;
	safe_strdup(d->key, key);
	d->value.client = client;
	return d;
}

LogData *log_data_channel(const char *key, Channel *channel)
{
	LogData *d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_CHANNEL;
	safe_strdup(d->key, key);
	d->value.channel = channel;
	return d;
}

LogData *log_data_source(const char *file, int line, const char *function)
{
	LogData *d = safe_alloc(sizeof(LogData));
	json_t *j;

	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, "source");
	d->value.object = j = json_object();
	json_object_set_new(j, "file", json_string_unreal(file));
	json_object_set_new(j, "line", json_integer(line));
	json_object_set_new(j, "function", json_string_unreal(function));
	return d;
}

LogData *log_data_socket_error(int fd)
{
	/* First, grab the error number very early here: */
#ifndef _WIN32
	int sockerr = errno;
#else
	int sockerr = WSAGetLastError();
#endif
	int v;
	int len = sizeof(v);
	LogData *d;
	json_t *j;

#ifdef SO_ERROR
	/* Try to get the "real" error from the underlying socket.
	 * If we succeed then we will override "sockerr" with it.
	 */
	if ((fd >= 0) && !getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&v, &len) && v)
		sockerr = v;
#endif

	d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, "socket_error");
	d->value.object = j = json_object();
	json_object_set_new(j, "error_code", json_integer(sockerr));
	json_object_set_new(j, "error_string", json_string_unreal(STRERROR(sockerr)));
	return d;
}

/** Populate log with the TLS error(s) stack */
LogData *log_data_tls_error(void)
{
	LogData *d;
	json_t *j;
	json_t *error_stack;
	json_t *name = NULL;
	json_t *jt;
	unsigned long e;
	char buf[512];
	static char all_errors[8192];

	d = safe_alloc(sizeof(LogData));
	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, "tls_error");
	d->value.object = j = json_object();

	error_stack = json_array();
	json_object_set_new(j, "error_stack", error_stack);
	*all_errors = '\0';

	do {
		json_t *obj;

		e = ERR_get_error();
		if (e == 0)
			break;
		ERR_error_string_n(e, buf, sizeof(buf));

		obj = json_object();
		json_object_set_new(obj, "code", json_integer(e));
		json_object_set_new(obj, "string", json_string_unreal(buf));
		json_array_append_new(error_stack, obj);

		if (name == NULL)
		{
			/* Set tls_error.name to the first error that was encountered */
			json_object_set_new(j, "name", json_string_unreal(buf));
		}
		strlcat(all_errors, buf, sizeof(all_errors));
		strlcat(all_errors, "\n", sizeof(all_errors));
	} while(e);

	json_object_set_new(j, "all", json_string_unreal(all_errors));

	return d;
}

LogData *log_data_link_block(ConfigItem_link *link)
{
	LogData *d = safe_alloc(sizeof(LogData));
	json_t *j;
	char *bind_ip;

	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, "link_block");
	d->value.object = j = json_object();
	json_object_set_new(j, "name", json_string_unreal(link->servername));
	json_object_set_new(j, "file", json_string_unreal(link->outgoing.file));
	json_object_set_new(j, "hostname", json_string_unreal(link->outgoing.hostname));
	json_object_set_new(j, "ip", json_string_unreal(link->connect_ip));
	json_object_set_new(j, "port", json_integer(link->outgoing.port));
	json_object_set_new(j, "class", json_string_unreal(link->class->name));

	if (!link->outgoing.bind_ip && iConf.link_bindip)
		bind_ip = iConf.link_bindip;
	else
		bind_ip = link->outgoing.bind_ip;
	if (!bind_ip)
		bind_ip = "*";
	json_object_set_new(j, "bind_ip", json_string_unreal(bind_ip));

	return d;
}

LogData *log_data_tkl(const char *key, TKL *tkl)
{
	char buf[BUFSIZE];
	LogData *d = safe_alloc(sizeof(LogData));
	json_t *j;

	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, key);
	d->value.object = j = json_object();

	json_object_set_new(j, "type", json_string_unreal(tkl_type_config_string(tkl))); // Eg 'kline'
	json_object_set_new(j, "type_string", json_string_unreal(tkl_type_string(tkl))); // Eg 'Soft K-Line'
	json_object_set_new(j, "set_by", json_string_unreal(tkl->set_by));
	json_object_set_new(j, "set_at", json_timestamp(tkl->set_at));
	json_object_set_new(j, "expire_at", json_timestamp(tkl->expire_at));
	*buf = '\0';
	short_date(tkl->set_at, buf);
	strlcat(buf, " GMT", sizeof(buf));
	json_object_set_new(j, "set_at_string", json_string_unreal(buf));
	if (tkl->expire_at <= 0)
	{
		json_object_set_new(j, "expire_at_string", json_string_unreal("Never"));
		json_object_set_new(j, "duration_string", json_string_unreal("permanent"));
	} else {
		*buf = '\0';
		short_date(tkl->expire_at, buf);
		strlcat(buf, " GMT", sizeof(buf));
		json_object_set_new(j, "expire_at_string", json_string_unreal(buf));
		json_object_set_new(j, "duration_string", json_string_unreal(pretty_time_val_r(buf, sizeof(buf), tkl->expire_at - tkl->set_at)));
	}
	json_object_set_new(j, "set_at_delta", json_integer(TStime() - tkl->set_at));
	if (TKLIsServerBan(tkl))
	{
		json_object_set_new(j, "name", json_string_unreal(tkl_uhost(tkl, buf, sizeof(buf), 0)));
		json_object_set_new(j, "reason", json_string_unreal(tkl->ptr.serverban->reason));
	} else
	if (TKLIsNameBan(tkl))
	{
		json_object_set_new(j, "name", json_string_unreal(tkl->ptr.nameban->name));
		json_object_set_new(j, "reason", json_string_unreal(tkl->ptr.nameban->reason));
	} else
	if (TKLIsBanException(tkl))
	{
		json_object_set_new(j, "name", json_string_unreal(tkl_uhost(tkl, buf, sizeof(buf), 0)));
		json_object_set_new(j, "reason", json_string_unreal(tkl->ptr.banexception->reason));
		json_object_set_new(j, "exception_types", json_string_unreal(tkl->ptr.banexception->bantypes));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		json_object_set_new(j, "name", json_string_unreal(tkl->ptr.spamfilter->match->str));
		json_object_set_new(j, "match_type", json_string_unreal(unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type)));
		json_object_set_new(j, "ban_action", json_string_unreal(banact_valtostring(tkl->ptr.spamfilter->action)));
		json_object_set_new(j, "spamfilter_targets", json_string_unreal(spamfilter_target_inttostring(tkl->ptr.spamfilter->target)));
		json_object_set_new(j, "reason", json_string_unreal(unreal_decodespace(tkl->ptr.spamfilter->tkl_reason)));
	}

	return d;
}

void log_data_free(LogData *d)
{
	if (d->type == LOG_FIELD_STRING)
		safe_free(d->value.string);
	else if ((d->type == LOG_FIELD_OBJECT) && d->value.object)
		json_decref(d->value.object);

	safe_free(d->key);
	safe_free(d);
}

const char *log_level_valtostring(LogLevel loglevel)
{
	switch(loglevel)
	{
		case ULOG_DEBUG:
			return "debug";
		case ULOG_INFO:
			return "info";
		case ULOG_WARNING:
			return "warn";
		case ULOG_ERROR:
			return "error";
		case ULOG_FATAL:
			return "fatal";
		default:
			return NULL;
	}
}

static NameValue log_colors_irc[] = {
	{ ULOG_INVALID,	"\0030,01" },
	{ ULOG_DEBUG,	"\0030,01" },
	{ ULOG_INFO,	"\00303" },
	{ ULOG_WARNING,	"\00307" },
	{ ULOG_ERROR,	"\00304" },
	{ ULOG_FATAL,	"\00313" },
};

static NameValue log_colors_terminal[] = {
	{ ULOG_INVALID,	"\033[90m" },
	{ ULOG_DEBUG,	"\033[37m" },
	{ ULOG_INFO,	"\033[92m" },
	{ ULOG_WARNING,	"\033[93m" },
	{ ULOG_ERROR,	"\033[91m" },
	{ ULOG_FATAL,	"\033[95m" },
};

const char *log_level_irc_color(LogLevel loglevel)
{
	return nv_find_by_value(log_colors_irc, loglevel);
}

const char *log_level_terminal_color(LogLevel loglevel)
{
	return nv_find_by_value(log_colors_terminal, loglevel);
}

LogLevel log_level_stringtoval(const char *str)
{
	if (!strcmp(str, "info"))
		return ULOG_INFO;
	if (!strcmp(str, "warn"))
		return ULOG_WARNING;
	if (!strcmp(str, "error"))
		return ULOG_ERROR;
	if (!strcmp(str, "fatal"))
		return ULOG_FATAL;
	if (!strcmp(str, "debug"))
		return ULOG_DEBUG;
	return ULOG_INVALID;
}

#define validvarcharacter(x)	(isalnum((x)) || ((x) == '_'))
#define valideventidcharacter(x)	(isupper((x)) || isdigit((x)) || ((x) == '_'))
#define validsubsystemcharacter(x)	(islower((x)) || isdigit((x)) || ((x) == '_') || ((x) == '-'))

int valid_event_id(const char *s)
{
	if (!*s)
		return 0;
	for (; *s; s++)
		if (!valideventidcharacter(*s))
			return 0;
	return 1;
}

int valid_subsystem(const char *s)
{
	if (!*s)
		return 0;
	if (log_level_stringtoval(s) != ULOG_INVALID)
		return 0;
	for (; *s; s++)
		if (!validsubsystemcharacter(*s))
			return 0;
	return 1;
}

const char *json_get_value(json_t *t)
{
	static char buf[32];

	if (json_is_string(t))
		return json_string_value(t);

	if (json_is_integer(t))
	{
		snprintf(buf, sizeof(buf), "%lld", (long long)json_integer_value(t));
		return buf;
	}

	return NULL;
}

// TODO: if in the function below we keep adding auto expanshion shit,
// like we currently have $client automatically expanding to $client.name
// and $socket_error to $socket_error.error_string,
// if this gets more than we should use some kind of array for it,
// especially for the hardcoded name shit like $socket_error.

/** Build a string and replace $variables where needed.
 * See src/modules/blacklist.c for an example.
 * @param inbuf		The input string
 * @param outbuf	The output string
 * @param len		The maximum size of the output string (including NUL)
 * @param name		Array of variables names
 * @param value		Array of variable values
 */
void buildlogstring(const char *inbuf, char *outbuf, size_t len, json_t *details)
{
	const char *i, *p;
	char *o;
	int left = len - 1;
	int cnt, found;
	char varname[256], *varp, *varpp;
	json_t *t;

#ifdef DEBUGMODE
	if (len <= 0)
		abort();
#endif

	for (i = inbuf, o = outbuf; *i; i++)
	{
		if (*i == '$')
		{
			i++;

			/* $$ = literal $ */
			if (*i == '$')
				goto literal;

			if (!validvarcharacter(*i))
			{
				/* What do we do with things like '$/' ? -- treat literal */
				i--;
				goto literal;
			}

			/* find termination */
			for (p=i; validvarcharacter(*p) || ((*p == '.') && validvarcharacter(p[1])); p++);

			/* find variable name in list */
			strlncpy(varname, i, sizeof(varname), p - i);
			varp = strchr(varname, '.');
			if (varp)
				*varp = '\0';
			t = json_object_get(details, varname);
			if (t)
			{
				const char *output = NULL;
				if (varp)
				{
					char *varpp;
					do {
						varpp = strchr(varp+1, '.');
						if (varpp)
							*varpp = '\0';
						/* Fetch explicit object.key */
						t = json_object_get(t, varp+1);
						varp = varpp;
					} while(t && varpp);
					if (t)
						output = json_get_value(t);
				} else
				if (!strcmp(varname, "socket_error"))
				{
					/* Fetch socket_error.error_string */
					t = json_object_get(t, "error_string");
					if (t)
						output = json_get_value(t);
				} else
				if (json_is_object(t))
				{
					/* Fetch object.name */
					t = json_object_get(t, "name");
					if (t)
						output = json_get_value(t);
				} else
				{
					output = json_get_value(t);
				}
				if (output)
				{
					strlcpy(o, output, left);
					left -= strlen(output); /* may become <0 */
					if (left <= 0)
						return; /* return - don't write \0 to 'o'. ensured by strlcpy already */
					o += strlen(output); /* value entirely written */
				}
			} else
			{
				/* variable name does not exist -- treat as literal string */
				i--;
				goto literal;
			}

			/* value written. we're done. */
			i = p - 1;
			continue;
		}
literal:
		if (!left)
			break;
		*o++ = *i;
		left--;
		if (!left)
			break;
	}
	*o = '\0';
}

/** Do the actual writing to log files */
void do_unreal_log_disk(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized, Client *from_server)
{
	static int last_log_file_warning = 0;
	Log *l;
	char timebuf[128];
	struct stat fstats;
	int n;
	int write_error;
	long snomask;
	MultiLine *m;

	snprintf(timebuf, sizeof(timebuf), "[%s] ", myctime(TStime()));

	RunHook(HOOKTYPE_LOG, loglevel, subsystem, event_id, msg, json_serialized, timebuf);

	if (!loop.forked && (loglevel > ULOG_DEBUG))
	{
		for (m = msg; m; m = m->next)
		{
#ifdef _WIN32
			if (show_event_console)
				win_log("* %s.%s%s [%s] %s\n", subsystem, event_id, m->next?"+":"", log_level_valtostring(loglevel), m->line);
			else
				win_log("* [%s] %s\n", log_level_valtostring(loglevel), m->line);
#else
			if (terminal_supports_color())
			{
				if (show_event_console)
				{
					fprintf(stderr, "%s%s.%s%s %s[%s]%s %s\n",
							log_level_terminal_color(ULOG_INVALID), subsystem, event_id, TERMINAL_COLOR_RESET,
							log_level_terminal_color(loglevel), log_level_valtostring(loglevel), TERMINAL_COLOR_RESET,
							m->line);
				} else {
					fprintf(stderr, "%s[%s]%s %s\n",
							log_level_terminal_color(loglevel), log_level_valtostring(loglevel), TERMINAL_COLOR_RESET,
							m->line);
				}
			} else {
				if (show_event_console)
					fprintf(stderr, "%s.%s%s [%s] %s\n", subsystem, event_id, m->next?"+":"", log_level_valtostring(loglevel), m->line);
				else
					fprintf(stderr, "[%s] %s\n", log_level_valtostring(loglevel), m->line);
			}
#endif
		}
	}

	/* In case of './unrealircd configtest': don't write to log file, only to stderr */
	if (loop.config_test)
		return;

	for (l = logs[LOG_DEST_DISK]; l; l = l->next)
	{
		if (!log_sources_match(l->sources, loglevel, subsystem, event_id, 0))
			continue;

#ifdef HAVE_SYSLOG
		if (l->file && !strcasecmp(l->file, "syslog"))
		{
			if (l->type == LOG_TYPE_JSON)
			{
				syslog(LOG_INFO, "%s", json_serialized);
			} else
			if (l->type == LOG_TYPE_TEXT)
			{
				for (m = msg; m; m = m->next)
					syslog(LOG_INFO, "%s.%s%s %s: %s", subsystem, event_id, m->next?"+":"", log_level_valtostring(loglevel), m->line);
			}
			continue;
		}
#endif

		/* This deals with dynamic log file names, such as ircd.%Y-%m-%d.log */
		if (l->filefmt)
		{
			char *fname = unreal_strftime(l->filefmt);
			if (l->file && (l->logfd != -1) && strcmp(l->file, fname))
			{
				/* We are logging already and need to switch over */
				fd_close(l->logfd);
				l->logfd = -1;
			}
			safe_strdup(l->file, fname);
		}

		/* log::maxsize code */
		if (l->maxsize && (stat(l->file, &fstats) != -1) && fstats.st_size >= l->maxsize)
		{
			char oldlog[512];
			if (l->logfd == -1)
			{
				/* Try to open, so we can write the 'Max file size reached' message. */
				l->logfd = fd_fileopen(l->file, O_CREAT|O_APPEND|O_WRONLY);
			}
			if (l->logfd != -1)
			{
				if (write(l->logfd, "Max file size reached, starting new log file\n", 45) < 0)
				{
					/* We already handle the unable to write to log file case for normal data.
					 * I think we can get away with not handling this one.
					 */
					;
				}
				fd_close(l->logfd);
			}
			l->logfd = -1;

			/* Rename log file to xxxxxx.old */
			snprintf(oldlog, sizeof(oldlog), "%s.old", l->file);
			unlink(oldlog); /* windows rename cannot overwrite, so unlink here.. ;) */
			rename(l->file, oldlog);
		}

		/* generic code for opening log if not open yet.. */
		if (l->logfd == -1)
		{
			l->logfd = fd_fileopen(l->file, O_CREAT|O_APPEND|O_WRONLY);
			if (l->logfd == -1)
			{
				if (!loop.booted)
				{
					config_status("WARNING: Unable to write to '%s': %s", l->file, strerror(errno));
				} else {
					if (last_log_file_warning + 300 < TStime())
					{
						config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", l->file, strerror(errno));
						last_log_file_warning = TStime();
					}
				}
				continue;
			}
		}

		/* Now actually WRITE to the log... */
		write_error = 0;
		if ((l->type == LOG_TYPE_JSON) && strcmp(subsystem, "rawtraffic"))
		{
			n = write(l->logfd, json_serialized, strlen(json_serialized));
			if (n < strlen(json_serialized))
			{
				write_error = 1;
			} else {
				if (write(l->logfd, "\n", 1) < 1) // FIXME: no.. we should do it this way..... and why do we use direct I/O at all?
					write_error = 1;
			}
		} else
		if (l->type == LOG_TYPE_TEXT)
		{
			for (m = msg; m; m = m->next)
			{
				char text_buf[8192];
				snprintf(text_buf, sizeof(text_buf), "%s%s %s.%s%s %s: %s\n",
					timebuf, from_server->name,
					subsystem, event_id, m->next?"+":"", log_level_valtostring(loglevel), m->line);
				n = write(l->logfd, text_buf, strlen(text_buf));
				if (n < strlen(text_buf))
				{
					write_error = 1;
					break;
				}
			}
		}

		if (write_error)
		{
			if (!loop.booted)
			{
				config_status("WARNING: Unable to write to '%s': %s", l->file, strerror(errno));
			} else {
				if (last_log_file_warning + 300 < TStime())
				{
					config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", l->file, strerror(errno));
					last_log_file_warning = TStime();
				}
			}
		}
	}
}

int log_sources_match(LogSource *logsource, LogLevel loglevel, const char *subsystem, const char *event_id, int matched_already)
{
	int retval = 0;
	LogSource *ls;

	// NOTE: This routine works by exclusion, so a bad struct would
	//       cause everything to match!!

	for (ls = logsource; ls; ls = ls->next)
	{
		/* First deal with all positive matchers.. */
		if (ls->negative)
			continue;
		if (!strcmp(ls->subsystem, "all"))
		{
			retval = 1;
			break;
		}
		if (!strcmp(ls->subsystem, "nomatch") && !matched_already)
		{
			/* catch-all */
			retval = 1;
			break;
		}
		if (*ls->event_id && strcmp(ls->event_id, event_id))
			continue;
		if (*ls->subsystem && strcmp(ls->subsystem, subsystem))
			continue;
		if ((ls->loglevel != ULOG_INVALID) && (ls->loglevel != loglevel))
			continue;
		/* MATCH */
		retval = 1;
		break;
	}

	/* No matches? Then we can stop here */
	if (retval == 0)
		return 0;

	/* There was a match, now check for exemptions, eg !operoverride */
	for (ls = logsource; ls; ls = ls->next)
	{
		/* Only deal with negative matches... */
		if (!ls->negative)
			continue;
		if (!strcmp(ls->subsystem, "nomatch") || !strcmp(ls->subsystem, "all"))
			continue; /* !nomatch and !all make no sense, so just ignore it */
		if (*ls->event_id && strcmp(ls->event_id, event_id))
			continue;
		if (*ls->subsystem && strcmp(ls->subsystem, subsystem))
			continue;
		if ((ls->loglevel != ULOG_INVALID) && (ls->loglevel != loglevel))
			continue;
		/* NEGATIVE MATCH */
		return 0;
	}

	return 1;
}

/** Convert loglevel/subsystem/event_id to a snomask.
 * @returns The snomask letters (may be more than one),
 *          an asterisk (for all ircops), or NULL (no delivery)
 */
const char *log_to_snomask(LogLevel loglevel, const char *subsystem, const char *event_id)
{
	Log *ld;
	static char snomasks[64];
	int matched = 0;

	*snomasks = '\0';
	for (ld = logs[LOG_DEST_SNOMASK]; ld; ld = ld->next)
	{
		if (log_sources_match(ld->sources, loglevel, subsystem, event_id, 0))
		{
			strlcat(snomasks, ld->destination, sizeof(snomasks));
			matched = 1;
		}
	}

	if (logs[LOG_DEST_OPER] && log_sources_match(logs[LOG_DEST_OPER]->sources, loglevel, subsystem, event_id, matched))
		strlcat(snomasks, "s", sizeof(snomasks));

	return *snomasks ? snomasks : NULL;
}

#define COLOR_NONE "\xf"
#define COLOR_DARKGREY "\00314"

/** Generic sendto function for logging to IRC. Used for notices to IRCOps and also for sending to individual users on channels */
void sendto_log(Client *client, const char *msgtype, const char *destination, int show_colors, int show_event,
                LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized, Client *from_server)
{
	MultiLine *m;

	for (m = msg; m; m = m->next)
	{
		MessageTag *mtags = NULL;
		new_message(from_server, NULL, &mtags);

		/* Add JSON data, but only if it is the first message (m == msg) */
		if (json_serialized && (m == msg))
		{
			MessageTag *json_mtag = safe_alloc(sizeof(MessageTag));
			safe_strdup(json_mtag->name, "unrealircd.org/json-log");
			safe_strdup(json_mtag->value, json_serialized);
			AddListItem(json_mtag, mtags);
		}

		if (show_colors)
		{
			if (show_event)
			{
				sendto_one(client, mtags, ":%s %s %s :%s%s.%s%s%s %s[%s]%s %s",
					from_server->name, msgtype, destination,
					COLOR_DARKGREY, subsystem, event_id, m->next?"+":"", COLOR_NONE,
					log_level_irc_color(loglevel), log_level_valtostring(loglevel), COLOR_NONE,
					m->line);
			} else {
				sendto_one(client, mtags, ":%s %s %s :%s[%s]%s %s",
					from_server->name, msgtype, destination,
					log_level_irc_color(loglevel), log_level_valtostring(loglevel), COLOR_NONE,
					m->line);
			}
		} else {
			if (show_event)
			{
				sendto_one(client, mtags, ":%s %s %s :%s.%s%s [%s] %s",
					from_server->name, msgtype, destination,
					subsystem, event_id, m->next?"+":"",
					log_level_valtostring(loglevel),
					m->line);
			} else {
				sendto_one(client, mtags, ":%s %s %s :[%s] %s",
					from_server->name, msgtype, destination,
					log_level_valtostring(loglevel),
					m->line);
			}
		}
		safe_free_message_tags(mtags);
	}
}

/** Send server notices to IRCOps */
void do_unreal_log_opers(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized, Client *from_server)
{
	Client *client;
	const char *snomask_destinations, *p;

	/* If not fully booted then we don't have a logging to snomask mapping so can't do much.. */
	if (!loop.booted)
		return;

	/* Never send these */
	if (!strcmp(subsystem, "rawtraffic"))
		return;

	snomask_destinations = log_to_snomask(loglevel, subsystem, event_id);
	if (!snomask_destinations)
		return;

	/* To specific snomasks... */
	list_for_each_entry(client, &oper_list, special_node)
	{
		const char *operlogin;
		ConfigItem_oper *oper;
		int show_colors = iConf.server_notice_colors;
		int show_event = iConf.server_notice_show_event;

		if (snomask_destinations)
		{
			char found = 0;
			if (!client->user->snomask)
				continue; /* None set, so will never match */
			for (p = snomask_destinations; *p; p++)
			{
				if (strchr(client->user->snomask, *p))
				{
					found = 1;
					break;
				}
			}
			if (!found)
				continue;
		}

		operlogin = get_operlogin(client);
		if (operlogin && (oper = find_oper(operlogin)))
		{
			show_colors = oper->server_notice_colors;
			show_event = oper->server_notice_show_event;
		}

		sendto_log(client, "NOTICE", client->name, show_colors, show_event, loglevel, subsystem, event_id, msg, json_serialized, from_server);
	}
}

/** Send server notices to channels */
void do_unreal_log_channels(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized, Client *from_server)
{
	Log *l;
	Member *m;
	Client *client;

	/* If not fully booted then we don't have a logging to snomask mapping so can't do much.. */
	if (!loop.booted)
		return;

	/* Never send these */
	if (!strcmp(subsystem, "rawtraffic"))
		return;

	for (l = logs[LOG_DEST_CHANNEL]; l; l = l->next)
	{
		const char *operlogin;
		ConfigItem_oper *oper;
		Channel *channel;

		if (!log_sources_match(l->sources, loglevel, subsystem, event_id, 0))
			continue;

		channel = find_channel(l->destination);
		if (!channel)
			continue;

		for (m = channel->members; m; m = m->next)
		{
			Client *client = m->client;
			if (!MyUser(client))
				continue;
			if (l->oper_only && !IsOper(client))
				continue;
			sendto_log(client, "PRIVMSG", channel->name, l->color, l->show_event,
			           loglevel, subsystem, event_id, msg,
			           l->json_message_tag ? json_serialized : NULL,
			           from_server);
		}
	}
}

void do_unreal_log_remote(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized)
{
	Log *l;
	int found = 0;

	for (l = logs[LOG_DEST_REMOTE]; l; l = l->next)
	{
		if (log_sources_match(l->sources, loglevel, subsystem, event_id, 0))
		{
			found = 1;
			break;
		}
	}
	if (found == 0)
		return;

	do_unreal_log_remote_deliver(loglevel, subsystem, event_id, msg, json_serialized);
}

/** Send server notices to control channel */
void do_unreal_log_control(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, const char *json_serialized, Client *from_server)
{
	Client *client;
	MultiLine *m;

	if (!loop.booted)
		return;

	/* Never send these */
	if (!strcmp(subsystem, "rawtraffic"))
		return;

	list_for_each_entry(client, &control_list, lclient_node)
		if (IsMonitorRehash(client))
			for (m = msg; m; m = m->next)
				sendto_one(client, NULL, "REPLY [%s] %s", log_level_valtostring(loglevel), m->line);
}

void do_unreal_log_free_args(va_list vl)
{
	LogData *d;

	while ((d = va_arg(vl, LogData *)))
	{
		log_data_free(d);
	}
}

static int unreal_log_recursion_trap = 0;

/* Logging function, called by the unreal_log() macro. */
void do_unreal_log(LogLevel loglevel, const char *subsystem, const char *event_id,
                   Client *client, const char *msg, ...)
{
	va_list vl;

	if (unreal_log_recursion_trap)
	{
		va_start(vl, msg);
		do_unreal_log_free_args(vl);
		va_end(vl);
		return;
	}

	unreal_log_recursion_trap = 1;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 1, msg, vl);
	va_end(vl);
	unreal_log_recursion_trap = 0;
}

/* Logging function, called by the unreal_log_raw() macro. */
void do_unreal_log_raw(LogLevel loglevel, const char *subsystem, const char *event_id,
                       Client *client, const char *msg, ...)
{
	va_list vl;

	if (unreal_log_recursion_trap)
	{
		va_start(vl, msg);
		do_unreal_log_free_args(vl);
		va_end(vl);
		return;
	}

	unreal_log_recursion_trap = 1;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 0, msg, vl);
	va_end(vl);
	unreal_log_recursion_trap = 0;
}

void do_unreal_log_norecursioncheck(LogLevel loglevel, const char *subsystem, const char *event_id,
                                    Client *client, const char *msg, ...)
{
	va_list vl;

	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 1, msg, vl);
	va_end(vl);
}

void do_unreal_log_internal(LogLevel loglevel, const char *subsystem, const char *event_id,
                            Client *client, int expand_msg, const char *msg, va_list vl)
{
	LogData *d;
	char *json_serialized;
	const char *str;
	json_t *j = NULL;
	json_t *j_details = NULL;
	json_t *t;
	char msgbuf[8192];
	const char *loglevel_string = log_level_valtostring(loglevel);
	MultiLine *mmsg;
	Client *from_server = NULL;

	if (loglevel_string == NULL)
	{
		do_unreal_log_norecursioncheck(ULOG_ERROR, "log", "BUG_LOG_LOGLEVEL", NULL,
		                       "[BUG] Next log message had an invalid log level -- corrected to ULOG_ERROR",
		                       NULL);
		loglevel = ULOG_ERROR;
		loglevel_string = log_level_valtostring(loglevel);
	}
	if (!valid_subsystem(subsystem))
	{
		do_unreal_log_norecursioncheck(ULOG_ERROR, "log", "BUG_LOG_SUBSYSTEM", NULL,
		                       "[BUG] Next log message had an invalid subsystem -- changed to 'unknown'",
		                       NULL);
		subsystem = "unknown";
	}
	if (!valid_event_id(event_id))
	{
		do_unreal_log_norecursioncheck(ULOG_ERROR, "log", "BUG_LOG_EVENT_ID", NULL,
		                       "[BUG] Next log message had an invalid event id -- changed to 'unknown'",
		                       NULL);
		event_id = "unknown";
	}
	/* This one is probably temporary since it should not be a real error, actually (but often is) */
	if (expand_msg && strchr(msg, '%'))
	{
		do_unreal_log_norecursioncheck(ULOG_ERROR, "log", "BUG_LOG_MESSAGE_PERCENT", NULL,
		                       "[BUG] Next log message contains a percent sign -- possibly accidental format string!",
		                       NULL);
	}

	j = json_object();
	j_details = json_object();

	json_object_set_new(j, "timestamp", json_string_unreal(timestamp_iso8601_now()));
	json_object_set_new(j, "level", json_string_unreal(loglevel_string));
	json_object_set_new(j, "subsystem", json_string_unreal(subsystem));
	json_object_set_new(j, "event_id", json_string_unreal(event_id));
	json_object_set_new(j, "log_source", json_string_unreal(*me.name ? me.name : "local"));

	/* We put all the rest in j_details because we want to enforce
	 * a certain ordering of the JSON output. We will merge these
	 * details later on.
	 */
	if (client)
		json_expand_client(j_details, "client", client, 0);
	/* Additional details (if any) */
	while ((d = va_arg(vl, LogData *)))
	{
		switch(d->type)
		{
			case LOG_FIELD_INTEGER:
				json_object_set_new(j_details, d->key, json_integer(d->value.integer));
				break;
			case LOG_FIELD_STRING:
				if (d->value.string)
					json_object_set_new(j_details, d->key, json_string_unreal(d->value.string));
				else
					json_object_set_new(j_details, d->key, json_null());
				break;
			case LOG_FIELD_CLIENT:
				json_expand_client(j_details, d->key, d->value.client, 0);
				break;
			case LOG_FIELD_CHANNEL:
				json_expand_channel(j_details, d->key, d->value.channel, 0);
				break;
			case LOG_FIELD_OBJECT:
				json_object_set_new(j_details, d->key, d->value.object);
				d->value.object = NULL; /* don't let log_data_free() free it */
				break;
			default:
#ifdef DEBUGMODE
				abort();
#endif
				break;
		}
		log_data_free(d);
	}

	if (expand_msg)
		buildlogstring(msg, msgbuf, sizeof(msgbuf), j_details);
	else
		strlcpy(msgbuf, msg, sizeof(msgbuf));

	json_object_set_new(j, "msg", json_string_unreal(msgbuf));

	/* Now merge the details into root object 'j': */
	json_object_update_missing(j, j_details);
	/* Generate the JSON */
	json_serialized = json_dumps(j, JSON_COMPACT);

	/* Convert the message buffer to MultiLine */
	mmsg = line2multiline(msgbuf);

	/* Parse the "from server" info, if any */
	t = json_object_get(j_details, "from_server_name");
	if (t && (str = json_get_value(t)))
		from_server = find_server(str, NULL);
	if (from_server == NULL)
		from_server = &me;

	/* Now call all the loggers: */

	do_unreal_log_disk(loglevel, subsystem, event_id, mmsg, json_serialized, from_server);

	if ((loop.rehashing == 2) || !strcmp(subsystem, "config"))
		do_unreal_log_control(loglevel, subsystem, event_id, mmsg, json_serialized, from_server);

	do_unreal_log_opers(loglevel, subsystem, event_id, mmsg, json_serialized, from_server);

	do_unreal_log_channels(loglevel, subsystem, event_id, mmsg, json_serialized, from_server);

	do_unreal_log_remote(loglevel, subsystem, event_id, mmsg, json_serialized);

	// NOTE: code duplication further down!

	/* Free everything */
	safe_free(json_serialized);
	safe_free_multiline(mmsg);
	json_decref(j_details);
	json_decref(j);
}

void do_unreal_log_internal_from_remote(LogLevel loglevel, const char *subsystem, const char *event_id,
                                        MultiLine *msg, const char *json_serialized, Client *from_server)
{
	if (unreal_log_recursion_trap)
		return;
	unreal_log_recursion_trap = 1;

	/* Call the disk loggers */
	do_unreal_log_disk(loglevel, subsystem, event_id, msg, json_serialized, from_server);

	/* And to IRC */
	do_unreal_log_opers(loglevel, subsystem, event_id, msg, json_serialized, from_server);
	do_unreal_log_channels(loglevel, subsystem, event_id, msg, json_serialized, from_server);

	unreal_log_recursion_trap = 0;
}


void free_log_block(Log *l)
{
	Log *l_next;
	LogSource *src, *src_next;
	for (; l; l = l_next)
	{
		l_next = l->next;
		if (l->logfd > 0)
		{
			fd_close(l->logfd);
			l->logfd = -1;
		}
		free_log_sources(l->sources);
		safe_free(l->file);
		safe_free(l->filefmt);
		safe_free(l);
	}
}

int log_tests(void)
{
	if (snomask_num_destinations <= 1)
	{
		unreal_log(ULOG_ERROR, "config", "LOG_SNOMASK_BLOCK_MISSING", NULL,
		           "Missing snomask logging configuration:\n"
		           "Please add the following line to your unrealircd.conf: "
		           "include \"snomasks.default.conf\";");
		return 0;
	}
	snomask_num_destinations = 0;
	return 1;
}

void postconf_defaults_log_block(void)
{
	Log *l;
	LogSource *ls;

	/* Is there any log block to disk? Then nothing to do. */
	if (logs[LOG_DEST_DISK])
		return;

	unreal_log(ULOG_WARNING, "log", "NO_DISK_LOG_BLOCK", NULL,
	           "No log { } block found that logs to disk -- "
	           "logging everything in text format to 'ircd.log'");

	/* Create a default log block */
	l = safe_alloc(sizeof(Log));
	l->logfd = -1;
	l->type = LOG_TYPE_TEXT; /* text */
	l->maxsize = 100000000; /* maxsize 100M */
	safe_strdup(l->file, "ircd.log");
	convert_to_absolute_path(&l->file, LOGDIR);
	AddListItem(l, logs[LOG_DEST_DISK]);

	/* And the source filter */
	ls = add_log_source("all");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!debug");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!join.LOCAL_CLIENT_JOIN");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!join.REMOTE_CLIENT_JOIN");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!part.LOCAL_CLIENT_PART");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!part.REMOTE_CLIENT_PART");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!kick.LOCAL_CLIENT_KICK");
	AppendListItem(ls, l->sources);
	ls = add_log_source("!kick.REMOTE_CLIENT_KICK");
	AppendListItem(ls, l->sources);
}

/* Called before CONFIG_TEST */
void log_pre_rehash(void)
{
	*snomasks_in_use_testing = '\0';
}

/* Called after CONFIG_TEST right before CONFIG_RUN */
void config_pre_run_log(void)
{
	*snomasks_in_use = '\0';
}

/* Called after CONFIG_RUN is complete */
void log_blocks_switchover(void)
{
	int i;
	for (i=0; i < NUM_LOG_DESTINATIONS; i++)
		free_log_block(logs[i]);
	memcpy(logs, temp_logs, sizeof(logs));
	memset(temp_logs, 0, sizeof(temp_logs));
}

/** Check if a letter is a valid snomask (that is:
 * one that exists in the log block configuration).
 * @param c	the snomask letter to check
 * @returns	1 if exists, 0 if not.
 */
int is_valid_snomask(char c)
{
	return strchr(snomasks_in_use, c) ? 1 : 0;
}

/** Check if a letter is a valid snomask during or after CONFIG_TEST
 * (the snomasks exist in the log block configuration read during config_test).
 * @param c	the snomask letter to check
 * @returns	1 if exists, 0 if not.
 */
int is_valid_snomask_testing(char c)
{
	return strchr(snomasks_in_use_testing, c) ? 1 : 0;
}

/** Check if a string all consists of valid snomasks during or after CONFIG_TEST
 * (the snomasks exist in the log block configuration read during config_test).
 * @param str			the snomask string to check
 * @param invalid_snomasks	list of unknown snomask letters
 * @returns			1 if exists, 0 if not.
 */
int is_valid_snomask_string_testing(const char *str, char **invalid_snomasks)
{
	static char invalid_snomasks_buf[256];

	*invalid_snomasks_buf = '\0';
	for (; *str; str++)
	{
		if ((*str == '+') || (*str == '-'))
			continue;
		if (!strchr(snomasks_in_use_testing, *str))
			strlcat_letter(invalid_snomasks_buf, *str, sizeof(invalid_snomasks_buf));
	}
	*invalid_snomasks = invalid_snomasks_buf;
	return *invalid_snomasks_buf ? 0 : 1;
}
