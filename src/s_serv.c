/*
 *   Unreal Internet Relay Chat Daemon, src/s_serv.c
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

#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)s_serv.c	2.55 2/7/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";
#endif
#define AllocCpy(x,y) x  = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "version.h"
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <time.h>
#include "h.h"
#include "proto.h"
#include <string.h>
#ifdef USE_LIBCURL
#include "url.h"
#include <curl/curl.h>
#endif
#ifndef _WIN32
/* for uname(), is POSIX so should be OK... */
#include <sys/utsname.h>
#endif
extern VOIDSIG s_die();

static char buf[BUFSIZE];

MODVAR int  max_connection_count = 1, max_client_count = 1;
extern ircstats IRCstats;
extern int do_garbage_collect;
/* We need all these for cached MOTDs -- codemastr */
extern char *buildid;
aMotdFile opermotd;
aMotdFile rules;
aMotdFile motd;
aMotdFile svsmotd;
aMotdFile botmotd;
aMotdFile smotd;

void read_motd(const char *filename, aMotdFile *motd);
void do_read_motd(const char *filename, aMotdFile *themotd);
#ifdef USE_LIBCURL
void read_motd_asynch_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, aMotdDownload *motd_download);
#endif

extern aMotdLine *Find_file(char *, short);

#ifdef USE_SSL
extern void reinit_ssl(aClient *);
#endif
void reread_motdsandrules();


