/*
 *   IRC - Internet Relay Chat, src/modules/sasl.c
 *   (C) 2012 The UnrealIRCd Team
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

ModuleHeader MOD_HEADER
  = {
	"sasl",
	"5.2.1",
	"SASL", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void saslmechlist_free(ModData *m);
const char *saslmechlist_serialize(ModData *m);
void saslmechlist_unserialize(const char *str, ModData *m);
const char *sasl_capability_parameter(Client *client);
int sasl_server_synced(Client *client);
int sasl_account_login(Client *client, MessageTag *mtags);
EVENT(sasl_timeout);

/* Macros */
#define MSG_AUTHENTICATE "AUTHENTICATE"
#define MSG_SASL "SASL"
#define AGENT_SID(agent_p)	(agent_p->user != NULL ? agent_p->user->server : agent_p->name)

/* Variables */
long CAP_SASL = 0L;

/*
 * The following people were involved in making the previous iteration of SASL over
 * IRC which allowed psuedo-identifiers:
 *
 * danieldg, Daniel de Graff <danieldg@inspircd.org>
 * jilles, Jilles Tjoelker <jilles@stack.nl>
 * Jobe, Matthew Beeching <jobe@mdbnet.co.uk>
 * gxti, Michael Tharp <gxti@partiallystapled.com>
 * nenolod, William Pitcock <nenolod@dereferenced.org>
 *
 * Thanks also to all of the client authors which have implemented SASL in their
 * clients.  With the backwards-compatibility layer allowing "lightweight" SASL
 * implementations, we now truly have a universal authentication mechanism for
 * IRC.
 */

int sasl_account_login(Client *client, MessageTag *mtags)
{
	if (!MyConnect(client))
		return 0;

	/* Notify user */
	if (IsLoggedIn(client))
	{
		sendnumeric(client, RPL_LOGGEDIN,
			BadPtr(client->name) ? "*" : client->name,
			BadPtr(client->user->username) ? "*" : client->user->username,
			BadPtr(client->user->realhost) ? "*" : client->user->realhost,
			client->user->account, client->user->account);
	}
	else
	{
		sendnumeric(client, RPL_LOGGEDOUT,
			BadPtr(client->name) ? "*" : client->name,
			BadPtr(client->user->username) ? "*" : client->user->username,
			BadPtr(client->user->realhost) ? "*" : client->user->realhost);
	}
	return 0;
}


/*
 * SASL message
 *
 * parv[1]: distribution mask
 * parv[2]: target
 * parv[3]: mode/state
 * parv[4]: data
 * parv[5]: out-of-bound data
 */
CMD_FUNC(cmd_sasl)
{
	if (!SASL_SERVER || MyUser(client) || (parc < 4) || !parv[4])
		return;

	if (!strcasecmp(parv[1], me.name) || !strncmp(parv[1], me.id, 3))
	{
		Client *target;

		target = find_client(parv[2], NULL);
		if (!target || !MyConnect(target))
			return;

		if (target->user == NULL)
			make_user(target);

		/* reject if another SASL agent is answering */
		if (*target->local->sasl_agent && strcasecmp(client->name, target->local->sasl_agent))
			return;
		else
			strlcpy(target->local->sasl_agent, client->name, sizeof(target->local->sasl_agent));

		if (*parv[3] == 'C')
		{
			RunHookReturn(HOOKTYPE_SASL_CONTINUATION, !=0, target, parv[4]);
			sendto_one(target, NULL, "AUTHENTICATE %s", parv[4]);
		}
		else if (*parv[3] == 'D')
		{
			*target->local->sasl_agent = '\0';
			if (*parv[4] == 'F')
			{
				target->local->sasl_sent_time = 0;
				add_fake_lag(target, 7000); /* bump fakelag due to failed authentication attempt */
				RunHookReturn(HOOKTYPE_SASL_RESULT, !=0, target, 0);
				sendnumeric(target, ERR_SASLFAIL);
			}
			else if (*parv[4] == 'S')
			{
				target->local->sasl_sent_time = 0;
				target->local->sasl_complete++;
				RunHookReturn(HOOKTYPE_SASL_RESULT, !=0, target, 1);
				sendnumeric(target, RPL_SASLSUCCESS);
			}
		}
		else if (*parv[3] == 'M')
			sendnumeric(target, RPL_SASLMECHS, parv[4]);

		return;
	}

	/* not for us; propagate. */
	sendto_server(client, 0, 0, NULL, ":%s SASL %s %s %c %s %s",
	    client->name, parv[1], parv[2], *parv[3], parv[4], parc > 5 ? parv[5] : "");
}

