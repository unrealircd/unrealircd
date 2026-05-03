/*
 * Unreal Internet Relay Chat Daemon, src/modules/maxperip.c
 * (C) 2025 Bram Matthys and the UnrealIRCd Team
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"maxperip",
	"1.0.0",
	"Limit user connections based on ip address",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Defines and macros */
#define IPUSERS_HASH_TABLE_SIZE 8192

/* Structs */
typedef struct IpUsersBucket IpUsersBucket;
struct IpUsersBucket
{
	IpUsersBucket *prev, *next;
	char rawip[16];
	int local_clients;
	int global_clients;
};

/* Variables */
IpUsersBucket **IpUsersHash_ipv4 = NULL;
IpUsersBucket **IpUsersHash_ipv6 = NULL;
char *siphashkey_ipusers = NULL;

/* Forward declarations */
int maxperip_config_test_allow(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int maxperip_config_run_allow(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr);
void maxperip_postconf(void);
int exceeds_maxperip(Client *client, ConfigItem_allow *aconf);
IpUsersBucket *find_ipusers_bucket(Client *client);
IpUsersBucket *add_ipusers_bucket(Client *client);
void decrease_ipusers_bucket(Client *client);
int decrease_ipusers_bucket_wrapper(Client *client);
static void rebuild_ipusers_buckets(void);
static void free_ipusers_buckets(void);
int stats_maxperip(Client *client, const char *para);
int maxperip_remote_connect(Client *client);
const char *maxperip_allow_client(Client *client, ConfigItem_allow *aconf);
int _get_connections_from_ip(Client *client);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, maxperip_config_test_allow);
	EfunctionAdd(modinfo->handle, EFUNC_GET_CONNECTIONS_FROM_IP, _get_connections_from_ip);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	siphashkey_ipusers = safe_alloc(SIPHASH_KEY_LENGTH);
	siphash_generate_key(siphashkey_ipusers);
	IpUsersHash_ipv4 = safe_alloc(sizeof(IpUsersBucket *) * IPUSERS_HASH_TABLE_SIZE);
	IpUsersHash_ipv6 = safe_alloc(sizeof(IpUsersBucket *) * IPUSERS_HASH_TABLE_SIZE);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN_EX, 0, maxperip_config_run_allow);
	HookAdd(modinfo->handle, HOOKTYPE_FREE_USER, 0, decrease_ipusers_bucket_wrapper);
	HookAdd(modinfo->handle, HOOKTYPE_STATS, 0, stats_maxperip);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, maxperip_remote_connect);
	HookAddConstString(modinfo->handle, HOOKTYPE_ALLOW_CLIENT, 0, maxperip_allow_client);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	maxperip_postconf();
	rebuild_ipusers_buckets();
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_ipusers_buckets();
	safe_free(siphashkey_ipusers);
	return MOD_SUCCESS;
}

