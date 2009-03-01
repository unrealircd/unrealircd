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

#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)ircd.c	2.48 3/9/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
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
#ifndef NO_FDLIST
#include "fdlist.h"
#endif
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#include "version.h"
#include "proto.h"
#ifdef _WIN32
extern BOOL IsService;
#endif
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
extern MODVAR aMotd *opermotd;
extern MODVAR aMotd *svsmotd;
extern MODVAR aMotd *motd;
extern MODVAR aMotd *rules;
extern MODVAR aMotd *botmotd;
extern MODVAR aMotd *smotd;
MODVAR MemoryInfo StatsZ;
#ifndef _WIN32
uid_t irc_uid = 0;
gid_t irc_gid = 0; 
#endif

int  R_do_dns, R_fin_dns, R_fin_dnsc, R_fail_dns, R_do_id, R_fin_id, R_fail_id;

char REPORT_DO_DNS[256], REPORT_FIN_DNS[256], REPORT_FIN_DNSC[256],
    REPORT_FAIL_DNS[256], REPORT_DO_ID[256], REPORT_FIN_ID[256],
    REPORT_FAIL_ID[256];
extern ircstats IRCstats;
aClient me;			/* That's me */
MODVAR char *me_hash;
aClient *client = &me;		/* Pointer to beginning of Client list */
extern char backupbuf[8192];
#ifdef _WIN32
extern void CleanUpSegv(int sig);
extern SERVICE_STATUS_HANDLE IRCDStatusHandle;
extern SERVICE_STATUS IRCDStatus;
#endif
#ifndef NO_FDLIST
fdlist default_fdlist;
fdlist busycli_fdlist;
fdlist serv_fdlist;
fdlist oper_fdlist;
fdlist unknown_fdlist;
float currentrate;
float currentrate2;		/* outgoing */
float highest_rate = 0;
float highest_rate2 = 0;
int  lifesux = 0;
int  LRV = LOADRECV;
TS   LCF = LOADCFREQ;
int  currlife = 0;
int  HTMLOCK = 0;
int  noisy_htm = 1;
long lastrecvK = 0;
long lastsendK = 0;

TS   check_fdlists();
#endif

unsigned char conf_debuglevel = 0;
char trouble_info[1024];

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

