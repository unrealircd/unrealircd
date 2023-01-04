/*
 * websocket_common - Common WebSocket functions (RFC6455)
 * (C)Copyright 2016 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 * The websocket module was sponsored by Aberrant Software Inc.
 */
   
#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"websocket_common",
	"6.0.0",
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

/* used to parse http Forwarded header (RFC 7239) */
#define IPLEN 48
#define FHEADER_NAMELEN	20

struct HTTPForwardedHeader
{
	int secure;
	char hostname[HOSTLEN+1];
	char ip[IPLEN+1];
};

/* Forward declarations - public functions */
int _websocket_handle_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2, int callback(Client *client, char *buf, int len));
int _websocket_create_packet(int opcode, char **buf, int *len);
int _websocket_create_packet_ex(int opcode, char **buf, int *len, char *sendbuf, size_t sendbufsize);
int _websocket_create_packet_simple(int opcode, const char **buf, int *len);
/* Forward declarations - other */
int websocket_handle_packet(Client *client, const char *readbuf, int length, int callback(Client *client, char *buf, int len));
int websocket_handle_packet_ping(Client *client, const char *buf, int len);
int websocket_handle_packet_pong(Client *client, const char *buf, int len);
int websocket_send_pong(Client *client, const char *buf, int len);
void websocket_mdata_free(ModData *m);

/* Global variables */
ModDataInfo *websocket_md;
static int ws_text_mode_available = 1;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_WEBSOCKET_HANDLE_WEBSOCKET, _websocket_handle_websocket);
	EfunctionAdd(modinfo->handle, EFUNC_WEBSOCKET_CREATE_PACKET, _websocket_create_packet);
	EfunctionAdd(modinfo->handle, EFUNC_WEBSOCKET_CREATE_PACKET_EX, _websocket_create_packet_ex);
	EfunctionAdd(modinfo->handle, EFUNC_WEBSOCKET_CREATE_PACKET_SIMPLE, _websocket_create_packet_simple);

	/* Init first, since we manage sockets */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_INIT);

	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "websocket";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = websocket_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_CLIENT;
	websocket_md = ModDataAdd(modinfo->handle, mreq);

	/* Unload last, since we manage sockets */
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, WEBSOCKET_MODULE_PRIORITY_UNLOAD);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int _websocket_handle_websocket(Client *client, WebRequest *web, const char *readbuf2, int length2, int callback(Client *client, char *buf, int len))
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
		n = websocket_handle_packet(client, ptr, length, callback);
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

/** WebSocket packet handler.
 * For more information on the format, check out page 28 of RFC6455.
 * @returns The number of bytes processed (the size of the frame)
 *          OR 0 to indicate a possible short read (want more data)
 *          OR -1 in case of an error.
 */
