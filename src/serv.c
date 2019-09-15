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

/* s_serv.c 2.55 2/7/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

#include "unrealircd.h"
#ifndef _WIN32
/* for uname(), is POSIX so should be OK... */
#include <sys/utsname.h>
#endif
extern void s_die();

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

void read_motd(const char *filename, MOTDFile *motd);
void do_read_motd(const char *filename, MOTDFile *themotd);
#ifdef USE_LIBCURL
void read_motd_asynch_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, MOTDDownload *motd_download);
#endif

extern MOTDLine *Find_file(char *, short);

void reread_motdsandrules();


/*
** cmd_functions execute protocol messages on this server:
**      CMD_FUNC(functionname) causes it to use the header
**            int functionname (Client *cptr,
**  	      	Client *sptr, int parc, char *parv[])
**
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the cmd_function to be
**		executed--some cmd_functions may call others...).
**
**	sptr	is the source of the message, defined by the
**		prefix part of the message if present. If not
**		or prefix not found, then sptr==cptr.
**
**		(!IsServer(cptr)) => (cptr == sptr), because
**		prefixes are taken *only* from servers...
**
**		(IsServer(cptr))
**			(sptr == cptr) => the message didn't
**			have the prefix.
**
**			(sptr != cptr && IsServer(sptr) means
**			the prefix specified servername. (?)
**
**			(sptr != cptr && !IsServer(sptr) means
**			that message originated from a remote
**			user (not local).
**
**		combining
**
**		(!IsServer(sptr)) means that, sptr can safely
**		taken as defining the target structure of the
**		message in this server.
**
**	*Always* true (if 'parse' and others are working correct):
**
**	1)	sptr->direction == cptr  (note: cptr->direction == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->direction) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[1]..parv[parc-1] are all
**			non-NULL pointers.
*/
#ifndef _WIN32
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


void send_version(Client* sptr, int reply)
{
	int i;

	for (i = 0; ISupportStrings[i]; i++)
	{
		sendnumeric(sptr, reply, ISupportStrings[i]);
	}
}

/*
** cmd_version
**	parv[1] = remote server
*/
CMD_FUNC(cmd_version)
{
	/* Only allow remote VERSIONs if registered -- Syzop */
	if (!IsUser(sptr) && !IsServer(cptr))
	{
		send_version(sptr,RPL_ISUPPORT);
		return 0;
	}

	if (hunt_server(cptr, sptr, recv_mtags, ":%s VERSION :%s", 1, parc, parv) == HUNTED_ISME)
	{
		sendnumeric(sptr, RPL_VERSION, version, debugmode, me.name,
			    (ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) ? serveropts : "0"),
			    extraflags ? extraflags : "",
			    tainted ? "3" : "",
			    (ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL) ? MYOSNAME : "*"),
			    UnrealProtocol);
		if (ValidatePermissionsForPath("server:info",sptr,NULL,NULL,NULL))
		{
			sendnotice(sptr, "%s", SSLeay_version(SSLEAY_VERSION));
			sendnotice(sptr, "%s", pcre2_version());
#ifdef USE_LIBCURL
			sendnotice(sptr, "%s", curl_version());
#endif
		}
		if (MyUser(sptr))
			send_version(sptr,RPL_ISUPPORT);
		else
			send_version(sptr,RPL_REMOTEISUPPORT);
	}
	return 0;
}

char *num = NULL;

/*
 * send_proto:
 * Sends PROTOCTL message to server
 * Now split up into multiple PROTOCTL messages (again), since we have
 * too many for a single line. If this breaks your services because
 * you fail to maintain PROTOCTL state, then fix them!
 */
void send_proto(Client *cptr, ConfigItem_link *aconf)
{
	ISupport *prefix = ISupportFind("PREFIX");

	/* CAUTION: If adding a token to an existing PROTOCTL line below,
	 *          then ensure that MAXPARA is not reached!
	 */

	/* First line */
	sendto_one(cptr, NULL, "PROTOCTL NOQUIT NICKv2 SJOIN SJOIN2 UMODE2 VL SJ3 TKLEXT TKLEXT2 NICKIP ESVID %s %s",
	           iConf.ban_setter_sync ? "SJSBY" : "",
	           ClientCapabilityFindReal("message-tags") ? "MTAGS" : "");

	/* Second line */
	sendto_one(cptr, NULL, "PROTOCTL CHANMODES=%s%s,%s%s,%s%s,%s%s USERMODES=%s BOOTED=%lld PREFIX=%s NICKCHARS=%s SID=%s MLOCK TS=%lld EXTSWHOIS",
		CHPAR1, EXPAR1, CHPAR2, EXPAR2, CHPAR3, EXPAR3, CHPAR4, EXPAR4,
		umodestring, (long long)me.local->since, prefix->value,
		charsys_get_current_languages(), me.id, (long long)TStime());
}

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION "-" PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

