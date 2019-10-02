/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/ircd.c
 *   Copyright (C) 1989-1990 Jarkko Oikarinen and
 *                 University of Oulu, Computing Center
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

#include "unrealircd.h"

#ifdef __FreeBSD__
char *malloc_options = "h" MALLOC_FLAGS_EXTRA;
#endif

#ifndef _WIN32
extern char unreallogo[];
#endif
int  SVSNOOP = 0;
extern MODVAR char *buildid;
time_t timeofday = 0;
struct timeval timeofday_tv;
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
IRCCounts irccounts;
Client me;			/* That's me */
MODVAR char *me_hash;
extern char backupbuf[8192];
#ifdef _WIN32
extern SERVICE_STATUS_HANDLE IRCDStatusHandle;
extern SERVICE_STATUS IRCDStatus;
#endif

MODVAR unsigned char conf_debuglevel = 0;

#ifdef USE_LIBCURL
extern void url_init(void);
#endif

void server_reboot(char *);
void restart(char *);
static void open_debugfile(), setup_signals();
extern void init_glines(void);
extern void tkl_init(void);
extern void process_clients(void);

#ifndef _WIN32
MODVAR char **myargv;
#else
LPCSTR cmdLine;
#endif
char *configfile = NULL; 	/* Server configuration file */
int  debuglevel = 0;		/* Server debug level */
int  bootopt = 0;		/* Server boot option flags */
char *debugmode = "";		/*  -"-    -"-   -"-  */
char *sbrk0;			/* initial sbrk(0) */
static int dorehash = 0, dorestart = 0, doreloadcert = 0;
MODVAR int  booted = FALSE;

void s_die()
{
#ifdef _WIN32
	Client *cptr;
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
static void s_rehash()
{
	struct sigaction act;
	dorehash = 1;
	act.sa_handler = s_rehash;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGHUP);
	(void)sigaction(SIGHUP, &act, NULL);
}

static void s_reloadcert()
{
	struct sigaction act;
	doreloadcert = 1;
	act.sa_handler = s_reloadcert;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGUSR1);
	(void)sigaction(SIGUSR1, &act, NULL);
}
#endif // #ifndef _WIN32

void restart(char *mesg)
{
	server_reboot(mesg);
}

void s_restart()
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
/** Signal handler for signals which we ignore,
 * like SIGPIPE ("Broken pipe") and SIGWINCH (terminal window changed) etc.
 */
void ignore_this_signal()
{
	struct sigaction act;

	act.sa_handler = ignore_this_signal;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGALRM);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaction(SIGALRM, &act, (struct sigaction *)NULL);
	(void)sigaction(SIGPIPE, &act, (struct sigaction *)NULL);
#ifdef SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, (struct sigaction *)NULL);
#endif
}
#endif /* #ifndef _WIN32 */


void server_reboot(char *mesg)
{
	int i;
	Client *cptr;
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
		memset(&status, 0, sizeof(status));
		memset(&si, 0, sizeof(si));
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
			safe_free(p.next);
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
	Client *cptr;
	int  confrq;
	ConfigItem_class *class;

	for (aconf = conf_link; aconf; aconf = aconf->next)
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

		cptr = find_client(aconf->servername, NULL);
		if (cptr)
			continue; /* Server already connected (or connecting) */

		if (class->clients >= class->maxclients)
			continue; /* Class is full */

		/* Check connect rules to see if we're allowed to try the link */
		for (deny = conf_deny_link; deny; deny = deny->next)
			if (match_simple(deny->mask, aconf->servername) && crule_eval(deny->rule))
				break;

		if (!deny && connect_server(aconf, NULL, NULL) == 0)
			sendto_realops("Connection to %s[%s] activated.",
				aconf->servername, aconf->outgoing.hostname);

	}
}

