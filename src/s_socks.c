/*
 *   Unreal Internet Relay Chat Daemon, src/s_socks.c
 *   Copyright (C) 1998 Lucas Madar
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
static char sccsid[] = "@(#)s_socks.c	1.0 28/9/98 ";
#endif


/* All these includes are copied from auth.c because we are
   just doing something very similar */
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
#ifdef	UNIXPORT
# include <sys/un.h>
#endif
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
#ifdef SOCKSPORT

static unsigned char socksid[12];

#define HICHAR(s)	(((unsigned short) s) >> 8)
#define LOCHAR(s)	(((unsigned short) s) & 0xFF)


/* init_socks
 * Set up socksid, simply.
 */

void init_socks(aClient *cptr)
{
	unsigned short sport = SOCKSPORT;
	struct SOCKADDR_IN sin;

	socksid[0] = 4;
	socksid[1] = 1;
	socksid[2] = HICHAR(sport);
	socksid[3] = LOCHAR(sport);
	socksid[8] = 0;

	if ((cptr->socksfd = socket(AFINET, SOCK_STREAM, 0)) == -1)
		return;

	set_non_blocking(cptr->socksfd, cptr);

#ifdef INET6
	sin.SIN_ADDR = in6addr_any;
#else
	sin.SIN_ADDR.S_ADDR = INADDR_ANY;
#endif
	sin.SIN_PORT = htons(sport);
	sin.SIN_FAMILY = AFINET;

	if (bind(cptr->socksfd, (struct SOCKADDR *)&sin, sizeof(sin)))
	{
#ifdef _WIN32
		closesocket(cptr->socksfd);
#else
		close(cptr->socksfd);
#endif
		cptr->socksfd = -1;
		return;
	}

	listen(cptr->socksfd, LISTEN_SIZE);

	/* Socks lietening port is now set up */
	/* Similar to discard port */
}

/*
 * start_socks
 *
 * attempts to connect to a socks server on port 1080 of the host.
 * if the connect fails, the user passes the socks check
 * otherwise, the connection is checked to see if it is a "secure"
 * socks4+ server
 */
void start_socks(aClient *cptr)
{
	struct SOCKADDR_IN sin;
	int  sinlen = sizeof(struct SOCKADDR_IN);
 
	if ((cptr->socksfd = socket(AFINET, SOCK_STREAM, 0)) < 0)
	{
		Debug((DEBUG_ERROR, "Unable to create socks socket for %s:%s",
		    get_client_name(cptr, TRUE), strerror(get_sockerr(cptr))));
		return;
	}
	if (++OpenFiles >= (MAXCONNECTIONS - 3))
	{
		sendto_ops("Can't allocate fd for socks on %s",
		    get_client_name(cptr, TRUE));
		CLOSE_SOCK(cptr->socksfd);
		--OpenFiles;
		cptr->socksfd = -1;
		return;
	}
#ifndef INET6
	if (Find_except((char *)inetntoa((char *)&cptr->ip),0))
#else
	if (Find_except((char *)inet_ntop(AF_INET6, (char *)&cptr->ip, mydummy, MYDUMMY_SIZE),0))
#endif
		goto skip_socks;

#ifdef SHOWCONNECTINFO
	sendto_one(cptr, REPORT_DO_SOCKS);
#endif

	set_non_blocking(cptr->socksfd, cptr);

	sin.SIN_PORT = htons(1080);
	sin.SIN_FAMILY = AFINET;
	bcopy((char *)&cptr->ip, (char *)&sin.SIN_ADDR, sizeof(struct IN_ADDR));

	if (connect(cptr->socksfd, (struct SOCKADDR *)&sin,
#ifndef _WIN32
	    sinlen) == -1 && errno != EINPROGRESS)
#else
	    sinlen) == -1 && (WSAGetLastError() != WSAEINPROGRESS && 
		WSAGetLastError() != WSAEWOULDBLOCK))
#endif
	{
		/* we have no socks server! */
		CLOSE_SOCK(cptr->socksfd);
		--OpenFiles;
		cptr->socksfd = -1;
#ifdef SHOWCONNECTINFO
		sendto_one(cptr, REPORT_NO_SOCKS);
#endif
		return;
	}
	cptr->flags |= (FLAGS_WRSOCKS | FLAGS_SOCKS);
	return;

skip_socks:
	CLOSE_SOCK(cptr->socksfd);
	--OpenFiles;
	cptr->socksfd = -1;
#ifdef SHOWCONNECTINFO
	sendto_one(cptr, REPORT_NO_SOCKS);
#endif
	return;
}

/*
 * send_socksquery
 *
 * send the socks server a query to see if it's open.
 */
void send_socksquery(aClient *cptr)
{

	struct SOCKADDR_IN sin;
	int  sinlen = sizeof(struct SOCKADDR_IN);
	unsigned char socksbuf[12];
	unsigned long theip;
#ifndef INET6
	bcopy((char *)&socksid, (char *)&socksbuf, 9);

	getsockname(cptr->fd, (struct SOCKADDR *)&sin, &sinlen);

	theip = htonl(sin.SIN_ADDR.S_ADDR);

	socksbuf[4] = (theip >> 24);
	socksbuf[5] = (theip >> 16) & 0xFF;
	socksbuf[6] = (theip >> 8) & 0xFF;
	socksbuf[7] = theip & 0xFF;

	if (send(cptr->socksfd, socksbuf, 9, 0) != 9)
	{
		CLOSE_SOCK(cptr->socksfd);
		--OpenFiles;
		cptr->socksfd = -1;
		cptr->flags &= ~FLAGS_SOCKS;
#ifdef SHOWCONNECTINFO
		sendto_one(cptr, REPORT_NO_SOCKS);
#endif
	}
	cptr->flags &= ~FLAGS_WRSOCKS;
#endif
	cptr->flags &= ~FLAGS_SOCKS;
	return;
}

/*
 * read_socks
 *
 * process the socks reply.
 */

void read_socks(aClient *cptr)
{
	unsigned char socksbuf[12];
	int  len;

	len = recv(cptr->socksfd, socksbuf, 9, 0);
	CLOSE_SOCK(cptr->socksfd);
	--OpenFiles;
	cptr->socksfd = -1;
	ClearSocks(cptr);

	if (len < 4)
	{
#ifdef SHOWCONNECTINFO
		sendto_one(cptr, REPORT_NO_SOCKS);
#endif
		return;
	}

	if (socksbuf[1] == 90)
	{
		cptr->flags |= FLAGS_GOTSOCKS;
		return;
	}

#ifdef SHOWCONNECTINFO
	sendto_one(cptr, REPORT_GOOD_SOCKS);
#endif
	return;
}
#endif
