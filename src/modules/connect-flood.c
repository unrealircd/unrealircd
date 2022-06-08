/*
 * Connection throttling (set::anti-flood::connect-flood)
 * (C) Copyright 2022- Bram Matthys and the UnrealIRCd team.
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"connect-flood",
	"6.0.0",
	"set::anti-flood::connect-flood",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declaration */
int connect_flood_accept(Client *client);
int connect_flood_ip_change(Client *client, const char *oldip);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_ACCEPT, -3000, connect_flood_accept);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, -3000, connect_flood_ip_change);

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

int connect_flood_throttle(Client *client, int exitflags)
{
	int val;
	char zlinebuf[512];

	if (!(val = throttle_can_connect(client)))
	{
		if (exitflags & NO_EXIT_CLIENT)
		{
			ircsnprintf(zlinebuf, sizeof(zlinebuf),
				"ERROR :Closing Link: [%s] (Throttled: Reconnecting too fast) - "
				"Email %s for more information.\r\n",
				client->ip, KLINE_ADDRESS);
			(void)send(client->local->fd, zlinebuf, strlen(zlinebuf), 0);
			return HOOK_DENY;
		} else {
			ircsnprintf(zlinebuf, sizeof(zlinebuf),
				    "Throttled: Reconnecting too fast - "
				    "Email %s for more information.",
				    KLINE_ADDRESS);
			exit_client(client, NULL, zlinebuf);
			return HOOK_DENY;
		}
	}
	else if (val == 1)
		add_throttling_bucket(client);

	return 0;
}

int connect_flood_accept(Client *client)
{
	if (client->local->listener->options & LISTENER_NO_CHECK_CONNECT_FLOOD)
		return 0;
	return connect_flood_throttle(client, NO_EXIT_CLIENT);
}

int connect_flood_ip_change(Client *client, const char *oldip)
{
	return connect_flood_throttle(client, 0);
}
