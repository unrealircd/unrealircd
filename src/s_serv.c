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
extern VOIDSIG s_die();

static char buf[BUFSIZE];

int  max_connection_count = 1, max_client_count = 1;
extern ircstats IRCstats;
extern int do_garbage_collect;
/* We need all these for cached MOTDs -- codemastr */
extern char *buildid;
aMotd *opermotd;
aMotd *rules;
aMotd *motd;
aMotd *svsmotd;
aMotd *botmotd;
aMotd *smotd;
struct tm motd_tm;
struct tm smotd_tm;
aMotd *read_file(char *filename, aMotd **list);
aMotd *read_file_ex(char *filename, aMotd **list, struct tm *);
extern aMotd *Find_file(char *, short);
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

/*
** m_version
**	parv[0] = sender prefix
**	parv[1] = remote server
*/
CMD_FUNC(m_version)
{
	extern char serveropts[];

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
		if (MyClient(sptr)) {
normal:
			sendto_one(sptr, ":%s 005 %s " PROTOCTL_CLIENT_1, me.name, sptr->name, PROTOCTL_PARAMETERS_1);
			sendto_one(sptr, ":%s 005 %s " PROTOCTL_CLIENT_2, me.name, sptr->name, PROTOCTL_PARAMETERS_2);
		}
		else {
			sendto_one(sptr, ":%s 105 %s " PROTOCTL_CLIENT_1, me.name, sptr->name, PROTOCTL_PARAMETERS_1);
			sendto_one(sptr, ":%s 105 %s " PROTOCTL_CLIENT_2, me.name, sptr->name, PROTOCTL_PARAMETERS_2);
		}
	}
	return 0;
}

/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[parc-1] = comment
*/
CMD_FUNC(m_squit)
{
	char *server;
	aClient *acptr;
	char *comment = (parc > 2 && parv[parc - 1]) ?
	    parv[parc - 1] : cptr->name;


	if (!IsPrivileged(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc > 1)
	{
		if (!(*parv[1] == '@'))
		{
			server = parv[1];

			/*
			   ** The following allows wild cards in SQUIT. Only usefull
			   ** when the command is issued by an oper.
			 */
			for (acptr = client;
			    (acptr = next_client(acptr, server));
			    acptr = acptr->next)
				if (IsServer(acptr) || IsMe(acptr))
					break;
			if (acptr && IsMe(acptr))
			{
				acptr = cptr;
				server = cptr->sockhost;
			}
		}
		else
		{
			server = parv[1];
			acptr = (aClient *)find_server_by_base64(server + 1);
			if (acptr && IsMe(acptr))
			{
				acptr = cptr;
				server = cptr->sockhost;
			}
		}
	}
	else
	{
		/*
		   ** This is actually protocol error. But, well, closing
		   ** the link is very proper answer to that...
		 */
		server = cptr->sockhost;
		acptr = cptr;
	}

	/*
	   ** SQUIT semantics is tricky, be careful...
	   **
	   ** The old (irc2.2PL1 and earlier) code just cleans away the
	   ** server client from the links (because it is never true
	   ** "cptr == acptr".
	   **
	   ** This logic here works the same way until "SQUIT host" hits
	   ** the server having the target "host" as local link. Then it
	   ** will do a real cleanup spewing SQUIT's and QUIT's to all
	   ** directions, also to the link from which the orinal SQUIT
	   ** came, generating one unnecessary "SQUIT host" back to that
	   ** link.
	   **
	   ** One may think that this could be implemented like
	   ** "hunt_server" (e.g. just pass on "SQUIT" without doing
	   ** nothing until the server having the link as local is
	   ** reached). Unfortunately this wouldn't work in the real life,
	   ** because either target may be unreachable or may not comply
	   ** with the request. In either case it would leave target in
	   ** links--no command to clear it away. So, it's better just
	   ** clean out while going forward, just to be sure.
	   **
	   ** ...of course, even better cleanout would be to QUIT/SQUIT
	   ** dependant users/servers already on the way out, but
	   ** currently there is not enough information about remote
	   ** clients to do this...   --msa
	 */
	if (!acptr)
	{
		sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
		    me.name, parv[0], server);
		return 0;
	}
	if (MyClient(sptr) && ((!OPCanGRoute(sptr) && !MyConnect(acptr)) ||
	    (!OPCanLRoute(sptr) && MyConnect(acptr))))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	/*
	   **  Notify all opers, if my local link is remotely squitted
	 */
	if (MyConnect(acptr) && !IsAnOper(cptr))
	{

		sendto_locfailops("Received SQUIT %s from %s (%s)",
		    acptr->name, get_client_name(sptr, FALSE), comment);
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Received SQUIT %s from %s (%s)", me.name,
		    server, get_client_name(sptr, FALSE), comment);
	}
	else if (MyConnect(acptr))
	{
		if (acptr->user)
		{
			sendto_one(sptr,
			    ":%s %s %s :*** Cannot do fake kill by SQUIT !!!",
			    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name);
			sendto_ops
			    ("%s tried to do a fake kill using SQUIT (%s (%s))",
			    sptr->name, acptr->name, comment);
			sendto_serv_butone(&me,
			    ":%s GLOBOPS :%s tried to fake kill using SQUIT (%s (%s))",
			    me.name, sptr->name, acptr->name, comment);
			return 0;
		}
		sendto_locfailops("Received SQUIT %s from %s (%s)",
		    acptr->name, get_client_name(sptr, FALSE), comment);
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Received SQUIT %s from %s (%s)", me.name,
		    acptr->name, get_client_name(sptr, FALSE), comment);
	}
	if (IsAnOper(sptr))
	{
		/*
		 * It was manually /squit'ed by a human being(we hope),
		 * there is a very good chance they don't want us to
		 * reconnect right away.  -Cabal95
		 */
		acptr->flags |= FLAGS_SQUIT;
	}

	return exit_client(cptr, acptr, sptr, comment);
}

/*
 * m_protoctl
 *	parv[0] = Sender prefix
 *	parv[1+] = Options
 */