int websocket_handle_packet(Client *client, const char *readbuf, int length, int callback(Client *client, char *buf, int len))
{
	char opcode; /**< Opcode */
	char masked; /**< Masked */
	int len; /**< Length of the packet */
	char maskkey[4]; /**< Key used for masking */
	const char *p;
	int total_packet_size;
	char *payload = NULL;
	static char payloadbuf[READBUF_SIZE];

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

	if (len > 0)
	{
		memcpy(payloadbuf, p, len);
		payload = payloadbuf;
	} /* else payload is NULL */

	if (len > 0)
	{
		/* Unmask this thing (page 33, section 5.3) */
		int n;
		char v;
		char *p;
		for (p = payload, n = 0; n < len; n++)
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
				if (!callback(client, payload, len))
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

int websocket_handle_packet_ping(Client *client, const char *buf, int len)
{
	if (len > 500)
	{
		dead_socket(client, "WebSocket: oversized PING request");
		return -1;
	}
	websocket_send_pong(client, buf, len);
	add_fake_lag(client, 1000); /* lag penalty of 1 second */
	return 0;
}

int websocket_handle_packet_pong(Client *client, const char *buf, int len)
{
	/* We only care about pongs for RPC websocket connections.
	 * Also, we don't verify the content, actually,
	 * so don't use this for security like a pingpong cookie.
	 */
	if (IsRPC(client))
	{
		client->local->last_msg_received = TStime();
		ClearPingSent(client);
	}
	return 0;
}

/** Create a simple websocket packet that is ready to be sent.
 * This is the simple version that is used ONLY for WSOP_PONG,
 * as it does not take \r\n into account.
 */
int _websocket_create_packet_simple(int opcode, const char **buf, int *len)
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
 * This version takes into account stripping off \r and \n,
 * and possibly multi line due to labeled-response.
 * It is used for WSOP_TEXT and WSOP_BINARY.
 * The end result is one or more websocket frames,
 * all in a single packet *buf with size *len.
 *
 * This is the version that uses the specified buffer,
 * it is used from the JSON-RPC code,
 * and indirectly from websocket_create_packet().
 */
int _websocket_create_packet_ex(int opcode, char **buf, int *len, char *sendbuf, size_t sendbufsize)
{
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
		else if (bytes_to_copy < 65536)
			bytes_single_frame = 4 + bytes_to_copy;
		else
			bytes_single_frame = 10 + bytes_to_copy;

		if (bytes_in_sendbuf + bytes_single_frame > sendbufsize)
		{
			/* Overflow. This should never happen. */
			unreal_log(ULOG_WARNING, "websocket", "BUG_WEBSOCKET_OVERFLOW", NULL,
			           "[BUG] [websocket] Overflow prevented in _websocket_create_packet(): "
			           "$bytes_in_sendbuf + $bytes_single_frame > $sendbuf_size",
			           log_data_integer("bytes_in_sendbuf", bytes_in_sendbuf),
			           log_data_integer("bytes_single_frame", bytes_single_frame),
			           log_data_integer("sendbuf_size", sendbufsize));
			return -1;
		}

		/* Create the new frame */
		o[0] = opcode | 0x80; /* opcode & final */

		if (bytes_to_copy < 126)
		{
			/* Short payload */
			o[1] = (char)bytes_to_copy;
			memcpy(&o[2], s, bytes_to_copy);
		} else
		if (bytes_to_copy < 65536)
		{
			/* Long payload */
			o[1] = 126;
			o[2] = (char)((bytes_to_copy >> 8) & 0xFF);
			o[3] = (char)(bytes_to_copy & 0xFF);
			memcpy(&o[4], s, bytes_to_copy);
		} else {
			/* Longest payload */
			// XXX: yeah we don't support sending more than 4GB.
			o[1] = 127;
			o[2] = 0;
			o[3] = 0;
			o[4] = 0;
			o[5] = 0;
			o[6] = (char)((bytes_to_copy >> 24) & 0xFF);
			o[7] = (char)((bytes_to_copy >> 16) & 0xFF);
			o[8] = (char)((bytes_to_copy >> 8) & 0xFF);
			o[9] = (char)(bytes_to_copy & 0xFF);
			memcpy(&o[10], s, bytes_to_copy);
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

/** Create a websocket packet that is ready to be send.
 * This version takes into account stripping off \r and \n,
 * and possibly multi line due to labeled-response.
 * It is used for WSOP_TEXT and WSOP_BINARY.
 * The end result is one or more websocket frames,
 * all in a single packet *buf with size *len.
 *
 * This is the version that uses a static sendbuf buffer,
 * it is used from IRC websockets.
 */
int _websocket_create_packet(int opcode, char **buf, int *len)
{
	static char sendbuf[WEBSOCKET_SEND_BUFFER_SIZE];
	return _websocket_create_packet_ex(opcode, buf, len, sendbuf, sizeof(sendbuf));
}

/** Create and send a WSOP_PONG frame */
int websocket_send_pong(Client *client, const char *buf, int len)
{
	const char *b = buf;
	int l = len;

	if (_websocket_create_packet_simple(WSOP_PONG, &b, &l) < 0)
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

/** UnrealIRCd internals: free WebSocketUser object. */
void websocket_mdata_free(ModData *m)
{
	WebSocketUser *wsu = (WebSocketUser *)m->ptr;
	if (wsu)
	{
		safe_free(wsu->handshake_key);
		safe_free(wsu->lefttoparse);
		safe_free(wsu->sec_websocket_protocol);
		safe_free(wsu->forwarded);
		safe_free(m->ptr);
	}
}