MODVAR TS   last_garbage_collect = 0;
#ifndef _WIN32
MODVAR char **myargv;
#else
LPCSTR cmdLine;
#endif
int  portnum = -1;		/* Server port number, listening this */
char *configfile = CONFIGFILE;	/* Server configuration file */
int  debuglevel = 10;		/* Server debug level */
int  bootopt = 0;		/* Server boot option flags */
char *debugmode = "";		/*  -"-    -"-   -"-  */
char *sbrk0;			/* initial sbrk(0) */
static int dorehash = 0, dorestart = 0;
static char *dpath = DPATH;
MODVAR int  booted = FALSE;
MODVAR TS   nextconnect = 1;		/* time for next try_connections call */
MODVAR TS   nextping = 1;		/* same as above for check_pings() */
MODVAR TS   nextdnscheck = 0;		/* next time to poll dns to force timeouts */
MODVAR TS   nextexpire = 1;		/* next expire run on the dns cache */
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
	int  i;
	aClient *cptr;
	if (!IsService)
	{
		unload_all_modules();
		for (i = LastSlot; i >= 0; i--)
			if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
				(void)send_queued(cptr);
		
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
	flush_connections(&me);
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
	int  i;
#ifdef _WIN32
	aClient *cptr;
#endif
	sendto_realops("Aieeeee!!!  Restarting server... %s", mesg);
	Debug((DEBUG_NOTICE, "Restarting server... %s", mesg));
#ifndef _WIN32
	flush_connections(&me);
#else
	for (i = LastSlot; i >= 0; i--)
		if ((cptr = local[i]) && DBufLength(&cptr->sendQ) > 0)
			(void)send_queued(cptr);
#endif
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
	if ((bootopt & BOOT_CONSOLE) || isatty(0))
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
static TS try_connections(TS currenttime)
{
	ConfigItem_link *aconf;
	ConfigItem_deny_link *deny;
	aClient *cptr;
	int  connecting, confrq;
	TS   next = 0;
	ConfigItem_class *cltmp;

	connecting = FALSE;
	Debug((DEBUG_NOTICE, "Connection check at   : %s",
	    myctime(currenttime)));
	for (aconf = conf_link; aconf; aconf = (ConfigItem_link *) aconf->next) {
		/*
		 * Also when already connecting! (update holdtimes) --SRB 
		 */
		if (!(aconf->options & CONNECT_AUTO) || (aconf->flag.temporary == 1))
			continue;

		cltmp = aconf->class;
		/*
		 * ** Skip this entry if the use of it is still on hold until
		 * ** future. Otherwise handle this entry (and set it on hold
		 * ** until next time). Will reset only hold times, if already
		 * ** made one successfull connection... [this algorithm is
		 * ** a bit fuzzy... -- msa >;) ]
		 */

		if ((aconf->hold > currenttime)) {
			if ((next > aconf->hold) || (next == 0))
				next = aconf->hold;
			continue;
		}

		confrq = cltmp->connfreq;
		aconf->hold = currenttime + confrq;
		/*
		 * ** Found a CONNECT config with port specified, scan clients
		 * ** and see if this server is already connected?
		 */
		cptr = find_name(aconf->servername, (aClient *)NULL);

		if (!cptr && (cltmp->clients < cltmp->maxclients)) {
			/*
			 * Check connect rules to see if we're allowed to try 
			 */
			for (deny = conf_deny_link; deny;
			    deny = (ConfigItem_deny_link *) deny->next)
				if (!match(deny->mask, aconf->servername)
				    && crule_eval(deny->rule))
					break;

			if (!deny && connect_server(aconf, (aClient *)NULL,
			    (struct hostent *)NULL) == 0)
				sendto_realops
				    ("Connection to %s[%s] activated.",
				    aconf->servername, aconf->hostname);

		}
		if ((next > aconf->hold) || (next == 0))
			next = aconf->hold;
	}
	Debug((DEBUG_NOTICE, "Next connection check : %s", myctime(next)));
	return (next);
}


/* Now find_kill is only called when a kline-related command is used:
   AKILL/RAKILL/KLINE/UNKLINE/REHASH.  Very significant CPU usage decrease.
   -- Barubary */



extern TS check_pings(TS currenttime)
{
	aClient *cptr = NULL;
	ConfigItem_ban *bconf = NULL;
	char killflag = 0;
	int  i = 0;
	char banbuf[1024];
	int  ping = 0;

	for (i = 0; i <= LastSlot; i++) {
		/*
		 * If something we should not touch .. 
		 */
		if (!(cptr = local[i]) || IsMe(cptr) || IsLog(cptr))
			continue;

		/*
		 * ** Note: No need to notify opers here. It's
		 * ** already done when "FLAGS_DEADSOCKET" is set.
		 */
		if (cptr->flags & FLAGS_DEADSOCKET) {
			(void)exit_client(cptr, cptr, &me, cptr->error_str ? cptr->error_str : "Dead socket");
			continue;
		}
		killflag = 0;
		/*
		 * Check if user is banned
		 */
		if (loop.do_bancheck) {
			if (find_tkline_match(cptr, 0) < 0) {
				/*
				 * Client exited 
				 */
				continue;
			}
			find_shun(cptr);
			if (!killflag && IsPerson(cptr)) {
				/*
				 * If it's a user, we check for CONF_BAN_USER
				 */
				bconf =
				    Find_ban(cptr, make_user_host(cptr->
				    user ? cptr->user->username : cptr->
				    username,
				    cptr->user ? cptr->user->realhost : cptr->
				    sockhost), CONF_BAN_USER);
				if (bconf)
					killflag++;

				if (!killflag && !IsAnOper(cptr) &&
				    (bconf =
				    Find_ban(NULL, cptr->info, CONF_BAN_REALNAME))) {
					killflag++;
				}

			}
			/*
			 * If no cookie, we search for Z:lines
			 */
			if (!killflag)
				if ((bconf =
				    Find_ban(cptr, Inet_ia2p(&cptr->ip),
				    CONF_BAN_IP)))
					killflag++;
			if (killflag) {
				if (IsPerson(cptr))
					sendto_realops("Ban active for %s (%s)",
					    get_client_name(cptr, FALSE),
					    bconf->reason ? bconf->
					    reason : "no reason");

				if (IsServer(cptr))
					sendto_realops
					    ("Ban active for server %s (%s)",
					    get_client_name(cptr, FALSE),
					    bconf->reason ? bconf->
					    reason : "no reason");
				if (bconf->reason) {
					if (IsPerson(cptr))
						snprintf(banbuf, sizeof banbuf - 1,
						         "User has been banned (%s)", bconf->reason);
					else
						snprintf(banbuf, sizeof banbuf - 1,
						         "Banned (%s)", bconf->reason);
					(void)exit_client(cptr, cptr, &me,
					    banbuf);
				} else {
					if (IsPerson(cptr))
						(void)exit_client(cptr, cptr,
						    &me,
						    "User has been banned");
					else
						(void)exit_client(cptr, cptr,
						    &me, "Banned");
				}
				continue;
			}

		}
		/* Do spamfilter 'user' banchecks.. */
		if (loop.do_bancheck_spamf_user && IsPerson(cptr))
		{
			if (find_spamfilter_user(cptr, SPAMFLAG_NOWARN) == FLUSH_BUFFER)
				continue;
		}
		if (loop.do_bancheck_spamf_away && IsPerson(cptr) && cptr->user->away)
		{
			if (dospamfilter(cptr, cptr->user->away, SPAMF_AWAY, NULL, SPAMFLAG_NOWARN, NULL) == FLUSH_BUFFER)
				continue;
		}
		/*
		 * We go into ping phase 
		 */
		ping =
		    IsRegistered(cptr) ? (cptr->class ? cptr->
		    class->pingfreq : CONNECTTIMEOUT) : CONNECTTIMEOUT;
		Debug((DEBUG_DEBUG, "c(%s)=%d p %d k %d a %d", cptr->name,
		    cptr->status, ping, killflag,
		    currenttime - cptr->lasttime));
		
		/* If ping is less than or equal to the last time we received a command from them */
		if (ping <= (currenttime - cptr->lasttime))
		{
			if (
				/* If we have sent a ping */
				((cptr->flags & FLAGS_PINGSENT)
				/* And they had 2x ping frequency to respond */
				&& ((currenttime - cptr->lasttime) >= (2 * ping)))
				|| 
				/* Or isn't registered and time spent is larger than ping .. */
				(!IsRegistered(cptr) && (currenttime - cptr->since >= ping))
				)
			{
				/* if it's registered and doing dns/auth, timeout */
				if (!IsRegistered(cptr) && (DoingDNS(cptr) || DoingAuth(cptr)))
				{
					if (cptr->authfd >= 0) {
						CLOSE_SOCK(cptr->authfd);
						--OpenFiles;
						cptr->authfd = -1;
						cptr->count = 0;
						*cptr->buffer = '\0';
					}
					if (SHOWCONNECTINFO && !cptr->serv) {
						if (DoingDNS(cptr))
							sendto_one(cptr,
							    REPORT_FAIL_DNS);
						else if (DoingAuth(cptr))
							sendto_one(cptr,
							    REPORT_FAIL_ID);
					}
					Debug((DEBUG_NOTICE,
					    "DNS/AUTH timeout %s",
					    get_client_name(cptr, TRUE)));
					unrealdns_delreq_bycptr(cptr);
					ClearAuth(cptr);
					ClearDNS(cptr);
					SetAccess(cptr);
					cptr->firsttime = currenttime;
					cptr->lasttime = currenttime;
					continue;
				}
				if (IsServer(cptr) || IsConnecting(cptr) ||
				    IsHandshake(cptr)
#ifdef USE_SSL
					|| IsSSLConnectHandshake(cptr)
#endif	    
				    ) {
					sendto_realops
					    ("No response from %s, closing link",
					    get_client_name(cptr, FALSE));
					sendto_serv_butone(&me,
					    ":%s GLOBOPS :No response from %s, closing link",
					    me.name, get_client_name(cptr,
					    FALSE));
				}
#ifdef USE_SSL
				if (IsSSLAcceptHandshake(cptr))
					Debug((DEBUG_DEBUG, "ssl accept handshake timeout: %s (%li-%li > %li)", cptr->sockhost,
						currenttime, cptr->since, ping));
#endif
				exit_client(cptr, cptr, &me, "Ping timeout");
				continue;
				
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
				cptr->lasttime = currenttime - ping;
				sendto_one(cptr, "%s :%s",
				    IsToken(cptr) ? TOK_PING : MSG_PING,
				    me.name);
			}
		}
		/*
		 * Check UNKNOWN connections - if they have been in this state
		 * for > 100s, close them.
		 */
		if (IsUnknown(cptr)
#ifdef USE_SSL
			|| (IsSSLAcceptHandshake(cptr) || IsSSLConnectHandshake(cptr))
#endif		
		)
			if (cptr->firsttime ? ((currenttime - cptr->firsttime) >
			    100) : 0)
				(void)exit_client(cptr, cptr, &me,
				    "Connection Timed Out");
	}
	/*
	 * EXPLANATION
	 * * on a server with a large volume of clients, at any given point
	 * * there may be a client which needs to be pinged the next second,
	 * * or even right away (a second may have passed while running
	 * * check_pings). Preserving CPU time is more important than
	 * * pinging clients out at exact times, IMO. Therefore, I am going to make
	 * * check_pings always return currenttime + 9. This means that it may take
	 * * a user up to 9 seconds more than pingfreq to timeout. Oh well.
	 * * Plus, the number is 9 to 'stagger' our check_pings calls out over
	 * * time, to avoid doing it and the other tasks ircd does at the same time
	 * * all the time (which are usually done on intervals of 5 seconds or so). 
	 * * - lucas
	 * *
	 */
	loop.do_bancheck = loop.do_bancheck_spamf_user = loop.do_bancheck_spamf_away = 0;
	Debug((DEBUG_NOTICE, "Next check_ping() call at: %s, %d %d %d",
	    myctime(currenttime+9), ping, currenttime+9, currenttime));

	return currenttime + 9;
}

/*
** bad_command
**	This is called when the commandline is not acceptable.
**	Give error message and exit without starting anything.
*/
static int bad_command(void)
{
#ifndef _WIN32
	(void)printf
	    ("Usage: ircd [-f config] [-h servername] [-p portnumber] [-x loglevel] [-t] [-H]\n");
	(void)printf("Server not started\n\n");
#else
	if (!IsService) {
		MessageBox(NULL,
		    "Usage: wircd [-h servername] [-p portnumber] [-x loglevel]\n",
		    "UnrealIRCD/32", MB_OK);
	}
#endif
	return (-1);
}

char chess[] = {
	85, 110, 114, 101, 97, 108, 0
};
#ifndef NO_FDLIST
inline TS check_fdlists(TS now)
{
	aClient *cptr;
	int  pri;		/* temp. for priority */
	int  i, j;
	j = 0;
	for (i = LastSlot; i >= 0; i--) {
		if (!(cptr = local[i]))
			continue;
		if (IsServer(cptr) || IsListening(cptr)
		    || IsOper(cptr) || DoingAuth(cptr)) {
			busycli_fdlist.entry[++j] = i;
			continue;
		}
		pri = cptr->priority;
		if (cptr->receiveM == cptr->lastrecvM)
			pri += 2;	/* lower a bit */
		else
			pri -= 30;
		if (pri < 0)
			pri = 0;
		if (pri > 80)
			pri = 80;
		cptr->lastrecvM = cptr->receiveM;
		cptr->priority = pri;
		if ((pri < 10) || (!lifesux && (pri < 25)))
			busycli_fdlist.entry[++j] = i;
	}
	busycli_fdlist.last_entry = j;	/* rest of the fdlist is garbage */
	return (now + FDLISTCHKFREQ + lifesux * FDLISTCHKFREQ);
}

EVENT(e_check_fdlists)
{
	check_fdlists(TStime());
}

#endif

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

/** Ugly version checker that ensures zlib/ssl/curl runtime libraries match the
 * version we compiled for.
 */
static void do_version_check()
{
const char *compiledfor, *runtime;
int error = 0;

#ifdef USE_SSL
	compiledfor = OPENSSL_VERSION_TEXT;
	runtime = SSLeay_version(SSLEAY_VERSION);
	if (strcasecmp(compiledfor, runtime))
	{
		version_check_logerror("OpenSSL version mismatch: compiled for '%s', library is '%s'",
			compiledfor, runtime);
		error=1;
	}
#endif
#ifdef ZIP_LINKS
	runtime = zlibVersion();
	compiledfor = ZLIB_VERSION;
	if (*compiledfor != *runtime)
	{
		version_check_logerror("Zlib version mismatch: compiled for '%s', library is '%s'",
			compiledfor, runtime);
		error = 1;
	}
#endif
#ifdef USE_LIBCURL
	/* Perhaps someone should tell them to do this a bit more easy ;)
	 * problem is runtime output is like: 'libcurl/7.11.1 zlib/1.2.1 c-ares/1.2.0'
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
		                "Make sure you don't have multiple versions of openssl or zlib installed (eg: "
		                "one in /usr and one in /usr/local). And, if you recently upgraded them, "
		                "be sure to recompile Unreal.");
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
struct ThrottlingBucket *n;
struct ThrottlingBucket z = { NULL, NULL, {0}, 0, 0};

	/* Client time stuff */
	for (i = 0; i <= LastSlot; i++)
	{
	
	        if (!(acptr = local[i]) || IsMe(acptr))
	        	continue;

		/* all (servers AND users) */
		if (MyConnect(acptr))
		{
			if (acptr->since > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->since %ld -> %ld",
					acptr->name, acptr->since, TStime()));
				acptr->since = TStime();
			}
			if (acptr->lasttime > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->lasttime %ld -> %ld",
					acptr->name, acptr->lasttime, TStime()));
				acptr->lasttime = TStime();
			}
			if (acptr->last > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->last %ld -> %ld",
					acptr->name, acptr->last, TStime()));
				acptr->last = TStime();
			}
		}
		
		/* users */
		if (MyClient(acptr))
		{
			if (acptr->nextnick > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->nextnick %ld -> %ld",
					acptr->name, acptr->nextnick, TStime()));
				acptr->nextnick = TStime();
			}
			if (acptr->nexttarget > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: acptr->nexttarget %ld -> %ld",
					acptr->name, acptr->nexttarget, TStime()));
				acptr->nexttarget = TStime();
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

	/* Just flush all throttle stuff... */
	cnt = 0;
	for (i = 0; i < THROTTLING_HASH_SIZE; i++)
		for (n = ThrottlingHash[i]; n; n = n->next)
		{
			z.next = (struct ThrottlingBucket *) DelListItem(n, ThrottlingHash[i]);
			cnt++;
			MyFree(n);
			n = &z;
		}
	Debug((DEBUG_DEBUG, "fix_timers(): removed %d throttling item(s)", cnt));
	
	Debug((DEBUG_DEBUG, "fix_timers(): updating nextping/nextconnect/nextdnscheck/nextexpire (%ld/%ld/%ld/%ld)",
		nextping, nextconnect, nextdnscheck, nextexpire));	
	nextping = nextconnect = nextdnscheck = nextexpire = 0;
}


