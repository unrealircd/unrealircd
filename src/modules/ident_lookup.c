/* src/modules/ident_lookup.c - Ident lookups (RFC1413)
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"ident_lookup",
	"1.0",
	"Ident lookups (RFC1413)",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
static EVENT(check_ident_timeout);
static int ident_lookup_connect(Client *client);
static void ident_lookup_send(int fd, int revents, void *data);
static void ident_lookup_receive(int fd, int revents, void *data);
static char *ident_lookup_parse(Client *client, char *buf);
void _cancel_ident_lookup(Client *client);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAddVoid(modinfo->handle, EFUNC_CANCEL_IDENT_LOOKUP, _cancel_ident_lookup);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1); /* needed? or not? */
	EventAdd(NULL, "check_ident_timeout", check_ident_timeout, NULL, 1000, 0);
	HookAdd(modinfo->handle, HOOKTYPE_IDENT_LOOKUP, 0, ident_lookup_connect);

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


static void ident_lookup_failed(Client *client)
{
	ircstats.is_abad++;
	if (client->local->authfd != -1)
	{
		fd_close(client->local->authfd);
		--OpenFiles;
		client->local->authfd = -1;
	}
	ClearIdentLookupSent(client);
	ClearIdentLookup(client);
	if (should_show_connect_info(client))
		sendto_one(client, NULL, ":%s %s", me.name, REPORT_FAIL_ID);
}

static EVENT(check_ident_timeout)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		if (IsIdentLookup(client))
		{
			if (IsIdentLookupSent(client))
			{
				/* set::ident::connect-timeout */
				if ((TStime() - client->local->creationtime) > IDENT_CONNECT_TIMEOUT)
					ident_lookup_failed(client);
			} else
			{
				/* set::ident::read-timeout */
				if ((TStime() - client->local->creationtime) > IDENT_READ_TIMEOUT)
					ident_lookup_failed(client);
			}
		}
	}
}

/** Start the ident lookup for this user */
static int ident_lookup_connect(Client *client)
{
	char buf[BUFSIZE];

	snprintf(buf, sizeof buf, "identd: %s", get_client_name(client, TRUE));
	if ((client->local->authfd = fd_socket(IsIPV6(client) ? AF_INET6 : AF_INET, SOCK_STREAM, 0, buf)) == -1)
	{
		ident_lookup_failed(client);
		return 0;
	}
	if (++OpenFiles >= maxclients+1)
	{
		unreal_log(ULOG_FATAL, "io", "IDENT_ERROR_MAXCLIENTS", client,
		           "Cannot do ident connection for $client.details: All connections in use");
		fd_close(client->local->authfd);
		--OpenFiles;
		client->local->authfd = -1;
		return 0;
	}

	if (should_show_connect_info(client))
		sendto_one(client, NULL, ":%s %s", me.name, REPORT_DO_ID);

	set_sock_opts(client->local->authfd, client, IsIPV6(client));

	/* Bind to the IP the user got in */
	unreal_bind(client->local->authfd, client->local->listener->ip, 0, IsIPV6(client));

	/* And connect... */
	if (!unreal_connect(client->local->authfd, client->ip, 113, IsIPV6(client)))
	{
		ident_lookup_failed(client);
		return 0;
	}
	SetIdentLookupSent(client);
	SetIdentLookup(client);

	fd_setselect(client->local->authfd, FD_SELECT_WRITE, ident_lookup_send, client);

	return 0;
}

/** Send the request to the ident server */
static void ident_lookup_send(int fd, int revents, void *data)
{
	char authbuf[32];
	Client *client = data;

	ircsnprintf(authbuf, sizeof(authbuf), "%d , %d\r\n",
		client->local->port,
		client->local->listener->port);

	if (WRITE_SOCK(client->local->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
	{
		if (ERRNO == P_EAGAIN)
			return; /* Not connected yet, try again later */
		ident_lookup_failed(client);
		return;
	}
	ClearIdentLookupSent(client);

	fd_setselect(client->local->authfd, FD_SELECT_READ, ident_lookup_receive, client);
	fd_setselect(client->local->authfd, FD_SELECT_WRITE, NULL, client);

	return;
}

/** Receive the ident response */
static void ident_lookup_receive(int fd, int revents, void *userdata)
{
	Client *client = userdata;
	char *ident = NULL;
	char buf[512];
	int len;

	len = READ_SOCK(client->local->authfd, buf, sizeof(buf)-1);

	/* We received a response. We don't bother with fragmentation
	 * since that is not going to happen for such a short string.
	 * Other IRCd's think the same and this simplifies things a lot.
	 */

	/* Before we continue, we can already tear down the connection
	 * and set the appropriate flags that we are finished.
	 */
	fd_close(client->local->authfd);
	--OpenFiles;
	client->local->authfd = -1;
	client->local->identbufcnt = 0;
	ClearIdentLookup(client);

	if (should_show_connect_info(client))
		sendto_one(client, NULL, ":%s %s", me.name, REPORT_FIN_ID);

	if (len > 0)
	{
		buf[len] = '\0'; /* safe, due to the READ_SOCK() being on sizeof(buf)-1 */
		ident = ident_lookup_parse(client, buf);
	}
	if (ident)
	{
		strlcpy(client->ident, ident, USERLEN + 1);
		SetIdentSuccess(client);
		ircstats.is_asuc++;
	} else {
		ircstats.is_abad++;
	}
	return;
}

static char *ident_lookup_parse(Client *client, char *buf)
{
	/* <port> , <port> : USERID : <OSTYPE>: <username>
	 * Actually the only thing we care about is <username>
	 */
	int port1 = 0, port2 = 0;
	char *ostype = NULL;
	char *username = NULL;
	char *p, *p2;

	skip_whitespace(&buf);
	p = strchr(buf, ',');
	if (!p)
		return NULL;
	*p = '\0';
	port1 = atoi(buf); /* port1 is set */

	/*  <port> : USERID : <OSTYPE>: <username> */
	buf = p + 1;
	p = strchr(buf, ':');
	if (!p)
		return NULL;
	*p = '\0';
	port2 = atoi(buf); /* port2 is set */

	/*  USERID : <OSTYPE>: <username> */
	buf = p + 1;
	skip_whitespace(&buf);
	if (strncmp(buf, "USERID", 6))
		return NULL;
	buf += 6; /* skip over strlen("USERID") */
	skip_whitespace(&buf);
	if (*buf != ':')
		return NULL;
	buf++;
	skip_whitespace(&buf);

	/*  <OSTYPE>: <username> */
	p = strchr(buf, ':');
	if (!p)
		return NULL;

	/*  <username> */
	buf = p+1;
	skip_whitespace(&buf);

	/* Username */
	// A) Skip any ~ or ^ at the start
	for (; *buf; buf++)
		if (!strchr("~^", *buf) && (*buf > 32))
			break;
	// B) Stop at the end, IOTW stop at newline, space, etc.
	for (p=buf; *p; p++)
	{
		if (strchr("\n\r@:", *p) || (*p <= 32))
		{
			*p = '\0';
			break;
		}
	}
	if (*buf == '\0')
		return NULL;
	return buf;
}

/** Stop ident lookup (can safely be called in all cases) */
void _cancel_ident_lookup(Client *client)
{
	if (client->local && (client->local->authfd >= 0))
	{
		fd_close(client->local->authfd);
		client->local->authfd = -1;
		--OpenFiles;
	}
}