CMD_FUNC(m_protoctl)
{
	int  i;
#ifndef PROTOCTL_MADNESS
	int  remove = 0;
#endif
	char proto[128], *s;
/*	static char *dummyblank = "";	Yes, it is kind of ugly */

#ifdef PROTOCTL_MADNESS
	if (GotProtoctl(sptr))
	{
		/*
		 * But we already GOT a protoctl msg!
		 */
		if (!IsServer(sptr))
			sendto_one(cptr,
			    "ERROR :Already got a PROTOCTL from you.");
		return 0;
	}
#endif
	cptr->flags |= FLAGS_PROTOCTL;
	/* parv[parc - 1] */
	for (i = 1; i < parc; i++)
	{
		strncpyzt(proto, parv[i], sizeof proto);
		s = proto;
#ifndef PROTOCTL_MADNESS
		if (*s == '-')
		{
			s++;
			remove = 1;
		}
		else
			remove = 0;
#endif
/*		equal = (char *)index(proto, '=');
		if (equal == NULL)
			options = dummyblank;
		else
		{
			options = &equal[1];
			equal[0] = '\0';
		}
*/
		if (strcmp(s, "NOQUIT") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearNoQuit(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetNoQuit(cptr);

		}
		else if (strcmp(s, "TOKEN") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearToken(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetToken(cptr);
		}
		else if (strcmp(s, "HCN") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearHybNotice(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetHybNotice(cptr);
		}
		else if (strcmp(s, "SJOIN") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJOIN(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJOIN(cptr);
		}
		else if (strcmp(s, "SJOIN2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJOIN2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJOIN2(cptr);
		}
		else if (strcmp(s, "NICKv2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearNICKv2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR, "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetNICKv2(cptr);
		}
		else if (strcmp(s, "UMODE2") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearUMODE2(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetUMODE2(cptr);
		}
		else if (strcmp(s, "NS") == 0)
		{
#ifdef PROTOCTL_MADNESS
			if (remove)
			{
				ClearNS(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetNS(cptr);
		}
		else if (strcmp(s, "VL") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearVL(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetVL(cptr);
		}
		else if (strcmp(s, "VHP") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearVHP(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetVHP(cptr);
		}
		else if (strcmp(s, "SJ3") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				ClearSJ3(cptr);
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			SetSJ3(cptr);
		}
		else if (strcmp(s, "SJB64") == 0)
		{
#ifndef PROTOCTL_MADNESS
			if (remove)
			{
				cptr->proto &= ~PROTO_SJB64;
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			cptr->proto |= PROTO_SJB64;
		}
		else if (strcmp(s, "ZIP") == 0)
		{
			if (remove)
			{
				cptr->proto &= ~PROTO_ZIP;
				continue;
			}
			Debug((DEBUG_ERROR,
				"Chose protocol %s for link %s",
				proto, cptr->name));
			cptr->proto |= PROTO_ZIP;
		}
		/*
		 * Add other protocol extensions here, with proto
		 * containing the base option, and options containing
		 * what it equals, if anything.
		 *
		 * DO NOT error or warn on unknown proto; we just don't
		 * support it.
		 */
	}

	return 0;
}

char *num = NULL;
int m_server_synch(aClient *cptr, long numeric, ConfigItem_link *conf);

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**      parv[2] = hopcount
**      parv[3] = numeric
**      parv[4] = serverinfo
**
** on old protocols, serverinfo is parv[3], and numeric is left out
**
**  Recode 2001 by Stskeeps
*/
CMD_FUNC(m_server)
{
	char *servername = NULL;	/* Pointer for servername */
 /*	char *password = NULL; */
	char *ch = NULL;	/* */
	char *inpath = get_client_name(cptr, TRUE);
	aClient *acptr = NULL, *ocptr = NULL;
	ConfigItem_ban *bconf;
	int  hop = 0, numeric = 0;
	char info[REALLEN + 61];
	ConfigItem_link *aconf = NULL;
	ConfigItem_deny_link *deny;
	char *flags = NULL, *protocol = NULL, *inf = NULL;


	/* Ignore it  */
	if (IsPerson(sptr))
	{
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		sendto_one(cptr,
		    ":%s %s %s :*** Sorry, but your IRC program doesn't appear to support changing servers.",
		    me.name, IsWebTV(cptr) ? "PRIVMSG" : "NOTICE", cptr->name);
		sptr->since += 7;
		return 0;
	}

	/*
	 *  We do some parameter checks now. We want atleast upto serverinfo now
	 */
	if (parc < 4 || (!*parv[3]))
	{
		sendto_one(sptr, "ERROR :Not enough SERVER parameters");
		return exit_client(cptr, sptr, &me, 
			"Not enough parameters");		
	}

	if (IsUnknown(cptr) && (cptr->listener->umodes & LISTENER_CLIENTSONLY))
	{
		return exit_client(cptr, sptr, &me,
		    "This port is for clients only");
	}

	/* Now, let us take a look at the parameters we got
	 * Passes here:
	 *    Check for bogus server name
	 */

	servername = parv[1];
	/* Cut off if too big */
	if (strlen(servername) > HOSTLEN)
		servername[HOSTLEN] = '\0';
	/* Check if bogus, like spaces and ~'s */
	for (ch = servername; *ch; ch++)
		if (*ch <= ' ' || *ch > '~')
			break;
	if (*ch || !index(servername, '.'))
	{
		sendto_one(sptr, "ERROR :Bogus server name (%s)",
		    sptr->name, servername);
		sendto_snomask
		    (SNO_JUNK,
		    "WARNING: Bogus server name (%s) from %s (maybe just a fishy client)",
		    servername, get_client_name(cptr, TRUE));

		return exit_client(cptr, sptr, &me, "Bogus server name");
	}

	if ((IsUnknown(cptr) || IsHandshake(cptr)) && !cptr->passwd)
	{
		sendto_one(sptr, "ERROR :Missing password");
		return exit_client(cptr, sptr, &me, "Missing password");
	}

	/*
	 * Now, we can take a look at it all
	 */
	if (IsUnknown(cptr) || IsHandshake(cptr))
	{
		char xerrmsg[256];
		ConfigItem_link *link;
		
		strcpy(xerrmsg, "No matching link configuration");
		/* First check if the server is in the list */
		if (!servername) {
			strcpy(xerrmsg, "Null servername");
			goto errlink;
		}
		for(link = conf_link; link; link = (ConfigItem_link *) link->next)
			if (!match(link->servername, servername))
				break;
		if (!link) {
			snprintf(xerrmsg, 256, "No link block named '%s'", servername);
			goto errlink;
		}
		if (link->username && match(link->username, cptr->username)) {
			snprintf(xerrmsg, 256, "Username '%s' didn't match '%s'",
				cptr->username, link->username);
			/* I assume nobody will have 2 link blocks with the same servername
			 * and different username. -- Syzop
			 */
			goto errlink;
		}
		/* For now, we don't check based on DNS, it is slow, and IPs are better.
		 * We also skip checking if link::options::nohostcheck is set.
		 */
		if (link->options & CONNECT_NOHOSTCHECK)
		{
			aconf = link;
			goto nohostcheck;
		}
		aconf = Find_link(cptr->username, cptr->sockhost, cptr->sockhost,
		    servername);
		
#ifdef INET6
		/*  
		 * We first try match on uncompressed form ::ffff:192.168.1.5 thing included
		*/
		if (!aconf)
			aconf = Find_link(cptr->username, cptr->sockhost, Inet_ia2pNB(&cptr->ip, 0), servername);
		/* 
		 * Then on compressed 
		*/
		if (!aconf)
			aconf = Find_link(cptr->username, cptr->sockhost, Inet_ia2pNB(&cptr->ip, 1), servername);
#endif		
		if (!aconf)
		{
			snprintf(xerrmsg, 256, "Server is in link block but IP/host didn't match");
errlink:
			/* Send the "simple" error msg to the server */
			sendto_one(cptr,
			    "ERROR :Link denied (No matching link configuration) %s",
			    inpath);
			/* And send the "verbose" error msg only to local failops */
			sendto_locfailops
			    ("Link denied for %s(%s@%s) (%s) %s",
			    servername, cptr->username, cptr->sockhost, xerrmsg, inpath);
			return exit_client(cptr, sptr, &me,
			    "Link denied (No matching link configuration)");
		}
nohostcheck:
		/* Now for checking passwords */
		if (Auth_Check(cptr, aconf->recvauth, cptr->passwd) == -1)
		{
			sendto_one(cptr,
			    "ERROR :Link denied (Authentication failed) %s",
			    inpath);
			sendto_locfailops
			    ("Link denied (Authentication failed [Bad password?]) %s", inpath);
			return exit_client(cptr, sptr, &me,
			    "Link denied (Authentication failed)");
		}

		/*
		 * Third phase, we check that the server does not exist
		 * already
		 */
		if ((acptr = find_server(servername, NULL)))
		{
			/* Found. Bad. Quit. */
			acptr = acptr->from;
			ocptr =
			    (cptr->firsttime > acptr->firsttime) ? acptr : cptr;
			acptr =
			    (cptr->firsttime > acptr->firsttime) ? cptr : acptr;
			sendto_one(acptr,
			    "ERROR :Server %s already exists from %s",
			    servername,
			    (ocptr->from ? ocptr->from->name : "<nobody>"));
			sendto_realops
			    ("Link %s cancelled, server %s already exists from %s",
			    get_client_name(acptr, TRUE), servername,
			    (ocptr->from ? ocptr->from->name : "<nobody>"));
			return exit_client(acptr, acptr, acptr,
			    "Server Exists");
		}
		if ((bconf = Find_ban(servername, CONF_BAN_SERVER)))
		{
			sendto_realops
				("Cancelling link %s, banned server",
				get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
			return exit_client(cptr, cptr, &me, "Banned server");
		}
		if (aconf->class->clients + 1 > aconf->class->maxclients)
		{
			sendto_realops
				("Cancelling link %s, full class",
					get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "Full class");
		}
		/* OK, let us check in the data now now */
		hop = TS2ts(parv[2]);
		numeric = (parc > 4) ? TS2ts(parv[3]) : 0;
		if ((numeric < 0) || (numeric > 255))
		{
			sendto_realops
				("Cancelling link %s, invalid numeric",
					get_client_name(cptr, TRUE));
			return exit_client(cptr, cptr, &me, "Invalid numeric");
		}
		strncpyzt(info, parv[parc - 1], REALLEN + 61);
		strncpyzt(cptr->name, servername, sizeof(cptr->name));
		cptr->hopcount = hop;
		/* Add ban server stuff */
		if (SupportVL(cptr))
		{
			/* we also have a fail safe incase they say they are sending
			 * VL stuff and don't -- codemastr
			 */
			ConfigItem_deny_version *vlines;
			inf = NULL;
			protocol = NULL;
			flags = NULL;
			num = NULL;
			protocol = (char *)strtok((char *)info, "-");
			if (protocol)
				flags = (char *)strtok((char *)NULL, "-");
			if (flags)
				num = (char *)strtok((char *)NULL, " ");
			if (num)
				inf = (char *)strtok((char *)NULL, "");
			if (inf) {
				strncpyzt(cptr->info, inf[0] ? inf : me.name,
				    sizeof(cptr->info));

				for (vlines = conf_deny_version; vlines; vlines = (ConfigItem_deny_version *) vlines->next) {
					if (!match(vlines->mask, cptr->name))
						break;
				}
				if (vlines) {
					char *proto = vlines->version;
					char *vflags = vlines->flags;
					int version, result = 0, i;
					protocol++;
					version = atoi(protocol);
					switch (*proto) {
						case '<':
							proto++;
							if (version < atoi(proto))
								result = 1;
							break;
						case '>':
							proto++;
							if (version > atoi(proto))
								result = 1;
							break;
						case '=':
							proto++;
							if (version == atoi(proto))
								result = 1;
							break;
						case '!':
							proto++;
							if (version != atoi(proto))
								result = 1;
							break;
						default:
							if (version == atoi(proto))
								result = 1;
							break;
					}
					if (version == 0 || *proto == '*')
						result = 0;

					if (result)
						return exit_client(cptr, cptr, cptr,
							"Denied by V:line");

					for (i = 0; vflags[i]; i++) {
						if (vflags[i] == '!') {
							i++;
							if (strchr(flags, vflags[i])) {
								result = 1;
								break;
							}
						}
						else if (!strchr(flags, vflags[i])) {
								result = 1;
								break;
						}
					}
					if (*vflags == '*' || !strcmp(flags, "0"))
						result = 0;
					if (result)
						return exit_client(cptr, cptr, cptr,
							"Denied by V:line");
				}
			}
			else
				strncpyzt(cptr->info, info[0] ? info : me.name,
				    sizeof(cptr->info));

		}
		else
				strncpyzt(cptr->info, info[0] ? info : me.name,
					sizeof(cptr->info));
		/* Numerics .. */
		numeric = num ? atol(num) : numeric;
		if (numeric)
		{
			if ((numeric < 0) || (numeric > 254))
			{
				sendto_locfailops("Link %s denied, numeric '%d' out of range (should be 0-254)",
					inpath, numeric);

				return exit_client(cptr, cptr, cptr,
				    "Numeric out of range (0-254)");
			}
			if (numeric_collides(numeric))
			{
				sendto_locfailops("Link %s denied, colliding server numeric",
					inpath);

				return exit_client(cptr, cptr, cptr,
				    "Colliding server numeric (choose another)");
			}
		}
		for (deny = conf_deny_link; deny; deny = (ConfigItem_deny_link *) deny->next) {
			if (deny->flag.type == CRULE_ALL && !match(deny->mask, servername)
				&& crule_eval(deny->rule)) {
				sendto_ops("Refused connection from %s.",
					get_client_host(cptr));
				return exit_client(cptr, cptr, cptr,
					"Disallowed by connection rule");
			}
		}
		if (aconf->options & CONNECT_QUARANTINE)
			cptr->flags |= FLAGS_QUARANTINE;
		/* Start synch now */
		if (m_server_synch(cptr, numeric, aconf) == FLUSH_BUFFER)
			return FLUSH_BUFFER;
	}
	else
	{
		return m_server_remote(cptr, sptr, parc, parv);
	}
	return 0;
}

CMD_FUNC(m_server_remote)
{
	aClient *acptr, *ocptr, *bcptr;
	ConfigItem_link	*aconf;
	ConfigItem_ban *bconf;
	int 	hop;
	char	info[REALLEN + 61];
	long	numeric = 0;
	char	*servername = parv[1];
	int	i;

	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */
		acptr = acptr->from;
		ocptr =
		    (cptr->firsttime > acptr->firsttime) ? acptr : cptr;
		acptr =
		    (cptr->firsttime > acptr->firsttime) ? cptr : acptr;
		sendto_one(acptr,
		    "ERROR :Server %s already exists from %s",
		    servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		sendto_realops
		    ("Link %s cancelled, server %s already exists from %s",
		    get_client_name(acptr, TRUE), servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		if (acptr == cptr) {
			return exit_client(acptr, acptr, acptr, "Server Exists");
		} else {
			/* AFAIK this can cause crashes if this happends remotely because
			 * we will still receive msgs for some time because of lag.
			 * Two possible solutions: unlink the directly connected server (cptr)
			 * and/or fix all those commands which blindly trust server input. -- Syzop
			 */
			exit_client(acptr, acptr, acptr, "Server Exists");
			return 0;
		}
	}
	if ((bconf = Find_ban(servername, CONF_BAN_SERVER)))
	{
		sendto_realops
			("Cancelling link %s, banned server %s",
			get_client_name(cptr, TRUE), servername);
		sendto_one(cptr, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
		return exit_client(cptr, cptr, &me, "Brought in banned server");
	}
	/* OK, let us check in the data now now */
	hop = TS2ts(parv[2]);
	numeric = (parc > 4) ? TS2ts(parv[3]) : 0;
	if ((numeric < 0) || (numeric > 255))
	{
		sendto_realops
			("Cancelling link %s, invalid numeric at server %s",
				get_client_name(cptr, TRUE), servername);
		sendto_one(cptr, "ERROR :Invalid numeric (%s)",
			servername);
		return exit_client(cptr, cptr, &me, "Invalid remote numeric");
	}
	strncpyzt(info, parv[parc - 1], REALLEN + 61);
	if (!cptr->serv->conf)
	{
		sendto_realops("Lost conf for %s!!, dropping link", cptr->name);
		return exit_client(cptr, cptr, cptr, "Lost configuration");
	}
	aconf = cptr->serv->conf;
	if (!aconf->hubmask)
	{
		sendto_locfailops("Link %s cancelled, is Non-Hub but introduced Leaf %s",
			cptr->name, servername);
		return exit_client(cptr, cptr, cptr, "Non-Hub Link");
	}
	if (match(aconf->hubmask, servername))
	{
		sendto_locfailops("Link %s cancelled, linked in %s, which hub config disallows", cptr->name, servername);
		return exit_client(cptr, cptr, cptr, "Not matching hub configuration");
	}
	if (aconf->leafmask)
	{
		if (match(aconf->leafmask, servername))
		{
			sendto_locfailops("Link %s(%s) cancelled, disallowed by leaf configuration", cptr->name, servername);
			return exit_client(cptr, cptr, cptr, "Disallowed by leaf configuration");
		}
	}
	if (aconf->leafdepth && (hop > aconf->leafdepth))
	{
			sendto_locfailops("Link %s(%s) cancelled, too deep depth", cptr->name, servername);
			return exit_client(cptr, cptr, cptr, "Too deep link depth (leaf)");
	}
	if (numeric)
	{
		if ((numeric < 0) || (numeric > 254))
		{
			sendto_locfailops("Link %s(%s) cancelled, numeric '%d' out of range (should be 0-254)",
				cptr->name, servername, numeric);
			return exit_client(cptr, cptr, cptr,
			    "Numeric out of range (0-254)");
		}
		if (numeric_collides(numeric))
		{
			sendto_locfailops("Link %s(%s) cancelled, colliding server numeric",
					cptr->name, servername);

			return exit_client(cptr, cptr, cptr,
			    "Colliding server numeric (choose another)");
		}
	}
	acptr = make_client(cptr, find_server_quick(parv[0]));
	(void)make_server(acptr);
	acptr->serv->numeric = numeric;
	acptr->hopcount = hop;
	strncpyzt(acptr->name, servername, sizeof(acptr->name));
	strncpyzt(acptr->info, info, sizeof(acptr->info));
	acptr->serv->up = find_or_add(parv[0]);
	SetServer(acptr);
	/* Taken from bahamut makes it so all servers behind a U:lined
	 * server are also U:lined, very helpful if HIDE_ULINES is on
	 */
	if (IsULine(cptr)
	    || (Find_uline(acptr->name)))
		acptr->flags |= FLAGS_ULINE;
	add_server_to_table(acptr);
	IRCstats.servers++;
	(void)find_or_add(acptr->name);
   /*	acptr->flags |= FLAGS_TS8; */
	add_client_to_list(acptr);
	(void)add_to_client_hash_table(acptr->name, acptr);
	RunHook(HOOKTYPE_SERVER_CONNECT, acptr);
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(bcptr = local[i]) || !IsServer(bcptr) ||
			    bcptr == cptr || IsMe(bcptr))
				continue;
		if (SupportNS(bcptr))
		{
			sendto_one(bcptr,
				"%c%s %s %s %d %i :%s",
				(sptr->serv->numeric ? '@' : ':'),
				(sptr->serv->numeric ? base64enc(sptr->serv->numeric) : sptr->name),
				IsToken(bcptr) ? TOK_SERVER : MSG_SERVER,
				acptr->name, hop + 1, numeric, acptr->info);
		}
			else
		{
			sendto_one(bcptr, ":%s %s %s %d :%s",
			    parv[0],
			    IsToken(bcptr) ? TOK_SERVER : MSG_SERVER,
			    acptr->name, hop + 1, acptr->info);
		}
	}
	return 0;
}

/*
 * send_proto:
 * sends PROTOCTL message to server, taking care of whether ZIP
 * should be enabled or not.
 */
void send_proto(aClient *cptr, ConfigItem_link *aconf)
{
#ifdef ZIP_LINKS
	if (aconf->options & CONNECT_ZIP)
	{
		sendto_one(cptr, "PROTOCTL %s ZIP", PROTOCTL_SERVER);
	} else {
#endif
		sendto_one(cptr, "PROTOCTL %s", PROTOCTL_SERVER);
#ifdef ZIP_LINKS
	}
#endif
}

int	m_server_synch(aClient *cptr, long numeric, ConfigItem_link *aconf)
{
	char		*inpath = get_client_name(cptr, TRUE);
	extern char 	serveropts[];
	aClient		*acptr;
	int		i;


	if (cptr->passwd)
	{
		MyFree(cptr->passwd);
		cptr->passwd = NULL;
	}
	if (IsUnknown(cptr))
	{
		/* If this is an incomming connection, then we have just received
		 * their stuff and now send our stuff back.
		 */
		send_proto(cptr, aconf);
		sendto_one(cptr, "PASS :%s", aconf->connpwd);
		sendto_one(cptr, "SERVER %s 1 :U%d-%s-%i %s",
			    me.name, UnrealProtocol,
			    serveropts, me.serv->numeric,
			    (me.info[0]) ? (me.info) : "IRCers United");
	}
#ifdef ZIP_LINKS
	if (aconf->options & CONNECT_ZIP)
	{
		if (cptr->proto & PROTO_ZIP)
		{
			if (zip_init(cptr, aconf->compression_level ? aconf->compression_level : ZIP_DEFAULT_LEVEL) == -1)
			{
				zip_free(cptr);
				sendto_realops("Unable to setup compressed link for %s", get_client_name(cptr, TRUE));
				return exit_client(cptr, cptr, &me, "zip_init() failed");
			}
			SetZipped(cptr);
			cptr->zip->first = 1;
		} else {
			sendto_realops("WARNING: Remote doesnt have link::options::zip set. Compression disabled.");
		}
	}
#endif

#if 0
/* Disabled because it may generate false warning when linking with cvs versions between b14 en b15 -- Syzop */
	if ((cptr->proto & PROTO_ZIP) && !(aconf->options & CONNECT_ZIP))
	{
#ifdef ZIP_LINKS
		sendto_realops("WARNING: Remote requested compressed link, but we don't have link::options::zip set. Compression disabled.");
#else
		sendto_realops("WARNING: Remote requested compressed link, but we don't have zip links support compiled in. Compression disabled.");
#endif
	}
#endif
	/* Set up server structure */
	SetServer(cptr);
	IRCstats.me_servers++;
	IRCstats.servers++;
	IRCstats.unknown--;
#ifndef NO_FDLIST
	addto_fdlist(cptr->slot, &serv_fdlist);
#endif
	if ((Find_uline(cptr->name)))
		cptr->flags |= FLAGS_ULINE;
  /*	cptr->flags |= FLAGS_TS8; */
	nextping = TStime();
	(void)find_or_add(cptr->name);
#ifdef USE_SSL
	if (IsSecure(cptr))
	{
		sendto_serv_butone(&me, ":%s SMO o :(\2link\2) Secure %slink %s -> %s established (%s)",
			me.name,
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath, (char *) ssl_get_cipher((SSL *)cptr->ssl));
		sendto_realops("(\2link\2) Secure %slink %s -> %s established (%s)",
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath, (char *) ssl_get_cipher((SSL *)cptr->ssl));
	}
	else
#endif
	{
		sendto_serv_butone(&me, ":%s SMO o :(\2link\2) %sLink %s -> %s established",
			me.name,
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath);
		sendto_realops("(\2link\2) %sLink %s -> %s established",
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath);
	}
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	(void)make_server(cptr);
	cptr->serv->up = me.name;
	cptr->srvptr = &me;
	cptr->serv->numeric = numeric;
	cptr->serv->conf = aconf;
	cptr->serv->conf->refcount++;
	cptr->serv->conf->class->clients++;
	cptr->class = cptr->serv->conf->class;
	add_server_to_table(cptr);
	RunHook(HOOKTYPE_SERVER_CONNECT, cptr);
	for (i = 0; i <= LastSlot; i++)
	{
		if (!(acptr = local[i]) || !IsServer(acptr) ||
		    acptr == cptr || IsMe(acptr))
			continue;

		if (SupportNS(acptr))
		{
			sendto_one(acptr, "%c%s %s %s 2 %i :%s",
			    (me.serv->numeric ? '@' : ':'),
			    (me.serv->numeric ? base64enc(me.
			    serv->numeric) : me.name),
			    (IsToken(acptr) ? TOK_SERVER : MSG_SERVER),
			    cptr->name, cptr->serv->numeric, cptr->info);
		}
		else
		{
			sendto_one(acptr, ":%s %s %s 2 :%s",
			    me.name,
			    (IsToken(acptr) ? TOK_SERVER : MSG_SERVER),
			    cptr->name, cptr->info);
		}
	}
	for (acptr = &me; acptr; acptr = acptr->prev)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsServer(acptr))
		{
			if (SupportNS(cptr))
			{
				/* this has to work. */
				numeric =
				    ((aClient *)find_server_quick(acptr->
				    serv->up))->serv->numeric;

				sendto_one(cptr, "%c%s %s %s %d %i :%s",
				    (numeric ? '@' : ':'),
				    (numeric ? base64enc(numeric) :
				    acptr->serv->up),
				    IsToken(cptr) ? TOK_SERVER : MSG_SERVER,
				    acptr->name, acptr->hopcount + 1,
				    acptr->serv->numeric, acptr->info);
			}
			else
				sendto_one(cptr, ":%s %s %s %d :%s",
				    acptr->serv->up,
				    (IsToken(cptr) ? TOK_SERVER : MSG_SERVER),
				    acptr->name, acptr->hopcount + 1,
				    acptr->info);

		}
	}
	/* Synching nick information */
	for (acptr = &me; acptr; acptr = acptr->prev)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsPerson(acptr))
		{
			if (!SupportNICKv2(cptr))
			{
				sendto_one(cptr,
				    "%s %s %d %d %s %s %s %lu :%s",
				    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
				    acptr->name, acptr->hopcount + 1,
				    acptr->lastnick, acptr->user->username,
				    acptr->user->realhost,
				    acptr->user->server,
				    acptr->user->servicestamp, acptr->info);
				send_umode(cptr, acptr, 0, SEND_UMODES, buf);
				if (IsHidden(acptr) && acptr->user->virthost)
					sendto_one(cptr, ":%s %s %s",
					    acptr->name,
					    (IsToken(cptr) ? TOK_SETHOST :
					    MSG_SETHOST),
					    acptr->user->virthost);
			}
			else
			{
				send_umode(NULL, acptr, 0, SEND_UMODES, buf);

				if (!SupportVHP(cptr))
				{
					if (SupportNS(cptr)
					    && acptr->srvptr->serv->numeric)
					{
						sendto_one(cptr,
						    cptr->proto & PROTO_SJB64 ?
						    "%s %s %d %B %s %s %b %lu %s %s :%s"
						    :
						    "%s %s %d %d %s %s %b %lu %s %s :%s",
						    (IsToken(cptr) ? TOK_NICK :
						    MSG_NICK), acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    acptr->srvptr->
						    serv->numeric,
						    acptr->user->servicestamp,
						    (!buf
						    || *buf ==
						    '\0' ? "+" : buf),
						    ((IsHidden(acptr)
						    && (acptr->
						    umodes & UMODE_SETHOST)) ?
						    acptr->
						    user->virthost : "*"),
						    acptr->info);
					}
					else
					{
						sendto_one(cptr,
						    (cptr->proto & PROTO_SJB64 ?
						    "%s %s %d %B %s %s %s %lu %s %s :%s"
						    :
						    "%s %s %d %d %s %s %s %lu %s %s :%s"),
						    (IsToken(cptr) ? TOK_NICK :
						    MSG_NICK), acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    acptr->user->server,
						    acptr->user->servicestamp,
						    (!buf
						    || *buf ==
						    '\0' ? "+" : buf),
						    ((IsHidden(acptr)
						    && (acptr->umodes &
						    UMODE_SETHOST)) ?
						    acptr->
						    user->virthost : "*"),
						    acptr->info);
					}
				}
				else
					sendto_one(cptr,
					    "%s %s %d %d %s %s %s %lu %s %s :%s",
					    (IsToken(cptr) ? TOK_NICK :
					    MSG_NICK), acptr->name,
					    acptr->hopcount + 1,
					    acptr->lastnick,
					    acptr->user->username,
					    acptr->user->realhost,
					    (SupportNS(cptr) ?
					    (acptr->srvptr->serv->numeric ?
					    base64enc(acptr->srvptr->
					    serv->numeric) : acptr->
					    user->server) : acptr->user->
					    server), acptr->user->servicestamp,
					    (!buf
					    || *buf == '\0' ? "+" : buf),
					    GetHost(acptr),
					    acptr->info);
			}

			if (acptr->user->away)
				sendto_one(cptr, ":%s %s :%s", acptr->name,
				    (IsToken(cptr) ? TOK_AWAY : MSG_AWAY),
				    acptr->user->away);
			if (acptr->user->swhois)
				if (*acptr->user->swhois != '\0')
					sendto_one(cptr, "%s %s :%s",
					    (IsToken(cptr) ? TOK_SWHOIS :
					    MSG_SWHOIS), acptr->name,
					    acptr->user->swhois);

			if (!SupportSJOIN(cptr))
				send_user_joins(cptr, acptr);
		}
	}
	/*
	   ** Last, pass all channels plus statuses
	 */
	{
		aChannel *chptr;
		for (chptr = channel; chptr; chptr = chptr->nextch)
		{
			if (!SupportSJOIN(cptr))
				send_channel_modes(cptr, chptr);
			else if (SupportSJOIN(cptr) && !SupportSJ3(cptr))
			{
				send_channel_modes_sjoin(cptr, chptr);
			}
			else
				send_channel_modes_sjoin3(cptr, chptr);
			if (chptr->topic_time)
				sendto_one(cptr,
				    (cptr->proto & PROTO_SJB64 ?
				    "%s %s %s %B :%s"
				    :
				    "%s %s %s %lu :%s"),
				    (IsToken(cptr) ? TOK_TOPIC : MSG_TOPIC),
				    chptr->chname, chptr->topic_nick,
				    chptr->topic_time, chptr->topic);
		}
	}
	/* pass on TKLs */
	tkl_synch(cptr);

	/* send out SVSFLINEs */
	dcc_sync(cptr);

	/*
	   ** Pass on all services based q-lines
	 */
	{
		ConfigItem_ban *bconf;
		char *ns = NULL;

		if (me.serv->numeric && SupportNS(cptr))
			ns = base64enc(me.serv->numeric);
		else
			ns = NULL;

		for (bconf = conf_ban; bconf; bconf = (ConfigItem_ban *) bconf->next)
		{
			if (bconf->flag.type == CONF_BAN_NICK) {
				if (bconf->flag.type2 == CONF_BAN_TYPE_AKILL) {
					if (bconf->reason)
						sendto_one(cptr, "%s%s %s %s :%s",
						    ns ? "@" : ":",
						    ns ? ns : me.name,
						    (IsToken(cptr) ? TOK_SQLINE :
						    MSG_SQLINE), bconf->mask,
						    bconf->reason);
					else
						sendto_one(cptr, "%s%s %s %s",
						    ns ? "@" : ":",
						    ns ? ns : me.name,
						    (IsToken(cptr) ? TOK_SQLINE :
						    MSG_SQLINE), bconf->mask);
				}
			}
		}
	}

	sendto_one(cptr, "%s %li %li %li %X 0 0 0 :%s",
	    (IsToken(cptr) ? TOK_NETINFO : MSG_NETINFO),
	    IRCstats.global_max, TStime(), UnrealProtocol,
	    CLOAK_KEYCRC,
	    ircnetwork);
	return 0;

}

/*
** m_links
**	parv[0] = sender prefix
** or
**	parv[0] = sender prefix
**
** Recoded by Stskeeps
*/
CMD_FUNC(m_links)
{
	Link *lp;
	aClient *acptr;

	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;

		/* Some checks */
		if (HIDE_ULINES && IsULine(acptr) && !IsAnOper(sptr))
			continue;
		sendto_one(sptr, rpl_str(RPL_LINKS),
		    me.name, parv[0], acptr->name, acptr->serv->up,
		    acptr->hopcount, (acptr->info[0] ? acptr->info :
		    "(Unknown Location)"));
	}

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0], "*");
	return 0;
}


/*
** m_netinfo
** by Stskeeps
**  parv[0] = sender prefix
**  parv[1] = max global count
**  parv[2] = time of end sync
**  parv[3] = unreal protocol using (numeric)
**  parv[4] = cloak-crc (> u2302)
**  parv[5] = free(**)
**  parv[6] = free(**)
**  parv[7] = free(**)
**  parv[8] = ircnet
**/

CMD_FUNC(m_netinfo)
{
	long 		lmax;
	time_t	 	xx;
	long 		endsync, protocol;
	char		buf[512];

	if (IsPerson(sptr))
		return 0;
	if (!IsServer(cptr))
		return 0;

	if (parc < 3)
	{
		/* Talking to a UnProtocol 2090 */
		sendto_realops
		    ("Link %s is using a too old UnProtocol - (parc < 3)",
		    cptr->name);
		return 0;
	}
	if (parc < 9)
	{
		return 0;
	}

	if (GotNetInfo(cptr))
	{
		sendto_realops("Already got NETINFO from Link %s", cptr->name);
		return 0;
	}
	/* is a long therefore not ATOI */
	lmax = atol(parv[1]);
	endsync = TS2ts(parv[2]);
	protocol = atol(parv[3]);

	/* max global count != max_global_count --sts */
	if (lmax > IRCstats.global_max)
	{
		IRCstats.global_max = lmax;
		sendto_realops("Max Global Count is now %li (set by link %s)",
		    lmax, cptr->name);
	}

	xx = TStime();
	if ((xx - endsync) < 0)
	{
		sendto_realops
		    ("Possible negative TS split at link %s (%li - %li = %li)",
		    cptr->name, (xx), (endsync), (xx - endsync));
		sendto_serv_butone(&me,
		    ":%s SMO o :\2(sync)\2 Possible negative TS split at link %s (%li - %li = %li)",
		    me.name, cptr->name, (xx), (endsync), (xx - endsync));
	}
	sendto_realops
	    ("Link %s -> %s is now synced [secs: %li recv: %li.%li sent: %li.%li]",
	    cptr->name, me.name, (TStime() - endsync), sptr->receiveK,
	    sptr->receiveB, sptr->sendK, sptr->sendB);
#ifdef ZIP_LINKS
	if ((MyConnect(cptr)) && (IsZipped(cptr)) && cptr->zip->in->total_out && cptr->zip->out->total_in) {
		sendto_realops
		("Zipstats for link to %s: decompressed (in): %01lu/%01lu (%3.1f%%), compressed (out): %01lu/%01lu (%3.1f%%)",
			get_client_name(cptr, TRUE),
			cptr->zip->in->total_in, cptr->zip->in->total_out,
			(100.0*(float)cptr->zip->in->total_in) /(float)cptr->zip->in->total_out,
			cptr->zip->out->total_in, cptr->zip->out->total_out,
			(100.0*(float)cptr->zip->out->total_out) /(float)cptr->zip->out->total_in);
	}
#endif

	sendto_serv_butone(&me,
	    ":%s SMO o :\2(sync)\2 Link %s -> %s is now synced [secs: %li recv: %li.%li sent: %li.%li]",
	    me.name, cptr->name, me.name, (TStime() - endsync), sptr->receiveK,
	    sptr->receiveB, sptr->sendK, sptr->sendB);

	if (!(strcmp(ircnetwork, parv[8]) == 0))
	{
		sendto_realops("Network name mismatch from link %s (%s != %s)",
		    cptr->name, parv[8], ircnetwork);
		sendto_serv_butone(&me,
		    ":%s SMO o :\2(sync)\2 Network name mismatch from link %s (%s != %s)",
		    me.name, cptr->name, parv[8], ircnetwork);
	}

	if ((protocol != UnrealProtocol) && (protocol != 0))
	{
		sendto_realops
		    ("Link %s is running Protocol u%li while we are running %li!",
		    cptr->name, protocol, UnrealProtocol);
		sendto_serv_butone(&me,
		    ":%s SMO o :\2(sync)\2 Link %s is running u%li while %s is running %li!",
		    me.name, cptr->name, protocol, me.name, UnrealProtocol);

	}
	ircsprintf(buf, "%lX", CLOAK_KEYCRC);
	if (*parv[4] != '*' && strcmp(buf, parv[4]))
	{
		sendto_realops
			("Link %s is having a different cloak key - %s != %s",
				cptr->name, parv[4], buf);
	}
	SetNetInfo(cptr);
	return 0;
}

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

/*
 * sends m_info into to sptr
*/

void m_info_send(aClient *sptr)
{
	sendto_one(sptr, ":%s %d %s :=-=-=-= %s =-=-=-=",
	    me.name, RPL_INFO, sptr->name, IRCDTOTALVERSION);
	sendto_one(sptr, ":%s %d %s :| Brought to you by the following people:",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| Head coders:", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Stskeeps     <stskeeps@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * codemastr    <codemastr@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Luke         <luke@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * McSkaf       <mcskaf@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Syzop        <syzop@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| Contributors:", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Zogg         <zogg@unrealircd.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * NiQuiL       <niquil@unrealircd.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * assyrian     <assyrian@unrealircd.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * chasm        <chasm@unrealircd.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * DrBin        <drbin@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * llthangel    <llthangel@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Griever      <griever@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * nighthawk    <nighthawk@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| Credits - Type /Credits",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| DALnet Credits - Type /DalInfo",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| This is an UnrealIRCD-style server",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| If you find any bugs, please mail",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|  bugs@lists.unrealircd.org",
	    me.name, RPL_INFO, sptr->name);

	sendto_one(sptr,
	    ":%s %d %s :| UnrealIRCd Homepage: http://www.unrealircd.com",
	    me.name, RPL_INFO, sptr->name);

#ifdef _WIN32
#ifdef WIN32_SPECIFY
	sendto_one(sptr, ":%s %d %s :| wIRCd porter: | %s",
	    me.name, RPL_INFO, sptr->name, WIN32_PORTER);
	sendto_one(sptr, ":%s %d %s :|     >>URL:    | %s",
	    me.name, RPL_INFO, sptr->name, WIN32_URL);
#endif
#endif
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


/*
 * RPL_NOWON	- Online at the moment (Succesfully added to WATCH-list)
 * RPL_NOWOFF	- Offline at the moement (Succesfully added to WATCH-list)
 * RPL_WATCHOFF	- Succesfully removed from WATCH-list.
 * ERR_TOOMANYWATCH - Take a guess :>  Too many WATCH entries.
 */
static void show_watch(aClient *cptr, char *name, int rpl1, int rpl2)
{
	aClient *acptr;


	if ((acptr = find_person(name, NULL)))
	{
		sendto_one(cptr, rpl_str(rpl1), me.name, cptr->name,
		    acptr->name, acptr->user->username,
		    IsHidden(acptr) ? acptr->user->virthost : acptr->user->
		    realhost, acptr->lastnick);
	}
	else
		sendto_one(cptr, rpl_str(rpl2), me.name, cptr->name,
		    name, "*", "*", 0);
}

/*
 * m_watch
 */
CMD_FUNC(m_watch)
{
	aClient *acptr;
	char *s, **pav = parv, *user;
	char *p = NULL, *def = "l";



	if (parc < 2)
	{
		/*
		 * Default to 'l' - list who's currently online
		 */
		parc = 2;
		parv[1] = def;
	}

	for (s = (char *)strtoken(&p, *++pav, " "); s;
	    s = (char *)strtoken(&p, NULL, " "))
	{
		if ((user = (char *)index(s, '!')))
			*user++ = '\0';	/* Not used */

		/*
		 * Prefix of "+", they want to add a name to their WATCH
		 * list.
		 */
		if (*s == '+')
		{
			if (do_nick_name(s + 1))
			{
				if (sptr->watches >= MAXWATCH)
				{
					sendto_one(sptr,
					    err_str(ERR_TOOMANYWATCH), me.name,
					    cptr->name, s + 1);

					continue;
				}

				add_to_watch_hash_table(s + 1, sptr);
			}

			show_watch(sptr, s + 1, RPL_NOWON, RPL_NOWOFF);
			continue;
		}

		/*
		 * Prefix of "-", coward wants to remove somebody from their
		 * WATCH list.  So do it. :-)
		 */
		if (*s == '-')
		{
			del_from_watch_hash_table(s + 1, sptr);
			show_watch(sptr, s + 1, RPL_WATCHOFF, RPL_WATCHOFF);

			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			hash_del_watch_list(sptr);

			continue;
		}

		/*
		 * Now comes the fun stuff, "S" or "s" returns a status report of
		 * their WATCH list.  I imagine this could be CPU intensive if its
		 * done alot, perhaps an auto-lag on this?
		 */
		if (*s == 'S' || *s == 's')
		{
			Link *lp;
			aWatch *anptr;
			int  count = 0;

			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on.
			 */
			anptr = hash_get_watch(sptr->name);
			if (anptr)
				for (lp = anptr->watch, count = 1;
				    (lp = lp->next); count++)
					;
			sendto_one(sptr, rpl_str(RPL_WATCHSTAT), me.name,
			    parv[0], sptr->watches, count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			if ((lp = sptr->watch) == NULL)
			{
				sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST),
				    me.name, parv[0], *s);
				continue;
			}
			*buf = '\0';
			strlcpy(buf, lp->value.wptr->nick, sizeof buf);
			count =
			    strlen(parv[0]) + strlen(me.name) + 10 +
			    strlen(buf);
			while ((lp = lp->next))
			{
				if (count + strlen(lp->value.wptr->nick) + 1 >
				    BUFSIZE - 2)
				{
					sendto_one(sptr, rpl_str(RPL_WATCHLIST),
					    me.name, parv[0], buf);
					*buf = '\0';
					count =
					    strlen(parv[0]) + strlen(me.name) +
					    10;
				}
				strcat(buf, " ");
				strcat(buf, lp->value.wptr->nick);
				count += (strlen(lp->value.wptr->nick) + 1);
			}
			sendto_one(sptr, rpl_str(RPL_WATCHLIST), me.name,
			    parv[0], buf);

			sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name,
			    parv[0], *s);
			continue;
		}

		/*
		 * Well that was fun, NOT.  Now they want a list of everybody in
		 * their WATCH list AND if they are online or offline? Sheesh,
		 * greedy arn't we?
		 */
		if (*s == 'L' || *s == 'l')
		{
			Link *lp = sptr->watch;

			while (lp)
			{
				if ((acptr =
				    find_person(lp->value.wptr->nick, NULL)))
				{
					sendto_one(sptr, rpl_str(RPL_NOWON),
					    me.name, parv[0], acptr->name,
					    acptr->user->username,
					    IsHidden(acptr) ? acptr->user->
					    virthost : acptr->user->realhost,
					    acptr->lastnick);
				}
				/*
				 * But actually, only show them offline if its a capital
				 * 'L' (full list wanted).
				 */
				else if (isupper(*s))
					sendto_one(sptr, rpl_str(RPL_NOWOFF),
					    me.name, parv[0],
					    lp->value.wptr->nick, "*", "*",
					    lp->value.wptr->lasttime);
				lp = lp->next;
			}

			sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name,
			    parv[0], *s);

			continue;
		}

		/*
		 * Hmm.. unknown prefix character.. Ignore it. :-)
		 */
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
** m_stats
**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
*/
/*
**    Note:   The info is reported in the order the server uses
**            it--not reversed as in unrealircd.conf!
*/

CMD_FUNC(m_stats)
{
#ifndef DEBUGMODE
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since :Idle";
	static char Lformat[] = ":%s %d %s %s%s %u %u %u %u %u %u :%u";
#else
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since CPU :Idle";
	static char Lformat[] = ":%s %d %s %s%s %u %u %u %u %u %u %s";
	char pbuf[96];		/* Should be enough for to ints */
#endif
	ConfigItem_link *link_p;
	ConfigItem_oper *oper_p;
	ConfigItem_oper_from *oper_p_from;
	aCommand *mptr;
	aClient *acptr;
	char stat = parc > 1 ? parv[1][0] : '\0';
	char stat2;
	int  i;
	int  doall = 0, wilds = 0, showports = IsAnOper(sptr), remote = 0;
	char *name;

	if (IsServer(sptr))
		return 0;
	if (hunt_server_token(cptr, sptr, MSG_STATS, TOK_STATS, "%s :%s", 2, parc,
	    parv) != HUNTED_ISME)
		return 0;
	if (OPER_ONLY_STATS) {
		if (!IsAnOper(sptr) && strchr(OPER_ONLY_STATS, '*'))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return 0;
		}
		stat2 = tolower(stat);
		if (!IsAnOper(sptr) && (stat2 == 'c' || stat2 == 'f' || stat2 == 'i' ||  stat2 == 'h' || 
			stat2 == 'y' || stat2 == 'x' || stat2 == 'g' || stat2 == 'k' || stat2 == 'o' || 
			stat2 == 'z' || stat2 == 'l')) {
			if (strchr(OPER_ONLY_STATS, toupper(stat)) || strchr(OPER_ONLY_STATS, stat2)) {
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
				return 0;
			}
			if (stat2 == 'c') {
				if (strchr(OPER_ONLY_STATS, 'h') || strchr(OPER_ONLY_STATS, 'H')) {
					sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
					return 0;
				}
			}
			if (stat2 == 'h') {
				if (strchr(OPER_ONLY_STATS, 'c') || strchr(OPER_ONLY_STATS, 'C')) {
					sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
					return 0;
				}
			}
		}
		if (!IsAnOper(sptr) && strchr(OPER_ONLY_STATS, stat))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return 0;
		}
	}
	if (parc > 2)
	{
		name = parv[2];
		if (!mycmp(name, me.name))
			doall = 2;
		else if (match(name, me.name) == 0)
			doall = 1;
		if (index(name, '*') || index(name, '?'))
			wilds = 1;
	}
	else
		name = me.name;

	switch (stat)
	{
#ifdef STRIPBADWORDS
	  case 'b':
	  {
		  ConfigItem_badword *words;

		  for (words = conf_badword_channel; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
			  sendto_one(sptr, ":%s %i %s :c %c %s%s%s %s",
			      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
			      (words->type & BADW_TYPE_FAST_L) ? "*" : "",
			      words->word,
			      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
			      words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");
 #else
			  sendto_one(sptr, ":%s %i %s :c %s %s", me.name, RPL_TEXT, sptr->name,  words->word, 
				words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");
 #endif
		  }
		  for (words = conf_badword_message; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
			  sendto_one(sptr, ":%s %i %s :m %c %s%s%s %s",
			      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
			      (words->type & BADW_TYPE_FAST_L) ? "*" : "",
			      words->word,
			      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
			      words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");
 #else
			  sendto_one(sptr, ":%s %i %s :m %s %s", me.name, RPL_TEXT, sptr->name, words->word, 
				words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");

 #endif
		  }
		  for (words = conf_badword_quit; words; words = (ConfigItem_badword *) words->next) {
 #ifdef FAST_BADWORD_REPLACE
			  sendto_one(sptr, ":%s %i %s :q %c %s%s%s %s",
			      me.name, RPL_TEXT, sptr->name, words->type & BADW_TYPE_REGEX ? 'R' : 'F',
			      (words->type & BADW_TYPE_FAST_L) ? "*" : "",
			      words->word,
			      (words->type & BADW_TYPE_FAST_R) ? "*" : "",
			      words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");
 #else
			  sendto_one(sptr, ":%s %i %s :q %s %s", me.name, RPL_TEXT, sptr->name, words->word, 
				words->action == BADWORD_REPLACE ? 
				(words->replace ? words->replace : "<censored>") : "");

 #endif
		  }
		  break;
	  }
#endif
	  case 'L':
	  case 'l':
		  /*
		   * send info about connections which match, or all if the
		   * mask matches me.name.  Only restrictions are on those who
		   * are invisible not being visible to 'foreigners' who use
		   * a wild card based search to list it.
		   */
		  sendto_one(sptr, Sformat, me.name, RPL_STATSLINKINFO,
		      parv[0]);
		  if (IsServer(cptr))
		  {
			  remote = 1;
			  wilds = 0;
		  }
		  for (i = 0; i <= LastSlot; i++)
		  {
			  if (!(acptr = local[i]))
				  continue;
			  if (IsInvisible(acptr) && (doall || wilds) &&
			      !(MyConnect(sptr) && IsOper(sptr)) &&
			      !IsAnOper(acptr) && (acptr != sptr))
				  continue;
			  if (remote && doall && !IsServer(acptr) &&
			      !IsMe(acptr))
				  continue;
			  if (remote && !doall && IsServer(acptr))
				  continue;
			  if (!doall && wilds && match(name, acptr->name))
				  continue;
			  if (!(parc == 2 && (IsServer(acptr)
			      || (acptr->flags & FLAGS_LISTEN))) && !(doall
			      || wilds) && mycmp(name, acptr->name))
				  continue;

#ifdef DEBUGMODE
			  ircsprintf(pbuf, "%d :%d", acptr->cputime,
			      (acptr->user && MyConnect(acptr)) ?
			      TStime() - acptr->last : 0);
#endif
			  if (IsOper(sptr))
			  {
				  sendto_one(sptr, Lformat, me.name,
				      RPL_STATSLINKINFO, parv[0],
				      (isupper(stat)) ?
				      get_client_name2(acptr, showports) :
				      get_client_name(acptr, FALSE),
				      get_cptr_status(acptr),
				      (int)DBufLength(&acptr->sendQ),
				      (int)acptr->sendM, (int)acptr->sendK,
				      (int)acptr->receiveM,
				      (int)acptr->receiveK,
				      TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				      (acptr->user && MyConnect(acptr)) ?
				      TStime() - acptr->last : 0);
#else
				      pbuf);
#endif
				  if (!IsServer(acptr) && !IsMe(acptr) && IsAnOper(acptr) && sptr != acptr)
					  sendto_one(acptr,
					      ":%s %s %s :*** %s did a /stats L on you! IP may have been shown",
					      me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name, sptr->name);
			  }
			  else if (!strchr(acptr->name, '.'))
				  sendto_one(sptr, Lformat, me.name,
				      RPL_STATSLINKINFO, parv[0],
				      IsHidden(acptr) ? acptr->name :
				      (isupper(stat)) ?	/* Potvin - PreZ */
				      get_client_name2(acptr, showports) :
				      get_client_name(acptr, FALSE),
				      get_cptr_status(acptr),
				      (int)DBufLength(&acptr->sendQ),
				      (int)acptr->sendM, (int)acptr->sendK,
				      (int)acptr->receiveM,
				      (int)acptr->receiveK,
				      TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				      (acptr->user && MyConnect(acptr)) ?
				      TStime() - acptr->last : 0);
#else
				      pbuf);
#endif
		  }
		  break;
	  case 'C':
	  case 'c':
	  case 'H':
	  case 'h':
		  for (link_p = conf_link; link_p; link_p = (ConfigItem_link *) link_p->next)
		  {
			sendto_one(sptr, ":%s 213 %s C %s@%s * %s %i %s %s%s%s%s%s",
				me.name, sptr->name, IsOper(sptr) ? link_p->username : "*",
				IsOper(sptr) ? link_p->hostname : "*", link_p->servername,
				link_p->port,
				link_p->class->name,
				(link_p->options & CONNECT_AUTO) ? "a" : "",
				(link_p->options & CONNECT_SSL) ? "S" : "",
				(link_p->options & CONNECT_ZIP) ? "z" : "",
				(link_p->options & CONNECT_NODNSCACHE) ? "d" : "",
				(link_p->options & CONNECT_NOHOSTCHECK) ? "h" : "");
			if (link_p->hubmask)
			{
				sendto_one(sptr, ":%s 244 %s H %s * %s",
					me.name, sptr->name, link_p->hubmask,
					link_p->servername);
			}
			else
			if (link_p->leafmask)
			{
				sendto_one(sptr, ":%s 241 %s L %s * %s %d",
					me.name, sptr->name,
					link_p->leafmask, link_p->servername, link_p->leafdepth);
			}
		  }
		  break;
	  case 'f':
	  case 'F':
		  report_flines(sptr);
		  break;

	  case 'G':
	  case 'g':
		  tkl_stats(sptr);
		  break;
	  case 'I':
	  case 'i':
	  {
		  ConfigItem_allow *allows;
		  for (allows = conf_allow; allows; allows = (ConfigItem_allow *) allows->next) {
			  sendto_one(sptr, rpl_str(RPL_STATSILINE), me.name,
				  parv[0], allows->ip, allows->hostname, allows->maxperip, allows->class->name, allows->server ? allows->server : defserv, allows->port ? allows->port : 6667);
		  }
		  break;
	  }
	  case 'E':
	  {
		  ConfigItem_except *excepts;
		  for (excepts = conf_except; excepts; excepts = (ConfigItem_except *) excepts->next) {
			if (excepts->flag.type == 1)
				sendto_one(sptr, rpl_str(RPL_STATSKLINE), me.name,
				    parv[0], "E", excepts->mask, "");
		  }
		  break;
	  }
	  case 'e':
	  {
		  ConfigItem_except *excepts;
		  for (excepts = conf_except; excepts;
		      excepts = (ConfigItem_except *) excepts->next)
		  {
			  if (excepts->flag.type == 0)
				  sendto_one(sptr, rpl_str(RPL_STATSELINE),
				      me.name, parv[0], excepts->mask);
		  }
		  break;
	  }
	  case 'K':
	  case 'k':
	  {
		  ConfigItem_ban *bans;
		  ConfigItem_except *excepts;
		  char type[2];
  		  for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) {
			  if (bans->flag.type == CONF_BAN_USER) {
				if (bans->flag.type2 == CONF_BAN_TYPE_CONF)
					  type[0] = 'K';
				else if (bans->flag.type2 == CONF_BAN_TYPE_AKILL)
					  type[0] = 'A';
				else if (bans->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
					  type[0] = 'k';
				type[1] = '\0';
	  			sendto_one(sptr, rpl_str(RPL_STATSKLINE),
			 	    me.name, parv[0], type, bans->mask, bans->reason ? bans->reason : "<no reason>");
			  }
			  else if (bans->flag.type == CONF_BAN_IP) {
				if (bans->flag.type2 == CONF_BAN_TYPE_CONF)
					type[0] = 'Z';
				else if (bans->flag.type2 == CONF_BAN_TYPE_AKILL)
					type[0] = 'S';
				else if (bans->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
					type[0] = 'z';
				type[1] = '\0';
				sendto_one(sptr, rpl_str(RPL_STATSKLINE),
				    me.name, parv[0], type, bans->mask, bans->reason ? bans->reason : "<no reason>");
			  }

		  }
		  for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
			  if (excepts->flag.type == 1)
				 sendto_one(sptr, rpl_str(RPL_STATSKLINE),
				     me.name, parv[0], "E", excepts->mask, "");
		  }
		  break;
	  }
	  case 'm':
		LockEventSystem();
		EventStatus(sptr);
		UnlockEventSystem();
		break;

	  case 'M':
		  for (i = 0; i <= 255; i++)
			  for (mptr = CommandHash[i]; mptr; mptr = mptr->next)
				  if (mptr->count)
#ifndef DEBUGMODE
					  sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
					      me.name, parv[0], mptr->cmd,
					      mptr->count, mptr->bytes);
#else
					  sendto_one(sptr, rpl_str(RPL_STATSCOMMANDS),
					      me.name, parv[0], mptr->cmd,
					      mptr->count, mptr->bytes,
					      mptr->lticks,
					      mptr->lticks / CLOCKS_PER_SEC,
					      mptr->rticks,
					      mptr->rticks / CLOCKS_PER_SEC);
#endif
		  break;
	  case 'n':
	  {
		  ConfigItem_ban *bans;

		  for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) {
			  if (bans->flag.type == CONF_BAN_REALNAME)
				sendto_one(sptr, rpl_str(RPL_STATSNLINE),
				    me.name, parv[0], bans->mask, bans->reason ? bans->reason : "<no reason>");
		  }
		  break;
	  }
	  case 'N':
		  if (IsOper(sptr))
			  report_network(sptr);
		  break;
	  case 'o':
	  case 'O':
		  for (oper_p = conf_oper; oper_p; oper_p = (ConfigItem_oper *) oper_p->next)
		  {
		  	if (!oper_p->from)
		  		sendto_one(sptr, rpl_str(RPL_STATSOLINE),
		  			me.name, sptr->name, 
		  			'O', "(none)", oper_p->name,
		  			oflagstr(oper_p->oflags),
		  			oper_p->class->name ? oper_p->class->name : "");
			else
				for (oper_p_from = (ConfigItem_oper_from *) oper_p->from; oper_p_from; oper_p_from = (ConfigItem_oper_from *) oper_p_from->next)
			  		sendto_one(sptr, rpl_str(RPL_STATSOLINE),
			  			me.name, sptr->name, 
			  			'O', oper_p_from->name, oper_p->name,
			  			oflagstr(oper_p->oflags),
			  			oper_p->class->name? oper_p->class->name : "");
		  }
		  break;
	  case 'P':
	  	  if (IsOper(sptr))
	  	  {
			  for (i = 0; i <= LastSlot; i++)
			  {
			    if (!(acptr = local[i]))
					continue;
	  	  	  	if (!IsListening(acptr))
	  	  	  		continue;
	  	  	  	sendto_one(sptr, ":%s %s %s :*** Listener on %s:%i, clients %i. is %s",
	  	  	  		me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name,
	  	  	  		((ConfigItem_listen *)acptr->class)->ip,
	  	  	  		((ConfigItem_listen *)acptr->class)->port,
	  	  	  		((ConfigItem_listen *)acptr->class)->clients,
	  	  	   		((ConfigItem_listen *)acptr->class)->flag.temporary ? "TEMPORARY" : "PERM");
	  	  	  }
	  	  }
	  	  break;
	  case 'Q':
	  {
		  ConfigItem_ban *bans;

		  for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) {
			  if (bans->flag.type == CONF_BAN_NICK && (bans->flag.type2 != CONF_BAN_TYPE_AKILL))
				sendto_one(sptr, rpl_str(RPL_STATSQLINE),
				    me.name, parv[0],  bans->reason, bans->mask);
		  }
	  }
		  break;
	  case 'q':
	  {
		  ConfigItem_ban *bans;

		  for (bans = conf_ban; bans; bans = (ConfigItem_ban *)bans->next) {
			  if (bans->flag.type == CONF_BAN_NICK && (bans->flag.type2 == CONF_BAN_TYPE_AKILL))
				sendto_one(sptr, rpl_str(RPL_SQLINE_NICK),
				    me.name, parv[0], bans->mask, bans->reason ? bans->reason : "No Reason");
		  }
		  break;
	  }
	  case 'R':
#ifdef DEBUGMODE
		  send_usage(sptr, parv[0]);
#endif
		  break;
	  case 's':
		  if (IsOper(sptr))
		  {
			  sendto_one(sptr, ":%s %s %s :*** SCACHE:",
			      me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name);
			  list_scache(sptr);
			  sendto_one(sptr, ":%s %s %s :*** NS:", me.name,
			      IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name);
			  ns_stats(sptr);
		  }
		  break;
	  case 'S':
		  if (IsOper(sptr)) {
			  report_dynconf(sptr);
			  RunHook2(HOOKTYPE_STATS, sptr, "S");
		  }
		  break;
	  case 'D':
	  {
		  ConfigItem_deny_link *links;

		  for (links = conf_deny_link; links; links = (ConfigItem_deny_link *) links->next) {
			  if (links->flag.type == CRULE_ALL)
				  sendto_one(sptr, rpl_str(RPL_STATSDLINE), me.name, sptr->name,
					  "D", links->mask, links->prettyrule);
		  }
		  break;
	  }
	  case 'd':
	  {
		  ConfigItem_deny_link *links;

		  for (links = conf_deny_link; links; links = (ConfigItem_deny_link *) links->next) {
			  if (links->flag.type == CRULE_AUTO)
				  sendto_one(sptr, rpl_str(RPL_STATSDLINE), me.name, sptr->name,
					  "d", links->mask, links->prettyrule);
		  }
		  break;
	  }
	  case 'r': {
		  ConfigItem_deny_channel *dchans;
		  ConfigItem_allow_channel *achans;
		  for (dchans = conf_deny_channel; dchans; dchans = (ConfigItem_deny_channel *) dchans->next) {
			sendto_one(sptr, ":%s %i %s :deny %s %s", me.name, RPL_TEXT, sptr->name,
			    dchans->channel, dchans->reason);
		  }
  		  for (achans = conf_allow_channel; achans; achans = (ConfigItem_allow_channel *) achans->next) {
			sendto_one(sptr, ":%s %i %s :allow %s", me.name, RPL_TEXT, sptr->name,
			    achans->channel);
		  }
		  break;
	  }
	  case 't':
	  {
		  ConfigItem_tld *tld;
		  for (tld = conf_tld; tld; tld = (ConfigItem_tld *) tld->next)
		  {
			  sendto_one(sptr, rpl_str(RPL_STATSTLINE), me.name,
			      parv[0], tld->mask, tld->motd_file,
			      tld->rules_file ? tld->rules_file : "none");
		  }
		  break;
	  }
	  case 'T':		/* /stats T not t:lines .. */
		  tstats(sptr, parv[0]);
		  break;
	  case 'U':
	  {
		  ConfigItem_ulines *ulines;
		  for (ulines = conf_ulines; ulines;
		      ulines = (ConfigItem_ulines *) ulines->next)
		  {
			  sendto_one(sptr, rpl_str(RPL_STATSULINE), me.name,
			      parv[0], ulines->servername);
		  }
		  break;
	  }
	  case 'u':
	  {
		  time_t tmpnow;

		  tmpnow = TStime() - me.since;
		  sendto_one(sptr, rpl_str(RPL_STATSUPTIME), me.name, parv[0],
		      tmpnow / 86400, (tmpnow / 3600) % 24, (tmpnow / 60) % 60,
		      tmpnow % 60);
		  sendto_one(sptr, rpl_str(RPL_STATSCONN), me.name, parv[0],
		      max_connection_count, IRCstats.me_max);
		  break;
	  }
	  case 'v':
	  {
		  ConfigItem_deny_version *versions;

		  for (versions = conf_deny_version; versions; versions = (ConfigItem_deny_version *) versions->next) {
			  sendto_one(sptr, rpl_str(RPL_STATSVLINE), me.name, sptr->name,
				  versions->version, versions->flags, versions->mask);
		  }
		  break;
	  }
	  case 'V': {
		  ConfigItem_vhost *vhosts;
		  for(vhosts = conf_vhost; vhosts; vhosts = (ConfigItem_vhost *) vhosts->next) {
			for (oper_p_from = (ConfigItem_oper_from *)vhosts->from; oper_p_from; oper_p_from = (ConfigItem_oper_from *)oper_p_from->next) {
				sendto_one(sptr, ":%s %i %s :vhost %s%s%s %s %s", me.name, RPL_TEXT, sptr->name,
				     vhosts->virtuser ? vhosts->virtuser : "", vhosts->virtuser ? "@" : "",
				     vhosts->virthost, vhosts->login, oper_p_from->name);
			}
		  }
		  break;
	  }
	  case 'X':
	  case 'x':
		  for (link_p = conf_link; link_p; link_p = (ConfigItem_link *) link_p->next)
		  {
			if (!find_server_quick(link_p->servername))
			{
				sendto_one(sptr, rpl_str(RPL_STATSXLINE),
					me.name, sptr->name, link_p->servername,
					link_p->port);
			}
		  }
		  break;
	  case 'Y':
	  case 'y':
	  {
		  ConfigItem_class *classes;
		  for (classes = conf_class; classes; classes = (ConfigItem_class *) classes->next) {
			  sendto_one(sptr, rpl_str(RPL_STATSYLINE),
				me.name, sptr->name, classes->name, classes->pingfreq, classes->connfreq,
				classes->maxclients, classes->sendq, classes->recvq ? classes->recvq : CLIENT_FLOOD);
		  }
		  break;
	  }
	  case 'Z':
	  case 'z':
		  if (!strcasecmp(parv[1], "zip"))
		  {
			/* Ugly, but I want a '/stats zip' -- Syzop */
			if (!IsAnOper(sptr)) {
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
				return 0;
			}
#ifndef ZIP_LINKS
			sendto_one(sptr, ":%s %i %s :Sorry this server doesn't support zip links", me.name, RPL_TEXT, parv[0]);
#else
			for (i=0; i <= LastSlot; i++)
			{
				if (!(acptr = local[i]))
					continue;
				if (!IsServer(acptr) || !IsZipped(acptr))
					continue;
				if (acptr->zip->in->total_out && acptr->zip->out->total_in)
				{
				  sendto_one(sptr,
				    ":%s %i %s :Zipstats for link to %s (compresslevel %d): decompressed (in): %01lu/%01lu (%3.1f%%), compressed (out): %01lu/%01lu (%3.1f%%)",
				    me.name, RPL_TEXT, parv[0], get_client_name(acptr, TRUE),
				    acptr->serv->conf->compression_level ? acptr->serv->conf->compression_level : ZIP_DEFAULT_LEVEL,
				    acptr->zip->in->total_in, acptr->zip->in->total_out,
				    (100.0*(float)acptr->zip->in->total_in) /(float)acptr->zip->in->total_out,
				    acptr->zip->out->total_in, acptr->zip->out->total_out,
				    (100.0*(float)acptr->zip->out->total_out) /(float)acptr->zip->out->total_in);
				} else {
					sendto_one(sptr, ":%s %i %s :Zipstats for link to %s: unavailable", me.name, RPL_TEXT, parv[0]);
				}
			}
#endif
			sendto_snomask(SNO_EYES, "Stats 'zip' requested by %s (%s@%s)",
		    	sptr->name, sptr->user->username, GetHost(sptr));
			stat = '*';
			break;
		  } /* 'zip' */
		  if (IsAnOper(sptr))
			  count_memory(sptr, parv[0]);
		  break;
	  default:
		  /* Display a help menu */
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "/Stats flags:");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "b - Send the badwords list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "C - Send the link block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "d - Send the deny link (auto) block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "D - Send the deny link (all) block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "e - Send the except socks block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "E - Send the except ban block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "F - Send the deny dcc block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "G - Report TKL information (G:lines/Shuns)");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "H - Send the link block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "I - Send the allow block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "K - Send the ban user/ban ip/except ban block list (Includes AKILLs)");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "L - Send Link information");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "m - Send the events list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "M - Send list of how many times each command was used");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "n - Send the ban realname block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "N - Send network configuration list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "O - Send the oper block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "P - Send information about ports");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "q - Send the SQLINE list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "Q - Send the ban nick block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "r - Send the channel deny/allow block list");
#ifdef DEBUGMODE
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "R - Send the usage list");
#endif
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "s - Send the SCache and NS list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "S - Send the dynamic configuration list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "t - Send the tld block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "T - Send connection information");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "u - Send server uptime and connection count");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "U - Send the ulines block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "v - Send the deny version block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "V - Send the vhost block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "X - Send a list of servers that are not currently linked");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "Y - Send the class block list");
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "Z - Send memory usage information");
#ifdef ZIP_LINKS
		  sendto_one(sptr, rpl_str(RPL_STATSHELP), me.name, parv[0],
		      "zip - Send compression information about ziplinked servers");
