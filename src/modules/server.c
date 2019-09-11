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

/* Forward declarations */
void send_channel_modes(Client *cptr, Channel *chptr);
void send_channel_modes_sjoin(Client *cptr, Channel *chptr);
void send_channel_modes_sjoin3(Client *cptr, Channel *chptr);
CMD_FUNC(m_server);
CMD_FUNC(m_server_remote);
int _verify_link(Client *cptr, Client *sptr, char *servername, ConfigItem_link **link_out);
void _send_protoctl_servers(Client *sptr, int response);
void _send_server_message(Client *sptr);
void _introduce_user(Client *to, Client *acptr);
int _check_deny_version(Client *cptr, char *software, int protocol, char *flags);
void _broadcast_sinfo(Client *acptr, Client *to, Client *except);

/* Global variables */
static char buf[BUFSIZE];

#define MSG_SERVER 	"SERVER"	

ModuleHeader MOD_HEADER(server)
  = {
	"server",
	"5.0",
	"command /server", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_TEST(server)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_PROTOCTL_SERVERS, _send_protoctl_servers);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_SERVER_MESSAGE, _send_server_message);
	EfunctionAdd(modinfo->handle, EFUNC_VERIFY_LINK, _verify_link);
	EfunctionAddVoid(modinfo->handle, EFUNC_INTRODUCE_USER, _introduce_user);
	EfunctionAdd(modinfo->handle, EFUNC_CHECK_DENY_VERSION, _check_deny_version);
	EfunctionAddVoid(modinfo->handle, EFUNC_BROADCAST_SINFO, _broadcast_sinfo);
	return MOD_SUCCESS;
}

