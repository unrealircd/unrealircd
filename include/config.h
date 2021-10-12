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

/* IMPORTANT:
 * Under normal conditions, you should not have to edit this file.  Run
 * the ./Config script in the root directory instead!
 *
 * Windows is not a normal condition, edit this file if you use it. :-)
 */

/* Additional flags to give FreeBSD's malloc, only play with this if you
 * know what you're doing.
 */
#define MALLOC_FLAGS_EXTRA ""

/* I/O Engine: determine what method to use.
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

/* Permit remote /rehash */
#define REMOTE_REHASH

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
 * read/write are restarted after signals defining this 1, gets
 * siginterrupt call compiled, which attempts to remove this
 * behaviour (apollo sr10.1/bsd4.3 needs this)
 */
#ifdef APOLLO
#define	RESTARTING_SYSTEMCALLS
#endif

/* DEBUGMODE: This should only be used when tracing a problem. It creates
 * an insane amount of log output which can be very useful for debugging.
 * You should *NEVER* enable this setting on production servers.
 */
/* #undef	DEBUGMODE */

/* Similarly, DEBUG_IOENGINE can be used to debug the I/O engine. */
/* #undef	DEBUG_IOENGINE */

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
//#undef FAKELAG_CONFIGURABLE

/* The default value for class::sendq */
#define DEFAULT_SENDQ	3000000
/* The default value for class::recvq */
#define	DEFAULT_RECVQ	8000

/* You can define the nickname of NickServ here (usually "NickServ").
 * This is ONLY used for the ""infamous IDENTIFY feature"", which is:
 * whenever a user connects with a server password but there isn't
 * a server password set, the password is sent to NickServ in an
 * 'IDENTIFY <pass>' message.
 */
#define NickServ "NickServ"

/*   STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP STOP  */

/* You shouldn't change anything below this line, unless absolutely needed. */

/*
 * Maximum number of network connections your server will allow.
 * On *NIX this is configured via ./Config so don't set it here.
 * The setting below is the Windows (default) setting and isn't actually
 * the maximum number of network connections but the highest FD (File
 * Descriptor) that we can deal with.
 *
 * 2004-10-13: 1024 -> 4096
 */
#ifdef _WIN32
 #define MAXCONNECTIONS	10240
#else
 /* Non-Windows: */
 #if (!defined(MAXCONNECTIONS_REQUEST) || (MAXCONNECTIONS_REQUEST < 1)) && \
      (defined(HAVE_POLL) || defined(HAVE_EPOLL) || defined(HAVE_KQUEUE))
  /* Have poll/epoll/kqueue and either no --with-maxconnections or
   * --with-maxconnections=0, either of which indicates 'automatic' mode.
   * At the time of writing we will try a limit of 16384.
   * It will automatically be lowered at boottime if we can only use
   * 4096, 2048 or 1024. No problem.
   */
  #define MAXCONNECTIONS 16384
 #elif defined(MAXCONNECTIONS_REQUEST) && (MAXCONNECTIONS_REQUEST >= 1)
  /* --with-maxconnections=something */
  #define MAXCONNECTIONS MAXCONNECTIONS_REQUEST
 #else
  /* Automatic mode, but we only have select(). Bummer... */
  #define MAXCONNECTIONS 1024
 #endif
#endif

/* Number of file descriptors reserved for non-incoming-clients.
 * One of which may be used by auth, the rest are really reserved.
 * They can be used for outgoing server links, listeners, logging, etc.
 */
#if MAXCONNECTIONS > 1024
 #define CLIENTS_RESERVE 8
#else
 #define CLIENTS_RESERVE 4
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
 * Maximum delay for socket loop (in miliseconds, so 1000 = 1 second). 
 * This means any other events and such may be delayed up to this value
 * when there is no socket data waiting for us (no clients sending anything).
 * Was 2000ms in 3.2.x, 1000ms for versions below 3.4-alpha4.
 * 500ms in UnrealIRCd 4 (?)
 * 250ms in UnrealIRCd 5.
 */
#define SOCKETLOOP_MAX_DELAY 250