#endif
		  stat = '*';
		  break;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat);


	if (stat != '*')
		sendto_snomask(SNO_EYES, "Stats \'%c\' requested by %s (%s@%s)",
		    stat, sptr->name, sptr->user->username, GetHost(sptr));

	return 0;
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




/*
** m_help (help/write to +h currently online) -Donwulff
**	parv[0] = sender prefix
**	parv[1] = optional message text
*/
CMD_FUNC(m_help)
{
	char *message, *s;
	Link *tmpl;


	message = parc > 1 ? parv[1] : NULL;

/* Drags along from wallops code... I'm not sure what it's supposed to do,
   at least it won't do that gracefully, whatever it is it does - but
   checking whether or not it's a person _is_ good... -Donwulff */

	if (!IsServer(sptr) && MyConnect(sptr) && !IsPerson(sptr))
	{
	}

	if (IsServer(sptr) || IsHelpOp(sptr))
	{
		if (BadPtr(message)) {
			if (MyClient(sptr)) {
				parse_help(sptr, parv[0], NULL);
				sendto_one(sptr,
					":%s NOTICE %s :*** NOTE: As a helpop you have to prefix your text with ? to query the help system, like: /helpop ?usercmds",
					me.name, sptr->name);
			}
			return 0;
		}
		if (message[0] == '?')
		{
			parse_help(sptr, parv[0], message + 1);
			return 0;
		}
		if (!myncmp(message, "IGNORE ", 7))
		{
			tmpl = make_link();
			DupString(tmpl->value.cp, message + 7);
			tmpl->next = helpign;
			helpign = tmpl;
			return 0;
		}
		if (message[0] == '!')
			message++;
		if (BadPtr(message))
			return 0;
		sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
		    parv[0], MSG_HELP, TOK_HELP, "%s", message);
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s (HelpOp): %s",
		    parv[0], message);
	}
	else if (MyConnect(sptr))
	{
		/* New syntax: ?... never goes out, !... always does. */
		if (BadPtr(message)) {
			parse_help(sptr, parv[0], NULL);
			return 0;
		}
		else if (message[0] == '?') {
			parse_help(sptr, parv[0], message+1);
			return 0;
		}
		else if (message[0] == '!') {
			message++;
		}
		else {
			if (parse_help(sptr, parv[0], message))
				return 0;
		}
		if (BadPtr(message))
			return 0;
		s = make_nick_user_host(cptr->name, cptr->user->username,
		    cptr->user->realhost);
		for (tmpl = helpign; tmpl; tmpl = tmpl->next)
			if (match(tmpl->value.cp, s) == 0)
			{
				sendto_one(sptr, rpl_str(RPL_HELPIGN), me.name,
				    parv[0]);
				return 0;
			}

		sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
		    parv[0], MSG_HELP, TOK_HELP, "%s", message);
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s (Local): %s",
		    parv[0], message);
		sendto_one(sptr, rpl_str(RPL_HELPFWD), me.name, parv[0]);
	}
	else
	{
		if (BadPtr(message))
			return 0;
		sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
		    parv[0], MSG_HELP, TOK_HELP, "%s", message);
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s: %s", parv[0],
		    message);
	}

	return 0;
}

