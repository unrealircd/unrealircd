/*
 *   Unreal Internet Relay Chat Daemon, include/config.h
 *   Copyright (C) 1990 Jarkko Oikarinen
 *
 *   $Id$
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

#ifndef	__config_include__
#define	__config_include__

#include "setup.h"

/*
 *
 *   NOTICE
 *
 * Under normal conditions, you should not have to edit this file.  Run
 * the Config script in the root directory instead!
 *
 * Windows is not a normal condition, edit this file if you use it. :-)
 *
 *
 */

 /*
    * Define this if you're testing/debugging/programming.
#undef DEBUG
  */

/* Type of host. These should be made redundant somehow. -avalon */

/*	BSD			Nothing Needed 4.{2,3} BSD, SunOS 3.x, 4.x */
/*	HPUX			Nothing needed (A.08/A.09) */
/*	ULTRIX			Nothing needed (4.2) */
/*	OSF			Nothing needed (1.2) */
/* #undef	AIX		IBM ugly so-called Unix, AIX */
/* #undef	MIPS		MIPS Unix */
/*	SGI			Nothing needed (IRIX 4.0.4) */
/* #undef 	SVR3		SVR3 stuff - being worked on where poss. */
/* #undef	DYNIXPTX	Sequents Brain-dead Posix implement. */
/* #undef	SOL20		Solaris2 */
/* #undef	ESIX		ESIX */
/* #undef	NEXT		NeXTStep */
/* #undef	SVR4 */

/* Additional flags to give FreeBSD's malloc, only play with this if you
 * know what you're doing.
 */

#define MALLOC_FLAGS_EXTRA ""

/*
    dog3/comstud ircd fdlists
    undef this to make them work
*/

#undef NO_FDLIST

/*
 * So, the way this works is we determine using the preprocessor
 * what polling backend to use for the eventloop.  We prefer epoll,
 * followed by kqueue, followed by poll, and then finally select.
 * Kind of ugly, but it gets the job done.  You can also fiddle with
 * this to determine what backend is used.
 */
#ifndef _WIN32
# ifdef HAVE_EPOLL
#  define BACKEND_EPOLL
# else
#  ifdef HAVE_KQUEUE
#   define BACKEND_KQUEUE
#  else
#   ifdef HAVE_POLL
#    define BACKEND_POLL
#   else
#    define BACKEND_SELECT
#   endif
#  endif
# endif
#else
# define BACKEND_SELECT
#endif

/* Define the ircd module suffix, should be .so on UNIX, and .dll on Windows. */
#ifndef _WIN32
# define MODULE_SUFFIX	".so"
#else
# define MODULE_SUFFIX	".dll"
#endif

/*
 * Defining this will allow all ircops to see people in +s channels
 * By default, only net/tech admins can see this
 */
#define SHOW_SECRET

/*
 * Admin's chat...
 */
#define ADMINCHAT 1

/* 
 * If channel mode is +z, only send to secure links & people
 *
*/
#undef SECURECHANMSGSONLYGOTOSECURE

/*
   If you want to support chinese and/or japanese nicks
*/
#undef NICK_GB2312
#undef NICK_GBK
#undef NICK_GBK_JAP

/*
  Remote rehash
*/
#define REMOTE_REHASH

/*
 * Special remote include caching, see this Changelog item:
 * - Added special caching of remote includes. When a remote include fails to
 *   load (for example when the webserver is down), then the most recent
 *   version of that remote include will be used, and the ircd will still boot
 *   and be able to rehash. Even though this is quite a simple feature, it
 *   can make a key difference when deciding to roll out remote includes on
 *   your network. Previously, servers would be unable to boot or rehash when
 *   the webserver was down, which would be a big problem (often unacceptable).
 *   The latest version of fetched urls are cached in the cache/ directory as
 *   cache/<md5 hash of url>.
 *   Obviously, if there's no 'latest version' and an url fails, the ircd will
 *   still not be able to boot. This would be the case if you added or changed
 *   the path of a remote include and it's trying to fetch it for the first time.
 * There usually is no reason to disable this.
 */
#define REMOTEINC_SPECIALCACHE

/*
 * No spoof code
 *
 * This enables the spoof protection.
 */
/* #define NOSPOOF 1  */


