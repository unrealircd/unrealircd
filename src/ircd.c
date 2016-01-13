/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/ircd.c
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

/* ircd.c	2.48 3/9/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "mempool.h"
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/file.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>
#else
#include <io.h>
#include <direct.h>
#endif
#ifdef HPUX
#define _KERNEL			/* HPUX has the world's worst headers... */
#endif
#ifndef _WIN32
#include <sys/resource.h>
#endif
#ifdef HPUX
#undef _KERNEL
#endif
#include <errno.h>
#ifdef HAVE_PSSTRINGS
#include <sys/exec.h>
#endif
#ifdef HAVE_PSTAT
#include <sys/pstat.h>
#endif
#include "h.h"
#include "fdlist.h"
#include "version.h"
#include "proto.h"
#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif
ID_Copyright
    ("(C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen");
ID_Notes("2.48 3/9/94");
#ifdef __FreeBSD__
char *malloc_options = "h" MALLOC_FLAGS_EXTRA;
#endif
time_t TSoffset = 0;

#ifndef _WIN32
extern char unreallogo[];
#endif
int  SVSNOOP = 0;
extern MODVAR char *buildid;
time_t timeofday = 0;
int  tainted = 0;
LoopStruct loop;
MODVAR MemoryInfo StatsZ;
#ifndef _WIN32
uid_t irc_uid = 0;
gid_t irc_gid = 0;
#endif

int  R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns, R_do_id, R_fin_id, R_fail_id;

char REPORT_DO_DNS[256], REPORT_FIN_DNS[256], REPORT_FIN_DNSC[256],
    REPORT_FAIL_DNS[256], REPORT_DO_ID[256], REPORT_FIN_ID[256],
    REPORT_FAIL_ID[256];
ircstats IRCstats;
aClient me;			/* That's me */
MODVAR char *me_hash;
extern char backupbuf[8192];
#ifdef _WIN32
extern void CleanUpSegv(int sig);
extern SERVICE_STATUS_HANDLE IRCDStatusHandle;
extern SERVICE_STATUS IRCDStatus;
#endif

MODVAR unsigned char conf_debuglevel = 0;

#ifdef USE_LIBCURL
extern void url_init(void);
#endif

time_t highesttimeofday=0, oldtimeofday=0, lasthighwarn=0;


void save_stats(void)
{
	FILE *stats = fopen("ircd.stats", "w");
	if (!stats)
		return;
	fprintf(stats, "%i\n", IRCstats.clients);
	fprintf(stats, "%i\n", IRCstats.invisible);
	fprintf(stats, "%i\n", IRCstats.servers);
	fprintf(stats, "%i\n", IRCstats.operators);
	fprintf(stats, "%i\n", IRCstats.unknown);
	fprintf(stats, "%i\n", IRCstats.me_clients);
	fprintf(stats, "%i\n", IRCstats.me_servers);
	fprintf(stats, "%i\n", IRCstats.me_max);
	fprintf(stats, "%i\n", IRCstats.global_max);
	fclose(stats);
}


void server_reboot(char *);
void restart(char *);
static void open_debugfile(), setup_signals();
extern void init_glines(void);
extern void tkl_init(void);
extern void process_clients(void);

MODVAR TS   last_garbage_collect = 0;
#ifndef _WIN32
MODVAR char **myargv;
#else
LPCSTR cmdLine;
#endif
int  portnum = -1;		/* Server port number, listening this */
char *configfile = CONFIGFILE;	/* Server configuration file */
int  debuglevel = 0;		/* Server debug level */
int  bootopt = 0;		/* Server boot option flags */
char *debugmode = "";		/*  -"-    -"-   -"-  */
char *sbrk0;			/* initial sbrk(0) */
static int dorehash = 0, dorestart = 0, doreloadcert = 0;
MODVAR int  booted = FALSE;
MODVAR TS   lastlucheck = 0;

#ifdef UNREAL_DEBUG
#undef CHROOTDIR
#define CHROOT
#endif

MODVAR TS   NOW;
#if	defined(PROFIL) && !defined(_WIN32)
extern etext();

VOIDSIG s_monitor(void)
{
	static int mon = 0;
#ifdef	POSIX_SIGNALS
	struct sigaction act;
#endif

	(void)moncontrol(mon);
	mon = 1 - mon;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
	(void)signal(SIGUSR1, s_monitor);
#endif
}

#endif

VOIDSIG s_die()
{
#ifdef _WIN32
	aClient *cptr;
	if (!IsService)
	{
		unload_all_modules();

		list_for_each_entry(cptr, &lclient_list, lclient_node)
			(void) send_queued(cptr);

		exit(-1);
	}
	else {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP);
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
	}
#else
	unload_all_modules();
	unlink(conf_files ? conf_files->pid_file : IRCD_PIDFILE);
	exit(-1);
#endif
}

#ifndef _WIN32
static VOIDSIG s_rehash()
#else
VOIDSIG s_rehash()
#endif
{
#ifdef	POSIX_SIGNALS
	struct sigaction act;
#endif
	dorehash = 1;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
#else
# ifndef _WIN32
	(void)signal(SIGHUP, s_rehash);	/* sysV -argv */
# endif
#endif
}

#ifndef _WIN32
static VOIDSIG s_reloadcert()
{
#ifdef	POSIX_SIGNALS
	struct sigaction act;
#endif
	doreloadcert = 1;
#ifdef	POSIX_SIGNALS
	act.sa_handler = s_reloadcert;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
	(void)signal(SIGUSR1, s_reloadcert);	/* sysV -argv */
#endif
}
#endif

void restart(char *mesg)
{
	server_reboot(mesg);
}

VOIDSIG s_restart()
{
	dorestart = 1;
#if 0
	static int restarting = 0;

	if (restarting == 0) {
		/*
		 * Send (or attempt to) a dying scream to oper if present
		 */

		restarting = 1;
		server_reboot("SIGINT");
	}
#endif
}


#ifndef _WIN32
VOIDSIG dummy()
{
#ifndef HAVE_RELIABLE_SIGNALS
	(void)signal(SIGALRM, dummy);
	(void)signal(SIGPIPE, dummy);
#ifndef HPUX			/* Only 9k/800 series require this, but don't know how to.. */
# ifdef SIGWINCH
	(void)signal(SIGWINCH, dummy);
# endif
#endif
#else
# ifdef POSIX_SIGNALS
	struct sigaction act;

	act.sa_handler = dummy;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGALRM);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
#  ifdef SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
#  endif
	(void)sigaction(SIGALRM, &act, (struct sigaction *)NULL);
	(void)sigaction(SIGPIPE, &act, (struct sigaction *)NULL);
#  ifdef SIGWINCH
	(void)sigaction(SIGWINCH, &act, (struct sigaction *)NULL);
#  endif
# endif
#endif
}

#endif				/* _WIN32 */


