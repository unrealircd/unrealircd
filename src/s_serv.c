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

#ifndef lint
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
#include "userload.h"
#include "version.h"
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <time.h>
#include "h.h"
#include <string.h>


static char buf[BUFSIZE];

int  max_connection_count = 1, max_client_count = 1;
extern ircstats IRCstats;
extern int do_garbage_collect;
/* We need all these for cached MOTDs -- codemastr */

aMotd *opermotd;
aMotd *rules;
aMotd *motd;
aMotd *svsmotd;
aMotd *botmotd;
aTrecord *tdata;
struct tm *motd_tm;
aMotd *read_opermotd(char *filename);
aMotd *read_motd(char *filename);
aMotd *read_rules(char *filename);
aMotd *read_svsmotd(char *filename);
aMotd *read_botmotd(char *filename);
void read_tlines(void);

/*
** m_functions execute protocol messages on this server:
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
int  m_version(cptr, sptr, parc, parv)
	aClient *sptr, *cptr;
	int  parc;
	char *parv[];
{
	extern char serveropts[];
	char *x;

	if (TRUEHUB == 1)
		x = "(H)";
	else
		x = "";

	if (hunt_server(cptr, sptr, ":%s VERSION :%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		sendto_one(sptr, rpl_str(RPL_VERSION), me.name,
		    parv[0], version, ircnetwork, debugmode, me.name,
		    serveropts,
		    (IsAnOper(sptr) ? MYOSNAME : "*"), UnrealProtocol, x);
		if (MyClient(sptr))
		{
			sendto_one(sptr, rpl_str(RPL_PROTOCTL), me.name,
			    sptr->name, PROTOCTL_PARAMETERS);
		}
	}
	return 0;
}

/*int IsMe (acptr)
aClient *acptr, server;
{
if (memcmp (acptr, server, sizeof(aClient)) == 0)
return 1;
return 0;
 }*/


