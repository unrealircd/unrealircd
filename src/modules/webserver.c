/*
 * Webserver
 * (C)Copyright 2016 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */
   
#include "unrealircd.h"
#include "dns.h"

ModuleHeader MOD_HEADER
  = {
	"webserver",
	"1.0.0",
	"Webserver",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

#if CHAR_MIN < 0
 #error "In UnrealIRCd char should always be unsigned. Check your compiler"
#endif

/* How many seconds to wait with closing after sending the response */
#define WEB_CLOSE_TIME 1

/* The "Server: xyz" in the response */
#define WEB_SOFTWARE "UnrealIRCd"

/* Macros */
#define WEB(client)		((WebRequest *)moddata_client(client, webserver_md).ptr)
#define WEBSERVER(client)	((client->local && client->local->listener) ? client->local->listener->webserver : NULL)
#define reset_handshake_timeout(client, delta)  do { client->local->creationtime = TStime() - iConf.handshake_timeout + delta; } while(0)

/* Forward declarations */
int webserver_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length);
int webserver_packet_in(Client *client, const char *readbuf, int *length);
void webserver_mdata_free(ModData *m);
int webserver_handle_packet(Client *client, const char *readbuf, int length);
int webserver_handle_handshake(Client *client, const char *readbuf, int *length);
int webserver_handle_request_header(Client *client, const char *readbuf, int *length);
void _webserver_send_response(Client *client, int status, char *msg);
void _webserver_close_client(Client *client);
int _webserver_handle_body(Client *client, WebRequest *web, const char *readbuf, int length);

/* Global variables */
ModDataInfo *webserver_md;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_WEBSERVER_SEND_RESPONSE, _webserver_send_response);
	EfunctionAddVoid(modinfo->handle, EFUNC_WEBSERVER_CLOSE_CLIENT, _webserver_close_client);
	EfunctionAdd(modinfo->handle, EFUNC_WEBSERVER_HANDLE_BODY, _webserver_handle_body);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	//HookAdd(modinfo->handle, HOOKTYPE_PACKET, INT_MAX, webserver_packet_out);
	HookAdd(modinfo->handle, HOOKTYPE_RAWPACKET_IN, INT_MIN, webserver_packet_in);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "web";
	mreq.serialize = NULL;
	mreq.unserialize = NULL;
	mreq.free = webserver_mdata_free;
	mreq.sync = 0;
	mreq.type = MODDATATYPE_CLIENT;
	webserver_md = ModDataAdd(modinfo->handle, mreq);

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

/** UnrealIRCd internals: free WebRequest object. */
void webserver_mdata_free(ModData *m)
{
	WebRequest *wsu = (WebRequest *)m->ptr;
	if (wsu)
	{
		safe_free(wsu->uri);
		free_nvplist(wsu->headers);
		safe_free(wsu->lefttoparse);
		safe_free(wsu->request_buffer);
		safe_free(m->ptr);
	}
}

/** Outgoing packet hook.
 * Do we need this?
 */
int webserver_packet_out(Client *from, Client *to, Client *intended_to, char **msg, int *length)
{
	static char utf8buf[510];

	if (MyConnect(to) && WEB(to))
	{
		// TODO: Inhibit all?
		// Websocket can override though?
		return 0;
	}
	return 0;
}

HttpMethod webserver_get_method(const char *buf)
{
	if (!strncmp(buf, "HEAD ", 5))
		return HTTP_METHOD_HEAD;
	if (!strncmp(buf, "GET ", 4))
		return HTTP_METHOD_GET;
	if (!strncmp(buf, "PUT ", 4))
		return HTTP_METHOD_PUT;
	if (!strncmp(buf, "POST ", 5))
		return HTTP_METHOD_POST;
	return HTTP_METHOD_NONE; /* invalid */
}

void webserver_possible_request(Client *client, const char *buf, int len)
{
	HttpMethod method;

	if (len < 8)
		return;

	/* Probably redundant, but just to be sure, if already tagged, then don't change it! */
	if (WEB(client))
		return;

	method = webserver_get_method(buf);
	if (method == HTTP_METHOD_NONE)
		return; /* invalid */

	moddata_client(client, webserver_md).ptr = safe_alloc(sizeof(WebRequest));
	WEB(client)->method = method;

	/* Set some default values: */
	WEB(client)->content_length = -1;
	WEB(client)->config_max_request_buffer_size = 4096; /* 4k */
}