#ifndef _WIN32
static void generate_cloakkeys()
{
	/* Generate 3 cloak keys */
#define GENERATE_CLOAKKEY_MINLEN 10
#define GENERATE_CLOAKKEY_MAXLEN 20 /* Length of cloak keys to generate. */
	char keyBuf[GENERATE_CLOAKKEY_MAXLEN + 1];
	int keyNum;
	int keyLen;
	int charIndex;
	int value;

	fprintf(stderr, "Here are 3 random cloak keys:\n");

	for (keyNum = 0; keyNum < 3; ++keyNum)
	{
		keyLen = (getrandom8() % (GENERATE_CLOAKKEY_MAXLEN - GENERATE_CLOAKKEY_MINLEN + 1)) + GENERATE_CLOAKKEY_MINLEN;
		for (charIndex = 0; charIndex < keyLen; ++charIndex)
		{
			switch (getrandom8() % 3)
			{
				case 0: /* Uppercase. */
					keyBuf[charIndex] = (char)('A' + (getrandom8() % ('Z' - 'A')));
					break;
				case 1: /* Lowercase. */
					keyBuf[charIndex] = (char)('a' + (getrandom8() % ('z' - 'a')));
					break;
				case 2: /* Digit. */
					keyBuf[charIndex] = (char)('0' + (getrandom8() % ('9' - '0')));
					break;
			}
		}
		keyBuf[keyLen] = '\0';
		(void)fprintf(stderr, "%s\n", keyBuf);
	}
}
#endif

