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

/* Forward declarations */
long log_to_snomask(LogLevel loglevel, char *subsystem, char *event_id);
void do_unreal_log_internal(LogLevel loglevel, char *subsystem, char *event_id, Client *client, int expand_msg, char *msg, va_list vl);

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
			return "???";
	}
}

/***** CONFIGURATION ******/

int config_test_log(ConfigFile *conf, ConfigEntry *ce)
{
	int fd, errors = 0;
	ConfigEntry *cep, *cepp;
	char has_flags = 0, has_maxsize = 0;
	char *fname;

	if (!ce->ce_vardata)
	{
		config_error("%s:%i: log block without filename",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}
	if (!ce->ce_entries)
	{
		config_error("%s:%i: empty log block",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		return 1;
	}

	/* Convert to absolute path (if needed) unless it's "syslog" */
	if (strcmp(ce->ce_vardata, "syslog"))
		convert_to_absolute_path(&ce->ce_vardata, LOGDIR);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "flags"))
		{
			if (has_flags)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log::flags");
				continue;
			}
			has_flags = 1;
			if (!cep->ce_entries)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log", cep->ce_varname);
				errors++;
				continue;
			}
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				// FIXME: old flags shit
			}
		}
		else if (!strcmp(cep->ce_varname, "maxsize"))
		{
			if (has_maxsize)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log::maxsize");
				continue;
			}
			has_maxsize = 1;
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log", cep->ce_varname);
				errors++;
			}
		}
		else if (!strcmp(cep->ce_varname, "type"))
		{
			if (!cep->ce_vardata)
			{
				config_error_empty(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "log", cep->ce_varname);
				errors++;
				continue;
			}
			if (!log_type_stringtoval(cep->ce_vardata))
			{
				config_error("%s:%i: unknown log type '%s'",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_vardata);
				errors++;
			}
		}
		else
		{
			config_error_unknown(cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
				"log", cep->ce_varname);
			errors++;
			continue;
		}
	}

	if (!has_flags)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"log::flags");
		errors++;
	}

	fname = unreal_strftime(ce->ce_vardata);
	if ((fd = fd_fileopen(fname, O_WRONLY|O_CREAT)) == -1)
	{
		config_error("%s:%i: Couldn't open logfile (%s) for writing: %s",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			fname, strerror(errno));
		errors++;
	} else
	{
		fd_close(fd);
	}

	return errors;
}

int config_run_log(ConfigFile *conf, ConfigEntry *ce)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_log *ca;

	ca = safe_alloc(sizeof(ConfigItem_log));
	ca->logfd = -1;
	if (strchr(ce->ce_vardata, '%'))
		safe_strdup(ca->filefmt, ce->ce_vardata);
	else
		safe_strdup(ca->file, ce->ce_vardata);

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "maxsize"))
		{
			ca->maxsize = config_checkval(cep->ce_vardata,CFG_SIZE);
		}
		else if (!strcmp(cep->ce_varname, "type"))
		{
			ca->type = log_type_stringtoval(cep->ce_vardata);
		}
		else if (!strcmp(cep->ce_varname, "flags"))
		{
			for (cepp = cep->ce_entries; cepp; cepp = cepp->ce_next)
			{
				// FIXME: old flags shit
			}
		}
	}
	AddListItem(ca, conf_log);
	return 1;

}



/***** RUNTIME *****/

// TODO: validate that all 'key' values are lowercase+underscore+digits in all functions below.

void json_expand_client(json_t *j, char *key, Client *client, int detail)
{
	char buf[BUFSIZE+1];
	json_t *child = json_object();
	json_object_set_new(j, key, child);

	json_object_set_new(child, "name", json_string(client->name));

	if (client->user)
		json_object_set_new(child, "username", json_string(client->user->username));

	if (client->user && *client->user->realhost)
		json_object_set_new(child, "host", json_string(client->user->realhost));
	else if (client->local && *client->local->sockhost)
		json_object_set_new(child, "host", json_string(client->local->sockhost));
	else
		json_object_set_new(child, "host", json_string(GetIP(client)));

	json_object_set_new(child, "ip", json_string(GetIP(client)));

	if (client->user)
	{
		snprintf(buf, sizeof(buf), "%s!%s@%s", client->name, client->user->username, client->user->realhost);
		json_object_set_new(child, "nuh", json_string(buf));
	} else {
		snprintf(buf, sizeof(buf), "%s@%s", client->name, GetIP(client));
		json_object_set_new(child, "nuh", json_string(buf));
	}

	if (IsLoggedIn(client))
		json_object_set_new(child, "account", json_string(client->user->svid));
}

