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


#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)s_debug.c	2.30 1/3/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include <string.h>
#include "proto.h"
/*
 * Option string.  Must be before #ifdef DEBUGMODE.
 */
char serveropts[] = {
#ifdef	CHROOTDIR
	'c',
#endif
#ifdef	CMDLINE_CONFIG
	'C',
#endif
#ifdef	DO_ID
	'd',
#endif
#ifdef	DEBUGMODE
	'D',
#endif
#ifndef	NO_FDLIST
	'F',
#endif
#ifdef	HUB
	'h',
#endif
#ifdef	SHOW_INVISIBLE_LUSERS
	'i',
#endif
#ifndef	NO_DEFAULT_INVISIBLE
	'I',
#endif
#ifdef NOSPOOF
	'n',
#endif
#ifdef	NPATH
	'N',
#endif
#ifdef	ENABLE_USERS
	'U',
#endif
#ifdef	VALLOC
	'V',
#endif
#ifdef	_WIN32
	'W',
#endif
#ifdef	USE_SYSLOG
	'Y',
#endif
#ifdef OPER_NO_HIDING
	'H',
#endif
#ifdef NO_IDENT_CHECKING
	'K',
#endif
#ifdef INET6
	'6',
#endif
#ifdef STRIPBADWORDS
	'X',
#endif
#ifdef USE_POLL
	'P',
#endif
#ifdef USE_SSL
	'e',
#endif
#ifndef NO_OPEROVERRIDE
	'O',
#endif
#ifndef OPEROVERRIDE_VERIFY
	'o',
#endif
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

void	flag_add(char *ch)
{
	char *newextra;
	if (extraflags)
	{
		newextra = (char *)MyMalloc(strlen(extraflags) + 1 + strlen(ch));
		strcpy(newextra, extraflags);
		strcat(newextra, ch);
		MyFree(extraflags);
		extraflags = newextra;
	}
	else
		extraflags = (char *)strdup(ch);
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

static char debugbuf[1024];

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
		(void)ircsprintf(debugbuf, form, p1, p2, p3, p4, p5, p6, p7, p8, p9, p10);
#else
		(void)ircvsprintf(debugbuf, form, vl);
#if 0
# ifdef _WIN32
		strcat(debugbuf,"\r\n");
# endif
#endif
#endif

#if 0
		if (local[2])
		{
			local[2]->sendM++;
			local[2]->sendB += strlen(debugbuf);
		}
#endif
#ifndef _WIN32
		(void)fprintf(stderr, "%s", debugbuf);
		(void)fputc('\n', stderr);
#else
# ifndef _WIN32GUI
		Cio_Puts(hCio, debugbuf, strlen(debugbuf));
# else
		strcat(debugbuf, "\r\n");
		OutputDebugString(debugbuf);
# endif
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
#ifdef _SOLARIS
		extern char *sys_errlist[];
#endif
		sendto_one(cptr, ":%s NOTICE %s :Getruseage error: %s.",
		    me.name, nick, sys_errlist[errno]);
		return;
	}
	secs = rus.ru_utime.tv_sec + rus.ru_stime.tv_sec;
	rup = TStime() - me.since;
	if (secs == 0)
		secs = 1;

	sendto_one(cptr,
	    ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
	    me.name, RPL_STATSDEBUG, nick, secs / 60, secs % 60,
	    rus.ru_utime.tv_sec / 60, rus.ru_utime.tv_sec % 60,
	    rus.ru_stime.tv_sec / 60, rus.ru_stime.tv_sec % 60);
	sendto_one(cptr, ":%s %d %s :RSS %d ShMem %d Data %d Stack %d",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_maxrss,
	    rus.ru_ixrss / (rup * hzz), rus.ru_idrss / (rup * hzz),
	    rus.ru_isrss / (rup * hzz));
	sendto_one(cptr, ":%s %d %s :Swaps %d Reclaims %d Faults %d",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_nswap,
	    rus.ru_minflt, rus.ru_majflt);
	sendto_one(cptr, ":%s %d %s :Block in %d out %d",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_inblock, rus.ru_oublock);
	sendto_one(cptr, ":%s %d %s :Msg Rcv %d Send %d",
	    me.name, RPL_STATSDEBUG, nick, rus.ru_msgrcv, rus.ru_msgsnd);
	sendto_one(cptr, ":%s %d %s :Signals %d Context Vol. %d Invol %d",
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
#  ifndef _WIN32
		    me.name, RPL_STATSDEBUG, nick, strerror(errno));
#  else
		me.name, RPL_STATSDEBUG, nick, strerror(WSAGetLastError()));
