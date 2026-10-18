/*
 * websocket - WebSocket support (RFC6455)
 * (C)Copyright 2016 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 * This module was sponsored by Aberrant Software Inc.
 */

#include "unrealircd.h"

#define WEBSOCKET_VERSION "1.1.0"

ModuleHeader MOD_HEADER = {
    "websocket",
    WEBSOCKET_VERSION,
    "WebSocket support (RFC6455)",
    "UnrealIRCd Team",
    "unrealircd-6",
};

#if CHAR_MIN < 0
 #error "In UnrealIRCd char should always be unsigned. Check your compiler"
#endif

#ifndef WEBSOCKET_SEND_BUFFER_SIZE
 #define WEBSOCKET_SEND_BUFFER_SIZE 16384
#endif

#define WSU(client) ((WebSocketUser *)moddata_client(client, websocket_md).ptr)
#define WEB(client) ((WebRequest *)moddata_local_client(client, webserver_md).ptr)

#define WEBSOCKET_PORT(client) ((client->local && client->local->listener) ? client->local->listener->websocket_options : 0)
#define WEBSOCKET_TYPE(client) (WSU(client)->type)

/* Forward declarations */
int websocket_config_test_listener(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int websocket_config_test_set(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int websocket_config_posttest(int *);
int websocket_config_run_listener(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
int websocket_config_run_set(ConfigFile *cf, ConfigEntry *ce, int type);
int websocket_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int websocket_handle_handshake(Client *client, const char *readbuf, int *length);
int websocket_handshake_send_response(Client *client);
int websocket_handle_body_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2);
int websocket_secure_connect(Client *client);
int websocket_handle_request(Client *client, WebRequest *web);
int websocket_config_listener(ConfigItem_listen *listener);
static int websocket_config_valid_host_entry(const char *str);
int websocket_stats(Client *client, const char *para);
static int websocket_check_origin(Client *client, WebRequest *web);
static void parse_origin_host(const char *origin, char *host, size_t hostsize);

/* set::websocket */
static struct {
	NameList *allowed_origins; /**< Permitted Origins (eg official webchat of your network) */
} cfg;

/* Global variables */
ModDataInfo *websocket_md = NULL; /* (by us) */
ModDataInfo *webserver_md = NULL; /* (external module, looked up) */
static int ws_text_mode_available = 1;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, websocket_config_test_listener);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, websocket_config_test_set);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, websocket_config_posttest);

	/* Call MOD_INIT very early, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_INIT + 1);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	websocket_md = findmoddata_byname("websocket", MODDATATYPE_CLIENT);
	if (!websocket_md)
		config_warn("The 'websocket_common' module is not loaded, even though it was promised to be ???");

	memset(&cfg, 0, sizeof(cfg));

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, websocket_config_run_listener);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, websocket_config_run_set);
	HookAdd(modinfo->handle, HOOKTYPE_PACKET, INT_MAX, websocket_packet_out);
	HookAdd(modinfo->handle, HOOKTYPE_SECURE_CONNECT, 0, websocket_secure_connect);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIG_LISTENER, 0, websocket_config_listener);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, websocket_stats);

	/* Call MOD_LOAD very late, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_UNLOAD - 1);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	webserver_md = findmoddata_byname("web", MODDATATYPE_LOCAL_CLIENT);

	if (non_utf8_nick_chars_in_use || (iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ANY))
		ws_text_mode_available = 0;

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_entire_name_list(cfg.allowed_origins);
	return MOD_SUCCESS;
}

int websocket_config_test_listener(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_type = 0;
	static char errored_once_nick = 0;

	if (type != CONFIG_LISTEN_OPTIONS)
		return 0;

	/* We are only interrested in listen::options::websocket.. */
	if (!ce || !ce->name || strcmp(ce->name, "websocket"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "type"))
		{
			CheckNull(cep);
			has_type = 1;
			if (!strcmp(cep->value, "text"))
			{
				if (non_utf8_nick_chars_in_use && !errored_once_nick)
				{
					/* This one is a hard error, since the consequences are grave */
					config_error("You have a websocket listener with type 'text' AND your set::allowed-nickchars contains at least one non-UTF8 character set.");
					config_error("This is a very BAD idea as this makes your websocket vulnerable to UTF8 conversion attacks. "
					             "This can cause things like unkickable users and 'ghosts' for websocket users.");
					config_error("You have 4 options: 1) Remove the websocket listener, 2) Use websocket type 'binary', "
					             "3) Remove the non-UTF8 character set from set::allowed-nickchars, 4) Replace the non-UTF8 with an UTF8 character set in set::allowed-nickchars");
					config_error("For more details see https://www.unrealircd.org/docs/WebSocket_support#websockets-and-non-utf8");
					errored_once_nick = 1;
					errors++;
				}
			} else if (!strcmp(cep->value, "binary"))
			{
			} else
			{
				config_error("%s:%i: listen::options::websocket::type must be either 'binary' or 'text' (not '%s')",
				             cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		} else if (!strcmp(cep->name, "forward"))
		{
			config_error("%s:%i: this functionality has been moved to the proxy { } block. "
			             "See https://www.unrealircd.org/docs/Proxy_block",
			             cep->file->filename, cep->line_number);
			errors++;
		} else if (!strcmp(cep->name, "allow-origin"))
		{
			config_error("%s:%i: listen::options::websocket::allow-origin has been moved to "
			             "set::websocket::allowed-origins. "
			             "See https://www.unrealircd.org/docs/Set_block#set::websocket",
			             cep->file->filename, cep->line_number);
			errors++;
		} else
		{
			config_error("%s:%i: unknown directive listen::options::websocket::%s",
			             cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}

	if (!has_type)
	{
		config_error("%s:%i: websocket set, but type unspecified. Use something like: listen { ip *; port 443; websocket { type text; } }",
		             ce->file->filename, ce->line_number);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int websocket_config_run_listener(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr)
{
	ConfigEntry *cep;
	ConfigItem_listen *l;
	static char warned_once_channel = 0;

	if (type != CONFIG_LISTEN_OPTIONS)
		return 0;

	/* We are only interrested in listen::options::websocket.. */
	if (!ce || !ce->name || strcmp(ce->name, "websocket"))
		return 0;

	l = (ConfigItem_listen *)ptr;
	l->webserver = safe_alloc(sizeof(WebServer));
	l->webserver->handle_request = websocket_handle_request;
	l->webserver->handle_body = websocket_handle_body_websocket;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "type"))
		{
			if (!strcmp(cep->value, "binary"))
				l->websocket_options = WEBSOCKET_TYPE_BINARY;
			else if (!strcmp(cep->value, "text"))
			{
				l->websocket_options = WEBSOCKET_TYPE_TEXT;
				if ((tempiConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ANY) && !warned_once_channel)
				{
					/* This one is a warning, since the consequences are less grave than with nicks */
					config_warn("You have a websocket listener with type 'text' AND your set::allowed-channelchars is set to 'any'.");
					config_warn("This is not a recommended combination as this makes your websocket vulnerable to UTF8 conversion attacks. "
					            "This can cause things like unpartable channels for websocket users.");
					config_warn("It is highly recommended that you use set { allowed-channelchars utf8; }");
					config_warn("For more details see https://www.unrealircd.org/docs/WebSocket_support#websockets-and-non-utf8");
					warned_once_channel = 1;
				}
			}
		}
	}
	return 1;
}

/** Is this a valid hostname entry for set::websocket::allowed-origins? */
static int websocket_config_valid_host_entry(const char *str)
{
	const char *p;

	if (!*str || (strlen(str) > HOSTLEN))
		return 0;

	/* Explicitly allow IPv4 or IPv6 (with colons) */
	if (is_valid_ip(str))
		return 1;

	/* Similar to valid_host(), but also allow '?' and '*' */
	for (p = str; *p; p++)
		if (!isalnum(*p) && !strchr("-._*?", *p))
			return 0;

	return 1;
}

/** Config test for set::websocket */
int websocket_config_test_set(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name || strcmp(ce->name, "websocket"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "allowed-origins"))
		{
			if (cep->value)
			{
				config_error("%s:%i: set::websocket::allowed-origins must be a block, eg: "
				             "allowed-origins { webchat.example.net; }",
				             cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!websocket_config_valid_host_entry(cepp->name))
				{
					config_error("%s:%i: set::websocket::allowed-origins: invalid hostname '%s'. "
					             "Use a hostname like webchat.example.net",
					             cepp->file->filename, cepp->line_number, cepp->name);
					errors++;
				}
			}
		} else
		{
			config_error("%s:%i: unknown directive set::websocket::%s",
			             cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/** Config run for set::websocket */
int websocket_config_run_set(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name || strcmp(ce->name, "websocket"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "allowed-origins"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
				append_name_list(cfg.allowed_origins, cepp->name);
		}
	}

	return 1;
}

int websocket_config_listener(ConfigItem_listen *listener)
{
	if (listener->websocket_options)
	{
		listener->webserver->handle_request = websocket_handle_request;
		listener->webserver->handle_body = websocket_handle_body_websocket;
	}
	return 0;
}

int websocket_config_posttest(int *errs)
{
	int errors = 0;
	char webserver_module = 1, websocket_common_module = 1;

	if (!is_module_loaded("webserver"))
	{
		config_error("The 'websocket' module requires the 'webserver' module to be loaded, otherwise websocket connections will not work!");
		webserver_module = 0;
		errors++;
	}

	if (!is_module_loaded("websocket_common"))
	{
		config_error("The 'websocket' module requires the 'websocket_common' module to be loaded, otherwise websocket connections will not work!");
		websocket_common_module = 0;
		errors++;
	}

	/* Is nicer for the admin when these are grouped... */
	if (!webserver_module)
		config_error("Add the following line to your config file: loadmodule \"webserver\";");
	if (!websocket_common_module)
		config_error("Add the following line to your config file: loadmodule \"websocket_common\";");

	*errs = errors;
	return errors ? -1 : 1;
}

int websocket_stats(Client *client, const char *para)
{
	NameList *n;

	if (strcmp(para, "S"))
		return 0;

	for (n = cfg.allowed_origins; n; n = n->next)
		sendtxtnumeric(client, "websocket::allowed-origins: %s", n->name);

	return 1;
}

/* Add LF (if needed) to a buffer. Max 4K. */
void add_lf_if_needed(char **buf, int *len)
{
	static char newbuf[MAXLINELENGTH];
	char *b = *buf;
	int l = *len;

	if (l <= 0)
		return; /* too short */

	if (b[l - 1] == '\n')
		return; /* already contains \n */

	if (l >= sizeof(newbuf) - 2)
		l = sizeof(newbuf) - 2; /* cut-off if necessary */

	memcpy(newbuf, b, l);
	newbuf[l] = '\n';
	newbuf[l + 1] = '\0'; /* not necessary, but I like zero termination */
	l++;
	*buf = newbuf; /* new buffer */
	*len = l; /* new length */
}

/** Called on decoded websocket frame (INPUT).
 * Should contain exactly 1 IRC line (command)
 */
int websocket_irc_callback(Client *client, char *buf, int len)
{
	add_lf_if_needed(&buf, &len);
	if (!process_packet(client, buf, len, 1)) /* Let UnrealIRCd handle this as usual */
		return 0; /* client killed */
	return 1;
}

int websocket_handle_body_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2)
{
	return websocket_handle_websocket(client, web, readbuf2, length2, websocket_irc_callback);
}

/** Outgoing packet hook.
 * This transforms the output to be Websocket-compliant, if necessary.
 */
int websocket_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	/* 8192 = MessageTag max length
 	 * 512 = IRC line length
   	 * 1 = the space between them
	 * 1 = NULL terminator?
 	*/
	static char utf8buf[8192 + 512 + 1 + 1];

	if (MyConnect(to) && !IsRPC(to) && websocket_md && WSU(to) && WSU(to)->handshake_completed)
	{
		if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_BINARY)
			websocket_create_packet(WSOP_BINARY, msg, length);
		else if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_TEXT)
		{
			/* Some more conversions are needed */
			char *safe_msg = unrl_utf8_make_valid(*msg, utf8buf, sizeof(utf8buf), 2);
			*msg = safe_msg;
			*length = *msg ? strlen(safe_msg) : 0;
			websocket_create_packet(WSOP_TEXT, msg, length);
		}
		return 0;
	}
	return 0;
}