void server_reboot(char *mesg)
{
	int i;
	aClient *cptr;
	sendto_realops("Aieeeee!!!  Restarting server... %s", mesg);
	Debug((DEBUG_NOTICE, "Restarting server... %s", mesg));

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		(void) send_queued(cptr);

	/*
	 * ** fd 0 must be 'preserved' if either the -d or -i options have
	 * ** been passed to us before restarting.
	 */
#ifdef HAVE_SYSLOG
	(void)closelog();
#endif
#ifndef _WIN32
	for (i = 3; i < MAXCONNECTIONS; i++)
		(void)close(i);
	if (!(bootopt & (BOOT_TTY | BOOT_DEBUG)))
		(void)close(2);
	(void)close(1);
	(void)close(0);
	(void)execv(MYNAME, myargv);
#else
	close_connections();
	if (!IsService)
	{
		CleanUp();
		WinExec(cmdLine, SW_SHOWDEFAULT);
	}
#endif
#ifndef _WIN32
	Debug((DEBUG_FATAL, "Couldn't restart server: %s", strerror(errno)));
#else
	Debug((DEBUG_FATAL, "Couldn't restart server: %s",
	    strerror(GetLastError())));
#endif
	unload_all_modules();
#ifdef _WIN32
	if (IsService)
	{
		SERVICE_STATUS status;
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		char fname[MAX_PATH];
		bzero(&status, sizeof(status));
		bzero(&si, sizeof(si));
		IRCDStatus.dwCurrentState = SERVICE_STOP_PENDING;
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus);
		GetModuleFileName(GetModuleHandle(NULL), fname, MAX_PATH);
		CreateProcess(fname, "restartsvc", NULL, NULL, FALSE,
			0, NULL, NULL, &si, &pi);
		IRCDStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(IRCDStatusHandle, &IRCDStatus);
		ExitProcess(0);
	}
	else
#endif
	exit(-1);
}

MODVAR char *areason;

EVENT(loop_event)
{
	if (loop.do_garbage_collect == 1) {
		garbage_collect(NULL);
	}
}

EVENT(garbage_collect)
{
	extern int freelinks;
	extern Link *freelink;
	Link p;
	int  ii;

	if (loop.do_garbage_collect == 1)
		sendto_realops("Doing garbage collection ..");
	if (freelinks > HOW_MANY_FREELINKS_ALLOWED) {
		ii = freelinks;
		while (freelink && (freelinks > HOW_MANY_FREELINKS_ALLOWED)) {
			freelinks--;
			p.next = freelink;
			freelink = freelink->next;
			MyFree(p.next);
		}
		if (loop.do_garbage_collect == 1) {
			loop.do_garbage_collect = 0;
			sendto_realops
			    ("Cleaned up %i garbage blocks", (ii - freelinks));
		}
	}
	if (loop.do_garbage_collect == 1)
		loop.do_garbage_collect = 0;
}

/*
** try_connections
**
**	Scan through configuration and try new connections.
**	Returns the calendar time when the next call to this
**	function should be made latest. (No harm done if this
**	is called earlier or later...)
*/
EVENT(try_connections)
{
	ConfigItem_link *aconf;
	ConfigItem_deny_link *deny;
	aClient *cptr;
	int  confrq;
	ConfigItem_class *class;

	for (aconf = conf_link; aconf; aconf = (ConfigItem_link *) aconf->next)
	{
		/* We're only interested in autoconnect blocks that are valid (and ignore temporary link blocks) */
		if (!(aconf->outgoing.options & CONNECT_AUTO) || !aconf->outgoing.hostname || (aconf->flag.temporary == 1))
			continue;

		class = aconf->class;

		/* Only do one connection attempt per <connfreq> seconds (for the same server) */
		if ((aconf->hold > TStime()))
			continue;
		confrq = class->connfreq;
		aconf->hold = TStime() + confrq;

		cptr = find_name(aconf->servername, NULL);
		if (cptr)
			continue; /* Server already connected (or connecting) */

		if (class->clients >= class->maxclients)
			continue; /* Class is full */

		/* Check connect rules to see if we're allowed to try the link */
		for (deny = conf_deny_link; deny; deny = deny->next)
			if (!match(deny->mask, aconf->servername) && crule_eval(deny->rule))
				break;

		if (!deny && connect_server(aconf, NULL, NULL) == 0)
			sendto_realops("Connection to %s[%s] activated.",
				aconf->servername, aconf->outgoing.hostname);

	}
}

