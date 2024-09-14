/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/hash.c
 *   Copyright (C) 1991 Darren Reed
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

/* Next #define's, the siphash_raw() and siphash_nocase() functions are based
 * on the SipHash reference C implementation to which the following applies:
 * Copyright (c) 2012-2016 Jean-Philippe Aumasson
 *  <jeanphilippe.aumasson@gmail.com>
 * Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
 * Further enhancements were made by:
 * Copyright (c) 2017 Salvatore Sanfilippo <antirez@gmail.com>
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 * You should have received a copy of the CC0 Public Domain Dedication along
 * with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 *
 * In addition to above, Bram Matthys (Syzop), did some minor enhancements,
 * such as dropping the uint8_t stuff (in UnrealIRCd char is always unsigned)
 * and getting rid of the length argument.
 *
 * The end result are simple functions for API end-users and we encourage
 * everyone to use these two hash functions everywhere in UnrealIRCd.
 */

#define ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))

#define U32TO8_LE(p, v)                                                        \
    (p)[0] = (char)((v));                                                   \
    (p)[1] = (char)((v) >> 8);                                              \
    (p)[2] = (char)((v) >> 16);                                             \
    (p)[3] = (char)((v) >> 24);

#define U64TO8_LE(p, v)                                                        \
    U32TO8_LE((p), (uint32_t)((v)));                                           \
    U32TO8_LE((p) + 4, (uint32_t)((v) >> 32));

#define U8TO64_LE(p)                                                           \
    (((uint64_t)((p)[0])) | ((uint64_t)((p)[1]) << 8) |                        \
     ((uint64_t)((p)[2]) << 16) | ((uint64_t)((p)[3]) << 24) |                 \
     ((uint64_t)((p)[4]) << 32) | ((uint64_t)((p)[5]) << 40) |                 \
     ((uint64_t)((p)[6]) << 48) | ((uint64_t)((p)[7]) << 56))

#define U8TO64_LE_NOCASE(p)                                                    \
    (((uint64_t)(tolower((p)[0]))) |                                           \
     ((uint64_t)(tolower((p)[1])) << 8) |                                      \
     ((uint64_t)(tolower((p)[2])) << 16) |                                     \
     ((uint64_t)(tolower((p)[3])) << 24) |                                     \
     ((uint64_t)(tolower((p)[4])) << 32) |                                              \
     ((uint64_t)(tolower((p)[5])) << 40) |                                              \
     ((uint64_t)(tolower((p)[6])) << 48) |                                              \
     ((uint64_t)(tolower((p)[7])) << 56))

#define SIPROUND                                                               \
    do {                                                                       \
        v0 += v1;                                                              \
        v1 = ROTL(v1, 13);                                                     \
        v1 ^= v0;                                                              \
        v0 = ROTL(v0, 32);                                                     \
        v2 += v3;                                                              \
        v3 = ROTL(v3, 16);                                                     \
        v3 ^= v2;                                                              \
        v0 += v3;                                                              \
        v3 = ROTL(v3, 21);                                                     \
        v3 ^= v0;                                                              \
        v2 += v1;                                                              \
        v1 = ROTL(v1, 17);                                                     \
        v1 ^= v2;                                                              \
        v2 = ROTL(v2, 32);                                                     \
    } while (0)

/** Generic hash function in UnrealIRCd - raw version.
 * Note that you probably want siphash() or siphash_nocase() instead.
 * @param in    The data to hash
 * @param inlen The length of the data
 * @param k     The key to use for hashing (SIPHASH_KEY_LENGTH bytes,
 *              which is actually 16, not NUL terminated)
 * @returns Hash result as a 64 bit unsigned integer.
 * @note  The key (k) should be random and must stay the same for
 *        as long as you use the function for your specific hash table.
 *        Simply use the following on boot: siphash_generate_key(k);
 *
 *        This siphash_raw() version is meant for non-strings,
 *        such as raw IP address structs and such.
 */
