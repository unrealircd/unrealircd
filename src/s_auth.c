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

#ifndef CLEAN_COMPILE
static char sccsid[] = "@(#)s_auth.c	1.18 4/18/94 (C) 1992 Darren Reed";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "version.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(__hpux)
# include "inet.h"
#endif
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

void ident_failed(aClient *cptr)
{
	Debug((DEBUG_NOTICE, "ident_failed() for %x", cptr));
	ircstp->is_abad++;
	if (cptr->authfd != -1)
	{
		CLOSE_SOCK(cptr->authfd);
		--OpenFiles;
		cptr->authfd = -1;
	}
	cptr->flags &= ~(FLAGS_WRAUTH | FLAGS_AUTH);
	if (!DoingDNS(cptr))
		SetAccess(cptr);
	if (SHOWCONNECTINFO && !cptr->serv)
		sendto_one(cptr, "%s", REPORT_FAIL_ID);
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
	struct SOCKADDR_IN sock, us;
	int len;
	
	if (IDENT_CHECK == 0) {
		cptr->flags &= ~(FLAGS_WRAUTH | FLAGS_AUTH);
		return;
	}
	Debug((DEBUG_NOTICE, "start_auth(%x) slot=%d, fd=%d, status=%d",
	    cptr, cptr->slot, cptr->fd, cptr->status));
	if ((cptr->authfd = socket(AFINET, SOCK_STREAM, 0)) == -1)
	{
		Debug((DEBUG_ERROR, "Unable to create auth socket for %s:%s",
		    get_client_name(cptr, TRUE), strerror(get_sockerr(cptr))));
		ident_failed(cptr);
		return;
	}
    if (++OpenFiles >= (MAXCONNECTIONS - 2))
	{
		sendto_ops("Can't allocate fd, too many connections.");
		CLOSE_SOCK(cptr->authfd);
		--OpenFiles;
		cptr->authfd = -1;
		return;
	}

	if (SHOWCONNECTINFO && !cptr->serv)
		sendto_one(cptr, "%s", REPORT_DO_ID);

	set_non_blocking(cptr->authfd, cptr);

	/* Bind to the IP the user got in */
	memset(&sock, 0, sizeof(sock));
	len = sizeof(us);
	if (!getsockname(cptr->fd, (struct SOCKADDR *)&us, &len))
	{
#ifndef INET6
		sock.SIN_ADDR = us.SIN_ADDR;
#else
		bcopy(&us.SIN_ADDR, &sock.SIN_ADDR, sizeof(struct IN_ADDR));
#endif
		sock.SIN_PORT = 0;
		sock.SIN_FAMILY = AFINET;	/* redundant? */
		(void)bind(cptr->authfd, (struct SOCKADDR *)&sock, sizeof(sock));
	}

	bcopy((char *)&cptr->ip, (char *)&sock.SIN_ADDR,
	    sizeof(struct IN_ADDR));

	sock.SIN_PORT = htons(113);
	sock.SIN_FAMILY = AFINET;

	if (connect(cptr->authfd, (struct sockaddr *)&sock, sizeof(sock)) == -1 && !(ERRNO == P_EWORKING))
	{
		ident_failed(cptr);
		return;
	}
	cptr->flags |= (FLAGS_WRAUTH | FLAGS_AUTH);
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
void send_authports(aClient *cptr)
{
	struct SOCKADDR_IN us, them;
	char authbuf[32];
	int  ulen, tlen;

	Debug((DEBUG_NOTICE, "write_authports(%x) fd %d authfd %d stat %d",
	    cptr, cptr->fd, cptr->authfd, cptr->status));
	tlen = ulen = sizeof(us);
	if (getsockname(cptr->fd, (struct SOCKADDR *)&us, &ulen) ||
	    getpeername(cptr->fd, (struct SOCKADDR *)&them, &tlen))
	{
		goto authsenderr;
	}

	(void)ircsprintf(authbuf, "%u , %u\r\n",
	    (unsigned int)ntohs(them.SIN_PORT),
	    (unsigned int)ntohs(us.SIN_PORT));

	Debug((DEBUG_SEND, "sending [%s] to auth port %s.113",
	    authbuf, inetntoa((char *)&them.SIN_ADDR)));
	if (WRITE_SOCK(cptr->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
	{
		if (ERRNO == P_EAGAIN)
			return; /* Not connected yet, try again later */
authsenderr:
		ident_failed(cptr);
	}
	cptr->flags &= ~FLAGS_WRAUTH;
	return;
}

/*
 * read_authports
 *
 * read the reply (if any) from the ident server we connected to.
 * The actual read processijng here is pretty weak - no handling of the reply
 * if it is fragmented by IP.
 */
void read_authports(aClient *cptr)
{
	char *s, *t;
	int  len;
	char ruser[USERLEN + 1], system[8];
	u_short remp = 0, locp = 0;

	*system = *ruser = '\0';
	Debug((DEBUG_NOTICE, "read_authports(%x) fd %d authfd %d stat %d",
	    cptr, cptr->fd, cptr->authfd, cptr->status));
	/*
	 * Nasty.  Cant allow any other reads from client fd while we're
	 * waiting on the authfd to return a full valid string.  Use the
	 * client's input buffer to buffer the authd reply.
	 * Oh. this is needed because an authd reply may come back in more
	 * than 1 read! -avalon
	 */
	  if ((len = READ_SOCK(cptr->authfd, cptr->buffer + cptr->count,
		  sizeof(cptr->buffer) - 1 - cptr->count)) >= 0)
	{
		cptr->count += len;
		cptr->buffer[cptr->count] = '\0';
	}

	cptr->lasttime = TStime();
	if ((len > 0) && (cptr->count != (sizeof(cptr->buffer) - 1)) &&
	    (sscanf(cptr->buffer, "%hd , %hd : USERID : %*[^:]: %10s",
	    &remp, &locp, ruser) == 3))
	{
		s = rindex(cptr->buffer, ':');
		*s++ = '\0';
		for (t = (rindex(cptr->buffer, ':') + 1); *t; t++)
			if (!isspace(*t))
				break;
		strncpyzt(system, t, sizeof(system));
		for (t = ruser; *s && *s != '@' && (t < ruser + sizeof(ruser));
		    s++)
			if (!isspace(*s) && *s != ':')
				*t++ = *s;
		*t = '\0';
		Debug((DEBUG_INFO, "auth reply ok [%s] [%s]", system, ruser));
	}
	else if (len != 0)
	{
		if (!index(cptr->buffer, '\n') && !index(cptr->buffer, '\r'))
			return;
		Debug((DEBUG_ERROR, "local %d remote %d", locp, remp));
		Debug((DEBUG_ERROR, "bad auth reply in [%s]", cptr->buffer));
		*ruser = '\0';
	}
    CLOSE_SOCK(cptr->authfd);
    --OpenFiles;
    cptr->authfd = -1;
	cptr->count = 0;
	ClearAuth(cptr);
	if (!DoingDNS(cptr))
		SetAccess(cptr);
	if (len > 0)
		Debug((DEBUG_INFO, "ident reply: [%s]", cptr->buffer));

	if (SHOWCONNECTINFO && !cptr->serv)
		sendto_one(cptr, "%s", REPORT_FIN_ID);

	if (!locp || !remp || !*ruser)
	{
		ircstp->is_abad++;
		return;
	}
	ircstp->is_asuc++;
	strncpyzt(cptr->username, ruser, USERLEN + 1);
	cptr->flags |= FLAGS_GOTID;
	Debug((DEBUG_INFO, "got username [%s]", ruser));
	return;
}
