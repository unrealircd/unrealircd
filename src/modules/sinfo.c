/*
 * cmd_sinfo - Server information
 * (C) Copyright 2019 Bram Matthys (Syzop) and the UnrealIRCd team.
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"sinfo",
	"5.0",
	"Server information",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
CMD_FUNC(cmd_sinfo);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "SINFO", cmd_sinfo, MAXPARA, CMD_USER|CMD_SERVER);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/** SINFO server-to-server command.
 * Technical documentation is available at:
 * https://www.unrealircd.org/docs/Server_protocol:SINFO_command
 * ^ contains important remarks regarding when to send it and when not.
 */
CMD_FUNC(sinfo_server)
{
	char buf[512];

	if (MyConnect(client))
	{
		/* It is a protocol violation to send an SINFO for yourself,
		 * eg if you are server 001, then you cannot send :001 SINFO ....
		 * Exiting the client may seem harsh, but this way we force users
		 * to use the correct protocol. If we would not do this then some
		 * services coders may think they should use only SINFO while in
		 * fact for directly connected servers they should use things like
		 * PROTOCTL CHANMODES=... USERMODES=... NICKCHARS=.... etc, and
		 * failure to do so will lead to potential desyncs or other major
		 * issues.
		 */
		exit_client(client, NULL, "Protocol error: you cannot send SINFO about yourself");
		return;
	}

	/* :SID SINFO up_since protocol umodes chanmodes nickchars :software name
	 *               1        2        3      4        5        6 (last one)
	 * If we extend it then 'software name' will still be the last one, so
	 * it may become 7, 8 or 9. New elements are inserted right before it.
	 */

	if ((parc < 6) || BadPtr(parv[6]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SINFO");
		return;
	}

	client->server->boottime = atol(parv[1]);
	client->server->features.protocol = atoi(parv[2]);

	if (!strcmp(parv[3], "*"))
		safe_free(client->server->features.usermodes);
	else
		safe_strdup(client->server->features.usermodes, parv[3]);

	if (!strcmp(parv[4], "*"))
	{
		safe_free(client->server->features.chanmodes[0]);
		safe_free(client->server->features.chanmodes[1]);
		safe_free(client->server->features.chanmodes[2]);
		safe_free(client->server->features.chanmodes[3]);
	} else {
		parse_chanmodes_protoctl(client, parv[4]);
	}

	if (!strcmp(parv[5], "*"))
		safe_free(client->server->features.nickchars);
	else
		safe_strdup(client->server->features.nickchars, parv[5]);

	/* Software is always the last parameter. It is currently parv[6]
	 * but may change later. So always use parv[parc-1].
	 */
	if (!strcmp(parv[parc-1], "*"))
		safe_free(client->server->features.software);
	else
		safe_strdup(client->server->features.software, parv[parc-1]);

	if (is_services_but_not_ulined(client))
	{
		char buf[512];
		snprintf(buf, sizeof(buf), "Services detected but no ulines { } for server name %s", client->name);
		exit_client_ex(client, &me, NULL, buf);
		return;
	}

	/* Broadcast to 'the other side' of the net */
	concat_params(buf, sizeof(buf), parc, parv);
	sendto_server(client, 0, 0, NULL, ":%s SINFO %s", client->id, buf);
}

#define SafeDisplayStr(x)  ((x && *(x)) ? (x) : "-")
CMD_FUNC(sinfo_user)
{
	Client *acptr;

	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		sendtxtnumeric(client, "*** Server %s:", acptr->name);
		sendtxtnumeric(client, "Protocol: %d",
		               acptr->server->features.protocol);
		sendtxtnumeric(client, "Software: %s",
		               SafeDisplayStr(acptr->server->features.software));
		if (!acptr->server->boottime)
		{
			sendtxtnumeric(client, "Up since: -");
			sendtxtnumeric(client, "Uptime: -");
		} else {
			sendtxtnumeric(client, "Up since: %s",
			               pretty_date(acptr->server->boottime));
			sendtxtnumeric(client, "Uptime: %s",
			               pretty_time_val(TStime() - acptr->server->boottime));
		}
		sendtxtnumeric(client, "User modes: %s",
		               SafeDisplayStr(acptr->server->features.usermodes));
		if (!acptr->server->features.chanmodes[0])
		{
			sendtxtnumeric(client, "Channel modes: -");
		} else {
			sendtxtnumeric(client, "Channel modes: %s,%s,%s,%s",
			               SafeDisplayStr(acptr->server->features.chanmodes[0]),
			               SafeDisplayStr(acptr->server->features.chanmodes[1]),
			               SafeDisplayStr(acptr->server->features.chanmodes[2]),
			               SafeDisplayStr(acptr->server->features.chanmodes[3]));
		}
		sendtxtnumeric(client, "Allowed nick characters: %s",
		               SafeDisplayStr(acptr->server->features.nickchars));
	}
}

CMD_FUNC(cmd_sinfo)
{
	if (IsServer(client))
		CALL_CMD_FUNC(sinfo_server);
	else if (MyUser(client))
		CALL_CMD_FUNC(sinfo_user);
}
