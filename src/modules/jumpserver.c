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

int m_jumpserver(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_JUMPSERVER 	"JUMPSERVER"
#define TOK_JUMPSERVER 	NULL

ModuleHeader MOD_HEADER(jumpserver)
  = {
	"jumpserver",
	"v1.1",
	"/jumpserver command",
	"3.2-b8-1",
	NULL 
    };

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

JSS *jss=NULL; /** JumpServer Status. NULL=disabled. */

int jumpserver_preconnect(aClient *);

#ifndef ircstrdup
#define ircstrdup(x,y) do { if (x) MyFree(x); if (!y) x = NULL; else x = strdup(y); } while(0)
#endif
#ifndef ircfree
#define ircfree(x) do { if (x) MyFree(x); x = NULL; } while(0)
#endif

MOD_INIT(jumpserver)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PERM, 1);
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
	return MOD_SUCCESS;
}

static int do_jumpserver_exit_client(aClient *sptr)
{
	if (IsSecure(sptr) && jss->ssl_server)
		sendto_one(sptr, rpl_str(RPL_REDIR), me.name,
			BadPtr(sptr->name) ? "*" : sptr->name,
			jss->ssl_server, jss->ssl_port);
	else
		sendto_one(sptr, rpl_str(RPL_REDIR), me.name,
			BadPtr(sptr->name) ? "*" : sptr->name,
			jss->server, jss->port);
 	return exit_client(sptr, sptr, sptr, jss->reason);
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
		ircfree(jss->server);
		ircfree(jss->reason);
		ircfree(jss->ssl_server);
		MyFree(jss);
		jss = NULL;
	}
}

int m_jumpserver(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
char *serv, *sslserv=NULL, *reason, *p, *p2;
int all=0, port=6667, sslport=6697;
char logbuf[512];

	if (!IsOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		if (jss && jss->ssl_server)
			sendnotice(sptr, "JumpServer is \002ENABLED\002 to %s:%d (SSL: %s:%d) with reason '%s'",
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
				sendnotice(sptr, "Invalid SSL serverport specified (%d)", sslport);
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
		snprintf(logbuf, sizeof(logbuf), "%s (%s@%s) added JUMPSERVER redirect for %s to %s:%d [SSL: %s:%d] with reason '%s'",
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
