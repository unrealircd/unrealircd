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
#include "settings.h"
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
 * To windows porters:
 *   You can specify name and url for their diff wircd sites
 *   #undef WIN32_SPECIFY for not having any notice about it in the wIRCd
 *    --Techie
 */
#undef WIN32_SPECIFY

#ifdef WIN32_SPECIFY
#define WIN32_PORTER ""
#define WIN32_URL ""
#endif

 /*
 * Define this if you're testing/debugging/programming.
 */
#undef DEBUG

/* Type of host. These should be made redundant somehow. -avalon */

/*	BSD			Nothing Needed 4.{2,3} BSD, SunOS 3.x, 4.x */
/*	HPUX			Nothing needed (A.08/A.09) */
/*	ULTRIX			Nothing needed (4.2) */
/*	OSF			Nothing needed (1.2) */
/* #undef	AIX		/* IBM ugly so-called Unix, AIX */
/* #undef	MIPS		/* MIPS Unix */
/*	SGI			Nothing needed (IRIX 4.0.4) */
/* #undef 	SVR3		/* SVR3 stuff - being worked on where poss. */
/* #undef	DYNIXPTX	/* Sequents Brain-dead Posix implement. */
/* #undef	SOL20		/* Solaris2 */
/* #undef	ESIX		/* ESIX */
/* #undef	NEXT		/* NeXTStep */
/* #undef	SVR4 /* */

/* Additional flags to give FreeBSD's malloc, only play with this if you
 * know what you're doing.
 */
 
#define MALLOC_FLAGS_EXTRA ""
/* 
   ConferenceRoom Java Client Hack -Fish
   if you want it to work #define CONFROOM_JAVA_PORT <port>
   where port MUST be a seperate port java clients connects on .. 
*/
#undef CONFROOM_JAVA_PORT

/*
    dog3/comstud ircd fdlists
    undef this to make them work
*/

#undef NO_FDLIST

/*
 * Admin's chat...
 */
#define ADMINCHAT 1


/*
  Remote rehash
*/
#define REMOTE_REHASH

/*
  Stripbadwords patch
*/
#define STRIPBADWORDS

/*
  NO_OPEROVERRIDE
    This will disable OperMode, OperTopic and Banwalks
*/
#undef NO_OPEROVERRIDE

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
/* #define NOSPOOF 1 /* */

/*
 *
 * This controls the "nospoof" system.  These numbers are "seeds" of the
 * "random" number generating formula.  Choose any number you like in the
 * range of 0x00000000 to 0xFFFFFFFF.  Don't tell anyone these numbers, and
 * don't use the default ones.  Change both #define NOSPOOF... lines below.
 *
 * Other data is mixed in as well, but these guarantee a per-server secret.
 * Also, these values need not remain constant over compilations...  Change
 * them as often as you like.
 */
#ifdef NOSPOOF

#ifndef NOSPOOF_SEED01
#define NOSPOOF_SEED01 0x12345678
#endif

#ifndef NOSPOOF_SEED02
#define NOSPOOF_SEED02 0x87654321
#endif

#endif /* NOSPOOF */

/*
 * HOSTILENAME - Define this if you want the hostile username patch included,
 *		 it will strip characters that are not 0-9,a-z,A-Z,_,- or .
 */
#define HOSTILENAME	/* */

/*
** Nick flood limit
** Minimum time between nick changes.
** (The first two changes are allowed quickly after another however).
**
** Define NICK_DELAY if you want this feature.
*/

#define NICK_DELAY 15                   /* recommended value 15 */

/*
** Freelinks garbage collector -Stskeeps
**
** GARBAGE_COLLECT_EVERY - how many seconds between every garbage collect
** HOW_MANY_FREELINKS_ALLOWED - how many freelinks allowed
*/
#ifndef GARBAGE_COLLECT_EVERY
#define GARBAGE_COLLECT_EVERY 		600 /* default: 600 (10 mins) */
#endif

#define HOW_MANY_FREELINKS_ALLOWED 	200 /* default: 200 */

/*
 * Define this if you wish to output a *file* to a K lined client rather
 * than the K line comment (the comment field is treated as a filename)
 */
#undef	COMMENT_IS_FILE


/* Do these work? I dunno... */

/* #undef	VMS		/* Should work for IRC client, not server */
/* #undef	MAIL50		/* If you're running VMS 5.0 */
/* #undef	PCS		/* PCS Cadmus MUNIX, use with BSD flag! */

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
#define	USE_VARARGS

 * NOTE: with current server code, varargs doesn't survive because it can't
 *       be used in a chain of 3 or more funtions which all have a variable
 *       number of params.  If anyone has a solution to this, please notify
 *       the maintainer.
 */

