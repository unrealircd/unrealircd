/*
 * websocket - WebSocket support (RFC6455)
 * (C)Copyright 2016 Bram Matthys and the UnrealIRCd team
 * License: GPLv2
 * This module was sponsored by Aberrant Software Inc.
 */
   
#include "unrealircd.h"

#define WEBSOCKET_VERSION "1.1.0"

ModuleHeader MOD_HEADER
  = {
	"websocket",
	WEBSOCKET_VERSION,
	"WebSocket support (RFC6455)",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

#if CHAR_MIN < 0
 #error "In UnrealIRCd char should always be unsigned. Check your compiler"
#endif

#ifndef WEBSOCKET_SEND_BUFFER_SIZE
 #define WEBSOCKET_SEND_BUFFER_SIZE 16384
#endif

typedef enum WebSocketType {
	WEBSOCKET_TYPE_BINARY = 1,
	WEBSOCKET_TYPE_TEXT   = 2
} WebSocketType;

typedef struct WebSocketUser WebSocketUser;
struct WebSocketUser {
	char get; /**< GET initiated */
	char handshake_completed; /**< Handshake completed, use websocket frames */
	char *handshake_key; /**< Handshake key (used during handshake) */
	char *lefttoparse; /**< Leftover buffer to parse */
	int lefttoparselen; /**< Length of lefttoparse buffer */
	WebSocketType type; /**< WEBSOCKET_TYPE_BINARY or WEBSOCKET_TYPE_TEXT */
	char *sec_websocket_protocol; /**< Only valid during parsing of the request, after that it is NULL again */
};

#define WSU(client)	((WebSocketUser *)moddata_client(client, websocket_md).ptr)

#define WEBSOCKET_PORT(client)	((client->local && client->local->listener) ? client->local->listener->websocket_options : 0)
#define WEBSOCKET_TYPE(client)	(WSU(client)->type)

#define WEBSOCKET_MAGIC_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11" /* see RFC6455 */

#define WSOP_CONTINUATION 0x00
#define WSOP_TEXT         0x01
#define WSOP_BINARY       0x02
#define WSOP_CLOSE        0x08
#define WSOP_PING         0x09
#define WSOP_PONG         0x0a

/* Forward declarations */
int websocket_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int websocket_config_run_ex(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
int websocket_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int websocket_packet_in(Client *client, char *readbuf, int *length);
void websocket_mdata_free(ModData *m);
int websocket_handle_packet(Client *client, char *readbuf, int length);
int websocket_handle_handshake(Client *client, char *readbuf, int *length);
int websocket_handshake_send_response(Client *client);
int websocket_handle_packet_ping(Client *client, char *buf, int len);
int websocket_handle_packet_pong(Client *client, char *buf, int len);
int websocket_create_packet(int opcode, char **buf, int *len);
int websocket_send_pong(Client *client, char *buf, int len);

/* Global variables */
ModDataInfo *websocket_md;
static int ws_text_mode_available = 1;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, websocket_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, websocket_config_run_ex);
	HookAdd(modinfo->handle, HOOKTYPE_PACKET, INT_MAX, websocket_packet_out);
	HookAdd(modinfo->handle, HOOKTYPE_RAWPACKET_IN, INT_MIN, websocket_packet_in);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "websocket";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = websocket_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_CLIENT;
	websocket_md = ModDataAdd(modinfo->handle, mreq);

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

#ifndef CheckNull
 #define CheckNull(x) if ((!(x)->ce_vardata) || (!(*((x)->ce_vardata)))) { config_error("%s:%i: missing parameter", (x)->ce_fileptr->cf_filename, (x)->ce_varlinenum); errors++; continue; }
#endif

int websocket_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_type = 0;
	static char errored_once_nick = 0;

	if (type != CONFIG_LISTEN_OPTIONS)
		return 0;

	/* We are only interrested in listen::options::websocket.. */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "websocket"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "type"))
		{
			CheckNull(cep);
			has_type = 1;
			if (!strcmp(cep->ce_vardata, "text"))
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
			else if (!strcmp(cep->ce_vardata, "binary"))
			{
			}
			else
			{
				config_error("%s:%i: listen::options::websocket::type must be either 'binary' or 'text' (not '%s')",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_vardata);
				errors++;
			}
		} else
		{
			config_error("%s:%i: unknown directive listen::options::websocket::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
	}

	if (!has_type)
	{
		config_error("%s:%i: websocket set, but type unspecified. Use something like: listen { ip *; port 443; websocket { type text; } }",
			ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
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
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "websocket"))
		return 0;

	l = (ConfigItem_listen *)ptr;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "type"))
		{
			if (!strcmp(cep->ce_vardata, "binary"))
				l->websocket_options = WEBSOCKET_TYPE_BINARY;
			else if (!strcmp(cep->ce_vardata, "text"))
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

/** UnrealIRCd internals: free WebSocketUser object. */
void websocket_mdata_free(ModData *m)
{
	WebSocketUser *wsu = (WebSocketUser *)m->ptr;
	if (wsu)
	{
		safe_free(wsu->handshake_key);
		safe_free(wsu->lefttoparse);
		safe_free(wsu->sec_websocket_protocol);
		safe_free(m->ptr);
	}
}

/** Outgoing packet hook.
 * This transforms the output to be Websocket-compliant, if necessary.
 */
int websocket_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	if (MyConnect(to) && WSU(to) && WSU(to)->handshake_completed)
	{
		if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_BINARY)
			websocket_create_packet(WSOP_BINARY, msg, length);
		else if (WEBSOCKET_TYPE(to) == WEBSOCKET_TYPE_TEXT)
		{
			/* Some more conversions are needed */
			char *safe_msg = unrl_utf8_make_valid(*msg);
			*msg = safe_msg;
			*length = *msg ? strlen(safe_msg) : 0;
			websocket_create_packet(WSOP_TEXT, msg, length);
		}
		return 0;
	}
	return 0;
}