/*
 * parv[0] = sender
 * parv[1] = server to query
 */
CMD_FUNC(m_lusers)
{
	if (hunt_server_token(cptr, sptr, MSG_LUSERS, TOK_LUSERS, ":%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;
	/* Just to correct results ---Stskeeps */
	if (IRCstats.clients > IRCstats.global_max)
		IRCstats.global_max = IRCstats.clients;
	if (IRCstats.me_clients > IRCstats.me_max)
		IRCstats.me_max = IRCstats.me_clients;

	sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
	    IRCstats.clients - IRCstats.invisible, IRCstats.invisible,
	    IRCstats.servers);

	if (IRCstats.operators)
		sendto_one(sptr, rpl_str(RPL_LUSEROP),
		    me.name, parv[0], IRCstats.operators);
	if (IRCstats.unknown)
		sendto_one(sptr, rpl_str(RPL_LUSERUNKNOWN),
		    me.name, parv[0], IRCstats.unknown);
	if (IRCstats.channels)
		sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS),
		    me.name, parv[0], IRCstats.channels);
	sendto_one(sptr, rpl_str(RPL_LUSERME),
	    me.name, parv[0], IRCstats.me_clients, IRCstats.me_servers);
	sendto_one(sptr, rpl_str(RPL_LOCALUSERS),
	    me.name, parv[0], IRCstats.me_clients, IRCstats.me_max);
	sendto_one(sptr, rpl_str(RPL_GLOBALUSERS),
	    me.name, parv[0], IRCstats.clients, IRCstats.global_max);
	if ((IRCstats.me_clients + IRCstats.me_servers) > max_connection_count)
	{
		max_connection_count =
		    IRCstats.me_clients + IRCstats.me_servers;
		if (max_connection_count % 10 == 0)	/* only send on even tens */
			sendto_ops("Maximum connections: %d (%d clients)",
			    max_connection_count, IRCstats.me_clients);
	}
	return 0;
}