#ifndef _WIN32
int main(int argc, char *argv[])
#else
int InitwIRCD(int argc, char *argv[])
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
#ifndef NO_FDLIST
	TS   nextfdlistcheck = 0;	/*end of priority code */
#endif
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
	if (chdir(dpath)) {
		perror("chdir");
		fprintf(stderr, "ERROR: Unable to change to directory '%s'\n", dpath);
		exit(-1);
	}
	if (geteuid() != 0)
		fprintf(stderr, "WARNING: IRCd compiled with CHROOTDIR but effective user id is not root!? "
		                "Booting is very likely to fail...\n");
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
	if (chroot(DPATH)) {
		(void)fprintf(stderr, "ERROR:  Cannot (chdir/)chroot to directory '%s'\n", dpath);
		exit(5);
	}
#endif	 /*CHROOTDIR*/
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
	bzero((char *)&me, sizeof(me));
	bzero(&StatsZ, sizeof(StatsZ));
	setup_signals();
	charsys_reset();
	init_ircstats();
#ifdef USE_LIBCURL
	url_init();
#endif
	tkl_init();
	umode_init();
#ifdef EXTCMODE
	extcmode_init();
#endif
	extban_init();
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
		  case 'd':
			  (void)setuid((uid_t) uid);
#else
		  case 'd':
