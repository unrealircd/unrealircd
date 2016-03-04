/*
 *   IRC - Internet Relay Chat, src/modules/m_server.c
 *   (C) 2004-present The UnrealIRCd Team
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

#include "unrealircd.h"

void send_channel_modes(aClient *cptr, aChannel *chptr);
void send_channel_modes_sjoin(aClient *cptr, aChannel *chptr);
void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr);
CMD_FUNC(m_server);
CMD_FUNC(m_server_remote);
int _verify_link(aClient *cptr, aClient *sptr, char *servername, ConfigItem_link **link_out);
void _send_protoctl_servers(aClient *sptr, int response);
void _send_server_message(aClient *sptr);
void _introduce_user(aClient *to, aClient *acptr);
int _check_deny_version(aClient *cptr, char *version_string, int protocol, char *flags);

static char buf[BUFSIZE];


#define MSG_SERVER 	"SERVER"	

ModuleHeader MOD_HEADER(m_server)
  = {
	"m_server",
	"4.0",
	"command /server", 
	"3.2-b8-1",
	NULL 
    };

MOD_TEST(m_server)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_PROTOCTL_SERVERS, _send_protoctl_servers);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_SERVER_MESSAGE, _send_server_message);
	EfunctionAdd(modinfo->handle, EFUNC_VERIFY_LINK, _verify_link);
	EfunctionAddVoid(modinfo->handle, EFUNC_INTRODUCE_USER, _introduce_user);
	EfunctionAdd(modinfo->handle, EFUNC_CHECK_DENY_VERSION, _check_deny_version);
	return MOD_SUCCESS;
}

MOD_INIT(m_server)
{
	CommandAdd(modinfo->handle, MSG_SERVER, m_server, MAXPARA, M_UNREGISTERED|M_SERVER);
	CommandAdd(modinfo->handle, "SID", m_server_remote, MAXPARA, M_SERVER);

	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

MOD_LOAD(m_server)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_server)
{
	return MOD_SUCCESS;
}

int m_server_synch(aClient *cptr, ConfigItem_link *conf);

/** Check deny version { } blocks.
 * NOTE: cptr will always be valid, but all the other values may be NULL or 0 !!!
 */
int _check_deny_version(aClient *cptr, char *version_string, int protocol, char *flags)
{
	ConfigItem_deny_version *vlines;
	
	for (vlines = conf_deny_version; vlines; vlines = (ConfigItem_deny_version *) vlines->next)
	{
		if (!match(vlines->mask, cptr->name))
			break;
	}
	
	if (vlines)
	{
		char *proto = vlines->version;
		char *vflags = vlines->flags;
		int result = 0, i;
		switch (*proto)
		{
			case '<':
				proto++;
				if (protocol < atoi(proto))
					result = 1;
				break;
			case '>':
				proto++;
				if (protocol > atoi(proto))
					result = 1;
				break;
			case '=':
				proto++;
				if (protocol == atoi(proto))
					result = 1;
				break;
			case '!':
				proto++;
				if (protocol != atoi(proto))
					result = 1;
				break;
			default:
				if (protocol == atoi(proto))
					result = 1;
				break;
		}
		if (protocol == 0 || *proto == '*')
			result = 0;

		if (result)
			return exit_client(cptr, cptr, cptr, "Denied by deny version { } block");

		if (flags)
		{
			for (i = 0; vflags[i]; i++)
			{
				if (vflags[i] == '!')
				{
					i++;
					if (strchr(flags, vflags[i])) {
						result = 1;
						break;
					}
				}
				else if (!strchr(flags, vflags[i]))
				{
						result = 1;
						break;
				}
			}

			if (*vflags == '*' || !strcmp(flags, "0"))
				result = 0;
		}

		if (result)
			return exit_client(cptr, cptr, cptr, "Denied by deny version { } block");
	}
	
	return 0;
}

/** Send our PROTOCTL SERVERS=x,x,x,x stuff.
 * When response is set, it will be PROTOCTL SERVERS=*x,x,x (mind the asterisk).
 */
