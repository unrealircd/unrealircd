/************************************************************************
 * IRC - Internet Relay Chat, src/dns.c
 * (C) 2005 Bram Matthys (Syzop) and the UnrealIRCd Team
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
 *
 */

#include "unrealircd.h"
#include "dns.h"

#if !defined(UNREAL_VERSION_TIME)
 #error "YOU MUST RUN ./Config WHENEVER YOU ARE UPGRADING UNREAL!!!!"
#endif


/* Prevent crashes due to invalid prototype/ABI.
 * And force the use of at least the version shipped with Unreal
 * (or at least one without known security issues).
 */
#if ARES_VERSION < 0x010600
 #error "You have an old c-ares version on your system and/or Unreals c-ares failed to compile!"
#endif

/* Forward declerations */
void unrealdns_cb_iptoname(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_cb_nametoip_verify(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_cb_nametoip_link(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_delasyncconnects(void);
static uint64_t unrealdns_hash_ip(const char *ip);
static void unrealdns_addtocache(const char *name, const char *ip);
static const char *unrealdns_findcache_ip(const char *ip);
struct hostent *unreal_create_hostent(const char *name, const char *ip);
static void unrealdns_freeandremovereq(DNSReq *r);
void unrealdns_removecacherecord(DNSCache *c);

/* Externs */
extern void proceed_normal_client_handshake(Client *client, struct hostent *he);

/* Global variables */

ares_channel resolver_channel; /**< The resolver channel. */

DNSStats dnsstats;

static DNSReq *requests = NULL; /**< Linked list of requests (pending responses). */

static DNSCache *cache_list = NULL; /**< Linked list of cache */
static DNSCache *cache_hashtbl[DNS_HASH_SIZE]; /**< Hash table of cache */

static unsigned int unrealdns_num_cache = 0; /**< # of cache entries in memory */

static char siphashkey_dns_ip[SIPHASH_KEY_LENGTH];

static void unrealdns_io_cb(int fd, int revents, void *data)
{
	ares_socket_t read_fd, write_fd;
	FDEntry *fde;

	read_fd = write_fd = ARES_SOCKET_BAD;
	fde = &fd_table[fd];

	if (revents & FD_SELECT_READ)
		read_fd = fde->fd;

	if (revents & FD_SELECT_WRITE)
		write_fd = fde->fd;

	ares_process_fd(resolver_channel, read_fd, write_fd);
}

static void unrealdns_sock_state_cb(void *data, ares_socket_t fd, int read, int write)
{
	int selflags = 0;

	if (!read && !write)
	{
		fd_close(fd);
		return;
	}
	
	if (read)
		selflags |= FD_SELECT_READ;

	if (write)
		selflags |= FD_SELECT_WRITE;

	fd_setselect(fd, selflags, unrealdns_io_cb, data);
}

/* Who thought providing a socket OPEN callback without a socket CLOSE callback was
 * a good idea...?  --nenolod
 */
static int unrealdns_sock_create_cb(ares_socket_t fd, int type, void *data)
{
	/* NOTE: We use FDCLOSE_NONE here because c-ares
	 * will take care of the closing. So *WE* must
	 * never close the socket.
	 */
	fd_open(fd, "DNS Resolver Socket", FDCLOSE_NONE);
	return ARES_SUCCESS;
}

EVENT(unrealdns_timeout)
{
	ares_process_fd(resolver_channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}

static Event *unrealdns_timeout_hdl = NULL;

void init_resolver(int firsttime)
{
	struct ares_options options;
	int n;
	int optmask;

	if (requests)
		abort(); /* should never happen */
		
	if (firsttime)
	{
		memset(&cache_hashtbl, 0, sizeof(cache_hashtbl));
		memset(&dnsstats, 0, sizeof(dnsstats));
		siphash_generate_key(siphashkey_dns_ip);
		ares_library_init(ARES_LIB_INIT_ALL);
	}

	memset(&options, 0, sizeof(options));
	options.timeout = 1500; /* 1.5 seconds */
	options.tries = 2;
	/* Note that the effective DNS timeout is NOT simply 1500*2=3000.
	 * This is because c-ares does some incremental timeout stuff itself
	 * that may add up to twice the timeout in the second round,
	 * so effective max is 1500ms first and then up to 3000s, so 4500ms in total
	 * (until they change the algorithm again, that is...).
	 */
	options.flags |= ARES_FLAG_NOALIASES|ARES_FLAG_IGNTC;
	options.sock_state_cb = unrealdns_sock_state_cb;
	/* Don't search domains or you'll get lookups for like
	 * 1.1.168.192.dnsbl.dronebl.org.mydomain.org which is a waste.
	 */
	options.domains = NULL;
	options.ndomains = 0;
	optmask = ARES_OPT_TIMEOUTMS|ARES_OPT_TRIES|ARES_OPT_FLAGS|ARES_OPT_SOCK_STATE_CB|ARES_OPT_DOMAINS;
#ifndef _WIN32
	/* on *NIX don't use the hosts file, since it causes countless useless reads.
	 * on Windows we use it for now, this could be changed in the future.
	 */
	options.lookups = "b";
	optmask |= ARES_OPT_LOOKUPS;
#endif
	n = ares_init_options(&resolver_channel, &options, optmask);
	if (n != ARES_SUCCESS)
	{
		/* FATAL */
		config_error("resolver: ares_init_options() failed with error code %d [%s]", n, ares_strerror(n));
#ifdef _WIN32
		win_error();
#endif
		exit(-7);
	}

	ares_set_socket_callback(resolver_channel, unrealdns_sock_create_cb, NULL);
	unrealdns_timeout_hdl = EventAdd(NULL, "unrealdns_timeout", unrealdns_timeout, NULL, 500, 0);
}

void reinit_resolver(Client *client)
{
	EventDel(unrealdns_timeout_hdl);

	unreal_log(ULOG_INFO, "dns", "REINIT_RESOLVER", client,
	           "$client requested reinitalization of the DNS resolver");
	ares_destroy(resolver_channel);
	init_resolver(0);
}

void unrealdns_addreqtolist(DNSReq *r)
{
	if (requests)
	{
		r->next = requests;
		requests->prev = r;
	}
	requests = r;
}

/** Get (and verify) the host for an incoming client.
 * - it checks the cache first, returns the host if found (and valid).
 * - if not found in cache it does ip->name and then name->ip, if both resolve
 *   to the same name it is accepted, otherwise not.
 *   We return NULL in this case and an asynchronic request is done.
 *   When done, proceed_normal_client_handshake() is called.
 */
struct hostent *unrealdns_doclient(Client *client)
{
	DNSReq *r;
	const char *cache_name;

	cache_name = unrealdns_findcache_ip(client->ip);
	if (cache_name)
		return unreal_create_hostent(cache_name, client->ip);

	/* Create a request */
	r = safe_alloc(sizeof(DNSReq));
	r->client = client;
	r->ipv6 = IsIPV6(client);
	unrealdns_addreqtolist(r);

	/* Execute it */
	if (r->ipv6)
	{
		struct in6_addr addr;
		memset(&addr, 0, sizeof(addr));
		inet_pton(AF_INET6, client->ip, &addr);
		ares_gethostbyaddr(resolver_channel, &addr, 16, AF_INET6, unrealdns_cb_iptoname, r);
	} else {
		struct in_addr addr;
		memset(&addr, 0, sizeof(addr));
		inet_pton(AF_INET, client->ip, &addr);
		ares_gethostbyaddr(resolver_channel, &addr, 4, AF_INET, unrealdns_cb_iptoname, r);
	}

	return NULL;
}

/** Resolve a name to an IP, for a link block.
 */
void unrealdns_gethostbyname_link(const char *name, ConfigItem_link *conf, int ipv4_only)
{
	DNSReq *r;

	/* Create a request */
	r = safe_alloc(sizeof(DNSReq));
	r->linkblock = conf;
	safe_strdup(r->name, name);
	if (!DISABLE_IPV6 && !ipv4_only)
	{
		/* We try an IPv6 lookup first, and if that fails we try IPv4. */
		r->ipv6 = 1;
	}

	unrealdns_addreqtolist(r);

	/* Execute it */
	ares_gethostbyname(resolver_channel, r->name, r->ipv6 ? AF_INET6 : AF_INET, unrealdns_cb_nametoip_link, r);
}

void unrealdns_cb_iptoname(void *arg, int status, int timeouts, struct hostent *he)
{
	DNSReq *r = (DNSReq *)arg;
	DNSReq *newr;
	Client *client = r->client;
	char ipv6 = r->ipv6;

	unrealdns_freeandremovereq(r);

	if (!client)
		return; 
	
	/* Check for status and null name (yes, we must) */
	if ((status != 0) || !he->h_name || !*he->h_name)
	{
		/* Failed */
		proceed_normal_client_handshake(client, NULL);
		return;
	}

	/* Good, we got a valid response, now prepare for name -> ip */
	newr = safe_alloc(sizeof(DNSReq));
	newr->client = client;
	newr->ipv6 = ipv6;
	safe_strdup(newr->name, he->h_name);
	unrealdns_addreqtolist(newr);

	ares_gethostbyname(resolver_channel, he->h_name, ipv6 ? AF_INET6 : AF_INET, unrealdns_cb_nametoip_verify, newr);
}

void unrealdns_cb_nametoip_verify(void *arg, int status, int timeouts, struct hostent *he)
{
	DNSReq *r = (DNSReq *)arg;
	Client *client = r->client;
	char ipv6 = r->ipv6;
	int i;
	struct hostent *he2;

	if (!client)
		goto bad;

	if ((status != 0) || (ipv6 && (he->h_length != 16)) || (!ipv6 && (he->h_length != 4)))
	{
		/* Failed: error code, or data length is incorrect */
		proceed_normal_client_handshake(client, NULL);
		goto bad;
	}

	/* Verify ip->name and name->ip mapping... */
	for (i = 0; he->h_addr_list[i]; i++)
	{
		if (r->ipv6)
		{
			struct in6_addr addr;
			if (inet_pton(AF_INET6, client->ip, &addr) != 1)
				continue; /* something fucked */
			if (!memcmp(he->h_addr_list[i], &addr, 16))
				break; /* MATCH */
		} else {
			struct in_addr addr;
			if (inet_pton(AF_INET, client->ip, &addr) != 1)
				continue; /* something fucked */
			if (!memcmp(he->h_addr_list[i], &addr, 4))
				break; /* MATCH */
		}
	}

	if (!he->h_addr_list[i])
	{
		/* Failed name <-> IP mapping */
		proceed_normal_client_handshake(client, NULL);
		goto bad;
	}

	if (!valid_host(r->name, 1))
	{
		/* Hostname is bad, don't cache and consider unresolved */
		proceed_normal_client_handshake(client, NULL);
		goto bad;
	}

	/* Get rid of stupid uppercase DNS names... */
	strtolower(r->name);

	/* Entry was found, verified, and can be added to cache */

	unrealdns_addtocache(r->name, client->ip);
	
	he2 = unreal_create_hostent(r->name, client->ip);
	proceed_normal_client_handshake(client, he2);

bad:
	unrealdns_freeandremovereq(r);
}

void unrealdns_cb_nametoip_link(void *arg, int status, int timeouts, struct hostent *he)
{
	DNSReq *r = (DNSReq *)arg;
	int n;
	struct hostent *he2;
	char ipbuf[HOSTLEN+1];
	const char *ip = NULL;

	if (!r->linkblock)
	{
		/* Possible if deleted due to rehash async removal */
		unrealdns_freeandremovereq(r);
		return;
	}

	if ((status != 0) || !he->h_addr_list || !he->h_addr_list[0])
	{
		if (r->ipv6)
		{
			/* Retry for IPv4... */
			r->ipv6 = 0;
			ares_gethostbyname(resolver_channel, r->name, AF_INET, unrealdns_cb_nametoip_link, r);

			return;
		}

		/* fatal error while resolving */
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_RESOLVING", NULL,
			   "Unable to resolve hostname $link_block.hostname, when trying to connect to server $link_block.",
			   log_data_link_block(r->linkblock));
		r->linkblock->refcount--;
		unrealdns_freeandremovereq(r);
		return;
	}
	r->linkblock->refcount--;

	if (!he->h_addr_list[0] || (he->h_length != (r->ipv6 ? 16 : 4)) ||
	    !(ip = inetntop(r->ipv6 ? AF_INET6 : AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf))))
	{
		/* Illegal response -- fatal */
		unreal_log(ULOG_ERROR, "link", "LINK_ERROR_RESOLVING", NULL,
		           "Unable to resolve hostname $link_block.hostname, when trying to connect to server $link_block.",
		           log_data_link_block(r->linkblock));
		unrealdns_freeandremovereq(r);
		return;
	}
	
	/* Ok, since we got here, it seems things were actually succesfull */

	/* Fill in [linkblockstruct]->ipnum */
	safe_strdup(r->linkblock->connect_ip, ip);
	he2 = unreal_create_hostent(he->h_name, ip);

	/* Try to connect to the server */
	connect_server(r->linkblock, r->client, he2);

	unrealdns_freeandremovereq(r);
	/* DONE */
}

static uint64_t unrealdns_hash_ip(const char *ip)
{
        return siphash(ip, siphashkey_dns_ip) % DNS_HASH_SIZE;
}

static void unrealdns_addtocache(const char *name, const char *ip)
{
	unsigned int hashv;
	DNSCache *c;

	dnsstats.cache_adds++;

	hashv = unrealdns_hash_ip(ip);

	/* Check first if it is already present in the cache.
	 * This is possible, when 2 clients connect at the same time.
	 */	
	for (c = cache_hashtbl[hashv]; c; c = c->hnext)
		if (!strcmp(ip, c->ip))
			return; /* already present in cache */

	/* Remove last item, if we got too many entries.. */
	if (unrealdns_num_cache >= DNS_MAX_ENTRIES)
	{
		for (c = cache_list; c->next; c = c->next);
		unrealdns_removecacherecord(c);
	}

	/* Create record */
	c = safe_alloc(sizeof(DNSCache));
	safe_strdup(c->name, name);
	safe_strdup(c->ip, ip);
	c->expires = TStime() + DNSCACHE_TTL;
	
	/* Add to hash table */
	if (cache_hashtbl[hashv])
	{
		cache_hashtbl[hashv]->hprev = c;
		c->hnext = cache_hashtbl[hashv];
	}
	cache_hashtbl[hashv] = c;
	
	/* Add to linked list */
	if (cache_list)
	{
		cache_list->prev = c;
		c->next = cache_list;
	}
	cache_list = c;

	unrealdns_num_cache++;
	/* DONE */
}

/** Search the cache for a confirmed ip->name and name->ip match, by address.
 * @returns The resolved hostname, or NULL if not found in cache.
 */
static const char *unrealdns_findcache_ip(const char *ip)
{
	unsigned int hashv;
	DNSCache *c;

	hashv = unrealdns_hash_ip(ip);
	
	for (c = cache_hashtbl[hashv]; c; c = c->hnext)
		if (!strcmp(ip, c->ip))
		{
			dnsstats.cache_hits++;
			return c->name;
		}
	
	dnsstats.cache_misses++;
	return NULL;
}

/** Removes dns cache record from list (and frees it).
 */
void unrealdns_removecacherecord(DNSCache *c)
{
unsigned int hashv;

	/* We basically got 4 pointers to update:
	 * <previous listitem>->next
	 * <next listitem>->previous
	 * <previous hashitem>->next
	 * <next hashitem>->prev.
	 * And we need to update 'cache_list' and 'cache_hash[]' if needed.
	 */
	if (c->prev)
		c->prev->next = c->next;
	else
		cache_list = c->next; /* new list HEAD */
	
	if (c->next)
		c->next->prev = c->prev;
	
	if (c->hprev)
		c->hprev->hnext = c->hnext;
	else {
		/* new hash HEAD */
		hashv = unrealdns_hash_ip(c->ip);
		if (cache_hashtbl[hashv] != c)
			abort(); /* impossible */
		cache_hashtbl[hashv] = c->hnext;
	}
	
	if (c->hnext)
		c->hnext->hprev = c->hprev;
	
	safe_free(c->name);
	safe_free(c->ip);
	safe_free(c);

	unrealdns_num_cache--;
}

/** This regulary removes old dns records from the cache */
EVENT(unrealdns_removeoldrecords)
{
DNSCache *c, *next;

	for (c = cache_list; c; c = next)
	{
		next = c->next;
		if (c->expires < TStime())
			unrealdns_removecacherecord(c);
	}
}

struct hostent *unreal_create_hostent(const char *name, const char *ip)
{
struct hostent *he;

	/* Create a hostent structure (I HATE HOSTENTS) and return it.. */
	he = safe_alloc(sizeof(struct hostent));
	safe_strdup(he->h_name, name);
	if (strchr(ip, ':'))
	{
		/* IPv6 */
		he->h_addrtype = AF_INET6;
		he->h_length = sizeof(struct in6_addr);
		he->h_addr_list = safe_alloc(sizeof(char *) * 2); /* alocate an array of 2 pointers */
		he->h_addr_list[0] = safe_alloc(sizeof(struct in6_addr));
		inet_pton(AF_INET6, ip, he->h_addr_list[0]);
	} else {
		he->h_addrtype = AF_INET;
		he->h_length = sizeof(struct in_addr);
		he->h_addr_list = safe_alloc(sizeof(char *) * 2); /* alocate an array of 2 pointers */
		he->h_addr_list[0] = safe_alloc(sizeof(struct in_addr));
		inet_pton(AF_INET, ip, he->h_addr_list[0]);
	}

	return he;
}

void unreal_free_hostent(struct hostent *he)
{
	safe_free(he->h_name);
	safe_free(he->h_addr_list[0]);
	safe_free(he->h_addr_list);
	safe_free(he);
}

static void unrealdns_freeandremovereq(DNSReq *r)
{
	if (r->prev)
		r->prev->next = r->next;
	else
		requests = r->next; /* new HEAD */
	
	if (r->next)
		r->next->prev = r->prev;

	safe_free(r->name);
	safe_free(r);
}

/** Delete requests for client 'client'.
 * Actually we DO NOT (and should not) delete them, but simply mark them as 'dead'.
 */
void unrealdns_delreq_bycptr(Client *client)
{
	DNSReq *r;

	for (r = requests; r; r = r->next)
		if (r->client == client)
			r->client = NULL;
}

void unrealdns_delasyncconnects(void)
{
	DNSReq *r;

	for (r = requests; r; r = r->next)
		if (r->type == DNSREQ_CONNECT)
			r->linkblock = NULL;
	
}

CMD_FUNC(cmd_dns)
{
	DNSCache *c;
	DNSReq *r;
	const char *param;

	if (!ValidatePermissionsForPath("server:dns",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc > 1) && !BadPtr(parv[1]))
		param = parv[1];
	else
		param = "";

	if (*param == 'l') /* LIST CACHE */
	{
		sendtxtnumeric(client, "DNS CACHE List (%u items):", unrealdns_num_cache);
		for (c = cache_list; c; c = c->next)
			sendtxtnumeric(client, " %s [%s]", c->name, c->ip);
	} else
	if (*param == 'r') /* LIST REQUESTS */
	{
		sendtxtnumeric(client, "DNS Request List:");
		for (r = requests; r; r = r->next)
			sendtxtnumeric(client, " %s", r->client ? r->client->ip : "<client lost>");
	} else
	if (*param == 'c') /* CLEAR CACHE */
	{
		unreal_log(ULOG_INFO, "dns", "DNS_CACHE_CLEARED", client,
		            "DNS cache cleared by $client");
		
		while (cache_list)
		{
			c = cache_list->next;
			safe_free(cache_list->name);
			safe_free(cache_list->ip);
			safe_free(cache_list);
			cache_list = c;
		}
		memset(&cache_hashtbl, 0, sizeof(cache_hashtbl));
		unrealdns_num_cache = 0;
		sendnotice(client, "DNS Cache has been cleared");
	} else
	if (*param == 'i') /* INFORMATION */
	{
		struct ares_options inf;
		struct ares_addr_node *serverlist = NULL, *ns;
		int i;
		int optmask;

		sendtxtnumeric(client, "****** DNS Configuration Information ******");
		sendtxtnumeric(client, " c-ares version: %s",ares_version(NULL));
		
		i = 0;
		ares_get_servers(resolver_channel, &serverlist);
		for (ns = serverlist; ns; ns = ns->next)
		{
			char ipbuf[128];
			const char *ip;
			i++;
			
			ip = inetntop(ns->family, &ns->addr, ipbuf, sizeof(ipbuf));
			sendtxtnumeric(client, "      server #%d: %s", i, ip ? ip : "<error>");
		}
		ares_free_data(serverlist);

		ares_save_options(resolver_channel, &inf, &optmask);
		if (optmask & ARES_OPT_TIMEOUTMS)
			sendtxtnumeric(client, "        timeout: %d", inf.timeout);
		if (optmask & ARES_OPT_TRIES)
			sendtxtnumeric(client, "          tries: %d", inf.tries);
		if (optmask & ARES_OPT_DOMAINS)
		{
			sendtxtnumeric(client, "   # of search domains: %d", inf.ndomains);
			for (i = 0; i < inf.ndomains; i++)
				sendtxtnumeric(client, "      domain #%d: %s", i+1, inf.domains[i]);
		}
		sendtxtnumeric(client, "****** End of DNS Configuration Info ******");
		
		ares_destroy_options(&inf);
	} else /* STATISTICS */
	{
		sendtxtnumeric(client, "DNS CACHE Stats:");
		sendtxtnumeric(client, " hits: %d", dnsstats.cache_hits);
		sendtxtnumeric(client, " misses: %d", dnsstats.cache_misses);
	}
	return;
}

/* Little helper function for dnsbl module.
 * No we will NOT copy the entire c-ares api, just this one.
 */
void unreal_gethostbyname(const char *name, int family, ares_host_callback callback, void *arg)
{
	ares_gethostbyname(resolver_channel, name, family, callback, arg);
}
