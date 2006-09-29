/*
 *   IRC - Internet Relay Chat, src/modules/out.c
 *   (C) 2004 The UnrealIRCd Team
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#include "version.h"

void send_channel_modes(aClient *cptr, aChannel *chptr);
void send_channel_modes_sjoin(aClient *cptr, aChannel *chptr);
void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr);
DLLFUNC int m_server(aClient *cptr, aClient *sptr, int parc, char *parv[]);

static char buf[BUFSIZE];


#define MSG_SERVER 	"SERVER"	
#define TOK_SERVER 	"'"	

ModuleHeader MOD_HEADER(m_server)
  = {
	"m_server",
	"$Id$",
	"command /server", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_server)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_SERVER, TOK_SERVER, m_server, MAXPARA, M_UNREGISTERED|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_server)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_server)(int module_unload)
{
	if (del_Command(MSG_SERVER, TOK_SERVER, m_server) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_server).name);
	}
	return MOD_SUCCESS;
}

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
DLLFUNC CMD_FUNC(m_server)
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
	char *flags = NULL, *protocol = NULL, *inf = NULL, *num = NULL;


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
		if (cptr->serv && cptr->serv->conf)
		{
			/* We already know what block we are dealing with (outgoing connect!) */
			link = cptr->serv->conf;
		} else {
			/* Hunt the linkblock down ;) */
			for(link = conf_link; link; link = (ConfigItem_link *) link->next)
				if (!match(link->servername, servername))
					break;
		}
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
		if ((bconf = Find_ban(NULL, servername, CONF_BAN_SERVER)))
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
	if ((bconf = Find_ban(NULL, servername, CONF_BAN_SERVER)))
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
	if (IsULine(sptr)
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

int	m_server_synch(aClient *cptr, long numeric, ConfigItem_link *aconf)
{
	char		*inpath = get_client_name(cptr, TRUE);
	extern MODVAR char 	serveropts[];
	aClient		*acptr;
	int		i;
	char buf[BUFSIZE];
	int incoming = IsUnknown(cptr) ? 1 : 0;

	ircd_log(LOG_SERVER, "SERVER %s", cptr->name);

	if (cptr->passwd)
	{
		MyFree(cptr->passwd);
		cptr->passwd = NULL;
	}
	if (incoming)
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
#ifndef NEW_IO
	addto_fdlist(cptr->slot, &serv_fdlist);
#else /* ifndef NEW_IO */
#endif /* ifndef NEW_IO */
	if ((Find_uline(cptr->name)))
		cptr->flags |= FLAGS_ULINE;
	nextping = TStime();
	(void)find_or_add(cptr->name);
#ifdef USE_SSL
	if (IsSecure(cptr))
	{
		sendto_serv_butone_token(&me, me.name, MSG_SMO, TOK_SMO, "o :(\2link\2) Secure %slink %s -> %s established (%s)",
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath, (char *) ssl_get_cipher((SSL *)cptr->ssl));
		sendto_realops("(\2link\2) Secure %slink %s -> %s established (%s)",
			IsZipped(cptr) ? "ZIP" : "",
			me.name, inpath, (char *) ssl_get_cipher((SSL *)cptr->ssl));
	}
	else
#endif
	{
		sendto_serv_butone_token(&me, me.name, MSG_SMO, TOK_SMO, "o :(\2link\2) %sLink %s -> %s established",
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
	if (incoming)
	{
		cptr->serv->conf->refcount++;
		Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
			cptr->name, cptr->serv->conf->servername, cptr->serv->conf->refcount));
	}
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
						    "%s %s %d %B %s %s %b %lu %s %s %s%s%s%s:%s"
						    :
						    "%s %s %d %lu %s %s %b %lu %s %s %s%s%s%s:%s"),
						    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
						    acptr->name,
						    acptr->hopcount + 1,
						    (long)acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    (long)(acptr->srvptr->serv->numeric),
						    (unsigned long)acptr->user->servicestamp,
						    (!buf || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr) && (acptr->umodes & UMODE_SETHOST)) ? acptr->user->virthost : "*"),
						    SupportCLK(cptr) ? getcloak(acptr) : "",
						    SupportCLK(cptr) ? " " : "",
						    SupportNICKIP(cptr) ? encode_ip(acptr->user->ip_str) : "",
					        SupportNICKIP(cptr) ? " " : "",
					        acptr->info);
					}
					else
					{
						sendto_one(cptr,
						    (cptr->proto & PROTO_SJB64 ?
						    "%s %s %d %B %s %s %s %lu %s %s %s%s%s%s:%s"
						    :
						    "%s %s %d %lu %s %s %s %lu %s %s %s%s%s%s:%s"),
						    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
						    acptr->name,
						    acptr->hopcount + 1,
						    (long)acptr->lastnick,
						    acptr->user->username,
						    acptr->user->realhost,
						    acptr->user->server,
						    (unsigned long)acptr->user->servicestamp,
						    (!buf || *buf == '\0' ? "+" : buf),
						    ((IsHidden(acptr) && (acptr->umodes & UMODE_SETHOST)) ? acptr->user->virthost : "*"),
						    SupportCLK(cptr) ? getcloak(acptr) : "",
						    SupportCLK(cptr) ? " " : "",
						    SupportNICKIP(cptr) ? encode_ip(acptr->user->ip_str) : "",
					        SupportNICKIP(cptr) ? " " : "",
					        acptr->info);
					}
				}
				else
					sendto_one(cptr,
					    "%s %s %d %ld %s %s %s %lu %s %s %s%s:%s",
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
					    SupportNICKIP(cptr) ? encode_ip(acptr->user->ip_str) : "",
				            SupportNICKIP(cptr) ? " " : "", acptr->info);
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
				    (long)chptr->topic_time, chptr->topic);
		}
	}
	/* pass on TKLs */
	tkl_synch(cptr);

	/* send out SVSFLINEs */
	dcc_sync(cptr);

	sendto_one(cptr, "%s %i %li %i %s 0 0 0 :%s",
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

static int send_mode_list(aClient *cptr, char *chname, TS creationtime, Member *top, int mask, char flag)
{
	Member *lp;
	char *cp, *name;
	int  count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)		/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	{
		/* 
		 * Okay, since ban's are stored in their own linked
		 * list, we won't even bother to check if CHFL_BAN
		 * is set in the flags. This should work as long
		 * as only ban-lists are feed in with CHFL_BAN mask.
		 * However, we still need to typecast... -Donwulff 
		 */
		if ((mask == CHFL_BAN) || (mask == CHFL_EXCEPT) || (mask == CHFL_INVEX))
		{
/*			if (!(((Ban *)lp)->flags & mask)) continue; */
			name = ((Ban *) lp)->banstr;
		}
		else
		{
			if (!(lp->flags & mask))
				continue;
			name = lp->cptr->name;
		}
		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strlcat(parabuf, " ", sizeof parabuf);
			(void)strlcat(parabuf, name, sizeof parabuf);
			count++;
			*cp++ = flag;
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == RESYNCMODES)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendmodeto_one(cptr, me.name, chname, modebuf,
			    parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != RESYNCMODES)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = flag;
			}
			count = 0;
			*cp = '\0';
		}
	}
	return sent;
}