int remotecmdfilter(Client *sptr, int parc, char *parv[])
{
	/* no remote requests permitted from non-ircops */
	if (MyUser(sptr) && !ValidatePermissionsForPath("server:remote",sptr,NULL,NULL,NULL) && !BadPtr(parv[1]))
	{
		parv[1] = NULL;
		parc = 1;
	}

	/* same as above, but in case an old server forwards a request to us: we ignore it */
	if (!MyUser(sptr) && !ValidatePermissionsForPath("server:remote",sptr,NULL,NULL,NULL))
		return 1; /* STOP (return) */
	
	return 0; /* Continue */
}


/*
 * sends cmd_info into to sptr
*/

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

void cmd_info_send(Client *sptr)
{
char **text = unrealinfo;

	sendnumericfmt(sptr, RPL_INFO, "========== %s ==========", IRCDTOTALVERSION);

	while (*text)
		sendnumericfmt(sptr, RPL_INFO, "| %s", *text++);

	sendnumericfmt(sptr, RPL_INFO, "|");
	sendnumericfmt(sptr, RPL_INFO, "|");
	sendnumericfmt(sptr, RPL_INFO, "| Credits - Type /Credits");
	sendnumericfmt(sptr, RPL_INFO, "| DALnet Credits - Type /DalInfo");
	sendnumericfmt(sptr, RPL_INFO, "|");
	sendnumericfmt(sptr, RPL_INFO, "| This is an UnrealIRCd-style server");
	sendnumericfmt(sptr, RPL_INFO, "| If you find any bugs, please report them at:");
	sendnumericfmt(sptr, RPL_INFO, "|  https://bugs.unrealircd.org/");
	sendnumericfmt(sptr,
	    RPL_INFO, "| UnrealIRCd Homepage: https://www.unrealircd.org");
	sendnumericfmt(sptr,
	    RPL_INFO, "============================================");
	sendnumericfmt(sptr, RPL_INFO, "Birth Date: %s, compile # %s", creation, generation);
	sendnumericfmt(sptr, RPL_INFO, "On-line since %s", myctime(me.local->firsttime));
	sendnumericfmt(sptr, RPL_INFO, "ReleaseID (%s)", buildid);
	sendnumeric(sptr, RPL_ENDOFINFO);
}

/*
** cmd_info
**	parv[1] = servername
**  Modified for hardcode by Stskeeps
*/

CMD_FUNC(cmd_info)
{
	if (remotecmdfilter(sptr, parc, parv))
		return 0;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s INFO :%s", 1, parc, parv) == HUNTED_ISME)
	{
		cmd_info_send(sptr);
	}

	return 0;
}

/*
** cmd_dalinfo
**      parv[1] = servername
*/
CMD_FUNC(cmd_dalinfo)
{
	char **text = dalinfotext;

	if (remotecmdfilter(sptr, parc, parv))
		return 0;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s DALINFO :%s", 1, parc, parv) == HUNTED_ISME)
	{
		while (*text)
			sendnumeric(sptr, RPL_INFO, *text++);

		sendnumeric(sptr, RPL_INFO, "");
		sendnumericfmt(sptr,
		    RPL_INFO, "Birth Date: %s, compile # %s", creation, generation);
		sendnumericfmt(sptr, RPL_INFO, "On-line since %s", myctime(me.local->firsttime));
		sendnumeric(sptr, RPL_ENDOFINFO);
	}

	return 0;
}

/*
** cmd_license
**      parv[1] = servername
*/
CMD_FUNC(cmd_license)
{
	char **text = gnulicense;

	if (remotecmdfilter(sptr, parc, parv))
		return 0;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s LICENSE :%s", 1, parc, parv) == HUNTED_ISME)
	{
		while (*text)
			sendnumeric(sptr, RPL_INFO, *text++);

		sendnumeric(sptr, RPL_INFO, "");
		sendnumeric(sptr, RPL_ENDOFINFO);
	}

	return 0;
}