EVENT(save_tunefile)
{
	FILE *tunefile;

	tunefile = fopen(IRCDTUNE, "w");
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

	tunefile = fopen(IRCDTUNE, "r");
	if (!tunefile)
		return;
	fprintf(stderr, "* Loading tunefile..\n");
	fgets(buf, 1023, tunefile);
	TSoffset = atol(buf);
	fgets(buf, 1023, tunefile);
	IRCstats.me_max = atol(buf);
	fclose(tunefile);
}
/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************//*
   ** m_connect
   **  parv[0] = sender prefix
   **  parv[1] = servername
   **  parv[2] = port number
   **  parv[3] = remote server
 */
CMD_FUNC(m_connect)
{
	int  port, tmpport, retval;
	ConfigItem_link	*aconf;
	ConfigItem_deny_link *deny;
	aClient *acptr;


	if (!IsPrivileged(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	if (MyClient(sptr) && !OPCanGRoute(sptr) && parc > 3)
	{			/* Only allow LocOps to make */
		/* local CONNECTS --SRB      */
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (MyClient(sptr) && !OPCanLRoute(sptr) && parc <= 3)
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (hunt_server_token(cptr, sptr, MSG_CONNECT, TOK_CONNECT, "%s %s :%s",
	    3, parc, parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "CONNECT");
		return -1;
	}

	if ((acptr = find_server_quick(parv[1])))
	{
		sendto_one(sptr, ":%s %s %s :*** Connect: Server %s %s %s.",
		    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], parv[1], "already exists from",
		    acptr->from->name);
		return 0;
	}

	for (aconf = conf_link; aconf; aconf = (ConfigItem_link *) aconf->next)
		if (!match(parv[1], aconf->servername))
			break;

	/* Checked first servernames, then try hostnames. */

	if (!aconf)
	{
		sendto_one(sptr,
		    ":%s %s %s :*** Connect: Server %s is not configured for linking", me.name,
		    IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], parv[1]);
		return 0;
	}
	/*
	   ** Get port number from user, if given. If not specified,
	   ** use the default form configuration structure. If missing
	   ** from there, then use the precompiled default.
	 */
	tmpport = port = aconf->port;
	if (parc > 2 && !BadPtr(parv[2]))
	{
		if ((port = atoi(parv[2])) <= 0)
		{
			sendto_one(sptr,
			    ":%s %s %s :*** Connect: Illegal port number", me.name,
			    IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0]);
			return 0;
		}
	}
	else if (port <= 0 && (port = PORTNUM) <= 0)
	{
		sendto_one(sptr, ":%s %s %s :*** Connect: missing port number",
		    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0]);
		return 0;
	}