/*
** m_squit
**	parv[0] = sender prefix
**	parv[1] = server name
**	parv[parc-1] = comment
*/
int  m_squit(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aConfItem *aconf;
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
			   ** To accomodate host masking, a squit for a masked server
			   ** name is expanded if the incoming mask is the same as
			   ** the server name for that link to the name of link.
			 */
			while ((*server == '*') && IsServer(cptr))
			{
				aconf = cptr->serv->nline;
				if (!aconf)
					break;
				if (!mycmp(server, my_name_for_link(me.name, aconf)))
					server = cptr->name;
				break;	/* WARNING is normal here */
			}

			/*
			   ** The following allows wild cards in SQUIT. Only usefull
			   ** when the command is issued by an oper.
			 */
			for (acptr = client; (acptr = next_client(acptr, server));
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
			acptr = (aClient *) find_server_by_base64(server + 1);
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
#if defined(USE_SYSLOG) && defined(SYSLOG_SQUIT)
		syslog(LOG_DEBUG, "SQUIT From %s : %s (%s)",
		    parv[0], server, comment);
#endif
	}
	else if (MyConnect(acptr))
	{
		if (acptr->user)
		{
			sendto_one(sptr,
			    ":%s NOTICE :*** Cannot do fake kill by SQUIT !!!",
			    me.name);
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
int  m_protoctl(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
		strncpy(proto, parv[i], 127);
		proto[127] = '\0';	/* Just to be safe... */
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
				cptr->proto &=~ PROTO_SJB64;
				continue;
			}
#endif
			Debug((DEBUG_ERROR,
			    "Chose protocol %s for link %s",
			    proto, cptr->name));
			cptr->proto |= PROTO_SJB64;
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

char	*num = NULL;

/*
** m_server
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = serverinfo/hopcount
**      parv[3] = serverinfo
*/
int  m_server(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *ch;
	int  i;
	char info[REALLEN + 61], *inpath, *host, *encr;
	aClient *acptr, *bcptr, *ocptr;
	aConfItem *aconf, *cconf;
	int  hop, numeric = 0;
	char *flags = NULL, *protocol = NULL, *inf = NULL;

	info[0] = '\0';
	inpath = get_client_name(cptr, FALSE);
	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(cptr, "ERROR :No servername");
		return 0;
	}
	if ((cptr->acpt->umodes & LISTENER_CLIENTSONLY) && MyConnect(cptr)) {
		 return exit_client(cptr, sptr, sptr, "This port is for clients only");
	}
		
	hop = 0;
	host = parv[1];
	if (parc > 4)
	{
		numeric = atoi(parv[3]);
		hop = atoi(parv[2]);
		(void)strncpy(info, parv[4], REALLEN + 60);
		info[REALLEN] = '\0';
	}
	else if (parc > 3 && atoi(parv[2]))
	{
		hop = atoi(parv[2]);
		(void)strncpy(info, parv[3], REALLEN + 60);
		info[REALLEN] = '\0';
	}
/*
	We do not support "SERVER server :desc" anymore, this is an ugly hack 
	too
	
	else if (parc > 2)
	{
		(void)strncpy(info, parv[2], REALLEN + 60);
		if (parc > 3 && ((i = strlen(info)) < (REALLEN - 2)))
		{
			(void)strcat(info, " ");
			(void)strncat(info, parv[3], REALLEN - i - 2);
			info[REALLEN] = '\0';
		}
	}
*/
	/*
	   ** Check for "FRENCH " infection ;-) (actually this should
	   ** be replaced with routine to check the hostname syntax in
	   ** general). [ This check is still needed, even after the parse
	   ** is fixed, because someone can send "SERVER :foo bar " ].
	   ** Also, changed to check other "difficult" characters, now
	   ** that parse lets all through... --msa
	 */
	if (strlen(host) > HOSTLEN)
		host[HOSTLEN] = '\0';
	for (ch = host; *ch; ch++)
		if (*ch <= ' ' || *ch > '~')
			break;
	if (*ch || !index(host, '.'))
	{
		sendto_one(sptr, "ERROR :Bogus server name (%s)",
		    sptr->name, host);
		sendto_umode
		    (UMODE_JUNK,"WARNING: Bogus server name (%s) from %s (maybe just a fishy client)",
		    host, get_client_name(cptr, TRUE));

		sptr->since += 7;
		return 0;
	}

	if (IsPerson(cptr))
	{
		/*
		   ** A local link that has been identified as a USER
		   ** tries something fishy... ;-)
		 */
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		sendto_one(cptr,
		    ":%s NOTICE %s :Sorry, but your IRC program doesn't appear to support changing servers.",
		    me.name, cptr->name);
/*		sendto_ops("User %s trying to become a server %s",
			   get_client_name(cptr, TRUE),host);
			   */
		sptr->since += 7;
		return 0;
	}
	/* *WHEN* can it be that "cptr != sptr" ????? --msa */
	/* When SERVER command (like now) has prefix. -avalon */

	/* take a prepeek at the password.. */
	if (IsUnknown(cptr))
	{
		aconf = find_conf_servern(host);
		if (!aconf)
		{
			sendto_one(cptr,
			    "ERROR :No Access (No matching N:line) %s", inpath);
			sendto_locfailops
			    ("Access denied (No matching N:line) %s", inpath);
			return exit_client(cptr, cptr, cptr,
			    "No matching N:line");
		}
#ifdef CRYPT_LINK_PASSWORD
		/* use first two chars of the password they send in as salt */

		/* passwd may be NULL. Head it off at the pass... */
		if (*cptr->passwd)
		{
			char salt[3];
			extern char *crypt();

			salt[0] = aconf->passwd[0];
			salt[1] = aconf->passwd[1];
			salt[2] = '\0';
			encr = crypt(cptr->passwd, salt);
		}
		else
			encr = "";
#else
		encr = cptr->passwd;
#endif /* CRYPT_LINK_PASSWORD */
		if (*aconf->passwd && !StrEq(aconf->passwd, encr))
		{
			sendto_one(cptr,
			    "ERROR :No Access (passwd mismatch) %s", inpath);
			sendto_locfailops("Access denied (passwd mismatch) %s",
			    inpath);
			return exit_client(cptr, cptr, cptr, "Bad Password");
		}
		/* bzero(cptr->passwd, sizeof(cptr->passwd)); */
	}
	if ((acptr = find_server/*_quickx*/(host, NULL)))
	{
		/*
		 * This link is trying feed me a server that I already have
		 * access through another path -- multiple paths not accepted
		 * currently, kill this link immeatedly!!
		 */
		acptr = acptr->from;
                ocptr = (cptr->firsttime > acptr->firsttime) ? acptr : cptr;
                acptr = (cptr->firsttime > acptr->firsttime) ? cptr : acptr;
                sendto_one(acptr,"ERROR :Server %s already exists from %s",
                           host,
                           (ocptr->from ? ocptr->from->name : "<nobody>"));
                sendto_ops("Link %s cancelled, server %s already exists from %s",
                	get_client_name(acptr, TRUE), host,
                           (ocptr->from ? ocptr->from->name : "<nobody>"));
                return exit_client(acptr, acptr, acptr, "Server Exists");       
	}
/*	if ((acptr = find_client(host, NULL)))
	{
		sendto_one(cptr, "ERROR :Nickname %s already exists!", host);
		sendto_locfailops
		    ("Link %s cancelled: Server/nick collision on %s", inpath,
		    host);
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Link %s cancelled: Server/nick collision on %s",
		    parv[0], inpath, host);
		return exit_client(cptr, cptr, cptr, "Nick as Server");
	} */

	if (IsServer(cptr))
	{
		/*
		   ** Server is informing about a new server behind
		   ** this link. Create REMOTE server structure,
		   ** add it to list and propagate word to my other
		   ** server links...
		 */
		if (parc == 1 || info[0] == '\0')
		{
			sendto_one(cptr,
			    "ERROR :No server info specified for %s", host);
			return 0;
		}

		/*
		   ** See if the newly found server is behind a guaranteed
		   ** leaf (L-line). If so, close the link.
		 */
		if ((aconf = find_conf_host(cptr->confs, host, CONF_LEAF)) &&
		    (!aconf->port || (hop > aconf->port)))
		{
			sendto_ops("Leaf-only link %s->%s - Closing",
			    get_client_name(cptr, TRUE),
			    aconf->host ? aconf->host : "*");
			sendto_one(cptr, "ERROR :Leaf-only link, sorry.");
			return exit_client(cptr, cptr, cptr,
			    "Leaf Only (You are L:lined)");
		}
		/*
		   **
		 */
		if (!(aconf = find_conf_host(cptr->confs, host, CONF_HUB)) ||
		    (aconf->port && (hop > aconf->port)))
		{
			sendto_ops("Non-Hub link %s introduced %s(%s).",
			    get_client_name(cptr, TRUE), host,
			    aconf ? (aconf->host ? aconf->host : "*") : "!");
			return exit_client(cptr, cptr, cptr,
			    "Too many servers (Missing H:Line)");
		}
		/*
		   ** See if the newly found server has a Q line for it in
		   ** our conf. If it does, lose the link that brought it
		   ** into our network. Format:
		   **
		   ** Q:<unused>:<reason>:<servername>
		   **
		   ** Example:  Q:*:for the hell of it:eris.Berkeley.EDU
		 */
		if ((aconf = find_conf_name(host, CONF_QUARANTINED_SERVER)))
		{
			sendto_ops_butone(NULL, &me,
			    ":%s WALLOPS * :%s brought in %s, %s %s",
			    me.name, get_client_name(cptr, FALSE),
			    host, "closing link because",
			    BadPtr(aconf->passwd) ? "reason unspecified" :
			    aconf->passwd);

			sendto_one(cptr,
			    "ERROR :%s is not welcome: %s. %s",
			    host, BadPtr(aconf->passwd) ?
			    "reason unspecified" : aconf->passwd,
			    "Try another network");

			return exit_client(cptr, cptr, cptr, "Q-Lined Server");
		}

		if (numeric_collides(numeric))
		{
			return exit_client(cptr, cptr, cptr,
			    "Colliding server numeric (choose another in the M:line)");
		}

		acptr = make_client(cptr, find_server_quick(parv[0]));
		(void)make_server(acptr);
		acptr->serv->numeric = numeric;
		acptr->hopcount = hop;
		strncpyzt(acptr->name, host, sizeof(acptr->name));
		strncpyzt(acptr->info, info, sizeof(acptr->info));
		acptr->serv->up = find_or_add(parv[0]);
		SetServer(acptr);
		/* Taken from bahamut makes it so all servers behind a U:lined
		 * server are also U:lined, very helpful if HIDE_ULINES is on
		 */
		if (IsULine(sptr)
		    || (find_uline(cptr->confs, acptr->name)))
			acptr->flags |= FLAGS_ULINE;
		add_server_to_table(acptr);
		IRCstats.servers++;
		(void)find_or_add(acptr->name);
		acptr->flags |= FLAGS_TS8;

		add_client_to_list(acptr);
		(void)add_to_client_hash_table(acptr->name, acptr);
		/*
		   ** Old sendto_serv_but_one() call removed because we now
		   ** need to send different names to different servers
		   ** (domain name matching)
		 */
		for (i = 0; i <= highest_fd; i++)
		{
			if (!(bcptr = local[i]) || !IsServer(bcptr) ||
			    bcptr == cptr || IsMe(bcptr))
				continue;
			if (!(aconf = bcptr->serv->nline))
			{
				sendto_ops("Lost N-line for %s on %s. Closing",
				    get_client_name(cptr, TRUE), host);
				return exit_client(cptr, cptr, cptr,
				    "Lost N line");
			}
			if (match(my_name_for_link(me.name, aconf),
			    acptr->name) == 0)
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

	if (!IsUnknown(cptr) && !IsHandshake(cptr))
		return 0;
	/*
	   ** A local link that is still in undefined state wants
	   ** to be a SERVER. Check if this is allowed and change
	   ** status accordingly...
	 */
	strncpyzt(cptr->name, host, sizeof(cptr->name));
	/* For now, just strip the VL stuff if it's there */
	if (SupportVL(cptr))
	{
		/* we also have a fail safe incase they say they are sending
		 * VL stuff and don't -- codemastr
		 */
		aConfItem *vlines = NULL;
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
		if (inf)
		{
			strncpyzt(cptr->info, *inf ? inf : me.name,
			    sizeof(cptr->info));
			for (vlines = conf; vlines; vlines = vlines->next)
			{
				if ((vlines->status & CONF_VERSION)
				    && !match(vlines->name, cptr->name))
					break;
			}
			if (vlines)
			{
				char *proto = vlines->host;
				char *vflags = vlines->passwd;
				int  result = 0;
				int  i;
				protocol++;
				/* check the protocol */
				switch (*proto)
				{
				  case '<':
					  proto++;
					  if (atoi(protocol) < atoi(proto))
						  result = 1;
					  else
						  result = 0;
					  break;
				  case '>':
					  proto++;
					  if (atoi(protocol) > atoi(proto))
						  result = 1;
					  else
						  result = 0;
					  break;
				  case '=':
					  proto++;
					  if (atoi(protocol) == atoi(proto))
						  result = 1;
					  else
						  result = 0;
					  break;
				  case '!':
					  proto++;
					  if (atoi(protocol) != atoi(proto))
						  result = 1;
					  else
						  result = 0;
					  break;
					  /* default to = if anything else */
				  default:
					  if (atoi(protocol) == atoi(proto))
						  result = 1;
					  else
						  result = 0;
					  break;
				}	/* switch(*proto) */
				/* For Services */
				if (atoi(protocol) == 0)
					result = 0;
				/* if the proto in the V:line is * let it pass */
				if (*proto == '*')
					result = 0;


				if (result)
					return exit_client(cptr, cptr, cptr,
					    "Denied by V:line");

				/* If it passed the protocol check, check the flags */

				for (i = 0; vflags[i]; i++)
				{
					if (vflags[i] == '!')
					{
						i++;
						if (strchr(flags,
						    (int)vflags[i]))
						{
							result = 1;
							break;
						}
					}
					if (!strchr(flags, (int)vflags[i]))
					{
						result = 1;
						break;
					}
				}	/* for(i = 0; vflags[i]; i++) */
				if (*vflags == '*')
					result = 0;
				/* for services */
				if (!strcmp(flags, "0"))
					result = 0;
				if (result)
					return exit_client(cptr, cptr, cptr,
					    "Denied by V:line");

			}	/* if (vlines) */
		}		/* if (flags) */
		else
			strncpyzt(cptr->info, info[0] ? info : me.name,
			    sizeof(cptr->info));
	}
	else
		strncpyzt(cptr->info, info[0] ? info : me.name,
		    sizeof(cptr->info));
	cptr->hopcount = hop;

	/* check connection rules */
	for (cconf = conf; cconf; cconf = cconf->next)
		if ((cconf->status == CONF_CRULEALL) &&
		    (match(cconf->host, host) == 0))
			if (crule_eval(cconf->passwd))
			{
				ircstp->is_ref++;
				sendto_ops("Refused connection from %s.",
				    get_client_host(cptr));
				return exit_client(cptr, cptr, cptr,
				    "Disallowed by connection rule");
			}

	switch (check_server_init(cptr))
	{
	  case 0:
		  return m_server_estab(cptr);
	  case 1:
		  sendto_ops("Access check for %s in progress",
		      get_client_name(cptr, TRUE));
		  return 1;
	  default:
		  ircstp->is_ref++;
		  sendto_ops("Received unauthorized connection from %s.",
		      get_client_host(cptr));
		  sendto_serv_butone(&me,
		      ":%s GLOBOPS :Recieved unauthorized connection from %s.",
		      parv[0], get_client_host(cptr));
		  return exit_client(cptr, cptr, cptr, "No C/N conf lines");
	}

}

int  m_server_estab(cptr)
	aClient *cptr;
{
	aClient *acptr;
	aConfItem *aconf, *bconf;
	char *inpath, *host, *s, *encr;
	int  split, i;
	extern char serveropts[];
	unsigned long numeric;

	inpath = get_client_name(cptr, TRUE);	/* "refresh" inpath with host */
	split = mycmp(cptr->name, cptr->sockhost);
	host = cptr->name;

	current_load_data.conn_count++;
	update_load();

	if (!(aconf = find_conf(cptr->confs, host, CONF_NOCONNECT_SERVER)))
	{
		ircstp->is_ref++;
		sendto_one(cptr,
		    "ERROR :Access denied. No N:line for server %s", inpath);
		sendto_ops("Access denied. No N:line for server %s", inpath);
		return exit_client(cptr, cptr, cptr, "No N line for server");
	}
	if (!(bconf = find_conf(cptr->confs, host, CONF_CONNECT_SERVER)))
	{
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :Only N (no C) field for server %s",
		    inpath);
		sendto_ops("Only N (no C) field for server %s", inpath);
		return exit_client(cptr, cptr, cptr, "No C line for server");
	}

#ifdef CRYPT_LINK_PASSWORD
	/* use first two chars of the password they send in as salt */

	/* passwd may be NULL. Head it off at the pass... */
	if (*cptr->passwd)
	{
		char salt[3];
		extern char *crypt();

		salt[0] = aconf->passwd[0];
		salt[1] = aconf->passwd[1];
		salt[2] = '\0';
		encr = crypt(cptr->passwd, salt);
	}
	else
		encr = "";
#else
	encr = cptr->passwd;
#endif /* CRYPT_LINK_PASSWORD */
	if (*aconf->passwd && !StrEq(aconf->passwd, encr))
	{
		ircstp->is_ref++;
		sendto_one(cptr, "ERROR :No Access (passwd mismatch) %s",
		    inpath);
		sendto_ops("Access denied (passwd mismatch) %s", inpath);
		return exit_client(cptr, cptr, cptr, "Bad Password");
	}
	if (cptr->passwd)
	MyFree(cptr->passwd);
#ifndef	HUB
	for (i = 0; i <= highest_fd; i++)
		if (local[i] && IsServer(local[i]))
		{
			ircstp->is_ref++;
			sendto_one(cptr, "ERROR :I'm a leaf not a hub");
			return exit_client(cptr, cptr, cptr, "I'm a leaf");
		}
#endif
	if (IsUnknown(cptr))
	{
		sendto_one(cptr, "PROTOCTL %s", PROTOCTL_SERVER);
		if (bconf->passwd[0])
			sendto_one(cptr, "PASS :%s", bconf->passwd);
		/*
		   ** Pass my info to the new server
		 */
		/* modified so we send out the Uproto and flags */
		sendto_one(cptr, "SERVER %s 1 :U%d-%s-%i %s",
		    my_name_for_link(me.name, aconf), UnrealProtocol,
		    serveropts, me.serv->numeric, (me.info[0]) ? (me.info) : "IRCers United");
	}
	else
	{
		s = (char *)index(aconf->host, '@');
		*s = '\0';	/* should never be NULL */
		Debug((DEBUG_INFO, "Check Usernames [%s]vs[%s]",
		    aconf->host, cptr->username));
		if (match(aconf->host, cptr->username))
		{
			*s = '@';
			ircstp->is_ref++;
			sendto_ops("Username mismatch [%s]v[%s] : %s",
			    aconf->host, cptr->username,
			    get_client_name(cptr, TRUE));
			sendto_one(cptr, "ERROR :No Username Match");
			return exit_client(cptr, cptr, cptr, "Bad User");
		}
		*s = '@';
		sendto_one(cptr, "PROTOCTL %s", PROTOCTL_SERVER);
	}

	det_confs_butmask(cptr,
	    CONF_LEAF | CONF_HUB | CONF_NOCONNECT_SERVER | CONF_UWORLD);
	/*
	   ** *WARNING*
	   **   In the following code in place of plain server's
	   **   name we send what is returned by get_client_name
	   **   which may add the "sockhost" after the name. It's
	   **   *very* *important* that there is a SPACE between
	   **   the name and sockhost (if present). The receiving
	   **   server will start the information field from this
	   **   first blank and thus puts the sockhost into info.
	   **   ...a bit tricky, but you have been warned, besides
	   **   code is more neat this way...  --msa
	 */
	SetServer(cptr);
	IRCstats.me_servers++;
	IRCstats.servers++;
	IRCstats.unknown--;
#ifndef NO_FDLIST
	addto_fdlist(cptr->fd, &serv_fdlist);
#endif
	if ((find_uline(cptr->confs, cptr->name)))
		cptr->flags |= FLAGS_ULINE;
	cptr->flags |= FLAGS_TS8;
	nextping = TStime();
	(void)find_or_add(cptr->name);
	if (TRUEHUB == 1)
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :%sLink with %s established.", me.name, 
		    IsSecure(cptr) ? "Secure " : "",
		    inpath);
	sendto_locfailops("%sLink with %s established.", 
		IsSecure(cptr) ? "Secure " : "", inpath);
	/* Insert here */
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	(void)make_server(cptr);
	cptr->serv->up = me.name;
	cptr->srvptr = &me;
	cptr->serv->nline = aconf;

	if (num && numeric_collides(atoi(num)))
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Cancelling link %s, colliding numeric", me.name, 
		    inpath);
		sendto_locfailops("Cancelling link %s, colliding numeric", inpath);
		return exit_client(cptr, cptr, cptr,
		    "Colliding server numeric (choose another in the M:line)");
	}

	if (num)
	{
		cptr->serv->numeric = atoi(num);
		num = NULL;
	}
	add_server_to_table(cptr);
	/*
	   ** Old sendto_serv_but_one() call removed because we now
	   ** need to send different names to different servers
	   ** (domain name matching) Send new server to other servers.
	 */
	for (i = 0; i <= highest_fd; i++)
	{
		if (!(acptr = local[i]) || !IsServer(acptr) ||
		    acptr == cptr || IsMe(acptr))
			continue;
		if ((aconf = acptr->serv->nline) &&
		    !match(my_name_for_link(me.name, aconf), cptr->name))
			continue;
				
		if (SupportNS(acptr))
		{
			sendto_one(acptr, "%c%s %s %s 2 %i :%s",
				(me.serv->numeric ? '@' : ':'),
				(me.serv->numeric ? base64enc(me.serv->numeric) : me.name),
				(IsToken(acptr) ? TOK_SERVER : MSG_SERVER),
				cptr->name, cptr->serv->numeric, 
				cptr->info);
		}
			else
		{
			sendto_one(acptr, ":%s %s %s 2 :%s",
				me.name, 
				(IsToken(acptr) ? TOK_SERVER : MSG_SERVER),
				cptr->name, cptr->info);
		}
	}

	/*
	   ** Pass on my client information to the new server
	   **
	   ** First, pass only servers (idea is that if the link gets
	   ** cancelled beacause the server was already there,
	   ** there are no NICK's to be cancelled...). Of course,
	   ** if cancellation occurs, all this info is sent anyway,
	   ** and I guess the link dies when a read is attempted...? --msa
	   ** 
	   ** Note: Link cancellation to occur at this point means
	   ** that at least two servers from my fragment are building
	   ** up connection this other fragment at the same time, it's
	   ** a race condition, not the normal way of operation...
	   **
	   ** ALSO NOTE: using the get_client_name for server names--
	   **   see previous *WARNING*!!! (Also, original inpath
	   **   is destroyed...)
	 */

	aconf = cptr->serv->nline;
	for (acptr = &me; acptr; acptr = acptr->prev)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsServer(acptr))
		{
			if (match(my_name_for_link(me.name, aconf),
			    acptr->name) == 0)
				continue;
			split = (MyConnect(acptr) &&
			    mycmp(acptr->name, acptr->sockhost));
			    
			if (SupportNS(cptr))
			{
				/* this has to work. */
				numeric = ((aClient *) find_server_quick(acptr->serv->up))->serv->numeric;
				
				sendto_one(cptr, "%c%s %s %s %d %i :%s",
					(numeric ? '@' : ':'),
					(numeric ? base64enc(numeric) : acptr->serv->up),
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

	
	for (acptr = &me; acptr; acptr = acptr->prev)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsPerson(acptr))
		{
			/*
			   ** IsPerson(x) is true only when IsClient(x) is true.
			   ** These are only true when *BOTH* NICK and USER have
			   ** been received. -avalon
			   ** Apparently USER command was forgotten... -Donwulff
			 */

			
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
					if (SupportNS(cptr) && acptr->srvptr->serv->numeric)
					{
						sendto_one(cptr,
							cptr->proto & PROTO_SJB64 ?
							"%s %s %d %B %s %s %b %lu %s %s :%s"
							:
							"%s %s %d %d %s %s %b %lu %s %s :%s"
							,
						    (IsToken(cptr) ? TOK_NICK :
						    MSG_NICK), acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						     acptr->srvptr->serv->numeric,
						    acptr->user->servicestamp, (!buf
						    || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr)
						    && (acptr->
						    umodes & UMODE_SETHOST)) ? acptr->
						    user->virthost : "*"), acptr->info);
					}
					else
					{
						sendto_one(cptr,
						    (cptr->proto & PROTO_SJB64 ?
						    "%s %s %d %B %s %s %s %lu %s %s :%s"
						    : "%s %s %d %d %s %s %s %lu %s %s :%s"),
						    (IsToken(cptr) ? TOK_NICK :
						    MSG_NICK), acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
					     		acptr->user->server,
						    acptr->user->servicestamp, (!buf
						    || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr)
						    && (acptr->
						    umodes & UMODE_SETHOST)) ? acptr->
						    user->virthost : "*"), acptr->info);
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
					     base64enc(acptr->srvptr->serv->numeric) : 
					     acptr->user->server) : acptr->user->server),
					    acptr->user->servicestamp, (!buf
					    || *buf == '\0' ? "+" : buf),
					    IsHidden(acptr) ? acptr->user->
					    virthost : acptr->user->realhost,
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
		aSqlineItem *tmp;
		char	*ns = NULL;
		
		if (me.serv->numeric && SupportNS(cptr))
			ns = base64enc(me.serv->numeric);
		else
			ns = NULL;
		
		for (tmp = sqline; tmp; tmp = tmp->next)
		{
			if (tmp->status != CONF_ILLEGAL)
				if (tmp->reason)
					sendto_one(cptr, "%s%s %s %s :%s",
					    ns ? "@" : ":",
					    ns ? ns : me.name,
					    (IsToken(cptr) ? TOK_SQLINE :
					    MSG_SQLINE), tmp->sqline,
					    tmp->reason);
				else
					sendto_one(cptr, "%s%s %s %s",
					    ns ? "@" : ":",
					    ns ? ns : me.name,
					    me.name,
					    (IsToken(cptr) ? TOK_SQLINE :
					    MSG_SQLINE), tmp->sqline);
		}
	}

	sendto_one(cptr, "%s %li %li %li 0 0 0 0 :%s",
	    (IsToken(cptr) ? TOK_NETINFO : MSG_NETINFO),
	    IRCstats.global_max, TStime(), UnrealProtocol, ircnetwork);
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
int  m_links(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	Link *lp;
	aClient *acptr;

	for (lp = (Link *) return_servers(); lp; lp = lp->next)
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

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0],
	    "*");
	return 0;
}