/*
** m_functions execute protocol messages on this server:
**      CMD_FUNC(functionname) causes it to use the header
**            int functionname (aClient *cptr,
**  	      	aClient *sptr, int parc, char *parv[])
**
**
**	cptr	is always NON-NULL, pointing to a *LOCAL* client
**		structure (with an open socket connected!). This
**		identifies the physical socket where the message
**		originated (or which caused the m_function to be
**		executed--some m_functions may call others...).
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
**	1)	sptr->from == cptr  (note: cptr->from == cptr)
**
**	2)	MyConnect(sptr) <=> sptr == cptr (e.g. sptr
**		*cannot* be a local connection, unless it's
**		actually cptr!). [MyConnect(x) should probably
**		be defined as (x == x->from) --msa ]
**
**	parc	number of variable parameter strings (if zero,
**		parv is allowed to be NULL)
**
**	parv	a NULL terminated list of parameter pointers,
**
**			parv[0], sender (prefix string), if not present
**				this points to an empty string.
**			parv[1]...parv[parc-1]
**				pointers to additional parameters
**			parv[parc] == NULL, *always*
**
**		note:	it is guaranteed that parv[0]..parv[parc-1] are all
**			non-NULL pointers.
*/
#ifndef NO_FDLIST
extern fdlist serv_fdlist;
#endif

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

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
CMD_FUNC(m_version)
{
	extern char serveropts[];
	extern char *IsupportStrings[];
	int reply, i;

	/* Only allow remote VERSIONs if registered -- Syzop */
	if (!IsPerson(sptr) && !IsServer(cptr))
		goto normal;

	if (hunt_server_token(cptr, sptr, MSG_VERSION, TOK_VERSION, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		sendto_one(sptr, rpl_str(RPL_VERSION), me.name,
		    parv[0], version, debugmode, me.name,
		    serveropts, extraflags ? extraflags : "",
		    tainted ? "3" : "",
		    (IsAnOper(sptr) ? MYOSNAME : "*"), UnrealProtocol);
#ifdef USE_SSL
		if (IsAnOper(sptr))
			sendto_one(sptr, ":%s NOTICE %s :%s", me.name, sptr->name, OPENSSL_VERSION_TEXT);
#endif
#ifdef ZIP_LINKS
		if (IsAnOper(sptr))
			sendto_one(sptr, ":%s NOTICE %s :zlib %s", me.name, sptr->name, zlibVersion());
#endif
#ifdef USE_LIBCURL
		if (IsAnOper(sptr))
			sendto_one(sptr, ":%s NOTICE %s :%s", me.name, sptr->name, curl_version());
#endif
		if (MyClient(sptr))
normal:
			reply = RPL_ISUPPORT;
		else
			reply = RPL_REMOTEISUPPORT;
		/* Send the text */
		for (i = 0; IsupportStrings[i]; i++)
			sendto_one(sptr, rpl_str(reply), me.name, sptr->name, 
				   IsupportStrings[i]); 
	}
	return 0;
}

char *num = NULL;

/*
 * send_proto:
 * Sends PROTOCTL message to server, taking care of whether ZIP
 * should be enabled or not.
 * Now split up into multiple PROTOCTL messages (again), since we have
 * too many for a single line. If this breaks your services because
 * you fail to maintain PROTOCTL state, then fix them!
 */
void send_proto(aClient *cptr, ConfigItem_link *aconf)
{
char buf[1024];

	/* First line */
	sendto_one(cptr, "PROTOCTL %s", PROTOCTL_SERVER);

	/* Second line */
	sprintf(buf, "CHANMODES=%s%s,%s%s,%s%s,%s%s NICKCHARS=%s MLOCK",
		CHPAR1, EXPAR1, CHPAR2, EXPAR2, CHPAR3, EXPAR3, CHPAR4, EXPAR4, langsinuse);
#ifdef ZIP_LINKS
	if (aconf->options & CONNECT_ZIP)
		strcat(buf, " ZIP");
#endif
	sendto_one(cptr, "PROTOCTL %s", buf);
}

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

/*
 * sends m_info into to sptr
*/

char *unrealinfo[] =
{
	"This release was brought to you by the following people:",
	"",
	"Head coder:",
	"* Syzop        <syzop@unrealircd.com>",
	"",
	"Coders:",
	"* binki        <binki@unrealircd.com>",
	"",
	"Contributors:",
	"* nenolod, Adam, warg, Stealth, WolfSage, katsklaw, darkex,"
	"  fspijkerman, fbi, Apocalypse",
	"",
	"RC Testers:",
	"* <<TODO>>,",
	"  and everyone else who downloaded the release candidates.",
	"",
	"Past UnrealIRCd3.2* coders/contributors:",
	"* Stskeeps (ret. head coder / project leader)",
	"* codemastr (ret. u3.2 head coder)",
	"* aquanight, WolfSage, ..",
	"* McSkaf, Zogg, NiQuiL, chasm, llthangel, nighthawk, ..",
	NULL
};

void m_info_send(aClient *sptr)
{
char **text = unrealinfo;

	sendto_one(sptr, ":%s %d %s :=-=-=-= %s =-=-=-=",
	    me.name, RPL_INFO, sptr->name, IRCDTOTALVERSION);

	while (*text)
		sendto_one(sptr, ":%s %d %s :| %s", 
		    me.name, RPL_INFO, sptr->name, *text++);

	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| Credits - Type /Credits",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| DALnet Credits - Type /DalInfo",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| This is an UnrealIRCd-style server",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| If you find any bugs, please report them at:",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|  http://bugs.unrealircd.org/",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr,
	    ":%s %d %s :| UnrealIRCd Homepage: http://www.unrealircd.com",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr,
	    ":%s %d %s :-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=", me.name,
	    RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :Birth Date: %s, compile # %s", me.name,
	    RPL_INFO, sptr->name, creation, generation);
	sendto_one(sptr, ":%s %d %s :On-line since %s", me.name, RPL_INFO,
	    sptr->name, myctime(me.firsttime));
	sendto_one(sptr, ":%s %d %s :ReleaseID (%s)", me.name, RPL_INFO,
	    sptr->name, buildid);
	sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, sptr->name);
}

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
**  Modified for hardcode by Stskeeps
*/

