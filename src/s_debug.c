/*
 *   Unreal Internet Relay Chat Daemon, src/s_debug.c
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

/* s_debug.c 2.30 1/3/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "struct.h"
#include <string.h>
#include "proto.h"
/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 */
MODVAR char serveropts[] = {
#ifdef	CHROOTDIR
	'c',
#endif
#ifdef	CMDLINE_CONFIG
	'C',
#endif
#ifdef	DEBUGMODE
	'D',
#endif
	/* FDLIST is always compiled in now.  Well, my replacement to it
	 * is anyway.  --nenolod
	 */
	'F',
	/*
	 * Marks that the ircd is ``compiled as'' a hub.
	 * Now always defined as it's impossible to build unrealircd
	 * without hub support AFAIK. (#0003891) --binki
	 */
	'h',
#ifdef	SHOW_INVISIBLE_LUSERS
	'i',
#endif
	/* NOSPOOF is always compiled in now. --nenolod */
	'n',
#ifdef	VALLOC
	'V',
#endif
#ifdef	_WIN32
	'W',
#endif
#ifdef	USE_SYSLOG
	'Y',
#endif
	'6',
#ifdef USE_SSL
	'e',
#endif
#ifndef NO_OPEROVERRIDE
	'O',
#endif
#ifndef OPEROVERRIDE_VERIFY
	'o',
#endif
	'E',
#ifdef USE_LIBCURL
	'r',
#endif
	'\0', /* Don't change those 3 nuls. -- Syzop */
	'\0',
	'\0'
};

char *extraflags = NULL;

#include "numeric.h"
#include "common.h"
#include "sys.h"
#include "whowas.h"
#include "hash.h"
#ifndef _WIN32
#include <sys/file.h>
#endif
#ifdef HPUX
#include <fcntl.h>
#endif
#if !defined(ULTRIX) && !defined(SGI) && \
    !defined(__convex__) && !defined(_WIN32)
# include <sys/param.h>
#endif
#ifdef HPUX
# include <sys/syscall.h>
# define getrusage(a,b) syscall(SYS_GETRUSAGE, a, b)
#endif
#ifdef GETRUSAGE_2
# ifdef _SOLARIS
#  include <sys/time.h>
#  ifdef RUSAGEH
#   include <sys/rusage.h>
#  endif
# endif
# include <sys/resource.h>
#else
#  ifdef TIMES_2
#   include <sys/times.h>
#  endif
#endif
#ifdef PCS
# include <time.h>
#endif
#ifdef HPUX
#include <unistd.h>
#endif
#include "h.h"

#ifndef ssize_t
#define ssize_t unsigned int
#endif

MODVAR int debugfd = 2;

void	flag_add(char ch)
{
	char *newextra;
	if (extraflags)
	{
		char tmp[2] = { ch, 0 };
		newextra = (char *)MyMalloc(strlen(extraflags) + 2);
		strcpy(newextra, extraflags);
		strcat(newextra, tmp);
		MyFree(extraflags);
		extraflags = newextra;
	}
	else
	{
		extraflags = malloc(2);
		extraflags[0] = ch;
		extraflags[1] = 0;
	}
}

void	flag_del(char ch)
{
	int newsz;
	char *p, *op;
	char *newflags;
	newsz = 0;	
	p = extraflags;
	for (newsz = 0, p = extraflags; *p; p++)
		if (*p != ch)
			newsz++;
	newflags = MyMalloc(newsz + 1);
	for (p = newflags, op = extraflags; (*op) && (newsz); newsz--, op++)
		if (*op != ch)
			*p++ = *op;
	*p = '\0';
	MyFree(extraflags);
	extraflags = newflags;
}



#ifdef DEBUGMODE
#ifndef _WIN32
#define SET_ERRNO(x) errno = x
#else
#define SET_ERRNO(x) WSASetLastError(x)
#endif /* _WIN32 */

static char debugbuf[4096];

