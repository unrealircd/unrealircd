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
 * Defining this will allow all ircops to see people in +s channels
 * By default, only net/tech admins can see this
 */
#define SHOW_SECRET

/*
 * This allows you to see modes in /list
*/
#define LIST_SHOW_MODES

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
 * be compatible with older cloak keys? If you link to servers beta4 and 
 * earlier without this the cloak keys will produce diff results
 * Not recommended, however, as beta4 and earlier 3.2 has an insecure
 * cloak algo -griever
 */
#undef COMPAT_BETA4_KEYS

/*
  If you want SHUN_NOTICES, define this
*/
#undef SHUN_NOTICES

/*
   If you want to support chinese and/or japanese nicks
*/
#undef CHINESE_NICK
#undef JAPANESE_NICK

/*
  Remote rehash
*/
#define REMOTE_REHASH

/*
  Stripbadwords patch
*/
#define STRIPBADWORDS

/*
 * Always strip badwords in channels? (channel does not have to be +G)
*/
#undef STRIPBADWORDS_CHAN_ALWAYS

/*
 * NO_OPEROVERRIDE
 *   This will disable OperMode, OperTopic and Banwalks
*/
#undef NO_OPEROVERRIDE

/*
 * OPEROVERRIDE_VERIFY
 *   This will prompt opers before permitting them to join +p/+s
 *   channels, decreasing the chances of someone "accidentally"
 *   entering a random channel.
 */
#undef OPEROVERRIDE_VERIFY

/*
 * THROTTLING
 *   This will only allow 1 connection per ip in THROTTLING_PERIOD time
 *   this will be adaptable using conf later
*/
#undef THROTTLING
#define THROTTLING_PERIOD 15

/*
 * NAZIISH_CHBAN_HANDLING (formerly ANNOYING_BAN_THING)
 *   Reject bans that are matched by existing bans, causes chanserv
 *   To flood-kick an akicked user if their akick is matched by another
 *   Ban, but if you don't mind, this can free up ban list space I guess
 */
#undef NAZIISH_CHBAN_HANDLING

/*
 * Disable /sethost, /setident, /chgname, /chghost, /chgident
*/
#undef DISABLE_USERMOD

/*
  Ident checking
  #define this to disable ident checking
*/
#undef NO_IDENT_CHECKING

/*
 * No spoof code
 *
 * This enables the spoof protection.
 */
/* #define NOSPOOF 1  */


/*
 * Enables locops to override the RFC1459 flood control too
*/
#undef FAKE_LAG_FOR_LOCOPS

/*
 * HOSTILENAME - Define this if you want the hostile username patch included,
 *		 it will strip characters that are not 0-9,a-z,A-Z,_,- or .
 */
#define HOSTILENAME		/* */

/*
** Nick flood limit
** Minimum time between nick changes.
** (The first two changes are allowed quickly after another however).
**
** Define NICK_DELAY if you want this feature.
*/

#define NICK_DELAY 15		/* recommended value 15 */

/*
 * This makes topics include nick!user@host instead of nick in topic whoset, 
 * ALL servers must be Unreal3.2-beta12 or higher, and services may have some
 * problems with this
*/
#undef TOPIC_NICK_IS_NUHOST

/*
 * Use JOIN instead of SJOIN on every remotely sent JOIN
*/
#undef JOIN_INSTEAD_OF_SJOIN_ON_REMOTEJOIN

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
 * Define this if you wish to output a *file* to a K lined client rather
 * than the K line comment (the comment field is treated as a filename)
 */
#undef	COMMENT_IS_FILE

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

/* #undef	DEBUGMODE	   define DEBUGMODE to enable debugging mode.*/

/*
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the recommened names and paths. Change as needed.
 * You must define these to something, even if you don't really want them.
 */
#define	CPATH		"unrealircd.conf"	/* server configuration file */
#define	MPATH		"ircd.motd"	/* server MOTD file */
#define RPATH   	"ircd.rules"	/* server rules file */
#define OPATH   	"oper.motd"	/* Operators MOTD file */
#define	LPATH		"debug.log"	/* Where the debug file lives, if DEBUGMODE */
#define	PPATH		"ircd.pid"	/* file for server pid */
#define lPATH		"ircd.log"	/* server log file */
#define VPATH		"ircd.svsmotd"	/* Services MOTD append. */
#define BPATH		"bot.motd"	/* Bot MOTD */
#define IRCDTUNE 	"ircd.tune"	/* tuning .. */

/* CHROOTDIR
 *
 * Define for value added security if you are a rooter.
 *
 * All files you access must be in the directory you define as DPATH.
 * (This may effect the PATH locations above, though you can symlink it)
 *
 * You may want to define IRC_UID and IRC_GID
 */
/* #define CHROOTDIR    */

/* SHOW_INVISIBLE_LUSERS
 *
 * As defined this will show the correct invisible count for anyone who does
 * LUSERS on your server. On a large net this doesnt mean much, but on a
 * small net it might be an advantage to undefine it.
 * (This will get defined for you if you're using userload (stats w).  -mlv)
 */
#define	SHOW_INVISIBLE_LUSERS

