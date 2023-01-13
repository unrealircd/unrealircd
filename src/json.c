/************************************************************************
 * UnralIRCd JSON functions, src/json.c
 * (C) 2021-.. Bram Matthys (Syzop) and the UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

/** @file
 * @brief JSON functions - used for logging and RPC.
 */

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

int json_object_get_boolean(json_t *j, const char *name, int default_value)
{
	json_t *v = json_object_get(j, name);
	if (!v)
		return default_value;
	if (json_is_true(v))
		return 1;
	return 0;
}

#define json_string __BAD___DO__NOT__USE__JSON__STRING__PLZ

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

json_t *json_timestamp(time_t v)
{
	const char *ts = timestamp_iso8601(v);
	if (ts)
		return json_string_unreal(ts);
	return json_null();
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
			if (detail == 0)
			{
				/* Short format, mainly for JSON logging */
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
			} else {
				/* Long format for JSON-RPC */
				for (m = client->user->channel; m; m = m->next)
				{
					json_t *e = json_object();
					json_object_set_new(e, "name", json_string_unreal(m->channel->name));
					if (*m->member_modes)
						json_object_set_new(e, "level", json_string_unreal(m->member_modes));
					json_array_append_new(channels, e);
				}
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

		/* client.server.features */
		features = json_object();
		json_object_set_new(server, "features", features);
		if (!BadPtr(client->server->features.software))
			json_object_set_new(features, "software", json_string_unreal(version));
		json_object_set_new(features, "protocol", json_integer(UnrealProtocol));
		if (!BadPtr(client->server->features.usermodes))
			json_object_set_new(features, "usermodes", json_string_unreal(umodestring));

		/* client.server.features.chanmodes (array) */
		{
			int i;
			char buf[512];
			json_t *chanmodes = json_array();
			json_object_set_new(features, "chanmodes", chanmodes);
			/* first one is special - wait.. is this still the case? lol. */
			snprintf(buf, sizeof(buf), "%s%s", CHPAR1, EXPAR1);
			json_array_append_new(chanmodes, json_string_unreal(buf));
			for (i=1; i < 4; i++)
				json_array_append_new(chanmodes, json_string_unreal(extchmstr[i]));
		}
		if (!BadPtr(client->server->features.nickchars))
			json_object_set_new(features, "nick_character_sets", json_string_unreal(charsys_get_current_languages()));

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
		json_object_set_new(server, "ulined", json_boolean(IsULine(client)));

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

void json_expand_channel_ban(json_t *child, const char *banlist_name, Ban *banlist)
{
	Ban *ban;
	json_t *list, *e;

	list = json_array();
	json_object_set_new(child, banlist_name, list);
	for (ban = banlist; ban; ban = ban->next)
	{
		e = json_object();
		json_array_append_new(list, e);
		json_object_set_new(e, "name", json_string_unreal(ban->banstr));
		json_object_set_new(e, "set_by", json_string_unreal(ban->who));
		json_object_set_new(e, "set_at", json_timestamp(ban->when));
	}
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

	if (detail > 1)
	{
		json_expand_channel_ban(child, "bans", channel->banlist);
		json_expand_channel_ban(child, "ban_exemptions", channel->exlist);
		json_expand_channel_ban(child, "invite_exceptions", channel->invexlist);
	}

	if (detail > 2)
	{
		Member *u;
		json_t *list = json_array();
		json_object_set_new(child, "members", list);

		for (u = channel->members; u; u = u->next)
		{
			json_t *e = json_object();
			json_object_set_new(e, "name", json_string_unreal(u->client->name));
			json_object_set_new(e, "id", json_string_unreal(u->client->id));
			if (*u->member_modes)
				json_object_set_new(e, "level", json_string_unreal(u->member_modes));
			json_array_append_new(list, e);
		}
	}

	// Possibly later: If detail is set to 1 then expand more...
	RunHook(HOOKTYPE_JSON_EXPAND_CHANNEL, channel, detail, child);
}

void json_expand_tkl(json_t *root, const char *key, TKL *tkl, int detail)
{
	char buf[BUFSIZE];
	json_t *j;

	if (key)
	{
		j = json_object();
		json_object_set_new(root, key, j);
	} else {
		j = root;
	}

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
	if (tkl->flags & TKL_FLAG_CONFIG)
		json_object_set_new(j, "set_in_config", json_boolean(1));
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
		json_object_set_new(j, "ban_duration", json_integer(tkl->ptr.spamfilter->tkl_duration));
		json_object_set_new(j, "ban_duration_string", json_string_unreal(pretty_time_val_r(buf, sizeof(buf), tkl->ptr.spamfilter->tkl_duration)));
		json_object_set_new(j, "spamfilter_targets", json_string_unreal(spamfilter_target_inttostring(tkl->ptr.spamfilter->target)));
		json_object_set_new(j, "reason", json_string_unreal(unreal_decodespace(tkl->ptr.spamfilter->tkl_reason)));
	}
}