MOD_INIT(server)
{
	CommandAdd(modinfo->handle, MSG_SERVER, m_server, MAXPARA, M_UNREGISTERED|M_SERVER);
	CommandAdd(modinfo->handle, "SID", m_server_remote, MAXPARA, M_SERVER);

	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

MOD_LOAD(server)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(server)
{
	return MOD_SUCCESS;
}

int m_server_synch(Client *cptr, ConfigItem_link *conf);

/** Check deny version { } blocks.
 * NOTE: cptr will always be valid, but all the other values may be NULL or 0 !!!
 */
int _check_deny_version(Client *cptr, char *software, int protocol, char *flags)
{
	ConfigItem_deny_version *vlines;
	
	for (vlines = conf_deny_version; vlines; vlines = vlines->next)
	{
		if (match_simple(vlines->mask, cptr->name))
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
			return exit_client(cptr, cptr, cptr, NULL, "Denied by deny version { } block");

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
			return exit_client(cptr, cptr, cptr, NULL, "Denied by deny version { } block");
	}
	
	return 0;
}

/** Send our PROTOCTL SERVERS=x,x,x,x stuff.
 * When response is set, it will be PROTOCTL SERVERS=*x,x,x (mind the asterisk).
 */
void _send_protoctl_servers(Client *sptr, int response)
{
	char buf[512];
	Client *acptr;

	if (!NEW_LINKING_PROTOCOL)
		return;

	sendto_one(sptr, NULL, "PROTOCTL EAUTH=%s,%d,%s%s,%s",
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

	sendto_one(sptr, NULL, "%s", buf);
}

void _send_server_message(Client *sptr)
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
		sendto_one(sptr, NULL, "SERVER %s 1 :U%d-%s%s-%s %s",
			me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", me.id, me.info);
	} else {
		sendto_one(sptr, NULL, "SERVER %s 1 :%s",
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
int _verify_link(Client *cptr, Client *sptr, char *servername, ConfigItem_link **link_out)
{
	char xerrmsg[256];
	ConfigItem_link *link;
	char *inpath = get_client_name(cptr, TRUE);
	Client *acptr = NULL, *ocptr = NULL;
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
		sendto_one(cptr, NULL, "ERROR :Missing password");
		return exit_client(cptr, sptr, &me, NULL, "Missing password");
	}

	/* First check if the server is in the list */
	if (!servername) {
		strcpy(xerrmsg, "Null servername");
		goto errlink;
	}
	
	if (cptr->serv && cptr->serv->conf)
	{
		/* This is an outgoing connect so we already know what link block we are
		 * dealing with. It's the one in: cptr->serv->conf
		 */

		/* Actually we still need to double check the servername to avoid confusion. */
		if (strcasecmp(servername, cptr->serv->conf->servername))
		{
			ircsnprintf(xerrmsg, sizeof(xerrmsg), "Outgoing connect from link block '%s' but server "
				"introduced himself as '%s'. Server name mismatch.",
				cptr->serv->conf->servername,
				servername);

			sendto_one(cptr, NULL, "ERROR :%s", xerrmsg);
			sendto_ops_and_log("Outgoing link aborted to %s(%s@%s) (%s) %s",
				cptr->serv->conf->servername, cptr->username, cptr->local->sockhost, xerrmsg, inpath);
			return exit_client(cptr, sptr, &me, NULL, xerrmsg);
		}
		link = cptr->serv->conf;
		goto skip_host_check;
	} else {
		/* Hunt the linkblock down ;) */
		for(link = conf_link; link; link = link->next)
			if (match_simple(link->servername, servername))
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
		sendto_one(cptr, NULL,
		    "ERROR :Link denied (No link block found named '%s' or link::incoming::mask did not match your IP %s) %s",
		    servername, GetIP(cptr), inpath);
		/* And send the "verbose" error msg only to locally connected ircops */
		sendto_ops_and_log("Link denied for %s(%s@%s) (%s) %s",
		    servername, cptr->username, cptr->local->sockhost, xerrmsg, inpath);
		return exit_client(cptr, sptr, &me, NULL,
		    "Link denied (No link block found with your server name or link::incoming::mask did not match)");
	}

skip_host_check:
	/* Now for checking passwords */
	if (!Auth_Check(cptr, link->auth, cptr->local->passwd))
	{
		/* Let's help admins a bit with a good error message in case
		 * they mix different authentication systems (plaintext password
		 * vs an "TLS Auth type" like spkifp/tlsclientcert/tlsclientcertfp).
		 * The 'if' statement below is a bit complex but it consists of 2 things:
		 * 1. Check if our side expects a plaintext password but we did not receive one
		 * 2. Check if our side expects a non-plaintext password but we did receive one
		 */
		if (((link->auth->type == AUTHTYPE_PLAINTEXT) && cptr->local->passwd && !strcmp(cptr->local->passwd, "*")) ||
		    ((link->auth->type != AUTHTYPE_PLAINTEXT) && cptr->local->passwd && strcmp(cptr->local->passwd, "*")))
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed due to different password types on both sides of the link) %s",
				servername, inpath);
			sendto_ops_and_log("Read https://www.unrealircd.org/docs/FAQ#auth-fail-mixed for more information");
		} else
		if (link->auth->type == AUTHTYPE_SPKIFP)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [spkifp mismatch]) %s",
				servername, inpath);
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERT)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [tlsclientcert mismatch]) %s",
				servername, inpath);
		} else
		if (link->auth->type == AUTHTYPE_TLS_CLIENTCERTFP)
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [tlsclientcertfp mismatch]) %s",
				servername, inpath);
		} else
		{
			sendto_ops_and_log("Link denied for '%s' (Authentication failed [Bad password?]) %s",
				servername, inpath);
		}
		sendto_one(cptr, NULL,
		    "ERROR :Link '%s' denied (Authentication failed) %s",
		    servername, inpath);
		return exit_client(cptr, sptr, &me, NULL,
		    "Link denied (Authentication failed)");
	}

	/* Verify the TLS certificate (if requested) */
	if (link->verify_certificate)
	{
		char *errstr = NULL;

		if (!IsTLS(cptr))
		{
			sendto_one(cptr, NULL,
				"ERROR :Link '%s' denied (Not using SSL/TLS) %s",
				servername, inpath);
			sendto_ops_and_log("Link denied for '%s' (Not using SSL/TLS and verify-certificate is on) %s",
				servername, inpath);
			return exit_client(cptr, sptr, &me, NULL,
				"Link denied (Not using SSL/TLS)");
		}
		if (!verify_certificate(cptr->local->ssl, link->servername, &errstr))
		{
			sendto_one(cptr, NULL,
				"ERROR :Link '%s' denied (Certificate verification failed) %s",
				servername, inpath);
			sendto_ops_and_log("Link denied for '%s' (Certificate verification failed) %s",
				servername, inpath);
			sendto_ops_and_log("Reason for certificate verification failure: %s", errstr);
			return exit_client(cptr, sptr, &me, NULL,
				"Link denied (Certificate verification failed)");
		}
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
			sendto_ops_and_log("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(sptr, TRUE), me.name);
			sendto_one(sptr, NULL, "ERROR: Server %s exists (it's me!)", me.name);
			return exit_client(sptr, sptr, sptr, NULL, "Server Exists");
		}

		acptr = acptr->from;
		ocptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? acptr : cptr;
		acptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? cptr : acptr;
		sendto_one(acptr, NULL,
		    "ERROR :Server %s already exists from %s",
		    servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		sendto_ops_and_log
		    ("Link %s cancelled, server %s already exists from %s",
		    get_client_name(acptr, TRUE), servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		return exit_client(acptr, acptr, acptr, NULL,
		    "Server Exists");
	}
	if ((bconf = Find_ban(NULL, servername, CONF_BAN_SERVER)))
	{
		sendto_ops_and_log
			("Cancelling link %s, banned server",
			get_client_name(cptr, TRUE));
		sendto_one(cptr, NULL, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
		return exit_client(cptr, cptr, &me, NULL, "Banned server");
	}
	if (link->class->clients + 1 > link->class->maxclients)
	{
		sendto_ops_and_log("Cancelling link %s, full class",
				get_client_name(cptr, TRUE));
		return exit_client(cptr, cptr, &me, NULL, "Full class");
	}
	if (!IsLocal(cptr) && (iConf.plaintext_policy_server == POLICY_DENY) && !IsSecure(cptr))
	{
		sendto_one(cptr, NULL, "ERROR :Servers need to use SSL/TLS (set::plaintext-policy::server is 'deny')");
		sendto_ops_and_log("Rejected insecure server %s. See https://www.unrealircd.org/docs/FAQ#ERROR:_Servers_need_to_use_SSL.2FTLS", cptr->name);
		return exit_client(cptr, sptr, &me, NULL, "Servers need to use SSL/TLS (set::plaintext-policy::server is 'deny')");
	}
	if (IsSecure(cptr) && (iConf.outdated_tls_policy_server == POLICY_DENY) && outdated_tls_client(cptr))
	{
		sendto_one(cptr, NULL, "ERROR :Server is using an outdated SSL/TLS protocol or cipher (set::outdated-tls-policy::server is 'deny')");
		sendto_ops_and_log("Rejected server %s using outdated %s. See https://www.unrealircd.org/docs/FAQ#server-outdated-tls", tls_get_cipher(cptr->local->ssl), cptr->name);
		return exit_client(cptr, sptr, &me, NULL, "Server using outdates SSL/TLS protocol or cipher (set::outdated-tls-policy::server is 'deny')");
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
		sendnumeric(cptr, ERR_ALREADYREGISTRED);
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
		sendto_one(sptr, NULL, "ERROR :Not enough SERVER parameters");
		return exit_client(cptr, sptr, &me, NULL,  "Not enough parameters");		
	}

	if (IsUnknown(cptr) && (cptr->local->listener->options & LISTENER_CLIENTSONLY))
	{
		return exit_client(cptr, sptr, &me, NULL,
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
	if (*ch || !strchr(servername, '.'))
	{
		sendto_one(sptr, NULL, "ERROR :Bogus server name (%s)", servername);
		sendto_snomask
		    (SNO_JUNK,
		    "WARNING: Bogus server name (%s) from %s (maybe just a fishy client)",
		    servername, get_client_name(cptr, TRUE));

		return exit_client(cptr, sptr, &me, NULL, "Bogus server name");
	}

	if ((IsUnknown(cptr) || IsHandshake(cptr)) && !cptr->local->passwd)
	{
		sendto_one(sptr, NULL, "ERROR :Missing password");
		return exit_client(cptr, sptr, &me, NULL, "Missing password");
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

		for (deny = conf_deny_link; deny; deny = deny->next)
		{
			if (deny->flag.type == CRULE_ALL && match_simple(deny->mask, servername)
				&& crule_eval(deny->rule)) {
				sendto_ops_and_log("Refused connection from %s. Rejected by deny link { } block.",
					get_client_host(cptr));
				return exit_client(cptr, cptr, cptr, NULL,
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
		return m_server_remote(cptr, sptr, recv_mtags, parc, parv);
	}
	return 0;
}

CMD_FUNC(m_server_remote)
{
	Client *acptr, *ocptr, *bcptr;
	ConfigItem_link	*aconf;
	ConfigItem_ban *bconf;
	int 	hop;
	char	info[REALLEN + 61];
	char	*servername = parv[1];

	if (parc < 4 || (!*parv[3]))
	{
		sendto_one(sptr, NULL, "ERROR :Not enough SERVER parameters");
		return 0;
	}

	if ((acptr = find_server(servername, NULL)))
	{
		/* Found. Bad. Quit. */

		if (IsMe(acptr))
		{
			sendto_ops_and_log("Link %s rejected, server trying to link with my name (%s)",
				get_client_name(sptr, TRUE), me.name);
			sendto_one(sptr, NULL, "ERROR: Server %s exists (it's me!)", me.name);
			return exit_client(sptr, sptr, sptr, NULL, "Server Exists");
		}

		acptr = acptr->from;
		ocptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? acptr : cptr;
		acptr =
		    (cptr->local->firsttime > acptr->local->firsttime) ? cptr : acptr;
		sendto_one(acptr, NULL,
		    "ERROR :Server %s already exists from %s",
		    servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		sendto_ops_and_log
		    ("Link %s cancelled, server %s already exists from %s",
		    get_client_name(acptr, TRUE), servername,
		    (ocptr->from ? ocptr->from->name : "<nobody>"));
		if (acptr == cptr) {
			return exit_client(acptr, acptr, acptr, NULL, "Server Exists");
		} else {
			/* AFAIK this can cause crashes if this happends remotely because
			 * we will still receive msgs for some time because of lag.
			 * Two possible solutions: unlink the directly connected server (cptr)
			 * and/or fix all those commands which blindly trust server input. -- Syzop
			 */
			exit_client(acptr, acptr, acptr, NULL, "Server Exists");
			return 0;
		}
	}
	if ((bconf = Find_ban(NULL, servername, CONF_BAN_SERVER)))
	{
		sendto_ops_and_log("Cancelling link %s, banned server %s",
			get_client_name(cptr, TRUE), servername);
		sendto_one(cptr, NULL, "ERROR :Banned server (%s)", bconf->reason ? bconf->reason : "no reason");
		return exit_client(cptr, cptr, &me, NULL, "Brought in banned server");
	}
	/* OK, let us check in the data now now */
	hop = atol(parv[2]);
	strlcpy(info, parv[parc - 1], sizeof(info));
	if (!cptr->serv->conf)
	{
		sendto_ops_and_log("Internal error: lost conf for %s!!, dropping link", cptr->name);
		return exit_client(cptr, cptr, cptr, NULL, "Lost configuration");
	}
	aconf = cptr->serv->conf;
	if (!aconf->hub)
	{
		sendto_ops_and_log("Link %s cancelled, is Non-Hub but introduced Leaf %s",
			cptr->name, servername);
		return exit_client(cptr, cptr, cptr, NULL, "Non-Hub Link");
	}
	if (!match_simple(aconf->hub, servername))
	{
		sendto_ops_and_log("Link %s cancelled, linked in %s, which hub config disallows",
			cptr->name, servername);
		return exit_client(cptr, cptr, cptr, NULL, "Not matching hub configuration");
	}
	if (aconf->leaf)
	{
		if (!match_simple(aconf->leaf, servername))
		{
			sendto_ops_and_log("Link %s(%s) cancelled, disallowed by leaf configuration",
				cptr->name, servername);
			return exit_client(cptr, cptr, cptr, NULL, "Disallowed by leaf configuration");
		}
	}
	if (aconf->leaf_depth && (hop > aconf->leaf_depth))
	{
			sendto_ops_and_log("Link %s(%s) cancelled, too deep depth",
				cptr->name, servername);
			return exit_client(cptr, cptr, cptr, NULL, "Too deep link depth (leaf)");
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
	/* Taken from bahamut makes it so all servers behind a U-Lined
	 * server are also U-Lined, very helpful if HIDE_ULINES is on
	 */
	if (IsULine(sptr)
	    || (Find_uline(acptr->name)))
		acptr->flags |= FLAGS_ULINE;
	ircstats.servers++;
	(void)find_or_add(acptr->name);
	add_client_to_list(acptr);
	(void)add_to_client_hash_table(acptr->name, acptr);

	if (*acptr->id)
		add_to_id_hash_table(acptr->id, acptr);

	list_move(&acptr->client_node, &global_server_list);
	RunHook(HOOKTYPE_SERVER_CONNECT, acptr);

	if (*acptr->id)
	{
		sendto_server(cptr, PROTO_SID, 0, NULL, ":%s SID %s %d %s :%s",
			    acptr->srvptr->id, acptr->name, hop + 1, acptr->id, acptr->info);
		sendto_server(cptr, 0, PROTO_SID, NULL, ":%s SERVER %s %d :%s",
				acptr->srvptr->name,
				acptr->name, hop + 1, acptr->info);
	} else {
		sendto_server(cptr, 0, 0, NULL, ":%s SERVER %s %d :%s",
				acptr->srvptr->name,
				acptr->name, hop + 1, acptr->info);
	}

	RunHook(HOOKTYPE_POST_SERVER_CONNECT, acptr);
	return 0;
}

void _introduce_user(Client *to, Client *acptr)
{
	send_umode(NULL, acptr, 0, SEND_UMODES, buf);

	sendto_one_nickcmd(to, acptr, buf);
	
	send_moddata_client(to, acptr);

	if (acptr->user->away)
		sendto_one(to, NULL, ":%s AWAY :%s", CHECKPROTO(to, PROTO_SID) ? ID(acptr) : acptr->name,
			acptr->user->away);

	if (acptr->user->swhois)
	{
		SWhois *s;
		for (s = acptr->user->swhois; s; s = s->next)
		{
			if (CHECKPROTO(to, PROTO_EXTSWHOIS))
			{
				sendto_one(to, NULL, ":%s SWHOIS %s + %s %d :%s",
					me.name, acptr->name, s->setby, s->priority, s->line);
			} else
			{
				sendto_one(to, NULL, ":%s SWHOIS %s :%s",
					me.name, acptr->name, s->line);
			}
		}
	}
}

void tls_link_notification_verify(Client *acptr, ConfigItem_link *aconf)
{
	char *spki_fp;
	char *tls_fp;
	char *errstr = NULL;
	int verify_ok;

	if (!MyConnect(acptr) || !acptr->local->ssl || !aconf)
		return;

	if ((aconf->auth->type == AUTHTYPE_TLS_CLIENTCERT) ||
	    (aconf->auth->type == AUTHTYPE_TLS_CLIENTCERTFP) ||
	    (aconf->auth->type == AUTHTYPE_SPKIFP))
	{
		/* Link verified by certificate or SPKI */
		return;
	}

	if (aconf->verify_certificate)
	{
		/* Link verified by trust chain */
		return;
	}

	tls_fp = moddata_client_get(acptr, "certfp");
	spki_fp = spki_fingerprint(acptr);
	if (!tls_fp || !spki_fp)
		return; /* wtf ? */

	/* Only bother the user if we are linking to UnrealIRCd 4.0.16+,
	 * since only for these versions we can give precise instructions.
	 */
	if (!acptr->serv || acptr->serv->features.protocol < 4016)
		return;

	sendto_realops("You may want to consider verifying this server link.");
	sendto_realops("More information about this can be found on https://www.unrealircd.org/Link_verification");

	verify_ok = verify_certificate(acptr->local->ssl, aconf->servername, &errstr);
	if (errstr && strstr(errstr, "not valid for hostname"))
	{
		sendto_realops("Unfortunately the certificate of server '%s' has a name mismatch:", acptr->name);
		sendto_realops("%s", errstr);
		sendto_realops("This isn't a fatal error but it will prevent you from using verify-certificate yes;");
	} else
	if (!verify_ok)
	{
		sendto_realops("In short: in the configuration file, change the 'link %s {' block to use this as a password:", acptr->name);
		sendto_realops("password \"%s\" { spkifp; };", spki_fp);
		sendto_realops("And follow the instructions on the other side of the link as well (which will be similar, but will use a different hash)");
	} else
	{
		sendto_realops("In short: in the configuration file, add the following to your 'link %s {' block:", acptr->name);
		sendto_realops("verify-certificate yes;");
		sendto_realops("Alternatively, you could use SPKI fingerprint verification. Then change the password in the link block to be:");
		sendto_realops("password \"%s\" { spkifp; };", spki_fp);
	}
}

#define SafeStr(x)    ((x && *(x)) ? (x) : "*")

/** Broadcast SINFO.
 * @param cptr   The server to send the information about.
 * @param to     The server to send the information TO (NULL for broadcast).
 * @param except The direction NOT to send to.
 * This function takes into account that the server may not
 * provide all of the detailed info. If any information is
 * absent we will send 0 for numbers and * for NULL strings.
 */
void _broadcast_sinfo(Client *acptr, Client *to, Client *except)
{
	char chanmodes[128], buf[512];

	if (acptr->serv->features.chanmodes[0])
	{
		snprintf(chanmodes, sizeof(chanmodes), "%s,%s,%s,%s",
			 acptr->serv->features.chanmodes[0],
			 acptr->serv->features.chanmodes[1],
			 acptr->serv->features.chanmodes[2],
			 acptr->serv->features.chanmodes[3]);
	} else {
		strlcpy(chanmodes, "*", sizeof(chanmodes));
	}

	snprintf(buf, sizeof(buf), "%lld %d %s %s %s :%s",
		      (long long)acptr->serv->boottime,
		      acptr->serv->features.protocol,
		      SafeStr(acptr->serv->features.usermodes),
		      chanmodes,
		      SafeStr(acptr->serv->features.nickchars),
		      SafeStr(acptr->serv->features.software));

	if (to)
	{
		/* Targetted to one server */
		sendto_one(to, NULL, ":%s SINFO %s", acptr->name, buf);
	} else {
		/* Broadcast (except one side...) */
		sendto_server(except, 0, 0, NULL, ":%s SINFO %s", acptr->name, buf);
	}
}

int	m_server_synch(Client *cptr, ConfigItem_link *aconf)
{
	char		*inpath = get_client_name(cptr, TRUE);
	Client		*acptr;
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
			sendto_one(cptr, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");

		send_proto(cptr, aconf);
		send_server_message(cptr);
	}

	/* Set up server structure */
	free_pending_net(cptr);
	SetServer(cptr);
	ircstats.me_servers++;
	ircstats.servers++;
	ircstats.unknown--;
	list_move(&cptr->client_node, &global_server_list);
	list_move(&cptr->lclient_node, &lclient_list);
	list_add(&cptr->special_node, &server_list);
	if ((Find_uline(cptr->name)))
	{
		if (cptr->serv && cptr->serv->features.software && !strncmp(cptr->serv->features.software, "UnrealIRCd-", 11))
		{
			sendto_realops("\002WARNING:\002 Bad ulines! It seems your server is misconfigured: "
			               "your ulines { } block is matching an UnrealIRCd server (%s). "
			               "This is not correct and will cause security issues. "
			               "ULines should only be added for services! "
			               "See https://www.unrealircd.org/docs/FAQ#bad-ulines",
			               cptr->name);
		}
		cptr->flags |= FLAGS_ULINE;
	}
	(void)find_or_add(cptr->name);
	if (IsSecure(cptr))
	{
		sendto_server(&me, 0, 0, NULL, ":%s SMO o :(\2link\2) Secure link %s -> %s established (%s)",
			me.name,
			me.name, inpath, tls_get_cipher(cptr->local->ssl));
		sendto_realops("(\2link\2) Secure link %s -> %s established (%s)",
			me.name, inpath, tls_get_cipher(cptr->local->ssl));
		tls_link_notification_verify(cptr, aconf);
	}
	else
	{
		sendto_server(&me, 0, 0, NULL, ":%s SMO o :(\2link\2) Link %s -> %s established",
			me.name,
			me.name, inpath);
		sendto_realops("(\2link\2) Link %s -> %s established",
			me.name, inpath);
		/* Print out a warning if linking to a non-TLS server unless it's localhost.
		 * Yeah.. there are still other cases when non-TLS links are fine (eg: local IP
		 * of the same machine), we won't bother with detecting that. -- Syzop
		 */
		if (!IsLocal(cptr) && (iConf.plaintext_policy_server == POLICY_WARN))
		{
			sendto_realops("\002WARNING:\002 This link is unencrypted (not SSL/TLS). We highly recommend to use "
			               "SSL/TLS for server linking. See https://www.unrealircd.org/docs/Linking_servers");
		}
		if (IsSecure(cptr) && (iConf.outdated_tls_policy_server == POLICY_WARN) && outdated_tls_client(cptr))
		{
			sendto_realops("\002WARNING:\002 This link is using an outdated SSL/TLS protocol or cipher (%s).",
			               tls_get_cipher(cptr->local->ssl));
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

	/* Broadcast new server to the rest of the network */
	if (*cptr->id)
	{
		sendto_server(cptr, PROTO_SID, 0, NULL, ":%s SID %s 2 %s :%s",
			    cptr->srvptr->id, cptr->name, cptr->id, cptr->info);
	}

	sendto_server(cptr, 0, *cptr->id ? PROTO_SID : 0, NULL, ":%s SERVER %s 2 :%s",
		    cptr->serv->up,
		    cptr->name, cptr->info);

	/* Broadcast the just-linked-in featureset to other servers on our side */
	broadcast_sinfo(cptr, NULL, cptr);

	/* Send moddata of &me (if any, likely minimal) */
	send_moddata_client(cptr, &me);

	list_for_each_entry_reverse(acptr, &global_server_list, client_node)
	{
		/* acptr->from == acptr for acptr == cptr */
		if (acptr->from == cptr)
			continue;

		if (IsServer(acptr))
		{
			if (SupportSID(cptr) && *acptr->id)
			{
				sendto_one(cptr, NULL, ":%s SID %s %d %s :%s",
				    acptr->srvptr->id,
				    acptr->name, acptr->hopcount + 1,
				    acptr->id, acptr->info);
			}
			else
				sendto_one(cptr, NULL, ":%s SERVER %s %d :%s",
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
				sendto_one(cptr, NULL, ":%s EOS", CHECKPROTO(cptr, PROTO_SID) ? ID(acptr) : acptr->name);
#ifdef DEBUGMODE
				ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to uplink '%s' with src %s...",
					cptr->name, acptr->name);
#endif
			}
			/* Send SINFO of our servers to their side */
			broadcast_sinfo(acptr, cptr, NULL);
			send_moddata_client(cptr, acptr); /* send moddata of server 'acptr' (if any, likely minimal) */
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
		Channel *chptr;
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
				sendto_one(cptr, NULL, "TOPIC %s %s %lld :%s",
				    chptr->chname, chptr->topic_nick,
				    (long long)chptr->topic_time, chptr->topic);
			send_moddata_channel(cptr, chptr);
		}
	}
	
	/* Send ModData for all member(ship) structs */
	send_moddata_members(cptr);
	
	/* pass on TKLs */
	tkl_synch(cptr);

	/* send out SVSFLINEs */
	dcc_sync(cptr);

	sendto_one(cptr, NULL, "NETINFO %i %lld %i %s 0 0 0 :%s",
	    ircstats.global_max, (long long)TStime(), UnrealProtocol,
	    CLOAK_KEYCRC,
	    ircnetwork);

	/* Send EOS (End Of Sync) to the just linked server... */
	sendto_one(cptr, NULL, ":%s EOS", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name);
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] m_server_synch: sending to justlinked '%s' with src ME...",
			cptr->name);
#endif
	RunHook(HOOKTYPE_POST_SERVER_CONNECT, cptr);
	return 0;
}

/** Send MODE +vhoaq list (depending on 'mask' and 'flag') to remote server.
 * Previously this function was called send_mode_list() when it was dual-function.
 * (only for old severs lacking SJOIN/SJ3
 */
static void send_channel_modes_members(Client *cptr, Channel *chptr, int mask, char flag)
{
	Member *lp;
	char *cp, *name;
	int  count = 0, send = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)		/* mode +l or +k xx */
		count = 1;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		if (!(lp->flags & mask))
			continue;
		name = lp->cptr->name;
		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				strlcat(parabuf, " ", sizeof parabuf);
			strlcat(parabuf, name, sizeof parabuf);
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
			/* cptr is always a server! So we send creationtime */
			sendmodeto_one(cptr, me.name, chptr->chname, modebuf, parabuf, chptr->creationtime);
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != RESYNCMODES)
			{
				strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = flag;
			}
			count = 0;
			*cp = '\0';
		}
	}
}

/** Send list modes such as +beI to remote server.
 * Previously this was combined with +vhoaq stuff in the send_mode_list() function.
 * (only for old severs lacking SJOIN/SJ3
 */
static void send_channel_modes_list_mode(Client *cptr, Channel *chptr, Ban *lp, char flag)
{
	char *cp, *name;
	int count = 0, send = 0;

	cp = modebuf + strlen(modebuf);

	if (*parabuf)		/* mode +l or +k xx */
		count = 1;

	for (; lp; lp = lp->next)
	{
		name = lp->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				strlcat(parabuf, " ", sizeof parabuf);
			strlcat(parabuf, name, sizeof parabuf);
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
			/* cptr is always a server! So we send creationtime */
			sendmodeto_one(cptr, me.name, chptr->chname, modebuf, parabuf, chptr->creationtime);
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != RESYNCMODES)
			{
				strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = flag;
			}
			count = 0;
			*cp = '\0';
		}
	}
}

/* (only for old severs lacking SJOIN/SJ3 */
static inline void send_channel_mode(Client *cptr, char *from, Channel *chptr)
{
	if (*parabuf)
		sendto_one(cptr, NULL, ":%s MODE %s %s %s %lld", from,
			chptr->chname,
			modebuf, parabuf, (long long)chptr->creationtime);
	else
		sendto_one(cptr, NULL, ":%s MODE %s %s %lld", from,
			chptr->chname,
			modebuf, (long long)chptr->creationtime);
}

/**  Send "cptr" a full list of the MODEs for channel chptr.
 * Note that this function is only used for servers lacking SJOIN/SJOIN3.
 */
void send_channel_modes(Client *cptr, Channel *chptr)
{
	if (*chptr->chname != '#')
		return;

	/* Send the "property" channel modes like +lks */
	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);
	send_channel_mode(cptr, me.name, chptr);

	/* Then send the +qaohv in one go */
	modebuf[0] = '+';
	modebuf[1] = '\0';
	parabuf[0] = '\0';
	send_channel_modes_members(cptr, chptr, CHFL_CHANOWNER, 'q');
	send_channel_modes_members(cptr, chptr, CHFL_CHANADMIN, 'a');
	send_channel_modes_members(cptr, chptr, CHFL_CHANOP, 'o');
	send_channel_modes_members(cptr, chptr, CHFL_HALFOP, 'h');
	send_channel_modes_members(cptr, chptr, CHFL_VOICE, 'v');
	/* ..including any remainder in the buffer.. */
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf, parabuf, chptr->creationtime);

	/* Then send the +beI in one go */
	modebuf[0] = '+';
	modebuf[1] = '\0';
	parabuf[0] = '\0';
	send_channel_modes_list_mode(cptr, chptr, chptr->banlist, 'b');
	send_channel_modes_list_mode(cptr, chptr, chptr->exlist, 'e');
	send_channel_modes_list_mode(cptr, chptr, chptr->invexlist, 'I');
	/* ..including any remainder in the buffer.. */
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf, parabuf, chptr->creationtime);

	/* send MLOCK here too... --nenolod */
	if (CHECKPROTO(cptr, PROTO_MLOCK))
	{
		sendto_one(cptr, NULL, "MLOCK %lld %s :%s",
			   (long long)chptr->creationtime, chptr->chname,
			   BadPtr(chptr->mode_lock) ? "" : chptr->mode_lock);
	}
}