/*
 * AUTHENTICATE message
 *
 * parv[1]: data
 */
CMD_FUNC(cmd_authenticate)
{
	Client *agent_p = NULL;

	/* Failing to use CAP REQ for sasl is a protocol violation. */
	if (!SASL_SERVER || !MyConnect(client) || BadPtr(parv[1]) || !HasCapability(client, "sasl"))
		return;

	if ((parv[1][0] == ':') || strchr(parv[1], ' '))
	{
		sendnumeric(client, ERR_CANNOTDOCOMMAND, "AUTHENTICATE", "Invalid parameter");
		return;
	}

	if (strlen(parv[1]) > 400)
	{
		sendnumeric(client, ERR_SASLTOOLONG);
		return;
	}

	if (*client->local->sasl_agent)
		agent_p = find_client(client->local->sasl_agent, NULL);

	if (agent_p == NULL)
	{
		char *addr = BadPtr(client->ip) ? "0" : client->ip;
		const char *certfp = moddata_client_get(client, "certfp");

		sendto_server(NULL, 0, 0, NULL, ":%s SASL %s %s H %s %s",
		    me.name, SASL_SERVER, client->id, addr, addr);

		if (certfp)
			sendto_server(NULL, 0, 0, NULL, ":%s SASL %s %s S %s %s",
			    me.name, SASL_SERVER, client->id, parv[1], certfp);
		else
			sendto_server(NULL, 0, 0, NULL, ":%s SASL %s %s S %s",
			    me.name, SASL_SERVER, client->id, parv[1]);
	}
	else
		sendto_server(NULL, 0, 0, NULL, ":%s SASL %s %s C %s",
		    me.name, AGENT_SID(agent_p), client->id, parv[1]);

	client->local->sasl_out++;
	client->local->sasl_sent_time = TStime();
}

static int abort_sasl(Client *client)
{
	client->local->sasl_sent_time = 0;

	if (client->local->sasl_out == 0 || client->local->sasl_complete)
		return 0;

	client->local->sasl_out = client->local->sasl_complete = 0;
	sendnumeric(client, ERR_SASLABORTED);

	if (*client->local->sasl_agent)
	{
		Client *agent_p = find_client(client->local->sasl_agent, NULL);

		if (agent_p != NULL)
		{
			sendto_server(NULL, 0, 0, NULL, ":%s SASL %s %s D A",
			    me.name, AGENT_SID(agent_p), client->id);
			return 0;
		}
	}

	sendto_server(NULL, 0, 0, NULL, ":%s SASL * %s D A", me.name, client->id);
	return 0;
}

/** Is this capability visible?
 * Note that 'client' may be NULL when queried from CAP DEL / CAP NEW
 */
int sasl_capability_visible(Client *client)
{
	if (!SASL_SERVER || !find_server(SASL_SERVER, NULL))
		return 0;

	/* Don't advertise 'sasl' capability if we are going to reject the
	 * user anyway due to set::plaintext-policy. This way the client
	 * won't attempt SASL authentication and thus it prevents the client
	 * from sending the password unencrypted (in case of method PLAIN).
	 */
	if (client && !IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_user == POLICY_DENY))
		return 0;

	/* Similarly, don't advertise when we are going to reject the user
	 * due to set::outdated-tls-policy.
	 */
	if (IsSecure(client) && (iConf.outdated_tls_policy_user == POLICY_DENY) && outdated_tls_client(client))
		return 0;

	return 1;
}