uint64_t siphash_raw(const char *in, size_t inlen, const char *k)
{
    uint64_t hash;
    char *out = (char*) &hash;
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;
    uint64_t k0 = U8TO64_LE(k);
    uint64_t k1 = U8TO64_LE(k + 8);
    uint64_t m;
    const char *end = in + inlen - (inlen % sizeof(uint64_t));
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    for (; in != end; in += 8) {
        m = U8TO64_LE(in);
        v3 ^= m;

        SIPROUND;
        SIPROUND;

        v0 ^= m;
    }

    switch (left) {
    case 7: b |= ((uint64_t)in[6]) << 48; /* fallthrough */
    case 6: b |= ((uint64_t)in[5]) << 40; /* fallthrough */
    case 5: b |= ((uint64_t)in[4]) << 32; /* fallthrough */
    case 4: b |= ((uint64_t)in[3]) << 24; /* fallthrough */
    case 3: b |= ((uint64_t)in[2]) << 16; /* fallthrough */
    case 2: b |= ((uint64_t)in[1]) << 8;  /* fallthrough */
    case 1: b |= ((uint64_t)in[0]); break;
    case 0: break;
    }

    v3 ^= b;

    SIPROUND;
    SIPROUND;

    v0 ^= b;
    v2 ^= 0xff;

    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
    U64TO8_LE(out, b);

    return hash;
}

/** Generic hash function in UnrealIRCd - case insensitive.
 * This deals with IRC case-insensitive matches, which is
 * what you need for things like nicks and channels.
 * @param str   The string to hash (NUL-terminated)
 * @param k     The key to use for hashing (SIPHASH_KEY_LENGTH bytes,
 *              which is actually 16, not NUL terminated)
 * @returns Hash result as a 64 bit unsigned integer.
 * @note  The key (k) should be random and must stay the same for
 *        as long as you use the function for your specific hash table.
 *        Simply use the following on boot: siphash_generate_key(k);
 */
uint64_t siphash_nocase(const char *in, const char *k)
{
    uint64_t hash;
    char *out = (char*) &hash;
    size_t inlen = strlen(in);
    uint64_t v0 = 0x736f6d6570736575ULL;
    uint64_t v1 = 0x646f72616e646f6dULL;
    uint64_t v2 = 0x6c7967656e657261ULL;
    uint64_t v3 = 0x7465646279746573ULL;
    uint64_t k0 = U8TO64_LE(k);
    uint64_t k1 = U8TO64_LE(k + 8);
    uint64_t m;
    const char *end = in + inlen - (inlen % sizeof(uint64_t));
    const int left = inlen & 7;
    uint64_t b = ((uint64_t)inlen) << 56;
    v3 ^= k1;
    v2 ^= k0;
    v1 ^= k1;
    v0 ^= k0;

    for (; in != end; in += 8) {
        m = U8TO64_LE_NOCASE(in);
        v3 ^= m;

        SIPROUND;
        SIPROUND;

        v0 ^= m;
    }

    switch (left) {
    case 7: b |= ((uint64_t)tolower(in[6])) << 48; /* fallthrough */
    case 6: b |= ((uint64_t)tolower(in[5])) << 40; /* fallthrough */
    case 5: b |= ((uint64_t)tolower(in[4])) << 32; /* fallthrough */
    case 4: b |= ((uint64_t)tolower(in[3])) << 24; /* fallthrough */
    case 3: b |= ((uint64_t)tolower(in[2])) << 16; /* fallthrough */
    case 2: b |= ((uint64_t)tolower(in[1])) << 8;  /* fallthrough */
    case 1: b |= ((uint64_t)tolower(in[0])); break;
    case 0: break;
    }

    v3 ^= b;

    SIPROUND;
    SIPROUND;

    v0 ^= b;
    v2 ^= 0xff;

    SIPROUND;
    SIPROUND;
    SIPROUND;
    SIPROUND;

    b = v0 ^ v1 ^ v2 ^ v3;
    U64TO8_LE(out, b);

    return hash;
}