/*
 * Enables locops to override the RFC1459 flood control too
*/
#undef NO_FAKE_LAG_FOR_LOCOPS

/*
 * HOSTILENAME - Define this if you want the hostile username patch included,
 *		 it will strip characters that are not 0-9,a-z,A-Z,_,- or .
 */
#define HOSTILENAME		/* [DO NOT CHANGE!] */

/*
 * So called 'smart' banning: if this is enabled and a ban on like *!*@*h.com is present,
 * then you cannot add a ban like *!*@*blah.com. In other words.. the ircd tries to be "smart".
 * In general this is considered quite annoying. This was on by default until Unreal 3.2.8.
 */
#undef SOCALLEDSMARTBANNING

/*
** Freelinks garbage collector -Stskeeps
**
** GARBAGE_COLLECT_EVERY - how many seconds between every garbage collect
** HOW_MANY_FREELINKS_ALLOWED - how many freelinks allowed
*/
#ifndef GARBAGE_COLLECT_EVERY
#define GARBAGE_COLLECT_EVERY 		600	/* default: 600 (10 mins) */
#endif

#define HOW_MANY_FREELINKS_ALLOWED 	200	/* default: 200 */

/*
 * MAXUNKNOWNCONNECTIONSPERIP
*/
#define MAXUNKNOWNCONNECTIONSPERIP 3


/* Do these work? I dunno... */

/* #undef	VMS		   Should work for IRC client, not server */
/* #undef	MAIL50		   If you're running VMS 5.0 */
/* #undef	PCS		   PCS Cadmus MUNIX, use with BSD flag! */

/*
 * NOTE: On some systems, valloc() causes many problems.
 */
#undef	VALLOC			/* Define this if you have valloc(3) */

/*
 * read/write are restarted after signals defining this 1, gets
 * siginterrupt call compiled, which attempts to remove this
 * behaviour (apollo sr10.1/bsd4.3 needs this)
 */
#ifdef APOLLO
#define	RESTARTING_SYSTEMCALLS
#endif

/*
 * If your host supports varargs and has vsprintf(), vprintf() and vscanf()
 * C calls in its library, then you can define USE_VARARGS to use varargs
 * instead of imitation variable arg passing.
*/
#define	USE_VARARGS

/* NOTE: with current server code, varargs doesn't survive because it can't
 *       be used in a chain of 3 or more funtions which all have a variable
 *       number of params.  If anyone has a solution to this, please notify
 *       the maintainer.
 */

/* DEBUGMODE: This should only be used when tracing a problem. It creates
 * an insane amount of log output which can be very useful for debugging.
 * You should *NEVER* enable this setting on production servers.
 */
/* #undef	DEBUGMODE */

/*
 * Full pathnames and defaults of irc system's support files.
 */
#define	CPATH		CONFDIR"/unrealircd.conf"	/* server configuration file */
#define	MPATH		CONFDIR"/ircd.motd"	/* server MOTD file */
#define SMPATH		CONFDIR"/ircd.smotd"    /* short MOTD file */
#define RPATH   	CONFDIR"/ircd.rules"	/* server rules file */
#define OPATH   	CONFDIR"/oper.motd"	/* Operators MOTD file */
#define	LPATH		LOGDIR"/debug.log"	/* Where the debug file lives, if DEBUGMODE */
#define VPATH		CONFDIR"/ircd.svsmotd"	/* Services MOTD append. */
#define BPATH		CONFDIR"/bot.motd"	/* Bot MOTD */
#define IRCDTUNE 	PERMDATADIR"/ircd.tune"	/* tuning .. */

/* CHROOTDIR
 *
 * This enables running the IRCd chrooted. Privileges will be dropped later
 * to IRC_USER/IRC_GROUP when those are defined.
 *
 * The directory to chroot to is simply DPATH (which is set via ./Config).
 * (This may effect the PATH locations above, though you can symlink it)
 *
 * If you want this, simple change this to '#define CHROOTDIR' and also
 * look at IRC_USER/IRC_GROUP a few lines below.
 * There's no need for you to create a special chroot environment;
 * UnrealIRCd will do that by itself (Unreal will create /dev/random, 
 * etc. etc.).
 */
/* #define CHROOTDIR    */