/* (only for old severs lacking SJ3) */
static int send_ban_list(Client *cptr, char *chname, time_t creationtime, Channel *channel)
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
		name = lp->banstr;

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
			sendto_one(cptr, NULL, "MODE %s %s %s %lld",
			    chname, modebuf, parabuf, (long long)creationtime);
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
		name = lp->banstr;

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
			sendto_one(cptr, NULL, "MODE %s %s %s %lld",
			    chname, modebuf, parabuf, (long long)creationtime);
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
		name = lp->banstr;

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
			sendto_one(cptr, NULL, "MODE %s %s %s %lld",
			    chname, modebuf, parabuf, (long long)creationtime);
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
 * NOTE: this is only for old servers who do not support SJ3.
 */
void send_channel_modes_sjoin(Client *cptr, Channel *chptr)
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

	ircsnprintf(buf, sizeof(buf), "SJOIN %lld %s %s %s :",
	    (long long)chptr->creationtime, chptr->chname, modebuf, parabuf);

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
		if (lp->flags & MODE_CHANADMIN)
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
			sendto_one(cptr, NULL, "%s", buf);

			ircsnprintf(buf, sizeof(buf), "SJOIN %lld %s %s %s :",
			    (long long)chptr->creationtime, chptr->chname, modebuf,
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
		sendto_one(cptr, NULL, "%s", buf);
	}
	/* Then we'll send the ban-list */

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_ban_list(cptr, chptr->chname, chptr->creationtime, chptr);

	if (modebuf[1] || *parabuf)
		sendto_one(cptr, NULL, "MODE %s %s %s %lld",
		    chptr->chname, modebuf, parabuf,
		    (long long)chptr->creationtime);

	return;
}