CMD_FUNC(m_info)
{

	if (hunt_server_token(cptr, sptr, MSG_INFO, TOK_INFO, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		m_info_send(sptr);
	}

	return 0;
}

/*
** m_dalinfo
**      parv[0] = sender prefix
**      parv[1] = servername
*/
CMD_FUNC(m_dalinfo)
{
	char **text = dalinfotext;

	if (hunt_server_token(cptr, sptr, MSG_DALINFO, TOK_DALINFO, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		while (*text)
			sendto_one(sptr, rpl_str(RPL_INFO),
			    me.name, parv[0], *text++);

		sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
		sendto_one(sptr,
		    ":%s %d %s :Birth Date: %s, compile # %s",
		    me.name, RPL_INFO, parv[0], creation, generation);
		sendto_one(sptr, ":%s %d %s :On-line since %s",
		    me.name, RPL_INFO, parv[0], myctime(me.firsttime));
		sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
	}

	return 0;
}

/*
** m_license
**      parv[0] = sender prefix
**      parv[1] = servername
*/
CMD_FUNC(m_license)
{
	char **text = gnulicense;

	if (hunt_server_token(cptr, sptr, MSG_LICENSE, TOK_LICENSE, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		while (*text)
			sendto_one(sptr, rpl_str(RPL_INFO),
			    me.name, parv[0], *text++);

		sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
		sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
	}

	return 0;
}

/*
** m_credits
**      parv[0] = sender prefix
**      parv[1] = servername
*/
CMD_FUNC(m_credits)
{
	char **text = unrealcredits;

	if (hunt_server_token(cptr, sptr, MSG_CREDITS, TOK_CREDITS, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		while (*text)
			sendto_one(sptr, rpl_str(RPL_INFO),
			    me.name, parv[0], *text++);

		sendto_one(sptr, rpl_str(RPL_INFO), me.name, parv[0], "");
		sendto_one(sptr,
		    ":%s %d %s :Birth Date: %s, compile # %s",
		    me.name, RPL_INFO, parv[0], creation, generation);
		sendto_one(sptr, ":%s %d %s :On-line since %s",
		    me.name, RPL_INFO, parv[0], myctime(me.firsttime));
		sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, parv[0]);
	}

	return 0;
}

char *get_cptr_status(aClient *acptr)
{
	static char buf[10];
	char *p = buf;

	*p = '\0';
	*p++ = '[';
	if (acptr->flags & FLAGS_LISTEN)
	{
		if (acptr->umodes & LISTENER_NORMAL)
			*p++ = '*';
		if (acptr->umodes & LISTENER_SERVERSONLY)
			*p++ = 'S';
		if (acptr->umodes & LISTENER_CLIENTSONLY)
			*p++ = 'C';
#ifdef USE_SSL
		if (acptr->umodes & LISTENER_SSL)
			*p++ = 's';
#endif
		if (acptr->umodes & LISTENER_REMOTEADMIN)
			*p++ = 'R';
		if (acptr->umodes & LISTENER_JAVACLIENT)
			*p++ = 'J';
	}
	else
	{
#ifdef USE_SSL
		if (acptr->flags & FLAGS_SSL)
			*p++ = 's';
#endif
	}
	*p++ = ']';
	*p++ = '\0';
	return (buf);
}

/* Used to blank out ports -- Barubary */
char *get_client_name2(aClient *acptr, int showports)
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
	strcpy((char *)strrchr((char *)pointer, '.'), ".0]");

	return pointer;
}

/*
** m_summon
** parv[0] = sender prefix
*/
CMD_FUNC(m_summon)
{
	/* /summon is old and out dated, we just return an error as
	 * required by RFC1459 -- codemastr
	 */ sendto_one(sptr, err_str(ERR_SUMMONDISABLED), me.name, parv[0]);
	return 0;
}
/*
** m_users
**	parv[0] = sender prefix
**	parv[1] = servername
*/ 
CMD_FUNC(m_users)
{
	/* /users is out of date, just return an error as  required by
	 * RFC1459 -- codemastr
	 */ sendto_one(sptr, err_str(ERR_USERSDISABLED), me.name, parv[0]);
	return 0;
}
/*
** Note: At least at protocol level ERROR has only one parameter,
** although this is called internally from other functions
** --msa
**
**	parv[0] = sender prefix
**	parv[*] = parameters
*/ 
CMD_FUNC(m_error)
{
	char *para;

	para = (parc > 1 && *parv[1] != '\0') ? parv[1] : "<>";

	Debug((DEBUG_ERROR, "Received ERROR message from %s: %s",
	    sptr->name, para));
	/*
	   ** Ignore error messages generated by normal user clients
	   ** (because ill-behaving user clients would flood opers
	   ** screen otherwise). Pass ERROR's from other sources to
	   ** the local operator...
	 */
	if (IsPerson(cptr) || IsUnknown(cptr))
		return 0;
	if (cptr == sptr)
	{
		sendto_serv_butone(&me, ":%s GLOBOPS :ERROR from %s -- %s",
		    me.name, get_client_name(cptr, FALSE), para);
		sendto_locfailops("ERROR :from %s -- %s",
		    get_client_name(cptr, FALSE), para);
	}
	else
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :ERROR from %s via %s -- %s", me.name,
		    sptr->name, get_client_name(cptr, FALSE), para);
		sendto_ops("ERROR :from %s via %s -- %s", sptr->name,
		    get_client_name(cptr, FALSE), para);
	}
	return 0;
}