/*
 * IRC_USER
 *
 * If you start the server as root but wish to have it run as another user,
 * define IRC_USER to that user name.
 */
/* #define IRC_USER  "<user name>" */
/* #define IRC_GROUP "<group name>" */


/* SHOW_INVISIBLE_LUSERS
 *
 * As defined this will show the correct invisible count for anyone who does
 * LUSERS on your server. On a large net this doesnt mean much, but on a
 * small net it might be an advantage to undefine it.
 * (This will get defined for you if you're using userload (stats w).  -mlv)
 */
#define	SHOW_INVISIBLE_LUSERS

/*
 * NOTE: defining CMDLINE_CONFIG and installing ircd SUID or SGID is a MAJOR
 *       security problem - they can use the "-f" option to read any files
 *       that the 'new' access lets them. Note also that defining this is
 *       a major security hole if your ircd goes down and some other user
 *       starts up the server with a new conf file that has some extra
 *       O-lines.
 *       Naturally, for non-suid/sgid ircds, this setting does not matter,
 *       hence command line parameters are always permitted then.
 */
#undef	CMDLINE_CONFIG

/** FAKELAG_CONFIGURABLE makes it possible to make certain classes exempted
 * from 'fake lag' (that is, the artificial delay that is added by the ircd
 * to prevent flooding, which causes the messages/commands of the user to
 * slow down). Naturally, incorrect use of this feature can cause SEVERE
 * issues, in fact it can easily bring your whole IRCd down if one of the
 * users with class::options::nofakelag does a good flood at full speed.
 * Hence, this is disabled by default, and you need to explicitly enable it
 * here IF YOU KNOW WHAT YOU ARE DOING. People complaining their ircd
 * ""crashed"" because of this setting will be shot. </DISCLAIMER>
 * Common usage for this are: a trusted bot ran by an IRCOp, that you only
 * want to give "flood access" and nothing else, and other such things.
 */
#undef FAKELAG_CONFIGURABLE

/*
 * Max amount of internal send buffering when socket is stuck (bytes)
 */
#ifndef MAXSENDQLENGTH
#define MAXSENDQLENGTH 3000000
#endif
/*
 *  BUFFERPOOL is the maximum size of the total of all sendq's.
 *  Recommended value is 2 * MAXSENDQLENGTH, for hubs, 5 *.
 */
#ifndef BUFFERPOOL
#define	BUFFERPOOL     (18 * MAXSENDQLENGTH)
#endif

/*
 * CLIENT_FLOOD
 *
 * this controls the number of bytes the server will allow a client to
 * send to the server without processing before disconnecting the client for
 * flooding it.  Values greater than 8000 make no difference to the server.
 * NOTE: you can now also set this in class::recvq, if that's not present,
 *       this default value will be used.
 */
#define	CLIENT_FLOOD	8000

/* Anti-Flood options
 * NO_FLOOD_AWAY - enables limiting of how frequently a client can set /away
 */

#define NO_FLOOD_AWAY

/* You can define the nickname of NickServ here (usually "NickServ").
 * This is ONLY used for the ""infamous IDENTIFY feature"", which is:
 * whenever a user connects with a server password but there isn't
 * a server password set, the password is sent to NickServ in an
 * 'IDENTIFY <pass>' message.
 */
#define NickServ "NickServ"

/*
 * How many open targets can one nick have for messaging nicks and
 * inviting them?
 */

#define MAXTARGETS		20
#define TARGET_DELAY		15

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

/*
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other uid than root.
 */
#define PORTNUM 6667		/* 6667 is default */

/*
 * Maximum number of network connections your server will allow.
 * This is usually configured via ./Config on *NIX,
 * the setting mentioned below is the default for Windows.
 * 2004-10-13: 1024 -> 4096
 */
#ifndef MAXCONNECTIONS
#define MAXCONNECTIONS	4096
#endif

