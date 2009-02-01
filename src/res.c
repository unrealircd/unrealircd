/************************************************************************
 * IRC - Internet Relay Chat, res.c
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include "version.h"

#if !defined(UNREAL_VERSION_TIME)
 #error "YOU MUST RUN ./Config WHENEVER YOU ARE UPGRADING UNREAL!!!!"
#endif

#include <time.h>
#ifdef _WIN32
#include <sys/timeb.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include "inet.h"
#include <fcntl.h>
#include "h.h"

#include <res.h>

/* Prevent crashes due to invalid prototype/ABI */
#if ARES_VERSION < 0x010600
 #error "You have an old c-ares version on your system and/or Unreals c-ares failed to compile!"
#endif

/* Forward declerations */
void unrealdns_cb_iptoname(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_cb_nametoip_verify(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_cb_nametoip_link(void *arg, int status, int timeouts, struct hostent *he);
void unrealdns_delasyncconnects(void);
static unsigned int unrealdns_haship(void *binaryip, int length);
static void unrealdns_addtocache(char *name, void *binaryip, int length);
static char *unrealdns_findcache_byaddr(struct IN_ADDR *addr);
struct hostent *unreal_create_hostent(char *name, struct IN_ADDR *addr);
static void unrealdns_freeandremovereq(DNSReq *r);
void unrealdns_removecacherecord(DNSCache *c);

/* Externs */
extern void proceed_normal_client_handshake(aClient *acptr, struct hostent *he);

/* Global variables */

ares_channel resolver_channel; /**< The resolver channel. */

DNSStats dnsstats;

static DNSReq *requests = NULL; /**< Linked list of requests (pending responses). */

static DNSCache *cache_list = NULL; /**< Linked list of cache */
static DNSCache *cache_hashtbl[DNS_HASH_SIZE]; /**< Hash table of cache */

static unsigned int unrealdns_num_cache = 0; /**< # of cache entries in memory */

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
	}

	memset(&options, 0, sizeof(options));
	options.timeout = 3;
	options.tries = 2;
	options.flags = ARES_FLAG_NOALIASES|ARES_FLAG_IGNTC;
	optmask = ARES_OPT_TIMEOUT|ARES_OPT_TRIES|ARES_OPT_FLAGS;
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
		/* Try again with set::dns::nameserver block */
		optmask |= ARES_OPT_SERVERS;
		options.servers = MyMallocEx(sizeof(struct in_addr));
		options.servers[0].s_addr = inet_addr(NAME_SERVER);
		options.nservers = 1;
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
		ircd_log(LOG_ERROR, "[warning] Unable to get DNS server from %s. Using the one from set::dns::nameserver (%s) instead",
#ifdef _WIN32
			"registry",
#else
			"resolv.conf",
#endif
			NAME_SERVER);
		MyFree(options.servers);
	}
}