int check_tkls(aClient *cptr)
{
	ConfigItem_ban *bconf = NULL;
	char banbuf[1024];

	char killflag = 0;

	/* Process dynamic *LINES */
	if (find_tkline_match(cptr, 0) < 0)
		return 0; /* stop processing this user, as (s)he is dead now. */

	find_shun(cptr); /* check for shunned and take action, if so */

	if (IsPerson(cptr))
	{
		/* Check ban user { } and ban realname { } */

		bconf = Find_ban(cptr, NULL, CONF_BAN_USER);
		if (bconf)
			killflag++;
		else if (!ValidatePermissionsForPath("immune",cptr,NULL,NULL,NULL) && (bconf = Find_ban(NULL, cptr->info, CONF_BAN_REALNAME)))
			killflag++;
	}

	/* If still no match, check ban ip { } */
	if (!killflag && (bconf = Find_ban(cptr, NULL, CONF_BAN_IP)))
		killflag++;

	/* If user is meant to be killed, take action: */
	if (killflag)
	{
		if (IsPerson(cptr))
			sendto_realops("Ban active for %s (%s)",
				get_client_name(cptr, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (IsServer(cptr))
			sendto_realops("Ban active for server %s (%s)",
				get_client_name(cptr, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (bconf->reason) {
			if (IsPerson(cptr))
				snprintf(banbuf, sizeof(banbuf), "User has been banned (%s)", bconf->reason);
			else
				snprintf(banbuf, sizeof(banbuf), "Banned (%s)", bconf->reason);
			(void)exit_client(cptr, cptr, &me, banbuf);
		} else {
			if (IsPerson(cptr))
				(void)exit_client(cptr, cptr, &me, "User has been banned");
			else
				(void)exit_client(cptr, cptr, &me, "Banned");
		}
		return 0; /* stop processing this user, as (s)he is dead now. */
	}

	if (loop.do_bancheck_spamf_user && IsPerson(cptr) && find_spamfilter_user(cptr, SPAMFLAG_NOWARN) == FLUSH_BUFFER)
		return 0;

	if (loop.do_bancheck_spamf_away && IsPerson(cptr) && cptr->user->away != NULL &&
		dospamfilter(cptr, cptr->user->away, SPAMF_AWAY, NULL, SPAMFLAG_NOWARN, NULL) == FLUSH_BUFFER)
		return 0;

	return 1;
}

/*
 * TODO:
 * This is really messy at the moment, but the k-line stuff is recurse-safe, so I removed it
 * a while back (see above).
 *
 * Other things that should likely go:
 *      - identd/dns timeout checking (should go to it's own event, idea here is that we just
 *        keep you in "unknown" state until you actually get 001, so we can cull the unknown list)
 *
 * No need to worry about server list vs lclient list because servers are on lclient.  There are
 * no good reasons for it not to be, considering that 95% of iterations of the lclient list apply
 * to both clients and servers.
 *      - nenolod
 */

/*
 * Check UNKNOWN connections - if they have been in this state
 * for more than CONNECTTIMEOUT seconds, close them.
 */
EVENT(check_unknowns)
{
	aClient *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &unknown_list, lclient_node)
	{
		if (cptr->local->firsttime && ((TStime() - cptr->local->firsttime) > CONNECTTIMEOUT))
		{
			(void)exit_client(cptr, cptr, &me, "Registration Timeout");
			continue;
		}
		if (DoingAuth(cptr) && ((TStime() - cptr->local->firsttime) > IDENT_CONNECT_TIMEOUT))
			ident_failed(cptr);
	}
}

/** Ping individual user, and check for ping timeout */
int check_ping(aClient *cptr)
{
	char scratch[64];
	int ping = 0;

	ping = cptr->local->class ? cptr->local->class->pingfreq : CONNECTTIMEOUT;
	Debug((DEBUG_DEBUG, "c(%s)=%d p %d a %d", cptr->name,
		cptr->status, ping,
		TStime() - cptr->local->lasttime));

	/* If ping is less than or equal to the last time we received a command from them */
	if (ping > (TStime() - cptr->local->lasttime))
		return 0; /* some recent command was executed */

	if (
		/* If we have sent a ping */
		((cptr->flags & FLAGS_PINGSENT)
		/* And they had 2x ping frequency to respond */
		&& ((TStime() - cptr->local->lasttime) >= (2 * ping)))
		||
		/* Or isn't registered and time spent is larger than ping (CONNECTTIMEOUT).. */
		(!IsRegistered(cptr) && (TStime() - cptr->local->since >= ping))
		)
	{
		if (IsServer(cptr) || IsConnecting(cptr) ||
			IsHandshake(cptr)
			|| IsSSLConnectHandshake(cptr)
			) {
			sendto_realops
				("No response from %s, closing link",
				get_client_name(cptr, FALSE));
			sendto_server(&me, 0, 0,
				":%s GLOBOPS :No response from %s, closing link",
				me.name, get_client_name(cptr,
				FALSE));
		}
		if (IsSSLAcceptHandshake(cptr))
			Debug((DEBUG_DEBUG, "ssl accept handshake timeout: %s (%li-%li > %li)", cptr->local->sockhost,
				TStime(), cptr->local->since, ping));
		(void)ircsnprintf(scratch, sizeof(scratch), "Ping timeout: %ld seconds",
			(long) (TStime() - cptr->local->lasttime));
		return exit_client(cptr, cptr, &me, scratch);
	}
	else if (IsRegistered(cptr) &&
		((cptr->flags & FLAGS_PINGSENT) == 0)) {
		/*
		 * if we havent PINGed the connection and we havent
		 * heard from it in a while, PING it to make sure
		 * it is still alive.
		 */
		cptr->flags |= FLAGS_PINGSENT;
		/*
		 * not nice but does the job
		 */
		cptr->local->lasttime = TStime() - ping;
		sendto_one(cptr, "PING :%s", me.name);
	}

	return 0;
}

/*
 * Check registered connections for PING timeout.
 * XXX: also does some other stuff still, need to sort this.  --nenolod
 * Perhaps it would be wise to ping servers as well mr nenolod, just an idea -- Syzop
 */
EVENT(check_pings)
{
	aClient *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &lclient_list, lclient_node)
	{
		/* Check TKLs for this user */
		if (loop.do_bancheck && !check_tkls(cptr))
			continue;
		check_ping(cptr);
		/* don't touch 'cptr' after this as it may have been killed */
	}

	list_for_each_entry_safe(cptr, cptr2, &server_list, special_node)
	{
		check_ping(cptr);
	}

	loop.do_bancheck = loop.do_bancheck_spamf_user = loop.do_bancheck_spamf_away = 0;
	/* done */
}

EVENT(check_deadsockets)
{
	aClient *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &unknown_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when "FLAGS_DEADSOCKET" is set. */
		if (cptr->flags & FLAGS_DEADSOCKET) {
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", cptr->fd, cptr->name);
#endif
			(void)exit_client(cptr, cptr, &me, cptr->local->error_str ? cptr->local->error_str : "Dead socket");
			continue;
		}
	}

	list_for_each_entry_safe(cptr, cptr2, &lclient_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when "FLAGS_DEADSOCKET" is set. */
		if (cptr->flags & FLAGS_DEADSOCKET) {
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", cptr->fd, cptr->name);
#endif
			(void)exit_client(cptr, cptr, &me, cptr->local->error_str ? cptr->local->error_str : "Dead socket");
			continue;
		}
	}
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static int bad_command(const char *argv0)
{
#ifndef _WIN32
	if (!argv0)
		argv0 = "unrealircd";

	(void)printf
	    ("Usage: %s [-f <config>] [-h <servername>] [-p <port>] [-x <loglevel>] [-t] [-F]\n"
	     "\n"
	     "UnrealIRCd\n"
	     " -f <config>     Load configuration from <config> instead of the default\n"
	     "                 (%s).\n"
	     " -h <servername> Override the me::name configuration setting with\n"
	     "                 <servername>.\n"
	     " -p <port>       Listen on <port> in addition to the ports specified by\n"
	     "                 the listen blocks.\n"
	     " -x <loglevel>   Set the log level to <loglevel>.\n"
	     " -t              Dump information to stdout as if you were a linked-in\n"
	     "                 server.\n"
	     " -F              Don't fork() when starting up. Use this when running\n"
	     "                 UnrealIRCd under gdb or when playing around with settings\n"
	     "                 on a non-production setup.\n"
	     "\n",
	     argv0, CONFIGFILE);
	(void)printf("Server not started\n\n");
#else
	if (!IsService) {
		MessageBox(NULL,
		    "Usage: UnrealIRCd [-h servername] [-p portnumber] [-x loglevel]\n",
		    "UnrealIRCD/32", MB_OK);
	}
#endif
	return (-1);
}

char chess[] = {
	85, 110, 114, 101, 97, 108, 0
};

static void version_check_logerror(char *fmt, ...)
{
va_list va;
char buf[1024];

	va_start(va, fmt);
	vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);
#ifndef _WIN32
	fprintf(stderr, "[!!!] %s\n", buf);
#else
	win_log("[!!!] %s", buf);
#endif
}

/** Ugly version checker that ensures ssl/curl runtime libraries match the
 * version we compiled for.
 */
static void do_version_check()
{
	const char *compiledfor, *runtime;
	int error = 0;
	char *p;

	/* OPENSSL:
	 * Nowadays (since openssl 1.0.0) they retain binary compatibility
	 * when the first two version numbers are the same: eg 1.0.0 and 1.0.2
	 */
	compiledfor = OPENSSL_VERSION_TEXT;
	runtime = SSLeay_version(SSLEAY_VERSION);
	p = strchr(compiledfor, '.');
	if (p)
	{
		p = strchr(p+1, '.');
		if (p)
		{
			int versionlen = p - compiledfor + 1;

			if (strncasecmp(compiledfor, runtime, versionlen))
			{
				version_check_logerror("OpenSSL version mismatch: compiled for '%s', library is '%s'",
					compiledfor, runtime);
				error=1;
			}
		}
	}


#ifdef USE_LIBCURL
	/* Perhaps someone should tell them to do this a bit more easy ;)
	 * problem is runtime output is like: 'libcurl/7.11.1 c-ares/1.2.0'
	 * while header output is like: '7.11.1'.
	 */
	{
		char buf[128], *p;

		runtime = curl_version();
		compiledfor = LIBCURL_VERSION;
		if (!strncmp(runtime, "libcurl/", 8))
		{
			strlcpy(buf, runtime+8, sizeof(buf));
			p = strchr(buf, ' ');
			if (p)
			{
				*p = '\0';
				if (strcmp(compiledfor, buf))
				{
					version_check_logerror("Curl version mismatch: compiled for '%s', library is '%s'",
						compiledfor, buf);
					error = 1;
				}
			}
		}
	}
#endif

	if (error)
	{
#ifndef _WIN32
		version_check_logerror("Header<->library mismatches can make UnrealIRCd *CRASH*! "
		                "Make sure you don't have multiple versions of openssl installed (eg: "
		                "one in /usr and one in /usr/local). And, if you recently upgraded them, "
		                "be sure to recompile UnrealIRCd.");
#else
		version_check_logerror("Header<->library mismatches can make UnrealIRCd *CRASH*! "
		                "This should never happen with official Windows builds... unless "
		                "you overwrote any .dll files with newer/older ones or something.");
		win_error();
#endif
		tainted = 1;
	}
}

extern time_t TSoffset;

extern int unreal_time_synch(int timeout);
extern void applymeblock(void);

extern MODVAR Event *events;
extern struct MODVAR ThrottlingBucket *ThrottlingHash[THROTTLING_HASH_SIZE+1];

/** This functions resets a couple of timers and does other things that
 * are absolutely cruicial when the clock is adjusted - particularly
 * when the clock goes backwards. -- Syzop
 */
void fix_timers(void)
{
	int i, cnt;
	aClient *acptr;
	Event *e;
	struct ThrottlingBucket *thr;
	ConfigItem_link *lnk;

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (acptr->local->since > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->local->since %ld -> %ld",
				acptr->name, acptr->local->since, TStime()));
			acptr->local->since = TStime();
		}
		if (acptr->local->lasttime > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->local->lasttime %ld -> %ld",
				acptr->name, acptr->local->lasttime, TStime()));
			acptr->local->lasttime = TStime();
		}
		if (acptr->local->last > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->local->last %ld -> %ld",
				acptr->name, acptr->local->last, TStime()));
			acptr->local->last = TStime();
		}

		/* users */
		if (MyClient(acptr))
		{
			if (acptr->local->nextnick > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->local->nextnick %ld -> %ld",
					acptr->name, acptr->local->nextnick, TStime()));
				acptr->local->nextnick = TStime();
			}
			if (acptr->local->nexttarget > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->local->nexttarget %ld -> %ld",
					acptr->name, acptr->local->nexttarget, TStime()));
				acptr->local->nexttarget = TStime();
			}

		}
	}

	/* Reset all event timers */
	for (e = events; e; e = e->next)
	{
		if (e->last > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: e->last %ld -> %ld",
				e->name, e->last, TStime()-1));
			e->last = TStime()-1;
		}
	}

	/* For throttling we only have to deal with time jumping backward, which
	 * is a real problem as if the jump was, say, 900 seconds, then it would
	 * (potentially) throttle for 900 seconds.
	 * Time going forward is "no problem", it just means we expire our entries
	 * sonner than we should.
	 */
	cnt = 0;
	for (i = 0; i < THROTTLING_HASH_SIZE; i++)
	{
		for (thr = ThrottlingHash[i]; thr; thr = thr->next)
		{
			if (thr->since > TStime())
				thr->since = TStime();
		}
	}
	Debug((DEBUG_DEBUG, "fix_timers(): removed %d throttling item(s)", cnt));

	/* Make sure autoconnect for servers still works (lnk->hold) */
	for (lnk = conf_link; lnk; lnk = (ConfigItem_link *) lnk->next)
	{
		int t = lnk->class ? lnk->class->connfreq : 90;

		if (lnk->hold > TStime() + t)
		{
			lnk->hold = TStime() + (t / 2); /* compromise */
			Debug((DEBUG_DEBUG, "fix_timers(): link '%s' hold-time adjusted to %ld", lnk->servername, lnk->hold));
		}
	}
}