/*
** m_netinfo
** by Stskeeps
**  parv[0] = sender prefix
**  parv[1] = max global count
**  parv[2] = time of end sync 
**  parv[3] = unreal protocol using (numeric)
**  parv[4] = free(for unrealprotocol > 2100)
**  parv[5] = free(**)
**  parv[6] = free(**)
**  parv[7] = free(**)
**  parv[8] = ircnet
**/

int  m_netinfo(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	long lmax;
	time_t xx;
	long endsync, protocol;

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
		if (KILLDIFF == 1)
		{
			sendto_realops
			    ("Dropped link %s - unProtocol 2090 is not compatible with unProtocol %li",
			    cptr->name, UnrealProtocol);
			sendto_one(cptr,
			    "ERROR :unProtocol 2090 is not compatible with unProtocol %li",
			    UnrealProtocol);
			return exit_client(cptr, cptr, cptr,
			    "Link using unProtocol 2090");
		}
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
	endsync = atol(parv[2]);
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

	SetNetInfo(cptr);
}

#ifndef IRCDTOTALVERSION
#define IRCDTOTALVERSION BASE_VERSION PATCH1 PATCH2 PATCH3 PATCH4 PATCH5 PATCH6 PATCH7 PATCH8 PATCH9
#endif

/* 
 * sends m_info into to sptr
*/