/* End of imported code */

/** Generic hash function in UnrealIRCd.
 * @param str   The string to hash (NUL-terminated)
 * @param k     The key to use for hashing (SIPHASH_KEY_LENGTH bytes,
 *              which is actually 16, not NUL terminated)
 * @returns Hash result as a 64 bit unsigned integer.
 * @note  The key (k) should be random and must stay the same for
 *        as long as you use the function for your specific hash table.
 *        Simply use the following on boot: siphash_generate_key(k);
 */
uint64_t siphash(const char *in, const char *k)
{
    size_t inlen = strlen(in);

    return siphash_raw(in, inlen, k);
}
/** Generate a key that is used by siphash() and siphash_nocase().
 * @param k   The key, this must be a char array of size 16.
 */
void siphash_generate_key(char *k)
{
	int i;
	for (i = 0; i < 16; i++)
		k[i] = getrandom8();
}

static struct list_head clientTable[NICK_HASH_TABLE_SIZE];
static struct list_head idTable[NICK_HASH_TABLE_SIZE];
static Channel *channelTable[CHAN_HASH_TABLE_SIZE];

static char siphashkey_nick[SIPHASH_KEY_LENGTH];
static char siphashkey_chan[SIPHASH_KEY_LENGTH];
static char siphashkey_whowas[SIPHASH_KEY_LENGTH];
static char siphashkey_throttling[SIPHASH_KEY_LENGTH];
static char siphashkey_ipusers[SIPHASH_KEY_LENGTH];

extern char unreallogo[];

/** Initialize all hash tables */
void init_hash(void)
{
	int i;

	siphash_generate_key(siphashkey_nick);
	siphash_generate_key(siphashkey_chan);
	siphash_generate_key(siphashkey_whowas);
	siphash_generate_key(siphashkey_throttling);
	siphash_generate_key(siphashkey_ipusers);

	for (i = 0; i < NICK_HASH_TABLE_SIZE; i++)
		INIT_LIST_HEAD(&clientTable[i]);

	for (i = 0; i < NICK_HASH_TABLE_SIZE; i++)
		INIT_LIST_HEAD(&idTable[i]);

	memset(channelTable, 0, sizeof(channelTable));

	memset(ThrottlingHash, 0, sizeof(ThrottlingHash));
	/* do not call init_throttling() here, as
	 * config file has not been read yet.
	 * The hash table is ready, anyway.
	 */

	if (strcmp(BASE_VERSION, &unreallogo[337]))
		loop.tainted = 1;
}

uint64_t hash_client_name(const char *name)
{
	return siphash_nocase(name, siphashkey_nick) % NICK_HASH_TABLE_SIZE;
}

uint64_t hash_channel_name(const char *name)
{
	return siphash_nocase(name, siphashkey_chan) % CHAN_HASH_TABLE_SIZE;
}

uint64_t hash_whowas_name(const char *name)
{
	return siphash_nocase(name, siphashkey_whowas) % WHOWAS_HASH_TABLE_SIZE;
}

/*
 * add_to_client_hash_table
 */
int add_to_client_hash_table(const char *name, Client *client)
{
	unsigned int hashv;
	/*
	 * If you see this, you have probably found your way to why changing the 
	 * base version made the IRCd become weird. This has been the case in all
	 * UnrealIRCd versions since 3.0. I'm sick of people ripping the IRCd off and 
	 * just slapping on some random <theirnet> BASE_VERSION while not changing
	 * a single bit of code. YOU DID NOT WRITE ALL OF THIS THEREFORE YOU DO NOT
	 * DESERVE TO BE ABLE TO DO THAT. If you found this however, I'm OK with you 
	 * removing the checks. However, keep in mind that the copyright headers must
	 * stay in place, which means no wiping of /credits and /info. We haven't 
	 * sat up late at night so some lamer could steal all our work without even
	 * giving us credit. Remember to follow all regulations in LICENSE.
	 * -Stskeeps
	*/
	if (loop.tainted)
		return 0;
	hashv = hash_client_name(name);
	list_add(&client->client_hash, &clientTable[hashv]);
	return 0;
}