void _send_protoctl_servers(aClient *sptr, int response)
{
	char buf[512];
	aClient *acptr;

	if (!NEW_LINKING_PROTOCOL)
		return;

	sendto_one(sptr, "PROTOCTL EAUTH=%s,%d,%s%s,%s",
		me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", version);
		
	ircsnprintf(buf, sizeof(buf), "PROTOCTL SERVERS=%s", response ? "*" : "");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (*acptr->id)
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%s,", acptr->id);
		if (strlen(buf) > sizeof(buf)-12)
			break; /* prevent overflow/cutoff if you have a network with more than 90 servers or something. */
	}
	
	/* Remove final comma (if any) */
	if (buf[strlen(buf)-1] == ',')
		buf[strlen(buf)-1] = '\0';

	sendto_one(sptr, "%s", buf);
}

void _send_server_message(aClient *sptr)
{
	if (sptr->serv && sptr->serv->flags.server_sent)
	{
#ifdef DEBUGMODE
		abort();
#endif
		return;
	}

	if (1) /* SupportVL(sptr)) -- always send like 3.2.x for now. */
	{
		sendto_one(sptr, "SERVER %s 1 :U%d-%s%s-%s %s",
			me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", me.id, me.info);
	} else {
		sendto_one(sptr, "SERVER %s 1 :%s",
			me.name, me.info);
	}

	if (sptr->serv)
		sptr->serv->flags.server_sent = 1;
}


/** Verify server link.
 * This does authentication and authorization checks.
 * @param cptr The client directly connected to us (cptr).
 * @param sptr The client which (originally) issued the server command (sptr).
 * @param servername The server name provided by the client.
 * @param link_out Pointer-to-pointer-to-link block. Will be set when auth OK. Caller may pass NULL if he doesn't care.
 * @returns This function returns 0 on succesful auth, other values should be returned by
 *          the calling function, as it will always be FLUSH_BUFFER due to exit_client().
 */
int _verify_link(aClient *cptr, aClient *sptr, char *servername, ConfigItem_link **link_out)
{
char xerrmsg[256];
ConfigItem_link *link;
char *inpath = get_client_name(cptr, TRUE);
aClient *acptr = NULL, *ocptr = NULL;
ConfigItem_ban *bconf;

	/* We set the sockhost here so you can have incoming masks based on hostnames.
	 * Perhaps a bit late to do it here, but does anyone care?
	 */
	if (cptr->local->hostp && cptr->local->hostp->h_name)
		set_sockhost(cptr, cptr->local->hostp->h_name);

	if (link_out)
		*link_out = NULL;
	
	strcpy(xerrmsg, "No matching link configuration");

	if (!cptr->local->passwd)
	{
		sendto_one(cptr, "ERROR :Missing password");
		return exit_client(cptr, sptr, &me, "Missing password");
	}


	/* First check if the server is in the list */
	if (!servername) {
		strcpy(xerrmsg, "Null servername");
		goto errlink;
	}
	
	if (cptr->serv && cptr->serv->conf)
	{
		/* We already know what block we are dealing with (outgoing connect!) */
		/* TODO: validate this comment ^ !! */
		link = cptr->serv->conf;
		goto skip_host_check;
	} else {
		/* Hunt the linkblock down ;) */
		for(link = conf_link; link; link = (ConfigItem_link *) link->next)
			if (!match(link->servername, servername))
				break;
	}
	
	if (!link)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "No link block named '%s'", servername);
		goto errlink;
	}
	
	if (!link->incoming.mask)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "Link block '%s' exists but has no link::incoming::mask", servername);
		goto errlink;
	}

	link = Find_link(servername, cptr);

	if (!link)
	{
		ircsnprintf(xerrmsg, sizeof(xerrmsg), "Server is in link block but link::incoming::mask didn't match");
errlink:
		/* Send the "simple" error msg to the server */
		sendto_one(cptr,
		    "ERROR :Link denied (No link block found named '%s' or link::incoming::mask did not match your IP %s) %s",
		    servername, GetIP(cptr), inpath);
		/* And send the "verbose" error msg only to locally connected ircops */
		sendto_umode(UMODE_OPER, "Link denied for %s(%s@%s) (%s) %s",
		    servername, cptr->username, cptr->local->sockhost, xerrmsg, inpath);
		return exit_client(cptr, sptr, &me,
		    "Link denied (No link block found with your server name or link::incoming::mask did not match)");
	}