/* MAXIMUM LINKS
 *
 * This define is useful for leaf nodes and gateways. It keeps you from
 * connecting to too many places. It works by keeping you from
 * connecting to more than "n" nodes which you have C:blah::blah:6667
 * lines for.
 *
 * Note that any number of nodes can still connect to you. This only
 * limits the number that you actively reach out to connect to.
 *
 * Leaf nodes are nodes which are on the edge of the tree. If you want
 * to have a backup link, then sometimes you end up connected to both
 * your primary and backup, routing traffic between them. To prevent
 * this, #define MAXIMUM_LINKS 1 and set up both primary and
 * secondary with C:blah::blah:6667 lines. THEY SHOULD NOT TRY TO
 * CONNECT TO YOU, YOU SHOULD CONNECT TO THEM.
 *
 * Gateways such as the server which connects Australia to the US can
 * do a similar thing. Put the American nodes you want to connect to
 * in with C:blah::blah:6667 lines, and the Australian nodes with
 * C:blah::blah lines. Have the Americans put you in with C:blah::blah
 * lines. Then you will only connect to one of the Americans.
 *
 * This value is only used if you don't have server classes defined, and
 * a server is in class 0 (the default class if none is set).
 *
 */

#define MAXIMUM_LINKS 1

/*
 * NOTE: defining CMDLINE_CONFIG and installing ircd SUID or SGID is a MAJOR
 *       security problem - they can use the "-f" option to read any files
 *       that the 'new' access lets them. Note also that defining this is
 *       a major security hole if your ircd goes down and some other user
 *       starts up the server with a new conf file that has some extra
 *       O-lines. So don't use this unless you're debugging.
 */
#define	CMDLINE_CONFIG		/* allow conf-file to be specified on command line */

/*
 * Size of the LISTEN request.  Some machines handle this large
 * without problem, but not all.  It defaults to 5, but can be
 * raised if you know your machine handles it.
 */
#ifndef LISTEN_SIZE
#define LISTEN_SIZE 5
#endif

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
#define	BUFFERPOOL     (9 * MAXSENDQLENGTH)
#endif

/*
 * IRC_UID
 *
 * If you start the server as root but wish to have it run as another user,
 * define IRC_UID to that UID.  This should only be defined if you are running
 * as root and even then perhaps not.
 * use #define IRC_UID <uid>
 * and #define IRC_GID <gid>
 */
 
#undef	IRC_UID
#undef	IRC_GID 

/*
 * CLIENT_FLOOD
 *
 * this controls the number of bytes the server will allow a client to
 * send to the server without processing before disconnecting the client for
 * flooding it.  Values greater than 8000 make no difference to the server.
 */
#define	CLIENT_FLOOD	8000

/*
 * Define your network service names here.
 */
#define ChanServ "ChanServ"
#define MemoServ "MemoServ"
#define NickServ "NickServ"
#define OperServ "OperServ"
#define HelpServ "HelpServ"
#define StatServ "StatServ"
#define InfoServ "InfoServ"
#define BotServ "BotServ"
/*
 * How many seconds in between simultaneous nick changes?
 */
#define NICK_CHANGE_DELAY	30

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
 * Maximum number of network connections your server will allow.  This should
 * never exceed max. number of open file descrpitors and wont increase this.
 * Should remain LOW as possible. Most sites will usually have under 30 or so
 * connections. A busy hub or server may need this to be as high as 50 or 60.
 * Making it over 100 decreases any performance boost gained from it being low.
 * if you have a lot of server connections, it may be worth splitting the load
 * over 2 or more servers.
 * 1 server = 1 connection, 1 user = 1 connection.
 * This should be at *least* 3: 1 listen port, 1 dns port + 1 client
 *
 * Note: this figure will be too high for most systems. If you get an
 * fd-related error on compile, change this to 256.
 *
 * Windows users: This should be a fairly high number.  Some operations
 * will slow down because of this, but it is _required_ because of the way
 * windows NT(and possibly 95) allocate fd handles. A good number is 16384.
 */
#ifndef MAXCONNECTIONS
#define MAXCONNECTIONS	1024
#endif

/*
 * this defines the length of the nickname history.  each time a user changes
 * nickname or signs off, their old nickname is added to the top of the list.
 * The following sizes are recommended:
 * 8MB or less  core memory : 500	(at least 1/4 of max users)
 * 8MB-16MB     core memory : 500-750	(1/4 -> 1/2 of max users)
 * 16MB-32MB    core memory : 750-1000	(1/2 -> 3/4 of max users)
 * 32MB or more core memory : 1000+	(> 3/4 if max users)
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
 * PINGFREQUENCY and CONNECTFREQUENCY
 */
#define TIMESEC  60		/* Recommended value: 60 */

/*
 * If daemon doesn't receive anything from any of its links within
 * PINGFREQUENCY seconds, then the server will attempt to check for
 * an active link with a PING message. If no reply is received within
 * (PINGFREQUENCY * 2) seconds, then the connection will be closed.
 */
#define PINGFREQUENCY    120	/* Recommended value: 120 */

/*
 * If the connection to to uphost is down, then attempt to reconnect every
 * CONNECTFREQUENCY  seconds.
 */
#define CONNECTFREQUENCY 600	/* Recommended value: 600 */

/*
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 */
#define HANGONRETRYDELAY 20	/* Recommended value: 20 seconds */
#define HANGONGOODLINK 300	/* Recommended value: 5 minutes */

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
 * Use much faster badwords replace routine?
 * Change #undef to #define to enable
 */
#undef FAST_BADWORD_REPLACE

/*
 * Enable zipped links? [EXPERIMENTAL]
 */
#undef ZIP_LINKS
/* If you use ziplinks, you can define the compression level here,
 * higher=better compressed but more CPU time, can be 1-9, but 1-4 is suggested.
 */
#define ZIP_LEVEL 2

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define RULES RPATH
#define	MYNAME SPATH
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PPATH

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
#ifdef HAVE_CRYPT
#define AUTHENABLE_UNIXCRYPT
#endif
#if defined(AIX) && defined(_XOPEN_SOURCE_EXTENDED) && _XOPEN_SOURCE_EXTENDED
# define SOCK_LEN_TYPE size_t
#else
# define SOCK_LEN_TYPE int
#endif

#endif				/* __config_include__ */

