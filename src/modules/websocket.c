/*
 * websocket - WebSocket support (RFC6455)
 * (C)Copyright 2016 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 * This module was sponsored by Aberrant Software Inc.
 */
   
#include "unrealircd.h"
#include "dns.h"

#define WEBSOCKET_VERSION "1.1.0"

ModuleHeader MOD_HEADER
  = {
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

#define WSU(client)	((WebSocketUser *)moddata_client(client, websocket_md).ptr)

#define WEBSOCKET_PORT(client)	((client->local && client->local->listener) ? client->local->listener->websocket_options : 0)
#define WEBSOCKET_TYPE(client)	(WSU(client)->type)

/* used to parse http Forwarded header (RFC 7239) */
#define IPLEN 48
#define FHEADER_NAMELEN	20

struct HTTPForwardedHeader
{
	int secure;
	char hostname[HOSTLEN+1];
	char ip[IPLEN+1];
};

/* Forward declarations */
int websocket_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int websocket_config_posttest(int *);
int websocket_config_run_ex(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
int websocket_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int websocket_handle_handshake(Client *client, const char *readbuf, int *length);
int websocket_handshake_send_response(Client *client);
int websocket_handle_body_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2);
int websocket_secure_connect(Client *client);
struct HTTPForwardedHeader *websocket_parse_forwarded_header(char *input);
int websocket_ip_compare(const char *ip1, const char *ip2);
int websocket_handle_request(Client *client, WebRequest *web);

/* Global variables */
ModDataInfo *websocket_md;
static int ws_text_mode_available = 1;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, websocket_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, websocket_config_posttest);

	/* Call MOD_INIT very early, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_INIT+1);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	websocket_md = findmoddata_byname("websocket", MODDATATYPE_CLIENT);
	if (!websocket_md)
		config_warn("The 'websocket_common' module is not loaded, even though it was promised to be ???");

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, websocket_config_run_ex);
	HookAdd(modinfo->handle, HOOKTYPE_PACKET, INT_MAX, websocket_packet_out);
	HookAdd(modinfo->handle, HOOKTYPE_SECURE_CONNECT, 0, websocket_secure_connect);

	/* Call MOD_LOAD very late, since we manage sockets, but depend on websocket_common */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_UNLOAD-1);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (non_utf8_nick_chars_in_use || (iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ANY))
		ws_text_mode_available = 0;
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int websocket_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
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
			}
			else if (!strcmp(cep->value, "binary"))
			{
			}
			else
			{
				config_error("%s:%i: listen::options::websocket::type must be either 'binary' or 'text' (not '%s')",
					cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		} else if (!strcmp(cep->name, "forward"))
		{
			if (!cep->value)
			{
				config_error_empty(cep->file->filename, cep->line_number, "listen::options::websocket::forward", cep->name);
				errors++;
				continue;
			}
			if (!is_valid_ip(cep->value))
			{
				config_error("%s:%i: invalid IP address '%s' in listen::options::websocket::forward", cep->file->filename, cep->line_number, cep->value);
				errors++;
				continue;
			}
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

int websocket_config_run_ex(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr)
{
	ConfigEntry *cep, *cepp;
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
		} else if (!strcmp(cep->name, "forward"))
		{
			safe_strdup(l->websocket_forward, cep->value);
		}
	}
	return 1;
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

	if (l >= sizeof(newbuf)-2)
		l = sizeof(newbuf)-2; /* cut-off if necessary */

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
	static char utf8buf[510];

	if (MyConnect(to) && !IsRPC(to) && websocket_md && WSU(to) && WSU(to)->handshake_completed)
	{
		if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_BINARY)
			websocket_create_packet(WSOP_BINARY, msg, length);
		else if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_TEXT)
		{
			/* Some more conversions are needed */
			char *safe_msg = unrl_utf8_make_valid(*msg, utf8buf, sizeof(utf8buf), 1);
			*msg = safe_msg;
			*length = *msg ? strlen(safe_msg) : 0;
			websocket_create_packet(WSOP_TEXT, msg, length);
		}
		return 0;
	}
	return 0;
}

#define FHEADER_STATE_NAME	0
#define FHEADER_STATE_VALUE	1
#define FHEADER_STATE_VALUE_QUOTED	2

#define FHEADER_ACTION_APPEND	0
#define FHEADER_ACTION_IGNORE	1
#define FHEADER_ACTION_PROCESS	2

