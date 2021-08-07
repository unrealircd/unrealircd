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

#include "unrealircd.h"

#define SNO_ALL INT_MAX

/* Variables */
Log *logs[NUM_LOG_DESTINATIONS] = { NULL, NULL, NULL, NULL, NULL };
Log *temp_logs[NUM_LOG_DESTINATIONS] = { NULL, NULL, NULL, NULL, NULL };

/* Forward declarations */
void do_unreal_log_internal(LogLevel loglevel, char *subsystem, char *event_id, Client *client, int expand_msg, char *msg, va_list vl);
void log_blocks_switchover(void);

json_t *json_string_possibly_null(char *s)
{
	if (s)
		return json_string(s);
	return json_null();
}

json_t *json_timestamp(time_t v)
{
	char *ts = timestamp_iso8601(v);
	if (ts)
		return json_string(ts);
	return json_null();
}

LogType log_type_stringtoval(char *str)
{
	if (!strcmp(str, "json"))
		return LOG_TYPE_JSON;
	if (!strcmp(str, "text"))
		return LOG_TYPE_TEXT;
	return LOG_TYPE_INVALID;
}

char *log_type_valtostring(LogType v)
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
	strlcpy(buf, str, sizeof(buf));
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
	if (!BadPtr(subsystem))
		strlcpy(ls->subsystem, subsystem, sizeof(ls->subsystem));
	if (!BadPtr(event_id))
		strlcpy(ls->event_id, event_id, sizeof(ls->event_id));

	return ls;
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
		}
		if (!strcmp(ce->name, "destination"))
		{
			for (cep = ce->items; cep; cep = cep->next)
			{
				if (!strcmp(cep->name, "snomask"))
				{
					destinations++;
					/* We need to validate the parameter here as well */
					if (!cep->value)
					{
						config_error_blank(cep->file->filename, cep->line_number, "set::logging::snomask");
						errors++;
					} else
					if (!strcmp(cep->value, "all"))
					{
						/* Fine */
					} else
					if ((strlen(cep->value) != 1) || !(islower(cep->value[0]) || isupper(cep->value[0])))
					{
						config_error("%s:%d: snomask must be a single letter or 'all'",
							cep->file->filename, cep->line_number);
						errors++;
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
				} else // TODO: re-add syslog support too
				{
					config_error_unknownopt(cep->file->filename, cep->line_number, "log::destination", cep->name);
					errors++;
					continue;
				}
			}
		}
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
	Log *log = safe_alloc(sizeof(Log));
	int type;

	// TODO: we may allow multiple destination entries later, then we need to 'clone' sources
	//       or work with reference counts.

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
					log->sources = sources;
					if (!strcmp(cep->value, "all"))
						AddListItem(log, temp_logs[LOG_DEST_OPER]);
					else
						AddListItem(log, temp_logs[LOG_DEST_SNOMASK]);
				} else
				if (!strcmp(cep->name, "channel"))
				{
					Log *d = safe_alloc(sizeof(Log));
					strlcpy(log->destination, cep->value, sizeof(log->destination)); /* destination is the channel */
					log->sources = sources;
					AddListItem(log, temp_logs[LOG_DEST_CHANNEL]);
				} else
				if (!strcmp(cep->name, "remote"))
				{
					Log *log = safe_alloc(sizeof(Log));
					/* destination stays empty */
					log->sources = sources;
					AddListItem(log, temp_logs[LOG_DEST_REMOTE]);
				} else
				if (!strcmp(cep->name, "file"))
				{
					Log *log = safe_alloc(sizeof(Log));
					log->sources = sources;
					log->logfd = -1;
					log->type = LOG_TYPE_TEXT; /* default */
					if (strchr(cep->value, '%'))
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
					AddListItem(log, temp_logs[LOG_DEST_OTHER]);
				}
			}
		}
	}

	return 0;
}




/***** RUNTIME *****/

// TODO: validate that all 'key' values are lowercase+underscore+digits in all functions below.

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
		json_array_append_new(child, json_string("known-users"));
	else
		json_array_append_new(child, json_string("unknown-users"));

	for (s = securitygroups; s; s = s->next)
		if (strcmp(s->name, "known-users") && user_allowed_by_security_group(client, s))
			json_array_append_new(child, json_string(s->name));
}