int sasl_connect(Client *client)
{
	return abort_sasl(client);
}

int sasl_quit(Client *client, MessageTag *mtags, const char *comment)
{
	return abort_sasl(client);
}

int sasl_server_quit(Client *client, MessageTag *mtags)
{
	if (!SASL_SERVER)
		return 0;

	/* If the set::sasl-server is gone, let everyone know 'sasl' is no longer available */
	if (!strcasecmp(client->name, SASL_SERVER))
		send_cap_notify(0, "sasl");

	return 0;
}

void auto_discover_sasl_server(int justlinked)
{
	if (!SASL_SERVER && SERVICES_NAME)
	{
		Client *client = find_server(SERVICES_NAME, NULL);
		if (client && moddata_client_get(client, "saslmechlist"))
		{
			/* SASL server found */
			if (justlinked)
			{
				unreal_log(ULOG_INFO, "config", "SASL_SERVER_AUTODETECT", client,
				           "Services server $client provides SASL authentication, good! "
				           "I'm setting set::sasl-server to \"$client\" internally.");
			}
			safe_strdup(SASL_SERVER, SERVICES_NAME);
			if (justlinked)
				sasl_server_synced(client);
		}
	}
}

int sasl_server_synced(Client *client)
{
	if (!SASL_SERVER)
	{
		auto_discover_sasl_server(1);
		return 0;
	}

	/* If the set::sasl-server is gone, let everyone know 'sasl' is no longer available */
	if (!strcasecmp(client->name, SASL_SERVER))
		send_cap_notify(1, "sasl");

	return 0;
}

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, MSG_SASL, cmd_sasl, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_AUTHENTICATE, cmd_authenticate, MAXPARA, CMD_UNREGISTERED|CMD_USER);

	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, sasl_connect);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, sasl_quit);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_QUIT, 0, sasl_server_quit);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_SYNCED, 0, sasl_server_synced);
	HookAdd(modinfo->handle, HOOKTYPE_ACCOUNT_LOGIN, 0, sasl_account_login);

	memset(&cap, 0, sizeof(cap));
	cap.name = "sasl";
	cap.visible = sasl_capability_visible;
	cap.parameter = sasl_capability_parameter;
	ClientCapabilityAdd(modinfo->handle, &cap, &CAP_SASL);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "saslmechlist";
	mreq.free = saslmechlist_free;
	mreq.serialize = saslmechlist_serialize;
	mreq.unserialize = saslmechlist_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.self_write = 1;
	mreq.type = MODDATATYPE_CLIENT;
	ModDataAdd(modinfo->handle, mreq);

	EventAdd(modinfo->handle, "sasl_timeout", sasl_timeout, NULL, 2000, 0);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	auto_discover_sasl_server(0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void saslmechlist_free(ModData *m)
{
	safe_free(m->str);
}

const char *saslmechlist_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void saslmechlist_unserialize(const char *str, ModData *m)
{
	safe_strdup(m->str, str);
}

const char *sasl_capability_parameter(Client *client)
{
	Client *server;

	if (SASL_SERVER)
	{
		server = find_server(SASL_SERVER, NULL);
		if (server)
			return moddata_client_get(server, "saslmechlist"); /* NOTE: could still return NULL */
	}

	return NULL;
}

EVENT(sasl_timeout)
{
	Client *client;

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (client->local->sasl_sent_time &&
		    (TStime() - client->local->sasl_sent_time > iConf.sasl_timeout))
		{
			sendnotice(client, "SASL request timed out (server or client misbehaving) -- aborting SASL and continuing connection...");
			abort_sasl(client);
		}
	}
}