/*
 * add_to_client_hash_table
 */
int add_to_id_hash_table(const char *name, Client *client)
{
	unsigned int hashv;
	hashv = hash_client_name(name);
	list_add(&client->id_hash, &idTable[hashv]);
	return 0;
}

/*
 * add_to_channel_hash_table
 */
int add_to_channel_hash_table(const char *name, Channel *channel)
{
	unsigned int hashv;

	hashv = hash_channel_name(name);
	channel->hnextch = channelTable[hashv];
	channelTable[hashv] = channel;
	return 0;
}
/*
 * del_from_client_hash_table
 */
int del_from_client_hash_table(const char *name, Client *client)
{
	if (!list_empty(&client->client_hash))
		list_del(&client->client_hash);

	INIT_LIST_HEAD(&client->client_hash);

	return 0;
}

int del_from_id_hash_table(const char *name, Client *client)
{
	if (!list_empty(&client->id_hash))
		list_del(&client->id_hash);

	INIT_LIST_HEAD(&client->id_hash);

	return 0;
}

/*
 * del_from_channel_hash_table
 */
void del_from_channel_hash_table(const char *name, Channel *channel)
{
	Channel *tmp, *prev = NULL;
	unsigned int hashv;

	hashv = hash_channel_name(name);
	for (tmp = channelTable[hashv]; tmp; tmp = tmp->hnextch)
	{
		if (tmp == channel)
		{
			if (prev)
				prev->hnextch = tmp->hnextch;
			else
				channelTable[hashv] = tmp->hnextch;
			tmp->hnextch = NULL;
			return; /* DONE */
		}
		prev = tmp;
	}
	return; /* NOTFOUND */
}

/*
 * hash_find_client
 */
Client *hash_find_client(const char *name, Client *client)
{
	Client *tmp;
	unsigned int hashv;

	hashv = hash_client_name(name);
	list_for_each_entry(tmp, &clientTable[hashv], client_hash)
	{
		if (smycmp(name, tmp->name) == 0)
			return tmp;
	}

	return client;
}

Client *hash_find_id(const char *name, Client *client)
{
	Client *tmp;
	unsigned int hashv;

	hashv = hash_client_name(name);
	list_for_each_entry(tmp, &idTable[hashv], id_hash)
	{
		if (smycmp(name, tmp->id) == 0)
			return tmp;
	}

	return client;
}

/*
 * hash_find_nickatserver
 */
Client *hash_find_nickatserver(const char *str, Client *def)
{
	char *serv;
	char nick[NICKLEN+HOSTLEN+1];
	Client *client;
	
	strlcpy(nick, str, sizeof(nick)); /* let's work on a copy */

	serv = strchr(nick, '@');
	if (serv)
		*serv++ = '\0';

	client = find_user(nick, NULL);
	if (!client)
		return NULL; /* client not found */
	
	if (!serv)
		return client; /* validated: was just 'nick' and not 'nick@serv' */

	/* Now validate the server portion */
	if (client->user && !smycmp(serv, client->user->server))
		return client; /* validated */
	
	return def;
}
/*
 * hash_find_server
 */
Client *hash_find_server(const char *server, Client *def)
{
	Client *tmp;
	unsigned int hashv;

	hashv = hash_client_name(server);
	list_for_each_entry(tmp, &clientTable[hashv], client_hash)
	{
		if (!IsServer(tmp) && !IsMe(tmp))
			continue;
		if (smycmp(server, tmp->name) == 0)
		{
			return tmp;
		}
	}

	return def;
}