/** This will send "cptr" a full list of the modes for channel chptr,
 *
 * Half of it recoded by Syzop: the whole buffering and size checking stuff
 * looked weird and just plain inefficient. We now fill up our send-buffer
 * really as much as we can, without causing any overflows of course.
 */
void send_channel_modes_sjoin3(Client *cptr, Channel *chptr)
{
	MessageTag *mtags = NULL;
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

	/* Generate a new message (including msgid).
	 * Due to the way SJOIN works, we will use the same msgid for
	 * multiple SJOIN messages to servers. Rest assured that clients
	 * will never see these duplicate msgid's though. They
	 * will see a 'special' version instead with a suffix.
	 */
	new_message(&me, NULL, &mtags);

	if (nomode && nopara)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
		    (long long)chptr->creationtime, chptr->chname);
	}
	if (nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
		    (long long)chptr->creationtime, chptr->chname, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsnprintf(buf, sizeof(buf),
		    ":%s SJOIN %lld %s %s %s :", CHECKPROTO(cptr, PROTO_SID) ? me.id : me.name,
		    (long long)chptr->creationtime, chptr->chname, modebuf, parabuf);
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
		if (lp->flags & MODE_CHANADMIN)
			*p++ = '~';

		p = mystpcpy(p, CHECKPROTO(cptr, PROTO_SID) ? ID(lp->cptr) : lp->cptr->name);
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, mtags, "%s", buf);
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
		if (SupportSJSBY(cptr))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '&';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, mtags, "%s", buf);
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
		if (SupportSJSBY(cptr))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '"';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, mtags, "%s", buf);
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
		if (SupportSJSBY(cptr))
			p += add_sjsby(p, ban->who, ban->when);
		*p++ = '\'';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, mtags, "%s", buf);
			sent++;
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	if (buf[prebuflen] || !sent)
		sendto_one(cptr, mtags, "%s", buf);

	free_message_tags(mtags);
}