void json_expand_client(json_t *j, char *key, Client *client, int detail)
{
	char buf[BUFSIZE+1];
	json_t *child = json_object();
	json_t *user = NULL;
	json_object_set_new(j, key, child);

	/* First the information that is available for ALL client types: */

	json_object_set_new(child, "name", json_string(client->name));

	/* hostname is available for all, it just depends a bit on whether it is DNS or IP */
	if (client->user && *client->user->realhost)
		json_object_set_new(child, "hostname", json_string(client->user->realhost));
	else if (client->local && *client->local->sockhost)
		json_object_set_new(child, "hostname", json_string(client->local->sockhost));
	else
		json_object_set_new(child, "hostname", json_string(GetIP(client)));

	/* same for ip, is there for all (well, some services pseudo-users may not have one) */
	json_object_set_new(child, "ip", json_string_possibly_null(client->ip));

	/* client.details is always available: it is nick!user@host, nick@host, server@host
	 * server@ip, or just server.
	 */
	if (client->user)
	{
		snprintf(buf, sizeof(buf), "%s!%s@%s", client->name, client->user->username, client->user->realhost);
		json_object_set_new(child, "details", json_string(buf));
	} else if (client->ip) {
		snprintf(buf, sizeof(buf), "%s@%s", client->name, client->ip);
		json_object_set_new(child, "details", json_string(buf));
	} else {
		json_object_set_new(child, "details", json_string(client->name));
	}

	if (client->local && client->local->firsttime)
		json_object_set_new(child, "connected_since", json_timestamp(client->local->firsttime));

	if (client->user)
	{
		/* client.user */
		user = json_object();
		json_object_set_new(child, "user", user);

		json_object_set_new(user, "username", json_string(client->user->username));
		if (!BadPtr(client->info))
			json_object_set_new(user, "realname", json_string(client->info));
		if (client->srvptr && client->srvptr->name)
			json_object_set_new(user, "servername", json_string(client->srvptr->name));
		if (IsLoggedIn(client))
			json_object_set_new(user, "account", json_string(client->user->svid));
		json_object_set_new(user, "reputation", json_integer(GetReputation(client)));
		json_expand_client_security_groups(user, client);
	} else
	if (IsServer(client))
	{
		/* client.server */

		/* Whenever a server is expanded, which is rare,
		 * we should probably expand as much as info as possible:
		 */
		json_t *server = json_object();
		json_t *features;

		/* client.server */
		json_object_set_new(child, "server", server);
		if (client->srvptr && client->srvptr->name)
			json_object_set_new(server, "uplink", json_string(client->srvptr->name));
		json_object_set_new(server, "num_users", json_integer(client->serv->users));
		json_object_set_new(server, "boot_time", json_timestamp(client->serv->boottime));
		json_object_set_new(server, "synced", json_boolean(client->serv->flags.synced));

		/* client.server.features */
		features = json_object();
		json_object_set_new(server, "features", features);
		if (!BadPtr(client->serv->features.software))
			json_object_set_new(features, "software", json_string(client->serv->features.software));
		json_object_set_new(features, "protocol", json_integer(client->serv->features.protocol));
		if (!BadPtr(client->serv->features.usermodes))
			json_object_set_new(features, "usermodes", json_string(client->serv->features.usermodes));
		if (!BadPtr(client->serv->features.chanmodes[0]))
		{
			/* client.server.features.chanmodes (array) */
			int i;
			json_t *chanmodes = json_array();
			json_object_set_new(features, "chanmodes", chanmodes);
			for (i=0; i < 4; i++)
				json_array_append_new(chanmodes, json_string_possibly_null(client->serv->features.chanmodes[i]));
		}
		if (!BadPtr(client->serv->features.nickchars))
			json_object_set_new(features, "nick_character_sets", json_string(client->serv->features.nickchars));
	}
}

void json_expand_channel(json_t *j, char *key, Channel *channel, int detail)
{
	json_t *child = json_object();
	json_object_set_new(j, key, child);
	json_object_set_new(child, "name", json_string(channel->name));
}

char *timestamp_iso8601_now(void)
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

char *timestamp_iso8601(time_t v)
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

