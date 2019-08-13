/*
 * JumpServer: This module can redirect clients to another server.
 * (C) Copyright 2004-2016 Bram Matthys (Syzop).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER(jumpserver)
  = {
	"jumpserver",
	"v1.1",
	"/jumpserver command",
	"3.2-b8-1",
	NULL 
    };

/* Defines */
#define MSG_JUMPSERVER 	"JUMPSERVER"

/* Forward declarations */
CMD_FUNC(m_jumpserver);
int jumpserver_preconnect(aClient *);
void jumpserver_free_jss(ModData *m);

/* Jumpserver status struct */
typedef struct _jss JSS;
struct _jss
{
	char *reason;
	char *server;
	int port;
 	char *ssl_server;
	int ssl_port;
};

JSS *jss=NULL; /**< JumpServer Status. NULL=disabled. */

MOD_INIT(jumpserver)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, jss, jumpserver_free_jss);
	CommandAdd(modinfo->handle, MSG_JUMPSERVER, m_jumpserver, 3, M_USER);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, jumpserver_preconnect);
	return MOD_SUCCESS;
}

MOD_LOAD(jumpserver)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(jumpserver)
{
	SavePersistentPointer(modinfo, jss);
	return MOD_SUCCESS;
}

static int do_jumpserver_exit_client(aClient *sptr)
{
	if (IsSecure(sptr) && jss->ssl_server)
		sendnumeric(sptr, RPL_REDIR, jss->ssl_server, NULL, jss->ssl_port);
	else
		sendnumeric(sptr, RPL_REDIR, jss->server, jss->port);
	return exit_client(sptr, sptr, sptr, NULL, jss->reason);
}

static void redirect_all_clients(void)
{
	int i, count = 0;
	aClient *acptr, *saved;

	list_for_each_entry_safe(acptr, saved, &lclient_list, lclient_node)
	{
		if (IsPerson(acptr) && !IsOper(acptr))
		{
			do_jumpserver_exit_client(acptr);
			count++;
		}
	}
	sendto_realops("JUMPSERVER: Redirected %d client%s",
		count, count == 1 ? "" : "s"); /* Language fun... ;p */
}

int jumpserver_preconnect(aClient *sptr)
{
	if (jss)
		return do_jumpserver_exit_client(sptr);
	return 0;
}

void free_jss(void)
{
	if (jss)
	{
		safefree(jss->server);
		safefree(jss->reason);
		safefree(jss->ssl_server);
		MyFree(jss);
		jss = NULL;
	}
}

void jumpserver_free_jss(ModData *m)
{
	free_jss();
}

CMD_FUNC(m_jumpserver)
{
	char *serv, *sslserv=NULL, *reason, *p, *p2;
	int all=0, port=6667, sslport=6697;
	char logbuf[512];

	if (!IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		if (jss && jss->ssl_server)
			sendnotice(sptr, "JumpServer is \002ENABLED\002 to %s:%d (SSL/TLS: %s:%d) with reason '%s'",
				jss->server, jss->port, jss->ssl_server, jss->ssl_port, jss->reason);
		else
		if (jss)
			sendnotice(sptr, "JumpServer is \002ENABLED\002 to %s:%d with reason '%s'",
				jss->server, jss->port, jss->reason);
		else
			sendnotice(sptr, "JumpServer is \002DISABLED\002");
		return 0;
	}

	if ((parc > 1) && (!strcasecmp(parv[1], "OFF") || !strcasecmp(parv[1], "STOP")))
	{
		if (!jss)
		{
			sendnotice(sptr, "JUMPSERVER: No redirect active (already OFF)");
			return 0;
		}
		free_jss();
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) turned JUMPSERVER OFF",
			sptr->name, sptr->user->username, sptr->user->realhost);
		sendto_realops("%s", logbuf);
		ircd_log(LOG_ERROR, "%s", logbuf);
		return 0;
	}

	if (parc < 4)
	{
		/* Waah, pretty verbose usage info ;) */
		sendnotice(sptr, "Use: /JUMPSERVER <server>[:port] <NEW|ALL> <reason>");
		sendnotice(sptr, " Or: /JUMPSERVER <server>[:port]/<sslserver>[:port] <NEW|ALL> <reason>");
		sendnotice(sptr, "if 'NEW' is chosen then only new (incoming) connections will be redirected");
		sendnotice(sptr, "if 'ALL' is chosen then all clients except opers will be redirected immediately (+incoming connections)");
		sendnotice(sptr, "Example: /JUMPSERVER irc2.test.net NEW This server will be upgraded, please use irc2.test.net for now");
		sendnotice(sptr, "And then for example 10 minutes later...");
		sendnotice(sptr, "         /JUMPSERVER irc2.test.net ALL This server will be upgraded, please use irc2.test.net for now");
		sendnotice(sptr, "Use: '/JUMPSERVER OFF' to turn off any redirects");
		return 0;
	}

	/* Parsing code follows...
	 * The parsing of the SSL stuff is still done even on non-SSL,
	 * but it's simply not used/applied :).
	 * Reason for this is to reduce non-SSL/SSL inconsistency issues.
	 */

	serv = parv[1];
	
	p = strchr(serv, '/');
	if (p)
	{
		*p = '\0';
		sslserv = p+1;
	}
	
	p = strchr(serv, ':');
	if (p)
	{
		*p++ = '\0';
		port = atoi(p);
		if ((port < 1) || (port > 65535))
		{
			sendnotice(sptr, "Invalid serverport specified (%d)", port);
			return 0;
		}
	}
	if (sslserv)
	{
		p = strchr(sslserv, ':');
		if (p)
		{
			*p++ = '\0';
			sslport = atoi(p);
			if ((sslport < 1) || (sslport > 65535))
			{
				sendnotice(sptr, "Invalid SSL/TLS serverport specified (%d)", sslport);
				return 0;
			}
		}
		if (!*sslserv)
			sslserv = NULL;
	}
	if (!strcasecmp(parv[2], "new"))
		all = 0;
	else if (!strcasecmp(parv[2], "all"))
		all = 1;
	else {
		sendnotice(sptr, "ERROR: Invalid action '%s', should be 'NEW' or 'ALL' (see /jumpserver help for usage)", parv[2]);
		return 0;
	}

	reason = parv[3];

	/* Free any old stuff (needed!) */
	if (jss)
		free_jss();

	jss = MyMallocEx(sizeof(JSS));

	/* Set it */
	jss->server = strdup(serv);
	jss->port = port;
	if (sslserv)
	{
		jss->ssl_server = strdup(sslserv);
		jss->ssl_port = sslport;
	}
	jss->reason = strdup(reason);

	/* Broadcast/log */
	if (sslserv)
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) added JUMPSERVER redirect for %s to %s:%d [SSL/TLS: %s:%d] with reason '%s'",
			sptr->name, sptr->user->username, sptr->user->realhost,
			all ? "ALL CLIENTS" : "all new clients",
			jss->server, jss->port, jss->ssl_server, jss->ssl_port, jss->reason);
	else
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) added JUMPSERVER redirect for %s to %s:%d with reason '%s'",
			sptr->name, sptr->user->username, sptr->user->realhost,
			all ? "ALL CLIENTS" : "all new clients",
			jss->server, jss->port, jss->reason);

	sendto_realops("%s", logbuf);
	ircd_log(LOG_ERROR, "%s", logbuf);

	if (all)
		redirect_all_clients();

	return 0;
}