#ifndef _WIN32
static void generate_cloakkeys()
{
	/* Generate 3 cloak keys */
#define GENERATE_CLOAKKEY_MINLEN 50
#define GENERATE_CLOAKKEY_MAXLEN 60 /* Length of cloak keys to generate. */
	char keyBuf[GENERATE_CLOAKKEY_MAXLEN + 1];
	int keyNum;
	int keyLen;
	int charIndex;
	int value;

	short has_upper;
	short has_lower;
	short has_num;

	fprintf(stderr, "Here are 3 random cloak keys:\n");

	for (keyNum = 0; keyNum < 3; ++keyNum)
	{
		has_upper = 0;
		has_lower = 0;
		has_num = 0;

		keyLen = (getrandom8() % (GENERATE_CLOAKKEY_MAXLEN - GENERATE_CLOAKKEY_MINLEN + 1)) + GENERATE_CLOAKKEY_MINLEN;
		for (charIndex = 0; charIndex < keyLen; ++charIndex)
		{
			switch (getrandom8() % 3)
			{
				case 0: /* Uppercase. */
					keyBuf[charIndex] = (char)('A' + (getrandom8() % ('Z' - 'A')));
					has_upper = 1;
					break;
				case 1: /* Lowercase. */
					keyBuf[charIndex] = (char)('a' + (getrandom8() % ('z' - 'a')));
					has_lower = 1;
					break;
				case 2: /* Digit. */
					keyBuf[charIndex] = (char)('0' + (getrandom8() % ('9' - '0')));
					has_num = 1;
					break;
			}
		}
		keyBuf[keyLen] = '\0';

		if (has_upper && has_lower && has_num)
			(void)fprintf(stderr, "%s\n", keyBuf);
		else
			/* Try again. For this reason, keyNum must be signed. */
			keyNum--;
	}
}
#endif

/* MY tdiff... because 'double' sucks.
 * This should work until 2038, and very likely after that as well
 * because 'long' should be 64 bit on all systems by then... -- Syzop
 */
#define mytdiff(a, b)   ((long)a - (long)b)