/** We got a HTTP(S) request and we need to check if we can upgrade the connection
 * to a websocket connection.
 */
int websocket_handle_request(Client *client, WebRequest *web)
{
	NameValuePrioList *r;
	const char *key, *value;

	/* Allocate a new WebSocketUser struct for this session */
	moddata_client(client, websocket_md).ptr = safe_alloc(sizeof(WebSocketUser));
	/* ...and set the default protocol (text or binary) */
	WSU(client)->type = client->local->listener->websocket_options;

	/** Now step through the lines.. **/
	for (r = web->headers; r; r = r->next)
	{
		key = r->name;
		value = r->value;
		if (!strcasecmp(key, "Sec-WebSocket-Key"))
		{
			if (strchr(value, ':'))
			{
				/* This would cause unserialization issues. Should be base64 anyway */
				webserver_send_response(client, 400, "Invalid characters in Sec-WebSocket-Key");
				return -1;
			}
			safe_strdup(WSU(client)->handshake_key, value);
		} else if (!strcasecmp(key, "Sec-WebSocket-Protocol"))
		{
			/* Save it here, will be processed later */
			safe_strdup(WSU(client)->sec_websocket_protocol, value);
		}
	}

	if (!websocket_check_origin(client, web))
		return 0;

	/* Not websocket and webredir loaded? Let that module serve a redirect. */
	if (!WSU(client)->handshake_key)
	{
		if (is_module_loaded("webredir"))
		{
			const char *parx[2] = {NULL, NULL};
			do_cmd(client, NULL, "GET", 1, parx);
		}
		webserver_send_response(client, 404, "This port is for IRC WebSocket only");
		return 0;
	}

	/* Sec-WebSocket-Protocol (optional) */
	if (WSU(client)->sec_websocket_protocol)
	{
		char *p = NULL, *name;
		int negotiated = 0;

		for (name = strtoken(&p, WSU(client)->sec_websocket_protocol, ",");
		     name;
		     name = strtoken(&p, NULL, ","))
		{
			skip_whitespace(&name);
			if (!strcmp(name, "binary.ircv3.net"))
			{
				negotiated = WEBSOCKET_TYPE_BINARY;
				break; /* First hit wins */
			} else if (!strcmp(name, "text.ircv3.net") && ws_text_mode_available)
			{
				negotiated = WEBSOCKET_TYPE_TEXT;
				break; /* First hit wins */
			}
		}
		if (negotiated == WEBSOCKET_TYPE_BINARY)
		{
			WSU(client)->type = WEBSOCKET_TYPE_BINARY;
			safe_strdup(WSU(client)->sec_websocket_protocol, "binary.ircv3.net");
		} else if (negotiated == WEBSOCKET_TYPE_TEXT)
		{
			WSU(client)->type = WEBSOCKET_TYPE_TEXT;
			safe_strdup(WSU(client)->sec_websocket_protocol, "text.ircv3.net");
		} else
		{
			/* Negotiation failed, fallback to the default (don't set it here) */
			safe_free(WSU(client)->sec_websocket_protocol);
		}
	}

	websocket_handshake_send_response(client);
	return 1;
}