#ifndef	USE_VARARGS
/*VARARGS2*/
void debug(level, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10)
	int  level;
	char *form, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10;
{
#else
void debug(int level, char *form, ...)
#endif
{
	int err = ERRNO;

	va_list vl;
	va_start(vl, form);

	if ((debuglevel >= 0) && (level <= debuglevel))
	{
#ifndef USE_VARARGS
		(void)ircsnprintf(debugbuf, sizeof(debugbuf), form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
		(void)ircvsnprintf(debugbuf, sizeof(debugbuf), form, vl);
#endif

#ifndef _WIN32
		strlcat(debugbuf, "\n", sizeof(debugbuf));
		write(debugfd, debugbuf, strlen(debugbuf));
#else
		strlcat(debugbuf, "\r\n", sizeof(debugbuf));
		OutputDebugString(debugbuf);
#endif
	}
	va_end(vl);
	SET_ERRNO(err);
}

/*
 * This is part of the STATS replies. There is no offical numeric for this
 * since this isnt an official command, in much the same way as HASH isnt.
 * It is also possible that some systems wont support this call or have
 * different field names for "struct rusage".
 * -avalon
 */
void send_usage(aClient *cptr, char *nick)
{

#ifdef GETRUSAGE_2
	struct rusage rus;
	time_t secs, rup;
#ifdef	hz
# define hzz hz
#else
# ifdef HZ
#  define hzz HZ
# else
	int  hzz = 1;
#  ifdef HPUX
	hzz = (int)sysconf(_SC_CLK_TCK);
#  endif
# endif
#endif

	if (getrusage(RUSAGE_SELF, &rus) == -1)
	{
		sendto_one(cptr, ":%s NOTICE %s :Getruseage error: %s.",
		    me.name, nick, strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	rup = TStime() - me.local->since;
	if (secs == 0)
		secs = 1;

	sendto_one(cptr,
	    ":%s %d %s :CPU Secs %ld:%ld User %ld:%ld System %ld:%ld",
	    me.name, RPL_STATSDEBUG, nick, secs / 60, secs % 60,
	    rus.ru_utime.tv_sec / 60, rus.ru_utime.tv_sec % 60,
	    rus.ru_stime.tv_sec / 60, rus.ru_stime.tv_sec % 60);
	sendto_one(cptr, ":%s %d %s :RSS %ld ShMem %ld Data %ld Stack %ld",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_maxrss,
	    rus.ru_ixrss / (rup * hzz), rus.ru_idrss / (rup * hzz),
	    rus.ru_isrss / (rup * hzz));
	sendto_one(cptr, ":%s %d %s :Swaps %ld Reclaims %ld Faults %ld",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_nswap,
	    rus.ru_minflt, rus.ru_majflt);
	sendto_one(cptr, ":%s %d %s :Block in %ld out %ld",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_inblock, rus.ru_oublock);
	sendto_one(cptr, ":%s %d %s :Msg Rcv %ld Send %ld",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_msgrcv, rus.ru_msgsnd);
	sendto_one(cptr, ":%s %d %s :Signals %ld Context Vol. %ld Invol %ld",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_nsignals,
	    rus.ru_nvcsw, rus.ru_nivcsw);
#else
# ifdef TIMES_2
	struct tms tmsbuf;
	time_t secs, mins;
	int  hzz = 1, ticpermin;
	int  umin, smin, usec, ssec;

#  ifdef HPUX
	hzz = sysconf(_SC_CLK_TCK);
#  endif
	ticpermin = hzz * 60;

	umin = tmsbuf.tms_utime / ticpermin;
	usec = (tmsbuf.tms_utime % ticpermin) / (float)hzz;
	smin = tmsbuf.tms_stime / ticpermin;
	ssec = (tmsbuf.tms_stime % ticpermin) / (float)hzz;
	secs = usec + ssec;
	mins = (secs / 60) + umin + smin;
	secs %= hzz;

	if (times(&tmsbuf) == -1)
	{
		sendto_one(cptr, ":%s %d %s :times(2) error: %s.",
		    me.name, RPL_STATSDEBUG, nick, STRERROR(ERRNO));
		return;
	}
	secs = tmsbuf.tms_utime + tmsbuf.tms_stime;

	sendto_one(cptr,
	    ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
	    me.name, RPL_STATSDEBUG, nick, mins, secs, umin, usec, smin, ssec);
# endif
#endif
	return;
}

int checkprotoflags(aClient *sptr, int flags, char *file, int line)
{
	if (!MyConnect(sptr))
		ircd_log(LOG_ERROR, "[Debug] [BUG] ERROR: %s:%d: IsToken(<%s>,%d) on remote client",
		         file, line, sptr->name, flags);
	return ((sptr->local->proto & flags) == flags) ? 1 : 0;
}
#endif
