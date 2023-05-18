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
int connect_flood_dns_finished(Client *client);
int connect_flood_ip_change(Client *client, const char *oldip);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	HookAdd(modinfo->handle, HOOKTYPE_ACCEPT, -3000, connect_flood_accept);
	HookAdd(modinfo->handle, HOOKTYPE_DNS_FINISHED, -3000, connect_flood_dns_finished);
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
		ircsnprintf(zlinebuf, sizeof(zlinebuf),
			    "Throttled: Reconnecting too fast - "
			    "Email %s for more information.",
			    KLINE_ADDRESS);
		/* There are two reasons why we can't use exit_client() here:
		 * 1) Because the HOOKTYPE_IP_CHANGE call may be too deep.
		 *    Eg: read_packet -> webserver_packet_in ->
		 *    webserver_handle_request_header -> webserver_handle_request ->
		 *    RunHook().... and then returning without touching anything
		 *    after an exit_client() would not be feasible.
		 * 2) Because in HOOKTYPE_ACCEPT we always need to use dead_socket
		 *    if we want to print a friendly message to TLS users.
		 */
		dead_socket(client, zlinebuf);
		return HOOK_DENY;
	}
	else if (val == 1)
		add_throttling_bucket(client);

	return 0;
}

int connect_flood_accept(Client *client)
{
	if (!quick_close)
		return 0; /* defer to connect_flood_dns_finished so DNS on except ban works */

	if (client->local->listener->options & LISTENER_NO_CHECK_CONNECT_FLOOD)
		return 0;

	client->flags |= CLIENT_FLAG_CONNECT_FLOOD_CHECKED;
	return connect_flood_throttle(client, NO_EXIT_CLIENT);
}

int connect_flood_dns_finished(Client *client)
{
	if (client->flags & CLIENT_FLAG_CONNECT_FLOOD_CHECKED)
		return 0;
	if (client->local->listener->options & LISTENER_NO_CHECK_CONNECT_FLOOD)
		return 0;
	return connect_flood_throttle(client, NO_EXIT_CLIENT);
}

int connect_flood_ip_change(Client *client, const char *oldip)
{
	return connect_flood_throttle(client, 0);
}
