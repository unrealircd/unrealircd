#include <ares.h>
#include <ares_version.h>

typedef struct _dnsreq DNSReq;

struct _dnsreq {
	DNSReq *prev, *next;
	aClient *cptr; /**< Client the request is for, if NULL then the client died */
	char ipv6; /**< Resolving for ipv6 or ipv4? */
};


typedef struct _dnscache DNSCache;

struct _dnscache {
	DNSCache *prev, *next;		/**< Previous and next in linked list */
	DNSCache *hprev, *hnext;	/**< Previous and next in hash list */
	char *name;					/**< The hostname */
	struct IN_ADDR addr;		/**< Stored IP address */
	time_t expires;				/**< When record expires */
};

typedef struct _dnsstats DNSStats;

struct _dnsstats {
	unsigned int cache_hits;
	unsigned int cache_misses;
	unsigned int cache_adds;
};

/** Time to keep cache records. */
#define DNSCACHE_TTL			600

/** Size of the hash table (prime!).
 * Consumes <this>*4 on ia32 and <this>*4 on 64 bit
 * 241 seems a good bet.. which ~1k on ia32 and ~2k on ia64.
 */
#define DNS_HASH_SIZE	241

/** Max # of entries we want in our cache.
 * This:
 * a) prevents us from using too much memory, and
 * b) prevents us from keeping useless cache records
 *
 * A dnscache item is roughly ~80 bytes in size (slightly more on x86),
 * so 241*80=~20k, which seems reasonable ;).
 */
#define DNS_MAX_ENTRIES	DNS_HASH_SIZE


extern ares_channel resolver_channel;

extern void init_resolver(void);

struct hostent *unrealdns_doclient(aClient *cptr);