void reinit_resolver(aClient *sptr)
{
#ifdef CHROOTDIR
	/* Prevent people from killing their ircd accidently if in CHROOTDIR mode... */
FILE *fd;

	fd = fopen("/etc/resolv.conf", "r");
	if (!fd)
	{
		sendnotice(sptr, "Rehashing DNS with CHROOTDIR enabled seems a BAD idea since /etc/resolv.conf "
		                 "is missing in your chroot. This is usually perfectly fine and normal, but "
		                 "prevents this exact rehash feature from working for obvious technical reasons "
		                 "(HINT: it is impossible to read the system /etc/resolv.conf since it's outside the chroot).");
		return;
	}
	fclose(fd);
#endif

	sendto_realops("%s requested reinitalization of resolver!", sptr->name);
	sendto_realops("Destroying resolver channel, along with all currently pending queries...");
	ares_destroy(resolver_channel);
	sendto_realops("Initializing resolver again...");
	init_resolver(0);
	sendto_realops("Reinitalization finished successfully.");
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
struct hostent *unrealdns_doclient(aClient *cptr)
{
DNSReq *r;
#ifdef INET6
char ipv4[4];
#endif
static struct hostent *he;
char *cache_name, ipv6;

	cache_name = unrealdns_findcache_byaddr(&cptr->ip);
	if (cache_name)
		return unreal_create_hostent(cache_name, &cptr->ip);

	/* Create a request */
	r = MyMallocEx(sizeof(DNSReq));
	r->cptr = cptr;
	r->ipv6 = isipv6(&cptr->ip);
	unrealdns_addreqtolist(r);
	
	/* Execute it */
#ifndef INET6
	/* easy */
	ares_gethostbyaddr(resolver_channel, &cptr->ip, 4, AF_INET, unrealdns_cb_iptoname, r);
#else
	if (r->ipv6)
		ares_gethostbyaddr(resolver_channel, &cptr->ip, 16, AF_INET6, unrealdns_cb_iptoname, r);
	else {
		/* This is slightly more tricky: convert it to an IPv4 presentation and issue the request with that */
		memcpy(ipv4, ((char *)&cptr->ip) + 12, 4);
		ares_gethostbyaddr(resolver_channel, ipv4, 4, AF_INET, unrealdns_cb_iptoname, r);
	}
#endif

	return NULL;
}

/** Resolve a name to an IP, for a link block.
 */
void unrealdns_gethostbyname_link(char *name, ConfigItem_link *conf)
{
DNSReq *r;
#ifdef INET6
char ipv4[4];
#endif

	/* Create a request */
	r = MyMallocEx(sizeof(DNSReq));
	r->linkblock = conf;
	r->name = strdup(name);
#ifdef INET6
	/* We try IPv6 first, and if that fails we try IPv4 */
	r->ipv6 = 1;
#else
	r->ipv6 = 0;
#endif
	unrealdns_addreqtolist(r);
	
	/* Execute it */
#ifndef INET6
	ares_gethostbyname(resolver_channel, r->name, AF_INET, unrealdns_cb_nametoip_link, r);
#else
	ares_gethostbyname(resolver_channel, r->name, r->ipv6 ? AF_INET6 : AF_INET, unrealdns_cb_nametoip_link, r);
#endif
}

void unrealdns_cb_iptoname(void *arg, int status, int timeouts, struct hostent *he)
{
DNSReq *r = (DNSReq *)arg;
DNSReq *newr;
aClient *acptr = r->cptr;
char ipv6 = r->ipv6;

	unrealdns_freeandremovereq(r);

	if (!acptr)
		return; 
	
	/* Check for status and null name (yes, we must) */
	if ((status != 0) || !he->h_name || !*he->h_name)
	{
		/* Failed */
		proceed_normal_client_handshake(acptr, NULL);
		return;
	}

	/* Good, we got a valid response, now prepare for name -> ip */
	newr = MyMallocEx(sizeof(DNSReq));
	newr->cptr = acptr;
	newr->ipv6 = ipv6;
	newr->name = strdup(he->h_name);
	unrealdns_addreqtolist(newr);
	
#ifndef INET6
	ares_gethostbyname(resolver_channel, he->h_name, AF_INET, unrealdns_cb_nametoip_verify, newr);
#else
	ares_gethostbyname(resolver_channel, he->h_name, ipv6 ? AF_INET6 : AF_INET, unrealdns_cb_nametoip_verify, newr);
#endif
}

int verify_hostname(char *name)
{
char *p;

	if (strlen(name) > HOSTLEN)
		return 0; 

	/* No underscores or other illegal characters */
	for (p = name; *p; p++)
		if (!isalnum(*p) && !strchr(".-", *p))
			return 0;

	return 1;
}


void unrealdns_cb_nametoip_verify(void *arg, int status, int timeouts, struct hostent *he)
{
DNSReq *r = (DNSReq *)arg;
aClient *acptr = r->cptr;
char ipv6 = r->ipv6;
int i;
struct hostent *he2;
u_int32_t ipv4_addr;

	if (!acptr)
		goto bad;

	if ((status != 0) ||
#ifdef INET6
	    ((he->h_length != 4) && (he->h_length != 16)))
#else
	    (he->h_length != 4))
#endif
	{
		/* Failed: error code, or data length is not 4 (nor 16) */
		proceed_normal_client_handshake(acptr, NULL);
		goto bad;
	}

	if (!ipv6)
#ifndef INET6
		ipv4_addr = acptr->ip.S_ADDR;
#else
		inet6_to_inet4(&acptr->ip, &ipv4_addr);
#endif

	/* Verify ip->name and name->ip mapping... */
	for (i = 0; he->h_addr_list[i]; i++)
	{
#ifndef INET6
		if ((he->h_length == 4) && !memcmp(he->h_addr_list[i], &ipv4_addr, 4))
			break;
#else
		if (ipv6)
		{
			if ((he->h_length == 16) && !memcmp(he->h_addr_list[i], &acptr->ip, 16))
				break;
		} else {
			if ((he->h_length == 4) && !memcmp(he->h_addr_list[i], &ipv4_addr, 4))
				break;
		}
#endif
	}

	if (!he->h_addr_list[i])
	{
		/* Failed name <-> IP mapping */
		proceed_normal_client_handshake(acptr, NULL);
		goto bad;
	}

	if (!verify_hostname(r->name))
	{
		/* Hostname is bad, don't cache and consider unresolved */
		proceed_normal_client_handshake(acptr, NULL);
		goto bad;
	}

	/* Entry was found, verified, and can be added to cache */
	unrealdns_addtocache(r->name, &acptr->ip, sizeof(acptr->ip));
	
	he2 = unreal_create_hostent(r->name, &acptr->ip);
	proceed_normal_client_handshake(acptr, he2);

bad:
	unrealdns_freeandremovereq(r);
}

