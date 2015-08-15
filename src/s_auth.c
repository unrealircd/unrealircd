/*
 *   Unreal Internet Relay Chat Daemon, src/s_auth.c
 *   Copyright (C) 1992 Darren Reed
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "version.h"
#include "inet.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "sock.h"		/* If FD_ZERO isn't define up to this point,  */
			/* define it (BSD4.2 needs this) */
#include "h.h"
#include "res.h"
#include "proto.h"
#include <string.h>

static void send_authports(int fd, int revents, void *data);
static void read_authports(int fd, int revents, void *data);

void ident_failed(aClient *cptr)
{
	Debug((DEBUG_NOTICE, "ident_failed() for %x", cptr));
	ircstp->is_abad++;
	if (cptr->local->authfd != -1)
	{
		fd_close(cptr->local->authfd);
		--OpenFiles;
		cptr->local->authfd = -1;
	}
	cptr->flags &= ~(FLAGS_WRAUTH | FLAGS_AUTH);
	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, "%s", REPORT_FAIL_ID);
	if (!DoingDNS(cptr))
		finish_auth(cptr);
}

/*
 * start_auth
 *
 * Flag the client to show that an attempt to contact the ident server on
 * the client's host.  The connect and subsequently the socket are all put
 * into 'non-blocking' mode.  Should the connect or any later phase of the
 * identifing process fail, it is aborted and the user is given a username
 * of "unknown".
 */
void start_auth(aClient *cptr)
{
	int len;
	char buf[BUFSIZE];

	/* If ident checking is disabled or it's an outgoing connect, then no ident check */
	if ((IDENT_CHECK == 0) || (cptr->serv && IsHandshake(cptr)))
	{
		cptr->flags &= ~(FLAGS_WRAUTH | FLAGS_AUTH);
		if (!DoingDNS(cptr))
			finish_auth(cptr);
		return;
	}
	Debug((DEBUG_NOTICE, "start_auth(%x) fd=%d, status=%d",
	    cptr, cptr->fd, cptr->status));
	snprintf(buf, sizeof buf, "identd: %s", get_client_name(cptr, TRUE));
	if ((cptr->local->authfd = fd_socket(IsIPV6(cptr) ? AF_INET6 : AF_INET, SOCK_STREAM, 0, buf)) == -1)
	{
		Debug((DEBUG_ERROR, "Unable to create auth socket for %s:%s",
		    get_client_name(cptr, TRUE), strerror(get_sockerr(cptr))));
		ident_failed(cptr);
		return;
	}
	if (++OpenFiles >= (MAXCONNECTIONS - 2))
	{
		sendto_ops("Can't allocate fd, too many connections.");
		fd_close(cptr->local->authfd);
		--OpenFiles;
		cptr->local->authfd = -1;
		return;
	}

	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, "%s", REPORT_DO_ID);

	set_sock_opts(cptr->local->authfd, cptr, IsIPV6(cptr));
	set_non_blocking(cptr->local->authfd, cptr);

	/* Bind to the IP the user got in */
	unreal_bind(cptr->local->authfd, cptr->local->listener->ip, 0, IsIPV6(cptr));

	/* And connect... */
	if (!unreal_connect(cptr->local->authfd, cptr->ip, 113, IsIPV6(cptr)))
	{
		ident_failed(cptr);
		return;
	}
	cptr->flags |= (FLAGS_WRAUTH | FLAGS_AUTH);

	fd_setselect(cptr->local->authfd, FD_SELECT_WRITE, send_authports, cptr);

	return;
}

/*
 * send_authports
 *
 * Send the ident server a query giving "theirport , ourport".
 * The write is only attempted *once* so it is deemed to be a fail if the
 * entire write doesn't write all the data given.  This shouldnt be a
 * problem since the socket should have a write buffer far greater than
 * this message to store it in should problems arise. -avalon
 */