int websocket_secure_connect(Client *client)
{
	/* Remove secure mode (-z) if the WEBIRC gateway did not ensure
	 * us that their [client]--[webirc gateway] connection is also
	 * secure (eg: using https)
	 */
	if (IsSecureConnect(client) && websocket_md && WSU(client) &&
	    WEB(client)->forwarded && !WEB(client)->forwarded->secure)
	{
		client->umodes &= ~UMODE_SECURE;
	}
	return 0;
}

/** Complete the handshake by sending the appropriate HTTP 101 response etc. */
int websocket_handshake_send_response(Client *client)
{
	char buf[512], hashbuf[64];
	char sha1out[20]; /* 160 bits */

	WSU(client)->handshake_completed = 1;

	snprintf(buf, sizeof(buf), "%s%s", WSU(client)->handshake_key, WEBSOCKET_MAGIC_KEY);
	sha1hash_binary(sha1out, buf, strlen(buf));
	b64_encode(sha1out, sizeof(sha1out), hashbuf, sizeof(hashbuf));

	snprintf(buf, sizeof(buf),
	         "HTTP/1.1 101 Switching Protocols\r\n"
	         "Upgrade: websocket\r\n"
	         "Connection: Upgrade\r\n"
	         "Sec-WebSocket-Accept: %s\r\n",
	         hashbuf);

	if (WSU(client)->sec_websocket_protocol)
	{
		/* using strlen() is safe here since above buffer will not
		 * cause it to be >=512 and thus we won't get into negatives.
		 */
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		         "Sec-WebSocket-Protocol: %s\r\n",
		         WSU(client)->sec_websocket_protocol);
	}

	strlcat(buf, "\r\n", sizeof(buf));

	/* Caution: we bypass sendQ flood checking by doing it this way.
	 * Risk is minimal, though, as we only permit limited text only
	 * once per session.
	 */
	dbuf_put(&client->local->sendQ, buf, strlen(buf));
	send_queued(client);

	return 0;
}