LogData *log_data_source(const char *file, int line, const char *function)
{
	LogData *d = safe_alloc(sizeof(LogData));
	json_t *j;

	d->type = LOG_FIELD_OBJECT;
	safe_strdup(d->key, "source");
	d->value.object = j = json_object();
	json_object_set_new(j, "file", json_string(file));
	json_object_set_new(j, "line", json_integer(line));
	json_object_set_new(j, "function", json_string(function));
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
	json_object_set_new(j, "error_string", json_string(STRERROR(sockerr)));
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
	json_object_set_new(j, "name", json_string(link->servername));
	json_object_set_new(j, "hostname", json_string(link->outgoing.hostname));
	json_object_set_new(j, "ip", json_string(link->connect_ip));
	json_object_set_new(j, "port", json_integer(link->outgoing.port));

	if (!link->outgoing.bind_ip && iConf.link_bindip)
		bind_ip = iConf.link_bindip;
	else
		bind_ip = link->outgoing.bind_ip;
	if (!bind_ip)
		bind_ip = "*";
	json_object_set_new(j, "bind_ip", json_string(bind_ip));

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

	json_object_set_new(j, "type", json_string(tkl_type_config_string(tkl))); // Eg 'kline'
	json_object_set_new(j, "type_string", json_string(tkl_type_string(tkl))); // Eg 'Soft K-Line'
	json_object_set_new(j, "set_by", json_string(tkl->set_by));
	json_object_set_new(j, "set_at", json_timestamp(tkl->set_at));
	json_object_set_new(j, "expire_at", json_timestamp(tkl->expire_at));
	*buf = '\0';
	short_date(tkl->set_at, buf);
	strlcat(buf, " GMT", sizeof(buf));
	json_object_set_new(j, "set_at_string", json_string(buf));
	if (tkl->expire_at <= 0)
	{
		json_object_set_new(j, "expire_at_string", json_string("Never"));
	} else {
		*buf = '\0';
		short_date(tkl->expire_at, buf);
		strlcat(buf, " GMT", sizeof(buf));
		json_object_set_new(j, "expire_at_string", json_string(buf));
	}
	json_object_set_new(j, "set_at_delta", json_integer(TStime() - tkl->set_at));
	if (TKLIsServerBan(tkl))
	{
		json_object_set_new(j, "name", json_string(tkl_uhost(tkl, buf, sizeof(buf), 0)));
		json_object_set_new(j, "reason", json_string(tkl->ptr.serverban->reason));
	} else
	if (TKLIsNameBan(tkl))
	{
		json_object_set_new(j, "name", json_string(tkl->ptr.nameban->name));
		json_object_set_new(j, "reason", json_string(tkl->ptr.nameban->reason));
	} else
	if (TKLIsBanException(tkl))
	{
		json_object_set_new(j, "name", json_string(tkl_uhost(tkl, buf, sizeof(buf), 0)));
		json_object_set_new(j, "reason", json_string(tkl->ptr.banexception->reason));
		json_object_set_new(j, "exception_types", json_string(tkl->ptr.banexception->bantypes));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		json_object_set_new(j, "name", json_string(tkl->ptr.spamfilter->match->str));
		json_object_set_new(j, "match_type", json_string(unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type)));
		json_object_set_new(j, "ban_action", json_string(banact_valtostring(tkl->ptr.spamfilter->action)));
		json_object_set_new(j, "spamfilter_targets", json_string(spamfilter_target_inttostring(tkl->ptr.spamfilter->target)));
		json_object_set_new(j, "reason", json_string(unreal_decodespace(tkl->ptr.spamfilter->tkl_reason)));
	}

	return d;
}

void log_data_free(LogData *d)
{
	if (d->type == LOG_FIELD_STRING)
		safe_free(d->value.string);
	safe_free(d->key);
	safe_free(d);
}

