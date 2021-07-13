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

ModuleHeader MOD_HEADER
  = {
	"jumpserver",
	"1.1",
	"/jumpserver command",
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* Defines */
#define MSG_JUMPSERVER 	"JUMPSERVER"

/* Forward declarations */
CMD_FUNC(cmd_jumpserver);
int jumpserver_preconnect(Client *);
void jumpserver_free_jss(ModData *m);

/* Jumpserver status struct */
typedef struct JSS JSS;
struct JSS
{
	char *reason;
	char *server;
	int port;
 	char *ssl_server;
	int ssl_port;
};

JSS *jss=NULL; /**< JumpServer Status. NULL=disabled. */

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	LoadPersistentPointer(modinfo, jss, jumpserver_free_jss);
	CommandAdd(modinfo->handle, MSG_JUMPSERVER, cmd_jumpserver, 3, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0, jumpserver_preconnect);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, jss);
	return MOD_SUCCESS;
}

static void do_jumpserver_exit_client(Client *client)
{
	if (IsSecure(client) && jss->ssl_server)
		sendnumeric(client, RPL_REDIR, jss->ssl_server, jss->ssl_port);
	else
		sendnumeric(client, RPL_REDIR, jss->server, jss->port);
	exit_client(client, NULL, jss->reason);
}

static void redirect_all_clients(void)
{
	int count = 0;
	Client *client, *next;

	list_for_each_entry_safe(client, next, &lclient_list, lclient_node)
	{
		if (IsUser(client) && !IsOper(client))
		{
			do_jumpserver_exit_client(client);
			count++;
		}
	}
	sendto_realops("JUMPSERVER: Redirected %d client%s",
		count, count == 1 ? "" : "s"); /* Language fun... ;p */
}

int jumpserver_preconnect(Client *client)
{
	if (jss)
	{
		do_jumpserver_exit_client(client);
		return HOOK_DENY;
	}
	return HOOK_CONTINUE;
}

void free_jss(void)
{
	if (jss)
	{
		safe_free(jss->server);
		safe_free(jss->reason);
		safe_free(jss->ssl_server);
		safe_free(jss);
		jss = NULL;
	}
}

void jumpserver_free_jss(ModData *m)
{
	free_jss();
}

CMD_FUNC(cmd_jumpserver)
{
	char *serv, *sslserv=NULL, *reason, *p;
	int all=0, port=6667, sslport=6697;
	char logbuf[512];

	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		if (jss && jss->ssl_server)
			sendnotice(client, "JumpServer is \002ENABLED\002 to %s:%d (SSL/TLS: %s:%d) with reason '%s'",
				jss->server, jss->port, jss->ssl_server, jss->ssl_port, jss->reason);
		else
		if (jss)
			sendnotice(client, "JumpServer is \002ENABLED\002 to %s:%d with reason '%s'",
				jss->server, jss->port, jss->reason);
		else
			sendnotice(client, "JumpServer is \002DISABLED\002");
		return;
	}

	if ((parc > 1) && (!strcasecmp(parv[1], "OFF") || !strcasecmp(parv[1], "STOP")))
	{
		if (!jss)
		{
			sendnotice(client, "JUMPSERVER: No redirect active (already OFF)");
			return;
		}
		free_jss();
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) turned JUMPSERVER OFF",
			client->name, client->user->username, client->user->realhost);
		sendto_realops("%s", logbuf);
		ircd_log(LOG_ERROR, "%s", logbuf);
		return;
	}

	if (parc < 4)
	{
		/* Waah, pretty verbose usage info ;) */
		sendnotice(client, "Use: /JUMPSERVER <server>[:port] <NEW|ALL> <reason>");
		sendnotice(client, " Or: /JUMPSERVER <server>[:port]/<sslserver>[:port] <NEW|ALL> <reason>");
		sendnotice(client, "if 'NEW' is chosen then only new (incoming) connections will be redirected");
		sendnotice(client, "if 'ALL' is chosen then all clients except opers will be redirected immediately (+incoming connections)");
		sendnotice(client, "Example: /JUMPSERVER irc2.test.net NEW This server will be upgraded, please use irc2.test.net for now");
		sendnotice(client, "And then for example 10 minutes later...");
		sendnotice(client, "         /JUMPSERVER irc2.test.net ALL This server will be upgraded, please use irc2.test.net for now");
		sendnotice(client, "Use: '/JUMPSERVER OFF' to turn off any redirects");
		return;
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
			sendnotice(client, "Invalid serverport specified (%d)", port);
			return;
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
				sendnotice(client, "Invalid SSL/TLS serverport specified (%d)", sslport);
				return;
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
		sendnotice(client, "ERROR: Invalid action '%s', should be 'NEW' or 'ALL' (see /jumpserver help for usage)", parv[2]);
		return;
	}

	reason = parv[3];

	/* Free any old stuff (needed!) */
	if (jss)
		free_jss();

	jss = safe_alloc(sizeof(JSS));

	/* Set it */
	safe_strdup(jss->server, serv);
	jss->port = port;
	if (sslserv)
	{
		safe_strdup(jss->ssl_server, sslserv);
		jss->ssl_port = sslport;
	}
	safe_strdup(jss->reason, reason);

	/* Broadcast/log */
	if (sslserv)
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) added JUMPSERVER redirect for %s to %s:%d [SSL/TLS: %s:%d] with reason '%s'",
			client->name, client->user->username, client->user->realhost,
			all ? "ALL CLIENTS" : "all new clients",
			jss->server, jss->port, jss->ssl_server, jss->ssl_port, jss->reason);
	else
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) added JUMPSERVER redirect for %s to %s:%d with reason '%s'",
			client->name, client->user->username, client->user->realhost,
			all ? "ALL CLIENTS" : "all new clients",
			jss->server, jss->port, jss->reason);

	sendto_realops("%s", logbuf);
	ircd_log(LOG_ERROR, "%s", logbuf);

	if (all)
		redirect_all_clients();
}