Link *helpign = NULL;

/* Now just empty ignore-list, in future reload dynamic help.
 * Move out to help.c -Donwulff */
void reset_help(void)
{
	free_str_list(helpign);
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
	fprintf(tunefile, "%li\n", TSoffset);
	fprintf(tunefile, "%d\n", IRCstats.me_max);
	fclose(tunefile);
}

void load_tunefile(void)
{
	FILE *tunefile;
	char buf[1024];

	tunefile = fopen(conf_files->tune_file, "r");
	if (!tunefile)
		return;
	fprintf(stderr, "* Loading tunefile..\n");
	fgets(buf, 1023, tunefile);
	TSoffset = atol(buf);
	fgets(buf, 1023, tunefile);
	IRCstats.me_max = atol(buf);
	fclose(tunefile);
}

/** Rehash motd and rule files (motd_file/rules_file and all tld entries). */
void rehash_motdrules()
{
ConfigItem_tld *tlds;

	reread_motdsandrules();
	for (tlds = conf_tld; tlds; tlds = (ConfigItem_tld *) tlds->next)
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

extern void reinit_resolver(aClient *sptr);

/*
** m_rehash
** remote rehash by binary
** now allows the -flags in remote rehash
** ugly code but it seems to work :) -- codemastr
** added -all and fixed up a few lines -- niquil (niquil@programmer.net)
** fixed remote rehashing, but it's getting a bit weird code again -- Syzop
** removed '-all' code, this is now considered as '/rehash', this is ok
** since we rehash everything with simple '/rehash' now. Syzop/20040205
*/
CMD_FUNC(m_rehash)
{
	int  x;

	if (MyClient(sptr) && !OPCanRehash(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!MyClient(sptr) && !IsNetAdmin(sptr)
	    && !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	x = 0;

	if ((parc < 3) || BadPtr(parv[2])) {
		/* If the argument starts with a '-' (like -motd, -opermotd, etc) then it's
		 * assumed not to be a server. -- Syzop
		 */
		if (parv[1] && (parv[1][0] == '-'))
			x = HUNTED_ISME;
		else
			x = hunt_server_token(cptr, sptr, MSG_REHASH, TOK_REHASH, "%s", 1, parc, parv);
	} else {
		if (!_match("-glob*", parv[1])) /* This is really ugly... hack to make /rehash -global -something work */
			x = HUNTED_ISME;
		else
			x = hunt_server_token(cptr, sptr, MSG_REHASH, TOK_REHASH, "%s %s", 1, parc, parv);
	}
	if (x != HUNTED_ISME)
		return 0; /* Now forwarded or server didnt exist */

	if (cptr != sptr)
	{
#ifndef REMOTE_REHASH
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
#endif
		if (parv[2] == NULL)
		{
			if (loop.ircd_rehashing)
			{
				sendto_one(sptr, ":%s NOTICE %s :A rehash is already in progress",
					me.name, sptr->name);
				return 0;
			}
			sendto_serv_butone(&me,
			    ":%s GLOBOPS :%s is remotely rehashing server config file",
			    me.name, sptr->name);
			sendto_ops
			    ("%s is remotely rehashing server config file",
			    parv[0]);
			reread_motdsandrules();
			return rehash(cptr, sptr,
			    (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
		}
		parv[1] = parv[2];
	} else {
		/* Ok this is in an 'else' because it should be only executed for sptr == cptr,
		 * but it's totally unrelated to the above ;).
		 */
		if (parv[1] && !_match("-glob*", parv[1]))
		{
			/* /REHASH -global [options] */
			Link *lp;
			aClient *acptr;
			
			/* Shift parv's to the left */
			parv[1] = parv[2];
			parv[2] = NULL;
			parc--;
			/* Only netadmins may use /REHASH -global, which is because:
			 * a) it makes sense
			 * b) remote servers don't support remote rehashes by non-netadmins
			 */
			if (!IsNetAdmin(sptr))
			{
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
				sendnotice(sptr, "'/REHASH -global' requires you to be NetAdmin");
				return 0;
			}
			if (parv[1] && *parv[1] != '-')
			{
				sendnotice(sptr, "You cannot specify a server name after /REHASH -global, for obvious reasons");
				return 0;
			}
			/* Broadcast it in an inefficient, but backwards compatible way. */
			for (lp = Servers; lp; lp = lp->next)
			{
				acptr = lp->value.cptr;
				if (acptr == &me)
					continue;
				sendto_one(acptr, ":%s %s %s",
					sptr->name,
					IsToken(acptr->from) ? TOK_REHASH : MSG_REHASH,
					parv[1] ? parv[1] : "-all");
			}
			/* Don't return, continue, because we need to REHASH ourselves as well. */
		}
	}

	if (!BadPtr(parv[1]) && stricmp(parv[1], "-all"))
	{

		if (!IsAdmin(sptr) && !IsCoAdmin(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return 0;
		}

		if (*parv[1] == '-')
		{
			if (!strnicmp("-gar", parv[1], 4))
			{
				loop.do_garbage_collect = 1;
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (!strnicmp("-dns", parv[1], 4))
			{
				reinit_resolver(sptr);
				return 0;
			}
			if (!_match("-ssl*", parv[1]))
			{
#ifdef USE_SSL
				reinit_ssl(sptr);
#else
				sendnotice(sptr, "SSL is not enabled on this server");
#endif
				return 0;
			}
			if (!_match("-o*motd", parv[1]))
			{
				sendto_ops
				    ("%sRehashing OperMOTD on request of %s",
				    cptr != sptr ? "Remotely " : "",
				    sptr->name);
				if (cptr != sptr)
					sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing OperMOTD", me.name, sptr->name);
				read_motd(conf_files->opermotd_file, &opermotd);
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (!_match("-b*motd", parv[1]))
			{
				sendto_ops
				    ("%sRehashing BotMOTD on request of %s",
				    cptr != sptr ? "Remotely " : "",
				    sptr->name);
				if (cptr != sptr)
					sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing BotMOTD", me.name, sptr->name);
				read_motd(conf_files->botmotd_file, &botmotd);
				RunHook3(HOOKTYPE_REHASHFLAG, cptr, sptr, parv[1]);
				return 0;
			}
			if (!strnicmp("-motd", parv[1], 5)
			    || !strnicmp("-rules", parv[1], 6))
			{
				sendto_ops
				    ("%sRehashing all MOTDs and RULES on request of %s",
				    cptr != sptr ? "Remotely " : "",
				    sptr->name);
				if (cptr != sptr)
					sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing all MOTDs and RULES", me.name, sptr->name);
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
			sendto_one(sptr, ":%s NOTICE %s :A rehash is already in progress",
				me.name, sptr->name);
			return 0;
		}
		sendto_ops("%s is rehashing server config file", parv[0]);
	}

	/* Normal rehash, rehash motds&rules too, just like the on in the tld block will :p */
	if (cptr == sptr)
		sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
	x = rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
	reread_motdsandrules();
	return x;
}

/*
** m_restart
**
** parv[1] - password *OR* reason if no X:line
** parv[2] - reason for restart (optional & only if X:line exists)
**
** The password is only valid if there is a matching X line in the
** config file. If it is not,  then it becomes the
*/
CMD_FUNC(m_restart)
{
char *reason = parv[1];
	aClient *acptr;
	int i;
	/* Check permissions */
	if (MyClient(sptr) && !OPCanRestart(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!MyClient(sptr) && !IsNetAdmin(sptr) && !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

#ifdef CHROOTDIR
	sendnotice(sptr, "/RESTART does not work on chrooted servers");
	return 0;
#endif

	/* Syntax: /restart */
	if (parc == 1)
	{
		if (conf_drpass)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "RESTART");
			return 0;
		}
	} else
	if (parc >= 2)
	{
		/* Syntax: /restart <pass> [reason] */
		if (conf_drpass)
		{
			int ret;
			ret = Auth_Check(cptr, conf_drpass->restartauth, parv[1]);
			if (ret == -1)
			{
				sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
				return 0;
			}
			if (ret < 1)
				return 0;
			reason = parv[2];
		}
	}
	sendto_ops("Server is Restarting by request of %s", parv[0]);
	
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
			    ":%s %s %s :Server Restarting. %s",
			    me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name, sptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Restarted by %s: %s",
			    me.name, get_client_name(sptr, TRUE), reason ? reason : "No reason");
	}

	server_reboot(reason ? reason : "No reason");
	return 0;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
int short_motd(aClient *sptr)
{
       ConfigItem_tld *tld;
       aMotdFile *themotd;
       aMotdLine *motdline;
       struct tm *tm;
       char userhost[HOSTLEN + USERLEN + 6];
       char is_short;

       tm = NULL;
       is_short = 1;

       strlcpy(userhost,make_user_host(sptr->user->username, sptr->user->realhost), sizeof userhost);
       tld = Find_tld(sptr, userhost);

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
               sendto_one(sptr, err_str(ERR_NOMOTD), me.name, sptr->name);
               return 0;
       }
       if (themotd->last_modified.tm_year)
       {
	       tm = &themotd->last_modified; /* for readability */
               sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, sptr->name,
                   me.name);
               sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", me.name,
                   RPL_MOTD, sptr->name, tm->tm_mday, tm->tm_mon + 1,
                   1900 + tm->tm_year, tm->tm_hour, tm->tm_min);
       }
       if (is_short)
       {
               sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
                       "This is the short MOTD. To view the complete MOTD type /motd");
               sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name, "");
       }

       motdline = NULL;
       if (themotd)
	       motdline = themotd->lines;
       while (motdline)
       {
               sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
                   motdline->line);
               motdline = motdline->next;
       }
       sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, sptr->name);
       return 0;
}



/*
 * A merge from ircu and bahamut, and some extra stuff added by codemastr
 * we can now use 1 function for multiple files -- codemastr
 * Merged read_motd/read_rules stuff into this -- Syzop
 */

#define MOTD_LINE_LEN 81


/** Read motd-like file, used for rules/motd/botmotd/opermotd/etc.
 *  Multiplexes to either directly reading the MOTD or downloading it asynchronously.
 * @param filename Filename of file to read or URL. NULL is accepted and causes the *motd to be free()d.
 * @param motd Reference to motd pointer (used for freeing if needed and for asynchronous remote MOTD support)
 */
void read_motd(const char *filename, aMotdFile *themotd)
{
#ifdef USE_LIBCURL
	time_t modtime;
	aMotdDownload *motd_download;
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
		motd_download = MyMallocEx(sizeof(aMotdDownload));
		if(!motd_download)
			outofmemory();
		motd_download->themotd = themotd;
		themotd->motd_download = motd_download;

#ifdef REMOTEINC_SPECIALCACHE
		modtime = unreal_getfilemodtime(unreal_mkcache(filename));
#else
		modtime = 0;
#endif

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
void read_motd_asynch_downloaded(const char *url, const char *filename, const char *errorbuf, int cached, aMotdDownload *motd_download)
{
	aMotdFile *themotd;

	themotd = motd_download->themotd;
	/*
	  check if the download was soft-canceled. See struct.h's docs on
	  struct MotdDownload for details.
	*/
	if(!themotd)
	{
		MyFree(motd_download);
		return;
	}

	/* errors -- check for specialcached version if applicable */
	if(!cached && !filename)
	{
#ifdef REMOTEINC_SPECIALCACHE
		if(has_cached_version(url))
		{
			config_warn("Error downloading MOTD file from \"%s\": %s -- using cached version instead.", url, errorbuf);
			filename = unreal_mkcache(url);
		} else {
#endif
			config_error("Error downloading MOTD file from \"%s\": %s", url, errorbuf);

			/* remove reference to this chunk of memory about to be freed. */
			motd_download->themotd->motd_download = NULL;
			MyFree(motd_download);
			return;
#ifdef REMOTEINC_SPECIALCACHE
		}
#endif
	}

#ifdef REMOTEINC_SPECIALCACHE
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
#endif

	do_read_motd(filename, themotd);
	MyFree(motd_download);
}
#endif /* USE_LIBCURL */


/**
   Does the actual reading of the MOTD. To be called only by
   read_motd() or read_motd_asynch_downloaded().
 */
void do_read_motd(const char *filename, aMotdFile *themotd)
{
	FILE *fd;
	struct tm *tm_tmp;
	time_t modtime;

	char line[512];
	char *tmp;

	aMotdLine *last, *temp;

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
		if (strlen(line) > MOTD_LINE_LEN)
			line[MOTD_LINE_LEN] = '\0';

		temp = MyMalloc(sizeof(aMotdLine));
		if (!temp)
			outofmemory();
		AllocCpy(temp->line, line);

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
   Frees the contents of a aMotdFile structure.
   The aMotdFile structure itself should be statically
   allocated and deallocated. If the caller wants, it must
   manually free the aMotdFile structure itself.
 */
void free_motd(aMotdFile *themotd)
{
	aMotdLine *next, *motdline;

	if(!themotd)
		return;

	for (motdline = themotd->lines; motdline; motdline = next)
	{
		next = motdline->next;
		MyFree(motdline->line);
		MyFree(motdline);
	}

	themotd->lines = NULL;
	memset(&themotd->last_modified, '\0', sizeof(struct tm));

#ifdef USE_LIBCURL
	/* see struct.h for more information about motd_download */
	themotd->motd_download = NULL;
#endif
}


/* m_die, this terminates the server, and it intentionally does not
 * have a reason. If you use it you should first do a GLOBOPS and
 * then a server notice to let everyone know what is going down...
 */
CMD_FUNC(m_die)
{
	aClient *acptr;
	int  i;
	if (!MyClient(sptr) || !OPCanDie(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (conf_drpass)	/* See if we have and DIE/RESTART password */
	{
		if (parc < 2)	/* And if so, require a password :) */
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name,
			    parv[0], "DIE");
			return 0;
		}
		i = Auth_Check(cptr, conf_drpass->dieauth, parv[1]);
		if (i == -1)
		{
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
			    parv[0]);
			return 0;
		}
		if (i < 1)
		{
			return 0;
		}
	}

	/* Let the +s know what is going on */
	sendto_ops("Server Terminating by request of %s", parv[0]);

	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
			    ":%s %s %s :Server Terminating. %s",
			    me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name, sptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr, ":%s ERROR :Terminated by %s",
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
	aClient *acptr;
	int  i;

	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
			    ":%s %s %s :Server Terminated by local console",
			    me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr,
			    ":%s ERROR :Terminated by local console", me.name);
	}
	(void)s_die();
	return 0;
}