void unrealdns_cb_nametoip_link(void *arg, int status, int timeouts, struct hostent *he)
{
DNSReq *r = (DNSReq *)arg;
int n;
struct hostent *he2;

	if (!r->linkblock)
	{
		/* Possible if deleted due to rehash async removal */
		unrealdns_freeandremovereq(r);
		return;
	}

	if (status != 0)
	{
#ifdef INET6
		if (r->ipv6)
		{
			/* Retry for IPv4... */
			r->ipv6 = 0;
			ares_gethostbyname(resolver_channel, r->name, AF_INET, unrealdns_cb_nametoip_link, r);
			return;
		}
#endif
		/* fatal error while resolving */
		sendto_realops("Unable to resolve hostname '%s', when trying to connect to server %s.",
			r->name, r->linkblock->servername);
		r->linkblock->refcount--;
		unrealdns_freeandremovereq(r);
		return;
	}
	r->linkblock->refcount--;

#ifdef INET6
	if (((he->h_length != 4) && (he->h_length != 16)) || !he->h_addr_list[0])
#else
	if ((he->h_length != 4) || !he->h_addr_list[0])
#endif
	{
		/* Illegal response -- fatal */
		sendto_realops("Unable to resolve hostname '%s', when trying to connect to server %s.",
			r->name, r->linkblock->servername);
		unrealdns_freeandremovereq(r);
		return;
	}

	/* Ok, since we got here, it seems things were actually succesfull */

	/* Fill in [linkblockstruct]->ipnum */
#ifdef INET6
	if (he->h_length == 4)
		inet4_to_inet6(he->h_addr_list[0], &r->linkblock->ipnum);
	else
#endif
		memcpy(&r->linkblock->ipnum, he->h_addr_list[0], sizeof(struct IN_ADDR));

	he2 = unreal_create_hostent(he->h_name, (struct IN_ADDR *)&he->h_addr_list[0]);

	switch ((n = connect_server(r->linkblock, r->cptr, he2)))
	{
		case 0:
			sendto_realops("Connecting to %s[%s].", r->linkblock->servername, r->linkblock->hostname);
			break;
		case -1:
			sendto_realops("Couldn't connect to %s.", r->linkblock->servername);
			break;
		case -2:
			/* Should not happen since he is not NULL */
			sendto_realops("Hostname %s is unknown for server %s (!?).", r->linkblock->hostname, r->linkblock->servername);
			break;
		default:
			sendto_realops("Connection to %s failed: %s", r->linkblock->servername, STRERROR(n));
	}
	
	unrealdns_freeandremovereq(r);
	/* DONE */
}