#endif
			  dpath = p;
			  break;
		  case 'F':
			  bootopt |= BOOT_NOFORK;
			  break;
#ifndef _WIN32
		  case 'f':
#ifndef CMDLINE_CONFIG
		      if ((uid == euid) && (gid == egid))
			       configfile = p;
			  else
			       printf("ERROR: Command line config with a setuid/setgid ircd is not allowed");
#else
			  (void)setuid((uid_t) uid);
			  configfile = p;
#endif
			  break;
		  case 'h':
			  if (!strchr(p, '.')) {

				  (void)printf
				      ("ERROR: %s is not valid: Server names must contain at least 1 \".\"\n",
				      p);
				  exit(1);
			  }
			  strncpyzt(me.name, p, sizeof(me.name));
			  break;
#endif
#ifndef _WIN32
		  case 'P':{
			  short type;
			  char *result;
			  srandom(TStime());
			  if ((type = Auth_FindType(p)) == -1) {
				  printf("No such auth type %s\n", p);
				  exit(0);
			  }
			  p = *++argv;
			  argc--;
#ifdef AUTHENABLE_UNIXCRYPT
			  if ((type == AUTHTYPE_UNIXCRYPT) && (strlen(p) > 8))
			  {
			      printf("WARNING: Password truncated to 8 characters due to 'crypt' algorithm. "
		                 "You are suggested to use the 'md5' algorithm instead.");
				  p[8] = '\0';
			  }
#endif
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
			  (void)setuid((uid_t) uid);
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
# ifndef _WIN32
			  (void)setuid((uid_t) uid);
# endif
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
		  default:
			  bad_command();
			  break;
		}
	}

	do_version_check();