/* Evaluate deny link */
	for (deny = conf_deny_link; deny; deny = (ConfigItem_deny_link *) deny->next) {
		if (deny->flag.type == CRULE_ALL && !match(deny->mask, aconf->servername)
			&& crule_eval(deny->rule)) {
			sendto_one(sptr,
				":%s %s %s :Connect: Disallowed by connection rule",
				me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0]);
			return 0;
		}
	}
	/*
	   ** Notify all operators about remote connect requests
	 */
	if (!IsAnOper(cptr))
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Remote CONNECT %s %s from %s",
		    me.name, parv[1], parv[2] ? parv[2] : "",
		    get_client_name(sptr, FALSE));
	}
	/* Interesting */
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	  case 0:
		  sendto_one(sptr,
		      ":%s %s %s :*** Connecting to %s[%s].",
		      me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], aconf->servername, aconf->hostname);
		  break;
	  case -1:
		  sendto_one(sptr, ":%s %s %s :*** Couldn't connect to %s.",
		      me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], aconf->servername);
		  break;
	  case -2:
		  sendto_one(sptr, ":%s %s %s :*** Resolving hostname '%s'...",
		      me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], aconf->hostname);
		  break;
	  default:
		  sendto_one(sptr,
		      ":%s %s %s :*** Connection to %s failed: %s",
		      me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], aconf->servername, strerror(retval));
	}
	aconf->port = tmpport;
	return 0;
}

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
CMD_FUNC(m_wallops)
{
	char *message;
	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "WALLOPS");
		return 0;
	}
	if (MyClient(sptr) && !OPCanWallOps(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
	    ":%s WALLOPS :%s", parv[0], message);
	return 0;
}


