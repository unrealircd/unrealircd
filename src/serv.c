/*
 *   Unreal Internet Relay Chat Daemon, src/serv.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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

/** @file
 * @brief Server-related functions
 */

/* s_serv.c 2.55 2/7/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"
#include <ares.h>
#ifndef _WIN32
/* for uname(), is POSIX so should be OK... */
#include <sys/utsname.h>
#endif

MODVAR int  max_connection_count = 1, max_client_count = 1;
extern int do_garbage_collect;
/* We need all these for cached MOTDs -- codemastr */
extern char *buildid;
MOTDFile opermotd;
MOTDFile rules;
MOTDFile motd;
MOTDFile svsmotd;
MOTDFile botmotd;
MOTDFile smotd;

/** Hash list of TKL entries */
MODVAR TKL *tklines[TKLISTLEN];
/** 2D hash list of TKL entries + IP address */
MODVAR TKL *tklines_ip_hash[TKLIPHASHLEN1][TKLIPHASHLEN2];
int MODVAR spamf_ugly_vchanoverride = 0;

void read_motd(const char *filename, MOTDFile *motd);
void do_read_motd(const char *filename, MOTDFile *themotd);
#ifdef USE_LIBCURL
void read_motd_async_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, MOTDDownload *motd_download);
#endif

extern MOTDLine *find_file(char *, short);

void reread_motdsandrules();

/** Send a message upstream if necessary and check if it's for us.
 * @param client	The sender
 * @param mtags		Message tags associated with this message
 * @param command	The command (eg: "NOTICE")
 * @param server	This indicates parv[server] contains the destination
 * @param parc		Parameter count (MAX 8!!)
 * @param parv		Parameter values (MAX 8!!)
 * @note Command can have only max 8 parameters (parv[8])
 * @note parv[server] is replaced with the name of the matched client.
 */
int hunt_server(Client *client, MessageTag *mtags, char *command, int server, int parc, char *parv[])
{
	Client *acptr;
	char *saved;

	/* This would be strange and bad. Previous version assumed "it's for me". Hmm.. okay. */
	if (parc <= server || BadPtr(parv[server]))
		return HUNTED_ISME;

	acptr = find_client(parv[server], NULL);

	/* find_client() may find a variety of clients. Only servers/persons please, no 'unknowns'. */
	if (acptr && MyConnect(acptr) && !IsMe(acptr) && !IsUser(acptr) && !IsServer(acptr))
		acptr = NULL;

	if (!acptr)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}

	if (IsMe(acptr) || MyUser(acptr))
		return HUNTED_ISME;

	/* Never send the message back from where it came from */
	if (acptr->direction == client->direction)
	{
		sendnumeric(client, ERR_NOSUCHSERVER, parv[server]);
		return HUNTED_NOSUCH;
	}

	/* Replace "server" part with actual servername (eg: 'User' -> 'x.y.net')
	 * Ugly. Previous version didn't even restore the state, now we do.
	 */
	saved = parv[server];
	parv[server] = acptr->id;

	sendto_one(acptr, mtags, command, client->id,
	    parv[1], parv[2], parv[3], parv[4],
	    parv[5], parv[6], parv[7], parv[8]);

	parv[server] = saved;

	return HUNTED_PASS;
}

#ifndef _WIN32
/** Grab operating system name on Windows (outdated) */
char *getosname(void)
{
	static char buf[1024];
	struct utsname osinf;
	char *p;

	memset(&osinf, 0, sizeof(osinf));
	if (uname(&osinf) != 0)
		return "<unknown>";
	snprintf(buf, sizeof(buf), "%s %s %s %s %s",
		osinf.sysname,
		osinf.nodename,
		osinf.release,
		osinf.version,
		osinf.machine);
	/* get rid of cr/lf */
	for (p=buf; *p; p++)
		if ((*p == '\n') || (*p == '\r'))
		{
			*p = '\0';
			break;
		}
	return buf;
}
#endif

/** Helper function to send version strings */
void send_version(Client *client, int reply)
{
	int i;

	for (i = 0; ISupportStrings[i]; i++)
		sendnumeric(client, reply, ISupportStrings[i]);
}

/** VERSION command:
 * Syntax: VERSION [server]
 */
CMD_FUNC(cmd_version)
{
	/* Only allow remote VERSIONs if registered -- Syzop */
	if (!IsUser(client) && !IsServer(client))
	{
		send_version(client, RPL_ISUPPORT);
		return;
	}

	if (hunt_server(client, recv_mtags, ":%s VERSION :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendnumeric(client, RPL_VERSION, version, debugmode, me.name,
			    (ValidatePermissionsForPath("server:info",client,NULL,NULL,NULL) ? serveropts : "0"),
			    extraflags ? extraflags : "",
			    tainted ? "3" : "",
			    (ValidatePermissionsForPath("server:info",client,NULL,NULL,NULL) ? MYOSNAME : "*"),
			    UnrealProtocol);
		if (ValidatePermissionsForPath("server:info",client,NULL,NULL,NULL))
		{
			sendnotice(client, "%s", SSLeay_version(SSLEAY_VERSION));
			sendnotice(client, "libsodium %s", sodium_version_string());
#ifdef USE_LIBCURL
			sendnotice(client, "%s", curl_version());
#endif
			sendnotice(client, "c-ares %s", ares_version(NULL));
			sendnotice(client, "%s", pcre2_version());
		}
		if (MyUser(client))
			send_version(client,RPL_ISUPPORT);
		else
			send_version(client,RPL_REMOTEISUPPORT);
	}
}