/* A little kludge to prevent sending double spaces -- codemastr */
static inline void send_channel_mode(aClient *cptr, char *from, aChannel *chptr)
{
	if (*parabuf)
		sendto_one(cptr, ":%s %s %s %s %s %lu", from,
			(IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
			modebuf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s %s %s %s %lu", from,
			(IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
			modebuf, chptr->creationtime);
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void send_channel_modes(aClient *cptr, aChannel *chptr)
{
	int  sent;
/* fixed a bit .. to fit halfops --sts */
	if (*chptr->chname != '#')
		return;

	*parabuf = '\0';
	*modebuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);
	sent = send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOP, 'o');
	if (!sent && chptr->creationtime)
		send_channel_mode(cptr, me.name, chptr);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';

	sent = send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_HALFOP, 'h');
	if (!sent && chptr->creationtime)
		send_channel_mode(cptr, me.name, chptr);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->exlist, CHFL_EXCEPT, 'e');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->invexlist, CHFL_INVEX, 'I');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_VOICE, 'v');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOWNER, 'q');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANPROT, 'a');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);
}


static int send_ban_list(aClient *cptr, char *chname, TS creationtime, aChannel *channel)
{
	Ban *top;

	Ban *lp;
	char *cp, *name;
	int  count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)		/* mode +l or +k xx */
		count = 1;
	top = channel->banlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'b';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'b';
			}
			count = 0;
			*cp = '\0';
		}
	}
	top = channel->exlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'e';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'e';
			}
			count = 0;
			*cp = '\0';
		}
	}
	top = channel->invexlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'I';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'I';
			}
			count = 0;
			*cp = '\0';
		}
	}
	return sent;
}


/* 
 * This will send "cptr" a full list of the modes for channel chptr,
 */