skip_host_check:
	/* Now for checking passwords */
	if (Auth_Check(cptr, link->auth, cptr->local->passwd) == -1)
	{
		sendto_one(cptr,
		    "ERROR :Link '%s' denied (Authentication failed) %s",
		    servername, inpath);
		sendto_umode(UMODE_OPER, "Link denied for '%s' (Authentication failed [Bad password?]) %s",
			servername, inpath);
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

		if (IsMe(acptr))
		{
			sendto_realops("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(sptr, TRUE), me.name);
			sendto_one(sptr, "ERROR: Server %s exists (it's me!)", me.name);
			return exit_client(sptr, sptr, sptr, "Server Exists");
		}

		acptr = acptr->from;
		ocptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? acptr : cptr;
		acptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? cptr : acptr;
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
	if (link->class->clients + 1 > link->class->maxclients)
	{
		sendto_realops
			("Cancelling link %s, full class",
				get_client_name(cptr, TRUE));
		return exit_client(cptr, cptr, &me, "Full class");
	}
	if (link_out)
		*link_out = link;
	return 0;
}

/*
** m_sid
**      parv[1] = servername
**      parv[2] = hopcount
**      parv[3] = sid
**      parv[4] = serverinfo
*/

/*
** m_server
**	parv[1] = servername
**      parv[2] = hopcount
**      parv[3] = numeric { ignored }
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
	char descbuf[BUFSIZE];
	char *inpath = get_client_name(cptr, TRUE);
	int  hop = 0;
	char info[REALLEN + 61];
	ConfigItem_link *aconf = NULL;
	ConfigItem_deny_link *deny;
	char *flags = NULL, *protocol = NULL, *inf = NULL, *num = NULL;


	/* Ignore it  */
	if (IsPerson(sptr))
	{
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, sptr->name);
		sendnotice(cptr,
		    "*** Sorry, but your IRC program doesn't appear to support changing servers.");
		sptr->local->since += 7;
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

	if (IsUnknown(cptr) && (cptr->local->listener->options & LISTENER_CLIENTSONLY))
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

	if ((IsUnknown(cptr) || IsHandshake(cptr)) && !cptr->local->passwd)
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
		int ret;
		ret = verify_link(cptr, sptr, servername, &aconf);
		if (ret < 0)
			return ret; /* FLUSH_BUFFER / failure */
			
		/* OK, let us check in the data now now */
		hop = atol(parv[2]);
		strlcpy(info, parv[parc - 1], sizeof(info));
		strlcpy(cptr->name, servername, sizeof(cptr->name));
		cptr->hopcount = hop;
		/* Add ban server stuff */
		if (SupportVL(cptr))
		{
			char tmp[REALLEN + 61];
			inf = protocol = flags = num = NULL;
			strlcpy(tmp, info, sizeof(tmp)); /* work on a copy */

			/* we also have a fail safe incase they say they are sending
			 * VL stuff and don't -- codemastr
			 */

			protocol = strtok(tmp, "-");
			if (protocol)
				flags = strtok(NULL, "-");
			if (flags)
				num = strtok(NULL, " ");
			if (num)
				inf = strtok(NULL, "");
			if (inf)
			{
				int ret;
				
				strlcpy(cptr->info, inf[0] ? inf : "server", sizeof(cptr->info)); /* set real description */
				
				ret = _check_deny_version(cptr, NULL, atoi(protocol), flags);
				if (ret < 0)
					return ret;
			} else {
				strlcpy(cptr->info, info[0] ? info : "server", sizeof(cptr->info));
			}
		} else {
			strlcpy(cptr->info, info[0] ? info : "server", sizeof(cptr->info));
		}

		for (deny = conf_deny_link; deny; deny = (ConfigItem_deny_link *) deny->next)
		{
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

		ircsnprintf(descbuf, sizeof descbuf, "Server: %s", servername);
		fd_desc(cptr->fd, descbuf);

		/* Start synch now */
		if (m_server_synch(cptr, aconf) == FLUSH_BUFFER)
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
	char	*servername = parv[1];

	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */

		if (IsMe(acptr))
		{
			sendto_realops("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(sptr, TRUE), me.name);
			sendto_one(sptr, "ERROR: Server %s exists (it's me!)", me.name);
			return exit_client(sptr, sptr, sptr, "Server Exists");
		}

		acptr = acptr->from;
		ocptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? acptr : cptr;
		acptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? cptr : acptr;
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
	hop = atol(parv[2]);
	strlcpy(info, parv[parc - 1], sizeof(info));
	if (!cptr->serv->conf)
	{
		sendto_realops("Lost conf for %s!!, dropping link", cptr->name);
		return exit_client(cptr, cptr, cptr, "Lost configuration");
	}
	aconf = cptr->serv->conf;
	if (!aconf->hub)
	{
		sendto_umode(UMODE_OPER, "Link %s cancelled, is Non-Hub but introduced Leaf %s",
			cptr->name, servername);
		return exit_client(cptr, cptr, cptr, "Non-Hub Link");
	}
	if (match(aconf->hub, servername))
	{
		sendto_umode(UMODE_OPER, "Link %s cancelled, linked in %s, which hub config disallows",
			cptr->name, servername);
		return exit_client(cptr, cptr, cptr, "Not matching hub configuration");
	}
	if (aconf->leaf)
	{
		if (match(aconf->leaf, servername))
		{
			sendto_umode(UMODE_OPER, "Link %s(%s) cancelled, disallowed by leaf configuration",
				cptr->name, servername);
			return exit_client(cptr, cptr, cptr, "Disallowed by leaf configuration");
		}
	}
	if (aconf->leaf_depth && (hop > aconf->leaf_depth))
	{
			sendto_umode(UMODE_OPER, "Link %s(%s) cancelled, too deep depth",
				cptr->name, servername);
			return exit_client(cptr, cptr, cptr, "Too deep link depth (leaf)");
	}
	acptr = make_client(cptr, find_server(sptr->name, cptr));
	(void)make_server(acptr);
	acptr->hopcount = hop;

	strlcpy(acptr->name, servername, sizeof(acptr->name));
	strlcpy(acptr->info, info, sizeof(acptr->info));

	if (isdigit(*parv[3]) && parc > 4)
		strlcpy(acptr->id, parv[3], sizeof(acptr->id));

	acptr->serv->up = find_or_add(acptr->srvptr->name);
	SetServer(acptr);
	ircd_log(LOG_SERVER, "SERVER %s (from %s)", acptr->name, acptr->srvptr->name);
	/* Taken from bahamut makes it so all servers behind a U:lined
	 * server are also U:lined, very helpful if HIDE_ULINES is on
	 */
	if (IsULine(sptr)
	    || (Find_uline(acptr->name)))
		acptr->flags |= FLAGS_ULINE;
	IRCstats.servers++;
	(void)find_or_add(acptr->name);
	add_client_to_list(acptr);
	(void)add_to_client_hash_table(acptr->name, acptr);

	if (*acptr->id)
		add_to_id_hash_table(acptr->id, acptr);

	list_move(&acptr->client_node, &global_server_list);
	RunHook(HOOKTYPE_SERVER_CONNECT, acptr);

	if (*acptr->id)
	{
		sendto_server(cptr, PROTO_SID, 0, ":%s SID %s %d %s :%s",
			    acptr->srvptr->id, acptr->name, hop + 1, acptr->id, acptr->info);
		sendto_server(cptr, 0, PROTO_SID, ":%s SERVER %s %d :%s",
				acptr->srvptr->name,
				acptr->name, hop + 1, acptr->info);
	} else {
		sendto_server(cptr, 0, 0, ":%s SERVER %s %d :%s",
				acptr->srvptr->name,
				acptr->name, hop + 1, acptr->info);
	}

	RunHook(HOOKTYPE_POST_SERVER_CONNECT, acptr);
	return 0;
}

void _introduce_user(aClient *to, aClient *acptr)
{
	send_umode(NULL, acptr, 0, SEND_UMODES, buf);

	sendto_one_nickcmd(to, acptr, buf);
	
	send_moddata_client(to, acptr);

	if (acptr->user->away)
		sendto_one(to, ":%s AWAY :%s", CHECKPROTO(to, PROTO_SID) ? ID(acptr) : acptr->name,
			acptr->user->away);

	if (acptr->user->swhois)
	{
		SWhois *s;
		for (s = acptr->user->swhois; s; s = s->next)
		{
			if (CHECKPROTO(to, PROTO_EXTSWHOIS))
			{
				sendto_one(to, ":%s SWHOIS %s + %s %d :%s",
					me.name, acptr->name, s->setby, s->priority, s->line);
			} else
			{
				sendto_one(to, ":%s SWHOIS %s :%s",
					me.name, acptr->name, s->line);
			}
		}
	}
}

int	m_server_synch(aClient *cptr, ConfigItem_link *aconf)
{
	char		*inpath = get_client_name(cptr, TRUE);
	aClient		*acptr;
	char buf[BUFSIZE];
	int incoming = IsUnknown(cptr) ? 1 : 0;

	ircd_log(LOG_SERVER, "SERVER %s", cptr->name);

	if (cptr->local->passwd)
	{
		MyFree(cptr->local->passwd);
		cptr->local->passwd = NULL;
	}
	if (incoming)
	{
		/* If this is an incomming connection, then we have just received
		 * their stuff and now send our stuff back.
		 */
		if (!IsEAuth(cptr)) /* if eauth'd then we already sent the passwd */
			sendto_one(cptr, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");

		send_proto(cptr, aconf);
		send_server_message(cptr);
	}

	/* Set up server structure */
	free_pending_net(cptr);
	SetServer(cptr);
	IRCstats.me_servers++;
	IRCstats.servers++;
	IRCstats.unknown--;
	list_move(&cptr->client_node, &global_server_list);
	list_move(&cptr->lclient_node, &lclient_list);
	list_add(&cptr->special_node, &server_list);
	if ((Find_uline(cptr->name)))
		cptr->flags |= FLAGS_ULINE;
	(void)find_or_add(cptr->name);
	if (IsSecure(cptr))
	{
		sendto_server(&me, 0, 0, ":%s SMO o :(\2link\2) Secure link %s -> %s established (%s)",
			me.name,
			me.name, inpath, (char *) ssl_get_cipher(cptr->local->ssl));
		sendto_realops("(\2link\2) Secure link %s -> %s established (%s)",
			me.name, inpath, (char *) ssl_get_cipher(cptr->local->ssl));
	}
	else
	{
		sendto_server(&me, 0, 0, ":%s SMO o :(\2link\2) Link %s -> %s established",
			me.name,
			me.name, inpath);
		sendto_realops("(\2link\2) Link %s -> %s established",
			me.name, inpath);
		/* Print out a warning if linking to a non-SSL server unless it's localhost.
		 * Yeah.. there are still other cases when non-SSL links are fine (eg: local IP
		 * of the same machine), we won't bother with detecting that. -- Syzop
		 */
		if (!IsLocal(cptr))
		{
			sendto_realops("\002WARNING:\002 This link is unencrypted (non-SSL). We highly recommend to use "
						   "SSL server linking. See https://www.unrealircd.org/docs/Linking_servers");
		}
	}
	(void)add_to_client_hash_table(cptr->name, cptr);
	/* doesnt duplicate cptr->serv if allocted this struct already */
	(void)make_server(cptr);
	cptr->serv->up = me.name;
	cptr->srvptr = &me;
	if (!cptr->serv->conf)
		cptr->serv->conf = aconf; /* Only set serv->conf to aconf if not set already! Bug #0003913 */
	if (incoming)
	{
		cptr->serv->conf->refcount++;
		Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
			cptr->name, cptr->serv->conf->servername, cptr->serv->conf->refcount));
	}
	cptr->serv->conf->class->clients++;
	cptr->local->class = cptr->serv->conf->class;
	RunHook(HOOKTYPE_SERVER_CONNECT, cptr);

	if (*cptr->id)
	{
		sendto_server(cptr, PROTO_SID, 0, ":%s SID %s 2 %s :%s",
			    cptr->srvptr->id, cptr->name, cptr->id, cptr->info);
	}

	sendto_server(cptr, 0, *cptr->id ? PROTO_SID : 0, ":%s SERVER %s 2 :%s",
		    cptr->serv->up,
		    cptr->name, cptr->info);

	list_for_each_entry_reverse(acptr, &global_server_list, client_node)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;

		if (IsServer(acptr))
		{
			if (SupportSID(cptr) && *acptr->id)
			{
				sendto_one(cptr, ":%s SID %s %d %s :%s",
				    acptr->srvptr->id,
				    acptr->name, acptr->hopcount + 1,
				    acptr->id, acptr->info);
			}
			else
				sendto_one(cptr, ":%s SERVER %s %d :%s",
				    acptr->serv->up,
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
				sendto_one(cptr, ":%s EOS", CHECKPROTO(cptr, PROTO_SID) ? ID(acptr) : acptr->name);
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to uplink '%s' with src %s...",
					cptr->name, acptr->name);
#endif
			}
		}
	}

	/* Synching nick information */
	list_for_each_entry_reverse(acptr, &client_list, client_node)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;
		if (IsPerson(acptr))
		{
			introduce_user(cptr, acptr);
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
			ModDataInfo *mdi;
			
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
				    "TOPIC %s %s %lu :%s",
				    chptr->chname, chptr->topic_nick,
				    (long)chptr->topic_time, chptr->topic);
			send_moddata_channel(cptr, chptr);
		}
	}
	
	/* Send ModData for all member(ship) structs */
	send_moddata_members(cptr);
	
	/* pass on TKLs */
	tkl_synch(cptr);

	/* send out SVSFLINEs */
	dcc_sync(cptr);

	sendto_one(cptr, "NETINFO %i %li %i %s 0 0 0 :%s",
	    IRCstats.global_max, TStime(), UnrealProtocol,
	    CLOAK_KEYCRC,
	    ircnetwork);

	/* Send EOS (End Of Sync) to the just linked server... */
	sendto_one(cptr, ":%s EOS", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name);
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to justlinked '%s' with src ME...",
			cptr->name);