char *num = NULL;

/** Send all our PROTOCTL messages to remote server.
 * We send multiple PROTOCTL's since 4.x. If this breaks your services
 * because you fail to maintain PROTOCTL state, then fix them!
 */
void send_proto(Client *client, ConfigItem_link *aconf)
{
	ISupport *prefix = ISupportFind("PREFIX");

	/* CAUTION: If adding a token to an existing PROTOCTL line below,
	 *          then ensure that MAXPARA is not reached!
	 */

	/* First line */
	sendto_one(client, NULL, "PROTOCTL NOQUIT NICKv2 SJOIN SJOIN2 UMODE2 VL SJ3 TKLEXT TKLEXT2 NICKIP ESVID %s %s",
	           iConf.ban_setter_sync ? "SJSBY" : "",
	           ClientCapabilityFindReal("message-tags") ? "MTAGS" : "");

	/* Second line */
	sendto_one(client, NULL, "PROTOCTL CHANMODES=%s%s,%s%s,%s%s,%s%s USERMODES=%s BOOTED=%lld PREFIX=%s SID=%s MLOCK TS=%lld EXTSWHOIS",
		CHPAR1, EXPAR1, CHPAR2, EXPAR2, CHPAR3, EXPAR3, CHPAR4, EXPAR4,
		umodestring, (long long)me.local->since, prefix->value,
		me.id, (long long)TStime());

	/* Third line */
	sendto_one(client, NULL, "PROTOCTL NICKCHARS=%s CHANNELCHARS=%s",
		charsys_get_current_languages(),
		allowed_channelchars_valtostr(iConf.allowed_channelchars));
}

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

/** Special filter for remote commands */
int remotecmdfilter(Client *client, int parc, char *parv[])
{
	/* no remote requests permitted from non-ircops */
	if (MyUser(client) && !ValidatePermissionsForPath("server:remote",client,NULL,NULL,NULL) && !BadPtr(parv[1]))
	{
		parv[1] = NULL;
		parc = 1;
	}

	/* same as above, but in case an old server forwards a request to us: we ignore it */
	if (!MyUser(client) && !ValidatePermissionsForPath("server:remote",client,NULL,NULL,NULL))
		return 1; /* STOP (return) */
	
	return 0; /* Continue */
}

/** Output for /INFO */
char *unrealinfo[] =
{
	"This release was brought to you by the following people:",
	"",
	"Head coder:",
	"* Bram Matthys (Syzop) <syzop@unrealircd.org>",
	"",
	"Coders:",
	"* Gottem <gottem@unrealircd.org>",
	"* i <i@unrealircd.org>",
	"",
	"Past UnrealIRCd 4.x coders/contributors:",
	"* Heero, binki, nenolod, ..",
	"",
	"Past UnrealIRCd 3.2.x coders/contributors:",
	"* Stskeeps (ret. head coder / project leader)",
	"* codemastr (ret. u3.2 head coder)",
	"* aquanight, WolfSage, ..",
	"* McSkaf, Zogg, NiQuiL, chasm, llthangel, nighthawk, ..",
	NULL
};

/** Send /INFO output */
void cmd_info_send(Client *client)
{
	char **text = unrealinfo;

	sendnumericfmt(client, RPL_INFO, ":========== %s ==========", IRCDTOTALVERSION);

	while (*text)
		sendnumericfmt(client, RPL_INFO, ":| %s", *text++);

	sendnumericfmt(client, RPL_INFO, ":|");
	sendnumericfmt(client, RPL_INFO, ":|");
	sendnumericfmt(client, RPL_INFO, ":| Credits - Type /CREDITS");
	sendnumericfmt(client, RPL_INFO, ":|");
	sendnumericfmt(client, RPL_INFO, ":| This is an UnrealIRCd-style server");
	sendnumericfmt(client, RPL_INFO, ":| If you find any bugs, please report them at:");
	sendnumericfmt(client, RPL_INFO, ":|  https://bugs.unrealircd.org/");
	sendnumericfmt(client, RPL_INFO, ":| UnrealIRCd Homepage: https://www.unrealircd.org");
	sendnumericfmt(client, RPL_INFO, ":============================================");
	sendnumericfmt(client, RPL_INFO, ":Birth Date: %s, compile # %s", creation, generation);
	sendnumericfmt(client, RPL_INFO, ":On-line since %s", myctime(me.local->firsttime));
	sendnumericfmt(client, RPL_INFO, ":ReleaseID (%s)", buildid);
	sendnumeric(client, RPL_ENDOFINFO);
}

/** The INFO command.
 * Syntax: INFO [server]
 */