int websocket_handle_websocket(Client *client, char *readbuf2, int length2)
{
	int n;
	char *ptr;
	int length;
	int length1 = WSU(client)->lefttoparselen;
	char readbuf[4096];

	length = length1 + length2;
	if (length > sizeof(readbuf)-1)
	{
		dead_socket(client, "Illegal buffer stacking/Excess flood");
		return 0;
	}

	if (length1 > 0)
		memcpy(readbuf, WSU(client)->lefttoparse, length1);
	memcpy(readbuf+length1, readbuf2, length2);

	safe_free(WSU(client)->lefttoparse);
	WSU(client)->lefttoparselen = 0;

	ptr = readbuf;
	do {
		n = websocket_handle_packet(client, ptr, length);
		if (n < 0)
			return -1; /* killed -- STOP processing */
		if (n == 0)
		{
			/* Short read. Stop processing for now, but save data for next time */
			safe_free(WSU(client)->lefttoparse);
			WSU(client)->lefttoparse = safe_alloc(length);
			WSU(client)->lefttoparselen = length;
			memcpy(WSU(client)->lefttoparse, ptr, length);
			return 0;
		}
		length -= n;
		ptr += n;
		if (length < 0)
			abort(); /* less than 0 is impossible */
	} while(length > 0);

	return 0;
}

/** Incoming packet hook.
 * This processes websocket frames, if this is a websocket connection.
 * NOTE The different return values:
 * -1 means: don't touch this client anymore, it has or might have been killed!
 * 0 means: don't process this data, but you can read another packet if you want
 * >0 means: process this data (regular IRC data, non-websocket stuff)
 */
int websocket_packet_in(Client *client, char *readbuf, int *length)
{
	if ((client->local->receiveM == 0) && WEBSOCKET_PORT(client) && !WSU(client) && (*length > 8) && !strncmp(readbuf, "GET ", 4))
	{
		/* Allocate a new WebSocketUser struct for this session */
		moddata_client(client, websocket_md).ptr = safe_alloc(sizeof(WebSocketUser));
		WSU(client)->get = 1;
		WSU(client)->type = client->local->listener->websocket_options; /* the default, unless the client chooses otherwise */
	}

	if (!WSU(client))
		return 1; /* "normal" IRC client */

	if (WSU(client)->handshake_completed)
		return websocket_handle_websocket(client, readbuf, *length);
	/* else.. */
	return websocket_handle_handshake(client, readbuf, length);
}

