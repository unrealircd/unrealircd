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
static unsigned int unrealdns_haship(char *ip);
static void unrealdns_addtocache(char *name, char *ip);
static char *unrealdns_findcache_ip(char *ip);
struct hostent *unreal_create_hostent(char *name, char *ip);
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
		/* Socket is going to be closed *BY C-ARES*..
		 * so don't call fd_close() but fd_unmap().
		 */
		fd_unmap(fd);
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
	fd_open(fd, "DNS Resolver Socket");
	return ARES_SUCCESS;
}

static EVENT(unrealdns_timeout)
{
	ares_process_fd(resolver_channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}

static Event *unrealdns_timeout_hdl = NULL;

void init_resolver(int firsttime)
{
struct ares_options options;
int n, v, k;
int optmask;

	if (requests)
		abort(); /* should never happen */
		
	if (firsttime)
	{
		memset(&cache_hashtbl, 0, sizeof(cache_hashtbl));
		memset(&dnsstats, 0, sizeof(dnsstats));
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
	optmask = ARES_OPT_TIMEOUTMS|ARES_OPT_TRIES|ARES_OPT_FLAGS|ARES_OPT_SOCK_STATE_CB;
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
	unrealdns_timeout_hdl = EventAddEx(NULL, "unrealdns_timeout", 0, 0, unrealdns_timeout, NULL);
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

	EventDel(unrealdns_timeout_hdl);

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
	static struct hostent *he;
	char *cache_name;

	cache_name = unrealdns_findcache_ip(cptr->ip);
	if (cache_name)
		return unreal_create_hostent(cache_name, cptr->ip);

	/* Create a request */
	r = MyMallocEx(sizeof(DNSReq));
	r->cptr = cptr;
	r->ipv6 = IsIPV6(cptr);
	unrealdns_addreqtolist(r);

	/* Execute it */
	if (r->ipv6)
	{
		struct in6_addr addr;
		memset(&addr, 0, sizeof(addr));
		inet_pton(AF_INET6, cptr->ip, &addr);
		ares_gethostbyaddr(resolver_channel, &addr, 16, AF_INET6, unrealdns_cb_iptoname, r);
	} else {
		struct in_addr addr;
		memset(&addr, 0, sizeof(addr));
		inet_pton(AF_INET, cptr->ip, &addr);
		ares_gethostbyaddr(resolver_channel, &addr, 4, AF_INET, unrealdns_cb_iptoname, r);
	}

	return NULL;
}

/** Resolve a name to an IP, for a link block.
 */
void unrealdns_gethostbyname_link(char *name, ConfigItem_link *conf)
{
	DNSReq *r;

	/* Create a request */
	r = MyMallocEx(sizeof(DNSReq));
	r->linkblock = conf;
	r->name = strdup(name);
	if (!DISABLE_IPV6)
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

	ares_gethostbyname(resolver_channel, he->h_name, ipv6 ? AF_INET6 : AF_INET, unrealdns_cb_nametoip_verify, newr);
}

/*
  returns:
  1 = good hostname
  0 = bad hostname
 */
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
	u_int32_t ipv4_addr = 0;

	if (!acptr)
		goto bad;

	if ((status != 0) || (ipv6 && (he->h_length != 16)) || (!ipv6 && (he->h_length != 4)))
	{
		/* Failed: error code, or data length is incorrect */
		proceed_normal_client_handshake(acptr, NULL);
		goto bad;
	}

	/* Verify ip->name and name->ip mapping... */
	for (i = 0; he->h_addr_list[i]; i++)
	{
		if (r->ipv6)
		{
			struct in6_addr addr;
			if (inet_pton(AF_INET6, acptr->ip, &addr) != 1)
				continue; /* something fucked */
			if (!memcmp(he->h_addr_list[i], &addr, 16))
				break; /* MATCH */
		} else {
			struct in_addr addr;
			if (inet_pton(AF_INET, acptr->ip, &addr) != 1)
				continue; /* something fucked */
			if (!memcmp(he->h_addr_list[i], &addr, 4))
				break; /* MATCH */
		}
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

	unrealdns_addtocache(r->name, acptr->ip);
	
	he2 = unreal_create_hostent(r->name, acptr->ip);
	proceed_normal_client_handshake(acptr, he2);

bad:
	unrealdns_freeandremovereq(r);
}

void unrealdns_cb_nametoip_link(void *arg, int status, int timeouts, struct hostent *he)
{
	DNSReq *r = (DNSReq *)arg;
	int n;
	struct hostent *he2;
	char ipbuf[HOSTLEN+1];
	char *ip = NULL;

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
		sendto_realops("Unable to resolve hostname '%s', when trying to connect to server %s.",
			r->name, r->linkblock->servername);
		r->linkblock->refcount--;
		unrealdns_freeandremovereq(r);
		return;
	}
	r->linkblock->refcount--;

	if (!he->h_addr_list[0] || (he->h_length != (r->ipv6 ? 16 : 4)) ||
	    !(ip = inetntop(r->ipv6 ? AF_INET6 : AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf))))
	{
		/* Illegal response -- fatal */
		sendto_realops("Unable to resolve hostname '%s', when trying to connect to server %s.",
			r->name, r->linkblock->servername);
		unrealdns_freeandremovereq(r);
		return;
	}
	
	/* Ok, since we got here, it seems things were actually succesfull */

	/* Fill in [linkblockstruct]->ipnum */
	r->linkblock->connect_ip = strdup(ip);
	he2 = unreal_create_hostent(he->h_name, ip);

	switch ((n = connect_server(r->linkblock, r->cptr, he2)))
	{
		case 0:
			sendto_realops("Connecting to %s[%s].", r->linkblock->servername, ip);
			break;
		case -1:
			sendto_realops("Couldn't connect to %s[%s].", r->linkblock->servername, ip);
			break;
		case -2:
			/* Should not happen since he is not NULL */
			sendto_realops("Hostname %s is unknown for server %s (!?).", r->linkblock->outgoing.hostname, r->linkblock->servername);
			break;
		default:
			sendto_realops("Connection to %s failed: %s", r->linkblock->servername, STRERROR(n));
	}
	
	unrealdns_freeandremovereq(r);
	/* DONE */
}