/** Validate the websocket origin.
 * @returns 1 if allowed, 0 if rejected (403 already sent).
 */
static int websocket_check_origin(Client *client, WebRequest *web)
{
	const char *origin = get_nvplist(web->headers, "Origin");
	char origin_host[HOSTLEN + 1];

	if (!origin)
		return 1; /* No Origin header = non-browser client = allowed */

	parse_origin_host(origin, origin_host, sizeof(origin_host));
	if (!strcmp(origin, "null"))
		safe_strdup(WSU(client)->origin, "null");
	else if (valid_host(origin_host, 1) || is_valid_ip(origin_host))
		safe_strdup(WSU(client)->origin, origin_host);

	if (!cfg.allowed_origins)
		return 1; /* No set::websocket::allowed-origins = no restrictions */

	/* We don't care about origins that are not http:// or https://.
	 * This because we don't want to block things like tauri:// phone apps.
	 */
	if (strstr(origin, "://") && strncasecmp(origin, "http://", 7) && strncasecmp(origin, "https://", 8))
		return 1;

	if (*origin_host)
	{
		/* Some phone apps use this (Tauri and Capacitor), safe to exempt. */
		if (!strcasecmp(origin_host, "localhost") || match_simple("*.localhost", origin_host))
			return 1;
		if (find_name_list_match(cfg.allowed_origins, origin_host))
			return 1;
	}

	webserver_send_response(client, 403, "This site is not permitted to connect to us with websockets");
	return 0;
}

/** Extract the hostname from an Origin header value,
 * eg "https://webchat.example.net:8443" becomes "webchat.example.net".
 */
static void parse_origin_host(const char *origin, char *host, size_t hostsize)
{
	const char *start, *end;

	*host = '\0';

	start = strstr(origin, "://");
	if (!start)
		return;
	start += 3;

	if (*start == '[')
	{
		/* IPv6: https://[::1]:8080 */
		start++;
		end = strchr(start, ']');
		if (!end)
			return;
	} else
	{
		/* IPv4/hostname: https://example.com:8443 */
		end = start;
		while (*end && *end != ':' && *end != '/')
			end++;
	}

	if (end - start >= hostsize)
		return;

	strlncpy(host, start, hostsize, end - start);
}
