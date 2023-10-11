/* OMG... OMG! WHAT AN INCLUDE HORROR !!! */
#include <ares.h>
#include <ares_version.h>

typedef enum {
	DNSREQ_CLIENT = 1,
	DNSREQ_LINKCONF = 2,
	DNSREQ_CONNECT = 3
} DNSReqType;

typedef struct DNSReq DNSReq;


/** DNS Request that is ongoing - used in src/dns.c.
 * Depending on the request type, some fields are filled in:
 * .client: DNSREQ_CLIENT, DNSREQ_CONNECT
 * .link: DNSREQ_LINKCONF, DNSREQ_CONNECT
 */
struct DNSReq {
	DNSReq *prev, *next;
	char *name;			/**< Name being resolved (only for DNSREQ_LINKCONF and DNSREQ_CONNECT) */
	char ipv6;			/**< Resolving for ipv6 or ipv4? */
	DNSReqType type;		/**< DNS Request type (DNSREQ_*) */
	Client *client;			/**< Client the request is for, NULL if client died OR unavailable */
	ConfigItem_link *linkblock;	/**< Linkblock */
};

typedef struct DNSCache DNSCache;

/** DNS Cache entry - used in src/dns.c */
struct DNSCache {
	DNSCache *prev, *next;		/**< Previous and next in linked list */
	DNSCache *hprev, *hnext;	/**< Previous and next in hash list */
	char *name;			/**< The hostname */
	char *ip;			/**< The IP address */
	time_t expires;			/**< When record expires */
};

typedef struct DNSStats DNSStats;

struct DNSStats {
	unsigned int cache_hits;
	unsigned int cache_misses;
	unsigned int cache_adds;
};

/** Time to keep cache records. */
#define DNS_CACHE_TTL			600
#define DNS_NEGCACHE_TTL		60

/** Size of the DNS cache hash table. */
#define DNS_HASH_SIZE	4096

/** Max # of entries we want in our cache.
 * This:
 * a) prevents us from using too much memory, and
 * b) prevents us from keeping useless cache records
 *
 * A dnscache item is roughly ~120 bytes in size,
 * so 4096*120=480kb, which seems reasonable ;).
 *
 * Note that in most situations there will be far
 * fewer items, as the TTL is rather short.
 */
#define DNS_MAX_ENTRIES	DNS_HASH_SIZE

extern ares_channel resolver_channel_client;
extern ares_channel resolver_channel_dnsbl;

extern void init_resolver(int);

struct hostent *unrealdns_doclient(Client *cptr);

extern void unreal_gethostbyname_dnsbl(const char *name, int family, ares_host_callback callback, void *arg);