/** If a valid Forwarded: http header is received from a trusted source (proxy server), this function will
  * extract remote IP address and secure (https) status from it. If more than one field with same name is received,
  * we'll accept the last one. This should work correctly with chained proxies. */
struct HTTPForwardedHeader *websocket_parse_forwarded_header(char *input)
{
	static struct HTTPForwardedHeader forwarded;
	int i, length;
	int state = FHEADER_STATE_NAME, action = FHEADER_ACTION_APPEND;
	char name[FHEADER_NAMELEN+1];
	char value[IPLEN+1];
	int name_length = 0;
	int value_length = 0;
	char c;
	
	memset(&forwarded, 0, sizeof(struct HTTPForwardedHeader));
	
	length = strlen(input);
	for (i = 0; i < length; i++)
	{
		c = input[i];
		switch (c)
		{
			case '"':
				switch (state)
				{
					case FHEADER_STATE_NAME:
						action = FHEADER_ACTION_APPEND;
						break;
					case FHEADER_STATE_VALUE:
						action = FHEADER_ACTION_IGNORE;
						state = FHEADER_STATE_VALUE_QUOTED;
						break;
					case FHEADER_STATE_VALUE_QUOTED:
						action = FHEADER_ACTION_IGNORE;
						state = FHEADER_STATE_VALUE;
						break;
				}
				break;
			case ',': case ';': case ' ':
				switch (state)
				{
					case FHEADER_STATE_NAME: /* name without value */
						name_length = 0;
						action = FHEADER_ACTION_IGNORE;
						break;
					case FHEADER_STATE_VALUE: /* end of value */
						action = FHEADER_ACTION_PROCESS;
						break;
					case FHEADER_STATE_VALUE_QUOTED: /* quoted character, process as normal */
						action = FHEADER_ACTION_APPEND;
						break;
				}
				break;
			case '=':
				switch (state)
				{
					case FHEADER_STATE_NAME: /* end of name */
						name[name_length] = '\0';
						state = FHEADER_STATE_VALUE;
						action = FHEADER_ACTION_IGNORE;
						break;
					case FHEADER_STATE_VALUE: case FHEADER_STATE_VALUE_QUOTED: /* none of the values is expected to contain = but proceed anyway */
						action = FHEADER_ACTION_APPEND;
						break;
				}
				break;
			default:
				action = FHEADER_ACTION_APPEND;
				break;
		}
		switch (action)
		{
			case FHEADER_ACTION_APPEND:
				if (state == FHEADER_STATE_NAME)
				{
					if (name_length < FHEADER_NAMELEN)
					{
						name[name_length++] = c;
					} else
					{
						/* truncate */
					}
				} else
				{
					if (value_length < IPLEN)
					{
						value[value_length++] = c;
					} else
					{
						/* truncate */
					}
				}
				break;
			case FHEADER_ACTION_IGNORE: default:
				break;
			case FHEADER_ACTION_PROCESS:
				value[value_length] = '\0';
				name[name_length] = '\0';
				if (!strcasecmp(name, "for"))
				{
					strlcpy(forwarded.ip, value, IPLEN+1);
				} else if (!strcasecmp(name, "proto"))
				{
					if (!strcasecmp(value, "https"))
					{
						forwarded.secure = 1;
					} else if (!strcasecmp(value, "http"))
					{
						forwarded.secure = 0;
					} else
					{
						/* ignore unknown value */
					}
				} else
				{
					/* ignore unknown field name */
				}
				value_length = 0;
				name_length = 0;
				state = FHEADER_STATE_NAME;
				break;
		}
	}
	
