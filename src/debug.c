/*
 *   Unreal Internet Relay Chat Daemon, src/debug.c
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

/* debug.c 2.30 1/3/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"

MODVAR char serveropts[] = {
#ifdef	DEBUGMODE
	'D',
#endif
	/* FDLIST (always) */
	'F',
	/* Hub (always) */
	'h',
	/* NOSPOOF (always) */
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
	'\0', /* Don't change those nuls. -- Syzop */
	'\0',
	'\0',
	'\0',
	'\0'
};

char *extraflags = NULL;

MODVAR int debugfd = 2;

void	flag_add(char ch)
{
	char *newextra;
	if (extraflags)
	{
		char tmp[2] = { ch, 0 };
		newextra = MyMallocEx(strlen(extraflags) + 2);
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
	newflags = MyMallocEx(newsz + 1);
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

void debug(int level, FORMAT_STRING(const char *form), ...)
{
	int err = ERRNO;

	va_list vl;
	va_start(vl, form);

	if ((debuglevel >= 0) && (level <= debuglevel))
	{
		(void)ircvsnprintf(debugbuf, sizeof(debugbuf), form, vl);

#ifndef _WIN32
		strlcat(debugbuf, "\n", sizeof(debugbuf));
		if (write(debugfd, debugbuf, strlen(debugbuf)) < 0)
		{
			/* Yeah.. what can we do if output isn't working? Outputting an error makes no sense */
			;
		}
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
		sendnotice(cptr, "Getruseage error: %s.", strerror(errno));
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	rup = TStime() - me.local->since;
	if (secs == 0)
		secs = 1;

	sendnumericfmt(cptr, RPL_STATSDEBUG,
	    "CPU Secs %ld:%ld User %ld:%ld System %ld:%ld",
	    secs / 60, secs % 60,
	    rus.ru_utime.tv_sec / 60, rus.ru_utime.tv_sec % 60,
	    rus.ru_stime.tv_sec / 60, rus.ru_stime.tv_sec % 60);
	sendnumericfmt(cptr, RPL_STATSDEBUG, "RSS %ld ShMem %ld Data %ld Stack %ld",
	    rus.ru_maxrss,
	    rus.ru_ixrss / (rup * hzz), rus.ru_idrss / (rup * hzz),
	    rus.ru_isrss / (rup * hzz));
	sendnumericfmt(cptr, RPL_STATSDEBUG, "Swaps %ld Reclaims %ld Faults %ld",
	    rus.ru_nswap, rus.ru_minflt, rus.ru_majflt);
	sendnumericfmt(cptr, RPL_STATSDEBUG, "Block in %ld out %ld",
	    rus.ru_inblock, rus.ru_oublock);
	sendnumericfmt(cptr, RPL_STATSDEBUG, "Msg Rcv %ld Send %ld",
	    rus.ru_msgrcv, rus.ru_msgsnd);
	sendnumericfmt(cptr, RPL_STATSDEBUG, "Signals %ld Context Vol. %ld Invol %ld",
	    rus.ru_nsignals, rus.ru_nvcsw, rus.ru_nivcsw);
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
		sendnumericfmt(cptr, RPL_STATSDEBUG, "times(2) error: %s.", STRERROR(ERRNO));
		return;
	}
	secs = tmsbuf.tms_utime + tmsbuf.tms_stime;

	sendnumericfmt(cptr, RPL_STATSDEBUG,
	    "CPU Secs %d:%d User %d:%d System %d:%d",
	    mins, secs, umin, usec, smin, ssec);
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