static unsigned int unrealdns_haship(char *ip)
{
	extern unsigned hash_nick_name(const char *nname);

	return hash_nick_name(ip) % DNS_HASH_SIZE; /* TODO: improve I guess ;D */
}

static void unrealdns_addtocache(char *name, char *ip)
{
	unsigned int hashv;
	DNSCache *c, *n;

	dnsstats.cache_adds++;

	hashv = unrealdns_haship(ip);

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
	c = MyMallocEx(sizeof(DNSCache));
	c->name = strdup(name);
	c->ip = strdup(ip);
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
static char *unrealdns_findcache_ip(char *ip)
{
	unsigned int hashv;
	DNSCache *c;

	hashv = unrealdns_haship(ip);
	
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
		hashv = unrealdns_haship(c->ip);
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
				c->name, c->ip, c->expires, TStime());
#endif
			unrealdns_removecacherecord(c);
		}
	}
}

struct hostent *unreal_create_hostent(char *name, char *ip)
{
struct hostent *he;

	/* Create a hostent structure (I HATE HOSTENTS) and return it.. */
	he = MyMallocEx(sizeof(struct hostent));
	he->h_name = strdup(name);
	if (strchr(ip, ':'))
	{
		/* IPv6 */
		he->h_addrtype = AF_INET6;
		he->h_length = sizeof(struct in6_addr);
		he->h_addr_list = MyMallocEx(sizeof(char *) * 2); /* alocate an array of 2 pointers */
		he->h_addr_list[0] = MyMallocEx(sizeof(struct in6_addr));
		inet_pton(AF_INET6, ip, he->h_addr_list[0]);
	} else {
		he->h_addrtype = AF_INET;
		he->h_length = sizeof(struct in_addr);
		he->h_addr_list = MyMallocEx(sizeof(char *) * 2); /* alocate an array of 2 pointers */
		he->h_addr_list[0] = MyMallocEx(sizeof(struct in_addr));
		inet_pton(AF_INET, ip, he->h_addr_list[0]);
	}

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

	if (!ValidatePermissionsForPath("server:dns",sptr,NULL,NULL,NULL))
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
			sendtxtnumeric(sptr, " %s [%s]", c->name, c->ip);
	} else
	if (*param == 'r') /* LIST REQUESTS */
	{
		sendtxtnumeric(sptr, "DNS Request List:");
		for (r = requests; r; r = r->next)
			sendtxtnumeric(sptr, " %s", r->cptr ? r->cptr->ip : "<client lost>");
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
		struct ares_options inf;
		struct ares_addr_node *ns = NULL;
		int i;
		int optmask;
		

		sendtxtnumeric(sptr, "****** DNS Configuration Information ******");
		sendtxtnumeric(sptr, " c-ares version: %s",ares_version(NULL));
		
		i = 0;
		for (ares_get_servers(resolver_channel, &ns); ns; ns = ns->next)
		{
			char ipbuf[128], *ip;
			i++;
			
			ip = inetntop(ns->family, &ns->addr, ipbuf, sizeof(ipbuf));
			sendtxtnumeric(sptr, "      server #%d: %s", i, ip ? ip : "<error>");
		}

		ares_save_options(resolver_channel, &inf, &optmask);
		if (optmask & ARES_OPT_TIMEOUTMS)
			sendtxtnumeric(sptr, "        timeout: %d", inf.timeout);
		if (optmask & ARES_OPT_TRIES)
			sendtxtnumeric(sptr, "          tries: %d", inf.tries);
		if (optmask & ARES_OPT_DOMAINS)
		{
			sendtxtnumeric(sptr, "   # of search domains: %d", inf.ndomains);
			for (i = 0; i < inf.ndomains; i++)
				sendtxtnumeric(sptr, "      domain #%d: %s", i+1, inf.domains[i]);
		}
		sendtxtnumeric(sptr, "****** End of DNS Configuration Info ******");
		
		ares_destroy_options(&inf);
	} else /* STATISTICS */
	{
		sendtxtnumeric(sptr, "DNS CACHE Stats:");
		sendtxtnumeric(sptr, " hits: %d", dnsstats.cache_hits);
		sendtxtnumeric(sptr, " misses: %d", dnsstats.cache_misses);
	}
	return 0;
}

/* Little helper function for dnsbl module.
 * No we will NOT copy the entire c-ares api, just this one.
 */
void unreal_gethostbyname(const char *name, int family, ares_host_callback callback, void *arg)
{
	ares_gethostbyname(resolver_channel, name, family, callback, arg);
}