#ifndef _WIN32
int main(int argc, char *argv[])
#else
int InitUnrealIRCd(int argc, char *argv[])
#endif
{
#ifdef _WIN32
	WORD wVersionRequested = MAKEWORD(1, 1);
	WSADATA wsaData;
#else
	uid_t uid, euid;
	gid_t gid, egid;
	TS   delay = 0;
	struct passwd *pw;
	struct group *gr;
#endif
#ifdef HAVE_PSTAT
	union pstun pstats;
#endif
	int  portarg = 0;
#ifdef  FORCE_CORE
	struct rlimit corelim;
#endif

	memset(&botmotd, '\0', sizeof(aMotdFile));
	memset(&rules, '\0', sizeof(aMotdFile));
	memset(&opermotd, '\0', sizeof(aMotdFile));
	memset(&motd, '\0', sizeof(aMotdFile));
	memset(&smotd, '\0', sizeof(aMotdFile));
	memset(&svsmotd, '\0', sizeof(aMotdFile));
	memset(&me, 0, sizeof(me));
	me.local = MyMallocEx(sizeof(aLocalClient));

	SetupEvents();

#ifdef _WIN32
	CreateMutex(NULL, FALSE, "UnrealMutex");
	SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
#if !defined(_WIN32) && !defined(_AMIGA)
	sbrk0 = (char *)sbrk((size_t)0);
	uid = getuid();
	euid = geteuid();
	gid = getgid();
	egid = getegid();
	timeofday = time(NULL);

#ifndef IRC_USER
	if (!euid && uid)
	{
		fprintf(stderr, "Sorry, a SUID root IRCd without IRC_USER in include/config.h is not supported.\n"
		                "It would be very dangerous. Go edit include/config.h and set IRC_USER and\n"
		                "IRC_GROUP to a nonprivileged username and recompile.\n");
		exit(-1);
	}
	if (!euid)
	{
		fprintf(stderr,
			"WARNING: You are running UnrealIRCd as root and it is not\n"
			"         configured to drop priviliges. This is VERY dangerous,\n"
			"         as any compromise of your UnrealIRCd is the same as\n"
			"         giving a cracker root SSH access to your box.\n"
			"         You should either start UnrealIRCd under a different\n"
			"         account than root, or set IRC_USER in include/config.h\n"
			"         to a nonprivileged username and recompile.\n");
		sleep(1); /* just to catch their attention */
	}
#endif /* IRC_USER */
# ifdef	PROFIL
	(void)monstartup(0, etext);
	(void)moncontrol(1);
	(void)signal(SIGUSR1, s_monitor);
# endif
#endif
#if defined(IRC_USER) && defined(IRC_GROUP)
	if ((int)getuid() == 0) {

		pw = getpwnam(IRC_USER);
		gr = getgrnam(IRC_GROUP);

		if ((pw == NULL) || (gr == NULL)) {
			fprintf(stderr, "ERROR: Unable to lookup to specified user (IRC_USER) or group (IRC_GROUP): %s\n", strerror(errno));
			exit(-1);
		} else {
			irc_uid = pw->pw_uid;
			irc_gid = gr->gr_gid;
		}
	}
#endif
#ifdef	CHROOTDIR
	if (chdir(CONFDIR)) {
		perror("chdir");
		fprintf(stderr, "ERROR: Unable to change to directory '%s'\n", dpath);
		exit(-1);
	}
	if (geteuid() != 0)
		fprintf(stderr, "WARNING: IRCd compiled with CHROOTDIR but effective user id is not root!? "
		                "Booting is very likely to fail. You should start the IRCd as root instead.\n");
	init_resolver(1);
	{
		struct stat sb;
		mode_t umaskold;

		umaskold = umask(0);
		if (mkdir("dev", S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IXOTH) != 0 && errno != EEXIST)
		{
			fprintf(stderr, "ERROR: Cannot mkdir dev: %s\n", strerror(errno));
			exit(5);
		}
		if (stat("/dev/urandom", &sb) != 0)
		{
			fprintf(stderr, "ERROR: Cannot stat /dev/urandom: %s\n", strerror(errno));
			exit(5);
		}
		if (mknod("dev/urandom", sb.st_mode, sb.st_rdev) != 0 && errno != EEXIST)
		{
			fprintf(stderr, "ERROR: Cannot mknod dev/urandom: %s\n", strerror(errno));
			exit(5);
		}
		if (stat("/dev/null", &sb) != 0)
		{
			fprintf(stderr, "ERROR: Cannot stat /dev/null: %s\n", strerror(errno));
			exit(5);
		}
		if (mknod("dev/null", sb.st_mode, sb.st_rdev) != 0 && errno != EEXIST)
		{
			fprintf(stderr, "ERROR: Cannot mknod dev/null: %s\n", strerror(errno));
			exit(5);
		}
		if (stat("/dev/tty", &sb) != 0)
		{
			fprintf(stderr, "ERROR: Cannot stat /dev/tty: %s\n", strerror(errno));
			exit(5);
		}
		if (mknod("dev/tty", sb.st_mode, sb.st_rdev) != 0 && errno != EEXIST)
		{
			fprintf(stderr, "ERROR: Cannot mknod dev/tty: %s\n", strerror(errno));
			exit(5);
		}
		umask(umaskold);
	}
	if (chroot(CONFDIR)) {
		(void)fprintf(stderr, "ERROR:  Cannot (chdir/)chroot to directory '%s'\n", dpath);
		exit(5);
	}
#endif	 /*CHROOTDIR*/
#if !defined(IRC_USER) && !defined(_WIN32)
	if ((uid != euid) && !euid) {
		(void)fprintf(stderr,
		    "ERROR: do not run ircd setuid root. Make it setuid a normal user.\n");
		exit(-1);
	}
#endif

#if defined(IRC_USER) && defined(IRC_GROUP)
	if ((int)getuid() == 0) {
		/* NOTE: irc_uid/irc_gid have been looked up earlier, before the chrooting code */

		if ((irc_uid == 0) || (irc_gid == 0)) {
			(void)fprintf(stderr,
			    "ERROR: SETUID and SETGID have not been set properly"
			    "\nPlease read your documentation\n(HINT: IRC_USER and IRC_GROUP in include/config.h cannot be root/wheel)\n");
			exit(-1);
		} else {
			/* run as a specified user */
			(void)fprintf(stderr, "ircd invoked as root, changing to uid %d (%s) and gid %d (%s)...\n", irc_uid, IRC_USER, irc_gid, IRC_GROUP);
			if (setgid(irc_gid))
			{
				fprintf(stderr, "ERROR: Unable to change group: %s\n", strerror(errno));
				exit(-1);
			}
			if (setuid(irc_uid))
			{
				fprintf(stderr, "ERROR: Unable to change userid: %s\n", strerror(errno));
				exit(-1);
			}
		}
	}
#endif
#ifndef _WIN32
	myargv = argv;
#else
	cmdLine = GetCommandLine();
#endif
#ifndef _WIN32
	(void)umask(077);	/* better safe than sorry --SRB */
#else
	WSAStartup(wVersionRequested, &wsaData);
#endif
	bzero(&StatsZ, sizeof(StatsZ));
	setup_signals();
	charsys_reset();

	memset(&IRCstats, '\0', sizeof(ircstats));
	IRCstats.servers = 1;

	mp_pool_init();
	dbuf_init();
	initlists();

#ifdef USE_LIBCURL
	url_init();
#endif
	tkl_init();
	umode_init();
	extcmode_init();
	init_random(); /* needs to be done very early!! */
	clear_scache_hash_table();
#ifdef FORCE_CORE
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	if (setrlimit(RLIMIT_CORE, &corelim))
		printf("unlimit core size failed; errno = %d\n", errno);
#endif
	/*
	 * ** All command line parameters have the syntax "-fstring"
	 * ** or "-f string" (e.g. the space is optional). String may
	 * ** be empty. Flag characters cannot be concatenated (like
	 * ** "-fxyz"), it would conflict with the form "-fstring".
	 */
	while (--argc > 0 && (*++argv)[0] == '-') {
		char *p = argv[0] + 1;
		int  flag = *p++;
		if (flag == '\0' || *p == '\0') {
			if (argc > 1 && argv[1][0] != '-') {
				p = *++argv;
				argc -= 1;
			} else
				p = "";
		}
		switch (flag) {
#ifndef _WIN32
		  case 'a':
			  bootopt |= BOOT_AUTODIE;
			  break;
		  case 'c':
			  bootopt |= BOOT_CONSOLE;
			  break;
		  case 'q':
			  bootopt |= BOOT_QUICK;
			  break;
#endif
		  case 'F':
			  bootopt |= BOOT_NOFORK;
			  break;
#ifndef _WIN32
		  case 'f':
#ifndef CMDLINE_CONFIG
		      if ((uid == euid) && (gid == egid))
			       configfile = strdup(p);
			  else
			       printf("ERROR: Command line config with a setuid/setgid ircd is not allowed");
#else
			  configfile = strdup(p);
#endif
			  convert_to_absolute_path(&configfile, CONFDIR);
			  break;
		  case 'h':
			  if (!strchr(p, '.')) {

				  (void)printf
				      ("ERROR: %s is not valid: Server names must contain at least 1 \".\"\n",
				      p);
				  exit(1);
			  }
			  strlcpy(me.name, p, sizeof(me.name));
			  break;
#endif
#ifndef _WIN32
		  case 'P':{
			  short type;
			  char *result;
			  srandom(TStime());
			  type = Auth_FindType(NULL, p);
			  if (type == -1)
			  {
			      type = AUTHTYPE_BCRYPT;
			  } else {
			      p = *++argv;
			      argc--;
			  }
			  if (BadPtr(p))
			  {
#ifndef _WIN32
			      p = getpass("Enter password to hash: ");
#else
				  printf("ERROR: You should specify a password to hash");
				  exit(1);
#endif
			  }
			  if ((type == AUTHTYPE_UNIXCRYPT) && (strlen(p) > 8))
			  {
			      /* Hmmm.. is this warning really still true (and always) ?? */
			      printf("WARNING: Password truncated to 8 characters due to 'crypt' algorithm. "
		                 "You are suggested to use the 'md5' algorithm instead.");
				  p[8] = '\0';
			  }
			  if (!(result = Auth_Make(type, p))) {
				  printf("Authentication failed\n");
				  exit(0);
			  }
			  printf("Encrypted password is: %s\n", result);
			  exit(0);
		  }
#endif

		  case 'p':
			  if ((portarg = atoi(p)) > 0)
				  portnum = portarg;
			  break;
		  case 's':
			  (void)printf("sizeof(aClient) == %ld\n",
			      (long)sizeof(aClient));
			  (void)printf("sizeof(aChannel) == %ld\n",
			      (long)sizeof(aChannel));
			  (void)printf("sizeof(aServer) == %ld\n",
			      (long)sizeof(aServer));
			  (void)printf("sizeof(Link) == %ld\n",
			      (long)sizeof(Link));
			  (void)printf("sizeof(anUser) == %ld\n",
			      (long)sizeof(anUser));
			  (void)printf("sizeof(aTKline) == %ld\n",
			      (long)sizeof(aTKline));
			  (void)printf("sizeof(struct ircstatsx) == %ld\n",
			      (long)sizeof(struct ircstatsx));
			  (void)printf("aClient remote == %ld\n",
			      (long)CLIENT_REMOTE_SIZE);
			  exit(0);
			  break;
#ifndef _WIN32
		  case 't':
			  bootopt |= BOOT_TTY;
			  break;
		  case 'v':
			  (void)printf("%s build %s\n", version, buildid);
#else
		  case 'v':
			  if (!IsService) {
				  MessageBox(NULL, version,
				      "UnrealIRCD/Win32 version", MB_OK);
			  }
#endif
			  exit(0);
		  case 'C':
			  config_verbose = atoi(p);
			  break;
		  case 'x':
#ifdef	DEBUGMODE
			  debuglevel = atoi(p);
			  debugmode = *p ? p : "0";
			  bootopt |= BOOT_DEBUG;
			  break;
#else
# ifndef _WIN32
			  (void)fprintf(stderr,
			      "%s: DEBUGMODE must be defined for -x y\n",
			      myargv[0]);
# else
			  if (!IsService) {
				  MessageBox(NULL,
				      "DEBUGMODE must be defined for -x option",
				      "UnrealIRCD/32", MB_OK);
			  }
# endif
			  exit(0);
#endif
#ifndef _WIN32
		  case 'k':
			  generate_cloakkeys();
			  exit(0);
#endif
		  case 'U':
		      chdir(CONFDIR);
		      update_conf();
		      exit(0);
		  case 'R':
		      report_crash();
		      exit(0);
		  default:
#ifndef _WIN32
			  return bad_command(myargv[0]);
#else
			  return bad_command(NULL);
#endif
			  break;
		}
	}

	do_version_check();

#if !defined(CHROOTDIR) && !defined(_WIN32)
#ifndef _WIN32
	mkdir(TMPDIR, S_IRUSR|S_IWUSR|S_IXUSR); /* Create the tmp dir, if it doesn't exist */
 	mkdir(CACHEDIR, S_IRUSR|S_IWUSR|S_IXUSR); /* Create the cache dir, if it doesn't exist */
#else
	mkdir(TMPDIR);
	mkdir(CACHEDIR);
#endif
	if (chdir(TMPDIR)) {
# ifndef _WIN32
		perror("chdir");
		fprintf(stderr, "ERROR: Unable to change to directory '%s'\n", TMPDIR);
# else
		if (!IsService) {
			MessageBox(NULL, strerror(GetLastError()),
			    "UnrealIRCD/32: chdir()", MB_OK);
		}
# endif
		exit(-1);
	}
#endif
#ifndef _WIN32
	/*
	 * didn't set debuglevel
	 */
	/*
	 * but asked for debugging output to tty
	 */
	if ((debuglevel < 0) && (bootopt & BOOT_TTY)) {
		(void)fprintf(stderr,
		    "you specified -t without -x. use -x <n>\n");
		exit(-1);
	}
#endif

	/* HACK! This ifndef should be removed when the restart-on-w32-brings-up-dialog bug
	 * is fixed. This is just an ugly "ignore the invalid parameter" thing ;). -- Syzop
	 */
#ifndef _WIN32
	if (argc > 0)
		return bad_command(myargv[0]);	/* This should exit out */
#endif
#ifndef _WIN32
	fprintf(stderr, "%s", unreallogo);
	fprintf(stderr, "                           v%s\n\n", VERSIONONLY);
	fprintf(stderr, "  using %s\n", tre_version());
	fprintf(stderr, "  using %s\n", SSLeay_version(SSLEAY_VERSION));
#ifdef USE_LIBCURL
	fprintf(stderr, "  using %s\n", curl_version());
#endif
	fprintf(stderr, "\n");
#endif
	clear_client_hash_table();
	clear_channel_hash_table();
	clear_watch_hash_table();
	bzero(&loop, sizeof(loop));
	init_CommandHash();
	initwhowas();
	initstats();
	DeleteTempModules();
	booted = FALSE;
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	/* Hack to stop people from being able to read the config file */
	(void)chmod(CPATH, DEFAULT_PERMISSIONS);
#endif
	init_dynconf();
	/*
	 * Add default class
	 */
	default_class =
	    (ConfigItem_class *) MyMallocEx(sizeof(ConfigItem_class));
	default_class->flag.permanent = 1;
	default_class->pingfreq = PINGFREQUENCY;
	default_class->maxclients = 100;
	default_class->sendq = MAXSENDQLENGTH;
	default_class->name = "default";
	AddListItem(default_class, conf_class);
	if (init_conf(configfile, 0) < 0)
	{
		exit(-1);
	}
	booted = TRUE;
	load_tunefile();
	make_umodestr();
	extcmodes_check_for_changes();
	make_extbanstr();
	isupport_init();
	clicap_init();
	if (!find_Command_simple("AWAY") /*|| !find_Command_simple("KILL") ||
		!find_Command_simple("OPER") || !find_Command_simple("PING")*/)
	{
		config_error("Someone forgot to load modules with proper commands in them. READ THE DOCUMENTATION");
		exit(-4);
	}

#ifndef _WIN32
	fprintf(stderr, "Initializing SSL..\n");
#endif
	if (!init_ssl())
	{
		config_warn("Failed to load SSL (see error above), proceeding without SSL support...");
		if (ssl_used_in_config_but_unavail())
		{
			config_error("IRCd failed to start");
#ifdef _WIN32
			win_error(); /* display error dialog box */
#endif
			exit(9);
		}
	}
#ifndef _WIN32
	fprintf(stderr, "Dynamic configuration initialized.. booting IRCd.\n");
#endif
	open_debugfile();
	if (portnum < 0)
		portnum = PORTNUM;
	me.local->port = portnum;
	(void)init_sys();
	me.flags = FLAGS_LISTEN;
	me.fd = -1;
	SetMe(&me);
	make_server(&me);
	applymeblock();
#ifdef HAVE_SYSLOG
	openlog("ircd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif
	uid_init();
	run_configuration();
	ircd_log(LOG_ERROR, "UnrealIRCd started.");

	read_motd(conf_files->botmotd_file, &botmotd);
	read_motd(conf_files->rules_file, &rules);
	read_motd(conf_files->opermotd_file, &opermotd);
	read_motd(conf_files->motd_file, &motd);
	read_motd(conf_files->smotd_file, &smotd);
	read_motd(conf_files->svsmotd_file, &svsmotd);

	me.hopcount = 0;
	me.local->authfd = -1;
	me.user = NULL;
	me.from = &me;

	/*
	 * This listener will never go away
	 */
	me_hash = find_or_add(me.name);
	me.serv->up = me_hash;
	timeofday = time(NULL);
	me.local->lasttime = me.local->since = me.local->firsttime = TStime();
	(void)add_to_client_hash_table(me.name, &me);
	(void)add_to_id_hash_table(me.id, &me);
	list_add(&me.client_node, &global_server_list);
#if !defined(_AMIGA) && !defined(_WIN32) && !defined(NO_FORKING)
	if (!(bootopt & BOOT_NOFORK))
	{
		if (fork())
			exit(0);
    fd_fork();
		loop.ircd_forked = 1;
	}
#endif
	(void)ircsnprintf(REPORT_DO_DNS, sizeof(REPORT_DO_DNS), ":%s %s", me.name, BREPORT_DO_DNS);
	(void)ircsnprintf(REPORT_FIN_DNS, sizeof(REPORT_FIN_DNS), ":%s %s", me.name, BREPORT_FIN_DNS);
	(void)ircsnprintf(REPORT_FIN_DNSC, sizeof(REPORT_FIN_DNSC), ":%s %s", me.name, BREPORT_FIN_DNSC);
	(void)ircsnprintf(REPORT_FAIL_DNS, sizeof(REPORT_FAIL_DNS), ":%s %s", me.name, BREPORT_FAIL_DNS);
	(void)ircsnprintf(REPORT_DO_ID, sizeof(REPORT_DO_ID), ":%s %s", me.name, BREPORT_DO_ID);
	(void)ircsnprintf(REPORT_FIN_ID, sizeof(REPORT_FIN_ID), ":%s %s", me.name, BREPORT_FIN_ID);
	(void)ircsnprintf(REPORT_FAIL_ID, sizeof(REPORT_FAIL_ID), ":%s %s", me.name, BREPORT_FAIL_ID);
	R_do_dns = strlen(REPORT_DO_DNS);
	R_fin_dns = strlen(REPORT_FIN_DNS);
	R_fin_dnsc = strlen(REPORT_FIN_DNSC);
	R_fail_dns = strlen(REPORT_FAIL_DNS);
	R_do_id = strlen(REPORT_DO_ID);
	R_fin_id = strlen(REPORT_FIN_ID);
	R_fail_id = strlen(REPORT_FAIL_ID);

	if (TIMESYNCH)
	{
		if (!unreal_time_synch(TIMESYNCH_TIMEOUT))
			ircd_log(LOG_ERROR, "TIME SYNCH: Unable to synchronize time: %s. "
			                    "This means UnrealIRCd was unable to synchronize the IRCd clock to a known good time source. "
			                    "As long as the server owner keeps the server clock synchronized through NTP, everything will be fine.",
				unreal_time_synch_error());
	}
	fix_timers(); /* Fix timers AFTER reading tune file AND timesynch */
	write_pidfile();
	Debug((DEBUG_NOTICE, "Server ready..."));
	init_throttling_hash();
	loop.ircd_booted = 1;
#if defined(HAVE_SETPROCTITLE)
	setproctitle("%s", me.name);
#elif defined(HAVE_PSTAT)
	pstats.pst_command = me.name;
	pstat(PSTAT_SETCMD, pstats, strlen(me.name), 0, 0);
#elif defined(HAVE_PSSTRINGS)
	PS_STRINGS->ps_nargvstr = 1;
	PS_STRINGS->ps_argvstr = me.name;
#endif
	module_loadall();

#ifdef _WIN32
	return 1;
}


void SocketLoop(void *dummy)
{
	TS   delay = 0;
	static TS lastglinecheck = 0;
	TS   last_tune;


	while (1)
#else
	/*
	 * Forever drunk .. forever drunk ..
	 * * (Sorry Alphaville.)
	 */
	for (;;)
#endif
	{

#define NEGATIVE_SHIFT_WARN	-15
#define POSITIVE_SHIFT_WARN	20

		timeofday = time(NULL) + TSoffset;
		if (oldtimeofday == 0)
			oldtimeofday = timeofday; /* pretend everything is ok the first time.. */
		if (mytdiff(timeofday, oldtimeofday) < NEGATIVE_SHIFT_WARN) {
			/* tdiff = # of seconds of time set backwards (positive number! eg: 60) */
			long tdiff = oldtimeofday - timeofday;
			ircd_log(LOG_ERROR, "WARNING: Time running backwards! Clock set back ~%ld seconds (%ld -> %ld)",
				tdiff, oldtimeofday, timeofday);
			ircd_log(LOG_ERROR, "[TimeShift] Resetting a few timers to prevent IRCd freeze!");
			sendto_realops("WARNING: Time running backwards! Clock set back ~%ld seconds (%ld -> %ld)",
				tdiff, oldtimeofday, timeofday);
			sendto_realops("Incorrect time for IRC servers is a serious problem. "
			               "Time being set backwards (either by TSCTL or by resetting the clock) is "
			               "even more serious and can cause clients to freeze, channels to be "
			               "taken over, and other issues.");
			sendto_realops("Please be sure your clock is always synchronized before "
			               "the IRCd is started or use the built-in timesynch feature.");
			sendto_realops("[TimeShift] Resetting a few timers to prevent IRCd freeze!");
			fix_timers();
		} else
		if (mytdiff(timeofday, oldtimeofday) > POSITIVE_SHIFT_WARN) /* do not set too low or you get false positives */
		{
			/* tdiff = # of seconds of time set forward (eg: 60) */
			long tdiff = timeofday - oldtimeofday;
			ircd_log(LOG_ERROR, "WARNING: Time jumped ~%ld seconds ahead! (%ld -> %ld)",
				tdiff, oldtimeofday, timeofday);
			ircd_log(LOG_ERROR, "[TimeShift] Resetting some timers!");
			sendto_realops("WARNING: Time jumped ~%ld seconds ahead! (%ld -> %ld)",
			        tdiff, oldtimeofday, timeofday);
			sendto_realops("Incorrect time for IRC servers is a serious problem. "
			               "Time being adjusted (either by TSCTL or by resetting the clock) "
			               "more than a few seconds forward/backward can lead to serious issues.");
			sendto_realops("Please be sure your clock is always synchronized before "
			               "the IRCd is started or use the built-in timesynch feature.");
			sendto_realops("[TimeShift] Resetting some timers!");
			fix_timers();
		}
		if (highesttimeofday+NEGATIVE_SHIFT_WARN > timeofday)
		{
			if (lasthighwarn > timeofday)
				lasthighwarn = timeofday;
			if (timeofday - lasthighwarn > 300)
			{
				ircd_log(LOG_ERROR, "[TimeShift] The (IRCd) clock was set backwards. "
					"Waiting for time to be OK again. This will be in %ld seconds",
					highesttimeofday - timeofday);
				sendto_realops("[TimeShift] The (IRCd) clock was set backwards. Timers, nick- "
				               "and channel-timestamps are possibly incorrect. This message will "
				               "repeat itself until we catch up with the original time, which will be "
				               "in %ld seconds", highesttimeofday - timeofday);
				lasthighwarn = timeofday;
			}
		} else {
			highesttimeofday = timeofday;
		}
		oldtimeofday = timeofday;
		LockEventSystem();
		DoEvents();
		UnlockEventSystem();

		/*
		 * ** Run through the hashes and check lusers every
		 * ** second
		 * ** also check for expiring glines
		 */
		if (IRCstats.clients > IRCstats.global_max)
			IRCstats.global_max = IRCstats.clients;
		if (IRCstats.me_clients > IRCstats.me_max)
			IRCstats.me_max = IRCstats.me_clients;

		fd_select(SOCKETLOOP_MAX_DELAY);

		process_clients();

		timeofday = time(NULL) + TSoffset;

		/*
		 * Debug((DEBUG_DEBUG, "Got message(s)"));
		 */
		/*
		 * ** ...perhaps should not do these loops every time,
		 * ** but only if there is some chance of something
		 * ** happening (but, note that conf->hold times may
		 * ** be changed elsewhere--so precomputed next event
		 * ** time might be too far away... (similarly with
		 * ** ping times) --msa
		 */
		if (dorehash)
		{
			(void)rehash(&me, &me, 1);
			dorehash = 0;
		}
		if (dorestart)
		{
			server_reboot("SIGINT");
		}
		if (doreloadcert)
		{
			reinit_ssl(NULL);
			doreloadcert = 0;
		}
	}
}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * If the debuglevel is not set from the command line by -x, use /dev/null
 * as the dummy logfile as long as DEBUGMODE has been defined, else don't
 * waste the fd.
 */
static void open_debugfile(void)
{
#ifdef	DEBUGMODE
	int  fd;
	aClient *cptr;
	if (debuglevel >= 0) {
		cptr = make_client(NULL, NULL);
		cptr->fd = 2;
		SetLog(cptr);
		cptr->local->port = debuglevel;
		cptr->flags = 0;

		(void)strlcpy(cptr->local->sockhost, me.local->sockhost, sizeof cptr->local->sockhost);
# ifndef _WIN32
		/*(void)printf("isatty = %d ttyname = %#x\n",
		    isatty(2), (u_int)ttyname(2)); */
		if (!(bootopt & BOOT_TTY)) {	/* leave debugging output on fd 2 */
			(void)truncate(LOGFILE, 0);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0)
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);

#if 1
			cptr->fd = fd;
			debugfd = fd;
#else
			/* if (fd != 2) {
				(void)dup2(fd, 2);
				(void)close(fd);
			} -- hands off stderr! */
#endif
			strlcpy(cptr->name, LOGFILE, sizeof(cptr->name));
		} else if (isatty(2) && ttyname(2))
			strlcpy(cptr->name, ttyname(2), sizeof(cptr->name));
		else
# endif
			strlcpy(cptr->name, "FD2-Pipe", sizeof(cptr->name));
		Debug((DEBUG_FATAL,
		    "Debug: File <%s> Level: %d at %s", cptr->name,
		    cptr->local->port, myctime(time(NULL))));
	}
