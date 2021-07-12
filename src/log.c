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

// TODO: validate that all 'key' values are lowercase+underscore+digits in all functions below.

void json_expand_client(json_t *j, char *key, Client *client, int detail)
{
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

			if (!isalnum(*i))
			{
				/* What do we do with things like '$/' ? -- treat literal */
				i--;
				goto literal;
			}

			/* find termination */
			for (p=i; isalnum(*p) || ((*p == '.') && isalnum(p[1])); p++);

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
						output = json_string_value(t);
				} else
				if (json_is_object(t))
				{
					/* Fetch object.name */
					t = json_object_get(t, "name");
					if (t)
						output = json_string_value(t);
				} else
				{
					output = json_string_value(t);
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

void do_unreal_log(LogLevel loglevel, char *subsystem, char *event_id,
                Client *client,
                char *msg, ...)
{
	va_list vl;
	LogData *d;
	char *str;
	json_t *j = NULL;
	json_t *j_details = NULL;
	const char *fmt_name[32], *fmt_value[32];
	char msgbuf[1024];

	/* TODO: Enforcement:
	 * - loglevel must be valid
	 * - subsystem may only contain lowercase, underscore and numbers
	 * - event_id may only contain UPPERCASE, underscore and numbers (but not start with a number)
	 * - msg may not contain percent signs (%) as that is an obvious indication something is wrong?
	 *   or maybe a temporary restriction while upgrading that can be removed later ;)
	 */
	memset(&fmt_name, 0, sizeof(fmt_name));
	memset(&fmt_value, 0, sizeof(fmt_value));

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
	va_start(vl, msg);
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
	// todo: use a special buildvarstring() so we don't end up strdup'ing lots of stuff that is unused anyway
	fmt_name[0] = "client";
	fmt_value[0] = client ? client->name : "";
	*msgbuf = '\0';
	//buildvarstring(msg, msgbuf, sizeof(msgbuf), fmt_name, fmt_value);
	buildlogstring(msg, msgbuf, sizeof(msgbuf), j_details);
	json_object_set_new(j, "msg", json_string(msgbuf));

	/* Now merge the details into root object 'j': */
	json_object_update_missing(j, j_details);

	str = json_dumps(j, 0);
	ircd_log(LOG_ERROR, "STD-STR: %s %s %s %s", loglevel_to_string(loglevel), subsystem, event_id, msgbuf);
	ircd_log(LOG_ERROR, "JSON-STR: %s", str);
	free(str);

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