#endif

aClient *find_match_server(char *mask)
{
	aClient *acptr;

	if (BadPtr(mask))
		return NULL;

	collapse(mask);

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!match(mask, acptr->name))
			break;
		continue;
	}

	return acptr;
}

aPendingNet *pendingnet = NULL;

void add_pending_net(aClient *sptr, char *str)
{
aPendingNet *e = NULL;
int num = 1;
char *p, *name;

	if (BadPtr(str) || !sptr)
		return;

	/* First, count them */
	for (p = str; *p; p++)
		if (*p == ',')
			num++;
	
	/* Allocate */
	e = MyMallocEx(sizeof(aPendingNet) + (sizeof(int) * num));
	
	e->numservers = num;
	e->sptr = sptr;

	/* Fill in */
	num = 0;
	for (name = strtoken(&p, str, ","); name; name = strtoken(&p, NULL, ","))
	{
		if (!*name)
			continue;

		/* skip any non-digit prefixes, if necessary. Possibly needed in the future. */
		while (*name && !isdigit(*name))
			name++; 

		e->servers[num++] = atoi(name);
	}
	
	AddListItem(e, pendingnet);
}

void free_pending_net(aClient *sptr)
{
aPendingNet *e, *e_next;

	for (e = pendingnet; e; e = e_next)
	{
		e_next = e->next;
		if (e->sptr == sptr)
		{
			DelListItem(e, pendingnet);
			MyFree(e);
			/* Don't break, there can be multiple objects */
		}
	}
}

