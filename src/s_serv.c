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
#include <curl/curl.h>
#endif
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
		sendto_one(sptr, "ERROR :Bogus server name (%s)", servername);
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
			sendto_locfailops("Link %s(%s) cancelled, numeric '%ld' out of range (should be 0-254)",
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
	ircd_log(LOG_SERVER, "SERVER %s", acptr->name);
	/* Taken from bahamut makes it so all servers behind a U:lined
	 * server are also U:lined, very helpful if HIDE_ULINES is on
	 */
	if (IsULine(cptr)
	    || (Find_uline(acptr->name)))
		acptr->flags |= FLAGS_ULINE;
	add_server_to_table(acptr);
	IRCstats.servers++;
	(void)find_or_add(acptr->name);
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
				"%c%s %s %s %d %ld :%s",
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
char buf[512];
	sprintf(buf, "CHANMODES=%s%s,%s%s,%s%s,%s%s",
		CHPAR1, EXPAR1, CHPAR2, EXPAR2, CHPAR3, EXPAR3, CHPAR4, EXPAR4);
#ifdef ZIP_LINKS
	if (aconf->options & CONNECT_ZIP)
	{
		sendto_one(cptr, "PROTOCTL %s ZIP %s", PROTOCTL_SERVER, buf);
	} else {
#endif
		sendto_one(cptr, "PROTOCTL %s %s", PROTOCTL_SERVER, buf);
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

	ircd_log(LOG_SERVER, "SERVER %s", cptr->name);

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

			/* Also signal to the just-linked server which
			 * servers are fully linked.
			 * Now you might ask yourself "Why don't we just
			 * assume every server you get during link phase
			 * is fully linked?", well.. there's a race condition
			 * if 2 servers link (almost) at the same time,
			 * then you would think the other one is fully linked
			 * while in fact he was not.. -- Syzop.
			 */
			if (acptr->serv->flags.synced)
			{
				sendto_one(cptr, ":%s %s", acptr->name,
					(IsToken(cptr) ? TOK_EOS : MSG_EOS));
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to uplink '%s' with src %s...",
					cptr->name, acptr->name);
#endif
			}
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
				    "%s %s %d %ld %s %s %s %lu :%s",
				    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
				    acptr->name, acptr->hopcount + 1,
				    acptr->lastnick, acptr->user->username,
				    acptr->user->realhost,
				    acptr->user->server,
				    (unsigned long)acptr->user->servicestamp, acptr->info);
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
						    ((cptr->proto & PROTO_SJB64) ?
						    "%s %s %d %B %s %s %b %lu %s %s :%s"
						    :
						    "%s %s %d %lu %s %s %b %lu %s %s :%s"),
						    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
						    acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    acptr->srvptr->serv->numeric,
						    (unsigned long)acptr->user->servicestamp,
						    (!buf || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr) && (acptr->umodes & UMODE_SETHOST)) ? acptr->user->virthost : "*"),
						    acptr->info);
					}
					else
					{
						sendto_one(cptr,
						    (cptr->proto & PROTO_SJB64 ?
						    "%s %s %d %B %s %s %s %lu %s %s :%s"
						    :
						    "%s %s %d %lu %s %s %s %lu %s %s :%s"),
						    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
						    acptr->name,
						    acptr->hopcount + 1,
						    acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    acptr->user->server,
						    (unsigned long)acptr->user->servicestamp,
						    (!buf || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr) && (acptr->umodes & UMODE_SETHOST)) ? acptr->user->virthost : "*"),
						    acptr->info);
					}
				}
				else
					sendto_one(cptr,
					    "%s %s %d %ld %s %s %s %lu %s %s :%s",
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
					    server), (unsigned long)acptr->user->servicestamp,
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

	sendto_one(cptr, "%s %i %li %i %lX 0 0 0 :%s",
	    (IsToken(cptr) ? TOK_NETINFO : MSG_NETINFO),
	    IRCstats.global_max, TStime(), UnrealProtocol,
	    CLOAK_KEYCRC,
	    ircnetwork);

	/* Send EOS (End Of Sync) to the just linked server... */
	sendto_one(cptr, ":%s %s", me.name,
		(IsToken(cptr) ? TOK_EOS : MSG_EOS));
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to justlinked '%s' with src ME...",
			cptr->name);
#endif
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
	    ("Link %s -> %s is now synced [secs: %li recv: %ld.%hu sent: %ld.%hu]",
	    cptr->name, me.name, (TStime() - endsync), sptr->receiveK,
	    sptr->receiveB, sptr->sendK, sptr->sendB);
#ifdef ZIP_LINKS
	if ((MyConnect(cptr)) && (IsZipped(cptr)) && cptr->zip->in->total_out && cptr->zip->out->total_in) {
		sendto_realops
		("Zipstats for link to %s: decompressed (in): %01lu=>%01lu (%3.1f%%), compressed (out): %01lu=>%01lu (%3.1f%%)",
			get_client_name(cptr, TRUE),
			cptr->zip->in->total_in, cptr->zip->in->total_out,
			(100.0*(float)cptr->zip->in->total_in) /(float)cptr->zip->in->total_out,
			cptr->zip->out->total_in, cptr->zip->out->total_out,
			(100.0*(float)cptr->zip->out->total_out) /(float)cptr->zip->out->total_in);
	}
