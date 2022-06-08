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

/* Global variables */
ModDataInfo *webserver_md;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_WEBSERVER_SEND_RESPONSE, _webserver_send_response);
	EfunctionAddVoid(modinfo->handle, EFUNC_WEBSERVER_CLOSE_CLIENT, _webserver_close_client);
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
		safe_free(wsu->lefttoparse);
		free_nvplist(wsu->headers);
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

void webserver_possible_request(Client *client, const char *buf, int len)
{
	if (len < 8)
		return;

	/* Probably redundant, but just to be sure, if already tagged, then don't change it! */
	if (WEB(client))
		return;

	if (!strncmp(buf, "HEAD ", 5))
	{
		moddata_client(client, webserver_md).ptr = safe_alloc(sizeof(WebRequest));
		WEB(client)->method = HTTP_METHOD_HEAD;
	} else
	if (!strncmp(buf, "GET ", 4))
	{
		moddata_client(client, webserver_md).ptr = safe_alloc(sizeof(WebRequest));
		WEB(client)->method = HTTP_METHOD_GET;
	} else
	if (!strncmp(buf, "PUT ", 4))
	{
		moddata_client(client, webserver_md).ptr = safe_alloc(sizeof(WebRequest));
		WEB(client)->method = HTTP_METHOD_PUT;
	} else
	if (!strncmp(buf, "POST ", 5))
	{
		moddata_client(client, webserver_md).ptr = safe_alloc(sizeof(WebRequest));
		WEB(client)->method = HTTP_METHOD_POST;
	}
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
		return WEBSERVER(client)->handle_data(client, WEB(client), readbuf, *length);

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
	if (!strncmp(p, "HEAD ", 5) ||!strncmp(p, "GET ", 4) || !strncmp(p, "PUT ", 4) || !strncmp(p, "POST ", 5))
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
		if (!strcasecmp(key, "REQUEST"))
		{
			// TODO: guard for a 16k URI ? ;D
			safe_strdup(WEB(client)->uri, value);
		} else
		{
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
			return WEBSERVER(client)->handle_data(client, WEB(client), nextframe, remaining_bytes);
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
	else if (status == 400)
		statusmsg = "Bad Request";
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