void send_channel_modes_sjoin(aClient *cptr, aChannel *chptr)
{

	Member *members;
	Member *lp;
	char *name;
	char *bufptr;

	int  n = 0;

	if (*chptr->chname != '#')
		return;

	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (*parabuf)
	{
	}
	else
	{
		if (!SupportSJOIN2(cptr))
			strlcpy(parabuf, "<none>", sizeof parabuf);
		else
			strlcpy(parabuf, "<->", sizeof parabuf);
	}
	ircsprintf(buf, "%s %ld %s %s %s :",
	    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
	    chptr->creationtime, chptr->chname, modebuf, parabuf);

	bufptr = buf + strlen(buf);

	for (lp = members; lp; lp = lp->next)
	{

		if (lp->flags & MODE_CHANOP)
			*bufptr++ = '@';

		if (lp->flags & MODE_VOICE)
			*bufptr++ = '+';

		if (lp->flags & MODE_HALFOP)
			*bufptr++ = '%';
		if (lp->flags & MODE_CHANOWNER)
			*bufptr++ = '*';
		if (lp->flags & MODE_CHANPROT)
			*bufptr++ = '~';



		name = lp->cptr->name;

		strcpy(bufptr, name);
		bufptr += strlen(bufptr);
		*bufptr++ = ' ';
		n++;

		if (bufptr - buf > BUFSIZE - 80)
		{
			*bufptr++ = '\0';
			if (bufptr[-1] == ' ')
				bufptr[-1] = '\0';
			sendto_one(cptr, "%s", buf);

			ircsprintf(buf, "%s %ld %s %s %s :",
			    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
			    chptr->creationtime, chptr->chname, modebuf,
			    parabuf);
			n = 0;

			bufptr = buf + strlen(buf);
		}
	}
	if (n)
	{
		*bufptr++ = '\0';
		if (bufptr[-1] == ' ')
			bufptr[-1] = '\0';
		sendto_one(cptr, "%s", buf);
	}
	/* Then we'll send the ban-list */

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_ban_list(cptr, chptr->chname, chptr->creationtime, chptr);

	if (modebuf[1] || *parabuf)
		sendto_one(cptr, "%s %s %s %s %lu",
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	return;
}

char *mystpcpy(char *dst, const char *src)
{
	for (; *src; src++)
		*dst++ = *src;
	*dst = '\0';
	return dst;
}



/** This will send "cptr" a full list of the modes for channel chptr,
 *
 * Half of it recoded by Syzop: the whole buffering and size checking stuff
 * looked weird and just plain inefficient. We now fill up our send-buffer
 * really as much as we can, without causing any overflows of course.
 */
void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr)
{
	Member *members;
	Member *lp;
	Ban *ban;
	char *name;
	short nomode, nopara;
	char tbuf[512]; /* work buffer, for temporary data */
	char buf[1024]; /* send buffer */
	char *bufptr; /* points somewhere in 'buf' */
	char *p; /* points to somewhere in 'tbuf' */
	int prebuflen = 0; /* points to after the <sjointoken> <TS> <chan> <fixmodes> <fixparas <..>> : part */

	if (*chptr->chname != '#')
		return;

	nomode = 0;
	nopara = 0;
	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (!modebuf[1])
		nomode = 1;
	if (!(*parabuf))
		nopara = 1;


	if (nomode && nopara)
	{
		ircsprintf(buf,
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s :" : "%s %ld %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname);
	}
	if (nopara && !nomode)
	{
		ircsprintf(buf, 
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s :" : "%s %ld %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsprintf(buf,
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s %s :" : "%s %ld %s %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname, modebuf, parabuf);
	}

	prebuflen = strlen(buf);
	bufptr = buf + prebuflen;

	/* RULES:
	 * - Use 'tbuf' as a working buffer, use 'p' to advance in 'tbuf'.
	 *   Thus, be sure to do a 'p = tbuf' at the top of the loop.
	 * - When one entry has been build, check if strlen(buf) + strlen(tbuf) > BUFSIZE - 8,
	 *   if so, do not concat but send the current result (buf) first to the server
	 *   and reset 'buf' to only the prebuf part (all until the ':').
	 *   Then, in both cases, concat 'tbuf' to 'buf' and continue
	 * - Be sure to ALWAYS zero terminate (*p = '\0') when the entry has been build.
	 * - Be sure to add a space after each entry ;)
	 *
	 * For a more illustrated view, take a look at the first for loop, the others
	 * are pretty much the same.
	 *
	 * Follow these rules, and things would be smooth and efficient (network-wise),
	 * if you ignore them, expect crashes and/or heap corruption, aka: HELL.
	 * You have been warned.
	 *
	 * Side note: of course things would be more efficient if the prebuf thing would
	 * not be sent every time, but that's another story
	 *      -- Syzop
	 */

	for (lp = members; lp; lp = lp->next)
	{
		p = tbuf;
		if (lp->flags & MODE_CHANOP)
			*p++ = '@';
		if (lp->flags & MODE_VOICE)
			*p++ = '+';
		if (lp->flags & MODE_HALFOP)
			*p++ = '%';
		if (lp->flags & MODE_CHANOWNER)
			*p++ = '*';
		if (lp->flags & MODE_CHANPROT)
			*p++ = '~';

		p = mystpcpy(p, lp->cptr->name);
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->banlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '&';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->exlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '"';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->invexlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '\'';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	if (buf[prebuflen])
		sendto_one(cptr, "%s", buf);
}