int check_tkls(Client *cptr)
{
	ConfigItem_ban *bconf = NULL;
	char banbuf[1024];

	char killflag = 0;

	/* Process dynamic *LINES */
	if (find_tkline_match(cptr, 0) < 0)
		return 0; /* stop processing this user, as (s)he is dead now. */

	find_shun(cptr); /* check for shunned and take action, if so */

	if (IsUser(cptr))
	{
		/* Check ban realname { } */
		if (!ValidatePermissionsForPath("immune",cptr,NULL,NULL,NULL) && (bconf = Find_ban(NULL, cptr->info, CONF_BAN_REALNAME)))
			killflag++;
	}

	/* If user is meant to be killed, take action: */
	if (killflag)
	{
		if (IsUser(cptr))
			sendto_realops("Ban active for %s (%s)",
				get_client_name(cptr, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (IsServer(cptr))
			sendto_realops("Ban active for server %s (%s)",
				get_client_name(cptr, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (bconf->reason) {
			if (IsUser(cptr))
				snprintf(banbuf, sizeof(banbuf), "User has been banned (%s)", bconf->reason);
			else
				snprintf(banbuf, sizeof(banbuf), "Banned (%s)", bconf->reason);
			(void)exit_client(cptr, cptr, &me, NULL, banbuf);
		} else {
			if (IsUser(cptr))
				(void)exit_client(cptr, cptr, &me, NULL, "User has been banned");
			else
				(void)exit_client(cptr, cptr, &me, NULL, "Banned");
		}
		return 0; /* stop processing this user, as (s)he is dead now. */
	}

	if (loop.do_bancheck_spamf_user && IsUser(cptr) && find_spamfilter_user(cptr, SPAMFLAG_NOWARN) == FLUSH_BUFFER)
		return 0;

	if (loop.do_bancheck_spamf_away && IsUser(cptr) && cptr->user->away != NULL &&
		run_spamfilter(cptr, cptr->user->away, SPAMF_AWAY, NULL, SPAMFLAG_NOWARN, NULL) == FLUSH_BUFFER)
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
	Client *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &unknown_list, lclient_node)
	{
		if (cptr->local->firsttime && ((TStime() - cptr->local->firsttime) > iConf.handshake_timeout))
		{
			if (cptr->serv && *cptr->serv->by)
			{
				/* If this is a handshake timeout to an outgoing server then notify ops & log it */
				sendto_ops_and_log("Connection handshake timeout while connecting to server '%s' (%s)",
					cptr->name, cptr->ip?cptr->ip:"<unknown ip>");
			}

			(void)exit_client(cptr, cptr, &me, NULL, "Registration Timeout");
			continue;
		}
	}
}

/** Ping individual user, and check for ping timeout */
int check_ping(Client *cptr)
{
	char scratch[64];
	int ping = 0;

	ping = cptr->local->class ? cptr->local->class->pingfreq : iConf.handshake_timeout;
	Debug((DEBUG_DEBUG, "c(%s)=%d p %d a %lld", cptr->name,
		cptr->status, ping,
		(long long)(TStime() - cptr->local->lasttime)));

	/* If ping is less than or equal to the last time we received a command from them */
	if (ping > (TStime() - cptr->local->lasttime))
		return 0; /* some recent command was executed */

	if (
		/* If we have sent a ping */
		(IsPingSent(cptr)
		/* And they had 2x ping frequency to respond */
		&& ((TStime() - cptr->local->lasttime) >= (2 * ping)))
		||
		/* Or isn't registered and time spent is larger than ping (CONNECTTIMEOUT).. */
		(!IsRegistered(cptr) && (TStime() - cptr->local->since >= ping))
		)
	{
		if (IsServer(cptr) || IsConnecting(cptr) ||
		    IsHandshake(cptr) || IsTLSConnectHandshake(cptr))
		{
			sendto_ops_and_log
				("No response from %s, closing link",
				get_client_name(cptr, FALSE));
			sendto_server(&me, 0, 0, NULL,
				":%s GLOBOPS :No response from %s, closing link",
				me.name, get_client_name(cptr,
				FALSE));
		}
		if (IsTLSAcceptHandshake(cptr))
			Debug((DEBUG_DEBUG, "ssl accept handshake timeout: %s (%lld-%lld > %lld)", cptr->local->sockhost,
				(long long)TStime(), (long long)cptr->local->since, (long long)ping));
		(void)ircsnprintf(scratch, sizeof(scratch), "Ping timeout: %lld seconds",
			(long long) (TStime() - cptr->local->lasttime));
		return exit_client(cptr, cptr, &me, NULL, scratch);
	}
	else if (IsRegistered(cptr) && !IsPingSent(cptr))
	{
		/* Time to send a PING */
		SetPingSent(cptr);
		ClearPingWarning(cptr);
		/* not nice but does the job */
		cptr->local->lasttime = TStime() - ping;
		sendto_one(cptr, NULL, "PING :%s", me.name);
	}
	else if (!IsPingWarning(cptr) && PINGWARNING > 0 &&
		(IsServer(cptr) || IsHandshake(cptr) || IsConnecting(cptr) ||
		IsTLSConnectHandshake(cptr)) &&
		(TStime() - cptr->local->lasttime) >= (ping + PINGWARNING))
	{
		SetPingWarning(cptr);
		sendto_realops("Warning, no response from %s for %d seconds",
			get_client_name(cptr, FALSE), PINGWARNING);
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
	Client *cptr, *cptr2;

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
	Client *cptr, *cptr2;

	list_for_each_entry_safe(cptr, cptr2, &unknown_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when dead socket is set */
		if (IsDeadSocket(cptr))
		{
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", cptr->local->fd, cptr->name);
#endif
			ClearDeadSocket(cptr); /* CPR. So we send the error. */
			(void)exit_client(cptr, cptr, &me, NULL, cptr->local->error_str ? cptr->local->error_str : "Dead socket");
			continue;
		}
	}

	list_for_each_entry_safe(cptr, cptr2, &lclient_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when dead socket is set */
		if (IsDeadSocket(cptr))
		{
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", cptr->local->fd, cptr->name);
#endif
			ClearDeadSocket(cptr); /* CPR. So we send the error. */
			(void)exit_client(cptr, cptr, &me, NULL, cptr->local->error_str ? cptr->local->error_str : "Dead socket");
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
	    ("Usage: %s [-f <config>] [-F]\n"
	     "\n"
	     "UnrealIRCd\n"
	     " -f <config>     Load configuration from <config> instead of the default\n"
	     "                 (%s).\n"
	     " -F              Don't fork() when starting up. Use this when running\n"
	     "                 UnrealIRCd under gdb or when playing around with settings\n"
	     "                 on a non-production setup.\n"
	     "\n",
	     argv0, CONFIGFILE);
	(void)printf("Server not started\n\n");
#else
	if (!IsService) {
		MessageBox(NULL,
		    "Usage: UnrealIRCd [-f configfile]\n",
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

extern void applymeblock(void);

extern MODVAR Event *events;

/** This functions resets a couple of timers and does other things that
 * are absolutely cruicial when the clock is adjusted - particularly
 * when the clock goes backwards. -- Syzop
 */
void fix_timers(void)
{
	int i, cnt;
	Client *acptr;
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
		if (MyUser(acptr))
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
		if (e->last_run.tv_sec > TStime())
		{
			e->last_run.tv_sec = TStime()-1;
			e->last_run.tv_usec = 0;
		}
	}

	/* For throttling we only have to deal with time jumping backward, which
	 * is a real problem as if the jump was, say, 900 seconds, then it would
	 * (potentially) throttle for 900 seconds.
	 * Time going forward is "no problem", it just means we expire our entries
	 * sonner than we should.
	 */
	cnt = 0;
	for (i = 0; i < THROTTLING_HASH_TABLE_SIZE; i++)
	{
		for (thr = ThrottlingHash[i]; thr; thr = thr->next)
		{
			if (thr->since > TStime())
				thr->since = TStime();
		}
	}
	Debug((DEBUG_DEBUG, "fix_timers(): removed %d throttling item(s)", cnt));

	/* Make sure autoconnect for servers still works (lnk->hold) */
	for (lnk = conf_link; lnk; lnk = lnk->next)
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

#define NEGATIVE_SHIFT_WARN	-15
#define POSITIVE_SHIFT_WARN	20

void detect_timeshift_and_warn(void)
{
	static time_t highesttimeofday=0, oldtimeofday=0, lasthighwarn=0;

	if (oldtimeofday == 0)
		oldtimeofday = timeofday; /* pretend everything is ok the first time.. */

	if (mytdiff(timeofday, oldtimeofday) < NEGATIVE_SHIFT_WARN) {
		/* tdiff = # of seconds of time set backwards (positive number! eg: 60) */
		time_t tdiff = oldtimeofday - timeofday;
		ircd_log(LOG_ERROR, "WARNING: Time running backwards! Clock set back ~%lld seconds (%lld -> %lld)",
			(long long)tdiff, (long long)oldtimeofday, (long long)timeofday);
		ircd_log(LOG_ERROR, "[TimeShift] Resetting a few timers to prevent IRCd freeze!");
		sendto_realops("WARNING: Time running backwards! Clock set back ~%lld seconds (%lld -> %lld)",
			(long long)tdiff, (long long)oldtimeofday, (long long)timeofday);
		sendto_realops("Incorrect time for IRC servers is a serious problem. "
			       "Time being set backwards (system clock changed) is "
			       "even more serious and can cause clients to freeze, channels to be "
			       "taken over, and other issues.");
		sendto_realops("Please be sure your clock is always synchronized before "
			       "the IRCd is started!");
		sendto_realops("[TimeShift] Resetting a few timers to prevent IRCd freeze!");
		fix_timers();
	} else
	if (mytdiff(timeofday, oldtimeofday) > POSITIVE_SHIFT_WARN) /* do not set too low or you get false positives */
	{
		/* tdiff = # of seconds of time set forward (eg: 60) */
		time_t tdiff = timeofday - oldtimeofday;
		ircd_log(LOG_ERROR, "WARNING: Time jumped ~%lld seconds ahead! (%lld -> %lld)",
			(long long)tdiff, (long long)oldtimeofday, (long long)timeofday);
		ircd_log(LOG_ERROR, "[TimeShift] Resetting some timers!");
		sendto_realops("WARNING: Time jumped ~%lld seconds ahead! (%lld -> %lld)",
			(long long)tdiff, (long long)oldtimeofday, (long long)timeofday);
		sendto_realops("Incorrect time for IRC servers is a serious problem. "
			       "Time being adjusted (by changing the system clock) "
			       "more than a few seconds forward/backward can lead to serious issues.");
		sendto_realops("Please be sure your clock is always synchronized before "
			       "the IRCd is started!");
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
				"Waiting for time to be OK again. This will be in %lld seconds",
				(long long)(highesttimeofday - timeofday));
			sendto_realops("[TimeShift] The (IRCd) clock was set backwards. Timers, nick- "
				       "and channel-timestamps are possibly incorrect. This message will "
				       "repeat itself until we catch up with the original time, which will be "
				       "in %lld seconds", (long long)(highesttimeofday - timeofday));
			lasthighwarn = timeofday;
		}
	} else {
		highesttimeofday = timeofday;
	}

	oldtimeofday = timeofday;
}

/** Check if at least 'minimum' seconds passed by since last run.
 * @param tv_old   Pointer to a timeval struct to keep track of things.
 * @param minimum  The time specified in milliseconds (eg: 1000 for 1 second)
 * @returns When 'minimum' msec passed 1 is returned and the time is reset, otherwise 0 is returned.
 */
int minimum_msec_since_last_run(struct timeval *tv_old, long minimum)
{
	long v;

	if (tv_old->tv_sec == 0)
	{
		/* First call ever */
		tv_old->tv_sec = timeofday_tv.tv_sec;
		tv_old->tv_usec = timeofday_tv.tv_usec;
		return 0;
	}
	v = ((timeofday_tv.tv_sec - tv_old->tv_sec) * 1000) + ((timeofday_tv.tv_usec - tv_old->tv_usec)/1000);
	if (v >= minimum)
	{
		tv_old->tv_sec = timeofday_tv.tv_sec;
		tv_old->tv_usec = timeofday_tv.tv_usec;
		return 1;
	}
	return 0;
}

/** The main function. This will call SocketLoop() once the server is ready. */
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
#endif
#ifdef HAVE_PSTAT
	union pstun pstats;
#endif
#ifndef _WIN32
	struct rlimit corelim;
#endif

	gettimeofday(&timeofday_tv, NULL);
	timeofday = timeofday_tv.tv_sec;

	safe_strdup(configfile, CONFIGFILE);

	init_random(); /* needs to be done very early!! */

	memset(&botmotd, '\0', sizeof(MOTDFile));
	memset(&rules, '\0', sizeof(MOTDFile));
	memset(&opermotd, '\0', sizeof(MOTDFile));
	memset(&motd, '\0', sizeof(MOTDFile));
	memset(&smotd, '\0', sizeof(MOTDFile));
	memset(&svsmotd, '\0', sizeof(MOTDFile));
	memset(&me, 0, sizeof(me));
	me.local = safe_alloc(sizeof(LocalClient));
	memset(&loop, 0, sizeof(loop));

	init_hash();

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

	if (euid == 0)
	{
		fprintf(stderr,
			"** ERROR **\n"
			"You attempted to run UnrealIRCd as root. This is VERY DANGEROUS\n"
			"as any compromise of your UnrealIRCd will result in full\n"
			"privileges to the attacker on the entire machine.\n"
			"You MUST start UnrealIRCd as a different user!\n"
			"\n"
			"For more information, see:\n"
			"https://www.unrealircd.org/docs/Do_not_run_as_root\n"
			"\n");
		exit(1);
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
	memset(&StatsZ, 0, sizeof(StatsZ));
	setup_signals();

	memset(&irccounts, '\0', sizeof(irccounts));
	irccounts.servers = 1;

	mp_pool_init();
	dbuf_init();
	initlists();

#ifdef USE_LIBCURL
	url_init();
#endif
	tkl_init();
	umode_init();
	extcmode_init();
	efunctions_init();
	clear_scache_hash_table();
#ifndef _WIN32
	/* Make it so we can dump core */
	corelim.rlim_cur = corelim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &corelim);
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
		switch (flag)
		{
			case 'F':
				bootopt |= BOOT_NOFORK;
				break;
			case 'f':
#ifndef _WIN32
				if ((uid != euid) || (gid != egid))
				{
					printf("ERROR: Command line config with a setuid/setgid ircd is not allowed");
					exit(1);
				}
#endif
				safe_strdup(configfile, p);
				convert_to_absolute_path(&configfile, CONFDIR);
				break;
#ifndef _WIN32
		  case 'P':{
			  short type;
			  char *result;
			  srandom(TStime());
			  type = Auth_FindType(NULL, p);
			  if (type == -1)
			  {
			      type = AUTHTYPE_ARGON2;
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
		                 "You are suggested to use the 'argon2' algorithm instead.");
				  p[8] = '\0';
			  }
			  if (!(result = Auth_Hash(type, p))) {
				  printf("Failed to generate password. Deprecated method? Try 'argon2' instead.\n");
				  exit(0);
			  }
			  printf("Encrypted password is: %s\n", result);
			  exit(0);
		  }
#endif
#if 0
		case 'S':
			charsys_dump_table(p ? p : "*");
			exit(0);
#endif
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
		      if (chdir(CONFDIR) < 0)
	{
		      	fprintf(stderr, "Unable to change to '%s' directory\n", CONFDIR);
		      	exit(1);
		      }
		      update_conf();
		      exit(0);
		  case 'R':
		      report_crash();
		      exit(0);
		  case '8':
		      utf8_test();
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

#if !defined(_WIN32)
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
	fprintf(stderr, "UnrealIRCd is brought to you by Bram Matthys (Syzop), Gottem and i\n\n");

	fprintf(stderr, "Using the following libraries:\n");
	fprintf(stderr, "* %s\n", pcre2_version());
	fprintf(stderr, "* %s\n", SSLeay_version(SSLEAY_VERSION));
#ifdef USE_LIBCURL
	fprintf(stderr, "* %s\n", curl_version());
#endif
#endif
	check_user_limit();
#ifndef _WIN32
	fprintf(stderr, "\n");
	fprintf(stderr, "This server can handle %d concurrent sockets (%d clients + %d reserve)\n\n",
		maxclients+CLIENTS_RESERVE, maxclients, CLIENTS_RESERVE);
#endif
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
	early_init_ssl();
	/*
	 * Add default class
	 */
	default_class = safe_alloc(sizeof(ConfigItem_class));
	default_class->flag.permanent = 1;
	default_class->pingfreq = 120;
	default_class->maxclients = 100;
	default_class->sendq = DEFAULT_RECVQ;
	default_class->name = "default";
	AddListItem(default_class, conf_class);
	if (init_conf(configfile, 0) < 0)
	{
		exit(-1);
	}
	booted = TRUE;
	load_tunefile();
	make_umodestr();
	SetListening(&me);
	me.local->fd = -1;
	SetMe(&me);
	make_server(&me);
	extcmodes_check_for_changes();
	umodes_check_for_changes();
	charsys_check_for_changes();
	clicap_init();
	if (!find_Command_simple("AWAY") /*|| !find_Command_simple("KILL") ||
		!find_Command_simple("OPER") || !find_Command_simple("PING")*/)
	{
		config_error("Someone forgot to load modules with proper commands in them. READ THE DOCUMENTATION");
		exit(-4);
	}

#ifndef _WIN32
	fprintf(stderr, "Initializing TLS..\n");
#endif
	if (!init_ssl())
	{
		config_error("Failed to load SSL/TLS (see errors above). UnrealIRCd can not start.");
#ifdef _WIN32
		win_error(); /* display error dialog box */
#endif
		exit(9);
	}
#ifndef _WIN32
	fprintf(stderr, "Dynamic configuration initialized.. booting IRCd.\n");
#endif
	open_debugfile();
	me.local->port = 6667; /* pointless? */
	(void)init_sys();
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
	me.direction = &me;

	/*
	 * This listener will never go away
	 */
	me_hash = find_or_add(me.name);
	me.serv->up = me_hash;
	timeofday = time(NULL);
	me.local->lasttime = me.local->since = me.local->firsttime = me.serv->boottime = TStime();
	me.serv->features.protocol = UnrealProtocol;
	safe_strdup(me.serv->features.software, version);
	(void)add_to_client_hash_table(me.name, &me);
	(void)add_to_id_hash_table(me.id, &me);
	list_add(&me.client_node, &global_server_list);
#if !defined(_AMIGA) && !defined(_WIN32) && !defined(NO_FORKING)
	if (!(bootopt & BOOT_NOFORK))
	{
		pid_t p;
		p = fork();
		if (p < 0)
		{
			fprintf(stderr, "Could not create background job. Call to fork() failed: %s\n",
				strerror(errno));
			exit(-1);
		}
		if (p > 0)
		{
			/* Background job created and we are the parent. We can terminate. */
			exit(0);
		}
		/* Background process (child) continues below... */
		close_std_descriptors();
		fd_fork();
		loop.ircd_forked = 1;
	}
#endif
#ifdef _WIN32
	loop.ircd_forked = 1;
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

	fix_timers();
	write_pidfile();
	Debug((DEBUG_NOTICE, "Server ready..."));
	init_throttling();
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

#ifndef _WIN32
	SocketLoop(NULL);
#endif
	return 1;
}

/** The main loop that the server will run all the time.
 * On Windows this is a thread, on *NIX we simply jump here from main()
 * when the server is ready.
 */
void SocketLoop(void *dummy)
{
	struct timeval doevents_tv, process_clients_tv;

	memset(&doevents_tv, 0, sizeof(doevents_tv));
	memset(&process_clients_tv, 0, sizeof(process_clients_tv));

	while (1)
	{
		gettimeofday(&timeofday_tv, NULL);
		timeofday = timeofday_tv.tv_sec;

		detect_timeshift_and_warn();

		if (minimum_msec_since_last_run(&doevents_tv, 250))
			DoEvents();

		/* Update statistics */
		if (irccounts.clients > irccounts.global_max)
			irccounts.global_max = irccounts.clients;
		if (irccounts.me_clients > irccounts.me_max)
			irccounts.me_max = irccounts.me_clients;

		/* Process I/O */
		fd_select(SOCKETLOOP_MAX_DELAY);

		if (minimum_msec_since_last_run(&process_clients_tv, 200))
			process_clients();

		/* Check if there are pending "actions".
		 * These are actions that should be done outside of
		 * process_clients() and fd_select() when we are not
		 * processing any clients.
		 */
		if (dorehash)
		{
			(void)rehash(&me, 1);
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
	Client *cptr;
	if (debuglevel >= 0) {
		cptr = make_client(NULL, NULL);
		cptr->local->fd = 2;
		SetLog(cptr);
		cptr->local->port = debuglevel;
		cptr->flags = 0;

		(void)strlcpy(cptr->local->sockhost, me.local->sockhost, sizeof cptr->local->sockhost);
# ifndef _WIN32
		/*(void)printf("isatty = %d ttyname = %#x\n",
		    isatty(2), (u_int)ttyname(2)); */
		if (!(bootopt & BOOT_TTY)) {	/* leave debugging output on fd 2 */
			if (truncate(LOGFILE, 0) < 0)
				fprintf(stderr, "WARNING: could not truncate log file '%s'\n", LOGFILE);
			if ((fd = open(LOGFILE, O_WRONLY | O_CREAT, 0600)) < 0)
				if ((fd = open("/dev/null", O_WRONLY)) < 0)
					exit(-1);

#if 1
			cptr->local->fd = fd;
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
	struct sigaction act;
	act.sa_handler = SIG_IGN;
	act.sa_flags = 0;
	(void)sigemptyset(&act.sa_mask);
	(void)sigaddset(&act.sa_mask, SIGPIPE);
	(void)sigaddset(&act.sa_mask, SIGALRM);
#ifdef SIGWINCH
	(void)sigaddset(&act.sa_mask, SIGWINCH);
	(void)sigaction(SIGWINCH, &act, NULL);
#endif
	(void)sigaction(SIGPIPE, &act, NULL);
	act.sa_handler = ignore_this_signal;
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
#endif
}