aPendingNet *find_pending_net_by_numeric_butone(int numeric, aClient *exempt)
{
aPendingNet *e;
int i;

	if (numeric <= 0)
		return NULL;

	for (e = pendingnet; e; e = e->next)
	{
		if (e->sptr == exempt)
			continue;
		for (i = 0; i < e->numservers; i++)
			if (e->servers[i] == numeric)
				return e;
	}
	return NULL;
}

/** Search the pending connections list for any identical numerics */
aClient *find_pending_net_duplicates(aClient *cptr, aClient **srv, int *numeric)
{
aPendingNet *e, *other;
int i;

	*srv = NULL;
	*numeric = 0;
	
	for (e = pendingnet; e; e = e->next)
	{
		if (e->sptr != cptr)
			continue;
		/* Ok, found myself */
		for (i = 0; i < e->numservers; i++)
		{
			int curr_numeric = e->servers[i];
			other = find_pending_net_by_numeric_butone(curr_numeric, cptr);
			if (other)
			{
				*srv = e->sptr;
				*numeric = curr_numeric;
				return other->sptr; /* Found another (pending) server with identical numeric */
			}
		}
	}
	
	return NULL;
}

aClient *find_non_pending_net_duplicates(aClient *cptr)
{
aPendingNet *e;
int i;
aClient *acptr;

	for (e = pendingnet; e; e = e->next)
	{
		if (e->sptr != cptr)
			continue;
		/* Ok, found myself */
		for (i = 0; i < e->numservers; i++)
		{
			int numeric = e->servers[i];
			acptr = find_server_by_numeric(numeric);
			if (acptr)
				return acptr; /* Found another (fully CONNECTED) server with identical numeric */
		}
	}
	
	return NULL;
}