int maxperip_config_test_allow(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	int ext = 0;

	if ((type != CONFIG_ALLOW_BLOCK) || !ce || !ce->name)
		return 0;

	if (!strcmp(ce->name, "maxperip") || !strcmp(ce->name, "global-maxperip"))
	{
		if (!ce->value)
		{
			config_error("%s:%i: missing parameter", ce->file->filename, ce->line_number);
			errors++;
		} else {
			int v = atoi(ce->value);
			if ((v <= 0) || (v > 1000000))
			{
				config_error("%s:%i: allow::%s with illegal value (must be 1-1000000)",
					ce->file->filename, ce->line_number, ce->name);
				errors++;
			}
		}
	} else {
		return 0; /* Unknown option for us */
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int maxperip_config_run_allow(ConfigFile *cf, ConfigEntry *ce, int type, void *ptr)
{
	ConfigEntry *cep, *cepp;
	ConfigItem_allow *allow = (ConfigItem_allow *)ptr;

	if ((type != CONFIG_ALLOW_BLOCK) || !ce || !ce->name)
		return 0;

	if (!strcmp(ce->name, "maxperip"))
	{
		allow->maxperip = atoi(ce->value);
	} else
	if (!strcmp(ce->name, "global-maxperip"))
	{
		allow->global_maxperip = atoi(ce->value);
	} else
	{
		return 0; /* Unknown option for us */
	}

	return 1; /* Handled */
}

void maxperip_postconf(void)
{
	ConfigItem_allow *allow;
	for (allow = conf_allow; allow; allow = allow->next)
	{
		/* Default: global-maxperip = maxperip+1 */
		if (allow->global_maxperip == 0)
			allow->global_maxperip = allow->maxperip+1;

		/* global-maxperip < maxperip makes no sense */
		if (allow->global_maxperip < allow->maxperip)
			allow->global_maxperip = allow->maxperip;
	}
}

/** Build the rawip used to identify this client's ipusers bucket.
 *
 * For IPv4: copies the 4 raw bytes of client->rawip.
 * For IPv6: copies and masks client->rawip according to
 *   iConf.default_ipv6_clone_mask, so all addresses within the
 *   same /N share one bucket.
 *
 * The 'rawip' buffer must be at least 16 bytes (only first 4 used for IPv4).
 */
static void make_ipusers_rawip(Client *client, char *rawip)
{
	if (IsIPV6(client))
		mask_ipv6_rawip(client->rawip, iConf.default_ipv6_clone_mask, rawip);
	else
		memcpy(rawip, client->rawip, 4);
}

uint64_t hash_ipusers(Client *client, const char *rawip)
{
	if (IsIPV6(client))
		return siphash_raw(rawip, 16, siphashkey_ipusers) % IPUSERS_HASH_TABLE_SIZE;
	else
		return siphash_raw(rawip, 4, siphashkey_ipusers) % IPUSERS_HASH_TABLE_SIZE;
}

IpUsersBucket *find_ipusers_bucket(Client *client)
{
	int hash;
	IpUsersBucket *p;
	char rawip[16];

	make_ipusers_rawip(client, rawip);
	hash = hash_ipusers(client, rawip);

	if (IsIPV6(client))
	{
		for (p = IpUsersHash_ipv6[hash]; p; p = p->next)
			if (memcmp(p->rawip, rawip, 16) == 0)
				return p;
	} else {
		for (p = IpUsersHash_ipv4[hash]; p; p = p->next)
			if (memcmp(p->rawip, rawip, 4) == 0)
				return p;
	}

	return NULL;
}

/* (wrapper needed because hook has return type 'int' and function is 'void' */
int decrease_ipusers_bucket_wrapper(Client *client)
{
	decrease_ipusers_bucket(client);
	return 0;
}

IpUsersBucket *add_ipusers_bucket(Client *client)
{
	int hash;
	IpUsersBucket *n;
	char rawip[16];

	make_ipusers_rawip(client, rawip);
	hash = hash_ipusers(client, rawip);

	n = safe_alloc(sizeof(IpUsersBucket));
	if (IsIPV6(client))
	{
		memcpy(n->rawip, rawip, 16);
		AddListItem(n, IpUsersHash_ipv6[hash]);
	} else {
		memcpy(n->rawip, rawip, 4);
		AddListItem(n, IpUsersHash_ipv4[hash]);
	}
	return n;
}

void decrease_ipusers_bucket(Client *client)
{
	int hash;
	IpUsersBucket *p;
	char rawip[16];

	if (!(client->flags & CLIENT_FLAG_IPUSERS_BUMPED))
		return; /* nothing to do */

	client->flags &= ~CLIENT_FLAG_IPUSERS_BUMPED;

	make_ipusers_rawip(client, rawip);
	hash = hash_ipusers(client, rawip);

	if (IsIPV6(client))
	{
		for (p = IpUsersHash_ipv6[hash]; p; p = p->next)
			if (memcmp(p->rawip, rawip, 16) == 0)
				break;
	} else {
		for (p = IpUsersHash_ipv4[hash]; p; p = p->next)
			if (memcmp(p->rawip, rawip, 4) == 0)
				break;
	}

	if (!p)
	{
		unreal_log(ULOG_INFO, "user", "BUG_DECREASE_IPUSERS_BUCKET", client,
		           "[BUG] decrease_ipusers_bucket() called but bucket is gone for client $client.details");
		return;
	}

	p->global_clients--;
	if (MyConnect(client))
		p->local_clients--;

	if ((p->global_clients == 0) && (p->local_clients == 0))
	{
		if (IsIPV6(client))
			DelListItem(p, IpUsersHash_ipv6[hash]);
		else
			DelListItem(p, IpUsersHash_ipv4[hash]);
		safe_free(p);
	}
}

/* Restore the buckets by walking current clients with the bumped flag.
 * Cost is negligible (a few tens of milliseconds even for ~10k clients).
 */
static void rebuild_ipusers_buckets(void)
{
	Client *client;
	IpUsersBucket *bucket;

	list_for_each_entry(client, &client_list, client_node)
	{
		if (!(client->flags & CLIENT_FLAG_IPUSERS_BUMPED))
			continue;
		if (!client->ip)
			continue; /* defensive */
		bucket = find_ipusers_bucket(client);
		if (!bucket)
			bucket = add_ipusers_bucket(client);
		bucket->global_clients++;
		if (MyConnect(client))
			bucket->local_clients++;
	}
	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (!(client->flags & CLIENT_FLAG_IPUSERS_BUMPED))
			continue;
		if (!client->ip)
			continue;
		bucket = find_ipusers_bucket(client);
		if (!bucket)
			bucket = add_ipusers_bucket(client);
		bucket->global_clients++;
		if (MyConnect(client))
			bucket->local_clients++;
	}
}

/* Free every bucket in both hash tables, then free the tables themselves. */
static void free_ipusers_buckets(void)
{
	int i;
	IpUsersBucket *p, *next;

	for (i = 0; i < IPUSERS_HASH_TABLE_SIZE; i++)
	{
		for (p = IpUsersHash_ipv4[i]; p; p = next)
		{
			next = p->next;
			safe_free(p);
		}
		IpUsersHash_ipv4[i] = NULL;
		for (p = IpUsersHash_ipv6[i]; p; p = next)
		{
			next = p->next;
			safe_free(p);
		}
		IpUsersHash_ipv6[i] = NULL;
	}
	safe_free(IpUsersHash_ipv4);
	safe_free(IpUsersHash_ipv6);
}

int stats_maxperip(Client *client, const char *para)
{
	int i;
	IpUsersBucket *e;
	char ipbuf[256];
	const char *ip;

	/* '/STATS 8' or '/STATS maxperip' is for us... */
	if (strcmp(para, "8") && strcasecmp(para, "maxperip"))
		return 0;

	if (!ValidatePermissionsForPath("server:info:stats",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return 0;
	}

	sendtxtnumeric(client, "MaxPerIp IPv4 hash table:");
	for (i=0; i < IPUSERS_HASH_TABLE_SIZE; i++)
	{
		for (e = IpUsersHash_ipv4[i]; e; e = e->next)
		{
			ip = inetntop(AF_INET, e->rawip, ipbuf, sizeof(ipbuf));
			if (!ip)
				ip = "<invalid>";
			sendtxtnumeric(client, "IPv4 #%d %s: %d local / %d global",
				       i, ip, e->local_clients, e->global_clients);
		}
	}

	sendtxtnumeric(client, "MaxPerIp IPv6 hash table:");
	for (i=0; i < IPUSERS_HASH_TABLE_SIZE; i++)
	{
		for (e = IpUsersHash_ipv6[i]; e; e = e->next)
		{
			ip = inetntop(AF_INET6, e->rawip, ipbuf, sizeof(ipbuf));
			if (!ip)
				ip = "<invalid>";
			sendtxtnumeric(client, "IPv6 #%d %s: %d local / %d global",
				       i, ip, e->local_clients, e->global_clients);
		}
	}

	return 1;
}

/** Returns 1 if allow::maxperip is exceeded by 'client' */
int exceeds_maxperip(Client *client, ConfigItem_allow *aconf)
{
	Client *acptr;
	IpUsersBucket *bucket;

	if (!client->ip)
		return 0; /* eg. services */

	bucket = find_ipusers_bucket(client);
	if (!bucket)
	{
		client->flags |= CLIENT_FLAG_IPUSERS_BUMPED;
		bucket = add_ipusers_bucket(client);
		bucket->global_clients = 1;
		if (MyConnect(client))
			bucket->local_clients = 1;
		return 0;
	}

	/* Bump if we haven't done so yet
	 * (Actually not sure if this can ever be false, but...
	 *  who knows with some 3rd party or some future change)
	 */
	if (!(client->flags & CLIENT_FLAG_IPUSERS_BUMPED))
	{
		bucket->global_clients++;
		if (MyConnect(client))
			bucket->local_clients++;
		client->flags |= CLIENT_FLAG_IPUSERS_BUMPED;
	}

	if (find_tkl_exception(TKL_MAXPERIP, client))
		return 0; /* exempt */

	if (aconf)
	{
		if ((bucket->local_clients > aconf->maxperip) ||
		    (bucket->global_clients > aconf->global_maxperip))
		{
			return 1;
		}
	}

	return 0;
}

/** Called for remote connects, to track global max restrictions */
int maxperip_remote_connect(Client *client)
{
	exceeds_maxperip(client, NULL);
	return 0;
}

/** Called from AllowClient(), to deal with restrictions */
const char *maxperip_allow_client(Client *client, ConfigItem_allow *aconf)
{
	if (exceeds_maxperip(client, aconf))
		return iConf.reject_message_too_many_connections;
	return NULL;
}

/** Return the number of connections from the same IP as 'client' */
int _get_connections_from_ip(Client *client)
{
	IpUsersBucket *bucket;

	if (!client->ip)
		return 0;

	bucket = find_ipusers_bucket(client);
	if (!bucket)
		return 0;

	return bucket->global_clients;
}
