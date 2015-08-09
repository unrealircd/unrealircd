 /*
 *   Unreal Internet Relay Chat Daemon, src/socket.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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
#include "h.h"
#include <signal.h>
#include "inet.h"
#ifndef _WIN32
extern int errno;		/* ...seems that errno.h doesn't define this everywhere */
#endif
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#endif

/*
** deliver_it
**	Attempt to send a sequence of bytes to the connection.
**	Returns
**
**	< 0	Some fatal error occurred, (but not EWOULDBLOCK).
**		This return is a request to close the socket and
**		clean up the link.
**	
**	>= 0	No real error occurred, returns the number of
**		bytes actually transferred. EWOULDBLOCK and other
**		possibly similar conditions should be mapped to
**		zero return. Upper level routine will have to
**		decide what to do with those unwritten bytes...
**
**	*NOTE*	alarm calls have been preserved, so this should
**		work equally well whether blocking or non-blocking
**		mode is used...
**
**	*NOTE*	I nuked 'em.  At the load of current ircd servers
**		you can't run with stuff that blocks. And we don't.
*/
int  deliver_it(aClient *cptr, char *str, int len)
{
	int  retval;

	if (IsDead(cptr) || (!IsServer(cptr) && !IsPerson(cptr)
	    && !IsHandshake(cptr) 
	    && !IsSSLHandshake(cptr)
 
	    && !IsUnknown(cptr)))
	{
		str[len] = '\0';
		sendto_ops
		    ("* * * DEBUG ERROR * * * !!! Calling deliver_it() for %s, status %d %s, with message: %s",
		    cptr->name, cptr->status, IsDead(cptr) ? "DEAD" : "", str);
		return -1;
	}

	if (IsSSL(cptr) && cptr->local->ssl != NULL)
	{
		retval = SSL_write(cptr->local->ssl, str, len);

		if (retval < 0)
		{
			switch (SSL_get_error(cptr->local->ssl, retval))
			{
			case SSL_ERROR_WANT_READ:
				/* retry later */
				return 0;
			case SSL_ERROR_WANT_WRITE:
				SET_ERRNO(P_EWOULDBLOCK);
				break;
			case SSL_ERROR_SYSCALL:
				break;
			case SSL_ERROR_SSL:
				if (ERRNO == P_EAGAIN)
					break;
			default:
				return -1; /* hm.. why was this 0?? we have an error! */
			}
		}
	}
	else
		retval = send(cptr->fd, str, len, 0);
	/*
	   ** Convert WOULDBLOCK to a return of "0 bytes moved". This
	   ** should occur only if socket was non-blocking. Note, that
	   ** all is Ok, if the 'write' just returns '0' instead of an
	   ** error and errno=EWOULDBLOCK.
	   **
	   ** ...now, would this work on VMS too? --msa
	 */
# ifndef _WIN32
	if (retval < 0 && (errno == EWOULDBLOCK || errno == EAGAIN ||
	    errno == ENOBUFS))
# else
		if (retval < 0 && (WSAGetLastError() == WSAEWOULDBLOCK ||
		    WSAGetLastError() == WSAENOBUFS))
# endif
			retval = 0;

	if (retval > 0)
	{
		cptr->local->sendB += retval;
		me.local->sendB += retval;
		if (cptr->local->sendB > 1023)
		{
			cptr->local->sendK += (cptr->local->sendB >> 10);
			cptr->local->sendB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
		}
		if (me.local->sendB > 1023)
		{
			me.local->sendK += (me.local->sendB >> 10);
			me.local->sendB &= 0x03ff;
		}
	}

	return (retval);
}

/** Initiate an outgoing connection, the actual connect() call. */
int unreal_connect(int fd, char *ip, int port, int ipv6)
{
	int n;
	
	if (ipv6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		inet_pton(AF_INET6, ip, &server.sin6_addr);
		server.sin6_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	} else {
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &server.sin_addr);
		server.sin_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	}

#ifndef _WIN32
	if (n < 0 && (errno != EINPROGRESS))
#else
	if (n < 0 && (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK))
#endif
	{
		return 0; /* FATAL ERROR */
	}
	
	return 1; /* SUCCESS (probably still in progress) */
}

/** Bind to an IP/port (port may be 0 for auto).
 * @returns 0 on failure, other on success.
 */
int unreal_bind(int fd, char *ip, int port, int ipv6)
{
	if (ipv6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		server.sin6_port = htons(port);
		if (inet_pton(AF_INET6, ip, &server.sin6_addr.s6_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	} else {
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &server.sin_addr.s_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	}
}