/** Helper function to parse the HTTP header consisting of multiple 'Key: value' pairs */
int websocket_handshake_helper(char *buffer, int len, char **key, char **value, char **lastloc, int *end_of_request)
{
	static char buf[4096], *nextptr;
	char *p;
	char *k = NULL, *v = NULL;
	int foundlf = 0;

	if (buffer)
	{
		/* Initialize */
		if (len > sizeof(buf) - 1)
			len = sizeof(buf) - 1;

		memcpy(buf, buffer, len);
		buf[len] = '\0';
		nextptr = buf;
	}

	*end_of_request = 0;

	p = nextptr;

	if (!p)
	{
		*key = *value = NULL;
		return 0; /* done processing data */
	}

	if (!strncmp(p, "\n", 1) || !strncmp(p, "\r\n", 2))
	{
		*key = *value = NULL;
		*end_of_request = 1;
		return 0;
	}

	/* Note: p *could* point to the NUL byte ('\0') */

	/* Special handling for GET line itself. */
	if (!strncmp(p, "GET ", 4))
	{
		k = "GET";
		p += 4;
		v = p; /* SET VALUE */
		nextptr = NULL; /* set to "we are done" in case next for loop fails */
		for (; *p; p++)
		{
			if (*p == ' ')
			{
				*p = '\0'; /* terminate before "HTTP/1.X" part */
			}
			else if (*p == '\r')
			{
				*p = '\0'; /* eat silently, but don't consider EOL */
			}
			else if (*p == '\n')
			{
				*p = '\0';
				nextptr = p+1; /* safe, there is data or at least a \0 there */
				break;
			}
		}
		*key = k;
		*value = v;
		return 1;
	}

	/* Header parsing starts here.
	 * Example line "Host: www.unrealircd.org"
	 */
	k = p; /* SET KEY */

	/* First check if the line contains a terminating \n. If not, don't process it
	 * as it may have been a cut header.
	 */
	for (; *p; p++)
	{
		if (*p == '\n')
		{
			foundlf = 1;
			break;
		}
	}

	if (!foundlf)
	{
		*key = *value = NULL;
		*lastloc = k;
		return 0;
	}

	p = k;

	for (; *p; p++)
	{
		if ((*p == '\n') || (*p == '\r'))
		{
			/* Reached EOL but 'value' not found */
			*p = '\0';
			break;
		}
		if (*p == ':')
		{
			*p++ = '\0';
			if (*p++ != ' ')
				break; /* missing mandatory space after ':' */

			v = p; /* SET VALUE */
			nextptr = NULL; /* set to "we are done" in case next for loop fails */
			for (; *p; p++)
			{
				if (*p == '\r')
				{
					*p = '\0'; /* eat silently, but don't consider EOL */
				}
				else if (*p == '\n')
				{
					*p = '\0';
					nextptr = p+1; /* safe, there is data or at least a \0 there */
					break;
				}
			}
			/* A key-value pair was succesfully parsed, return it */
			*key = k;
			*value = v;
			return 1;
		}
	}

	/* Fatal parse error */
	*key = *value = NULL;
	return 0;
}

/** Finally, validate the websocket request (handshake) and proceed or reject. */
int websocket_handshake_valid(Client *client)
{
	if (!WSU(client)->handshake_key)
	{
		if (is_module_loaded("webredir"))
		{
			char *parx[2] = { NULL, NULL };
			do_cmd(client, NULL, "GET", 1, parx);
		}
		dead_socket(client, "Invalid WebSocket request");
		return 0;
	}
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
	return 1;
}

/** Handle client GET WebSocket handshake.
 * Yes, I'm going to assume that the header fits in one packet and one packet only.
 */