/** Find a client, user (person), server or channel by name.
 * If you are looking for "other find functions", then the alphabetical index of functions
 * at 'f' is your best bet: https://www.unrealircd.org/api/5/globals_func_f.html#index_f
 * @defgroup FindFunctions Find functions
 * @{
 */

/** Find a client by name.
 * This searches in the list of all types of clients, user/person, servers or an unregistered clients.
 * If you know what type of client to search for, then use find_server() or find_user() instead!
 * @param name        The name to search for (eg: "nick" or "irc.example.net")
 * @param requester   The client that is searching for this name
 * @note  If 'requester' is a server or NULL, then we also check
 *        the ID table, otherwise not.
 * @returns If the client is found then the Client is returned, otherwise NULL.
 */
Client *find_client(const char *name, Client *requester)
{
	if (requester == NULL || IsServer(requester))
	{
		Client *client;

		if ((client = hash_find_id(name, NULL)) != NULL)
			return client;
	}

	return hash_find_client(name, NULL);
}

/** Find a server by name.
 * @param name        The server name to search for (eg: 'irc.example.net'
 *                    or '001')
 * @param requester   The client searching for the name.
 * @note  If 'requester' is a server or NULL, then we also check
 *        the ID table, otherwise not.
 * @returns If the server is found then the Client is returned, otherwise NULL.
 */
Client *find_server(const char *name, Client *requester)
{
	if (name)
	{
		Client *client;

		if ((client = find_client(name, NULL)) != NULL && (IsServer(client) || IsMe(client)))
			return client;
	}

	return NULL;
}

/** Find a user (a person)
 * @param name        The name to search for (eg: "nick" or "001ABCDEFG")
 * @param requester   The client that is searching for this name
 * @note  If 'requester' is a server or NULL, then we also check
 *        the ID table, otherwise not.
 * @returns If the user is found then the Client is returned, otherwise NULL.
 */
Client *find_user(const char *name, Client *requester)
{
	Client *c2ptr;

	c2ptr = find_client(name, requester);

	if (c2ptr && IsUser(c2ptr) && c2ptr->user)
		return c2ptr;

	return NULL;
}


/** Find a channel by name.
 * @param name			The channel name to search for
 * @returns If the channel exists then the Channel is returned, otherwise NULL.
 */
Channel *find_channel(const char *name)
{
	unsigned int hashv;
	Channel *channel;

	hashv = hash_channel_name(name);

	for (channel = channelTable[hashv]; channel; channel = channel->hnextch)
		if (smycmp(name, channel->name) == 0)
			return channel;

	return NULL;
}

/** @} */

Channel *hash_get_chan_bucket(uint64_t hashv)
{
	if (hashv > CHAN_HASH_TABLE_SIZE)
		return NULL;
	return channelTable[hashv];
}

/** Find a server by the SID-part of a UID.
 * Eg you pass "001ABCDEFG" and it would look up server "001".
 *
 * @param uid	The UID, eg 001ABCDEFG
 * @returns Server where the UID would be hosted on, or NULL
 * if no such server is linked.
 */
Client *find_server_by_uid(const char *uid)
{
	char sid[SIDLEN+1];

	if (!isdigit(*uid))
		return NULL; /* not a UID/SID */

	strlcpy(sid, uid, sizeof(sid));
	return hash_find_id(sid, NULL);
}

/* Throttling - originally by Stskeeps */

/* Note that we call this set::anti-flood::connect-flood nowadays */

struct MODVAR ThrottlingBucket *ThrottlingHash[THROTTLING_HASH_TABLE_SIZE];

void update_throttling_timer_settings(void)
{
	long v;
	EventInfo eInfo;

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

	memset(&eInfo, 0, sizeof(eInfo));
	eInfo.flags = EMOD_EVERY;
	eInfo.every_msec = v;
	EventMod(EventFind("throttling_check_expire"), &eInfo);
}