static unsigned int unrealdns_haship(void *binaryip, int length)
{
#ifdef INET6
u_int32_t alpha, beta;
#endif

	if (length == 4)
		return (*(unsigned int *)binaryip) % DNS_HASH_SIZE;

#ifndef INET6
	abort(); /* impossible */
#else	
	if (length != 16)
		abort(); /* impossible */
	
	memcpy(&alpha, (char *)binaryip + 8, 4);
	memcpy(&beta, (char *)binaryip + 12, 4);

	return (alpha ^ beta) % DNS_HASH_SIZE;
#endif
}

static void unrealdns_addtocache(char *name, void *binaryip, int length)
{
unsigned int hashv;
DNSCache *c, *n;
struct IN_ADDR addr;

	dnsstats.cache_adds++;
#ifdef INET6
	/* If it was an ipv4 address, then we need to convert it to ipv4-in-ipv6 first. */
	if (length == 4)
	{
		inet4_to_inet6(binaryip, &addr);
	} else
#endif
		memcpy(&addr, binaryip, length);

	hashv = unrealdns_haship(binaryip, length);

	/* Check first if it is already present in the cache.
	 * This is possible, when 2 clients connect at the same time.
	 */	
	for (c = cache_hashtbl[hashv]; c; c = c->hnext)
		if (!memcmp(&addr, &c->addr, sizeof(struct IN_ADDR)))
			break;

	if (c)
	{
		/* Already present (don't add duplicate), return. */
		return;
	}

	/* Remove last item, if we got too many.. */
	if (unrealdns_num_cache >= DNS_MAX_ENTRIES)
	{
		for (c = cache_list; c->next; c = c->next);
		if (!c)
			abort(); /* impossible */
		unrealdns_removecacherecord(c);
	}

	/* Create record */
	c = MyMallocEx(sizeof(DNSCache));
	c->name = strdup(name);
	c->expires = TStime() + DNSCACHE_TTL;
	memcpy(&c->addr, &addr, sizeof(addr));
	
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
static char *unrealdns_findcache_byaddr(struct IN_ADDR *addr)
{
unsigned int hashv;
DNSCache *c;

	hashv = unrealdns_haship(addr, sizeof(struct IN_ADDR));
	
	for (c = cache_hashtbl[hashv]; c; c = c->hnext)
		if (!memcmp(addr, &c->addr, sizeof(struct IN_ADDR)))
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
		hashv = unrealdns_haship(&c->addr, sizeof(struct IN_ADDR));
		if (cache_hashtbl[hashv] != c)
			abort(); /* impossible */
		cache_hashtbl[hashv] = c->hnext;
	}
	
	if (c->hnext)
		c->hnext->hprev = c->hprev;
	
	MyFree(c->name);
	MyFree(c);

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
		{
#if 0
			sendto_realops(sptr, "[Syzop/DNS] Expire: %s [%s] (%ld < %ld)",
				c->name, Inet_ia2p(&c->addr), c->expires, TStime());
#endif
			unrealdns_removecacherecord(c);
		}
	}
}

struct hostent *unreal_create_hostent(char *name, struct IN_ADDR *addr)
{
struct hostent *he;

	/* Create a hostent structure (I HATE HOSTENTS) and return it.. */
	he = MyMallocEx(sizeof(struct hostent));
	he->h_name = strdup(name);
	he->h_addrtype = AFINET;
	he->h_length = sizeof(struct IN_ADDR);
	he->h_addr_list = MyMallocEx(sizeof(char *) * 2); /* alocate an array of 2 pointers */
	he->h_addr_list[0] = MyMallocEx(sizeof(struct IN_ADDR));
	memcpy(he->h_addr_list[0], addr, sizeof(struct IN_ADDR));