#ifndef	CHROOTDIR
	if (chdir(dpath)) {
# ifndef _WIN32
		perror("chdir");
		fprintf(stderr, "ERROR: Unable to change to directory '%s'\n", dpath);
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
	mkdir("tmp", S_IRUSR|S_IWUSR|S_IXUSR); /* Create the tmp dir, if it doesn't exist */
#else
	mkdir("tmp");
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
		return bad_command();	/* This should exit out */
#endif
#ifndef _WIN32
	fprintf(stderr, "%s", unreallogo);
	fprintf(stderr, "                           v%s\n", VERSIONONLY);
	fprintf(stderr, "                     using %s\n", tre_version());
#ifdef USE_SSL
	fprintf(stderr, "                     using %s\n", SSLeay_version(SSLEAY_VERSION));
#endif
#ifdef ZIP_LINKS
	fprintf(stderr, "                     using zlib %s\n", zlibVersion());
#endif
#ifdef USE_LIBCURL
	fprintf(stderr, "                     using %s\n", curl_version());
#endif
	fprintf(stderr, "\n");
#endif
	clear_client_hash_table();
	clear_channel_hash_table();
	clear_watch_hash_table();
	bzero(&loop, sizeof(loop));
	init_CommandHash();
	initlists();
	initwhowas();
	initstats();
	DeleteTempModules();
	booted = FALSE;
/* Hack to stop people from being able to read the config file */
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	chmod(CPATH, DEFAULT_PERMISSIONS);
#endif
	init_dynconf();
#ifdef STATIC_LINKING
	{
		ModuleInfo ModCoreInfo;
		ModCoreInfo.size = sizeof(ModuleInfo);
		ModCoreInfo.module_load = 0;
		ModCoreInfo.handle = NULL;
		l_commands_Test(&ModCoreInfo);
	}
#endif
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
	make_cmodestr();
#ifdef EXTCMODE
	make_extcmodestr();
#endif
	make_extbanstr();
	isupport_init();
	if (!find_Command_simple("AWAY") /*|| !find_Command_simple("KILL") ||
		!find_Command_simple("OPER") || !find_Command_simple("PING")*/)
	{ 
		config_error("Someone forgot to load modules with proper commands in them. READ THE DOCUMENTATION");
#ifdef _WIN32
		/* Temporary! */
		config_error("As of Unreal3.2.1 modules are supported on windows, "
		    "therefore you MUST load the commands.dll module and a cloaking module. "
		    "Just add 'loadmodule \"modules/commands.dll\"' and 'loadmodule \"modules/cloak.dll\"' "
		    "to your unrealircd.conf and be sure to read the release notes!");
		win_error();
#endif
		exit(-4);
	}

#ifdef USE_SSL
#ifndef _WIN32
	fprintf(stderr, "* Initializing SSL.\n");
#endif
	init_ssl();
#endif
#ifndef _WIN32
	fprintf(stderr,
	    "* Dynamic configuration initialized .. booting IRCd.\n");
	fprintf(stderr,
	    "---------------------------------------------------------------------\n");
#endif
	open_debugfile();
#ifndef NO_FDLIST
	init_fdlist(&serv_fdlist);
	init_fdlist(&busycli_fdlist);
	init_fdlist(&default_fdlist);
	init_fdlist(&oper_fdlist);
	init_fdlist(&unknown_fdlist);
	{
		int  i;
		for (i = MAXCONNECTIONS + 1; i > 0; i--)
			default_fdlist.entry[i] = i;
	}
#endif
	if (portnum < 0)
		portnum = PORTNUM;
	me.port = portnum;
	(void)init_sys();
	me.flags = FLAGS_LISTEN;
	me.fd = -1;
	SetMe(&me);
	make_server(&me);
#ifdef HAVE_SYSLOG
	openlog("ircd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif
	/*
	 * Put in our info 
	 */
	strncpyzt(me.info, conf_me->info, sizeof(me.info));
	strncpyzt(me.name, conf_me->name, sizeof(me.name));
	/*
	 * We accept the first listen record 
	 */
	portnum = conf_listen->port;
/*
 *      This is completely unneeded-Sts
   	me.ip.S_ADDR =
	    *conf_listen->ip != '*' ? inet_addr(conf_listen->ip) : INADDR_ANY;
*/
	Debug((DEBUG_ERROR, "Port = %d", portnum));
	if (inetport(&me, conf_listen->ip, portnum))
		exit(1);
	set_non_blocking(me.fd, &me);
	conf_listen->options |= LISTENER_BOUND;
	me.umodes = conf_listen->options;
	conf_listen->listener = &me;
	run_configuration();
	botmotd = (aMotd *) read_file(BPATH, NULL);
	rules = (aMotd *) read_file(RPATH, NULL);
	opermotd = (aMotd *) read_file(OPATH, NULL);
	motd = (aMotd *) read_file_ex(MPATH, NULL, &motd_tm);
	smotd = (aMotd *) read_file_ex(SMPATH, NULL, &smotd_tm);
	svsmotd = (aMotd *) read_file(VPATH, NULL);
	strncpy(me.sockhost, conf_listen->ip, sizeof(me.sockhost) - 1);
	if (me.name[0] == '\0')
		strncpyzt(me.name, me.sockhost, sizeof(me.name));
	me.hopcount = 0;
	me.authfd = -1;
	me.next = NULL;
	me.user = NULL;
	me.from = &me;
	me.class = (ConfigItem_class *) conf_listen;
	/*
	 * This listener will never go away 
	 */
	conf_listen->clients++;
	me_hash = find_or_add(me.name);
	me.serv->up = me_hash;
	me.serv->numeric = conf_me->numeric;
	add_server_to_table(&me);
	timeofday = time(NULL);
	me.lasttime = me.since = me.firsttime = TStime();
	(void)add_to_client_hash_table(me.name, &me);
#if !defined(_AMIGA) && !defined(_WIN32) && !defined(NO_FORKING)
	if (!(bootopt & BOOT_NOFORK))
		if (fork())
			exit(0);
#endif
	(void)ircsprintf(REPORT_DO_DNS, ":%s %s", me.name, BREPORT_DO_DNS);
	(void)ircsprintf(REPORT_FIN_DNS, ":%s %s", me.name, BREPORT_FIN_DNS);
	(void)ircsprintf(REPORT_FIN_DNSC, ":%s %s", me.name, BREPORT_FIN_DNSC);
	(void)ircsprintf(REPORT_FAIL_DNS, ":%s %s", me.name, BREPORT_FAIL_DNS);
	(void)ircsprintf(REPORT_DO_ID, ":%s %s", me.name, BREPORT_DO_ID);
	(void)ircsprintf(REPORT_FIN_ID, ":%s %s", me.name, BREPORT_FIN_ID);
	(void)ircsprintf(REPORT_FAIL_ID, ":%s %s", me.name, BREPORT_FAIL_ID);
	R_do_dns = strlen(REPORT_DO_DNS);
	R_fin_dns = strlen(REPORT_FIN_DNS);
	R_fin_dnsc = strlen(REPORT_FIN_DNSC);
	R_fail_dns = strlen(REPORT_FAIL_DNS);
	R_do_id = strlen(REPORT_DO_ID);
	R_fin_id = strlen(REPORT_FIN_ID);
	R_fail_id = strlen(REPORT_FAIL_ID);

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
			/*
			 * run as a specified user 
			 */

			(void)fprintf(stderr, "WARNING: ircd invoked as root\n");
			(void)fprintf(stderr, "         changing to uid %d\n", irc_uid);
			(void)fprintf(stderr, "         changing to gid %d\n", irc_gid);
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
	if (TIMESYNCH)
	{
		if (!unreal_time_synch(TIMESYNCH_TIMEOUT))
			ircd_log(LOG_ERROR, "TIME SYNCH: Unable to synchronize time: %s. This happens sometimes, no error on your part.",
				unreal_time_synch_error());
	}
	write_pidfile();
	Debug((DEBUG_NOTICE, "Server ready..."));
	SetupEvents();
#ifdef THROTTLING
	init_throttling_hash();
#endif
#ifdef NEWCHFLOODPROT
	init_modef();
#endif
	loop.do_bancheck = 0;
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
	module_loadall(0);
#ifdef STATIC_LINKING
	l_commands_Load(0);
#endif

#ifndef NO_FDLIST
	check_fdlists(TStime());
#endif

#ifdef _WIN32
	return 1;
}


void SocketLoop(void *dummy)
{
	TS   delay = 0;
	static TS lastglinecheck = 0;
	TS   last_tune;
#ifndef NO_FDLIST
	TS   nextfdlistcheck = 0;	/*end of priority code */
#endif
	
	
	while (1)
#else
	nextping = timeofday;
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
		if (timeofday - oldtimeofday < NEGATIVE_SHIFT_WARN) {
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
			nextfdlistcheck = 0;
		} else
		if ((oldtimeofday > 0) && (timeofday - oldtimeofday > POSITIVE_SHIFT_WARN)) /* do not set too low or you get false positives */
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
			nextfdlistcheck = 0;
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

#ifndef NO_FDLIST
		lastrecvK = me.receiveK;
		lastsendK = me.sendK;
#endif
		if (IRCstats.clients > IRCstats.global_max)
			IRCstats.global_max = IRCstats.clients;
		if (IRCstats.me_clients > IRCstats.me_max)
			IRCstats.me_max = IRCstats.me_clients;
		/*
		 * ** We only want to connect if a connection is due,
		 * ** not every time through.  Note, if there are no
		 * ** active C lines, this call to Tryconnections is
		 * ** made once only; it will return 0. - avalon
		 */
		if (nextconnect && timeofday >= nextconnect)
			nextconnect = try_connections(timeofday);

		/*
		 * ** take the smaller of the two 'timed' event times as
		 * ** the time of next event (stops us being late :) - avalon
		 * ** WARNING - nextconnect can return 0!
		 */
		if (nextconnect)
			delay = MIN(nextping, nextconnect);
		else
			delay = nextping;
		delay = MIN(nextdnscheck, delay);
		delay = MIN(nextexpire, delay);
		delay -= timeofday;
		/*
		 * ** Adjust delay to something reasonable [ad hoc values]
		 * ** (one might think something more clever here... --msa)
		 * ** We don't really need to check that often and as long
		 * ** as we don't delay too long, everything should be ok.
		 * ** waiting too long can cause things to timeout...
		 * ** i.e. PINGS -> a disconnection :(
		 * ** - avalon
		 */
		if (delay < 1)
			delay = 1;
		else
			delay = MIN(delay, TIMESEC);
#ifdef NO_FDLIST
		(void)read_message(delay);
		timeofday = time(NULL) + TSoffset;
#else
		(void)read_message(0, &serv_fdlist);	/* servers */
		(void)read_message(1, &busycli_fdlist);	/* busy clients */
		if (lifesux) {
			static time_t alllasttime = 0;

			(void)read_message(1, &serv_fdlist);
			/*
			 * read servs more often 
			 */
			if (lifesux > 9) {	/* life really sucks */
				(void)read_message(1, &busycli_fdlist);
				(void)read_message(1, &serv_fdlist);
			}
			flush_fdlist_connections(&serv_fdlist);
			timeofday = time(NULL) + TSoffset;
			if ((alllasttime + (lifesux + 1)) < timeofday) {
				read_message(delay, NULL);	/*  check everything */
				alllasttime = timeofday;
			}
		} else {
			read_message(delay, NULL);	/*  check everything */
			timeofday = time(NULL) + TSoffset;
		}

#endif
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
#ifdef NO_FDLIST
		if (timeofday >= nextping || loop.do_bancheck)
#else
		if ((timeofday >= nextping && !lifesux) || loop.do_bancheck)
#endif
			nextping = check_pings(timeofday);
		if (dorehash) 
		{
			(void)rehash(&me, &me, 1);
			dorehash = 0;
		}
		if (dorestart)
		{
			server_reboot("SIGINT");
		}

		/*
		 * ** Flush output buffers on all connections timeofday if they
		 * ** have data in them (or at least try to flush)
		 * ** -avalon
		 */

#ifndef NO_FDLIST
		/*
		 * check which clients are active 
		 */
		if (timeofday > nextfdlistcheck)
			nextfdlistcheck = check_fdlists(timeofday);
#endif
		flush_connections(&me);

		/* ThA UnReAl TrOuBlE RePoRtInG SyStEm!!! */
		if (trouble_info[0] != '\0')
		{
			sendto_realops("*** TROUBLE: %s ***", trouble_info);
			ircd_log(LOG_ERROR, "TROUBLE: %s", trouble_info);
			trouble_info[0] = '\0';
		}
	}
}