/*
** cmd_credits
**      parv[1] = servername
*/
CMD_FUNC(cmd_credits)
{
	char **text = unrealcredits;

	if (remotecmdfilter(sptr, parc, parv))
		return 0;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s CREDITS :%s", 1, parc, parv) == HUNTED_ISME)
	{
		while (*text)
			sendnumeric(sptr, RPL_INFO, *text++);

		sendnumeric(sptr, RPL_INFO, "");
		sendnumericfmt(sptr,
		    RPL_INFO, "Birth Date: %s, compile # %s", creation, generation);
		sendnumericfmt(sptr, RPL_INFO, "On-line since %s", myctime(me.local->firsttime));
		sendnumeric(sptr, RPL_ENDOFINFO);
	}

	return 0;
}

char *get_cptr_status(Client *acptr)
{
	static char buf[10];
	char *p = buf;

	*p = '\0';
	*p++ = '[';
	if (IsListening(acptr))
	{
		if (acptr->umodes & LISTENER_NORMAL)
			*p++ = '*';
		if (acptr->umodes & LISTENER_SERVERSONLY)
			*p++ = 'S';
		if (acptr->umodes & LISTENER_CLIENTSONLY)
			*p++ = 'C';
		if (acptr->umodes & LISTENER_TLS)
			*p++ = 's';
	}
	else
	{
		if (IsTLS(acptr))
			*p++ = 's';
	}
	*p++ = ']';
	*p++ = '\0';
	return (buf);
}

