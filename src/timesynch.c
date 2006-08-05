/************************************************************************
 * IRC - Internet Relay Chat, timesynch.c
 * (C) 2006 Bram Matthys (Syzop)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"

#if !defined(UNREAL_VERSION_TIME)
 #error "YOU MUST RUN ./Config WHENEVER YOU ARE UPGRADING UNREAL!!!!"
#endif

#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/socket.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "inet.h"

/* seconds from midnight Jan 1900 - 1970 */
#if __STDC__
#define TSDIFFERENCE 2208988800UL
#else
#define TSDIFFERENCE 2208988800
#endif

/* Maximum timeservers (which are sent in parallel).
 * Currently defaults to 4, which is rightfully "low", since we don't
 * want users to specify like 20 and flood the poor internet time servers.
 */
#define MAXTIMESERVERS 4

#define TSPKTLEN 48

static char tserr[512];

char *unreal_time_synch_error(void)
{
	return tserr;
}

/* our "secret cookie". This is sent to the time server(s) and we check it
 * in the response(s) we get. This is to make sure the reply is actually from
 * the server we sent the request to, and not some kind of spoofidiot...
 */
static char tscookie[4];

char *ts_buildpacket(void)
{
static char tspkt[TSPKTLEN];
u_int32_t current;

	memset(tspkt, 0, sizeof(tspkt));
	/* bit 0-1: LI (00)
	 * bit 2-4: version number (4, which is: 100)
	 * bit 5-7: client mode (3, which is: 011)
	 *
	 * This comes down to 00100011, which is 0x23
	 */
	tspkt[0] = 0x23;
	/* rest is zero... */
	
	/* Except the current timestamp: */
	current = htonl((u_int32_t)time(NULL));
	memcpy(tspkt+40, &current, 4);

	/* Our cookie... */
	tscookie[0] = getrandom8();
	tscookie[1] = getrandom8();
	tscookie[2] = getrandom8();
	tscookie[3] = getrandom8();
	memcpy(tspkt+44, tscookie, 4);
	
	return tspkt;
}

time_t extracttime(const char *buf)
{
unsigned long v;

	

	/* Verify cookie in originate timestamp */
	if (memcmp(buf+28, tscookie, 4))
	{
		/* COOKIE MISMATCH !? */
		ircd_log(LOG_ERROR, "TimeSynch: Received invalid cookie from time server, this might indicate a security problem, or some faulty NTP implementation.");
		return 0;
	}


	v = ((unsigned long)buf[40] << 24) | \
	    ((unsigned long)buf[41] << 16) | \
	    ((unsigned long)buf[42] << 8) | \
	    ((unsigned long)buf[43]);

	if (v == 0)
	{
		ircd_log(LOG_ERROR, "TimeSynch: Zero timestamp received");
		return 0;
	}

	v -= TSDIFFERENCE;
	
	if (v < 1147900561)
	{
		ircd_log(LOG_ERROR, "TimeSynch: Received timestamp in the (very) past from time server (ts=%lu)", v);
		return 0;
	}
	
	return (time_t)v;
}



int unreal_time_synch(int timeout)
{
int reply = 0;
char tmptimeservbuf[512], *p, *servname;
int s[MAXTIMESERVERS];
int numservers = 0;
time_t start, now, t, offset;
struct sockaddr_in addr[MAXTIMESERVERS];
int n, addrlen, i, highestfd = 0;
fd_set r;
struct timeval tv;
char buf[512], *buf_out;

	strlcpy(tmptimeservbuf, TIMESYNCH_SERVER, sizeof(tmptimeservbuf));

	n = 0;
	for (servname = strtoken(&p, tmptimeservbuf, ","); servname; servname = strtoken(&p, NULL, ","))
	{
		s[n] = socket(AF_INET, SOCK_DGRAM, 0); /* always ipv4 */
		if (s[n] < 0)
		{
			ircsprintf(tserr, "unable to create socket: %s [%d]", STRERROR(ERRNO), (int)ERRNO);
			goto end;
		}
		
		highestfd = (highestfd < s[n]) ? s[n] : highestfd; /* adjust highestfd if needed */

		memset(&addr[n], 0, sizeof(addr[n]));
		addr[n].sin_family = AF_INET;
		addr[n].sin_port = htons(123);
		addr[n].sin_addr.s_addr = inet_addr(servname);
		if (addr[n].sin_addr.s_addr == INADDR_NONE)
		{
			ircsprintf(tserr, "invalid timeserver IP '%s'", servname);
			goto end;
		}

		set_non_blocking(s[n], &me);
		n++;
	}
	numservers = n;
	
	buf_out = ts_buildpacket();
	
	/* Prepared... mass send.. */
	for (i = 0; i < numservers; i++)
	{
		n = sendto(s[i], buf_out, TSPKTLEN, 0, (struct sockaddr *)&addr[i], sizeof(struct sockaddr_in));
		if (n < 0)
			ircd_log(LOG_ERROR, "TimeSync: WARNING: Was unable to send message to server #%d...", i);
	}

	

	start = time(NULL); /* yes, indeed.. not TStime() here.. that is correct */
	
	while(1)
	{
		now = time(NULL);
		if (start + timeout <= now)
		{
			strcpy(tserr, "Timeout");
			goto end;
		}
		
		FD_ZERO(&r);
		for (i = 0; i < numservers; i++)
			FD_SET(s[i], &r);

		memset(&tv, 0, sizeof(tv));

		tv.tv_sec = timeout - (now - start);
	
		n = select(highestfd+1, &r, NULL, NULL, &tv);
		if (n < 0)
		{
			/* select error == teh bad.. */
			ircsprintf(tserr, "select() error: %s [%d]", STRERROR(ERRNO), (int)ERRNO);
			goto end;
		}
		
		for (i = 0; i < numservers; i++)
		{
			if (FD_ISSET(s[i], &r))
			{
				n = recv(s[i], buf, sizeof(buf), 0);
#ifndef _WIN32
				if ((n < 0) && (errno != EINPROGRESS))
#else
				if ((n < 0) &&
				    (WSAGetLastError() != WSAEINPROGRESS) &&
				    (WSAGetLastError() != WSAEWOULDBLOCK))
#endif
				{
					/* Some kind of error.. uh ok... */
					continue;
				}

				/* succes... check size */
				if (n >= 48)
				{
					t = extracttime(buf);
					if (t != 0)
						goto gotit;
				} else {
					ircd_log(LOG_ERROR, "TimeSynch: Odd short packet from time server received (len=%d)", n);
				}
			}
		} 
	} /* while */

gotit:
	/* Got time */
	now = time(NULL);
	offset = t - now;

	ircd_log(LOG_ERROR, "TIME SYNCH: timeserver=%ld, our=%ld, offset = %ld [old offset: %ld]\n",
		(long)t, (long)now, (long)offset, (long)TSoffset);
	
	TSoffset = offset;

	reply = 1;

end:
	for (i = 0; i < numservers; i++)
		CLOSE_SOCK(s[i]);

	return reply;
}