/* m_gnotice  (Russell) sort of like wallop, but only to +g clients on
** this server.
**	parv[0] = sender prefix
**	parv[1] = message text
*/
CMD_FUNC(m_gnotice)
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "GNOTICE");
		return 0;
	}
	if (!IsServer(sptr) && MyConnect(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
	    MSG_GNOTICE, TOK_GNOTICE, ":%s", message);
	sendto_failops("from %s: %s", parv[0], message);
	return 0;
}

/*
** m_addline (write a line to unrealircd.conf)
**
** De-Potvinized by codemastr
*/
CMD_FUNC(m_addline)
{
	FILE *conf;
	char *text;
	text = parc > 1 ? parv[1] : NULL;

	if (!(IsAdmin(sptr) || IsCoAdmin(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADDLINE");
		return 0;
	}
	/* writes to current -f */
	conf = fopen(configfile, "a");
	if (conf == NULL)
	{
		return 0;
	}
	/* Display what they wrote too */
	sendto_one(sptr, ":%s %s %s :*** Wrote (%s) to %s",
	    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], text, configfile);
	fprintf(conf, "// Added by %s\n", make_nick_user_host(sptr->name,
	    sptr->user->username, sptr->user->realhost));
/*	for (i=1 ; i<parc ; i++)
	{
		if (i!=parc-1)
			fprintf (conf,"%s ",parv[i]);
		else
			fprintf (conf,"%s\n",parv[i]);
	}
	 * I dunno what Potvin was smoking when he made this code, but it plain SUX
	 * this should work just as good, and no need for a loop -- codemastr */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}

/*
** m_addmotd (write a line to ircd.motd)
**
** De-Potvinized by codemastr
*/
CMD_FUNC(m_addmotd)
{
	FILE *conf;
	char *text;

	text = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(sptr))
		return 0;

	if (!(IsAdmin(sptr) || IsCoAdmin(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADDMOTD");
		return 0;
	}
	conf = fopen(MOTD, "a");
	if (conf == NULL)
	{
		return 0;
	}
	sendto_one(sptr, ":%s %s %s :*** Wrote (%s) to file: ircd.motd",
	    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], text);
	/*      for (i=1 ; i<parc ; i++)
	   {
	   if (i!=parc-1)
	   fprintf (conf,"%s ",parv[i]);
	   else
	   fprintf (conf,"%s\n",parv[i]);
	   } */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}


/*
** m_addomotd (write a line to opermotd)
**
** De-Potvinized by codemastr
*/
CMD_FUNC(m_addomotd)
{
	FILE *conf;
	char *text;

	text = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(sptr))
		return 0;

	if (!(IsAdmin(sptr) || IsCoAdmin(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADDMOTD");
		return 0;
	}
	conf = fopen(OPATH, "a");
	if (conf == NULL)
	{
		return 0;
	}
	sendto_one(sptr, ":%s %s %s :*** Wrote (%s) to OperMotd",
	    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], text);
	/*      for (i=1 ; i<parc ; i++)
	   {
	   if (i!=parc-1)
	   fprintf (conf,"%s ",parv[i]);
	   else
	   fprintf (conf,"%s\n",parv[i]);
	   } */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}


/*
** m_globops (write to opers who are +g currently online)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
CMD_FUNC(m_globops)
{
	char *message;

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "GLOBOPS");
		return 0;
	}
	if (MyClient(sptr) && !OPCanGlobOps(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
	    parv[0], MSG_GLOBOPS, TOK_GLOBOPS, ":%s", message);
	sendto_failops_whoare_opers("from %s: %s", parv[0], message);
	return 0;
}

/*
** m_locops (write to opers who are +g currently online *this* server)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
CMD_FUNC(m_locops)
{
	char *message;

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LOCOPS");
		return 0;
	}
	if (MyClient(sptr) && !OPCanLocOps(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_locfailops("from %s: %s", parv[0], message);
	return 0;
}

/*
** m_chatops (write to opers who are currently online)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
CMD_FUNC(m_chatops)
{
	char *message;

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "CHATOPS");
		return 0;
	}

	if (MyClient(sptr) && !IsAnOper(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
	    parv[0], MSG_CHATOPS, TOK_CHATOPS, ":%s", message);
		sendto_umode(UMODE_OPER, "*** ChatOps -- from %s: %s",
		    parv[0], message);
		sendto_umode(UMODE_LOCOP, "*** ChatOps -- from %s: %s",
		    parv[0], message);
	return 0;
}


/* m_goper  (Russell) sort of like wallop, but only to ALL +o clients on
** every server.
**      parv[0] = sender prefix
**      parv[1] = message text
*/
CMD_FUNC(m_goper)
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "GOPER");
		return 0;
	}
/*      if (!IsServer(sptr) && MyConnect(sptr) && !IsAnOper(sptr))*/
	if (!IsServer(sptr) || !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
	    parv[0], MSG_GOPER, TOK_GOPER, ":%s", message);
	sendto_opers("from %s: %s", parv[0], message);
	return 0;
}
/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
CMD_FUNC(m_time)
{
	if (hunt_server_token(cptr, sptr, MSG_TIME, TOK_TIME, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_TIME), me.name, parv[0], me.name,
		    date((long)0));
	return 0;
}

/*
** m_svskill
**	parv[0] = servername
**	parv[1] = client
**	parv[2] = kill message
*/
CMD_FUNC(m_svskill)
{
	aClient *acptr;
	/* this is very wierd ? */
	char *comment = NULL;


	if (parc < 2)
		return -1;
	if (parc > 3)
		return -1;
	if (parc == 3)
		comment = parv[2];

	if (parc == 2)
		comment = "SVS Killed";

	if (!IsULine(sptr))
		return -1;


	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	sendto_serv_butone_token(cptr, parv[0],
	    MSG_SVSKILL, TOK_SVSKILL, "%s :%s", parv[1], comment);

	return exit_client(cptr, acptr, sptr, comment);

}

/*
** m_admin
**	parv[0] = sender prefix
**	parv[1] = servername
*/
CMD_FUNC(m_admin)
{
	ConfigItem_admin *admin;
	/* Users may want to get the address in case k-lined, etc. -- Barubary

	   * Only allow remote ADMINs if registered -- Barubary */
	if (IsPerson(sptr) || IsServer(cptr))
		if (hunt_server_token(cptr, sptr, MSG_ADMIN, TOK_ADMIN, ":%s", 1, parc,
		    parv) != HUNTED_ISME)
			return 0;

	if (!conf_admin_tail)
	{
		sendto_one(sptr, err_str(ERR_NOADMININFO),
		    me.name, parv[0], me.name);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_ADMINME), me.name, parv[0], me.name);

	/* cycle through the list backwards */
	for (admin = conf_admin_tail; admin;
	    admin = (ConfigItem_admin *) admin->prev)
	{
		if (!admin->next)
			sendto_one(sptr, rpl_str(RPL_ADMINLOC1),
			    me.name, parv[0], admin->line);
		else if (!admin->next->next)
			sendto_one(sptr, rpl_str(RPL_ADMINLOC2),
			    me.name, parv[0], admin->line);
		else
			sendto_one(sptr, rpl_str(RPL_ADMINEMAIL),
			    me.name, parv[0], admin->line);
	}
	return 0;
}

/** Rehash motd and rule files (MPATH/RPATH and all tld entries). */
void rehash_motdrules()
{
ConfigItem_tld *tlds;

	motd = (aMotd *) read_file_ex(MPATH, &motd, &motd_tm);
	rules = (aMotd *) read_file(RPATH, &rules);
	smotd = (aMotd *) read_file_ex(SMPATH, &smotd, &smotd_tm);
	for (tlds = conf_tld; tlds; tlds = (ConfigItem_tld *) tlds->next)
	{
		tlds->motd = read_file_ex(tlds->motd_file, &tlds->motd, &tlds->motd_tm);
		tlds->rules = read_file(tlds->rules_file, &tlds->rules);
		if (tlds->smotd_file)
			tlds->smotd = read_file_ex(tlds->smotd_file, &tlds->smotd, &tlds->smotd_tm);
	}
}

/*
** m_rehash
** remote rehash by binary
** now allows the -flags in remote rehash
** ugly code but it seems to work :) -- codemastr
** added -all and fixed up a few lines -- niquil (niquil@programmer.net)
** fixed remote rehashing, but it's getting a bit weird code again -- Syzop
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

	if (BadPtr(parv[2])) {
		/* If the argument starts with a '-' (like -motd, -opermotd, etc) then it's
		 * assumed not to be a server. -- Syzop
		 */
		if (parv[1] && (parv[1][0] == '-'))
			x = HUNTED_ISME;
		else
			x = hunt_server_token(cptr, sptr, MSG_REHASH, TOK_REHASH, "%s", 1, parc, parv);
	} else {
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
			sendto_serv_butone(&me,
			    ":%s GLOBOPS :%s is remotely rehashing server config file",
			    me.name, sptr->name);
			sendto_ops
			    ("%s is remotely rehashing server config file",
			    parv[0]);
			return rehash(cptr, sptr,
			    (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
		}
		parv[1] = parv[2];
	}

	if (!BadPtr(parv[1]))
	{

		if (!IsAdmin(sptr) && !IsCoAdmin(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return 0;
		}

		if (*parv[1] == '-')
		{
			if (!_match("-all", parv[1]))
			{
				sendto_ops("%sRehashing everything on the request of %s",
					cptr != sptr ? "Remotely " : "",sptr->name);
				if (cptr != sptr)
					sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing everything", me.name, sptr->name);
				opermotd = (aMotd *) read_file(OPATH, &opermotd);
				botmotd = (aMotd *) read_file(BPATH, &botmotd);
				rehash_motdrules();
				return 0;
			}
			if (!strnicmp("-gar", parv[1], 4))
			{
				loop.do_garbage_collect = 1;
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
				opermotd = (aMotd *) read_file(OPATH, &opermotd);
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
				botmotd = (aMotd *) read_file(BPATH, &botmotd);
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
				return 0;
			}
			/* didn't match / fall trough... should we continue?? */
			sendto_ops("%s is %srehashing server config file (unknown option)",
				sptr->name, cptr != sptr ? "Remotely " : "");
			if (cptr != sptr)
				sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing server config file (unknown option)",
					me.name, sptr->name);
		}
	}
	else
		sendto_ops("%s is rehashing server config file", parv[0]);

	/* Normal rehash, rehash main motd&rules too, just like the on in the tld block will :p */
	motd = (aMotd *) read_file_ex(MPATH, &motd, &motd_tm);
	rules = (aMotd *) read_file(RPATH, &rules);
	smotd = (aMotd *) read_file_ex(SMPATH, &smotd, &smotd_tm);
	if (cptr == sptr)
		sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
	return rehash(cptr, sptr, (parc > 1) ? ((*parv[1] == 'q') ? 2 : 0) : 0);
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
	int  x;
	if (MyClient(sptr) && !OPCanRestart(sptr))
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
	if (parc > 3)
	{
		/* Remote restart. */
		if (MyClient(sptr) && !IsNetAdmin(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}

		if ((x =
		    hunt_server_token(cptr, sptr, MSG_RESTART, TOK_RESTART, "%s %s :%s", 2, parc,
		    parv)) != HUNTED_ISME)
			return 0;
	}

	if (conf_drpass)
	{
		if (parc < 2)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name,
			    parv[0], "RESTART");
			return 0;
		}
		x = Auth_Check(cptr, conf_drpass->restartauth, parv[1]);
		if (x == -1)
		{
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
			    parv[0]);
			return 0;
		}
		if (x < 1)
		{
			return 0;
		}
		/* Hack to make the code after this if { } easier: we assign the comment to the
		 * first param, as if we had not had an X:line. We do not need the password
		 * now anyways. Accordingly we decrement parc ;)    -- NikB
		 */
		parv[1] = parv[2];
		parc--;
	}
	if (cptr != sptr)
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :%s is remotely restarting server (%s)",
		    me.name, sptr->name, parv[3]);
		sendto_ops("%s is remotely restarting IRCd (%s)", parv[0],
		    parv[3]);

	}

	sendto_ops("Server is Restarting by request of %s", parv[0]);
	server_reboot((!MyClient(sptr) ? (parc > 2 ? parv[3] : "No reason")
	    : (parc > 1 ? parv[2] : "No reason")));
	return 0;
}