/* Used to blank out ports -- Barubary */
char *get_client_name2(Client *acptr, int showports)
{
	char *pointer = get_client_name(acptr, TRUE);

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

/*
** cmd_summon
*/
CMD_FUNC(cmd_summon)
{
	/* /summon is old and out dated, we just return an error as
	 * required by RFC1459 -- codemastr
	 */ sendnumeric(sptr, ERR_SUMMONDISABLED);
	return 0;
}
/*
** cmd_users
**	parv[1] = servername
*/ 
CMD_FUNC(cmd_users)
{
	/* /users is out of date, just return an error as  required by
	 * RFC1459 -- codemastr
	 */ sendnumeric(sptr, ERR_USERSDISABLED);
	return 0;
}

/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions
** --msa
**
**	parv[*] = parameters
*/ 
CMD_FUNC(cmd_error)
{
	char *para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	Debug((DEBUG_ERROR, "Received ERROR message from %s: %s",
	    sptr->name, para));

	/* Errors from untrusted sources only go to the junk snomask
	 * (which is only for debugging issues and such).
	 * This to prevent flooding and confusing IRCOps by
	 * malicious users.
	 */
	if (!IsServer(cptr) && !cptr->serv)
	{
		sendto_snomask(SNO_JUNK, "ERROR from %s -- %s",
			get_client_name(cptr, FALSE), para);
		return 0;
	}

	sendto_umode_global(UMODE_OPER, "ERROR from %s -- %s",
	    get_client_name(cptr, FALSE), para);

	return 0;
}

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

void reread_motdsandrules()
{
	read_motd(conf_files->motd_file, &motd);
	read_motd(conf_files->rules_file, &rules);
	read_motd(conf_files->smotd_file, &smotd);
	read_motd(conf_files->botmotd_file, &botmotd);
	read_motd(conf_files->opermotd_file, &opermotd);
	read_motd(conf_files->svsmotd_file, &svsmotd);
}

extern void reinit_resolver(Client *sptr);

/*
** cmd_rehash
** remote rehash by binary
** now allows the -flags in remote rehash
** ugly code but it seems to work :) -- codemastr
** added -all and fixed up a few lines -- niquil (niquil@programmer.net)
** fixed remote rehashing, but it's getting a bit weird code again -- Syzop
** removed '-all' code, this is now considered as '/rehash', this is ok
** since we rehash everything with simple '/rehash' now. Syzop/20040205
*/
CMD_FUNC(cmd_rehash)
{
	int x = 0;

	if (!ValidatePermissionsForPath("server:rehash",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if ((parc < 3) || BadPtr(parv[2])) {
		/* If the argument starts with a '-' (like -motd, -opermotd, etc) then it's
		 * assumed not to be a server. -- Syzop
		 */
		if (parv[1] && (parv[1][0] == '-'))
			x = HUNTED_ISME;
		else
			x = hunt_server(cptr, sptr, recv_mtags, ":%s REHASH :%s", 1, parc, parv);
	} else {
		if (match_simple("-glob*", parv[1])) /* This is really ugly... hack to make /rehash -global -something work */
		{
			x = HUNTED_ISME;
		} else {
			x = hunt_server(cptr, sptr, NULL, ":%s REHASH %s :%s", 1, parc, parv);
			// XXX: FIXME: labeled-response can't handle this, multiple servers.
		}
	}
	if (x != HUNTED_ISME)
		return 0; /* Now forwarded or server didnt exist */

	if (MyUser(sptr) && IsWebsocket(sptr))
	{
		sendnotice(sptr, "Sorry, for technical reasons it is not possible to REHASH "
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
		return 0;
	}

	if (cptr != sptr)
	{
#ifndef REMOTE_REHASH
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
#endif
		if (parv[2] == NULL)
		{
			if (loop.ircd_rehashing)
			{
				sendnotice(sptr, "A rehash is already in progress");
				return 0;
			}
			sendto_umode_global(UMODE_OPER,
			    ":%s GLOBOPS :%s is remotely rehashing server config file",
			    me.name, sptr->name);
			remote_rehash_client = sptr;
			reread_motdsandrules();
			return rehash(cptr, sptr,
			    (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
		}
		parv[1] = parv[2];
	} else {
		/* Ok this is in an 'else' because it should be only executed for sptr == cptr,
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
			if (!ValidatePermissionsForPath("server:rehash",sptr,NULL,NULL,NULL))
			{
				sendnumeric(sptr, ERR_NOPRIVILEGES);
				sendnotice(sptr, "'/REHASH -global' requires you to have server::rehash permissions");
				return 0;
			}
			if (parv[1] && *parv[1] != '-')
			{
				sendnotice(sptr, "You cannot specify a server name after /REHASH -global, for obvious reasons");
				return 0;
			}
			/* Broadcast it in an inefficient, but backwards compatible way. */
			list_for_each_entry(acptr, &global_server_list, client_node)
			{
				if (acptr == &me)
					continue;
				sendto_one(acptr, NULL, ":%s REHASH %s %s",
					sptr->name,
					acptr->name,
					parv[1] ? parv[1] : "-all");
			}
			/* Don't return, continue, because we need to REHASH ourselves as well. */
		}
	}

	if (!BadPtr(parv[1]) && strcasecmp(parv[1], "-all"))
	{

		if (!ValidatePermissionsForPath("server:rehash",sptr,NULL,NULL,NULL))
		{
			sendnumeric(sptr, ERR_NOPRIVILEGES);
			return 0;
		}

		if (*parv[1] == '-')
		{
			if (!strncasecmp("-gar", parv[1], 4))
			{
				loop.do_garbage_collect = 1;
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (!strncasecmp("-dns", parv[1], 4))
			{
				reinit_resolver(sptr);
				return 0;
			}
			if (match_simple("-ssl*", parv[1]) || match_simple("-tls*", parv[1]))
			{
				reinit_ssl(sptr);
				return 0;
			}
			if (match_simple("-o*motd", parv[1]))
			{
				if (cptr != sptr)
					sendto_umode_global(UMODE_OPER, "Remotely rehashing OPERMOTD on request of %s", sptr->name);
				else
					sendto_ops("Rehashing OPERMOTD on request of %s", sptr->name);
				read_motd(conf_files->opermotd_file, &opermotd);
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (match_simple("-b*motd", parv[1]))
			{
				if (cptr != sptr)
					sendto_umode_global(UMODE_OPER, "Remotely rehashing BOTMOTD on request of %s", sptr->name);
				else
					sendto_ops("Rehashing BOTMOTD on request of %s", sptr->name);
				read_motd(conf_files->botmotd_file, &botmotd);
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (!strncasecmp("-motd", parv[1], 5)
			    || !strncasecmp("-rules", parv[1], 6))
			{
				if (cptr != sptr)
					sendto_umode_global(UMODE_OPER, "Remotely rehasing all MOTDs and RULES on request of %s", sptr->name);
				else
					sendto_ops("Rehashing all MOTDs and RULES on request of %s", sptr->name);
				rehash_motdrules();
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
			return 0;
		}
	}
	else
	{
		if (loop.ircd_rehashing)
		{
			sendnotice(sptr, "A rehash is already in progress");
			return 0;
		}
		sendto_ops("%s is rehashing server config file", sptr->name);
	}

	/* Normal rehash, rehash motds&rules too, just like the on in the tld block will :p */
	if (cptr == sptr)
		sendnumeric(sptr, RPL_REHASHING, configfile);
	x = rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
	reread_motdsandrules();
	return x;
}

/*
** cmd_restart
**
** parv[1] - password *OR* reason if no drpass { } block exists
** parv[2] - reason for restart (optional & only if drpass block exists)
*/
CMD_FUNC(cmd_restart)
{
char *reason = parv[1];
	Client *acptr;

	/* Check permissions */
	if (!ValidatePermissionsForPath("server:restart",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	/* Syntax: /restart */
	if (parc == 1)
	{
		if (conf_drpass)
		{
			sendnumeric(sptr, ERR_NEEDMOREPARAMS, "RESTART");
			return 0;
		}
	} else
	if (parc >= 2)
	{
		/* Syntax: /restart <pass> [reason] */
		if (conf_drpass)
		{
			if (!Auth_Check(cptr, conf_drpass->restartauth, parv[1]))
			{
				sendnumeric(sptr, ERR_PASSWDMISMATCH);
				return 0;
			}
			reason = parv[2];
		}
	}
	sendto_ops("Server is Restarting by request of %s", sptr->name);

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
			sendnotice(acptr, "Server Restarted by %s", sptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr, NULL, ":%s ERROR :Restarted by %s: %s",
			    me.name, get_client_name(sptr, TRUE), reason ? reason : "No reason");
	}

	server_reboot(reason ? reason : "No reason");
	return 0;
}

/*
 * Heavily modified from the ircu cmd_motd by codemastr
 * Also svsmotd support added
 */
int short_motd(Client *sptr)
{
       ConfigItem_tld *tld;
       MOTDFile *themotd;
       MOTDLine *motdline;
       struct tm *tm;
       char is_short;

       tm = NULL;
       is_short = 1;

       tld = Find_tld(sptr);

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
               sendnumeric(sptr, ERR_NOMOTD);
               return 0;
       }
       if (themotd->last_modified.tm_year)
       {
	       tm = &themotd->last_modified; /* for readability */
               sendnumeric(sptr, RPL_MOTDSTART, me.name);
               sendnumericfmt(sptr, RPL_MOTD, "- %d/%d/%d %d:%02d", tm->tm_mday, tm->tm_mon + 1,
                   1900 + tm->tm_year, tm->tm_hour, tm->tm_min);
       }
       if (is_short)
       {
               sendnumeric(sptr, RPL_MOTD, "This is the short MOTD. To view the complete MOTD type /motd");
               sendnumeric(sptr, RPL_MOTD, "");
       }

       motdline = NULL;
       if (themotd)
	       motdline = themotd->lines;
       while (motdline)
       {
               sendnumeric(sptr, RPL_MOTD, motdline->line);
               motdline = motdline->next;
       }
       sendnumeric(sptr, RPL_ENDOFMOTD);
       return 0;
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
		 * read_motd_asynch_downloaded() function will do that
		 * when it sees that ->themod == NULL.
		 */
		themotd->motd_download = NULL;
	}

	/* if filename is NULL, do_read_motd will catch it */
	if(filename && url_is_valid(filename))
	{
		/* prepare our payload for read_motd_asynch_downloaded() */
		motd_download = safe_alloc(sizeof(MOTDDownload));
		motd_download->themotd = themotd;
		themotd->motd_download = motd_download;

		modtime = unreal_getfilemodtime(unreal_mkcache(filename));

		download_file_async(filename, modtime, (vFP)read_motd_asynch_downloaded, motd_download);
		return;
	}
#endif /* USE_LIBCURL */

	do_read_motd(filename, themotd);

	return;
}

#ifdef USE_LIBCURL
/**
   Callback for download_file_async() called from read_motd()
   below.
   @param url the URL curl groked or NULL if the MOTD is stored locally.
   @param filename the path to the local copy of the MOTD or NULL if either cached=1 or there's an error.
   @param errorbuf NULL or an errorstring if there was an error while downloading the MOTD.
   @param cached 0 if the URL was downloaded freshly or 1 if the last download was canceled and the local copy should be used.
 */
void read_motd_asynch_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, MOTDDownload *motd_download)
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


/**
   Does the actual reading of the MOTD. To be called only by
   read_motd() or read_motd_asynch_downloaded().
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

/**
   Frees the contents of a MOTDFile structure.
   The MOTDFile structure itself should be statically
   allocated and deallocated. If the caller wants, it must
   manually free the MOTDFile structure itself.
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


/* cmd_die, this terminates the server, and it intentionally does not
 * have a reason. If you use it you should first do a GLOBOPS and
 * then a server notice to let everyone know what is going down...
 */
CMD_FUNC(cmd_die)
{
	Client *acptr;

	if (!ValidatePermissionsForPath("server:die",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (conf_drpass)	/* See if we have and DIE/RESTART password */
	{
		if (parc < 2)	/* And if so, require a password :) */
		{
			sendnumeric(sptr, ERR_NEEDMOREPARAMS, "DIE");
			return 0;
		}
		if (!Auth_Check(cptr, conf_drpass->dieauth, parv[1]))
		{
			sendnumeric(sptr, ERR_PASSWDMISMATCH);
			return 0;
		}
	}

	/* Let the +s know what is going on */
	sendto_ops("Server Terminating by request of %s", sptr->name);

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
			sendnotice(acptr, "Server Terminated by %s", 
				sptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr, NULL, ":%s ERROR :Terminated by %s",
			    me.name, get_client_name(sptr, TRUE));
	}

	(void)s_die();

	return 0;
}

#ifdef _WIN32
/*
 * Added to let the local console shutdown the server without just
 * calling exit(-1), in Windows mode.  -Cabal95
 */
int  localdie(void)
{
	Client *acptr;

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr))
			sendnotice(acptr, "Server Terminated by local console");
		else if (IsServer(acptr))
			sendto_one(acptr, NULL,
			    ":%s ERROR :Terminated by local console", me.name);
	}
	(void)s_die();
	return 0;
}

#endif

PendingNet *pendingnet = NULL;

void add_pending_net(Client *sptr, char *str)
{
	PendingNet *net;
	PendingServer *srv;
	char *p, *name;

	if (BadPtr(str) || !sptr)
		return;

	/* Allocate */
	net = safe_alloc(sizeof(PendingNet));
	net->sptr = sptr;

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

void free_pending_net(Client *sptr)
{
	PendingNet *net, *net_next;
	PendingServer *srv, *srv_next;
	
	for (net = pendingnet; net; net = net_next)
	{
		net_next = net->next;
		if (net->sptr == sptr)
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

PendingNet *find_pending_net_by_sid_butone(char *sid, Client *exempt)
{
	PendingNet *net;
	PendingServer *srv;

	if (BadPtr(sid))
		return NULL;

	for (net = pendingnet; net; net = net->next)
	{
		if (net->sptr == exempt)
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
		if (net->sptr != cptr)
			continue;
		/* Ok, found myself */
		for (s = net->servers; s; s = s->next)
		{
			char *curr_sid = s->sid;
			other = find_pending_net_by_sid_butone(curr_sid, cptr);
			if (other)
			{
				*srv = net->sptr;
				*sid = s->sid;
				return other->sptr; /* Found another (pending) server with identical numeric */
			}
		}
	}
	
	return NULL;
}

Client *find_non_pending_net_duplicates(Client *cptr)
{
	PendingNet *net;
	PendingServer *s;
	Client *acptr;

	for (net = pendingnet; net; net = net->next)
	{
		if (net->sptr != cptr)
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

void parse_chanmodes_protoctl(Client *sptr, char *str)
{
	char *modes, *p;
	char copy[256];

	strlcpy(copy, str, sizeof(copy));

	modes = strtoken(&p, copy, ",");
	if (modes)
	{
		safe_strdup(sptr->serv->features.chanmodes[0], modes);
		modes = strtoken(&p, NULL, ",");
		if (modes)
		{
			safe_strdup(sptr->serv->features.chanmodes[1], modes);
			modes = strtoken(&p, NULL, ",");
			if (modes)
			{
				safe_strdup(sptr->serv->features.chanmodes[2], modes);
				modes = strtoken(&p, NULL, ",");
				if (modes)
				{
					safe_strdup(sptr->serv->features.chanmodes[3], modes);
				}
			}
		}
	}
}

static char previous_langsinuse[512];
static int previous_langsinuse_ready = 0;

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
		sendto_server(&me, 0, 0, NULL, "PROTOCTL NICKCHARS=%s", langsinuse);
	}

	strlcpy(previous_langsinuse, langsinuse, sizeof(previous_langsinuse));
}