/* #undef	DEBUGMODE	/* define DEBUGMODE to enable debugging mode.*/

/*
 * defining FORCE_CORE will automatically "unlimit core", forcing the
 * server to dump a core file whenever it has a fatal error.  -mlv
 */
#define FORCE_CORE

/*
 * Full pathnames and defaults of irc system's support files. Please note that
 * these are only the recommened names and paths. Change as needed.
 * You must define these to something, even if you don't really want them.
 */
#define	CPATH		"ircd.conf"	/* server configuration file */
#define	MPATH		"ircd.motd"	/* server MOTD file */
#define RPATH   	"ircd.rules"    /* server rules file */
#define ZPATH		"ircd.notes"	/* server notes */
#define ZCONF   	"networks/unrealircd.conf" /* ircd configuration .. */
#define OPATH   	"oper.motd"     /* Operators MOTD file */
#define	LPATH		"debug.log"	/* Where the debug file lives, if DEBUGMODE */
#define	PPATH		"ircd.pid"	/* file for server pid */
#define lPATH		"ircd.log"	/* server log file */
#define VPATH		"ircd.svsmotd"  /* Services MOTD append. */
#define BPATH		"bot.motd"	/* Bot MOTD */
#define IRCDTUNE 	"ircd.tune" 	/* tuning .. */

/*
 * Define this filename to maintain a list of persons who log
 * into this server. Logging will stop when the file does not exist.
 * Logging will be disable also if you do not define this.
 * FNAME_USERLOG just logs user connections, FNAME_OPERLOG logs every
 * successful use of /oper.  These are either full paths or files within DPATH.
 */
#define FNAME_USERLOG "users.log"
#define FNAME_OPERLOG "opers.log"

/* FAILOPER_WARN
 *
 * When defined, warns users on a failed oper attempt that it was/is logged
 * Only works when FNAME_OPERLOG is defined, and a logfile exists.
 * NOTE: Failed oper attempts are logged regardless.
 */
#define FAILOPER_WARN

/* CHROOTDIR
 *
 * Define for value added security if you are a rooter.
 *
 * All files you access must be in the directory you define as DPATH.
 * (This may effect the PATH locations above, though you can symlink it)
 *
 * You may want to define IRC_UID and IRC_GID
 */
/* #define CHROOTDIR /* */

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
#define	CMDLINE_CONFIG /* allow conf-file to be specified on command line */

/*
 * If you wish to have the server send 'vital' messages about server
 * through syslog, define USE_SYSLOG. Only system errors and events critical
 * to the server are logged although if this is defined with FNAME_USERLOG,
 * syslog() is used instead of the above file. It is not recommended that
 * this option is used unless you tell the system administrator beforehand
 * and obtain their permission to send messages to the system log files.
 */
#ifndef _WIN32
#undef	USE_SYSLOG
#endif

#ifdef	USE_SYSLOG
/*
 * If you use syslog above, you may want to turn some (none) of the
 * spurious log messages for KILL/SQUIT off.
 */
#undef	SYSLOG_KILL	/* log all operator kills to syslog */
#undef  SYSLOG_SQUIT	/* log all remote squits for all servers to syslog */
#undef	SYSLOG_CONNECT	/* log remote connect messages for other all servs */
#undef	SYSLOG_USERS	/* send userlog stuff to syslog */
#undef	SYSLOG_OPER	/* log all users who successfully become an Op */

/*
 * If you want to log to a different facility than DAEMON, change
 * this define.
 */
#define LOG_FACILITY LOG_DAEMON
#endif /* USE_SYSLOG */

/*
 * IDLE_FROM_MSG
 *
 * Idle-time nullified only from privmsg, if undefined idle-time
 * is nullified from everything except ping/pong.
 * Added 3.8.1992, kny@cs.hut.fi (nam)
 */
#define IDLE_FROM_MSG

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
 */

/*
 * Ok this one is being changed. it is advisable never to run anything that
 * uses sockets etc. and has the potential for the outside world to connect to it 
 * to run as root... Hackers do things like buffer overruns, and get dumped on
 * a shell with root access effectivley ... so DONT do it.. if a program uses a
 * port <1024 it will run as root, once the program has binded to the socket it
 * will set its uid to something OTHER than root ... you set that in unrealircd.conf
 *
 * If you _must_ insist on running as root and not wanting the program to change its
 * UID, then define BIG_SECURITY_HOLE below
 */
