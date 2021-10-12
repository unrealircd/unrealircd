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
#include <ares.h>

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

MODVAR IRCCounts irccounts;
MODVAR Client me;			/* That's me */
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
extern void unrealdb_test(void);

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
	Client *client;
	if (!IsService)
	{
		loop.ircd_terminating = 1;
		unload_all_modules();

		list_for_each_entry(client, &lclient_list, lclient_node)
			(void) send_queued(client);

		exit(-1);
	}
	else {
		SERVICE_STATUS status;
		SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
		SC_HANDLE hService = OpenService(hSCManager, "UnrealIRCd", SERVICE_STOP);
		ControlService(hService, SERVICE_CONTROL_STOP, &status);
	}
#else
	loop.ircd_terminating = 1;
	unload_all_modules();
	unlink(conf_files ? conf_files->pid_file : IRCD_PIDFILE);
	exit(0);
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
	Client *client;
	sendto_realops("Aieeeee!!!  Restarting server... %s", mesg);
	Debug((DEBUG_NOTICE, "Restarting server... %s", mesg));

	list_for_each_entry(client, &lclient_list, lclient_node)
		(void) send_queued(client);

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

/** Perform autoconnect to servers that are not linked yet. */
EVENT(try_connections)
{
	ConfigItem_link *aconf;
	ConfigItem_deny_link *deny;
	Client *client;
	int  confrq;
	ConfigItem_class *class;

	for (aconf = conf_link; aconf; aconf = aconf->next)
	{
		/* We're only interested in autoconnect blocks that are valid. Also, we ignore temporary link blocks. */
		if (!(aconf->outgoing.options & CONNECT_AUTO) || !aconf->outgoing.hostname || (aconf->flag.temporary == 1))
			continue;

		class = aconf->class;

		/* Only do one connection attempt per <connfreq> seconds (for the same server) */
		if ((aconf->hold > TStime()))
			continue;

		confrq = class->connfreq;
		aconf->hold = TStime() + confrq;

		client = find_client(aconf->servername, NULL);
		if (client)
			continue; /* Server already connected (or connecting) */

		if (class->clients >= class->maxclients)
			continue; /* Class is full */

		/* Check connect rules to see if we're allowed to try the link */
		for (deny = conf_deny_link; deny; deny = deny->next)
			if (match_simple(deny->mask, aconf->servername) && crule_eval(deny->rule))
				break;

		if (!deny && connect_server(aconf, NULL, NULL) == 0)
			sendto_ops_and_log("Trying to activate link with server %s[%s]...",
				aconf->servername, aconf->outgoing.hostname);

	}
}

/** Does this user match any TKL's? */
int match_tkls(Client *client)
{
	ConfigItem_ban *bconf = NULL;
	char banbuf[1024];

	char killflag = 0;

	/* Process dynamic *LINES */
	if (find_tkline_match(client, 0))
		return 1; /* user killed */

	find_shun(client); /* check for shunned and take action, if so */

	if (IsUser(client))
	{
		/* Check ban realname { } */
		if (!ValidatePermissionsForPath("immune",client,NULL,NULL,NULL) && (bconf = find_ban(NULL, client->info, CONF_BAN_REALNAME)))
			killflag++;
	}

	/* If user is meant to be killed, take action: */
	if (killflag)
	{
		if (IsUser(client))
			sendto_realops("Ban active for %s (%s)",
				get_client_name(client, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (IsServer(client))
			sendto_realops("Ban active for server %s (%s)",
				get_client_name(client, FALSE),
				bconf->reason ? bconf->reason : "no reason");

		if (bconf->reason) {
			if (IsUser(client))
				snprintf(banbuf, sizeof(banbuf), "User has been banned (%s)", bconf->reason);
			else
				snprintf(banbuf, sizeof(banbuf), "Banned (%s)", bconf->reason);
			exit_client(client, NULL, banbuf);
		} else {
			if (IsUser(client))
				exit_client(client, NULL, "User has been banned");
			else
				exit_client(client, NULL, "Banned");
		}
		return 1; /* stop processing this user, as (s)he is dead now. */
	}

	if (loop.do_bancheck_spamf_user && IsUser(client) && find_spamfilter_user(client, SPAMFLAG_NOWARN))
		return 1;

	if (loop.do_bancheck_spamf_away && IsUser(client) &&
	    client->user->away != NULL &&
	    match_spamfilter(client, client->user->away, SPAMF_AWAY, "AWAY", NULL, SPAMFLAG_NOWARN, NULL))
	{
		return 1;
	}

	return 0;
}

/** Time out connections that are still in handshake. */
EVENT(handshake_timeout)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		if (client->local->firsttime && ((TStime() - client->local->firsttime) > iConf.handshake_timeout))
		{
			if (client->serv && *client->serv->by)
			{
				/* If this is a handshake timeout to an outgoing server then notify ops & log it */
				sendto_ops_and_log("Connection handshake timeout while trying to link to server '%s' (%s)",
					client->name, client->ip?client->ip:"<unknown ip>");
			}

			exit_client(client, NULL, "Registration Timeout");
			continue;
		}
	}
}