/*
 * this defines the length of the nickname history.  each time a user changes
 * nickname or signs off, their old nickname is added to the top of the list.
 * The following sizes are recommended:
 * 8MB or less  core memory : 500	(at least 1/4 of max users)
 * 8MB-16MB     core memory : 500-750	(1/4 -> 1/2 of max users)
 * 16MB-32MB    core memory : 750-1000	(1/2 -> 3/4 of max users)
 * 32MB or more core memory : 1000+	(> 3/4 of max users)
 * where max users is the expected maximum number of users.
 * (100 nicks/users ~ 25k)
 * NOTE: this is directly related to the amount of memory ircd will use whilst
 *	 resident and running - it hardly ever gets swapped to disk! You can
 *	 ignore these recommendations- they only are meant to serve as a guide
 * NOTE: But the *Minimum* ammount should be 100, in order to make nick
 *       chasing possible for mode and kick.
 */
#ifndef NICKNAMEHISTORYLENGTH
#define NICKNAMEHISTORYLENGTH 2000
#endif

/*
 * Time interval to wait and if no messages have been received, then check for
 * pings, outgoing connects, events, and a couple of other things.
 * Imo this is quite useless nowdays, it only saves _some_ cpu on tiny networks
 * with like 10 users all of them being inactive. On a normal network with >30
 * users this value is completely irrelevant.
 * The original value here was 60 (which was [hopefuly] never reached and was
 * stupid anyway), changed to 2.
 * DO NOT SET THIS TO ANYTHING MORE THAN 5. BETTER YET, JUST LEAVE IT AT 2!
 */
#define TIMESEC  2
/*
 * Maximum delay for socket loop (in miliseconds, so 1000 = 1 second). 
 * This means any other events and such may be delayed up to this value
 * when there is no socket data waiting for us (no clients sending anything).
 * Was 2000ms in 3.2.x, 1000ms for versions below 3.4-alpha4.
 */
#define SOCKETLOOP_MAX_DELAY 500

/*
 * If daemon doesn't receive anything from any of its links within
 * PINGFREQUENCY seconds, then the server will attempt to check for
 * an active link with a PING message. If no reply is received within
 * (PINGFREQUENCY * 2) seconds, then the connection will be closed.
 * NOTE: This is simply the class::pingfreq for the default class, nothing fancy ;)
 */
#define PINGFREQUENCY    120	/* Recommended value: 120 */

/*
 * Number of seconds to wait for write to complete if stuck.
 */
#define WRITEWAITDELAY     15	/* Recommended value: 15 */

/*
 * Number of seconds to wait for a connect(2) call to complete.
 * NOTE: this must be at *LEAST* 10.  When a client connects, it has
 * CONNECTTIMEOUT - 10 seconds for its host to respond to an ident lookup
 * query and for a DNS answer to be retrieved.
 */
#define	CONNECTTIMEOUT	30	/* Recommended value: 60 */

/*
 * Max time from the nickname change that still causes KILL
 * automaticly to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90	/* Recommended value: 90 */

/*
 * Forces Unreal to use compressed IPv6 addresses rather than expanding them
 */
#undef IPV6_COMPRESSED
  
/* Detect slow spamfilters? This requires a little more cpu time when processing
 * any spamfilter (like on text/connect/..) but will save you from slowing down
 * your IRCd to a near-halt (well, in most cases.. there are still cases like when
 * it goes into a loop that it will still stall completely... forever..).
 * This is kinda experimental, and requires getrusage.
 */
#ifndef _WIN32
#define SPAMFILTER_DETECTSLOW
#endif

/* Use TRE Regex Library (as well) ? */
#define USE_TRE

/* If EXPERIMENTAL is #define'd then all users will receive a notice about
 * this when they connect, along with a pointer to bugs.unrealircd.org where
 * they can report any problems. This is mainly to help UnrealIRCd development.
 */
#undef EXPERIMENTAL

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define RULES RPATH
#define	MYNAME BINDIR "/unrealircd"
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PIDFILE

#if defined(CHROOTDIR) && !defined(IRC_USER)
#error "ERROR: It makes no sense to define CHROOTDIR but not IRC_USER and IRC_GROUP! Please define IRC_USER and IRC_GROUP properly as the user/group to change to."
#endif

#ifdef	__osf__
#define	OSF
/* OSF defines BSD to be its version of BSD */
#undef BSD
#include <sys/param.h>
#ifndef BSD
#define BSD
#endif
#endif

#ifdef	ultrix
#define	ULTRIX
#endif

#ifdef	__hpux
#define	HPUX
#endif

#ifdef	sgi
#define	SGI
#endif