CMD_FUNC(cmd_info)
{
	if (remotecmdfilter(client, parc, parv))
		return;

	if (hunt_server(client, recv_mtags, ":%s INFO :%s", 1, parc, parv) == HUNTED_ISME)
		cmd_info_send(client);
}

/** LICENSE command
 * Syntax: LICENSE [server]
 */
CMD_FUNC(cmd_license)
{
	char **text = gnulicense;

	if (remotecmdfilter(client, parc, parv))
		return;

	if (hunt_server(client, recv_mtags, ":%s LICENSE :%s", 1, parc, parv) == HUNTED_ISME)
	{
		while (*text)
			sendnumeric(client, RPL_INFO, *text++);

		sendnumeric(client, RPL_INFO, "");
		sendnumeric(client, RPL_ENDOFINFO);
	}
}

/** CREDITS command
 * Syntax: CREDITS [servername]
 */
CMD_FUNC(cmd_credits)
{
	char **text = unrealcredits;

	if (remotecmdfilter(client, parc, parv))
		return;

	if (hunt_server(client, recv_mtags, ":%s CREDITS :%s", 1, parc, parv) == HUNTED_ISME)
	{
		while (*text)
			sendnumeric(client, RPL_INFO, *text++);

		sendnumeric(client, RPL_INFO, "");
		sendnumericfmt(client, RPL_INFO, ":Birth Date: %s, compile # %s", creation, generation);
		sendnumericfmt(client, RPL_INFO, ":On-line since %s", myctime(me.local->firsttime));
		sendnumeric(client, RPL_ENDOFINFO);
	}
}

/** Return flags for a client (connection), eg 's' for SSL/TLS - used in STATS L/l */
char *get_client_status(Client *client)
{
	static char buf[10];
	char *p = buf;

	*p = '\0';
	*p++ = '[';
	if (IsListening(client))
	{
		if (client->umodes & LISTENER_NORMAL)
			*p++ = '*';
		if (client->umodes & LISTENER_SERVERSONLY)
			*p++ = 'S';
		if (client->umodes & LISTENER_CLIENTSONLY)
			*p++ = 'C';
		if (client->umodes & LISTENER_TLS)
			*p++ = 's';
	}
	else
	{
		if (IsTLS(client))
			*p++ = 's';
	}
	*p++ = ']';
	*p++ = '\0';
	return (buf);
}

/** Used to blank out ports -- Barubary - only used in STATS l/L */
char *get_client_name2(Client *client, int showports)
{
	char *pointer = get_client_name(client, TRUE);

	if (!pointer)
		return NULL;
	if (showports)
		return pointer;
	if (!strrchr(pointer, '.'))
		return NULL;
	/*
	 * This may seem like wack but remind this is only used 
	 * in rows of get_client_name2's, so it's perfectly fair
	 * 
	*/
	strcpy(strrchr(pointer, '.'), ".0]");

	return pointer;
}

/** ERROR command - used by servers to indicate errors.
 * Syntax: ERROR :<reason>
 */
CMD_FUNC(cmd_error)
{
	char *para;

	if (!MyConnect(client))
		return;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	/* Errors from untrusted sources only go to the junk snomask
	 * (which is only for debugging issues and such).
	 * This to prevent flooding and confusing IRCOps by
	 * malicious users.
	 */
	if (!IsServer(client) && !client->serv)
	{
		sendto_snomask(SNO_JUNK, "ERROR from server %s: %s",
			get_client_name(client, FALSE), para);
		return;
	}

	sendto_umode_global(UMODE_OPER, "ERROR from server %s: %s",
	                    get_client_name(client, FALSE), para);
	ircd_log(LOG_ERROR, "ERROR from server %s: %s",
	                    get_client_name(client, FALSE), para);
}

/** Save the tunefile (such as: highest seen connection count) */
EVENT(save_tunefile)
{
	FILE *tunefile;

	tunefile = fopen(conf_files->tune_file, "w");
	if (!tunefile)
	{
#if !defined(_WIN32) && !defined(_AMIGA)
		sendto_ops("Unable to write tunefile.. %s", strerror(errno));
#else
		sendto_ops("Unable to write tunefile..");
#endif
		return;
	}
	fprintf(tunefile, "0\n");
	fprintf(tunefile, "%d\n", irccounts.me_max);
	fclose(tunefile);
}

/** Load the tunefile (such as: highest seen connection count) */
void load_tunefile(void)
{
	FILE *tunefile;
	char buf[1024];

	tunefile = fopen(conf_files->tune_file, "r");
	if (!tunefile)
		return;
	fprintf(stderr, "Loading tunefile..\n");
	if (!fgets(buf, sizeof(buf), tunefile))
	    fprintf(stderr, "Warning: error while reading the timestamp offset from the tunefile%s%s\n",
		errno? ": ": "", errno? strerror(errno): "");

	if (!fgets(buf, sizeof(buf), tunefile))
	    fprintf(stderr, "Warning: error while reading the peak user count from the tunefile%s%s\n",
		errno? ": ": "", errno? strerror(errno): "");
	irccounts.me_max = atol(buf);
	fclose(tunefile);
}