int websocket_handle_handshake(Client *client, char *readbuf, int *length)
{
	char *key, *value;
	int r, end_of_request;
	char netbuf[2048];
	char *lastloc = NULL;
	int n, maxcopy, nprefix=0;

	/** Frame re-assembling starts here **/
	*netbuf = '\0';
	if (WSU(client)->lefttoparse)
	{
		strlcpy(netbuf, WSU(client)->lefttoparse, sizeof(netbuf));
		nprefix = strlen(netbuf);
	}
	maxcopy = sizeof(netbuf) - nprefix - 1;
	/* (Need to some manual checking here as strlen() can't be safely used
	 *  on readbuf. Same is true for strlncat since it uses strlen().)
	 */
	n = *length;
	if (n > maxcopy)
		n = maxcopy;
	if (n <= 0)
	{
		dead_socket(client, "Oversized line");
		return -1;
	}
	memcpy(netbuf+nprefix, readbuf, n); /* SAFE: see checking above */
	netbuf[n+nprefix] = '\0';
	safe_free(WSU(client)->lefttoparse);

	/** Now step through the lines.. **/
	for (r = websocket_handshake_helper(netbuf, strlen(netbuf), &key, &value, &lastloc, &end_of_request);
	     r;
	     r = websocket_handshake_helper(NULL, 0, &key, &value, &lastloc, &end_of_request))
	{
		if (!strcasecmp(key, "Sec-WebSocket-Key"))
		{
			if (strchr(value, ':'))
			{
				/* This would cause unserialization issues. Should be base64 anyway */
				dead_socket(client, "Invalid characters in Sec-WebSocket-Key");
				return -1;
			}
			safe_strdup(WSU(client)->handshake_key, value);
		} else
		if (!strcasecmp(key, "Sec-WebSocket-Protocol"))
		{
			/* Save it here, will be processed later */
			safe_strdup(WSU(client)->sec_websocket_protocol, value);
		}
	}

	if (end_of_request)
	{
		if (!websocket_handshake_valid(client))
			return -1;
		websocket_handshake_send_response(client);
		return 0;
	}

	if (lastloc)
	{
		/* Last line was cut somewhere, save it for next round. */
		safe_strdup(WSU(client)->lefttoparse, lastloc);
	}
	return 0; /* don't let UnrealIRCd process this */
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

/* Add LF (if needed) to a buffer. Max 4K. */
void add_lf_if_needed(char **buf, int *len)
{
	static char newbuf[4096];
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

/** WebSocket packet handler.
 * For more information on the format, check out page 28 of RFC6455.
 * @returns The number of bytes processed (the size of the frame)
 *          OR 0 to indicate a possible short read (want more data)
 *          OR -1 in case of an error.
 */
int websocket_handle_packet(Client *client, char *readbuf, int length)
{
	char opcode; /**< Opcode */
	char masked; /**< Masked */
	int len; /**< Length of the packet */
	char maskkey[4]; /**< Key used for masking */
	char *p, *payload;
	int total_packet_size;

	if (length < 4)
	{
		/* WebSocket packet too short */
		return 0;
	}

	/* fin    = readbuf[0] & 0x80; -- unused */
	opcode = readbuf[0] & 0x7F;
	masked = readbuf[1] & 0x80;
	len    = readbuf[1] & 0x7F;
	p = &readbuf[2]; /* point to next element */

	/* actually 'fin' is unused.. we don't care. */

	if (!masked)
	{
		dead_socket(client, "WebSocket packet not masked");
		return -1; /* Having the masked bit set is required (RFC6455 p29) */
	}

	if (len == 127)
	{
		dead_socket(client, "WebSocket packet with insane size");
		return -1; /* Packets requiring 64bit lengths are not supported. Would be insane. */
	}

	total_packet_size = len + 2 + 4; /* 2 for header, 4 for mask key, rest for payload */

	/* Early (minimal) length check */
	if (length < total_packet_size)
	{
		/* WebSocket frame too short */
		return 0;
	}

	/* Len=126 is special. It indicates the data length is actually "126 or more" */
	if (len == 126)
	{
		/* Extended payload length (16 bit). For packets of >=126 bytes */
		len = (readbuf[2] << 8) + readbuf[3];
		if (len < 126)
		{
			dead_socket(client, "WebSocket protocol violation (extended payload length too short)");
			return -1; /* This is a violation (not a short read), see page 29 */
		}
		p += 2; /* advance pointer 2 bytes */

		/* Need to check the length again, now it has changed: */
		if (length < len + 4 + 4)
		{
			/* WebSocket frame too short */
			return 0;
		}
		/* And update the packet size */
		total_packet_size = len + 4 + 4; /* 4 for header, 4 for mask key, rest for payload */
	}

	memcpy(maskkey, p, 4);
	p+= 4;
	payload = (len > 0) ? p : NULL;

	if (len > 0)
	{
		/* Unmask this thing (page 33, section 5.3) */
		int n;
		char v;
		for (n = 0; n < len; n++)
		{
			v = *p;
			*p++ = v ^ maskkey[n % 4];
		}
	}

	switch(opcode)
	{
		case WSOP_CONTINUATION:
		case WSOP_TEXT:
		case WSOP_BINARY:
			if (len > 0)
			{
				add_lf_if_needed(&payload, &len);
				if (!process_packet(client, payload, len, 1)) /* let UnrealIRCd process this data */
					return -1; /* fatal error occured (such as flood kill) */
			}
			return total_packet_size;

		case WSOP_CLOSE:
			dead_socket(client, "Connection closed"); /* TODO: Improve I guess */
			return -1;

		case WSOP_PING:
			if (websocket_handle_packet_ping(client, payload, len) < 0)
				return -1;
			return total_packet_size;

		case WSOP_PONG:
			if (websocket_handle_packet_pong(client, payload, len) < 0)
				return -1;
			return total_packet_size;

		default:
			dead_socket(client, "WebSocket: Unknown opcode");
			return -1;
	}

	return -1; /* NOTREACHED */
}

int websocket_handle_packet_ping(Client *client, char *buf, int len)
{
	if (len > 500)
	{
		dead_socket(client, "WebSocket: oversized PING request");
		return -1;
	}
	websocket_send_pong(client, buf, len);
	client->local->since++; /* lag penalty of 1 second */
	return 0;
}

int websocket_handle_packet_pong(Client *client, char *buf, int len)
{
	/* We don't care */
	return 0;
}

/** Create a simple websocket packet that is ready to be send.
 * This is the simple version that is used ONLY for WSOP_PONG,
 * as it does not take \r\n into account.
 */
int websocket_create_packet_simple(int opcode, char **buf, int *len)
{
	static char sendbuf[8192];

	sendbuf[0] = opcode | 0x80; /* opcode & final */

	if (*len > sizeof(sendbuf) - 8)
		return -1; /* should never happen (safety) */

	if (*len < 126)
	{
		/* Short payload */
		sendbuf[1] = (char)*len;
		memcpy(&sendbuf[2], *buf, *len);
		*buf = sendbuf;
		*len += 2;
	} else {
		/* Long payload */
		sendbuf[1] = 126;
		sendbuf[2] = (char)((*len >> 8) & 0xFF);
		sendbuf[3] = (char)(*len & 0xFF);
		memcpy(&sendbuf[4], *buf, *len);
		*buf = sendbuf;
		*len += 4;
	}
	return 0;
}

/** Create a websocket packet that is ready to be send.
 * This is the more complex version that takes into account
 * stripping off \r and \n, and possibly multi line due to
 * labeled-response. It is used for WSOP_TEXT and WSOP_BINARY.
 * The end result is one or more websocket frames,
 * all in a single packet *buf with size *len.
 */
int websocket_create_packet(int opcode, char **buf, int *len)
{
	static char sendbuf[WEBSOCKET_SEND_BUFFER_SIZE];
	char *s = *buf; /* points to start of current line */
	char *s2; /* used for searching of end of current line */
	char *lastbyte = *buf + *len - 1; /* points to last byte in *buf that can be safely read */
	int bytes_to_copy;
	char newline;
	char *o = sendbuf; /* points to current byte within 'sendbuf' of output buffer */
	int bytes_in_sendbuf = 0;
	int bytes_single_frame;

	/* Sending 0 bytes makes no sense, and the code below may assume >0, so reject this. */
	if (*len == 0)
		return -1;

	do {
		/* Find next \r or \n */
		for (s2 = s; *s2 && (s2 <= lastbyte); s2++)
		{
			if ((*s2 == '\n') || (*s2 == '\r'))
				break;
		}

		/* Now 's' points to start of line and 's2' points to beyond end of the line
		 * (either at \r, \n or beyond the buffer).
		 */
		bytes_to_copy = s2 - s;

		if (bytes_to_copy < 126)
			bytes_single_frame = 2 + bytes_to_copy;
		else
			bytes_single_frame = 4 + bytes_to_copy;

		if (bytes_in_sendbuf + bytes_single_frame > sizeof(sendbuf))
		{
			/* Overflow. This should never happen. */
			sendto_ops("[websocket] [BUG] Overflow prevented: %d + %d > %d",
				bytes_in_sendbuf, bytes_single_frame, (int)sizeof(sendbuf));
			return -1;
		}

		/* Create the new frame */
		o[0] = opcode | 0x80; /* opcode & final */

		if (bytes_to_copy < 126)
		{
			/* Short payload */
			o[1] = (char)bytes_to_copy;
			memcpy(&o[2], s, bytes_to_copy);
		} else {
			/* Long payload */
			o[1] = 126;
			o[2] = (char)((bytes_to_copy >> 8) & 0xFF);
			o[3] = (char)(bytes_to_copy & 0xFF);
			memcpy(&o[4], s, bytes_to_copy);
		}

		/* Advance destination pointer and counter */
		o += bytes_single_frame;
		bytes_in_sendbuf += bytes_single_frame;

		/* Advance source pointer and skip all trailing \n and \r */
		for (s = s2; *s && (s <= lastbyte) && ((*s == '\n') || (*s == '\r')); s++);
	} while(s <= lastbyte);

	*buf = sendbuf;
	*len = bytes_in_sendbuf;
	return 0;
}

/** Create and send a WSOP_PONG frame */
int websocket_send_pong(Client *client, char *buf, int len)
{
	char *b = buf;
	int l = len;

	if (websocket_create_packet_simple(WSOP_PONG, &b, &l) < 0)
		return -1;

	if (DBufLength(&client->local->sendQ) > get_sendq(client))
	{
		dead_socket(client, "Max SendQ exceeded");
		return -1;
	}

	dbuf_put(&client->local->sendQ, b, l);
	send_queued(client);
	return 0;
}