/** Incoming packet hook. This processes web requests.
 * NOTE The different return values:
 * -1 means: don't touch this client anymore, it has or might have been killed!
 * 0 means: don't process this data, but you can read another packet if you want
 * >0 means: process this data (regular IRC data, non-web stuff)
 */
int webserver_packet_in(Client *client, const char *readbuf, int *length)
{
	if ((client->local->traffic.messages_received == 0) && WEBSERVER(client))
		webserver_possible_request(client, readbuf, *length);

	if (!WEB(client))
		return 1; /* "normal" IRC client */

	if (WEB(client)->request_header_parsed)
		return WEBSERVER(client)->handle_body(client, WEB(client), readbuf, *length);

	/* else.. */
	return webserver_handle_request_header(client, readbuf, length);
}

/** Helper function to parse the HTTP header consisting of multiple 'Key: value' pairs */
int webserver_handshake_helper(char *buffer, int len, char **key, char **value, char **lastloc, int *end_of_request)
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
	if (webserver_get_method(p) != HTTP_METHOD_NONE)
	{
		k = "REQUEST";
		p = strchr(p, ' ') + 1; /* space (0x20) is guaranteed to be there, see strncmp above */
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

/** Check if there is any data at the end of the request */
char *find_end_of_request(char *header, int totalsize, int *remaining_bytes)
{
	char *nextframe1;
	char *nextframe2;
	char *nextframe = NULL;

	// find first occurance, yeah this is just stupid, but it works.
	nextframe1 = strstr(header, "\r\n\r\n"); // = +4
	nextframe2 = strstr(header, "\n\n");     // = +2
	if (nextframe1 && nextframe2)
	{
		if (nextframe1 < nextframe2)
		{
			nextframe = nextframe1 + 4;
		} else {
			nextframe = nextframe2 + 2;
		}
	} else
	if (nextframe1)
	{
		nextframe = nextframe1 + 4;
	} else
	if (nextframe2)
	{
		nextframe = nextframe2 + 2;
	}
	if (nextframe)
	{
		*remaining_bytes = totalsize - (nextframe - header);
		if (*remaining_bytes > 0)
			return nextframe;
	}
	return NULL;
}

/** Handle HTTP request
 * Yes, I'm going to assume that the header fits in one packet and one packet only.
 */
int webserver_handle_request_header(Client *client, const char *readbuf, int *length)
{
	char *key, *value;
	int r, end_of_request;
	static char netbuf[16384];
	static char netbuf2[16384];
	char *lastloc = NULL;
	int n, maxcopy, nprefix=0;
	int totalsize;

	/* Totally paranoid: */
	memset(netbuf, 0, sizeof(netbuf));
	memset(netbuf2, 0, sizeof(netbuf2));

	/** Frame re-assembling starts here **/
	if (WEB(client)->lefttoparse)
	{
		strlcpy(netbuf, WEB(client)->lefttoparse, sizeof(netbuf));
		nprefix = strlen(netbuf);
	}
	maxcopy = sizeof(netbuf) - nprefix - 1;
	/* (Need to do some manual checking here as strlen() can't be safely used
	 *  on readbuf. Same is true for strlncat since it uses strlen().)
	 */
	n = *length;
	if (n > maxcopy)
		n = maxcopy;
	if (n <= 0)
	{
		webserver_close_client(client); // Oversized line
		return -1;
	}
	memcpy(netbuf+nprefix, readbuf, n); /* SAFE: see checking above */
	totalsize = n + nprefix;
	netbuf[totalsize] = '\0';
	memcpy(netbuf2, netbuf, totalsize+1); // copy, including the "always present \0 at the end just in case we use strstr etc".
	safe_free(WEB(client)->lefttoparse);

	/** Now step through the lines.. **/
	for (r = webserver_handshake_helper(netbuf, strlen(netbuf), &key, &value, &lastloc, &end_of_request);
	     r;
	     r = webserver_handshake_helper(NULL, 0, &key, &value, &lastloc, &end_of_request))
	{
		if (BadPtr(value))
			continue; /* skip empty values */

		if (!strcasecmp(key, "REQUEST"))
		{
			safe_strdup(WEB(client)->uri, value);
		} else
		{
			if (!strcasecmp(key, "Content-Length"))
			{
				WEB(client)->content_length = atoll(value);
			} else
			if (!strcasecmp(key, "Transfer-Encoding"))
			{
				if (!strcasecmp(value, "chunked"))
					WEB(client)->transfer_encoding = TRANSFER_ENCODING_CHUNKED;
			}
			add_nvplist(&WEB(client)->headers, WEB(client)->num_headers, key, value);
		}
	}

	if (end_of_request)
	{
		int n;
		int remaining_bytes = 0;
		char *nextframe;

		WEB(client)->request_header_parsed = 1;
		n = WEBSERVER(client)->handle_request(client, WEB(client));
		if ((n <= 0) || IsDead(client))
			return n; /* byebye */
		
		/* There could be data directly after the request header (eg for
		 * a POST or PUT), check for it here so it isn't lost.
		 */
		nextframe = find_end_of_request(netbuf2, totalsize, &remaining_bytes);
		if (nextframe)
			return WEBSERVER(client)->handle_body(client, WEB(client), nextframe, remaining_bytes);
		return 0;
	}

	if (lastloc)
	{
		/* Last line was cut somewhere, save it for next round. */
		safe_strdup(WEB(client)->lefttoparse, lastloc);
	}
	return 0; /* don't let UnrealIRCd process this */
}

/** Send a HTTP(S) response.
 * @param client	Client to send to
 * @param status	HTTP status code
 * @param msg		The message body.
 * @note if 'msgs' is NULL then don't close the connection.
 */
void _webserver_send_response(Client *client, int status, char *msg)
{
	char buf[512];
	char *statusmsg = "???";

	if (status == 200)
		statusmsg = "OK";
	else if (status == 201)
		statusmsg = "Created";
	else if (status == 500)
		statusmsg = "Internal Server Error";
	else if (status == 400)
		statusmsg = "Bad Request";
	else if (status == 401)
		statusmsg = "Unauthorized";
	else if (status == 403)
		statusmsg = "Forbidden";
	else if (status == 404)
		statusmsg = "Not Found";
	else if (status == 416)
		statusmsg = "Range Not Satisfiable";

	snprintf(buf, sizeof(buf),
		"HTTP/1.1 %d %s\r\nServer: %s\r\nConnection: close\r\n\r\n",
		status, statusmsg, WEB_SOFTWARE);
	if (msg)
	{
		strlcat(buf, msg, sizeof(buf));
		strlcat(buf, "\n", sizeof(buf));
	}

	dbuf_put(&client->local->sendQ, buf, strlen(buf));
	if (msg)
		webserver_close_client(client);
}

/** Close a web client softly, after data has been sent. */
void _webserver_close_client(Client *client)
{
	send_queued(client);
	if (DBufLength(&client->local->sendQ) == 0)
	{
		exit_client(client, NULL, "End of request");
		//dead_socket(client, "");
	} else {
		send_queued(client);
		reset_handshake_timeout(client, WEB_CLOSE_TIME);
	}
}

int webserver_handle_body_append_buffer(Client *client, const char *buf, int len)
{
	/* Guard.. */
	if (len <= 0)
	{
		dead_socket(client, "HTTP request error");
		return 0;
	}

	if (WEB(client)->request_buffer)
	{
		long long newsize = WEB(client)->request_buffer_size + len + 1;
		if (newsize > WEB(client)->config_max_request_buffer_size)
		{
			/* We would overflow */
			unreal_log(ULOG_WARNING, "webserver", "HTTP_BODY_TOO_LARGE", client,
			           "[webserver] Client $client: request body too large ($length)",
			           log_data_integer("length", newsize));
			dead_socket(client, "");
			return 0;
		}
		WEB(client)->request_buffer = realloc(WEB(client)->request_buffer, newsize);
	} else
	{
		if (len + 1 > WEB(client)->config_max_request_buffer_size)
		{
			/* We would overflow */
			unreal_log(ULOG_WARNING, "webserver", "HTTP_BODY_TOO_LARGE", client,
			           "[webserver] Client $client: request body too large ($length)",
			           log_data_integer("length", len+1));
			dead_socket(client, "");
			return 0;
		}
		WEB(client)->request_buffer = malloc(len+1);
	}
	memcpy(WEB(client)->request_buffer + WEB(client)->request_buffer_size, buf, len);
	WEB(client)->request_buffer_size += len;
	WEB(client)->request_buffer[WEB(client)->request_buffer_size] = '\0';
	return 1;
}

/** Handle HTTP body parsing, eg for a PUT request, concatting it all together.
 * @param client	The client
 * @param web		The WEB(client)
 * @param readbuf	Packet in the read buffer
 * @param pktsize	Packet size of the read buffer
 * @return 1 to continue processing, 0 if client is killed.
 */
int _webserver_handle_body(Client *client, WebRequest *web, const char *readbuf, int pktsize)
{
	char *buf;
	long long n;
	char *free_this_buffer = NULL;

	if (WEB(client)->transfer_encoding == TRANSFER_ENCODING_NONE)
	{
		if (!webserver_handle_body_append_buffer(client, readbuf, pktsize))
			return 0;

		if ((WEB(client)->content_length >= 0) &&
		    (WEB(client)->request_buffer_size >= WEB(client)->content_length))
		{
			WEB(client)->request_body_complete = 1;
		}
		return 1;
	}

	/* Fill 'buf' nd set 'buflen' with what we had + what we have now.
	 * Makes things easy.
	 */
	if (WEB(client)->lefttoparse)
	{
		n = WEB(client)->lefttoparselen + pktsize;
		free_this_buffer = buf = safe_alloc(n);
		memcpy(buf, WEB(client)->lefttoparse, WEB(client)->lefttoparselen);
		memcpy(buf+WEB(client)->lefttoparselen, readbuf, pktsize);
		safe_free(WEB(client)->lefttoparse);
		WEB(client)->lefttoparselen = 0;
	} else {
		n = pktsize;
		free_this_buffer = buf = safe_alloc(n);
		memcpy(buf, readbuf, n);
	}

	/* Chunked transfers.. yayyyy.. */
	while (n > 0)
	{
		if (WEB(client)->chunk_remaining > 0)
		{
			/* Eat it */
			int eat = MIN(WEB(client)->chunk_remaining, n);
			if (!webserver_handle_body_append_buffer(client, buf, eat))
			{
				/* fatal error such as size exceeded */
				safe_free(free_this_buffer);
				return 0;
			}
			n -= eat;
			buf += eat;
			WEB(client)->chunk_remaining -= eat;
		} else
		{
			int gotlf = 0;
			int i;

			/* First check if it is a (trailing) empty line,
			 * eg from a previous chunk. Skip over.
			 */
			if ((n >= 2) && !strncmp(buf, "\r\n", 2))
			{
				buf += 2;
				n -= 2;
			} else
			if ((n >= 1) && !strncmp(buf, "\n", 1))
			{
				buf++;
				n--;
			}

			/* Now we are (possibly) at the chunk size line,
			 * this is or example '7f' + newline.
			 * So first, check if we have a newline at all.
			 */
			for (i=0; i < n; i++)
			{
				if (buf[i] == '\n')
				{
					gotlf = 1;
					break;
				}
			}
			if (!gotlf)
			{
				/* The line telling us the chunk size is incomplete,
				 * as it does not contain an \n. Wait for more data
				 * from the network socket.
				 */
				if (n > 0)
				{
					/* Store what we have first.. */
					WEB(client)->lefttoparselen = n;
					WEB(client)->lefttoparse = safe_alloc(n);
					memcpy(WEB(client)->lefttoparse, buf, n);
				}
				safe_free(free_this_buffer);
				return 1; /* WE WANT MORE! */
			}
			buf[i] = '\0'; /* cut at LF */
			i++; /* point to next data */
			WEB(client)->chunk_remaining = strtol(buf, NULL, 16);
			if (WEB(client)->chunk_remaining < 0)
			{
				unreal_log(ULOG_WARNING, "webserver", "WEB_NEGATIVE_CHUNK", client,
				           "Webrequest from $client: Negative chunk encountered");
				safe_free(free_this_buffer);
				dead_socket(client, "");
				return 0;
			}
			if (WEB(client)->chunk_remaining == 0)
			{
				/* DONE! */
				WEB(client)->request_body_complete = 1;
				safe_free(free_this_buffer);
				return 1;
			}
			buf += i;
			n -= i;
		}
	}

	safe_free(free_this_buffer);
	return 1;
}