#endif

	sendto_serv_butone(&me,
	    ":%s SMO o :\2(sync)\2 Link %s -> %s is now synced [secs: %li recv: %ld.%hu sent: %ld.%hu]",
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
		    ("Link %s is running Protocol u%li while we are running %d!",
		    cptr->name, protocol, UnrealProtocol);
		sendto_serv_butone(&me,
		    ":%s SMO o :\2(sync)\2 Link %s is running u%li while %s is running %d!",
		    me.name, cptr->name, protocol, me.name, UnrealProtocol);

	}
	ircsprintf(buf, "%lX", CLOAK_KEYCRC);
	if (*parv[4] != '*' && strcmp(buf, parv[4]))
	{
		sendto_realops
			("Link %s is having a DIFFERENT CLOAK KEY - %s != %s. \002YOU SHOULD CORRECT THIS ASAP\002.",
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
	sendto_one(sptr, ":%s %d %s :| * Syzop        <syzop@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * Luke         <luke@unrealircd.com>",
	    me.name, RPL_INFO, sptr->name);
	sendto_one(sptr, ":%s %d %s :| * McSkaf       <mcskaf@unrealircd.com>",
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

static void reread_motdsandrules()
{
	motd = (aMotd *) read_file_ex(MPATH, &motd, &motd_tm);
	rules = (aMotd *) read_file(RPATH, &rules);
	smotd = (aMotd *) read_file_ex(SMPATH, &smotd, &smotd_tm);
	botmotd = (aMotd *) read_file(BPATH, &botmotd);
	opermotd = (aMotd *) read_file(OPATH, &opermotd);
}

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
	}

	if (!BadPtr(parv[1]) && strcmp(parv[1], "-all"))
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
			if (!_match("-o*motd", parv[1]))
			{
				sendto_ops
				    ("%sRehashing OperMOTD on request of %s",
				    cptr != sptr ? "Remotely " : "",
				    sptr->name);
				if (cptr != sptr)
					sendto_serv_butone(&me, ":%s GLOBOPS :%s is remotely rehashing OperMOTD", me.name, sptr->name);
				opermotd = (aMotd *) read_file(OPATH, &opermotd);
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
				botmotd = (aMotd *) read_file(BPATH, &botmotd);
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
	reread_motdsandrules();
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
	char *reason = NULL;
	/* Check permissions */
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

	/* Syntax: /restart */
	if (parc == 1)
	{
		if (conf_drpass)
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name,
                            parv[0], "RESTART");
                        return 0;
		}
	}
	else if (parc == 2)
	{
		/* Syntax: /restart <pass> */
		if (conf_drpass)
		{
			int ret;
			ret = Auth_Check(cptr, conf_drpass->restartauth, parv[1]);
			if (ret == -1)
			{
				sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
					   parv[0]);
				return 0;
			}
			if (ret < 1)
				return 0;
		}
		/* Syntax: /rehash <reason> */
		else 
			reason = parv[1];
	}
	else if (parc == 3)
	{
		/* Syntax: /restart <pass> <reason> */
		if (conf_drpass)
		{
			int ret;
			ret = Auth_Check(cptr, conf_drpass->restartauth, parv[1]);
			if (ret == -1)
			{
				sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name,
					   parv[0]);
				return 0;
			}
			if (ret < 1)
				return 0;
		}
		reason = parv[2];
	}
	sendto_ops("Server is Restarting by request of %s", parv[0]);
	server_reboot(reason ? reason : "No reason");
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
		sendto_one(sptr, ":%s NOTICE %s :BOTMOTD File not found",
		    me.name, sptr->name);
		return 0;
	}
	sendto_one(sptr, ":%s NOTICE %s :- %s Bot Message of the Day - ",
	    me.name, sptr->name, me.name);

	temp = botmotd;
	while (temp)
	{
		sendto_one(sptr, ":%s NOTICE %s :- %s", me.name, sptr->name, temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, ":%s NOTICE %s :End of /BOTMOTD command.", me.name, sptr->name);
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

/*
 * EOS (End Of Sync) command.
 * Type: Broadcast
 * Purpose: Broadcasted over a network if a server is synced (after the users, channels,
 *          etc are introduced). Makes us able to know if a server is linked.
 * History: Added in beta18 (in cvs since 2003-08-11) by Syzop
 */
CMD_FUNC(m_eos)
{
	if (!IsServer(sptr))
		return 0;
	sptr->serv->flags.synced = 1;
	/* pass it on ^_- */
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] m_eos: got sync from %s (path:%s)", sptr->name, cptr->name);
	ircd_log(LOG_ERROR, "[EOSDBG] m_eos: broadcasting it back to everyone except route from %s", cptr->name);
#endif
	sendto_serv_butone_token(cptr,
		parv[0], MSG_EOS, TOK_EOS, "", NULL);
	return 0;
}