#endif
}

static void setup_signals()
{
#ifndef _WIN32
#ifdef	POSIX_SIGNALS
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaddset(&act.sa_mask, SIGALRM);
# ifdef	SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, NULL);
# endif
	(void)sigaction(SIGPIPE, &act, NULL);
	act.sa_handler = dummy;
	(void)sigaction(SIGALRM, &act, NULL);
	act.sa_handler = s_rehash;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
	act.sa_handler = s_restart;
	(void)sigaddset(&act.sa_mask, SIGINT);
	(void)sigaction(SIGINT, &act, NULL);
	act.sa_handler = s_die;
	(void)sigaddset(&act.sa_mask, SIGTERM);
	(void)sigaction(SIGTERM, &act, NULL);
	act.sa_handler = s_reloadcert;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
#else
# ifndef	HAVE_RELIABLE_SIGNALS
	(void)signal(SIGPIPE, dummy);
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, dummy);
#  endif
# else
#  ifdef	SIGWINCH
	(void)signal(SIGWINCH, SIG_IGN);
#  endif
	(void)signal(SIGPIPE, SIG_IGN);
# endif
	(void)signal(SIGALRM, dummy);
	(void)signal(SIGHUP, s_rehash);
	(void)signal(SIGTERM, s_die);
	(void)signal(SIGINT, s_restart);
	(void)signal(SIGUSR1, s_reloadcert);
#endif
#ifdef RESTARTING_SYSTEMCALLS
	/*
	 * ** At least on Apollo sr10.1 it seems continuing system calls
	 * ** after signal is the default. The following 'siginterrupt'
	 * ** should change that default to interrupting calls.
	 */
	(void)siginterrupt(SIGALRM, 1);
#endif
#else
	(void)signal(SIGSEGV, CleanUpSegv);
#endif
}