#if !defined(_WIN32) && !defined(_AMIGA)
/* Change This Line Below \/ */ 
#define BIG_SECURITY_HOLE 
/* Its the one above ^^^^^^^ */
#ifndef BIG_SECUTIRY_HOLE
 #define	IRC_UID un_uid
 #define	IRC_GID un_gid
#endif
#endif

/*
 * CLIENT_FLOOD
 *
 * this controls the number of bytes the server will allow a client to
 * send to the server without processing before disconnecting the client for
 * flooding it.  Values greater than 8000 make no difference to the server.
 */
#define	CLIENT_FLOOD	8000

/* Define this if you want the server to accomplish ircII standard */
/* Sends an extra NOTICE in the beginning of client connection     */
#undef	IRCII_KLUDGE

/* 
 * Define your network service names here.
 */
#define ChanServ "ChanServ"
#define MemoServ "MemoServ"
#define NickServ "NickServ"
#define OperServ "OperServ"
#define HelpServ "HelpServ"
#define StatServ "StatServ"

/*
 * How many seconds in between simultaneous nick changes?
 */
#define NICK_CHANGE_DELAY	30

/*
 * How many open targets can one nick have for messaging nicks and
 * inviting them?
 */

#define MAXTARGETS		20
#define TARGET_DELAY		120

/* 
 * Would you like all clients to see the progress of their connections?
 */

#define SHOWCONNECTINFO

/*
 * SOCKS proxy checker
 *
 * At the moment this isn't an ideal solution, however it's better
 * than nothing. Smaller servers shouldn't notice much of a performance
 * hit, larger servers might have to reduce their Y-lines. In either
 * case it's advisable to increase the number of FD's you define by
 * about 10%.
 *
 * This determines the port on the local ircd machine that the open
 * SOCKS server test attempts to connect back to. The default should
 * be fine except for those unusual situations where the default
 * port is in use for some reason.
 *
 * Undefining this will eliminate the checker from ircd.
 */
#define SOCKSPORT 6013

/* Define default Z:line time for SOCKS   -taz */
#define ZLINE_TIME     300

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

/*
 * Port where ircd resides. NOTE: This *MUST* be greater than 1024 if you
 * plan to run ircd under any other uid than root.
 */
#define PORTNUM 6667 		/* 6667 is default */

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
#define	CONNECTTIMEOUT	60	/* Recommended value: 60 */

/*
 * Max time from the nickname change that still causes KILL
 * automaticly to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90   /* Recommended value: 90 */


/*
 * SendQ-Always causes the server to put all outbound data into the sendq and
 * flushing the sendq at the end of input processing. This should cause more
 * efficient write's to be made to the network.
 * There *shouldn't* be any problems with this method.
 * -avalon
 */
#define	SENDQ_ALWAYS

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define RULES RPATH
#define SNOTES ZPATH
#define	MYNAME SPATH
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PPATH
#define GLINE_LOG GPATH

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
extern	void	debug();
# define Debug(x) debug x
# define LOGFILE LPATH
#else
# define Debug(x) ;
# if VMS
#	define LOGFILE "NLA0:"
# else
#	define LOGFILE "/dev/null"
# endif
#endif

#ifndef ENABLE_SUMMON
#  undef LEAST_IDLE
#endif

#if defined(mips) || defined(PCS)
#undef SYSV
#endif

#ifdef MIPS
#undef BSD
#define BSD             1       /* mips only works in bsd43 environment */
#endif

#ifdef	BSD_RELIABLE_SIGNALS
# if defined(SYSV_UNRELIABLE_SIGNALS) || defined(POSIX_SIGNALS)
error You stuffed up config.h signals #defines use only one.
# endif
#define	HAVE_RELIABLE_SIGNALS
#endif

#ifdef	SYSV_UNRELIABLE_SIGNALS
# ifdef	POSIX_SIGNALS
error You stuffed up config.h signals #defines use only one.
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
#  if	(CLIENT_FLOOD > 8000)
#    define CLIENT_FLOOD 8000
#  else
#    if (CLIENT_FLOOD < 512)
error CLIENT_FLOOD needs redefining.
#    endif
#  endif
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
/* use cflag longmodes */
#define USE_LONGMODE
#define Reg1 register
#define Reg2 register
#define Reg3 register
#define Reg4 register
#define Reg5 register
#define Reg6 register
#define Reg7 register
#define Reg8 register
#define Reg9 register
#define Reg10 register

#endif /* __config_include__ */