void m_info_send(sptr)
	aClient *sptr;
{
	sendto_one(sptr, ":%s %d %s :=-=-=-= %s =-=-=-=",
	    me.name, RPL_INFO, sptr->name, IRCDTOTALVERSION);
	sendto_one(sptr, ":%s %d %s :| Brought to you by the following people:",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :|", me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Stskeeps     <stskeeps@tspre.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * codemastr    <codemastr@tspre.org>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * DrBin        <drbin@tspre.org>",
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
	sendto_one(sptr, ":%s %d %s :|  unreal-dev@lists.sourceforge.net",
	    me.name, RPL_INFO, sptr->name);

	sendto_one(sptr,
	    ":%s %d %s :| UnrealIRCd Homepage: http://unreal.tspre.org",
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
	    sptr->name, RELEASEID);
	sendto_one(sptr, rpl_str(RPL_ENDOFINFO), me.name, sptr->name);
}

/*
** m_info
**	parv[0] = sender prefix
**	parv[1] = servername
**  Modified for hardcode by Stskeeps  
*/

int  m_info(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{

	if (hunt_server(cptr, sptr, ":%s INFO :%s", 1, parc,
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
int  m_dalinfo(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char **text = dalinfotext;

	if (hunt_server(cptr, sptr, ":%s DALINFO :%s", 1, parc,
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
int  m_license(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char **text = gnulicense;

	if (hunt_server(cptr, sptr, ":%s LICENSE :%s", 1, parc,
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
int  m_credits(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char **text = unrealcredits;

	if (hunt_server(cptr, sptr, ":%s CREDITS :%s", 1, parc,
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
static void show_watch(cptr, name, rpl1, rpl2)
	aClient *cptr;
	char *name;
	int  rpl1, rpl2;
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
int  m_watch(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
				if (sptr->notifies >= MAXWATCH)
				{
					sendto_one(sptr,
					    err_str(ERR_TOOMANYWATCH), me.name,
					    cptr->name, s + 1);

					continue;
				}

				add_to_notify_hash_table(s + 1, sptr);
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
			del_from_notify_hash_table(s + 1, sptr);
			show_watch(sptr, s + 1, RPL_WATCHOFF, RPL_WATCHOFF);

			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			hash_del_notify_list(sptr);

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
			aNotify *anptr;
			int  count = 0;

			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on.
			 */
			anptr = hash_get_notify(sptr->name);
			if (anptr)
				for (lp = anptr->notify, count = 1;
				    (lp = lp->next); count++)
					;
			sendto_one(sptr, rpl_str(RPL_WATCHSTAT), me.name,
			    parv[0], sptr->notifies, count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			if ((lp = sptr->notify) == NULL)
			{
				sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST),
				    me.name, parv[0], *s);
				continue;
			}
			*buf = '\0';
			strcpy(buf, lp->value.nptr->nick);
			count =
			    strlen(parv[0]) + strlen(me.name) + 10 +
			    strlen(buf);
			while ((lp = lp->next))
			{
				if (count + strlen(lp->value.nptr->nick) + 1 >
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
				strcat(buf, lp->value.nptr->nick);
				count += (strlen(lp->value.nptr->nick) + 1);
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
			Link *lp = sptr->notify;

			while (lp)
			{
				if ((acptr =
				    find_person(lp->value.nptr->nick, NULL)))
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
					    lp->value.nptr->nick, "*", "*",
					    lp->value.nptr->lasttime);
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





/*
** m_stats

**	parv[0] = sender prefix
**	parv[1] = statistics selector (defaults to Message frequency)
**	parv[2] = server name (current server defaulted, if omitted)
**
**	Currently supported are:
**		M = Message frequency (the old stat behaviour)
**		L = Local Link statistics
**              C = Report C and N configuration lines
*/
/*
** m_stats/stats_conf
**    Report N/C-configuration lines from this server. This could
**    report other configuration lines too, but converting the
**    status back to "char" is a bit akward--not worth the code
**    it needs...
**
**    Note:   The info is reported in the order the server uses
**            it--not reversed as in ircd.conf!
*/

static int report_array[][3] = {
	{CONF_CONNECT_SERVER, RPL_STATSCLINE, 'C'},
	{CONF_NOCONNECT_SERVER, RPL_STATSOLDNLINE, 'N'},
	{CONF_NLINE, RPL_STATSNLINE, 'n'},
	{CONF_CLIENT, RPL_STATSILINE, 'I'},
	{CONF_KILL, RPL_STATSKLINE, 'K'},
	{CONF_EXCEPT, RPL_STATSKLINE, 'E'},
	{CONF_ZAP, RPL_STATSKLINE, 'Z'},
	{CONF_QUARANTINED_NICK, RPL_STATSQLINE, 'Q'},
	{CONF_LEAF, RPL_STATSLLINE, 'L'},
	{CONF_OPERATOR, RPL_STATSOLINE, 'O'},
	{CONF_HUB, RPL_STATSHLINE, 'H'},
	{CONF_LOCOP, RPL_STATSOLINE, 'o'},
	{CONF_CRULEALL, RPL_STATSDLINE, 'D'},
	{CONF_CRULEAUTO, RPL_STATSDLINE, 'd'},
	{CONF_UWORLD, RPL_STATSULINE, 'U'},
	{CONF_MISSING, RPL_STATSXLINE, 'X'},
	{CONF_TLINE, RPL_STATSTLINE, 't'},
	{CONF_SOCKSEXCEPT, RPL_STATSELINE, 'e'},
	{CONF_VERSION, RPL_STATSVLINE, 'V'},
	{0, 0}
};

static void report_sqlined_nicks(sptr)
	aClient *sptr;
{
	aSqlineItem *tmp;
	char *nickmask, *reason;

	for (tmp = sqline; tmp; tmp = tmp->next)
	{
		if (tmp->status != CONF_ILLEGAL)
		{
			nickmask = BadPtr(tmp->sqline) ? "<NULL>" : tmp->sqline;
			reason = BadPtr(tmp->reason) ? "<NULL>" : tmp->reason;
			sendto_one(sptr, rpl_str(RPL_SQLINE_NICK), me.name,
			    sptr->name, nickmask, reason);
		}
	}
}

static void report_configured_links(sptr, mask)
	aClient *sptr;
	int  mask;
{
	static char null[] = "<NULL>";
	aConfItem *tmp;
	int *p, port, tmpmask;
	char c, *host, *pass, *name;
	tmpmask = (mask == CONF_MISSING) ? CONF_CONNECT_SERVER : mask;

	for (tmp = conf; tmp; tmp = tmp->next)
		if (tmp->status & tmpmask)
		{
			for (p = &report_array[0][0]; *p; p += 3)
				if (*p == tmp->status)
					break;
			if (!*p)
				continue;
			c = (char)*(p + 2);
			host = BadPtr(tmp->host) ? null : tmp->host;
			pass = BadPtr(tmp->passwd) ? null : tmp->passwd;
			name = BadPtr(tmp->name) ? null : tmp->name;
			port = (int)tmp->port;
			/*
			 * On K line the passwd contents can be
			 * displayed on STATS reply.    -Vesa
			 */
			/* Same with Z-lines and q/Q-lines -- Barubary */
			if ((tmp->status == CONF_KILL) || (tmp->status &
			    CONF_QUARANTINE) || (tmp->status & CONF_EXCEPT)
			    || (tmp->status == CONF_ZAP))
			{
/* These mods are to tell the difference between the different kinds
 * of klines.  the only effect it has is in the display.  --Russell
 */
/* Now translates spaces to _'s to show comments in klines -- Barubary */
				char *temp;
				if (!pass)
					strcpy(buf, "<NULL>");
				else
				{
					strcpy(buf, pass);
					for (temp = buf; *temp; temp++)
						if (*temp == ' ')
							*temp = '_';
				}
				/* semicolon intentional -- Barubary */
				if (tmp->status == CONF_QUARANTINED_NICK);
				/* Hide password for servers -- Barubary */
				else if (tmp->status & CONF_QUARANTINE)
					strcpy(buf, "*");
				else
				{
/* This wasn't documented before - comments aren't displayed for akills
   because they are all the same. -- Barubary */
					if (tmp->tmpconf == KLINE_AKILL)
						strcpy(buf, "*");

					/*                Show comments in E:Lines..
					   if (tmp->tmpconf == KLINE_EXCEPT)
					   strcpy(buf, "*");
					 */
					/* KLINE_PERM == 0 - watch out when doing
					   Z-lines. -- Barubary */
					if (tmp->status != CONF_ZAP)
					{
						if (tmp->tmpconf == KLINE_PERM)
							c = 'K';
						if (tmp->tmpconf == KLINE_TEMP)
							c = 'k';
						if (tmp->tmpconf == KLINE_AKILL)
							c = 'A';
						if (tmp->tmpconf ==
						    KLINE_EXCEPT) c = 'E';
					}
					else
					{
						if (tmp->tmpconf == KLINE_PERM)
							c = 'Z';
						if (tmp->tmpconf == KLINE_TEMP)
							c = 'z';
						if (tmp->tmpconf == KLINE_AKILL)
							c = 'S';
						if (tmp->tmpconf ==
						    KLINE_EXCEPT) c = 'e';
					}
				}
				sendto_one(sptr, rpl_str(p[1]), me.name,
				    sptr->name, c, host,
				    buf, name, port, get_conf_class(tmp));
			}
			else if (mask & CONF_OPS)
			{
				sendto_one(sptr, rpl_str(p[1]), me.name,
				    sptr->name, c, host, name, oflagstr(port),
				    get_conf_class(tmp));
			}

			/* connect rules are classless */
			else if (tmp->status & CONF_CRULE)
				sendto_one(sptr, rpl_str(p[1]), me.name,
				    sptr->name, c, host, name);
			/* Only display on X if server is missing */
			else if (mask == CONF_MISSING)
			{
				if (!find_server_quick(name))
					sendto_one(sptr,
					    rpl_str(RPL_STATSXLINE), me.name,
					    sptr->name, name, port);
			}
			else if (mask == CONF_TLINE)
			{
				sendto_one(sptr, rpl_str(RPL_STATSTLINE),
				    me.name, sptr->name, host, pass, name);
			}
			else if (mask == CONF_SOCKSEXCEPT)
			{
				sendto_one(sptr, rpl_str(RPL_STATSELINE),
				    me.name, sptr->name, host, pass, name);
			}
			else if (mask == CONF_NLINE)
			{
				sendto_one(sptr, rpl_str(RPL_STATSNLINE),
				    me.name, sptr->name, host, pass);
			}
			else if (mask == CONF_VERSION)
				sendto_one(sptr, rpl_str(RPL_STATSVLINE),
				    me.name, sptr->name, host, pass, name);
/*			else if (mask == CONF_EXCEPT)
			{
				ppx = MyMalloc(strlen(tmp->passwd) + 1);
				strcpy(ppx, tmp->passwd);
				for (pp = ppx; *pp != '\0'; pp++) {
					if (*pp == ' ')
						*pp = '_';
				}
				sendto_one(sptr, rpl_str(RPL_STATSKLINE), me.name,
					sptr->name, "E", host, ppx, name, 0,0, -1);
				MyFree(ppx);
			} */
			else
			{
				if (!IsOper(sptr)
				    && (mask & CONF_NOCONNECT_SERVER
				    || mask & CONF_CONNECT_SERVER))
					sendto_one(sptr, rpl_str(p[1]), me.name,
					    sptr->name, c, "*", name, port,
					    get_conf_class(tmp));
				else
					sendto_one(sptr, rpl_str(p[1]), me.name,
					    sptr->name, c, host, name, port,
					    get_conf_class(tmp));
			}
		}
	return;
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
	strcpy((char *)strrchr((char *)pointer, '.'), ".0]");

	return pointer;
}

int  m_stats(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
#ifndef DEBUGMODE
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since :Idle";
	static char Lformat[] = ":%s %d %s %s %u %u %u %u %u %u :%u";
#else
	static char Sformat[] =
	    ":%s %d %s SendQ SendM SendBytes RcveM RcveBytes Open_since CPU :Idle";
	static char Lformat[] = ":%s %d %s %s %u %u %u %u %u %u %s";
	char pbuf[96];		/* Should be enough for to ints */
#endif
	struct Message *mptr;
	aClient *acptr;
	char stat = parc > 1 ? parv[1][0] : '\0';
	int  i;
	int  doall = 0, wilds = 0, showports = IsAnOper(sptr), remote = 0;
	char *name;


#ifdef STATS_ONLYOPER
	if (!IsAnOper(sptr))
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);

#endif

	if (hunt_server(cptr, sptr, ":%s STATS %s :%s", 2, parc,
	    parv) != HUNTED_ISME)
		return 0;

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

	if (stat != '\0')
		sendto_umode(UMODE_EYES, "Stats \'%c\' requested by %s (%s@%s)",
		    stat, sptr->name, sptr->user->username,
		    IsHidden(sptr) ? sptr->user->virthost : sptr->user->
		    realhost);
	else
		sendto_umode(UMODE_EYES,
		    "Stats \'NULL\' requested by %s (%s@%s)", sptr->name,
		    sptr->user->username,
		    IsHidden(sptr) ? sptr->user->virthost : sptr->user->
		    realhost);

	switch (stat)
	{
#ifdef STRIPBADWORDS
	  case 'b':
		  badwords_stats();
		  break;
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
		  for (i = 0; i <= highest_fd; i++)
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
			  if (!(parc == 2 && IsServer(acptr)) &&
			      !(doall || wilds) && mycmp(name, acptr->name))
				  continue;
#ifdef DEBUGMODE
			  ircsprintf(pbuf, "%d :%d", acptr->cputime,
			      (acptr->user && MyConnect(acptr)) ?
			      TStime() - acptr->user->last : 0);
#endif
			  if (IsOper(sptr))
			  {
				  sendto_one(sptr, Lformat, me.name,
				      RPL_STATSLINKINFO, parv[0],
				      (isupper(stat)) ?
				      get_client_name2(acptr, showports) :
				      get_client_name(acptr, FALSE),
				      (int)DBufLength(&acptr->sendQ),
				      (int)acptr->sendM, (int)acptr->sendK,
				      (int)acptr->receiveM,
				      (int)acptr->receiveK,
				      TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				      (acptr->user && MyConnect(acptr)) ?
				      TStime() - acptr->user->last : 0);
#else
				      pbuf);
#endif
				  if (!IsServer(acptr) && IsAnOper(acptr))
					  sendto_one(acptr,
					      ":%s NOTICE %s :*** %s did a /stats L on you! IP may have been shown",
					      me.name, acptr->name, sptr->name);
			  }
			  else if (!strchr(acptr->name, '.'))
				  sendto_one(sptr, Lformat, me.name,
				      RPL_STATSLINKINFO, parv[0],
				      IsHidden(acptr) ? acptr->name :
				      (isupper(stat)) ?	/* Potvin - PreZ */
				      get_client_name2(acptr, showports) :
				      get_client_name(acptr, FALSE),
				      (int)DBufLength(&acptr->sendQ),
				      (int)acptr->sendM, (int)acptr->sendK,
				      (int)acptr->receiveM,
				      (int)acptr->receiveK,
				      TStime() - acptr->firsttime,
#ifndef DEBUGMODE
				      (acptr->user && MyConnect(acptr)) ?
				      TStime() - acptr->user->last : 0);
#else
				      pbuf);
#endif
		  }
		  break;
	  case 'C':
	  case 'c':
		  report_configured_links(sptr, CONF_CONNECT_SERVER |
		      CONF_NOCONNECT_SERVER);
		  break;
	  case 'f':
	  case 'F':
		  report_flines(sptr);
		  break;

	  case 'G':
	  case 'g':
		  tkl_stats(sptr);
		  break;
	  case 'H':
	  case 'h':
		  report_configured_links(sptr, CONF_HUB | CONF_LEAF);
		  break;
	  case 'I':
	  case 'i':
		  report_configured_links(sptr, CONF_CLIENT);
		  break;
	  case 'E':
		  report_configured_links(sptr, CONF_EXCEPT);
		  break;
	  case 'e':
		  report_configured_links(sptr, CONF_SOCKSEXCEPT);
		  break;
	  case 'K':
	  case 'k':
		  report_configured_links(sptr,
		      CONF_KILL | CONF_ZAP | CONF_EXCEPT);
		  break;
	  case 'M':
	  case 'm':
		  for (mptr = msgtab; mptr->cmd; mptr++)
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
		  report_configured_links(sptr, CONF_NLINE);
		  break;
	  case 'N':
		  if (IsOper(sptr))
			  report_network(sptr);
		  break;
	  case 'o':
	  case 'O':
/*			if (SHOWOPERS == 1) {
				if(IsOper(sptr)) {
					report_configured_links(sptr, CONF_OPS);
					break;
				}
			}
			 	else
			if (SHOWOPERS == 0) {
				report_configured_links(sptr, CONF_OPS);			
*/
		  if (SHOWOPERS == 0 && (IsOper(sptr)))
		  {
			  report_configured_links(sptr, CONF_OPS);
			  break;
		  }
		  if (SHOWOPERS == 1)
			  report_configured_links(sptr, CONF_OPS);
		  break;
	  case 'Q':
		  report_configured_links(sptr, CONF_QUARANTINE);
		  break;
	  case 'q':
		  report_sqlined_nicks(sptr);
		  break;
	  case 'R':
#ifdef DEBUGMODE
		  send_usage(sptr, parv[0]);
#endif
		  break;
	  case 's':
		  if (IsOper(sptr))
		  {
			  sendto_one(sptr, ":%s NOTICE %s :*** SCACHE:",
			      me.name, sptr->name);
			  list_scache(sptr);
			  sendto_one(sptr, ":%s NOTICE %s :*** NS:", me.name,
			      sptr->name);
			  ns_stats(sptr);
		  }
		  break;
	  case 'S':
		  if (IsOper(sptr))
			  report_dynconf(sptr);
		  break;
	  case 'D':
		  report_configured_links(sptr, CONF_CRULEALL);
		  break;
	  case 'd':
		  report_configured_links(sptr, CONF_CRULE);
		  break;
	  case 'r':
		  cr_report(sptr);
		  break;
	  case 't':
		  report_configured_links(sptr, CONF_TLINE);
		  break;
	  case 'T':		/* /stats T not t:lines .. */
		  tstats(sptr, parv[0]);
		  break;
	  case 'U':
		  report_configured_links(sptr, CONF_UWORLD);
		  break;
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
		  report_configured_links(sptr, CONF_VERSION);
		  break;
	  case 'V':
		  vhost_report(sptr);
		  break;
	  case 'W':
	  case 'w':
		  calc_load(sptr, parv[0]);
		  break;
	  case 'X':
	  case 'x':
		  report_configured_links(sptr, CONF_MISSING);
		  break;
	  case 'Y':
	  case 'y':
		  report_classes(sptr);
		  break;
	  case 'Z':
	  case 'z':
		  if (IsAnOper(sptr))
			  count_memory(sptr, parv[0]);
		  break;
	  default:
		  stat = '*';
		  break;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFSTATS), me.name, parv[0], stat);
	return 0;
}

/*
** m_summon
** parv[0] = sender prefix
*/
int m_summon(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
	/* /summon is old and out dated, we just return an error as
         * required by RFC1459 -- codemastr
	 */
	sendto_one(sptr, err_str(ERR_SUMMONDISABLED), me.name, parv[0]);
	return 0;
}

/*
** m_users
**	parv[0] = sender prefix
**	parv[1] = servername
*/
int  m_users(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	/* /users is out of date, just return an error as  required by
	 * RFC1459 -- codemastr
	 */
	sendto_one(sptr, err_str(ERR_USERSDISABLED), me.name, parv[0]);
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
int  m_error(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
void reset_help()
{
	free_str_list(helpign);
}




/*
** m_help (help/write to +h currently online) -Donwulff
**	parv[0] = sender prefix
**	parv[1] = optional message text
*/
int  m_help(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
		if (BadPtr(message))
			return 0;
		if (message[0] == '?')
		{
			parse_help(sptr, parv[0], message + 1);
			return 0;
		}
		if (message[1] == '!')
			sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
			    parv[0], MSG_HELP, TOK_HELP, "%s", message);
		if (!myncmp(message, "IGNORE ", 7))
		{
			tmpl = make_link();
			DupString(tmpl->value.cp, message + 7);
			tmpl->next = helpign;
			helpign = tmpl;
		}
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s (HelpOp): %s", parv[0], message);		
	}
	else if (MyConnect(sptr))
	{
		/* New syntax: ?... never goes out, !... always does. */
		if (!BadPtr(message))
		{
			parse_help(sptr, parv[0], message);
			return 0;
		}
		if ((!BadPtr(message) && !(message[0] == '!'))
		    || BadPtr(message))
			if (parse_help(sptr, parv[0], message))
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
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s (Local): %s", parv[0], message);		
		sendto_one(sptr, rpl_str(RPL_HELPFWD), me.name, parv[0]);
	}
	else
	{
		sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
		    parv[0], MSG_HELP, TOK_HELP, "%s", message);
		sendto_umode(UMODE_HELPOP, "*** HelpOp -- from %s: %s", parv[0], message);		
	}

	return 0;
}

/*
 * parv[0] = sender
 * parv[1] = host/server mask.
 * parv[2] = server to query
 */
int  m_lusers(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char mydom_mask[HOSTLEN + 1];

/*	Doesn't work anyways --Stskeeps

	if (parc > 2)
		if(hunt_server(cptr, sptr, ":%s LUSERS %s :%s", 2, parc, parv)
				!= HUNTED_ISME)
			return 0;
*/

	mydom_mask[0] = '*';
	strncpy(&mydom_mask[1], DOMAINNAME, HOSTLEN - 1);
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
	current_load_data.client_count = IRCstats.me_clients;
	current_load_data.conn_count =
	    IRCstats.me_clients + IRCstats.me_servers;
	return 0;
}


void save_tunefile(void)
{
	FILE *tunefile;

	tunefile = fopen(IRCDTUNE, "w+");
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
	fprintf(tunefile, "%li\n", IRCstats.me_max);
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
 ***********************************************************************/

/*
** m_connect
**	parv[0] = sender prefix
**	parv[1] = servername
**	parv[2] = port number
**	parv[3] = remote server
*/
int  m_connect(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	int  port, tmpport, retval;
	aConfItem *aconf, *cconf;
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
	if (hunt_server(cptr, sptr, ":%s CONNECT %s %s :%s",
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
		sendto_one(sptr, ":%s NOTICE %s :Connect: Server %s %s %s.",
		    me.name, parv[0], parv[1], "already exists from",
		    acptr->from->name);
		return 0;
	}

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status == CONF_CONNECT_SERVER &&
		    match(parv[1], aconf->name) == 0)
			break;
	/* Checked first servernames, then try hostnames. */
	if (!aconf)
		for (aconf = conf; aconf; aconf = aconf->next)
			if (aconf->status == CONF_CONNECT_SERVER &&
			    (match(parv[1], aconf->host) == 0 ||
			    match(parv[1], index(aconf->host, '@') + 1) == 0))
				break;

	if (!aconf)
	{
		sendto_one(sptr,
		    "NOTICE %s :Connect: Host %s not listed in ircd.conf",
		    parv[0], parv[1]);
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
			    "NOTICE %s :Connect: Illegal port number", parv[0]);
			return 0;
		}
	}
	else if (port <= 0 && (port = PORTNUM) <= 0)
	{
		sendto_one(sptr, ":%s NOTICE %s :Connect: missing port number",
		    me.name, parv[0]);
		return 0;
	}

	/*
	   ** Evaluate connection rules...  If no rules found, allow the
	   ** connect.  Otherwise stop with the first true rule (ie: rules
	   ** are ored together.  Oper connects are effected only by D
	   ** lines (CRULEALL) not d lines (CRULEAUTO).
	 */
	for (cconf = conf; cconf; cconf = cconf->next)
		if ((cconf->status == CONF_CRULEALL) &&
		    (match(cconf->host, aconf->name) == 0))
			if (crule_eval(cconf->passwd))
			{
				sendto_one(sptr,
				    "NOTICE %s :Connect: Disallowed by rule: %s",
				    parv[0], cconf->name);
				return 0;
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
#if defined(USE_SYSLOG) && defined(SYSLOG_CONNECT)
		syslog(LOG_DEBUG, "CONNECT From %s : %s %d", parv[0], parv[1],
		    parv[2] ? parv[2] : "");
#endif
	}
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	  case 0:
		  sendto_one(sptr,
		      ":%s NOTICE %s :*** Connecting to %s[%s].",
		      me.name, parv[0], aconf->host, aconf->name);
		  break;
	  case -1:
		  sendto_one(sptr, ":%s NOTICE %s :*** Couldn't connect to %s.",
		      me.name, parv[0], aconf->host);
		  break;
	  case -2:
		  sendto_one(sptr, ":%s NOTICE %s :*** Host %s is unknown.",
		      me.name, parv[0], aconf->host);
		  break;
	  default:
		  sendto_one(sptr,
		      ":%s NOTICE %s :*** Connection to %s failed: %s",
		      me.name, parv[0], aconf->host, strerror(retval));
	}
	aconf->port = tmpport;
	return 0;
}

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
int  m_wallops(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
int  m_gnotice(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
** m_addline (write a line to ircd.conf)
**
** De-Potvinized by codemastr
*/
int  m_addline(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
	sendto_one(sptr, ":%s NOTICE %s :*** Wrote (%s) to ircd.conf",
	    me.name, parv[0], text);
	fprintf(conf, "# Added by %s\n", make_nick_user_host(sptr->name,
	    sptr->user->username, sptr->user->realhost));
/*	for (i=1 ; i<parc ; i++)
	{
		if (i!=parc-1)
			fprintf (conf,"%s ",parv[i]);
		else
			fprintf (conf,"%s\n",parv[i]);
	} 
	/* I dunno what Potvin was smoking when he made this code, but it plain SUX
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
int  m_addmotd(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
	sendto_one(sptr, ":%s NOTICE %s :*** Wrote (%s) to file: ircd.motd",
	    me.name, parv[0], text);
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
** m_svsmotd
**
*/
int  m_svsmotd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	FILE *conf = NULL;

	if (!IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "SVSMOTD");
		return 0;
	}

	if ((*parv[1] != '!') && parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "SVSMOTD");
		return 0;
	}

	switch (*parv[1])
	{
	  case '#':
		  conf = fopen(VPATH, "a");
		  sendto_ops("Added '%s' to services motd", parv[2]);
		  break;
	  case '!':
	  {
		  remove(VPATH);
		  sendto_ops("Wiped out services motd data");
		  break;
	  }
	  default:
		  break;
	}

	sendto_serv_butone_token(cptr, parv[0], MSG_SVSMOTD, TOK_SVSMOTD,
	    "%s :%s", parv[1], parv[2]);

	if (conf == NULL)
	{
		return 0;
	}

	if (parc < 3 && (*parv[1] == '!'))
	{
		fclose(conf);
		return 1;
	}
	fprintf(conf, "%s\n", parv[2]);
	if (*parv[1] == '!')
		sendto_ops("Added '%s' to services motd", parv[2]);

	fclose(conf);
	/* We editted it, so rehash it -- codemastr */
	read_svsmotd(VPATH);
	return 1;
}

/*
** m_addomotd (write a line to opermotd)
**
** De-Potvinized by codemastr
*/
int  m_addomotd(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
	sendto_one(sptr, ":%s NOTICE %s :*** Wrote (%s) to OperMotd",
	    me.name, parv[0], text);
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
int  m_globops(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
int  m_locops(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
int  m_chatops(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *message;

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "CHATOPS");
		return 0;
	}
	if (ALLOW_CHATOPS == 1)
	{
		if (MyClient(sptr) && !IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}
	}
	else
	{
		if (MyClient(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
	    parv[0], MSG_CHATOPS, TOK_CHATOPS, ":%s", message);
	if (ALLOW_CHATOPS == 1)
	{
		sendto_umode(UMODE_OPER, "*** ChatOps -- from %s: %s",
		    parv[0], message);
		sendto_umode(UMODE_LOCOP, "*** ChatOps -- from %s: %s",
		    parv[0], message);
	}

	return 0;
}


/* m_goper  (Russell) sort of like wallop, but only to ALL +o clients on
** every server.
**      parv[0] = sender prefix
**      parv[1] = message text
*/
int  m_goper(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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
int  m_time(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	if (hunt_server(cptr, sptr, ":%s TIME :%s", 1, parc,
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
int  m_svskill(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
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

/*	if (hunt_server(cptr,sptr,":%s SVSKILL %s :%s",1,parc,parv) != HUNTED_ISME)
		return 0;
*/

	if (parc < 1 || (!(acptr = find_client(parv[1], NULL))))
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
int  m_admin(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aConfItem *aconf;

	/* Users may want to get the address in case k-lined, etc. -- Barubary

	   /* Only allow remote ADMINs if registered -- Barubary */
	if (IsPerson(sptr) || IsServer(cptr))
		if (hunt_server(cptr, sptr, ":%s ADMIN :%s", 1, parc,
		    parv) != HUNTED_ISME)
			return 0;
	if ((aconf = find_admin()))
	{
		sendto_one(sptr, rpl_str(RPL_ADMINME),
		    me.name, parv[0], me.name);
		sendto_one(sptr, rpl_str(RPL_ADMINLOC1),
		    me.name, parv[0], (aconf->host ? aconf->host : "-"));
		sendto_one(sptr, rpl_str(RPL_ADMINLOC2),
		    me.name, parv[0], (aconf->passwd ? aconf->passwd : "-"));
		sendto_one(sptr, rpl_str(RPL_ADMINEMAIL),
		    me.name, parv[0], (aconf->name ? aconf->name : "-"));
	}
	else
		sendto_one(sptr, err_str(ERR_NOADMININFO),
		    me.name, parv[0], me.name);
	return 0;
}


/*
** m_rehash
** remote rehash by binary
** now allows the -flags in remote rehash
** ugly code but it seems to work :) -- codemastr
*/
int  m_rehash(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	int  x;

	if (MyClient(sptr) && !OPCanRehash(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!MyClient(sptr) && !(IsTechAdmin(sptr) || IsNetAdmin(sptr))
	    && !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	x = 0;

	if (!BadPtr(parv[1]))
	{
		if (*parv[1] == '-')
		{
			if (!match("-dcc*", parv[1]))
			{
				sendto_ops
				    ("Rehashing dccdeny.conf on request of %s",
				    sptr->name);
				dcc_rehash();
				return 0;
			}
			if (!match("-dyn*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing dynamic configuration on request of %s",
				    sptr->name);
				load_conf(ZCONF, 1);
				return 0;
			}
			if (!match("-gar*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				loop.do_garbage_collect = 1;
				return 0;
			}
			if (!match("-rest*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing channel restrict configuration on request of %s",
				    sptr->name);
				cr_rehash();
				return 0;
			}
			if (!match("-o*motd", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing OperMOTD on request of %s",
				    sptr->name);
				opermotd = (aMotd *) read_opermotd(OPATH);
				return 0;
			}
			if (!match("-b*motd", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops("Rehashing BotMOTD on request of %s",
				    sptr->name);
				botmotd = (aMotd *) read_botmotd(BPATH);
				return 0;
			}
			if (!match("-motd*", parv[1])
			    || !match("-rules*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing all MOTDs and RULES on request of %s",
				    sptr->name);
				motd = (aMotd *) read_motd(MPATH);
				rules = (aMotd *) read_rules(RPATH);
				read_tlines();
				return 0;
			}
			if (!match("-vhos*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing vhost configuration on request of %s",
				    sptr->name);
				vhost_rehash();
				return 0;
			}
#ifdef STRIPBADWORDS
			if (!match("-bad*", parv[1]))
			{
				if (!IsAdmin(sptr))
					return 0;
				sendto_ops
				    ("Rehashing badword configuration on request of %s",
				    sptr->name);
				freebadwords();
				loadbadwords_channel("badwords.channel.conf");
				loadbadwords_message("badwords.message.conf");
				return 0;
			}
#endif
		}
		if (MyClient(sptr) && !(IsNetAdmin(sptr) || IsTechAdmin(sptr)))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}
		/* This little number allows us to send a -param to another server,
		 * but only if it exists -- codemastr*/
		if (parv[2] != NULL)
		{
			if ((x =
			    hunt_server(cptr, sptr, ":%s REHASH %s %s", 1, parc,
			    parv)) != HUNTED_ISME)
				return 0;
		}
		if (parv[2] == NULL)
		{
			if ((x =
			    hunt_server(cptr, sptr, ":%s REHASH %s", 1, parc,
			    parv)) != HUNTED_ISME)
				return 0;
		}

	}

	if (cptr != sptr)
	{

#ifndef REMOTE_REHASH
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
#else
		if (parv[2] == NULL)
		{
			sendto_serv_butone(&me,
			    ":%s GLOBOPS :%s is remotely rehashing server config file",
			    me.name, sptr->name);
			sendto_ops
			    ("%s is remotely rehashing server config file",
			    parv[0]);
		}
		/* Then we have a hack here to parse the flags (thats the ugly part) */
		else
		{
			if (!BadPtr(parv[2]))
			{
				if (*parv[2] == '-')
				{
					if (!match("-dcc*", parv[2]))
					{
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing dccdeny.conf",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing dccdeny.conf on request of %s",
						    sptr->name);
						dcc_rehash();
						return 0;
					}
					if (!match("-dyn*", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing dynamic configuration",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing dynamic configuration on request of %s",
						    sptr->name);
						load_conf(ZCONF, 1);
						return 0;
					}
					if (!match("-rest*", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing channel restrict configuration",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing channel restrict configuration on request of %s",
						    sptr->name);
						cr_rehash();
						return 0;
					}
					if (!match("-o*motd", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing OperMOTD",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing OperMOTD on request of %s",
						    sptr->name);
						opermotd =
						    (aMotd *)
						    read_opermotd(OPATH);
						return 0;
					}
					if (!match("-b*motd", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing BotMOTD",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing BotMOTD on request of %s",
						    sptr->name);
						botmotd =
						    (aMotd *)
						    read_botmotd(BPATH);
						return 0;
					}
					if (!match("-motd*", parv[2])
					    || !match("-rules*", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing all MOTDs and RULES",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing all MOTDs and RULES on request of %s",
						    sptr->name);
						motd =
						    (aMotd *) read_motd(MPATH);
						rules =
						    (aMotd *) read_rules(RPATH);
						read_tlines();
						return 0;
					}
					if (!match("-vhos*", parv[2]))
					{
						if (!IsAdmin(sptr))
							return 0;
						sendto_serv_butone(&me,
						    ":%s GLOBOPS :%s is remotely rehashing vhost configuration",
						    me.name, sptr->name);
						sendto_ops
						    ("Remotely rehashing vhost configuration on request of %s",
						    sptr->name);
						vhost_rehash();
						return 0;
					}
				}
			}
		}

#endif
	}
	else
	{
		sendto_ops("%s is rehashing server config file", parv[0]);
	}
	sendto_one(sptr, rpl_str(RPL_REHASHING), me.name, parv[0], configfile);
#ifdef USE_SYSLOG
	syslog(LOG_INFO, "REHASH From %s\n", get_client_name(sptr, FALSE));
#endif
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
int  m_restart(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *pass = NULL, *encr;
	int  x;
#ifdef CRYPT_XLINE_PASSWORD
	char salt[3];
	extern char *crypt();
#endif
	if (MyClient(sptr) && !OPCanRestart(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (!MyClient(sptr) && !(IsTechAdmin(sptr) || IsNetAdmin(sptr))
	    && !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc > 2)
	{
		/* Remote restart. */
		if (MyClient(sptr) && !(IsNetAdmin(sptr) || IsTechAdmin(sptr)))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}

		if ((x =
		    hunt_server(cptr, sptr, ":%s RESTART %s %s :%s", 2, parc,
		    parv)) != HUNTED_ISME)
			return 0;
	}

	if (cptr != sptr)
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :%s is remotely restarting server (%s)",
		    me.name, sptr->name, parv[3]);
		sendto_ops("%s is remotely restarting IRCd (%s)", parv[0],
		    parv[3]);

	}

	if ((pass = find_restartpass()))
	{
		if (parc < 2)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name,
			    parv[0], "RESTART");
			return 0;
		}
#ifdef CRYPT_XLINE_PASSWORD
		salt[0] = pass[0];
		salt[1] = pass[1];
		salt[3] = '\0';

		encr = crypt(parv[1], salt);
#else
		encr = parv[1];
#endif
		if (strcmp(pass, encr))
		{
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
			    parv[0]);
			return 0;
		}
		/* Hack to make the code after this if { } easier: we assign the comment to the
		 * first param, as if we had not had an X:line. We do not need the password
		 * now anyways. Accordingly we decrement parc ;)    -- NikB
		 */
		parv[1] = parv[2];
		parc--;
	}

#ifdef USE_SYSLOG
	syslog(LOG_WARNING, "Server RESTART by %s - %s\n",
	    get_client_name(sptr, FALSE),
	    (!MyClient(sptr) ? (parc > 2 ? parv[3] : "No reason")
	    : (parc > 1 ? parv[2] : "No reason")));
#endif
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
int  m_trace(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	int  i;
	aClient *acptr;
	aClass *cltmp;
	char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;


	if (parc > 2)
		if (hunt_server(cptr, sptr, ":%s TRACE %s :%s", 2, parc, parv))
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

	switch (hunt_server(cptr, sptr, ":%s TRACE :%s", 1, parc, parv))
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

#ifndef _WIN32
	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;
#else
	bzero(link_s, sizeof(link_s));
	bzero(link_u, sizeof(link_u));
#endif

	if (doall)
		for (acptr = client; acptr; acptr = acptr->next)
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->fd]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || IsOper(sptr)))
				link_u[acptr->from->fd]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->fd]++;

	/* report all direct connections */

	now = TStime();
	for (i = 0; i <= highest_fd; i++)
	{
		char *name;
		int  class;

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
		class = get_client_class(acptr);

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
					      IsHidden(acptr) ? acptr->user->
					      virthost : acptr->user->realhost,
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
		  default:	/* ...we actually shouldn't come here... --msa */
			  sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
			      parv[0], name);
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
		    me.name, parv[0], 0, link_s[me.fd],
		    link_u[me.fd], me.name, "*", "*", me.name);
		return 0;
	}
	for (cltmp = FirstClass(); doall && cltmp; cltmp = NextClass(cltmp))
		if (Links(cltmp) > 0)
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
			    parv[0], Class(cltmp), Links(cltmp));
	return 0;
}


/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
int  m_motd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aTrecord *ptr;
	aMotd *temp, *temp2;
	struct tm *tm = motd_tm;
	int  svsnofile = 0;

	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;
#ifndef TLINE_Remote
	if (!MyConnect(sptr))
	{
		temp = motd;
		goto playmotd;
	}
#endif
	for (ptr = tdata; ptr; ptr = ptr->next)
	{
		if (!match(ptr->hostmask, cptr->user->realhost))
			break;
	}

	if (ptr)
	{
		temp = ptr->tmotd;
		tm = ptr->tmotd_tm;
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
int  m_opermotd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aMotd *temp;

	if (!IsAnOper(sptr))
		return 0;

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

/* read_tlines 
 * Read info from T:lines into trecords which include the file 
 * timestamp, the hostmask, and the contents of the motd file 
 * -Ghostwolf 7sep97
 * Modified for Unreal and to support RULES by codemastr
 */
void read_tlines()
{
	aConfItem *tmp;
	aTrecord *temp, *last = NULL;	/* Init. to avoid compiler warning */
	aMotd *amotd, *arules;

	/* Free the old trecords and the associated motd contents first */
	while (tdata)
	{
		last = tdata->next;
		while (tdata->tmotd)
		{
			amotd = tdata->tmotd->next;
			MyFree(tdata->tmotd->line);
			MyFree(tdata->tmotd);
			tdata->tmotd = amotd;
		}
		/* Clear rules too */
		while (tdata->trules)
		{
			arules = tdata->trules->next;
			MyFree(tdata->trules->line);
			MyFree(tdata->trules);
			tdata->trules = arules;
		}

		MyFree(tdata);
		tdata = last;
	}

	for (tmp = conf; tmp; tmp = tmp->next)
		if (tmp->status == CONF_TLINE && tmp->host && tmp->passwd)
		{
			temp = (aTrecord *) MyMalloc(sizeof(aTrecord));
			if (!temp)
				outofmemory();
			temp->hostmask = tmp->host;
			temp->tmotd = read_motd(tmp->passwd);
			temp->trules = read_rules(tmp->name);
			temp->tmotd_tm = motd_tm;
			temp->next = NULL;
			if (!tdata)
				tdata = temp;
			else
				last->next = temp;
			last = temp;
		}
}


/* 
 * A merge from ircu and bahamut, and some extra stuff added by codemastr 
 */

aMotd *read_svsmotd(char *filename)
{
	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	char line[82];
	char *tmp;
	int  i;

	if (fd == -1)
		return NULL;

	while (svsmotd)
	{
		old = svsmotd->next;
		MyFree(svsmotd->line);
		MyFree(svsmotd);
		svsmotd = old;
	}

	newmotd = last = NULL;
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */

	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
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
 * A merge from ircu and bahamut, and some extra stuff added by codemastr 
 */

aMotd *read_rules(char *filename)
{
	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	char line[82];
	char *tmp;
	int  i;

	if (fd == -1)
		return NULL;
/* If it is the default RULES, clear it -- codemastr */
	if (!stricmp(filename, RPATH))
	{
		while (rules)
		{
			old = rules->next;
			MyFree(rules->line);
			MyFree(rules);
			rules = old;
		}
	}


	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */

	newmotd = last = NULL;
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
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
 * A merge from ircu and bahamut, and some extra stuff added by codemastr 
 */

aMotd *read_motd(char *filename)
{ 
	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	struct stat sb;
	char line[82];
	char *tmp;
	int  i;
	if (fd == -1)
		return NULL;

	fstat(fd, &sb);

	/* If it is the default MOTD, clear it -- codemastr */
	if (!stricmp(filename, MPATH))
	{
		while (motd)
		{
			old = motd->next;
			MyFree(motd->line);
			MyFree(motd);
		motd = old;
		}
		/* We also wanna set it's last changed value -- codemastr */
		motd_tm = localtime(&sb.st_mtime);
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
 * A merge from ircu and bahamut, and some extra stuff added by codemastr 
 */

aMotd *read_opermotd(char *filename)
{

	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	char line[82];
	char *tmp;
	int  i;

	if (fd == -1)
		return NULL;

	while (opermotd)
	{
		old = opermotd->next;
		MyFree(opermotd->line);
		MyFree(opermotd);
		opermotd = old;
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
 * A merge from ircu and bahamut, and some extra stuff added by codemastr 
 */

aMotd *read_botmotd(char *filename)
{
	int  fd = open(filename, O_RDONLY);
	aMotd *temp, *newmotd, *last, *old;
	char line[82];
	char *tmp;
	int  i;

	if (fd == -1)
		return NULL;

	while (botmotd)
	{
		old = botmotd->next;
		
		MyFree(botmotd->line);
		MyFree(botmotd);
		botmotd = old;
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
int  m_botmotd(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aMotd *temp;
	if (hunt_server(cptr, sptr, ":%s BOTMOTD :%s", 1, parc,
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
int  m_rules(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aTrecord *ptr;
	aMotd *temp;

	if (hunt_server(cptr, sptr, ":%s RULES :%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;
#ifndef TLINE_Remote
	if (!MyConnect(sptr))
	{
		temp = rules;
		goto playrules;
	}
#endif
	for (ptr = tdata; ptr; ptr = ptr->next)
	{
		if (!match(ptr->hostmask, cptr->user->realhost))
			break;
	}

	if (ptr)
	{
		temp = ptr->trules;

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
int  m_close(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	int  i;
	int  closed = 0;


	if (!MyOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	for (i = highest_fd; i; i--)
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
	    sptr->user->username,
	    IsHidden(sptr) ? sptr->user->virthost : sptr->user->realhost,
	    closed);
	IRCstats.unknown = 0;
	return 0;
}

/* m_die, this terminates the server, and it intentionally does not
 * have a reason. If you use it you should first do a GLOBOPS and 
 * then a server notice to let everyone know what is going down...
 */
int  m_die(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	int  i;
	char *pass = NULL, *encr;

#ifdef CRYPT_XLINE_PASSWORD
	char salt[3];
	extern char *crypt();
#endif

	if (!MyClient(sptr) || !OPCanDie(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if ((pass = find_diepass()))	/* See if we have and DIE/RESTART password */
	{
		if (parc < 2)	/* And if so, require a password :) */
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name,
			    parv[0], "DIE");
			return 0;
		}

#ifdef CRYPT_XLINE_PASSWORD
		salt[0] = pass[0];
		salt[1] = pass[1];
		salt[3] = '\0';

		encr = crypt(parv[1], salt);
#else
		encr = parv[1];
#endif

		if (strcmp(pass, encr))
		{
			sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
			    parv[0]);
			return 0;
		}
	}

	/* Let the +s know what is going on */
	sendto_ops("Server Terminating by request of %s", parv[0]);

	for (i = 0; i <= highest_fd; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
			    ":%s NOTICE %s :Server Terminating. %s",
			    me.name, acptr->name, get_client_name(sptr, TRUE));
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
void dump_map(cptr, server, mask, prompt_length, length)
	aClient *cptr, *server;
	char *mask;
	int  prompt_length;
	int  length;
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


	for (lp = (Link *) return_servers(); lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->srvptr != server)
			continue;
			
		if (IsULine(acptr) && HIDE_ULINES && !IsAnOper(cptr))
			continue;
		acptr->flags |= FLAGS_MAP;
		cnt++;
	}

	for (lp = (Link *) return_servers(); lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
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
int  m_map(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	Link *lp;
	aClient *acptr;
	int  longest = strlen(me.name);


	if (parc < 2)
		parv[1] = "*";
	for (lp = (Link *) return_servers(); lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
	}
	for (acptr = client; acptr; acptr = acptr->next)

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

	for (i = 0; i <= highest_fd; i++)
	{
		if (!(acptr = local[i]))
			continue;
		if (IsClient(acptr))
			sendto_one(acptr,
			    ":%s NOTICE %s :Server Terminated by local console",
			    me.name, acptr->name);
		else if (IsServer(acptr))
			sendto_one(acptr,
			    ":%s ERROR :Terminated by local console", me.name);
	}
	(void)s_die();
	return 0;
}
#endif
