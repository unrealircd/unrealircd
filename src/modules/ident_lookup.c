/* src/modules/ident_lookup.c - Ident lookups (RFC1413)
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"ident_lookup",
	"1.0",
	"Ident lookups (RFC1413)",
	"UnrealIRCd Team",
	"unrealircd-5",
};

/* Forward declarations */
static EVENT(check_ident_timeout);
static int ident_lookup_connect(Client *cptr);
static void ident_lookup_send(int fd, int revents, void *data);
static void ident_lookup_receive(int fd, int revents, void *data);
static char *ident_lookup_parse(Client *acptr, char *buf);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1); /* needed? or not? */
	EventAdd(NULL, "check_ident_timeout", 1, 0, check_ident_timeout, NULL);
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


static void ident_lookup_failed(Client *cptr)
{
	Debug((DEBUG_NOTICE, "ident_lookup_failed() for %p", cptr));
	ircstats.is_abad++;
	if (cptr->local->authfd != -1)
	{
		fd_close(cptr->local->authfd);
		--OpenFiles;
		cptr->local->authfd = -1;
	}
	ClearIdentLookupSent(cptr);
	ClearIdentLookup(cptr);
	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, NULL, "%s", REPORT_FAIL_ID);
	if (!IsDNSLookup(cptr))
		finish_auth(cptr);
}

static EVENT(check_ident_timeout)
{
	Client *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &unknown_list, lclient_node)
	{
		if (IsIdentLookup(cptr) && ((TStime() - cptr->local->firsttime) > IDENT_CONNECT_TIMEOUT))
			ident_lookup_failed(cptr);
	}
}

/** Start the ident lookup for this user */
static int ident_lookup_connect(Client *cptr)
{
	char buf[BUFSIZE];

	snprintf(buf, sizeof buf, "identd: %s", get_client_name(cptr, TRUE));
	if ((cptr->local->authfd = fd_socket(IsIPV6(cptr) ? AF_INET6 : AF_INET, SOCK_STREAM, 0, buf)) == -1)
	{
		ident_lookup_failed(cptr);
		return 0;
	}
	if (++OpenFiles >= maxclients+1)
	{
		sendto_ops("Can't allocate fd, too many connections.");
		fd_close(cptr->local->authfd);
		--OpenFiles;
		cptr->local->authfd = -1;
		return 0;
	}

	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, NULL, "%s", REPORT_DO_ID);

	set_sock_opts(cptr->local->authfd, cptr, IsIPV6(cptr));

	/* Bind to the IP the user got in */
	unreal_bind(cptr->local->authfd, cptr->local->listener->ip, 0, IsIPV6(cptr));

	/* And connect... */
	if (!unreal_connect(cptr->local->authfd, cptr->ip, 113, IsIPV6(cptr)))
	{
		ident_lookup_failed(cptr);
		return 0;
	}
	SetIdentLookupSent(cptr);
	SetIdentLookup(cptr);

	fd_setselect(cptr->local->authfd, FD_SELECT_WRITE, ident_lookup_send, cptr);

	return 0;
}

/** Send the request to the ident server */
static void ident_lookup_send(int fd, int revents, void *data)
{
	char authbuf[32];
	Client *cptr = data;

	ircsnprintf(authbuf, sizeof(authbuf), "%d , %d\r\n",
		cptr->local->port,
		cptr->local->listener->port);

	if (WRITE_SOCK(cptr->local->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
	{
		if (ERRNO == P_EAGAIN)
			return; /* Not connected yet, try again later */
		ident_lookup_failed(cptr);
		return;
	}
	ClearIdentLookupSent(cptr);

	fd_setselect(cptr->local->authfd, FD_SELECT_READ, ident_lookup_receive, cptr);
	fd_setselect(cptr->local->authfd, FD_SELECT_WRITE, NULL, cptr);

	return;
}

/** Receive the ident response */
static void ident_lookup_receive(int fd, int revents, void *userdata)
{
	Client *cptr = userdata;
	char *ident = NULL;
	char buf[512];
	ssize_t len;

	len = READ_SOCK(cptr->local->authfd, buf, sizeof(buf)-1);
	if (ERRNO == P_EAGAIN)
		return; /* Try again later */

	/* We received a response. We don't bother with fragmentation
	 * since that is not going to happen for such a short string.
	 * Other IRCd's think the same and this simplifies things a lot.
	 */

	/* Before we continue, we can already tear down the connection
	 * and set the appropriate flags that we are finished.
	 */
	fd_close(cptr->local->authfd);
	--OpenFiles;
	cptr->local->authfd = -1;
	cptr->local->identbufcnt = 0;
	ClearIdentLookup(cptr);
	if (!IsDNSLookup(cptr))
		finish_auth(cptr);

	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, NULL, "%s", REPORT_FIN_ID);

	if (len > 0)
	{
		buf[len] = '\0'; /* safe, due to the READ_SOCK() being on sizeof(buf)-1 */
		ident = ident_lookup_parse(cptr, buf);
	}
	if (ident)
	{
		strlcpy(cptr->ident, ident, USERLEN + 1);
		SetIdentSuccess(cptr);
		ircstats.is_asuc++;
	} else {
		ircstats.is_abad++;
	}
	return;
}

static char *ident_lookup_parse(Client *acptr, char *buf)
{
	/* <port> , <port> : USERID : <OSTYPE>: <username>
	 * Actually the only thing we care about is <username>
	 */
	int port1 = 0, port2 = 0;
	char *ostype = NULL;
	char *username = NULL;
	char *p, *p2;

	/* First port */
	p = strstr(buf, " , ");
	if (!p)
		return NULL;
	*p = '\0';
	port1 = atoi(buf);
	buf = p+ strlen(" , ");

	/* Second port */
	p = strstr(buf, " : USERID : ");
	if (!p)
		return NULL;
	*p = '\0';
	port2 = atoi(buf);
	buf = p+ strlen(" : USERID : ");

	/* USERID */
	p = strstr(buf, " : ");
	if (!p)
		return NULL;
	*p = '\0';
	ostype = buf;
	buf = p+ strlen(" : ");

	/* Username */
	// A) Skip any ~ or ^ at the start
	for (; *buf; buf++)
		if (!strchr("~^", *buf) && (*buf > 32))
			break;
	// B) Stop at the end, IOTW stop at newline, space, etc.
	for (p=buf; *p; p++)
		if (strchr("\n\r@:", *p) || (*p <= 32))
		{
			*p = '\0';
			break;
		}
	if (*buf == '\0')
		return NULL;
	return buf;
}