	return &forwarded;
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
		} else
		if (!strcasecmp(key, "Sec-WebSocket-Protocol"))
		{
			/* Save it here, will be processed later */
			safe_strdup(WSU(client)->sec_websocket_protocol, value);
		} else
		if (!strcasecmp(key, "Forwarded"))
		{
			/* will be processed later too */
			safe_strdup(WSU(client)->forwarded, value);
		}
	}

	/** Finally, validate the websocket request (handshake) and proceed or reject. */

	/* Not websocket and webredir loaded? Let that module serve a redirect. */
	if (!WSU(client)->handshake_key)
	{
		if (is_module_loaded("webredir"))
		{
			const char *parx[2] = { NULL, NULL };
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
			} else
			if (!strcmp(name, "text.ircv3.net") && ws_text_mode_available)
			{
				negotiated = WEBSOCKET_TYPE_TEXT;
				break; /* First hit wins */
			}
		}
		if (negotiated == WEBSOCKET_TYPE_BINARY)
		{
			WSU(client)->type = WEBSOCKET_TYPE_BINARY;
			safe_strdup(WSU(client)->sec_websocket_protocol, "binary.ircv3.net");
		} else
		if (negotiated == WEBSOCKET_TYPE_TEXT)
		{
			WSU(client)->type = WEBSOCKET_TYPE_TEXT;
			safe_strdup(WSU(client)->sec_websocket_protocol, "text.ircv3.net");
		} else
		{
			/* Negotiation failed, fallback to the default (don't set it here) */
			safe_free(WSU(client)->sec_websocket_protocol);
		}
	}

	/* Check forwarded header (by k4be) */
	if (WSU(client)->forwarded)
	{
		struct HTTPForwardedHeader *forwarded;
		char oldip[64];

		/* check for source ip */
		if (BadPtr(client->local->listener->websocket_forward) || !websocket_ip_compare(client->local->listener->websocket_forward, client->ip))
		{
			unreal_log(ULOG_WARNING, "websocket", "UNAUTHORIZED_FORWARDED_HEADER", client, "Received unauthorized Forwarded header from $ip", log_data_string("ip", client->ip));
			webserver_send_response(client, 403, "Forwarded: no access");
			return 0;
		}
		/* parse the header */
		forwarded = websocket_parse_forwarded_header(WSU(client)->forwarded);
		/* check header values */
		if (!is_valid_ip(forwarded->ip))
		{
			unreal_log(ULOG_WARNING, "websocket", "INVALID_FORWARDED_IP", client, "Received invalid IP in Forwarded header from $ip", log_data_string("ip", client->ip));
			webserver_send_response(client, 400, "Forwarded: invalid IP");
			return 0;
		}
		/* store data */
		WSU(client)->secure = forwarded->secure;
		strlcpy(oldip, client->ip, sizeof(oldip));
		safe_strdup(client->ip, forwarded->ip);
		/* Update client->local->hostp */
		strlcpy(client->local->sockhost, forwarded->ip, sizeof(client->local->sockhost)); /* in case dns lookup fails or is disabled */
		/* (free old) */
		if (client->local->hostp)
		{
			unreal_free_hostent(client->local->hostp);
			client->local->hostp = NULL;
		}
		/* (create new) */
		if (!DONT_RESOLVE)
		{
			/* taken from socket.c */
			struct hostent *he;
			unrealdns_delreq_bycptr(client); /* in case the proxy ip is still in progress of being looked up */
			ClearDNSLookup(client);
			he = unrealdns_doclient(client); /* call this once more */
			if (!client->local->hostp)
			{
				if (!he)
					SetDNSLookup(client); /* DNS lookup in progress */
				else if (!he->h_name)
					unreal_free_hostent(he); /* unresolved IP (negcache) */
				else
					client->local->hostp = he; /* cached */
			} else
			{
				/* Race condition detected, DNS has been done, continue with auth */
			}
		}
		RunHook(HOOKTYPE_IP_CHANGE, client, oldip);
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
	if (IsSecureConnect(client) && websocket_md && WSU(client) && WSU(client)->forwarded && !WSU(client)->secure)
		client->umodes &= ~UMODE_SECURE;
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
		snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
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

/** Compare IP addresses (for authorization checking) */
int websocket_ip_compare(const char *ip1, const char *ip2)
{
	uint32_t ip4[2];
	uint16_t ip6[16];
	int i;
	if (inet_pton(AF_INET, ip1, &ip4[0]) == 1) /* IPv4 */
	{
		if (inet_pton(AF_INET, ip2, &ip4[1]) == 1) /* both are valid, let's compare */
		{
			return ip4[0] == ip4[1];
		}
		return 0;
	}
	if (inet_pton(AF_INET6, ip1, &ip6[0]) == 1) /* IPv6 */
	{
		if (inet_pton(AF_INET6, ip2, &ip6[8]) == 1)
		{
			for (i = 0; i < 8; i++)
			{
				if (ip6[i] != ip6[i+8])
					return 0;
			}
			return 1;
		}
	}
	return 0; /* neither valid IPv4 nor IPv6 */
}