#  endif
		return;
	}
	secs = tmsbuf.tms_utime + tmsbuf.tms_stime;

	sendto_one(cptr,
	    ":%s %d %s :CPU Secs %d:%d User %d:%d System %d:%d",
	    me.name, RPL_STATSDEBUG, nick, mins, secs, umin, usec, smin, ssec);
# endif
#endif
	sendto_one(cptr, ":%s %d %s :Reads %d Writes %d",
	    me.name, RPL_STATSDEBUG, nick, readcalls, writecalls);
	sendto_one(cptr, ":%s %d %s :DBUF alloc %d blocks %d",
	    me.name, RPL_STATSDEBUG, nick, dbufalloc, dbufblocks);
	sendto_one(cptr,
	    ":%s %d %s :Writes:  <0 %d 0 %d <16 %d <32 %d <64 %d",
	    me.name, RPL_STATSDEBUG, nick,
	    writeb[0], writeb[1], writeb[2], writeb[3], writeb[4]);
	sendto_one(cptr,
	    ":%s %d %s :<128 %d <256 %d <512 %d <1024 %d >1024 %d",
	    me.name, RPL_STATSDEBUG, nick,
	    writeb[5], writeb[6], writeb[7], writeb[8], writeb[9]);
	return;
}
#endif

void count_memory(aClient *cptr, char *nick)
{
	extern aChannel *channel;
	extern int flinks;
	extern Link *freelink;
	extern MemoryInfo StatsZ;

	aClient *acptr;
	Ban *ban;
	Link *link;
	aChannel *chptr;

	int  lc = 0,		/* local clients */
	     ch = 0,		/* channels */
	     lcc = 0,		/* local client conf links */
	     rc = 0,		/* remote clients */
	     us = 0,		/* user structs */
	     chu = 0,		/* channel users */
	     chi = 0,		/* channel invites */
	     chb = 0,		/* channel bans */
	     wwu = 0,		/* whowas users */
	     fl = 0,		/* free links */
	     cl = 0,		/* classes */
	     co = 0;		/* conf lines */

	int  usi = 0,		/* users invited */
	     usc = 0,		/* users in channels */
	     aw = 0,		/* aways set */
	     wwa = 0,		/* whowas aways */
	     wlh = 0,		/* watchlist headers */
	     wle = 0;		/* watchlist entries */

	u_long chm = 0,		/* memory used by channels */
	     chbm = 0,		/* memory used by channel bans */
	     lcm = 0,		/* memory used by local clients */
	     rcm = 0,		/* memory used by remote clients */
	     awm = 0,		/* memory used by aways */
	     wwam = 0,		/* whowas away memory used */
	     wwm = 0,		/* whowas array memory used */
	     com = 0,		/* memory used by conf lines */
	     wlhm = 0,		/* watchlist memory used */
	     db = 0,		/* memory used by dbufs */
	     rm = 0,		/* res memory used */
	     totcl = 0, totch = 0, totww = 0, tot = 0;

	count_whowas_memory(&wwu, &wwam);
	count_watch_memory(&wlh, &wlhm);
	wwm = sizeof(aName) * NICKNAMEHISTORYLENGTH;

	for (acptr = client; acptr; acptr = acptr->next)
	{
		if (MyConnect(acptr))
		{
			lc++;
			/*for (link = acptr->confs; link; link = link->next)
				lcc++;
			wle += acptr->notifies;*/
			
		}
		else
			rc++;
		if (acptr->user)
		{
			Membership *mb;
			us++;
			for (link = acptr->user->invited; link;
			    link = link->next)
				usi++;
			for (mb = acptr->user->channel; mb;
			    mb = mb->next)
				usc++;
			if (acptr->user->away)
			{
				aw++;
				awm += (strlen(acptr->user->away) + 1);
			}
		}
	}
	lcm = lc * CLIENT_LOCAL_SIZE;
	rcm = rc * CLIENT_REMOTE_SIZE;

	for (chptr = channel; chptr; chptr = chptr->nextch)
	{
		Member *member;
		
		ch++;
		chm += (strlen(chptr->chname) + sizeof(aChannel));
		for (member = chptr->members; member; member = member->next)
			chu++;
		for (link = chptr->invites; link; link = link->next)
			chi++;
		for (ban = chptr->banlist; ban; ban = ban->next)
		{
			chb++;
			chbm += (strlen(ban->banstr) + 1 +
			    strlen(ban->who) + 1 + sizeof(Ban));
		}
	}

/*	for (aconf = conf; aconf; aconf = aconf->next)
	{
		co++;
		com += aconf->host ? strlen(aconf->host) + 1 : 0;
		com += aconf->passwd ? strlen(aconf->passwd) + 1 : 0;
		com += aconf->name ? strlen(aconf->name) + 1 : 0;
		com += sizeof(aConfItem);
	}

	for (cltmp = classes; cltmp; cltmp = cltmp->next)
		cl++;
*/
	sendto_one(cptr, ":%s %d %s :Client Local %d(%d) Remote %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, lc, lcm, rc, rcm);
	sendto_one(cptr, ":%s %d %s :Users %d(%d) Invites %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, us, us * sizeof(anUser), usi,
	    usi * sizeof(Link));
	sendto_one(cptr, ":%s %d %s :User channels %d(%d) Aways %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, usc, usc * sizeof(Link), aw, awm);
	sendto_one(cptr, ":%s %d %s :WATCH headers %d(%d) entries %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, wlh, wlhm, wle, wle * sizeof(Link));
	sendto_one(cptr, ":%s %d %s :Attached confs %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, lcc, lcc * sizeof(Link));

	totcl = lcm + rcm + us * sizeof(anUser) + usc * sizeof(Link) + awm;
	totcl += lcc * sizeof(Link) + usi * sizeof(Link) + wlhm;
	totcl += wle * sizeof(Link);

	sendto_one(cptr, ":%s %d %s :Conflines %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, co, com);

	sendto_one(cptr, ":%s %d %s :Classes %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, StatsZ.classes, StatsZ.classesmem);

	sendto_one(cptr, ":%s %d %s :Channels %d(%d) Bans %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, ch, chm, chb, chbm);
	sendto_one(cptr, ":%s %d %s :Channel members %d(%d) invite %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, chu, chu * sizeof(Link),
	    chi, chi * sizeof(Link));

	totch = chm + chbm + chu * sizeof(Link) + chi * sizeof(Link);

	sendto_one(cptr, ":%s %d %s :Whowas users %d(%d) away %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, wwu, wwu * sizeof(anUser),
	    wwa, wwam);
	sendto_one(cptr, ":%s %d %s :Whowas array %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, NICKNAMEHISTORYLENGTH, wwm);

	totww = wwu * sizeof(anUser) + wwam + wwm;

	sendto_one(cptr,
	    ":%s %d %s :Hash: client %d(%d) chan %d(%d) watch %d(%d)", me.name,
	    RPL_STATSDEBUG, nick, U_MAX, sizeof(aHashEntry) * U_MAX, CH_MAX,
	    sizeof(aHashEntry) * CH_MAX, WATCHHASHSIZE,
	    sizeof(aWatch *) * WATCHHASHSIZE);
	db = dbufblocks * sizeof(dbufbuf);
	sendto_one(cptr, ":%s %d %s :Dbuf blocks %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, dbufblocks, db);

	link = freelink;
	while ((link = link->next))
		fl++;
	fl++;
	sendto_one(cptr, ":%s %d %s :Link blocks free %d(%d) total %d(%d)",
	    me.name, RPL_STATSDEBUG, nick, fl, fl * sizeof(Link),
	    flinks, flinks * sizeof(Link));

	rm = cres_mem(cptr,cptr->name);

	tot = totww + totch + totcl + com + cl * sizeof(aClass) + db + rm;
	tot += fl * sizeof(Link);
	tot += sizeof(aHashEntry) * U_MAX;
	tot += sizeof(aHashEntry) * CH_MAX;
	tot += sizeof(aWatch *) * WATCHHASHSIZE;

	sendto_one(cptr, ":%s %d %s :Total: ww %d ch %d cl %d co %d db %d",
	    me.name, RPL_STATSDEBUG, nick, totww, totch, totcl, com, db);
#if !defined(_WIN32) && !defined(_AMIGA)
#ifdef __alpha
	sendto_one(cptr, ":%s %d %s :TOTAL: %d sbrk(0)-etext: %u",
	    me.name, RPL_STATSDEBUG, nick, tot,
	    (u_int)sbrk((size_t)0) - (u_int)sbrk0);
#else
	sendto_one(cptr, ":%s %d %s :TOTAL: %d sbrk(0)-etext: %ul",
	    me.name, RPL_STATSDEBUG, nick, tot,
	    (u_long)sbrk((size_t)0) - (u_long)sbrk0);

#endif
#else
	sendto_one(cptr, ":%s %d %s :TOTAL: %d",
	    me.name, RPL_STATSDEBUG, nick, tot);
#endif
	return;
}
