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
#ifdef DEBUGMODE
int  writecalls = 0, writeb[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
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
	aClient *acpt = cptr->listener;

#ifdef	DEBUGMODE
	writecalls++;
#endif
#ifdef VMS
	retval = netwrite(cptr->fd, str, len);
#else
	if (IsDead(cptr) || (!IsServer(cptr) && !IsPerson(cptr)
	    && !IsHandshake(cptr) 
#ifdef USE_SSL
	    && !IsSSLHandshake(cptr)
#endif 
 
	    && !IsUnknown(cptr)))
	{
		str[len] = '\0';
		sendto_ops
		    ("* * * DEBUG ERROR * * * !!! Calling deliver_it() for %s, status %d %s, with message: %s",
		    cptr->name, cptr->status, IsDead(cptr) ? "DEAD" : "", str);
		return -1;
	}

#ifdef USE_SSL
	if (cptr->flags & FLAGS_SSL)
		 retval = ircd_SSL_write(cptr, str, len);	
	else
#endif
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
		{
			retval = 0;
			SetBlocked(cptr);
		}
		else if (retval > 0)
		{
			ClearBlocked(cptr);
		}

#endif
#ifdef DEBUGMODE
	if (retval < 0)
	{
		writeb[0]++;
		Debug((DEBUG_ERROR, "write error (%s) to %s", STRERROR(ERRNO), cptr->name));

	}
	else if (retval == 0)
		writeb[1]++;
	else if (retval < 16)
		writeb[2]++;
	else if (retval < 32)
		writeb[3]++;
	else if (retval < 64)
		writeb[4]++;
	else if (retval < 128)
		writeb[5]++;
	else if (retval < 256)
		writeb[6]++;
	else if (retval < 512)
		writeb[7]++;
	else if (retval < 1024)
		writeb[8]++;
	else
		writeb[9]++;
#endif
	if (retval > 0)
	{
		cptr->sendB += retval;
		me.sendB += retval;
		if (cptr->sendB > 1023)
		{
			cptr->sendK += (cptr->sendB >> 10);
			cptr->sendB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
		}
		if (acpt != &me)
		{
			acpt->sendB += retval;
			if (acpt->sendB > 1023)
			{
				acpt->sendK += (acpt->sendB >> 10);
				acpt->sendB &= 0x03ff;
			}
		}
		if (me.sendB > 1023)
		{
			me.sendK += (me.sendB >> 10);
			me.sendB &= 0x03ff;
		}
	}
	return (retval);
}

char	*Inet_si2pB(struct SOCKADDR_IN *sin, char *buf, int sz)
{
#ifdef INET6
	u_char	*cp;
	
	cp = (u_char *)sin->SIN_ADDR.s6_addr;
	if (cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
	    && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
	    && cp[9] == 0 && cp[10] == 0xff
	    && cp[11] == 0xff)
	{
		(void)ircsprintf(buf, "%u.%u.%u.%u",
		    (u_int)(cp[12]), (u_int)(cp[13]),
		    (u_int)(cp[14]), (u_int)(cp[15]));
	
		return (buf);
	}
	else
		return ((char *)inetntop(AFINET, &sin->SIN_ADDR.s6_addr, buf, sz));
#else
	return ((char *)inet_ntoa(sin->SIN_ADDR));	
#endif
}

char	*Inet_si2p(struct SOCKADDR_IN *sin)
{
	static char	buf[256];
	
	return (Inet_si2pB(sin, buf, sizeof(buf)));
}

char	*Inet_ia2p(struct IN_ADDR *ia)
{
#ifndef INET6
	return ((char *)inet_ntoa(*ia));
#else
	/* Hack to make proper addresses */
	static char buf[256]; 
	u_char	*cp;
	
	cp = (u_char *)((struct IN_ADDR *)ia)->s6_addr;
	if (cp[0] == 0 && cp[1] == 0 && cp[2] == 0 && cp[3] == 0 && cp[4] == 0
	    && cp[5] == 0 && cp[6] == 0 && cp[7] == 0 && cp[8] == 0
	    && cp[9] == 0 && cp[10] == 0xff
	    && cp[11] == 0xff)
	{
		(void)ircsprintf(buf, "%u.%u.%u.%u",
		    (u_int)(cp[12]), (u_int)(cp[13]),
		    (u_int)(cp[14]), (u_int)(cp[15]));
	
		return (buf);
	}
	else
		return((char *)inet_ntop(AFINET, ia, buf, sizeof(buf)));
#endif
}

char	*Inet_ia2pNB(struct IN_ADDR *ia, int compressed)
{
#ifndef INET6
	return ((char *)inet_ntoa(*ia));
#else
	static char buf[256];
	if (compressed)
		return ((char *)inet_ntop(AFINET, ia, buf, sizeof(buf)));
	else
		return ((char *)inetntop(AFINET, ia, buf, sizeof(buf)));
#endif	
}