uint64_t hash_throttling(const char *ip)
{
	return siphash(ip, siphashkey_throttling) % THROTTLING_HASH_TABLE_SIZE;
}

struct ThrottlingBucket *find_throttling_bucket(Client *client)
{
	int hash = 0;
	struct ThrottlingBucket *p;
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
	struct ThrottlingBucket *n, *n_next;
	int	i;
	static time_t t = 0;
		
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

	if (!t || (TStime() - t > 30))
	{
		extern Module *Modules;
		char *p = serveropts + strlen(serveropts);
		Module *mi;
		t = TStime();
		if (!Hooks[HOOKTYPE_USERMSG] && strchr(serveropts, 'm'))
		{ p = strchr(serveropts, 'm'); *p = '\0'; }
		if (!Hooks[HOOKTYPE_CHANMSG] && strchr(serveropts, 'M'))
		{ p = strchr(serveropts, 'M'); *p = '\0'; }
		if (Hooks[HOOKTYPE_USERMSG] && !strchr(serveropts, 'm'))
			*p++ = 'm';
		if (Hooks[HOOKTYPE_CHANMSG] && !strchr(serveropts, 'M'))
			*p++ = 'M';
		*p = '\0';
		for (mi = Modules; mi; mi = mi->next)
			if (!(mi->options & MOD_OPT_OFFICIAL))
				tainted = 99;
	}

	return;
}

void add_throttling_bucket(Client *client)
{
	int hash;
	struct ThrottlingBucket *n;

	n = safe_alloc(sizeof(struct ThrottlingBucket));
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
	struct ThrottlingBucket *b;

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

/**** IP users hash table *****/

MODVAR IpUsersBucket *IpUsersHash_ipv4[IPUSERS_HASH_TABLE_SIZE];
MODVAR IpUsersBucket *IpUsersHash_ipv6[IPUSERS_HASH_TABLE_SIZE];

uint64_t hash_ipusers(const char *ip)
{
	return siphash(ip, siphashkey_ipusers) % IPUSERS_HASH_TABLE_SIZE;
}

IpUsersBucket *find_ipusers_bucket(Client *client)
{
	int hash = 0;
	IpUsersBucket *p;

	hash = hash_ipusers(client->ip);

	if (IsIPV6(client))
	{
		for (p = IpUsersHash_ipv6[hash]; p; p = p->next)
			if (memcmp(p->rawip, client->rawip, 16) == 0)
				return p;
	} else {
		for (p = IpUsersHash_ipv4[hash]; p; p = p->next)
			if (memcmp(p->rawip, client->rawip, 4) == 0)
				return p;
	}

	return NULL;
}

IpUsersBucket *add_ipusers_bucket(Client *client)
{
	int hash;
	IpUsersBucket *n;

	hash = hash_ipusers(client->ip);

	n = safe_alloc(sizeof(IpUsersBucket));
	if (IsIPV6(client))
	{
		memcpy(n->rawip, client->rawip, 16);
		AddListItem(n, IpUsersHash_ipv6[hash]);
	} else {
		memcpy(n->rawip, client->rawip, 4);
		AddListItem(n, IpUsersHash_ipv4[hash]);
	}
	return n;
}

void decrease_ipusers_bucket(Client *client)
{
	int hash = 0;
	IpUsersBucket *p;

	if (!(client->flags & CLIENT_FLAG_IPUSERS_BUMPED))
		return; /* nothing to do */

	client->flags &= ~CLIENT_FLAG_IPUSERS_BUMPED;

	hash = hash_ipusers(client->ip);

	if (IsIPV6(client))
	{
		for (p = IpUsersHash_ipv6[hash]; p; p = p->next)
			if (memcmp(p->rawip, client->rawip, 16) == 0)
				break;
	} else {
		for (p = IpUsersHash_ipv4[hash]; p; p = p->next)
			if (memcmp(p->rawip, client->rawip, 4) == 0)
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