/** Ping individual user, and check for ping timeout */
void check_ping(Client *client)
{
	char scratch[64];
	int ping = 0;

	ping = client->local->class ? client->local->class->pingfreq : iConf.handshake_timeout;
	Debug((DEBUG_DEBUG, "c(%s)=%d p %d a %lld", client->name,
		client->status, ping,
		(long long)(TStime() - client->local->lasttime)));

	/* If ping is less than or equal to the last time we received a command from them */
	if (ping > (TStime() - client->local->lasttime))
		return; /* some recent command was executed */

	if (
		/* If we have sent a ping */
		(IsPingSent(client)
		/* And they had 2x ping frequency to respond */
		&& ((TStime() - client->local->lasttime) >= (2 * ping)))
		||
		/* Or isn't registered and time spent is larger than ping (CONNECTTIMEOUT).. */
		(!IsRegistered(client) && (TStime() - client->local->since >= ping))
		)
	{
		if (IsServer(client) || IsConnecting(client) ||
		    IsHandshake(client) || IsTLSConnectHandshake(client))
		{
			sendto_umode_global(UMODE_OPER, "No response from %s, closing link",
			                    get_client_name(client, FALSE));
			ircd_log(LOG_ERROR, "No response from %s, closing link",
			         get_client_name(client, FALSE));
		}
		if (IsTLSAcceptHandshake(client))
			Debug((DEBUG_DEBUG, "ssl accept handshake timeout: %s (%lld-%lld > %lld)", client->local->sockhost,
				(long long)TStime(), (long long)client->local->since, (long long)ping));
		ircsnprintf(scratch, sizeof(scratch), "Ping timeout: %lld seconds",
			(long long) (TStime() - client->local->lasttime));
		exit_client(client, NULL, scratch);
		return;
	}
	else if (IsRegistered(client) && !IsPingSent(client))
	{
		/* Time to send a PING */
		SetPingSent(client);
		ClearPingWarning(client);
		/* not nice but does the job */
		client->local->lasttime = TStime() - ping;
		sendto_one(client, NULL, "PING :%s", me.name);
	}
	else if (!IsPingWarning(client) && PINGWARNING > 0 &&
		(IsServer(client) || IsHandshake(client) || IsConnecting(client) ||
		IsTLSConnectHandshake(client)) &&
		(TStime() - client->local->lasttime) >= (ping + PINGWARNING))
	{
		SetPingWarning(client);
		sendto_realops("Warning, no response from %s for %d seconds",
			get_client_name(client, FALSE), PINGWARNING);
	}

	return;
}

/** Check registered connections for ping timeout. Also, check for server bans. */
EVENT(check_pings)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &lclient_list, lclient_node)
	{
		/* Check TKLs for this user */
		if (loop.do_bancheck && match_tkls(client))
			continue;
		check_ping(client);
		/* don't touch 'client' after this as it may have been killed */
	}

	list_for_each_entry_safe(client, next, &server_list, special_node)
	{
		check_ping(client);
	}

	loop.do_bancheck = loop.do_bancheck_spamf_user = loop.do_bancheck_spamf_away = 0;
	/* done */
}

