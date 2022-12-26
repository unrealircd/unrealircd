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

/* Forward declarations */
void server_reboot(const char *);
void restart(const char *);
static void open_debugfile(), setup_signals();

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
		unreal_log(ULOG_INFO, "main", "GARBAGE_COLLECT_STARTED", NULL, "Doing garbage collection...");
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
			unreal_log(ULOG_INFO, "main", "GARBAGE_COLLECT_STARTED", NULL, "Cleaned up $count garbage blocks",
			           log_data_integer("count", (ii - freelinks)));
		}
	}
	if (loop.do_garbage_collect == 1)
		loop.do_garbage_collect = 0;
}

/** Does this user match any TKL's? */
int match_tkls(Client *client)
{
	ConfigItem_ban *bconf = NULL;
	char banbuf[1024];

	/* Process dynamic *LINES */
	if (find_tkline_match(client, 0))
		return 1; /* user killed */

	find_shun(client); /* check for shunned and take action, if so */

	if (IsUser(client))
	{
		/* Check ban realname { } */
		if (!ValidatePermissionsForPath("immune",client,NULL,NULL,NULL) && (bconf = find_ban(NULL, client->info, CONF_BAN_REALNAME)))
		{
			unreal_log(ULOG_INFO, "tkl", "BAN_REALNAME", client,
			           "Banned client $client.details due to realname ban: $reason",
			           log_data_string("reason", bconf->reason ? bconf->reason : "no reason"));

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
			return 1; /* stop processing, client is dead now */
		}
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
		if (client->local->creationtime &&
		    ((TStime() - client->local->creationtime) > iConf.handshake_timeout) &&
		    !(client->local->listener && (client->local->listener->socket_type == SOCKET_TYPE_UNIX)))
		{
			Hook *h;
			int n = HOOK_CONTINUE;
			const char *quitreason = "Registration Timeout";
			char reasonbuf[512];

			if (client->server && *client->server->by)
				continue; /* handled by server module */

			for (h = Hooks[HOOKTYPE_PRE_LOCAL_HANDSHAKE_TIMEOUT]; h; h = h->next)
			{
				n = (*(h->func.intfunc))(client, &quitreason);
				if (n == HOOK_ALLOW)
					break;
			}
			if (n == HOOK_ALLOW)
				continue; /* Do not exit the client due to registration timeout */

			/* Work on a copy here, since the 'quitreason' may point to
			 * some kind of buffer that gets freed in the exit code.
			 */
			strlcpy(reasonbuf, quitreason ? quitreason : "Registration Timeout", sizeof(reasonbuf));
			exit_client(client, NULL, reasonbuf);
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

	/* If ping is less than or equal to the last time we received a command from them */
	if (ping > (TStime() - client->local->last_msg_received))
		return; /* some recent command was executed */

	if (
		/* If we have sent a ping */
		(IsPingSent(client)
		/* And they had 2x ping frequency to respond */
		&& ((TStime() - client->local->last_msg_received) >= (2 * ping)))
		||
		/* Or isn't registered and time spent is larger than ping (CONNECTTIMEOUT).. */
		(!IsRegistered(client) && (TStime() - client->local->fake_lag >= ping))
		)
	{
		if (IsServer(client) || IsConnecting(client) ||
		    IsHandshake(client) || IsTLSConnectHandshake(client))
		{
			unreal_log(ULOG_ERROR, "link", "LINK_DISCONNECTED", client,
			           "Lost server link to $client [$client.ip]: No response (Ping timeout)",
			           client->server->conf ? log_data_link_block(client->server->conf) : NULL);
			SetServerDisconnectLogged(client);
		}
		ircsnprintf(scratch, sizeof(scratch), "Ping timeout: %lld seconds",
			(long long) (TStime() - client->local->last_msg_received));
		exit_client(client, NULL, scratch);
		return;
	}
	else if (IsRegistered(client) && !IsPingSent(client))
	{
		/* Time to send a PING */
		SetPingSent(client);
		ClearPingWarning(client);
		/* not nice but does the job */
		client->local->last_msg_received = TStime() - ping;
		sendto_one(client, NULL, "PING :%s", me.name);
	}
	else if (!IsPingWarning(client) && PINGWARNING > 0 &&
		(IsServer(client) || IsHandshake(client) || IsConnecting(client) ||
		IsTLSConnectHandshake(client)) &&
		(TStime() - client->local->last_msg_received) >= (ping + PINGWARNING))
	{
		SetPingWarning(client);
		unreal_log(ULOG_WARNING, "link", "LINK_UNRELIABLE", client,
			   "Warning, no response from $client for $time_delta seconds",
			   log_data_integer("time_delta", PINGWARNING),
			   client->server->conf ? log_data_link_block(client->server->conf) : NULL);
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
		if (client->local->fake_lag > TStime())
			client->local->fake_lag = TStime();
		if (client->local->last_msg_received > TStime())
			client->local->last_msg_received = TStime();
		if (client->local->idle_since > TStime())
			client->local->idle_since = TStime();

		/* users */
		if (MyUser(client))
		{
			if (client->local->next_nick_allowed > TStime())
				client->local->next_nick_allowed = TStime();
			if (client->local->nexttarget > TStime())
				client->local->nexttarget = TStime();
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

	/* Make sure autoconnect for servers still works (lnk->hold) */
	for (lnk = conf_link; lnk; lnk = lnk->next)
	{
		int t = lnk->class ? lnk->class->connfreq : 90;

		if (lnk->hold > TStime() + t)
		{
			lnk->hold = TStime() + (t / 2); /* compromise */
		}
	}
}


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

	if (mytdiff(timeofday, oldtimeofday) < NEGATIVE_SHIFT_WARN)
	{
		/* tdiff = # of seconds of time set backwards (positive number! eg: 60) */
		time_t tdiff = oldtimeofday - timeofday;
		unreal_log(ULOG_WARNING, "system", "SYSTEM_CLOCK_JUMP_BACKWARDS", NULL,
		           "System clock jumped back in time ~$time_delta seconds ($time_from -> $time_to)\n"
		           "Incorrect time for IRC servers is a serious problem. "
		           "Time being set backwards (system clock changed) is "
		           "even more serious and can cause clients to freeze, channels to be "
		           "taken over, and other issues.\n"
		           "Please be sure your clock is always synchronized before the IRCd is started!",
		           log_data_integer("time_delta", tdiff),
		           log_data_timestamp("time_from", oldtimeofday),
		           log_data_timestamp("time_to", timeofday));
		fix_timers();
	} else
	if (mytdiff(timeofday, oldtimeofday) > POSITIVE_SHIFT_WARN) /* do not set too low or you get false positives */
	{
		/* tdiff = # of seconds of time set forward (eg: 60) */
		time_t tdiff = timeofday - oldtimeofday;
		unreal_log(ULOG_WARNING, "system", "SYSTEM_CLOCK_JUMP_FORWARDS", NULL,
		           "System clock jumped ~$time_delta seconds forward ($time_from -> $time_to)\n"
		           "Incorrect time for IRC servers is a serious problem. "
		           "Time being adjusted (by changing the system clock) "
		           "more than a few seconds forward/backward can lead to serious issues.\n"
		           "Please be sure your clock is always synchronized before the IRCd is started!",
		           log_data_integer("time_delta", tdiff),
		           log_data_timestamp("time_from", oldtimeofday),
		           log_data_timestamp("time_to", timeofday));
		fix_timers();
	}

	if (highesttimeofday+NEGATIVE_SHIFT_WARN > timeofday)
	{
		if (lasthighwarn > timeofday)
			lasthighwarn = timeofday;
		if (timeofday - lasthighwarn > 300)
		{
			unreal_log(ULOG_WARNING, "system", "SYSTEM_CLOCK_JUMP_BACKWARDS_PREVIOUSLY", NULL,
				   "The system clock previously went backwards. Waiting for time to be OK again. This will be in $time_delta seconds.",
				   log_data_integer("time_delta", highesttimeofday - timeofday),
				   log_data_timestamp("time_from", highesttimeofday),
				   log_data_timestamp("time_to", timeofday));
			lasthighwarn = timeofday;
		}
	} else {
		highesttimeofday = timeofday;
	}

	oldtimeofday = timeofday;
}

void SetupEvents(void)
{
	/* Start events */
	EventAdd(NULL, "tunefile", save_tunefile, NULL, 300*1000, 0);
	EventAdd(NULL, "garbage", garbage_collect, NULL, GARBAGE_COLLECT_EVERY*1000, 0);
	EventAdd(NULL, "loop", loop_event, NULL, 1000, 0);
	EventAdd(NULL, "unrealdns_removeoldrecords", unrealdns_removeoldrecords, NULL, 15000, 0);
	EventAdd(NULL, "check_pings", check_pings, NULL, 1000, 0);
	EventAdd(NULL, "check_deadsockets", check_deadsockets, NULL, 1000, 0);
	EventAdd(NULL, "handshake_timeout", handshake_timeout, NULL, 1000, 0);
	EventAdd(NULL, "tls_check_expiry", tls_check_expiry, NULL, (86400/2)*1000, 0);
	EventAdd(NULL, "unrealdb_expire_secret_cache", unrealdb_expire_secret_cache, NULL, 61000, 0);
	EventAdd(NULL, "throttling_check_expire", throttling_check_expire, NULL, 1000, 0);
}

/** The main function. This will call SocketLoop() once the server is ready. */
#ifndef _WIN32
int main(int argc, char *argv[])
#else
int InitUnrealIRCd(int argc, char *argv[])
#endif
{
#ifndef _WIN32
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
	init_winsock();
#endif
	setup_signals();

	memset(&irccounts, '\0', sizeof(irccounts));
	irccounts.servers = 1;

	mp_pool_init();
	dbuf_init();
	initlists();
	initlist_channels();

	early_init_tls();
	url_init();
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
#if 0
		case 'S':
			charsys_dump_table(p ? p : "*");
			//unrealdb_test();
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
	fprintf(stderr, "UnrealIRCd is brought to you by Bram Matthys (Syzop),\n"
	                "Krzysztof Beresztant (k4be), Gottem and i\n\n");

	fprintf(stderr, "Using the following libraries:\n");
	fprintf(stderr, "* %s\n", SSLeay_version(SSLEAY_VERSION));
	fprintf(stderr, "* libsodium %s\n", sodium_version_string());
#ifdef USE_LIBCURL
	fprintf(stderr, "* %s\n", curl_version());
#endif
	fprintf(stderr, "* c-ares %s\n", ares_version(NULL));
	fprintf(stderr, "* %s\n", pcre2_version());
#endif
#if JANSSON_VERSION_HEX >= 0x020D00
	fprintf(stderr, "* jansson %s\n", jansson_version_str());
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
#if !defined(_WIN32) && !defined(_AMIGA) && !defined(OSXTIGER) && DEFAULT_PERMISSIONS != 0
	/* Hack to stop people from being able to read the config file */
	(void)chmod(CPATH, DEFAULT_PERMISSIONS);
#endif
	init_dynconf();
	init_sys();
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
	if (config_read_start() < 0)
		exit(-1);
	while (!is_config_read_finished())
	{
		gettimeofday(&timeofday_tv, NULL);
		timeofday = timeofday_tv.tv_sec;
		url_socket_timeout(NULL);
		unrealdns_timeout(NULL);
		fd_select(500);
	}
	if (config_test() < 0)
		exit(-1);
	load_tunefile();
	make_umodestr();
	SetListening(&me);
	me.local->fd = -1;
	SetMe(&me);
	make_server(&me);
	umodes_check_for_changes();
	charsys_check_for_changes();
	clicap_init();
	if (!find_command_simple("PRIVMSG"))
	{
		config_error("Someone forgot to load modules with proper commands in them. READ THE DOCUMENTATION");
		exit(-4);
	}

	if (!init_tls())
	{
		config_error("Failed to load TLS (see errors above). UnrealIRCd can not start.");
#ifdef _WIN32
		win_error(); /* display error dialog box */
#endif
		exit(9);
	}
	unreal_log(ULOG_INFO, "config", "CONFIG_PASSED", NULL, "Configuration test passed OK");
	if (loop.config_test)
	{
		fflush(stderr);
		exit(0);
	}
	if (loop.boot_function)
		loop.boot_function();
	open_debugfile();
	me.local->port = 6667; /* pointless? */
	applymeblock();
#ifdef HAVE_SYSLOG
	openlog("ircd", LOG_PID | LOG_NDELAY, LOG_DAEMON);
#endif
	config_run();
	unreal_log(ULOG_INFO, "main", "UNREALIRCD_START", NULL, "UnrealIRCd started.");

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
	timeofday = time(NULL);
	me.local->last_msg_received = me.local->fake_lag = me.local->creationtime = me.server->boottime = TStime();
	me.server->features.protocol = UnrealProtocol;
	safe_strdup(me.server->features.software, version);
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
		loop.forked = 1;
	}
#endif
#ifdef _WIN32
	loop.forked = 1;
#endif

	fix_timers();
	write_pidfile();
	loop.booted = 1;
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
	loop.config_status = CONFIG_STATUS_COMPLETE;

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
			request_rehash(NULL);
			dorehash = 0;
		}
		if (dorestart)
		{
			server_reboot("SIGINT");
		}
		if (doreloadcert)
		{
			unreal_log(ULOG_INFO, "config", "CONFIG_RELOAD_TLS", NULL, "Reloading all TLS related data (./unrealircd reloadtls)");
			reinit_tls();
			doreloadcert = 0;
		}
		/* If rehashing, check if we are done. */
		if (loop.rehashing && is_config_read_finished())
			rehash_internal(loop.rehash_save_client);
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
