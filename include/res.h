/*
 * ircd/res_def.h (C)opyright 1992 Darren Reed.
 */

#define	RES_INITLIST	1
#define	RES_CALLINIT	2
#define RES_INITSOCK	4
#define RES_INITDEBG	8
#define RES_INITCACH    16

#define MAXPACKET	1024
#define MAXALIASES	35
#define MAXADDRS	35

#define	AR_TTL		300	/* minimum TTL in seconds for dns cache entries */

struct	hent {
	char	*h_name;	/* official name of host */
	char	*h_aliases[MAXALIASES];	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	/* list of addresses from name server */
	struct	IN_ADDR	h_addr_list[MAXADDRS];
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

typedef	struct	reslist {
	int	id;
	int	sent;	/* number of requests sent */
	int	srch;
	time_t	ttl;
	char	type;
	char	retries; /* retry counter */
	char	sends;	/* number of sends (>1 means resent) */
	char	resend;	/* send flag. 0 == dont resend */
	time_t	sentat;
	time_t	timeout;
	struct	IN_ADDR	addr;
	char	*name;
	struct	reslist	*next;
	Link	cinfo;
	struct	hent he;
	} ResRQ;

typedef	struct	cache {
	time_t	expireat;
	time_t	ttl;
	struct hostent he;
	struct	cache	*hname_next, *hnum_next, *list_next;
	} aCache;

typedef struct	cachetable {
	aCache	*num_list;
	aCache	*name_list;
	} CacheTable;

#define ARES_CACSIZE	101

#define	MAXCACHED	81
#ifdef _WIN32
typedef unsigned short u_int16_t;
#endif
extern struct __res_state ircd_res;
extern int ircd_res_init();
extern u_int ircd_res_randomid();
extern u_int16_t ircd_getshort(const u_char *msgp);
extern u_int32_t ircd_getlong(const u_char *msgp);
extern void ircd__putshort(register u_int16_t s, register u_char *msgp);
extern void ircd__putlong(register u_int32_t l,register u_char *msgp);
extern int ircd_dn_expand(const u_char *msg, const u_char *eom, const u_char *src, char *dst, int dstsiz);
extern int __ircd_dn_skipname(const u_char *ptr, const u_char *eom);
extern int ircd_dn_comp(const char *src, u_char *dst, int dstsiz, u_char **dnptrs, u_char **lastdnptr);
extern int ircd_res_mkquery(int op, const char *dname, int class, int type, const u_char *data, 
	int datalen, const u_char *newrr_in, u_char *buf, int buflen);