void json_expand_channel(json_t *j, char *key, Channel *channel, int detail)
{
	json_t *child = json_object();
	json_object_set_new(j, key, child);
	json_object_set_new(child, "name", json_string(channel->chname));
}

char *timestamp_iso8601(void)
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
	json_object_set_new(j, "set_at", json_integer(tkl->set_at));
	json_object_set_new(j, "expire_at", json_integer(tkl->expire_at));
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

char *loglevel_to_string(LogLevel loglevel)
{
	switch(loglevel)
	{
		case ULOG_INFO:
			return "info";
		case ULOG_WARN:
			return "warn";
		case ULOG_ERROR:
			return "error";
		case ULOG_FATAL:
			return "fatal";
		default:
			return "???";
	}
}

#define validvarcharacter(x)	(isalnum((x)) || ((x) == '_'))

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
	char varname[256], *varp;
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
				*varp++ = '\0';
			t = json_object_get(details, varname);
			if (t)
			{
				const char *output = NULL;
				if (varp)
				{
					/* Fetch explicit object.key */
					t = json_object_get(t, varp);
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
void do_unreal_log_loggers(LogLevel loglevel, char *subsystem, char *event_id, char *msg, char *json_serialized)
{
	static int last_log_file_warning = 0;
	static char recursion_trap=0;
	ConfigItem_log *l;
	char text_buf[2048], timebuf[128];
	struct stat fstats;
	int n;
	int write_error;
	long snomask;

	/* Trap infinite recursions to avoid crash if log file is unavailable,
	 * this will also avoid calling ircd_log from anything else called
	 */
	if (recursion_trap == 1)
		return;

	recursion_trap = 1;

	/* NOTE: past this point you CANNOT just 'return'.
	 * You must set 'recursion_trap = 0;' before 'return'!
	 */

	snprintf(timebuf, sizeof(timebuf), "[%s] ", myctime(TStime()));
	snprintf(text_buf, sizeof(text_buf), "%s %s %s: %s\n",
	         loglevel_to_string(loglevel), subsystem, event_id, msg);

	//RunHook3(HOOKTYPE_LOG, flags, timebuf, text_buf); // FIXME: call with more parameters and possibly not even 'text_buf' at all

	if (!loop.ircd_forked && (loglevel >= ULOG_ERROR))
	{
#ifdef _WIN32
		win_log("* %s", text_buf);
#else
		fprintf(stderr, "%s", text_buf);
#endif
	}

	/* In case of './unrealircd configtest': don't write to log file, only to stderr */
	if (loop.config_test)
	{
		recursion_trap = 0;
		return;
	}

	/* Log to all ircops for now */
	// FIXME: obviously there should be snomask filtering here ;)
	// TODO: don't show loglevel for simple INFO messages?
	if (strncmp(msg, "->", 2) && strncmp(msg, "<-", 2))
	{
		snomask = log_to_snomask(loglevel, subsystem, event_id);
		if (snomask == SNO_ALL)
			sendto_realops("[%s] %s.%s %s", loglevel_to_string(loglevel), subsystem, event_id, msg);
		else
			sendto_snomask(snomask, "[%s] %s.%s %s", loglevel_to_string(loglevel), subsystem, event_id, msg);
	}

	for (l = conf_log; l; l = l->next)
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
		if (l->type == LOG_TYPE_JSON)
		{
			n = write(l->logfd, json_serialized, strlen(json_serialized));
			if (n < strlen(text_buf))
				write_error = 1;
			else
				write(l->logfd, "\n", 1); // FIXME: no.. we should do it this way..... and why do we use direct I/O at all?
		} else
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

	recursion_trap = 0;
}

/* Logging function, called by the unreal_log() macro. */
void do_unreal_log(LogLevel loglevel, char *subsystem, char *event_id,
                   Client *client, char *msg, ...)
{
	va_list vl;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 1, msg, vl);
	va_end(vl);
}

/* Logging function, called by the unreal_log_raw() macro. */
void do_unreal_log_raw(LogLevel loglevel, char *subsystem, char *event_id,
                       Client *client, char *msg, ...)
{
	va_list vl;
	va_start(vl, msg);
	do_unreal_log_internal(loglevel, subsystem, event_id, client, 0, msg, vl);
	va_end(vl);
}

void do_unreal_log_internal(LogLevel loglevel, char *subsystem, char *event_id,
                            Client *client, int expand_msg, char *msg, va_list vl)
{
	LogData *d;
	char *json_serialized;
	json_t *j = NULL;
	json_t *j_details = NULL;
	char msgbuf[1024];

	/* TODO: Enforcement:
	 * - loglevel must be valid
	 * - subsystem may only contain lowercase, underscore and numbers
	 * - event_id may only contain UPPERCASE, underscore and numbers (but not start with a number)
	 * - msg may not contain percent signs (%) as that is an obvious indication something is wrong?
	 *   or maybe a temporary restriction while upgrading that can be removed later ;)
	 */

	j = json_object();
	j_details = json_object();

	json_object_set_new(j, "timestamp", json_string(timestamp_iso8601()));
	json_object_set_new(j, "level", json_string(loglevel_to_string(loglevel)));
	json_object_set_new(j, "subsystem", json_string(subsystem));
	json_object_set_new(j, "event_id", json_string(event_id));

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
				json_object_set_new(j_details, d->key, json_string(d->value.string));
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

	/* Now call the actual loggers */
	do_unreal_log_loggers(loglevel, subsystem, event_id, msgbuf, json_serialized);

	/* Free everything */
	safe_free(json_serialized);
	json_decref(j_details);
	json_decref(j);
}

void simpletest(void)
{
	char *str;
	json_t *j = json_object();
	json_t *j_client = json_array();

	json_object_set_new(j, "id", json_integer(1));
	json_object_set_new(j, "data", j_client);
	json_array_append_new(j_client, json_integer(1));
	json_array_append_new(j_client, json_integer(2));
	json_array_append_new(j_client, json_integer(3));

	str = json_dumps(j, 0);
	printf("RESULT:\n%s\n", str);
	free(str);

	json_decref(j);
}

void logtest(void)
{
	strcpy(me.name, "irc.test.net");
	unreal_log(ULOG_INFO, "test", "TEST", &me, "Hello there!");
	unreal_log(ULOG_INFO, "test", "TEST", &me, "Hello there i like $client!");
	unreal_log(ULOG_INFO, "test", "TEST", &me, "Hello there i like $client with IP $client.ip!");
	unreal_log(ULOG_INFO, "test", "TEST", &me, "More data!", log_data_string("fun", "yes lots of fun"));
	unreal_log(ULOG_INFO, "test", "TEST", &me, "More data, fun: $fun!", log_data_string("fun", "yes lots of fun"), log_data_integer("some_integer", 1337));
	unreal_log(ULOG_INFO, "sacmds", "SAJOIN_COMMAND", &me, "Client $client used SAJOIN to join $target to y!", log_data_client("target", &me));
}

void add_log_snomask(Configuration *i, char *subsystem, long snomask)
{
	LogSnomask *l = safe_alloc(sizeof(LogSnomask));
	safe_strdup(l->subsystem, subsystem);
	l->snomask = snomask;
	AddListItem(l, i->log_snomasks);
}

void log_snomask_free(LogSnomask *l)
{
	safe_free(l->subsystem);
	safe_free(l);
}

void log_snomask_free_settings(Configuration *i)
{
	LogSnomask *l, *l_next;
	for (l = i->log_snomasks; l; l = l_next)
	{
		l_next = l->next;
		log_snomask_free(l);
	}
	i->log_snomasks = NULL;
}

void log_snomask_setdefaultsettings(Configuration *i)
{
	add_log_snomask(i, "linking", SNO_ALL);
	add_log_snomask(i, "*", SNO_ALL);
}

long log_to_snomask(LogLevel loglevel, char *subsystem, char *event_id)
{
	LogSnomask *l;
	long snomask = 0;

	for (l = iConf.log_snomasks; l; l = l->next)
	{
		if (match_simple(l->subsystem, subsystem))
		{
			if (l->snomask == SNO_ALL)
				return SNO_ALL; /* return early */
			snomask |= l->snomask;
		}
	}
	return snomask;
}