#endif
	RunHook(HOOKTYPE_POST_SERVER_CONNECT, cptr);
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
		sendto_one(cptr, ":%s MODE %s %s %s %lu", from,
			chptr->chname,
			modebuf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s MODE %s %s %lu", from,
			chptr->chname,
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
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);
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

	/* send MLOCK here too... --nenolod */
	if (CHECKPROTO(cptr, PROTO_MLOCK))
	{
		sendto_one(cptr, "MLOCK %lu %s :%s",
			   chptr->creationtime, chptr->chname,
			   BadPtr(chptr->mode_lock) ? "" : chptr->mode_lock);
	}
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
			sendto_one(cptr, "MODE %s %s %s %lu",
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
			sendto_one(cptr, "MODE %s %s %s %lu",
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
			sendto_one(cptr, "MODE %s %s %s %lu",
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
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);

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

	ircsnprintf(buf, sizeof(buf), "SJOIN %ld %s %s %s :",
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



		name = CHECKPROTO(cptr, PROTO_SID) ? ID(lp->cptr) : lp->cptr->name;

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

			ircsnprintf(buf, sizeof(buf), "SJOIN %ld %s %s %s :",
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
		sendto_one(cptr, "MODE %s %s %s %lu",
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
	int sent = 0; /* we need this so we send at least 1 message about the channel (eg if +P and no members, no bans, #4459) */

	if (*chptr->chname != '#')
		return;

	nomode = 0;
	nopara = 0;
	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);

	if (!modebuf[1])
		nomode = 1;
	if (!(*parabuf))
		nopara = 1;


	if (nomode && nopara)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %ld %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
		    (long)chptr->creationtime, chptr->chname);
	}
	if (nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %ld %s %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
		    (long)chptr->creationtime, chptr->chname, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %ld %s %s %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
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

		p = mystpcpy(p, CHECKPROTO(cptr, PROTO_SID) ? ID(lp->cptr) : lp->cptr->name);
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			sent++;
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
			sent++;
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
			sent++;
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
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	if (buf[prebuflen] || !sent)
		sendto_one(cptr, "%s", buf);
}
