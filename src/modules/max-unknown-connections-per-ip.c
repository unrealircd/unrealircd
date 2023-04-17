/*
 * Connection throttling (set::max-unknown-connections-per-ip)
 * (C) Copyright 2022- Bram Matthys and the UnrealIRCd team.
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"max-unknown-connections-per-ip",
	"6.0.0",
	"set::max-unknown-connections-per-ip",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declaration */
int max_unknown_connections_accept(Client *client);
int max_unknown_connections_ip_change(Client *client, const char *oldip);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_ACCEPT, -2000, max_unknown_connections_accept);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, -2000, max_unknown_connections_ip_change);

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

/** This checks set::max-unknown-connections-per-ip,
 * which is an important safety feature.
 */
static int check_too_many_unknown_connections(Client *client)
{
	int cnt = 1;
	Client *c;

	if (!find_tkl_exception(TKL_CONNECT_FLOOD, client))
	{
		list_for_each_entry(c, &unknown_list, lclient_node)
		{
			if (!IsRPC(client) && !strcmp(client->ip,GetIP(c)))
			{
				cnt++;
				if (cnt > iConf.max_unknown_connections_per_ip)
					return 1;
			}
		}
	}

	return 0;
}

int max_unknown_connections_accept(Client *client)
{
	if (client->local->listener->options & LISTENER_NO_CHECK_CONNECT_FLOOD)
		return 0;

	/* Check set::max-unknown-connections-per-ip */
	if (check_too_many_unknown_connections(client))
	{
		send_raw_direct(client, "ERROR :Closing Link: [%s] (Too many unknown connections from your IP)", client->ip);
		return HOOK_DENY;
	}

	return 0;
}

int max_unknown_connections_ip_change(Client *client, const char *oldip)
{
	/* Check set::max-unknown-connections-per-ip */
	if (check_too_many_unknown_connections(client))
	{
		sendto_one(client, NULL, "ERROR :Closing Link: [%s] (Too many unknown connections from your IP)", client->ip);
		return HOOK_DENY;
	}

	return 0;
}