static void send_authports(int fd, int revents, void *data)
{
	char authbuf[32];
	int  ulen, tlen;
	aClient *cptr = data;

	Debug((DEBUG_NOTICE, "write_authports(%x) fd %d authfd %d stat %d",
	    cptr, cptr->fd, cptr->local->authfd, cptr->status));

	ircsnprintf(authbuf, sizeof(authbuf), "%d , %d\r\n",
		cptr->local->port,
		cptr->local->listener->port);

	Debug((DEBUG_SEND, "sending [%s] to auth port %s.113", authbuf, cptr->ip));
	if (WRITE_SOCK(cptr->local->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
	{
		if (ERRNO == P_EAGAIN)
			return; /* Not connected yet, try again later */
authsenderr:
		ident_failed(cptr);
		return;
	}
	cptr->flags &= ~FLAGS_WRAUTH;

	fd_setselect(cptr->local->authfd, FD_SELECT_READ|FD_SELECT_NOWRITE, read_authports, cptr);

	return;
}

/*
 * read_authports
 *
 * read the reply (if any) from the ident server we connected to.
 * The actual read processijng here is pretty weak - no handling of the reply
 * if it is fragmented by IP.
 */
static void read_authports(int fd, int revents, void *userdata)
{
	char *s, *t;
	int  len;
	char ruser[USERLEN + 1], system[8];
	u_short remp = 0, locp = 0;
	aClient *cptr = userdata;

	*system = *ruser = '\0';
	Debug((DEBUG_NOTICE, "read_authports(%x) fd %d authfd %d stat %d",
	    cptr, cptr->fd, cptr->local->authfd, cptr->status));
	/*
	 * Nasty.  Cant allow any other reads from client fd while we're
	 * waiting on the authfd to return a full valid string.  Use the
	 * client's input buffer to buffer the authd reply.
	 * Oh. this is needed because an authd reply may come back in more
	 * than 1 read! -avalon
	 */
	  if ((len = READ_SOCK(cptr->local->authfd, cptr->local->buffer + cptr->count,
		  sizeof(cptr->local->buffer) - 1 - cptr->count)) >= 0)
	{
		cptr->count += len;
		cptr->local->buffer[cptr->count] = '\0';
	}

	cptr->local->lasttime = TStime();
	if ((len > 0) && (cptr->count != (sizeof(cptr->local->buffer) - 1)) &&
	    (sscanf(cptr->local->buffer, "%hd , %hd : USERID : %*[^:]: %10s",
	    &remp, &locp, ruser) == 3))
	{
		s = rindex(cptr->local->buffer, ':');
		*s++ = '\0';
		for (t = (rindex(cptr->local->buffer, ':') + 1); *t; t++)
			if (!isspace(*t))
				break;
		strlcpy(system, t, sizeof(system));
		for (t = ruser; *s && *s != '@' && (t < ruser + sizeof(ruser));
		    s++)
			if (!isspace(*s) && *s != ':')
				*t++ = *s;
		*t = '\0';
		Debug((DEBUG_INFO, "auth reply ok [%s] [%s]", system, ruser));
	}
	else if (len != 0)
	{
		if (!index(cptr->local->buffer, '\n') && !index(cptr->local->buffer, '\r'))
			return;
		Debug((DEBUG_ERROR, "local %d remote %d", locp, remp));
		Debug((DEBUG_ERROR, "bad auth reply in [%s]", cptr->local->buffer));
		*ruser = '\0';
	}
    fd_close(cptr->local->authfd);
    --OpenFiles;
    cptr->local->authfd = -1;
	cptr->count = 0;
	ClearAuth(cptr);
	if (!DoingDNS(cptr))
		finish_auth(cptr);
	if (len > 0)
		Debug((DEBUG_INFO, "ident reply: [%s]", cptr->local->buffer));

	if (SHOWCONNECTINFO && !cptr->serv && !IsServersOnlyListener(cptr->local->listener))
		sendto_one(cptr, "%s", REPORT_FIN_ID);

	if (!locp || !remp || !*ruser)
	{
		ircstp->is_abad++;
		return;
	}
	ircstp->is_asuc++;
	strlcpy(cptr->username, ruser, USERLEN + 1);
	cptr->flags |= FLAGS_GOTID;
	Debug((DEBUG_INFO, "got username [%s]", ruser));
	return;
}