/*
** m_trace
**	parv[0] = sender prefix
**	parv[1] = servername
*/
CMD_FUNC(m_trace)
{
	int  i;
	aClient *acptr;
	ConfigItem_class *cltmp;
	char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;


	if (parc > 2)
		if (hunt_server_token(cptr, sptr, MSG_TRACE, TOK_TRACE, "%s :%s", 2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	if (!IsOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	switch (hunt_server_token(cptr, sptr, MSG_TRACE, TOK_TRACE, ":%s", 1, parc, parv))
	{
	  case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
	  {
		  aClient *ac2ptr;

		  ac2ptr = next_client(client, tname);
		  sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, parv[0],
		      version, debugmode, tname, ac2ptr->from->name);
		  return 0;
	  }
	  case HUNTED_ISME:
		  break;
	  default:
		  return 0;
	}

	doall = (parv[1] && (parc > 1)) ? !match(tname, me.name) : TRUE;
	wilds = !parv[1] || index(tname, '*') || index(tname, '?');
	dow = wilds || doall;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;


	if (doall) {
		for (acptr = client; acptr; acptr = acptr->next)
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->slot]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || IsOper(sptr)))
				link_u[acptr->from->slot]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->slot]++;
	}

	/* report all direct connections */

	now = TStime();
	for (i = 0; i <= LastSlot; i++)
	{
		char *name;
		char *class;

		if (!(acptr = local[i]))	/* Local Connection? */
			continue;
/* More bits of code to allow oers to see all users on remote traces
 *		if (IsInvisible(acptr) && dow &&
 *		if (dow &&
 *		    !(MyConnect(sptr) && IsOper(sptr)) && */
		if (!IsOper(sptr) && !IsAnOper(acptr) && (acptr != sptr))
			continue;
		if (!doall && wilds && match(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr, FALSE);
		class = acptr->class ? acptr->class->name : "default";
		switch (acptr->status)
		{
		  case STAT_CONNECTING:
			  sendto_one(sptr, rpl_str(RPL_TRACECONNECTING),
			      me.name, parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_HANDSHAKE:
			  sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE), me.name,
			      parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_ME:
			  break;
		  case STAT_UNKNOWN:
			  sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
			      me.name, parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_CLIENT:
			  /* Only opers see users if there is a wildcard
			   * but anyone can see all the opers.
			   */
/*			if (IsOper(sptr) &&
 * Allow opers to see invisible users on a remote trace or wildcard
 * search  ... sure as hell  helps to find clonebots.  --Russell
 *			    (MyClient(sptr) || !(dow && IsInvisible(acptr)))
 *                           || !dow || IsAnOper(acptr)) */
			  if (IsOper(sptr) ||
			      (IsAnOper(acptr) && !IsInvisible(acptr)))
			  {
				  if (IsAnOper(acptr))
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEOPERATOR),
					      me.name,
					      parv[0], class, acptr->name,
					      GetHost(acptr),
					      now - acptr->lasttime);
				  else
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEUSER), me.name,
					      parv[0], class, acptr->name,
					      acptr->user->realhost,
					      now - acptr->lasttime);
				  cnt++;
			  }
			  break;
		  case STAT_SERVER:
			  if (acptr->serv->user)
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, parv[0], class, link_s[i],
				      link_u[i], name, acptr->serv->by,
				      acptr->serv->user->username,
				      acptr->serv->user->realhost,
				      now - acptr->lasttime);
			  else
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, parv[0], class, link_s[i],
				      link_u[i], name, *(acptr->serv->by) ?
				      acptr->serv->by : "*", "*", me.name,
				      now - acptr->lasttime);
			  cnt++;
			  break;
		  case STAT_LOG:
			  sendto_one(sptr, rpl_str(RPL_TRACELOG), me.name,
			      parv[0], LOGFILE, acptr->port);
			  cnt++;
			  break;
#ifdef USE_SSL
		  case STAT_SSL_CONNECT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 parv[0], "SSL-Connect-Handshake", name); 
			cnt++;
			break;
		  case STAT_SSL_ACCEPT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 parv[0], "SSL-Accept-Handshake", name); 
			cnt++;
			break;
#endif
		  default:	/* ...we actually shouldn't come here... --msa */
			  sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
			      parv[0], "<newtype>", name);
			  cnt++;
			  break;
		}
	}
	/*
	 * Add these lines to summarize the above which can get rather long
	 * and messy when done remotely - Avalon
	 */
	if (!IsAnOper(sptr) || !cnt)
	{
		if (cnt)
			return 0;
		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one(sptr, rpl_str(RPL_TRACESERVER),
		    me.name, parv[0], 0, link_s[me.slot],
		    link_u[me.slot], me.name, "*", "*", me.name);
		return 0;
	}
	for (cltmp = conf_class; doall && cltmp; cltmp = (ConfigItem_class *) cltmp->next)
	/*	if (cltmp->clients > 0) */
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
			    parv[0], cltmp->name ? cltmp->name : "[noname]", cltmp->clients);
	return 0;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
int short_motd(aClient *sptr) {
	ConfigItem_tld *ptr;
	aMotd *temp, *temp2;
	struct tm *tm = &smotd_tm;
	char userhost[HOSTLEN + USERLEN + 6];
	char is_short = 1;
	strlcpy(userhost,make_user_host(sptr->user->username, sptr->user->realhost), sizeof userhost);
	ptr = Find_tld(sptr, userhost);

	if (ptr)
	{
		if (ptr->smotd)
		{
			temp = ptr->smotd;
			tm = &ptr->smotd_tm;
		}
		else if (smotd)
			temp = smotd;
		else
		{
			temp = ptr->motd;
			tm = &ptr->motd_tm;
			is_short = 0;
		}
	}
	else
	{
		if (smotd)
			temp = smotd;
		else
		{
			temp = motd;
			tm = &motd_tm;
			is_short = 0;
		}
	}

	if (!temp)
	{
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, sptr->name);
		return 0;
	}
	if (tm)
	{
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

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, sptr->name);
	return 0;
}


/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
CMD_FUNC(m_motd)
{
	ConfigItem_tld *ptr;
	aMotd *temp, *temp2;
	struct tm *tm = &motd_tm;
	int  svsnofile = 0;
	char userhost[HOSTLEN + USERLEN + 6];

	if (IsServer(sptr))
		return 0;
	if (hunt_server_token(cptr, sptr, MSG_MOTD, TOK_MOTD, ":%s", 1, parc, parv) !=
HUNTED_ISME)
		return 0;
#ifndef TLINE_Remote
	if (!MyConnect(sptr))
	{
		temp = motd;
		goto playmotd;
	}
#endif
	strlcpy(userhost,make_user_host(cptr->user->username, cptr->user->realhost), sizeof userhost);
	ptr = Find_tld(sptr, userhost);

	if (ptr)
	{
		temp = ptr->motd;
		tm = &ptr->motd_tm;
	}
	else
		temp = motd;

      playmotd:
	if (temp == NULL)
	{
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv[0]);
		svsnofile = 1;
		goto svsmotd;

	}

	if (tm)
	{
		sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0],
		    me.name);
		sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d", me.name,
		    RPL_MOTD, parv[0], tm->tm_mday, tm->tm_mon + 1,
		    1900 + tm->tm_year, tm->tm_hour, tm->tm_min);
	}

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
		    temp->line);
		temp = temp->next;
	}
      svsmotd:
	temp2 = svsmotd;
	while (temp2)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
		    temp2->line);
		temp2 = temp2->next;
	}
	if (svsnofile == 0)
		sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	return 0;
}
/*
 * Modified from comstud by codemastr
 */
CMD_FUNC(m_opermotd)
{
	aMotd *temp;

	if (!IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (opermotd == (aMotd *) NULL)
	{
		sendto_one(sptr, err_str(ERR_NOOPERMOTD), me.name, parv[0]);
		return 0;
	}
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
	sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
	    "\2IRC Operator Message of the Day\2");

	temp = opermotd;
	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	return 0;
}

/*
 * A merge from ircu and bahamut, and some extra stuff added by codemastr
 * we can now use 1 function for multiple files -- codemastr
 * Merged read_motd/read_rules stuff into this -- Syzop
 */

/** Read motd-like file, used for rules/motd/botmotd/opermotd/etc.
 * @param filename Filename of file to read.
 * @param list Reference to motd pointer (used for freeing if needed, can be NULL)
 * @returns Pointer to MOTD or NULL if reading failed.
 */
aMotd *read_file(char *filename, aMotd **list)
{
	return read_file_ex(filename, list, NULL);
}

/** Read motd-like file, used for rules/motd/botmotd/opermotd/etc.
 * @param filename Filename of file to read.
 * @param list Reference to motd pointer (used for freeing if needed, NULL allowed)
 * @param t Pointer to struct tm to store filedatetime info in (NULL allowed)
 * @returns Pointer to MOTD or NULL if reading failed.
 */
aMotd *read_file_ex(char *filename, aMotd **list, struct tm *t)
{

	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	char line[82];
	char *tmp;
	int  i;

	if (fd == -1)
		return NULL;

	if (list)
	{
		while (*list)
		{
			old = (*list)->next;
			MyFree((*list)->line);
			MyFree(*list);
			*list  = old;
		}
	}

	if (t)
	{
		struct tm *ttmp;
		struct stat sb;
		if (!fstat(fd, &sb))
		{
			ttmp = localtime(&sb.st_mtime);
			memcpy(t, ttmp, sizeof(struct tm));
		} else {
			/* Sure, fstat() shouldn't fail, but... */
			memset(t, 0, sizeof(struct tm));
		}
	}

	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */

	newmotd = last = NULL;
	while ((i = dgets(fd, line, 81)) > 0)
	{
		line[i] = '\0';
		if ((tmp = (char *)strchr(line, '\n')))
			*tmp = '\0';
		if ((tmp = (char *)strchr(line, '\r')))
			*tmp = '\0';
		temp = (aMotd *) MyMalloc(sizeof(aMotd));
		if (!temp)
			outofmemory();
		AllocCpy(temp->line, line);
		temp->next = NULL;
		if (!newmotd)
			newmotd = temp;
		else
			last->next = temp;
		last = temp;
	}
	close(fd);
	return newmotd;

}

/*
 * Modified from comstud by codemastr
 */
CMD_FUNC(m_botmotd)
{
	aMotd *temp;
	if (hunt_server_token(cptr, sptr, MSG_BOTMOTD, TOK_BOTMOTD, ":%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;

	if (botmotd == (aMotd *) NULL)
	{
		sendto_one(sptr, ":%s NOTICE AUTH :BOTMOTD File not found",
		    me.name);
		return 0;
	}
	sendto_one(sptr, ":%s NOTICE AUTH :- %s Bot Message of the Day - ",
	    me.name, me.name);

	temp = botmotd;
	while (temp)
	{
		sendto_one(sptr, ":%s NOTICE AUTH :- %s", me.name, temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, ":%s NOTICE AUTH :End of /BOTMOTD command.", me.name);
	return 0;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
CMD_FUNC(m_rules)
{
	ConfigItem_tld *ptr;
	aMotd *temp;
	char userhost[USERLEN + HOSTLEN + 6];
	if (IsServer(sptr))
		return 0;
		
	if (hunt_server_token(cptr, sptr, MSG_RULES, TOK_RULES, ":%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;
#ifndef TLINE_Remote
	if (!MyConnect(sptr))
	{
		temp = rules;
		goto playrules;
	}
#endif
	strlcpy(userhost,make_user_host(cptr->user->username, cptr->user->realhost), sizeof userhost);
	ptr = Find_tld(sptr, userhost);

	if (ptr)
	{
		temp = ptr->rules;

	}
	else
		temp = rules;

      playrules:
	if (temp == NULL)
	{
		sendto_one(sptr, err_str(ERR_NORULES), me.name, parv[0]);
		return 0;

	}

	sendto_one(sptr, rpl_str(RPL_RULESSTART), me.name, parv[0], me.name);

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_RULES), me.name, parv[0],
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFRULES), me.name, parv[0]);
	return 0;
}

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
CMD_FUNC(m_close)
{
	aClient *acptr;
	int  i;
	int  closed = 0;


	if (!MyOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	for (i = LastSlot; i >= 0; --i)
	{
		if (!(acptr = local[i]))
			continue;
		if (!IsUnknown(acptr) && !IsConnecting(acptr) &&
		    !IsHandshake(acptr))
			continue;
		sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, parv[0],
		    get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, "Oper Closing");
		closed++;
	}
	sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, parv[0], closed);
	sendto_realops("%s!%s@%s closed %d unknown connections", sptr->name,
	    sptr->user->username, GetHost(sptr), closed);
	IRCstats.unknown = 0;
	return 0;
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

char servername[128][128];
int  server_usercount[128];
int  numservers = 0;

/*
 * New /MAP format -Potvin
 * dump_map function.
 */
void dump_map(aClient *cptr, aClient *server, char *mask, int prompt_length, int length)
{
	static char prompt[64];
	char *p = &prompt[prompt_length];
	int  cnt = 0;
	aClient *acptr;
	Link *lp;

	*p = '\0';

	if (prompt_length > 60)
		sendto_one(cptr, rpl_str(RPL_MAPMORE), me.name, cptr->name,
		    prompt, server->name);
	else
	{
		sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, prompt,
		    length, server->name, server->serv->users,
		    (server->serv->numeric ? (char *)my_itoa(server->serv->
		    numeric) : ""));
		cnt = 0;
	}

	if (prompt_length > 0)
	{
		p[-1] = ' ';
		if (p[-2] == '`')
			p[-2] = ' ';
	}
	if (prompt_length > 60)
		return;

	strcpy(p, "|-");


	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->srvptr != server ||
 		    (IsULine(acptr) && !IsOper(cptr) && HIDE_ULINES))
			continue;
		acptr->flags |= FLAGS_MAP;
		cnt++;
	}

	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (IsULine(acptr) && HIDE_ULINES && !IsOper(cptr))
			continue;
		if (acptr->srvptr != server)
			continue;
		if (!acptr->flags & FLAGS_MAP)
			continue;
		if (--cnt == 0)
			*p = '`';
		dump_map(cptr, acptr, mask, prompt_length + 2, length - 2);

	}

	if (prompt_length > 0)
		p[-1] = '-';
}

/*
** New /MAP format. -Potvin
** m_map (NEW)
**
**      parv[0] = sender prefix
**      parv[1] = server mask
**/
CMD_FUNC(m_map)
{
	Link *lp;
	aClient *acptr;
	int  longest = strlen(me.name);


	if (parc < 2)
		parv[1] = "*";
	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
	}
	if (longest > 60)
		longest = 60;
	longest += 2;
	dump_map(sptr, &me, "*", 0, longest);
	sendto_one(sptr, rpl_str(RPL_MAPEND), me.name, parv[0]);

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
	for (acptr = client, collapse(mask); acptr; acptr = acptr->next)
	{
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;
		if (!match(mask, acptr->name))
			break;
		continue;
	}
	return acptr;
}