/*
 * Max time from the nickname change that still causes KILL
 * automaticly to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 30

/* Detect slow spamfilters? This requires a little more cpu time when processing
 * any spamfilter (like on text/connect/..) but will save you from slowing down
 * your IRCd to a near-halt (well, in most cases.. there are still cases like when
 * it goes into a loop that it will still stall completely... forever..).
 * This is kinda experimental, and requires getrusage.
 */
#ifndef _WIN32
#define SPAMFILTER_DETECTSLOW
#endif

/* Maximum number of ModData objects that may be attached to an object */
/* UnrealIRCd 4.0.0 - 4.0.13:  8,    8, 4, 4
 * UnrealIRCd 4.0.14+       : 12,    8, 4, 4
 * UnrealIRCd 5.0.0         : 12, 8, 8, 4, 4, 500, 500
 */
#define MODDATA_MAX_CLIENT		 12
#define MODDATA_MAX_LOCAL_CLIENT	  8
#define MODDATA_MAX_CHANNEL		  8
#define MODDATA_MAX_MEMBER		  4
#define MODDATA_MAX_MEMBERSHIP		  4
#define MODDATA_MAX_LOCAL_VARIABLE	500
#define MODDATA_MAX_GLOBAL_VARIABLE	500

/* If EXPERIMENTAL is #define'd then all users will receive a notice about
 * this when they connect, along with a pointer to bugs.unrealircd.org where
 * they can report any problems. This is mainly to help UnrealIRCd development.
 */
#undef EXPERIMENTAL

/* Default SSL/TLS cipherlist (except for TLS1.3, see further down).
 * This can be changed via set::ssl::options::ciphers in the config file.
 */
#define UNREALIRCD_DEFAULT_CIPHERS "TLS13-CHACHA20-POLY1305-SHA256 TLS13-AES-256-GCM-SHA384 TLS13-AES-128-GCM-SHA256 EECDH+CHACHA20 EECDH+AESGCM EECDH+AES AES256-GCM-SHA384 AES128-GCM-SHA256 AES256-SHA256 AES128-SHA256 AES256-SHA AES128-SHA"

/* Default TLS 1.3 ciphersuites.
 * This can be changed via set::ssl::options::ciphersuites in the config file.
 */
#define UNREALIRCD_DEFAULT_CIPHERSUITES "TLS_CHACHA20_POLY1305_SHA256:TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_AES_128_CCM_8_SHA256:TLS_AES_128_CCM_SHA256"

/* Default SSL/TLS curves for ECDH(E)
 * This can be changed via set::ssl::options::ecdh-curve in the config file.
 * NOTE: This requires openssl 1.0.2 or newer, otherwise these defaults
 *       are not applied, due to the missing openssl API call.
 */
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#define UNREALIRCD_DEFAULT_ECDH_CURVES "X25519:secp521r1:secp384r1:prime256v1"
#else
#define UNREALIRCD_DEFAULT_ECDH_CURVES "secp521r1:secp384r1:prime256v1"
#endif

/* ------------------------- END CONFIGURATION SECTION -------------------- */
#define MOTD MPATH
#define RULES RPATH
#define	MYNAME BINDIR "/unrealircd"
#define	CONFIGFILE CPATH
#define	IRCD_PIDFILE PIDFILE

#ifdef DEBUGMODE
 #define Debug(x) debug x
 #define LOGFILE LPATH
#else
 #define Debug(x) ;
 #define LOGFILE "/dev/null"
#endif

#if defined(DEFAULT_RECVQ)
#    if (DEFAULT_RECVQ < 512)
     error DEFAULT_RECVQ needs redefining.
#    endif
#else
     error DEFAULT_RECVQ undefined
#endif
#if (NICKNAMEHISTORYLENGTH < 100)
#  define NICKNAMEHISTORYLENGTH 100
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

#ifndef __has_feature
 #define __has_feature(x) 0  // Compatibility with non-clang compilers.
#endif
#ifndef __has_extension
 #define __has_extension __has_feature // Compatibility with pre-3.0 compilers.
#endif

#endif				/* __config_include__ */