/** Rehash motd and rule files (motd_file/rules_file and all tld entries). */
void rehash_motdrules()
{
ConfigItem_tld *tlds;

	reread_motdsandrules();
	for (tlds = conf_tld; tlds; tlds = tlds->next)
	{
		/* read_motd() accepts NULL in first arg and acts sanely */
		read_motd(tlds->motd_file, &tlds->motd);
		read_motd(tlds->rules_file, &tlds->rules);
		read_motd(tlds->smotd_file, &tlds->smotd);
		read_motd(tlds->opermotd_file, &tlds->opermotd);
		read_motd(tlds->botmotd_file, &tlds->botmotd);
	}
}

/** Rehash motd and rules (only the default files) */
void reread_motdsandrules()
{
	read_motd(conf_files->motd_file, &motd);
	read_motd(conf_files->rules_file, &rules);
	read_motd(conf_files->smotd_file, &smotd);
	read_motd(conf_files->botmotd_file, &botmotd);
	read_motd(conf_files->opermotd_file, &opermotd);
	read_motd(conf_files->svsmotd_file, &svsmotd);
}

extern void reinit_resolver(Client *client);

/** REHASH command - reload configuration file on server(s).
 * Syntax: see HELPOP REHASH
 */
CMD_FUNC(cmd_rehash)
{
	int x = 0;

	/* This is one of the (few) commands that cannot be handled
	 * by labeled-response accurately in all circumstances.
	 */
	labeled_response_inhibit = 1;

	if (!ValidatePermissionsForPath("server:rehash",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 3) || BadPtr(parv[2])) {
		/* If the argument starts with a '-' (like -motd, -opermotd, etc) then it's
		 * assumed not to be a server. -- Syzop
		 */
		if (parv[1] && (parv[1][0] == '-'))
			x = HUNTED_ISME;
		else
			x = hunt_server(client, recv_mtags, ":%s REHASH :%s", 1, parc, parv);
	} else {
		if (match_simple("-glob*", parv[1])) /* This is really ugly... hack to make /rehash -global -something work */
		{
			x = HUNTED_ISME;
		} else {
			x = hunt_server(client, NULL, ":%s REHASH %s :%s", 1, parc, parv);
		}
	}
	if (x != HUNTED_ISME)
		return; /* Now forwarded or server didnt exist */

	if (MyUser(client) && IsWebsocket(client))
	{
		sendnotice(client, "Sorry, for technical reasons it is not possible to REHASH "
		                 "the local server from a WebSocket connection.");
		/* Issue details:
		 * websocket_handle_packet -> process_packet -> parse_client_queued ->
		 * dopacket -> parse -> cmd_rehash... and then 'websocket' is unloaded so
		 * we "cannot get back" as that websocket_handle_packet function is gone.
		 *
		 * Solution would be either to delay the rehash or to make websocket perm.
		 * The latter removes all our ability to upgrade the module on the fly
		 * and the former is rather ugly.. not going to do that hassle now anyway.
		 */
		return;
	}

	if (!MyConnect(client))
	{
#ifndef REMOTE_REHASH
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
#endif
		if (parv[2] == NULL)
		{
			if (loop.ircd_rehashing)
			{
				sendnotice(client, "A rehash is already in progress");
				return;
			}
			sendto_umode_global(UMODE_OPER, "%s is remotely rehashing server %s config file", client->name, me.name);
			remote_rehash_client = client;
			reread_motdsandrules();
			// TODO: clean this next line up, wtf man.
			rehash(client, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
			return;
		}
		parv[1] = parv[2];
	} else {
		/* Ok this is in an 'else' because it should be only executed for local clients,
		 * but it's totally unrelated to the above ;).
		 */
		if (parv[1] && match_simple("-glob*", parv[1]))
		{
			/* /REHASH -global [options] */
			Client *acptr;
			
			/* Shift parv's to the left */
			parv[1] = parv[2];
			parv[2] = NULL;
			parc--;
			/* Only netadmins may use /REHASH -global, which is because:
			 * a) it makes sense
			 * b) remote servers don't support remote rehashes by non-netadmins
			 */
			if (!ValidatePermissionsForPath("server:rehash",client,NULL,NULL,NULL))
			{
				sendnumeric(client, ERR_NOPRIVILEGES);
				sendnotice(client, "'/REHASH -global' requires you to have server::rehash permissions");
				return;
			}
			if (parv[1] && *parv[1] != '-')
			{
				sendnotice(client, "You cannot specify a server name after /REHASH -global, for obvious reasons");
				return;
			}
			/* Broadcast it in an inefficient, but backwards compatible way. */
			list_for_each_entry(acptr, &global_server_list, client_node)
			{
				if (acptr == &me)
					continue;
				sendto_one(acptr, NULL, ":%s REHASH %s %s",
					client->name,
					acptr->name,
					parv[1] ? parv[1] : "-all");
			}
			/* Don't return, continue, because we need to REHASH ourselves as well. */
		}
	}

	if (!BadPtr(parv[1]) && strcasecmp(parv[1], "-all"))
	{

		if (!ValidatePermissionsForPath("server:rehash",client,NULL,NULL,NULL))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			return;
		}

		if (*parv[1] == '-')
		{
			if (!strncasecmp("-gar", parv[1], 4))
			{
				loop.do_garbage_collect = 1;
				RunHook2(HOOKTYPE_REHASHFLAG, client, parv[1]);
				return;
			}
			if (!strncasecmp("-dns", parv[1], 4))
			{
				reinit_resolver(client);
				return;
			}
			if (match_simple("-ssl*", parv[1]) || match_simple("-tls*", parv[1]))
			{
				if (IsUser(client))
				{
					sendto_realops_and_log("%s (%s@%s) requested a reload of all SSL related data (/rehash -tls)",
					                       client->name, client->user->username, client->user->realhost);
				} else {
					sendto_realops_and_log("%s requested a reload of all SSL related data (/rehash -tls)",
					                       client->name);
				}
				reinit_tls();
				return;
			}
			if (match_simple("-o*motd", parv[1]))
			{
				if (MyUser(client))
					sendto_ops("Rehashing OPERMOTD on request of %s", client->name);
				else
					sendto_umode_global(UMODE_OPER, "Remotely rehashing OPERMOTD on request of %s", client->name);
				read_motd(conf_files->opermotd_file, &opermotd);
				RunHook2(HOOKTYPE_REHASHFLAG, client, parv[1]);
				return;
			}
			if (match_simple("-b*motd", parv[1]))
			{
				if (MyUser(client))
					sendto_ops("Rehashing BOTMOTD on request of %s", client->name);
				else
					sendto_umode_global(UMODE_OPER, "Remotely rehashing BOTMOTD on request of %s", client->name);
				read_motd(conf_files->botmotd_file, &botmotd);
				RunHook2(HOOKTYPE_REHASHFLAG, client, parv[1]);
				return;
			}
			if (!strncasecmp("-motd", parv[1], 5) || !strncasecmp("-rules", parv[1], 6))
			{
				if (MyUser(client))
					sendto_ops("Rehashing all MOTDs and RULES on request of %s", client->name);
				else
					sendto_umode_global(UMODE_OPER, "Remotely rehasing all MOTDs and RULES on request of %s", client->name);
				rehash_motdrules();
				RunHook2(HOOKTYPE_REHASHFLAG, client, parv[1]);
				return;
			}
			RunHook2(HOOKTYPE_REHASHFLAG, client, parv[1]);
			return;
		}
	}
	else
	{
		if (loop.ircd_rehashing)
		{
			sendnotice(client, "A rehash is already in progress");
			return;
		}
		sendto_ops("%s is rehashing server config file", client->name);
	}

	/* Normal rehash, rehash motds&rules too, just like the on in the tld block will :p */
	sendnumeric(client, RPL_REHASHING, configfile);
	// TODO: fix next line - occurence #2
	x = rehash(client, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
	reread_motdsandrules();
}

/** RESTART command - restart the server (discouraged command)
 * parv[1] - password *OR* reason if no drpass { } block exists
 * parv[2] - reason for restart (optional & only if drpass block exists)
 */
CMD_FUNC(cmd_restart)
{
	char *reason = parv[1];
	Client *acptr;

	if (!MyUser(client))
		return;

	/* Check permissions */
	if (!ValidatePermissionsForPath("server:restart",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	/* Syntax: /restart */
	if (parc == 1)
	{
		if (conf_drpass)
		{
			sendnumeric(client, ERR_NEEDMOREPARAMS, "RESTART");
			return;
		}
	} else
	if (parc >= 2)
	{
		/* Syntax: /restart <pass> [reason] */
		if (conf_drpass)
		{
			if (!Auth_Check(client, conf_drpass->restartauth, parv[1]))
			{
				sendnumeric(client, ERR_PASSWDMISMATCH);
				return;
			}
			reason = parv[2];
		}
	}
	sendto_ops("Server is Restarting by request of %s", client->name);

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
			sendnotice(acptr, "Server Restarted by %s", client->name);
		else if (IsServer(acptr))
			sendto_one(acptr, NULL, ":%s ERROR :Restarted by %s: %s",
			    me.name, get_client_name(client, TRUE), reason ? reason : "No reason");
	}

	server_reboot(reason ? reason : "No reason");
}

/** Send short message of the day to the client */
void short_motd(Client *client)
{
       ConfigItem_tld *tld;
       MOTDFile *themotd;
       MOTDLine *motdline;
       struct tm *tm;
       char is_short;

       tm = NULL;
       is_short = 1;

       tld = find_tld(client);

       /*
	* Try different sources of short MOTDs, falling back to the
	* long MOTD.
       */
       themotd = &smotd;
       if (tld && tld->smotd.lines)
	       themotd = &tld->smotd;

       /* try long MOTDs */
       if (!themotd->lines)
       {
	       is_short = 0;
	       if (tld && tld->motd.lines)
		       themotd = &tld->motd;
	       else
		       themotd = &motd;
       }

       if (!themotd->lines)
       {
               sendnumeric(client, ERR_NOMOTD);
               return;
       }
       if (themotd->last_modified.tm_year)
       {
	       tm = &themotd->last_modified; /* for readability */
               sendnumeric(client, RPL_MOTDSTART, me.name);
               sendnumericfmt(client, RPL_MOTD, ":- %d/%d/%d %d:%02d", tm->tm_mday, tm->tm_mon + 1,
                   1900 + tm->tm_year, tm->tm_hour, tm->tm_min);
       }
       if (is_short)
       {
               sendnumeric(client, RPL_MOTD, "This is the short MOTD. To view the complete MOTD type /motd");
               sendnumeric(client, RPL_MOTD, "");
       }

       motdline = NULL;
       if (themotd)
	       motdline = themotd->lines;
       while (motdline)
       {
               sendnumeric(client, RPL_MOTD, motdline->line);
               motdline = motdline->next;
       }
       sendnumeric(client, RPL_ENDOFMOTD);
}

/*
 * A merge from ircu and bahamut, and some extra stuff added by codemastr
 * we can now use 1 function for multiple files -- codemastr
 * Merged read_motd/read_rules stuff into this -- Syzop
 */

/** Read motd-like file, used for rules/motd/botmotd/opermotd/etc.
 *  Multiplexes to either directly reading the MOTD or downloading it asynchronously.
 * @param filename Filename of file to read or URL. NULL is accepted and causes the *motd to be free()d.
 * @param motd Reference to motd pointer (used for freeing if needed and for asynchronous remote MOTD support)
 */
void read_motd(const char *filename, MOTDFile *themotd)
{
#ifdef USE_LIBCURL
	time_t modtime;
	MOTDDownload *motd_download;
#endif

	/* TODO: if themotd points to a tld's motd,
	   could a rehash disrupt this pointer?*/
#ifdef USE_LIBCURL
	if(themotd->motd_download)
	{
		themotd->motd_download->themotd = NULL;
		/*
		 * It is not our job to free() motd_download, the
		 * read_motd_async_downloaded() function will do that
		 * when it sees that ->themod == NULL.
		 */
		themotd->motd_download = NULL;
	}

	/* if filename is NULL, do_read_motd will catch it */
	if(filename && url_is_valid(filename))
	{
		/* prepare our payload for read_motd_async_downloaded() */
		motd_download = safe_alloc(sizeof(MOTDDownload));
		motd_download->themotd = themotd;
		themotd->motd_download = motd_download;

		modtime = unreal_getfilemodtime(unreal_mkcache(filename));

		download_file_async(filename, modtime, (vFP)read_motd_async_downloaded, motd_download);
		return;
	}
#endif /* USE_LIBCURL */

	do_read_motd(filename, themotd);

	return;
}

#ifdef USE_LIBCURL
/** Callback for download_file_async() called from read_motd() below.
 * @param url the URL curl groked or NULL if the MOTD is stored locally.
 * @param filename the path to the local copy of the MOTD or NULL if either cached=1 or there's an error.
 * @param errorbuf NULL or an errorstring if there was an error while downloading the MOTD.
 * @param cached 0 if the URL was downloaded freshly or 1 if the last download was canceled and the local copy should be used.
 */
void read_motd_async_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, MOTDDownload *motd_download)
{
	MOTDFile *themotd;

	themotd = motd_download->themotd;
	/*
	  check if the download was soft-canceled. See struct.h's docs on
	  struct MOTDDownload for details.
	*/
	if(!themotd)
	{
		safe_free(motd_download);
		return;
	}

	/* errors -- check for specialcached version if applicable */
	if(!cached && !filename)
	{
		if(has_cached_version(url))
		{
			config_warn("Error downloading MOTD file from \"%s\": %s -- using cached version instead.", displayurl(url), errorbuf);
			filename = unreal_mkcache(url);
		} else {
			config_error("Error downloading MOTD file from \"%s\": %s", displayurl(url), errorbuf);

			/* remove reference to this chunk of memory about to be freed. */
			motd_download->themotd->motd_download = NULL;
			safe_free(motd_download);
			return;
		}
	}

	/*
	 * We need to move our newly downloaded file to its cache file
	 * if it isn't there already.
	 */
	if(!cached)
	{
		/* create specialcached version for later */
		unreal_copyfileex(filename, unreal_mkcache(url), 1);
	} else {
		/*
		 * The file is cached. Thus we must look for it at the
		 * cache location where we placed it earlier.
		 */
		filename = unreal_mkcache(url);
	}

	do_read_motd(filename, themotd);
	safe_free(motd_download);
}
#endif /* USE_LIBCURL */


/** The actual reading of the MOTD - used by read_motd() and read_motd_async_downloaded()
 */
void do_read_motd(const char *filename, MOTDFile *themotd)
{
	FILE *fd;
	struct tm *tm_tmp;
	time_t modtime;

	char line[512];
	char *tmp;

	MOTDLine *last, *temp;

	free_motd(themotd);

	if(!filename)
		return;

	fd = fopen(filename, "r");
	if (!fd)
		return;

	/* record file modification time */
	modtime = unreal_getfilemodtime(filename);
	tm_tmp = localtime(&modtime);
	memcpy(&themotd->last_modified, tm_tmp, sizeof(struct tm));

	last = NULL;
	while (fgets(line, sizeof(line), fd))
	{
		if ((tmp = strchr(line, '\n')))
			*tmp = '\0';
		if ((tmp = strchr(line, '\r')))
			*tmp = '\0';
		
		if (strlen(line) > 510)
			line[510] = '\0';

		temp = safe_alloc(sizeof(MOTDLine));
		safe_strdup(temp->line, line);

		if(last)
			last->next = temp;
		else
			/* handle the special case of the first line */
			themotd->lines = temp;

		last = temp;
	}
	/* the file could be zero bytes long? */
	if(last)
		last->next = NULL;

	fclose(fd);
	
	return;
}

/** Free the contents of a MOTDFile structure.
 * The MOTDFile structure itself should be statically
 * allocated and deallocated. If the caller wants, it must
 * manually free the MOTDFile structure itself.
 */
void free_motd(MOTDFile *themotd)
{
	MOTDLine *next, *motdline;

	if(!themotd)
		return;

	for (motdline = themotd->lines; motdline; motdline = next)
	{
		next = motdline->next;
		safe_free(motdline->line);
		safe_free(motdline);
	}

	themotd->lines = NULL;
	memset(&themotd->last_modified, '\0', sizeof(struct tm));

#ifdef USE_LIBCURL
	/* see struct.h for more information about motd_download */
	themotd->motd_download = NULL;
#endif
}

/** DIE command - terminate the server
 * DIE [password]
 */
CMD_FUNC(cmd_die)
{
	Client *acptr;

	if (!MyUser(client))
		return;

	if (!ValidatePermissionsForPath("server:die",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (conf_drpass)	/* See if we have and DIE/RESTART password */
	{
		if (parc < 2)	/* And if so, require a password :) */
		{
			sendnumeric(client, ERR_NEEDMOREPARAMS, "DIE");
			return;
		}
		if (!Auth_Check(client, conf_drpass->dieauth, parv[1]))
		{
			sendnumeric(client, ERR_PASSWDMISMATCH);
			return;
		}
	}

	/* Let the +s know what is going on */
	sendto_ops("Server Terminating by request of %s", client->name);

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
			sendnotice(acptr, "Server Terminated by %s", 
				client->name);
		else if (IsServer(acptr))
			sendto_one(acptr, NULL, ":%s ERROR :Terminated by %s",
			    me.name, get_client_name(client, TRUE));
	}

	s_die();
}

/** Server list (network) of pending connections */
PendingNet *pendingnet = NULL;

/** Add server list (network) from 'client' connection */
void add_pending_net(Client *client, char *str)
{
	PendingNet *net;
	PendingServer *srv;
	char *p, *name;

	if (BadPtr(str) || !client)
		return;

	/* Allocate */
	net = safe_alloc(sizeof(PendingNet));
	net->client = client;

	/* Fill in */
	if (*str == '*')
		str++;
	for (name = strtoken(&p, str, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (!*name)
			continue;
		
		srv = safe_alloc(sizeof(PendingServer));
		strlcpy(srv->sid, name, sizeof(srv->sid));
		AddListItem(srv, net->servers);
	}
	
	AddListItem(net, pendingnet);
}

/** Free server list (network) previously added by 'client' */
void free_pending_net(Client *client)
{
	PendingNet *net, *net_next;
	PendingServer *srv, *srv_next;
	
	for (net = pendingnet; net; net = net_next)
	{
		net_next = net->next;
		if (net->client == client)
		{
			for (srv = net->servers; srv; srv = srv_next)
			{
				srv_next = srv->next;
				safe_free(srv);
			}
			DelListItem(net, pendingnet);
			safe_free(net);
			/* Don't break, there can be multiple objects */
		}
	}
}

/** Find SID in any server list (network) that is pending, except 'exempt' */
PendingNet *find_pending_net_by_sid_butone(char *sid, Client *exempt)
{
	PendingNet *net;
	PendingServer *srv;

	if (BadPtr(sid))
		return NULL;

	for (net = pendingnet; net; net = net->next)
	{
		if (net->client == exempt)
			continue;
		for (srv = net->servers; srv; srv = srv->next)
			if (!strcmp(srv->sid, sid))
				return net;
	}
	return NULL;
}

/** Search the pending connections list for any identical sids */
Client *find_pending_net_duplicates(Client *cptr, Client **srv, char **sid)
{
	PendingNet *net, *other;
	PendingServer *s;

	*srv = NULL;
	*sid = NULL;
	
	for (net = pendingnet; net; net = net->next)
	{
		if (net->client != cptr)
			continue;
		/* Ok, found myself */
		for (s = net->servers; s; s = s->next)
		{
			char *curr_sid = s->sid;
			other = find_pending_net_by_sid_butone(curr_sid, cptr);
			if (other)
			{
				*srv = net->client;
				*sid = s->sid;
				return other->client; /* Found another (pending) server with identical numeric */
			}
		}
	}
	
	return NULL;
}

/** Like find_pending_net_duplicates() but the other way around? Eh.. */
Client *find_non_pending_net_duplicates(Client *client)
{
	PendingNet *net;
	PendingServer *s;
	Client *acptr;

	for (net = pendingnet; net; net = net->next)
	{
		if (net->client != client)
			continue;
		/* Ok, found myself */
		for (s = net->servers; s; s = s->next)
		{
			acptr = find_server(s->sid, NULL);
			if (acptr)
				return acptr; /* Found another (fully CONNECTED) server with identical numeric */
		}
	}
	
	return NULL;
}

/** Parse CHANMODES= in PROTOCTL */
void parse_chanmodes_protoctl(Client *client, char *str)
{
	char *modes, *p;
	char copy[256];

	strlcpy(copy, str, sizeof(copy));

	modes = strtoken(&p, copy, ",");
	if (modes)
	{
		safe_strdup(client->serv->features.chanmodes[0], modes);
		modes = strtoken(&p, NULL, ",");
		if (modes)
		{
			safe_strdup(client->serv->features.chanmodes[1], modes);
			modes = strtoken(&p, NULL, ",");
			if (modes)
			{
				safe_strdup(client->serv->features.chanmodes[2], modes);
				modes = strtoken(&p, NULL, ",");
				if (modes)
				{
					safe_strdup(client->serv->features.chanmodes[3], modes);
				}
			}
		}
	}
}

static char previous_langsinuse[512];
static int previous_langsinuse_ready = 0;

/** Check the nick character system (set::allowed-nickchars) for changes.
 * If there are changes, then we broadcast the new PROTOCTL NICKCHARS= to all servers.
 */
void charsys_check_for_changes(void)
{
	char *langsinuse = charsys_get_current_languages();
	/* already called by charsys_finish() */
	safe_strdup(me.serv->features.nickchars, langsinuse);

	if (!previous_langsinuse_ready)
	{
		previous_langsinuse_ready = 1;
		strlcpy(previous_langsinuse, langsinuse, sizeof(previous_langsinuse));
		return; /* not booted yet. then we are done here. */
	}

	if (strcmp(langsinuse, previous_langsinuse))
	{
		ircd_log(LOG_ERROR, "Permitted nick characters changed at runtime: %s -> %s",
			previous_langsinuse, langsinuse);
		sendto_realops("Permitted nick characters changed at runtime: %s -> %s",
			previous_langsinuse, langsinuse);
		/* Broadcast change to all (locally connected) servers */
		sendto_server(NULL, 0, 0, NULL, "PROTOCTL NICKCHARS=%s", langsinuse);
	}

	strlcpy(previous_langsinuse, langsinuse, sizeof(previous_langsinuse));
}

/** Check if supplied server name is valid, that is: does not contain forbidden characters etc */
int valid_server_name(char *name)
{
	char *p;

	if (strlen(name) >= HOSTLEN)
		return 0;

	for (p = name; *p; p++)
		if ((*p <= ' ') || (*p > '~'))
			return 0;

	if (!strchr(name, '.'))
		return 0;

	return 1;
}

/** Check if the supplied name is a valid SID, as in: syntax. */
int valid_sid(char *name)
{
	if (strlen(name) != 3)
		return 0;
	if (!isdigit(*name))
		return 0;
	if (!isdigit(name[1]) && !isupper(name[1]))
		return 0;
	if (!isdigit(name[2]) && !isupper(name[2]))
		return 0;
	return 1;
}

/** Check if the supplied name is a valid UID, as in: syntax. */
int valid_uid(char *name)
{
	char *p;

	/* Enforce at least some minimum length */
	if (strlen(name) < 6)
		return 0;

	/* UID cannot be larger than IDLEN or it would be cut off later */
	if (strlen(name) > IDLEN)
		return 0;

	/* Must start with a digit */
	if (!isdigit(*name))
		return 0;

	/* For all the remaining characters: digit or uppercase character */
	for (p = name+1; *p; p++)
		if (!isdigit(*p) && !isupper(*p))
			return 0;

	return 1;
}

/** Initialize the TKL subsystem */
void tkl_init(void)
{
	memset(tklines, 0, sizeof(tklines));
	memset(tklines_ip_hash, 0, sizeof(tklines_ip_hash));
}

/** Called when a server link is lost.
 * Used for logging only, API users can use the HOOKTYPE_SERVER_QUIT hook.
 */
void lost_server_link(Client *serv, FORMAT_STRING(const char *fmt), ...)
{
	va_list vl;
	static char buf[1024], buf2[512];

	va_start(vl, fmt);
	vsnprintf(buf2, sizeof(buf2), fmt, vl);
	va_end(vl);

	if (IsServer(serv))
	{
		/* An already established link is now lost. Broadcast this to all opers. */
		snprintf(buf, sizeof(buf), "Lost server link to %s: %s",
			get_client_name(serv, FALSE), buf2);
		sendto_umode_global(UMODE_OPER, "%s", buf);
	} else {
		/* A link attempt failed. Only send this to local opers (can be noisy every xx seconds). */
		snprintf(buf, sizeof(buf), "Unable to link with server %s: %s",
			get_client_name(serv, FALSE), buf2);
		sendto_umode(UMODE_OPER, "%s", buf);
	}

	/* Always log! */
	ircd_log(LOG_ERROR, "%s", buf);
}