char *log_level_valtostring(LogLevel loglevel)
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
#define validsubsystemcharacter(x)	(islower((x)) || isdigit((x)) || ((x) == '_'))

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
			*varname = '\0';
			strlncat(varname, i, sizeof(varname), p - i);
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
void do_unreal_log_disk(LogLevel loglevel, char *subsystem, char *event_id, char *msg, char *json_serialized)
{
	static int last_log_file_warning = 0;
	Log *l;
	char text_buf[2048], timebuf[128];
	struct stat fstats;
	int n;
	int write_error;
	long snomask;

	snprintf(timebuf, sizeof(timebuf), "[%s] ", myctime(TStime()));
	snprintf(text_buf, sizeof(text_buf), "%s %s %s: %s\n",
	         log_level_valtostring(loglevel), subsystem, event_id, msg);

	//RunHook3(HOOKTYPE_LOG, flags, timebuf, text_buf); // FIXME: call with more parameters and possibly not even 'text_buf' at all

	if (!loop.ircd_forked && (loglevel > ULOG_DEBUG))
	{
#ifdef _WIN32
		win_log("* %s", text_buf);
#else
		fprintf(stderr, "%s", text_buf);
#endif
	}

	/* In case of './unrealircd configtest': don't write to log file, only to stderr */
	if (loop.config_test)
		return;

	for (l = logs[LOG_DEST_OTHER]; l; l = l->next)
	{
		// FIXME: implement the proper log filters (eg what 'flags' previously was)
		//if (!(l->flags & flags))
		//	continue;

#ifdef HAVE_SYSLOG
		if (l->file && !strcasecmp(l->file, "syslog"))
		{
			syslog(LOG_INFO, "%s", text_buf);
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
				if (!loop.ircd_booted)
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
		if ((l->type == LOG_TYPE_JSON) && strcmp(subsystem, "traffic"))
		{
			n = write(l->logfd, json_serialized, strlen(json_serialized));
			if (n < strlen(text_buf))
				write_error = 1;
			else
				write(l->logfd, "\n", 1); // FIXME: no.. we should do it this way..... and why do we use direct I/O at all?
		} else
		if (l->type == LOG_TYPE_TEXT)
		{
			// FIXME: don't write in 2 stages, waste of slow system calls
			if (write(l->logfd, timebuf, strlen(timebuf)) < 0)
			{
				/* Let's ignore any write errors for this one. Next write() will catch it... */
				;
			}
			n = write(l->logfd, text_buf, strlen(text_buf));
			if (n < strlen(text_buf))
				write_error = 1;
		}

		if (write_error)
		{
			if (!loop.ircd_booted)
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

int log_sources_match(LogSource *ls, LogLevel loglevel, char *subsystem, char *event_id)
{
	// NOTE: This routine works by exclusion, so a bad struct would
	//       cause everything to match!!
	for (; ls; ls = ls->next)
	{
		if (*ls->event_id && strcmp(ls->event_id, event_id))
			continue;
		if (*ls->subsystem && strcmp(ls->subsystem, subsystem))
			continue;
		if ((ls->loglevel != ULOG_INVALID) && (ls->loglevel != loglevel))
			continue;
		return 1; /* MATCH */
	}
	return 0;
}

/** Convert loglevel/subsystem/event_id to a snomask.
 * @returns The snomask letters (may be more than one),
 *          an asterisk (for all ircops), or NULL (no delivery)
 */
char *log_to_snomask(LogLevel loglevel, char *subsystem, char *event_id)
{
	Log *ld;
	static char snomasks[64];

	/* At the top right now. TODO: "nomatch" support */
	if (logs[LOG_DEST_OPER] && log_sources_match(logs[LOG_DEST_OPER]->sources, loglevel, subsystem, event_id))
		return "*";

	*snomasks = '\0';
	for (ld = logs[LOG_DEST_SNOMASK]; ld; ld = ld->next)
	{
		if (log_sources_match(ld->sources, loglevel, subsystem, event_id))
			strlcat(snomasks, ld->destination, sizeof(snomasks));
	}

	return *snomasks ? snomasks : NULL;
}

/** Do the actual writing to log files */
void do_unreal_log_ircops(LogLevel loglevel, char *subsystem, char *event_id, char *msg, char *json_serialized)
{
	Client *client;
	char *snomask_destinations;
	char *client_snomasks;
	char *p;

	/* If not fully booted then we don't have a logging to snomask mapping so can't do much.. */
	if (!loop.ircd_booted)
		return;

	/* Never send these */
	if (!strcmp(subsystem, "traffic"))
		return;

	snomask_destinations = log_to_snomask(loglevel, subsystem, event_id);

	/* Zero destinations? Then return.
	 * XXX temporarily log to all ircops until we ship with default conf ;)
	 */
	if (snomask_destinations == NULL)
	{
		sendto_realops("[%s] %s.%s %s", log_level_valtostring(loglevel), subsystem, event_id, msg);
		return;
	}

	/* All ircops? Simple case. */
	if (!strcmp(snomask_destinations, "*"))
	{
		sendto_realops("[%s] %s.%s %s", log_level_valtostring(loglevel), subsystem, event_id, msg);
		return;
	}

	/* To specific snomasks... */
	list_for_each_entry(client, &oper_list, special_node)
	{
		client_snomasks = get_snomask_string(client);
		for (p = snomask_destinations; *p; p++)
		{
			if (strchr(client_snomasks, *p))
			{
				sendnotice(client, "[%s] %s.%s %s", log_level_valtostring(loglevel), subsystem, event_id, msg);
				break;
			}
		}
	}
}

void do_unreal_log_remote(LogLevel loglevel, char *subsystem, char *event_id, char *msg, char *json_serialized)
{
	Log *l;
	int found = 0;

	for (l = logs[LOG_DEST_REMOTE]; l; l = l->next)
	{
		if (log_sources_match(l->sources, loglevel, subsystem, event_id))
		{
			found = 1;
			break;
		}
	}
	if (found == 0)
		return;

	do_unreal_log_remote_deliver(loglevel, subsystem, event_id, msg, json_serialized);
}

static int unreal_log_recursion_trap = 0;

/* Logging function, called by the unreal_log() macro. */
void do_unreal_log(LogLevel loglevel, char *subsystem, char *event_id,
                   Client *client, char *msg, ...)
{
	if (unreal_log_recursion_trap)
		return;
	unreal_log_recursion_trap = 1;
	va_list vl;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 1, msg, vl);
	va_end(vl);
	unreal_log_recursion_trap = 0;
}

/* Logging function, called by the unreal_log_raw() macro. */
void do_unreal_log_raw(LogLevel loglevel, char *subsystem, char *event_id,
                       Client *client, char *msg, ...)
{
	if (unreal_log_recursion_trap)
		return;
	unreal_log_recursion_trap = 1;
	va_list vl;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 0, msg, vl);
	va_end(vl);
	unreal_log_recursion_trap = 0;
}

void do_unreal_log_internal(LogLevel loglevel, char *subsystem, char *event_id,
                            Client *client, int expand_msg, char *msg, va_list vl)
{
	LogData *d;
	char *json_serialized;
	json_t *j = NULL;
	json_t *j_details = NULL;
	char msgbuf[1024];
	char *loglevel_string = log_level_valtostring(loglevel);

	/* TODO: Enforcement:
	 * - loglevel must be valid
	 * - subsystem may only contain lowercase, underscore and numbers
	 * - event_id may only contain UPPERCASE, underscore and numbers (but not start with a number)
	 * - msg may not contain percent signs (%) as that is an obvious indication something is wrong?
	 *   or maybe a temporary restriction while upgrading that can be removed later ;)
	 */
	if (loglevel_string == NULL)
		abort();
	if (!valid_subsystem(subsystem))
		abort();
	if (!valid_event_id(event_id))
		abort();
	if (expand_msg && strchr(msg, '%'))
		abort();

	j = json_object();
	j_details = json_object();

	json_object_set_new(j, "timestamp", json_string(timestamp_iso8601_now()));
	json_object_set_new(j, "level", json_string(loglevel_string));
	json_object_set_new(j, "subsystem", json_string(subsystem));
	json_object_set_new(j, "event_id", json_string(event_id));
	json_object_set_new(j, "log_source", json_string(*me.name ? me.name : "local"));

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
					json_object_set_new(j_details, d->key, json_string(d->value.string));
				else
					json_object_set_new(j_details, d->key, json_null());
				break;
			case LOG_FIELD_CLIENT:
				json_expand_client(j_details, d->key, d->value.client, 0);
				break;
			case LOG_FIELD_OBJECT:
				json_object_set_new(j_details, d->key, d->value.object);
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

	json_object_set_new(j, "msg", json_string(msgbuf));

	/* Now merge the details into root object 'j': */
	json_object_update_missing(j, j_details);
	/* Generate the JSON */
	json_serialized = json_dumps(j, 0);

	/* Now call the disk loggers */
	do_unreal_log_disk(loglevel, subsystem, event_id, msgbuf, json_serialized);

	/* And the ircops stuff */
	do_unreal_log_ircops(loglevel, subsystem, event_id, msgbuf, json_serialized);

	do_unreal_log_remote(loglevel, subsystem, event_id, msgbuf, json_serialized);

	// NOTE: code duplication further down!

	/* Free everything */
	safe_free(json_serialized);
	json_decref(j_details);
	json_decref(j);
}

void do_unreal_log_internal_from_remote(LogLevel loglevel, char *subsystem, char *event_id,
                                        char *msgbuf, char *json_serialized)
{
	if (unreal_log_recursion_trap)
		return;
	unreal_log_recursion_trap = 1;

	/* Call the disk loggers */
	do_unreal_log_disk(loglevel, subsystem, event_id, msgbuf, json_serialized);

	/* And the ircops stuff */
	do_unreal_log_ircops(loglevel, subsystem, event_id, msgbuf, json_serialized);

	unreal_log_recursion_trap = 0;
}


void free_log_block(Log *l)
{
	Log *l_next;
	for (; l; l = l_next)
	{
		l_next = l->next;
		if (l->logfd > 0)
		{
			fd_close(l->logfd);
			l->logfd = -1;
		}
		safe_free(l->file);
		safe_free(l->filefmt);
		safe_free(l);
	}
}

void log_blocks_switchover(void)
{
	int i;
	for (i=0; i < NUM_LOG_DESTINATIONS; i++)
		free_log_block(logs[i]);
	memcpy(logs, temp_logs, sizeof(logs));
	memset(temp_logs, 0, sizeof(temp_logs));
}

/* TODO: if logging to the same file from multiple log { }
 * blocks, then we would have opened the file twice.
 * Better to use an extra layer to keep track of files.
 */
