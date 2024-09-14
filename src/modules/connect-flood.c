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

/* Defines */
#define THROTTLING_HASH_TABLE_SIZE 8192

/* Structs */
typedef struct ThrottlingBucket ThrottlingBucket;
struct ThrottlingBucket
{
	ThrottlingBucket *prev, *next;
	char *ip;
	time_t since;
	char count;
};

/* Variables */
char *siphashkey_throttling = NULL;
ThrottlingBucket **ThrottlingHash = NULL;

/* Forward declaration */
int connect_flood_accept(Client *client);
int connect_flood_dns_finished(Client *client);
int connect_flood_ip_change(Client *client, const char *oldip);
void siphashkey_throttling_free(ModData *m);
void throttlinghash_free(ModData *m);
void add_throttling_timeout_timer(ModuleInfo *modinfo);
int throttle_can_connect(Client *client);
void add_throttling_bucket(Client *client);
EVENT(throttling_check_expire);

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	LoadPersistentPointer(modinfo, siphashkey_throttling, siphashkey_throttling_free);
	if (!siphashkey_throttling)
	{
		siphashkey_throttling = safe_alloc(SIPHASH_KEY_LENGTH);
		siphash_generate_key(siphashkey_throttling);
	}
	LoadPersistentPointer(modinfo, ThrottlingHash, throttlinghash_free);
	if (!ThrottlingHash)
	{
		ThrottlingHash = safe_alloc(sizeof(ThrottlingBucket *) * THROTTLING_HASH_TABLE_SIZE);
	}
	HookAdd(modinfo->handle, HOOKTYPE_ACCEPT, -3000, connect_flood_accept);
	HookAdd(modinfo->handle, HOOKTYPE_DNS_FINISHED, -3000, connect_flood_dns_finished);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, -3000, connect_flood_ip_change);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	add_throttling_timeout_timer(modinfo);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, siphashkey_throttling);
	SavePersistentPointer(modinfo, ThrottlingHash);
	return MOD_SUCCESS;
}

void siphashkey_throttling_free(ModData *m)
{
	safe_free(siphashkey_throttling);
	m->ptr = NULL;
}

void throttlinghash_free(ModData *m)
{
	// FIXME: need to free every throttling bucket in a for loop
	// and then end with this:
	safe_free(ThrottlingHash);
	m->ptr = NULL;
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

void add_throttling_timeout_timer(ModuleInfo *modinfo)
{
	long v;

	if (!THROTTLING_PERIOD)
	{
		v = 120*1000;
	} else
	{
		v = (THROTTLING_PERIOD*1000)/2;
		if (v > 5000)
			v = 5000; /* run at least every 5s */
		if (v < 1000)
			v = 1000; /* run at max once every 1s */
	}

	EventAdd(modinfo->handle, "throttling_check_expire", throttling_check_expire, NULL, v, 0);
}

uint64_t hash_throttling(const char *ip)
{
	return siphash(ip, siphashkey_throttling) % THROTTLING_HASH_TABLE_SIZE;
}

ThrottlingBucket *find_throttling_bucket(Client *client)
{
	int hash = 0;
	ThrottlingBucket *p;
	hash = hash_throttling(client->ip);

	for (p = ThrottlingHash[hash]; p; p = p->next)
	{
		if (!strcmp(p->ip, client->ip))
			return p;
	}

	return NULL;
}

EVENT(throttling_check_expire)
{
	ThrottlingBucket *n, *n_next;
	int i;

	for (i = 0; i < THROTTLING_HASH_TABLE_SIZE; i++)
	{
		for (n = ThrottlingHash[i]; n; n = n_next)
		{
			n_next = n->next;
			if ((TStime() - n->since) > (THROTTLING_PERIOD ? THROTTLING_PERIOD : 15))
			{
				DelListItem(n, ThrottlingHash[i]);
				safe_free(n->ip);
				safe_free(n);
			}
		}
	}
}

void add_throttling_bucket(Client *client)
{
	int hash;
	ThrottlingBucket *n;

	n = safe_alloc(sizeof(ThrottlingBucket));
	n->next = n->prev = NULL;
	safe_strdup(n->ip, client->ip);
	n->since = TStime();
	n->count = 1;
	hash = hash_throttling(client->ip);
	AddListItem(n, ThrottlingHash[hash]);
	return;
}

/** Checks whether the user is connect-flooding.
 * @retval 0 Denied, throttled.
 * @retval 1 Allowed, but known in the list.
 * @retval 2 Allowed, not in list or is an exception.
 * @see add_connection()
 */
int throttle_can_connect(Client *client)
{
	ThrottlingBucket *b;

	if (!THROTTLING_PERIOD || !THROTTLING_COUNT)
		return 2;

	if (!(b = find_throttling_bucket(client)))
		return 1;
	else
	{
		if (find_tkl_exception(TKL_CONNECT_FLOOD, client))
			return 2;
		if (b->count+1 > (THROTTLING_COUNT ? THROTTLING_COUNT : 3))
			return 0;
		b->count++;
		return 2;
	}
}