/** Check for clients that are pending to be terminated */
EVENT(check_deadsockets)
{
	Client *client, *next;

	list_for_each_entry_safe(client, next, &unknown_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when dead socket is set */
		if (IsDeadSocket(client))
		{
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", client->local->fd, client->name);
#endif
			ClearDeadSocket(client); /* CPR. So we send the error. */
			exit_client(client, NULL, client->local->error_str ? client->local->error_str : "Dead socket");
			continue;
		}
	}

	list_for_each_entry_safe(client, next, &lclient_list, lclient_node)
	{
		/* No need to notify opers here. It's already done when dead socket is set */
		if (IsDeadSocket(client))
		{
#ifdef DEBUGMODE
			ircd_log(LOG_ERROR, "Closing deadsock: %d/%s", client->local->fd, client->name);
#endif
			ClearDeadSocket(client); /* CPR. So we send the error. */
			exit_client(client, NULL, client->local->error_str ? client->local->error_str : "Dead socket");
			continue;
		}
	}

	/* Next is for clients that are already exited (unlike the above).
	 * The client is already out of all lists (channels, invites, etc etc)
	 * and 90% has been freed. Here we actually free the remaining parts.
	 * We don't have to send anything anymore.
	 */
	list_for_each_entry_safe(client, next, &dead_list, client_node)
	{
		if (!IsDead(client))
			abort(); /* impossible */
		list_del(&client->client_node);
		free_client(client);
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

	printf("ERROR: Incorrect command line argument encountered.\n"
	       "This is the unrealircd BINARY. End-users should NOT call this binary directly.\n"
	       "Please run the SCRIPT instead: %s/unrealircd\n", SCRIPTDIR);
	printf("Server not started\n\n");
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

extern void applymeblock(void);

extern MODVAR Event *events;

/** This functions resets a couple of timers and does other things that
 * are absolutely cruicial when the clock is adjusted - particularly
 * when the clock goes backwards. -- Syzop
 */
void fix_timers(void)
{
	int i, cnt;
	Client *client;
	Event *e;
	struct ThrottlingBucket *thr;
	ConfigItem_link *lnk;

	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (client->local->since > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: client->local->since %ld -> %ld",
				client->name, client->local->since, TStime()));
			client->local->since = TStime();
		}
		if (client->local->lasttime > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: client->local->lasttime %ld -> %ld",
				client->name, client->local->lasttime, TStime()));
			client->local->lasttime = TStime();
		}
		if (client->local->last > TStime())
		{
			Debug((DEBUG_DEBUG, "fix_timers(): %s: client->local->last %ld -> %ld",
				client->name, client->local->last, TStime()));
			client->local->last = TStime();
		}

		/* users */
		if (MyUser(client))
		{
			if (client->local->nextnick > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: client->local->nextnick %ld -> %ld",
					client->name, client->local->nextnick, TStime()));
				client->local->nextnick = TStime();
			}
			if (client->local->nexttarget > TStime())
			{
				Debug((DEBUG_DEBUG, "fix_timers(): %s: client->local->nexttarget %ld -> %ld",
					client->name, client->local->nexttarget, TStime()));
				client->local->nexttarget = TStime();
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
	if (sodium_init() < 0)
	{
		fprintf(stderr, "Failed to initialize sodium library -- error accessing random device?\n");
		exit(-1);
	}

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
#if 1
		case 'S':
			//charsys_dump_table(p ? p : "*");
			unrealdb_test();
			exit(0);
#endif
#ifndef _WIN32
		  case 't':
			  bootopt |= BOOT_TTY;
			  break;
		  case 'v':
			  (void)printf("%s\n", version);
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
		  case 'c':
			  loop.config_test = 1;
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
		  case 'K':
			  {
			  	char *p = NULL;
			  	if (chdir(TMPDIR) < 0)
			  	{
			  		fprintf(stderr, "Could not change to directory '%s'\n", TMPDIR);
			  		exit(1);
			  	}
			  	fprintf(stderr, "Starting crash test!\n");
			  	*p = 'a';
			  	fprintf(stderr, "It is impossible to get here\n");
			  	exit(0);
			  }
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
#ifndef _WIN32
		  case 'm':
		      modulemanager(argc, argv);
		      exit(0);
#endif
		  case '8':
		      utf8_test();
		      exit(0);
		  case 'L':
		      loop.boot_function = link_generator;
		      break;
		  default:
#ifndef _WIN32
			  return bad_command(myargv[0]);
#else
			  return bad_command(NULL);
#endif
			  break;
		}
	}

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
	fprintf(stderr, "* %s\n", SSLeay_version(SSLEAY_VERSION));
	fprintf(stderr, "* libsodium %s\n", sodium_version_string());
#ifdef USE_LIBCURL
	fprintf(stderr, "* %s\n", curl_version());
#endif
	fprintf(stderr, "* c-ares %s\n", ares_version(NULL));
	fprintf(stderr, "* %s\n", pcre2_version());
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
	if (!loop.config_test)
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
	if (!find_command_simple("PRIVMSG"))
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
	if (loop.config_test)
	{
		ircd_log(LOG_ERROR, "Configuration test passed OK");
		fflush(stderr);
		exit(0);
	}
	if (loop.boot_function)
		loop.boot_function();
#ifndef _WIN32
	fprintf(stderr, "Dynamic configuration initialized.. booting IRCd.\n");
#endif
	open_debugfile();
	me.local->port = 6667; /* pointless? */
	init_sys();
	applymeblock();
#ifdef HAVE_SYSLOG
	openlog("ircd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif
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
	add_to_client_hash_table(me.name, &me);
	add_to_id_hash_table(me.id, &me);
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
	Client *client;
	if (debuglevel >= 0) {
		client = make_client(NULL, NULL);
		client->local->fd = 2;
		SetLog(client);
		client->local->port = debuglevel;
		client->flags = 0;

		strlcpy(client->local->sockhost, me.local->sockhost, sizeof client->local->sockhost);
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
			client->local->fd = fd;
			debugfd = fd;
#else
			/* if (fd != 2) {
				(void)dup2(fd, 2);
				(void)close(fd);
			} -- hands off stderr! */
#endif
			strlcpy(client->name, LOGFILE, sizeof(client->name));
		} else if (isatty(2) && ttyname(2))
			strlcpy(client->name, ttyname(2), sizeof(client->name));
		else
# endif
			strlcpy(client->name, "FD2-Pipe", sizeof(client->name));
		Debug((DEBUG_FATAL,
		    "Debug: File <%s> Level: %d at %s", client->name,
		    client->local->port, myctime(time(NULL))));
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