/*
 * open_debugfile
 *
 * If the -t option is not given on the command line when the server is
 * started, all debugging output is sent to the file set by LPATH in config.h
 * Here we just open that file and make sure it is opened to fd 2 so that
 * any fprintf's to stderr also goto the logfile.  If the debuglevel is not
 * set from the command line by -x, use /dev/null as the dummy logfile as long
 * as DEBUGMODE has been defined, else dont waste the fd.
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
		cptr->port = debuglevel;
		cptr->flags = 0;
		cptr->listener = cptr;
		/*
		 * local[2] = cptr;  winlocal 
		 */
		(void)strlcpy(cptr->sockhost, me.sockhost,
		    sizeof cptr->sockhost);
# ifndef _WIN32
		(void)printf("isatty = %d ttyname = %#x\n",
		    isatty(2), (u_int)ttyname(2));
		if (!(bootopt & BOOT_TTY)) {	/* leave debugging output on fd 2 */
			(void)truncate(LOGFILE, 0);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0)
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);
			if (fd != 2) {
				(void)dup2(fd, 2);
				(void)close(fd);
			}
			strncpyzt(cptr->name, LOGFILE, sizeof(cptr->name));
		} else if (isatty(2) && ttyname(2))
			strncpyzt(cptr->name, ttyname(2), sizeof(cptr->name));
		else
# endif
			(void)strcpy(cptr->name, "FD2-Pipe");
		Debug((DEBUG_FATAL,
		    "Debug: File <%s> Level: %d at %s", cptr->name,
		    cptr->port, myctime(time(NULL))));
	} else
		/*
		 * local[2] = NULL; winlocal 
		 */
#endif
		return;
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
