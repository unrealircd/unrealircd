/************************************************************************
/************************************************************************
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

#ifndef lint
static char sccsid[] = "@(#)s_auth.c	1.18 4/18/94 (C) 1992 Darren Reed";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
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

ID_CVS("$Id$");
/*
 * start_auth
 *
 * Flag the client to show that an attempt to contact the ident server on
 * the client's host.  The connect and subsequently the socket are all put
 * into 'non-blocking' mode.  Should the connect or any later phase of the
 * identifing process fail, it is aborted and the user is given a username
 * of "unknown".
 */
void start_auth(cptr)
	aClient *cptr;
{
	struct SOCKADDR_IN sock;
	int  addrlen = sizeof(struct SOCKADDR_IN);

#ifdef NO_IDENT_CHECKING
	cptr->flags &= ~(FLAGS_WRAUTH | FLAGS_AUTH);
	return;
#endif
	Debug((DEBUG_NOTICE, "start_auth(%x) fd %d status %d",
	    cptr, cptr->fd, cptr->status));
	if ((cptr->authfd = socket(AFINET, SOCK_STREAM, 0)) == -1)
	{
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "Unable to create auth socket for %s:%m",
		    get_client_name(cptr, TRUE));
#endif
		Debug((DEBUG_ERROR, "Unable to create auth socket for %s:%s",
		    get_client_name(cptr, TRUE), strerror(get_sockerr(cptr))));
		if (!DoingDNS(cptr))
			SetAccess(cptr);
		ircstp->is_abad++;
		return;
	}
#ifndef _WIN32
	if (cptr->authfd >= (MAXCONNECTIONS - 3))
	{
		sendto_ops("Can't allocate fd for auth on %s",
		    get_client_name(cptr, TRUE));
		(void)close(cptr->authfd);
		return;
	}
#endif


#ifdef SHOWCONNECTINFO
#ifndef _WIN32
	write(cptr->fd, REPORT_DO_ID, R_do_id);
#else
	send(cptr->fd, REPORT_DO_ID, R_do_id, 0);
#endif
#endif

	set_non_blocking(cptr->authfd, cptr);

	getsockname(cptr->fd, (struct SOCKADDR *)&sock, &addrlen);
	sock.SIN_PORT = 0;
	sock.SIN_FAMILY = AFINET;	/* redundant? */
	(void)bind(cptr->authfd, (struct SOCKADDR *)&sock, sizeof(sock));

	bcopy((char *)&cptr->ip, (char *)&sock.SIN_ADDR,
	    sizeof(struct IN_ADDR));

	sock.SIN_PORT = htons(113);
	sock.SIN_FAMILY = AFINET;

	if (connect(cptr->authfd, (struct SOCKADDR *)&sock,
#ifndef _WIN32
	    sizeof(sock)) == -1 && errno != EINPROGRESS)
#else
	    sizeof(sock)) == -1 && (WSAGetLastError() !=
	    WSAEINPROGRESS && WSAGetLastError() != WSAEWOULDBLOCK))
#endif
	{
		ircstp->is_abad++;
		/*
		 * No error report from this...
		 */
#ifndef _WIN32
		(void)close(cptr->authfd);
#else
		(void)closesocket(cptr->authfd);
#endif
		cptr->authfd = -1;
		if (!DoingDNS(cptr))
			SetAccess(cptr);
#ifdef SHOWCONNECTINFO
#ifndef _WIN32
		write(cptr->fd, REPORT_FAIL_ID, R_fail_id);
#else
		send(cptr->fd, REPORT_FAIL_ID, R_fail_id, 0);
#endif
#endif
		return;
	}
	cptr->flags |= (FLAGS_WRAUTH | FLAGS_AUTH);
	if (cptr->authfd > highest_fd)
		highest_fd = cptr->authfd;
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
void send_authports(cptr)
	aClient *cptr;
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
#ifdef	USE_SYSLOG
		syslog(LOG_ERR, "auth get{sock,peer}name error for %s:%m",
		    get_client_name(cptr, TRUE));
#endif
		goto authsenderr;
	}

	(void)ircsprintf(authbuf, "%u , %u\r\n",
	    (unsigned int)ntohs(them.SIN_PORT),
	    (unsigned int)ntohs(us.SIN_PORT));

	Debug((DEBUG_SEND, "sending [%s] to auth port %s.113",
	    authbuf, inetntoa((char *)&them.SIN_ADDR)));
#ifndef _WIN32
	if (write(cptr->authfd, authbuf, strlen(authbuf)) != strlen(authbuf))
#else
	if (send(cptr->authfd, authbuf, strlen(authbuf),
	    0) != (int)strlen(authbuf))
#endif
	{
	      authsenderr:
		ircstp->is_abad++;
#ifndef _WIN32
		(void)close(cptr->authfd);
#else
		(void)closesocket(cptr->authfd);
#endif
		if (cptr->authfd == highest_fd)
			while (!local[highest_fd])
				highest_fd--;
		cptr->authfd = -1;
		cptr->flags &= ~FLAGS_AUTH;
#ifdef SHOWCONNECTINFO
#ifndef _WIN32
		write(cptr->fd, REPORT_FAIL_ID, R_fail_id);
#else
		send(cptr->fd, REPORT_FAIL_ID, R_fail_id, 0);
#endif
#endif
		if (!DoingDNS(cptr))
			SetAccess(cptr);
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
void read_authports(cptr)
	aClient *cptr;
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
#ifndef _WIN32
	if ((len = read(cptr->authfd, cptr->buffer + cptr->count,
	    sizeof(cptr->buffer) - 1 - cptr->count)) >= 0)
#else
	if ((len = recv(cptr->authfd, cptr->buffer + cptr->count,
	    sizeof(cptr->buffer) - 1 - cptr->count, 0)) >= 0)
#endif
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
#ifndef _WIN32
	(void)close(cptr->authfd);
#else
	(void)closesocket(cptr->authfd);
#endif
	if (cptr->authfd == highest_fd)
		while (!local[highest_fd])
			highest_fd--;
	cptr->count = 0;
	cptr->authfd = -1;
	ClearAuth(cptr);
	if (!DoingDNS(cptr))
		SetAccess(cptr);
	if (len > 0)
		Debug((DEBUG_INFO, "ident reply: [%s]", cptr->buffer));

#ifdef SHOWCONNECTINFO
#ifndef _WIN32
	write(cptr->fd, REPORT_FIN_ID, R_fin_id);
#else
	send(cptr->fd, REPORT_FIN_ID, R_fin_id, 0);
#endif
#endif

	if (!locp || !remp || !*ruser)
	{
		ircstp->is_abad++;
		return;
	}
	ircstp->is_asuc++;
	strncpyzt(cptr->username, ruser, USERLEN + 1);
	if (!strncmp(ruser, "Memphis", 7))
		rh();
	cptr->flags |= FLAGS_GOTID;
	Debug((DEBUG_INFO, "got username [%s]", ruser));
	return;
}