	return he;
}

void unreal_free_hostent(struct hostent *he)
{
	MyFree(he->h_name);
	MyFree(he->h_addr_list[0]);
	MyFree(he->h_addr_list);
	MyFree(he);
}

static void unrealdns_freeandremovereq(DNSReq *r)
{
	if (r->prev)
		r->prev->next = r->next;
	else
		requests = r->next; /* new HEAD */
	
	if (r->next)
		r->next->prev = r->prev;

	if (r->name)
		MyFree(r->name);
	MyFree(r);
}

/** Delete requests for client 'cptr'.
 * Actually we DO NOT (and should not) delete them, but simply mark them as 'dead'.
 */
void unrealdns_delreq_bycptr(aClient *cptr)
{
DNSReq *r;

	for (r = requests; r; r = r->next)
		if (r->cptr == cptr)
			r->cptr = NULL;
}

void unrealdns_delasyncconnects(void)
{
DNSReq *r;
	for (r = requests; r; r = r->next)
		if (r->type == DNSREQ_CONNECT)
			r->linkblock = NULL;
	
}

CMD_FUNC(m_dns)
{
DNSCache *c;
DNSReq *r;
char *param;

	if (!IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if ((parc > 1) && !BadPtr(parv[1]))
		param = parv[1];
	else
		param = "";

	if (*param == 'l') /* LIST CACHE */
	{
		sendtxtnumeric(sptr, "DNS CACHE List (%u items):", unrealdns_num_cache);
		for (c = cache_list; c; c = c->next)
			sendtxtnumeric(sptr, " %s [%s]", c->name, Inet_ia2p(&c->addr));
	} else
	if (*param == 'r') /* LIST REQUESTS */
	{
		sendtxtnumeric(sptr, "DNS Request List:");
		for (r = requests; r; r = r->next)
			sendtxtnumeric(sptr, " %s",
				r->cptr ? Inet_ia2p(&r->cptr->ip) : "<client lost>");
	} else
	if (*param == 'c') /* CLEAR CACHE */
	{
		sendto_realops("%s (%s@%s) cleared the DNS cache list (/QUOTE DNS c)",
			sptr->name, sptr->user->username, sptr->user->realhost);
		
		while (cache_list)
		{
			c = cache_list->next;
			MyFree(cache_list->name);
			MyFree(cache_list);
			cache_list = c;
		}
		memset(&cache_hashtbl, 0, sizeof(cache_hashtbl));
		unrealdns_num_cache = 0;
		sendnotice(sptr, "DNS Cache has been cleared");
	} else
	if (*param == 'i') /* INFORMATION */
	{
		struct ares_config_info inf;
		int i;
		
		ares_get_config(&inf, resolver_channel);

		sendtxtnumeric(sptr, "****** DNS Configuration Information ******");
		sendtxtnumeric(sptr, " c-ares version: %s",ares_version(NULL));
		sendtxtnumeric(sptr, "        timeout: %d", inf.timeout);
		sendtxtnumeric(sptr, "          tries: %d", inf.tries);
		sendtxtnumeric(sptr, "   # of servers: %d", inf.numservers);
		for (i = 0; i < inf.numservers; i++)
			sendtxtnumeric(sptr, "      server #%d: %s", i+1, inf.servers[i] ? inf.servers[i] : "[???]");
			
		/* TODO: free or get memleak ! */
		sendtxtnumeric(sptr, "****** End of DNS Configuration Info ******");
	} else /* STATISTICS */
	{
		sendtxtnumeric(sptr, "DNS CACHE Stats:");
		sendtxtnumeric(sptr, " hits: %d", dnsstats.cache_hits);
		sendtxtnumeric(sptr, " misses: %d", dnsstats.cache_misses);
	}
	return 0;
}