#ifndef KLINE_TEMP
#define KLINE_PERM 0
#define KLINE_TEMP 1
#define KLINE_AKILL 2
#define KLINE_EXCEPT 3
#endif

#ifdef DEBUGMODE
#ifndef _WIN32
		extern void debug(int, char *, ...);
#define Debug(x) debug x
#else
		extern void debug(int, char *, ...);
#define Debug(x) debug x
#endif
#define LOGFILE LPATH
#else
#define Debug(x) ;
#if VMS
#define LOGFILE "NLA0:"
#else
#define LOGFILE "/dev/null"
#endif
#endif


#if defined(mips) || defined(PCS)
#undef SYSV
#endif

#ifdef MIPS
#undef BSD
#define BSD             1	/* mips only works in bsd43 environment */
#endif

#ifdef	BSD_RELIABLE_SIGNALS
# if defined(SYSV_UNRELIABLE_SIGNALS) || defined(POSIX_SIGNALS)
error You stuffed up config.h signals
#define use only one.
# endif
#define	HAVE_RELIABLE_SIGNALS
#endif
#ifdef	SYSV_UNRELIABLE_SIGNALS
# ifdef	POSIX_SIGNALS
     error You stuffed up config.h signals
#define use only one.
# endif
#undef	HAVE_RELIABLE_SIGNALS
#endif
#ifdef	POSIX_SIGNALS
#define	HAVE_RELIABLE_SIGNALS
#endif
/*
 * safety margin so we can always have one spare fd, for motd/authd or
 * whatever else.  -4 allows "safety" margin of 1 and space reserved.
 */
#define	MAXCLIENTS	(MAXCONNECTIONS-4)
#ifdef HAVECURSES
# define DOCURSES
#else
# undef DOCURSES
#endif
#ifdef HAVETERMCAP
# define DOTERMCAP
#else
# undef DOTERMCAP
#endif
# define stricmp strcasecmp
# define strnicmp strncasecmp
#if defined(CLIENT_FLOOD)
#    if (CLIENT_FLOOD < 512)
     error CLIENT_FLOOD needs redefining.
#    endif
#else
     error CLIENT_FLOOD undefined
#endif
#if (NICKNAMEHISTORYLENGTH < 100)
#  define NICKNAMEHISTORYLENGTH 100
#endif
/*
 * Some ugliness for AIX platforms.
 */
#ifdef AIX
# include <sys/machine.h>
# if BYTE_ORDER == BIG_ENDIAN
#  define BIT_ZERO_ON_LEFT
# endif
# if BYTE_ORDER == LITTLE_ENDIAN
#  define BIT_ZERO_ON_RIGHT
# endif
/*
 * this one is used later in sys/types.h (or so i believe). -avalon
 */
# define BSD_INCLUDES
#endif
/*
 * This is just to make Solaris porting easier -- codemastr
 */
#if defined(SOL20) || defined(SOL25) || defined(SOL26) || defined(SOL27)
#define _SOLARIS
#endif
/*
 * Cleaup for WIN32 platform.
 */
#ifdef _WIN32
# undef FORCE_CORE
#endif
#ifdef NEED_BCMP
#define bcmp memcmp
#endif
#ifdef NEED_BCOPY
#define bcopy(a,b,c) memcpy(b,a,c)
#endif
#ifdef NEED_BZERO
#define bzero(a,b) memset(a,0,b)
#endif
#if defined(AIX) && defined(_XOPEN_SOURCE_EXTENDED) && _XOPEN_SOURCE_EXTENDED
# define SOCK_LEN_TYPE size_t
#else
# define SOCK_LEN_TYPE int
#endif

#if defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3)) && \
    !defined(__cplusplus)
 #define GCC_TYPECHECKING

 /* copied from cURL: */

 #define _UNREAL_WARNING(id, message) \
   static void __attribute__((__warning__(message))) \
   __attribute__((__unused__)) __attribute__((__noinline__)) \
   id(void) { __asm__(""); }

 #define _UNREAL_ERROR(id, message) \
   static void __attribute__((__error__(message))) \
   __attribute__((__unused__)) __attribute__((__noinline__)) \
   id(void) { __asm__(""); }
#endif

#endif				/* __config_include__ */

