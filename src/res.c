/*
 * src/res.c (C)opyright 1992, 1993, 1994 Darren Reed. All rights reserved.
 * This file may not be distributed without the author's prior permission in
 * any shape or form. The author takes no responsibility for any damage or
 * loss of property which results from the use of this software.  Distribution
 * of this file must include this notice.
 */
// *INDENT-OFF*
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "h.h"

#include <signal.h>
#ifndef _WIN32
#include <sys/time.h>
#include <sys/socket.h>
#endif
#include "nameser.h"
#include "resolv.h"

#ifndef lint
static  char sccsid[] = "@(#)res.c	2.38 4/13/94 (C) 1992 Darren Reed";
#endif

ID_CVS("$Id$");
ID_Copyright("(C) 1992 Darren Reed");
ID_Notes("2.38 4/13/94");

#undef	DEBUG	/* because there is a lot of debug code in here :-) */
#define INADDRSZ sizeof(struct IN_ADDR)
#define IN6ADDRSZ sizeof(struct IN_ADDR)
extern	int	dn_expand PROTO((char *, char *, char *, char *, int));
extern	int	dn_skipname PROTO((char *, char *));
extern	int	res_mkquery PROTO((int, char *, int, int, char *, int,
				   struct rrec *, char *, int));

#ifndef _WIN32
extern	int	errno, h_errno;
#endif
extern	int	highest_fd;
extern	aClient	*local[];

static	char	hostbuf[512]; /* tq lamego/ptlink ircd */
static	char	dot[] = ".";
static	int	incache = 0;
static	CacheTable	hashtable[ARES_CACSIZE];
static	aCache	*cachetop = NULL;
static	ResRQ	*last, *first;

static	void	rem_cache PROTO((aCache *));
static	void	rem_request PROTO((ResRQ *));
static	int	do_query_name PROTO((Link *, char *, ResRQ *));
static	int	do_query_number PROTO((Link *, struct IN_ADDR *, ResRQ *));
static	void	resend_query PROTO((ResRQ *));
static	int	proc_answer PROTO((ResRQ *, HEADER *, char *, char *));
static	int	query_name PROTO((char *, int, int, ResRQ *));
static	aCache	*make_cache PROTO((ResRQ *));
static	aCache	*find_cache_name PROTO((char *));
static	aCache	*find_cache_number PROTO((ResRQ *, char *));
static	int	add_request PROTO((ResRQ *));
static	ResRQ	*make_request PROTO((Link *));
static	int	send_res_msg PROTO((char *, int, int));
static	ResRQ	*find_id PROTO((int));
static	int	hash_number PROTO((unsigned char *));
static	void	update_list PROTO((ResRQ *, aCache *));
static	int	hash_name PROTO((char *));

#ifdef _WIN32
static	void	async_dns(void *parm);
#endif

static	struct cacheinfo {
	int	ca_adds;
	int	ca_dels;
	int	ca_expires;
	int	ca_lookups;
	int	ca_na_hits;
	int	ca_nu_hits;
	int	ca_updates;
} cainfo;

static	struct	resinfo {
	int	re_errors;
	int	re_nu_look;
	int	re_na_look;
	int	re_replies;
	int	re_requests;
	int	re_resends;
	int	re_sent;
	int	re_timeouts;
	int	re_shortttl;
	int	re_unkrep;
} reinfo;

int	init_resolver(op)
int	op;
{
	int	ret = 0;

#ifdef	LRAND48
	srand48(TStime());
#endif
	if (op & RES_INITLIST)
	    {
		bzero((char *)&reinfo, sizeof(reinfo));
		first = last = NULL;
	    }
	if (op & RES_CALLINIT)
	    {
		ret = res_init();
		if (!_res.nscount)
		    {
			_res.nscount = 1;
			_res.nsaddr_list[0].SIN_ADDR.S_ADDR =
				inet_addr("127.0.0.1");
		    }
	    }

	if (op & RES_INITSOCK)
        {
#ifndef _WIN32
        	int on = 0;
		ret = resfd = socket(AFINET, SOCK_DGRAM, 0);
		set_non_blocking(resfd, &me);
                (void) setsockopt(ret, SOL_SOCKET, SO_BROADCAST,
                    (char *)&on, sizeof(on));
#else
		/* We use Windows internal resolv functions so we have nothing
		 * to do here
		 */
#endif
        }
#ifdef DEBUG
	if (op & RES_INITDEBG);
		_res.options |= RES_DEBUG;
#endif
	if (op & RES_INITCACH)
	    {
		bzero((char *)&cainfo, sizeof(cainfo));
		bzero((char *)hashtable, sizeof(hashtable));
	    }
	if (op == 0)
		ret = resfd;
	return ret;
}

static	int	add_request(new)
ResRQ *new;
{
	if (!new)
		return -1;
	if (!first)
		first = last = new;
	else
	    {
		last->next = new;
		last = new;
	    }
	new->next = NULL;
	reinfo.re_requests++;
	return 0;
}

/*
 * remove a request from the list. This must also free any memory that has
 * been allocated for temporary storage of DNS results.
 */
static	void	rem_request(old)
ResRQ	*old;
{
		ResRQ	**rptr, *r2ptr = NULL;
		int	i;
		char	*s;

	if (!old)
		return;
#ifdef _WIN32
	while (old->locked)
		Sleep(0);
#endif
	for (rptr = &first; *rptr; r2ptr = *rptr, rptr = &(*rptr)->next)
		if (*rptr == old)
		    {
			*rptr = old->next;
			if (last == old)
				last = r2ptr;
			break;
		    }
#ifdef	DEBUG
	Debug((DEBUG_INFO,"rem_request:Remove %#x at %#x %#x",
		old, *rptr, r2ptr));
#endif
	r2ptr = old;
#ifndef _WIN32
	if (r2ptr->he.h_name)
		MyFree((char *)r2ptr->he.h_name);
	for (i = 0; i < MAXALIASES; i++)
		if ((s = r2ptr->he.h_aliases[i]))
			MyFree(s);
#else
	if (r2ptr->he)
		MyFree(r2ptr->he);
#endif
	if (r2ptr->name)
		MyFree(r2ptr->name);
	MyFree(r2ptr);

	return;
}

/*
 * Create a DNS request record for the server.
 */
static	ResRQ	*make_request(lp)
Link	*lp;
{
		ResRQ	*nreq;

	nreq = (ResRQ *)MyMalloc(sizeof(ResRQ));
	bzero((char *)nreq, sizeof(ResRQ));
	nreq->next = NULL; /* where NULL is non-zero ;) */
	nreq->sentat = TStime();
	nreq->retries = HOST_RETRIES;
	nreq->resend = 1;
	nreq->srch = -1;
	if (lp)
		bcopy((char *)lp, (char *)&nreq->cinfo, sizeof(Link));
	else
		bzero((char *)&nreq->cinfo, sizeof(Link));
	nreq->timeout = HOST_TIMEOUT;
#ifndef _WIN32
	nreq->he.h_addrtype = AFINET;
	nreq->he.h_name = NULL;
	nreq->he.h_aliases[0] = NULL;
#else
	nreq->he = (struct hostent *)MyMalloc(MAXGETHOSTSTRUCT);
	bzero((char *)nreq->he, MAXGETHOSTSTRUCT);
	nreq->he->h_addrtype = AFINET;
	nreq->he->h_name = NULL;
#endif
	(void)add_request(nreq);
	return nreq;
}

/*
 * Remove queries from the list which have been there too long without
 * being resolved.
 */
time_t	timeout_query_list(now)
time_t	now;
{
		ResRQ	*rptr, *r2ptr;
		time_t	next = 0, tout;
	aClient	*cptr;

	Debug((DEBUG_DNS,"timeout_query_list at %s",myctime(now)));
	for (rptr = first; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->next;
		tout = rptr->sentat + rptr->timeout;
#ifndef _WIN32
		if (now >= tout)
#else
		if (now >= tout && !rptr->locked)
#endif
			if (--rptr->retries <= 0)
			    {
#ifdef DEBUG
				Debug((DEBUG_ERROR,"timeout %x now %d cptr %x",
					rptr, now, rptr->cinfo.value.cptr));
#endif
				reinfo.re_timeouts++;
				cptr = rptr->cinfo.value.cptr;
				switch (rptr->cinfo.flags)
				{
				case ASYNC_CLIENT :
#ifdef SHOWCONNECTINFO
#ifndef _WIN32
					write(cptr->fd, REPORT_FAIL_DNS,
						R_fail_dns);
#else
					send(cptr->fd, REPORT_FAIL_DNS,
						R_fail_dns, 0);
#endif
#endif
					ClearDNS(cptr);
					if (!DoingAuth(cptr))
						SetAccess(cptr);
					break;
				case ASYNC_SERVER :
					sendto_ops("Host %s unknown",
						   rptr->name);
					ClearDNS(cptr);
					if (check_server(cptr, NULL,
							 NULL, NULL, 1))
						(void)exit_client(cptr, cptr,
							&me, "No Permission");
					break;
				case ASYNC_CONNECT :
					sendto_ops("Host %s unknown",
						   rptr->name);
					break;
				}
				rem_request(rptr);
				continue;
			    }
			else
			    {
				rptr->sentat = now;
				rptr->timeout += rptr->timeout;
#ifndef _WIN32
 				resend_query(rptr);
#endif
				tout = now + rptr->timeout;
#ifdef DEBUG
				Debug((DEBUG_INFO,"r %x now %d retry %d c %x",
					rptr, now, rptr->retries,
					rptr->cinfo.value.cptr));
#endif
			    }
		if (!next || tout < next)
			next = tout;
	    }
	Debug((DEBUG_DNS,"Next timeout_query_list() at %s, %d",
	    myctime((next > now) ? next : (now + AR_TTL)),
	    (next > now) ? (next - now) : AR_TTL));
	return (next > now) ? next : (now + AR_TTL);
}

/*
 * del_queries - called by the server to cleanup outstanding queries for
 * which there no longer exist clients or conf lines.
 */
void	del_queries(cp)
char	*cp;
{
		ResRQ	*rptr, *r2ptr;

	for (rptr = first; rptr; rptr = r2ptr)
	    {
		r2ptr = rptr->next;
		if (cp == rptr->cinfo.value.cp)
			rem_request(rptr);
	    }
}

#ifndef _WIN32
/*
 * sends msg to all nameservers found in the "_res" structure.
 * This should reflect /etc/resolv.conf. We will get responses
 * which arent needed but is easier than checking to see if nameserver
 * isnt present. Returns number of messages successfully sent to 
 * nameservers or -1 if no successful sends.
 */
static	int	send_res_msg(msg, len, rcount)
char	*msg;
int	len, rcount;
{
		int	i;
	int	sent = 0, max;

	if (!msg)
		return -1;

	max = MIN(_res.nscount, rcount);
	if (_res.options & RES_PRIMARY)
		max = 1;
	if (!max)
		max = 1;

	for (i = 0; i < max; i++)
	    {
		_res.nsaddr_list[i].SIN_FAMILY = AFINET;
		if (sendto(resfd, msg, len, 0, (struct SOCKADDR *)
			&(_res.nsaddr_list[i]), sizeof(struct SOCKADDR)) == len)
		    {
			reinfo.re_sent++;
			sent++;
		    }
		else
			Debug((DEBUG_ERROR,"s_r_m:sendto: %d on %d",
				errno, resfd));
	    }

	return (sent) ? sent : -1;
}
#endif /*_WIN32*/

/*
 * find a dns request id (id is determined by dn_mkquery)
 */
static	ResRQ	*find_id(id)
int	id;
{
		ResRQ	*rptr;

	for (rptr = first; rptr; rptr = rptr->next)
		if (rptr->id == id)
			return rptr;
	return NULL;
}

struct	hostent	*gethost_byname(name, lp)
char	*name;
Link	*lp;
{
		aCache	*cp;

	reinfo.re_na_look++;
	if ((cp = find_cache_name(name)))
#ifndef _WIN32
		return (struct hostent *)&(cp->he);
#else
		return (struct hostent *)cp->he;
#endif
	if (!lp)
		return NULL;
	(void)do_query_name(lp, name, NULL);
	return NULL;
}

struct	hostent	*gethost_byaddr(addr, lp)
char	*addr;
Link	*lp;
{
	aCache	*cp;

	reinfo.re_nu_look++;
	if ((cp = find_cache_number(NULL, addr)))
#ifndef _WIN32
		return (struct hostent *)&(cp->he);
#else
		return (struct hostent *)cp->he;
#endif
	if (!lp)
		return NULL;
	(void)do_query_number(lp, (struct IN_ADDR *)addr, NULL);
	return NULL;
}

static	int	do_query_name(lp, name, rptr)
Link	*lp;
char	*name;
	ResRQ	*rptr;
{
#ifndef _WIN32
	char	hname[HOSTLEN+1];
	int	len;

	(void)strncpy(hname, name, sizeof(hname) - 1);
	len = strlen(hname);

	if (rptr && !index(hname, '.') && _res.options & RES_DEFNAMES)
	    {
		(void)strncat(hname, dot, sizeof(hname) - len - 1);
		len++;
		(void)strncat(hname, _res.defdname, sizeof(hname) - len -1);
	    }
#endif
	/*
	 * Store the name passed as the one to lookup and generate other host
	 * names to pass onto the nameserver(s) for lookups.
	 */
	if (!rptr)
	    {
		rptr = make_request(lp);
		rptr->type = T_A;
		rptr->name = (char *)MyMalloc(strlen(name) + 1);
		(void)strcpy(rptr->name, name);
	    }
#ifndef _WIN32
	return (query_name(hname, C_IN, T_A, rptr));
#else

	rptr->id = _beginthread(async_dns, 0, (void *)rptr);
	rptr->sends++;
	return 0;
#endif
}

/*
 * Use this to do reverse IP# lookups.
 */
static	int	do_query_number(lp, numb, rptr)
Link	*lp;
struct	IN_ADDR	*numb;
	ResRQ	*rptr;
{
	char	ipbuf[128];
		u_char	*cp;

#ifndef _WIN32
	cp = (u_char *)&numb->S_ADDR;
	(void)ircsprintf(ipbuf,"%u.%u.%u.%u.in-addr.arpa.",
		(u_int)(cp[3]), (u_int)(cp[2]),
		(u_int)(cp[1]), (u_int)(cp[0]));
#endif
	if (!rptr)
	    {
		rptr = make_request(lp);
		rptr->type = T_PTR;
		rptr->addr.S_ADDR = numb->S_ADDR;
#ifndef _WIN32

		bcopy((char *)&numb->S_ADDR,
			(char *)&rptr->he.h_addr, sizeof(struct IN_ADDR));
		rptr->he.h_length = sizeof(struct IN_ADDR);
#else
		rptr->he->h_length = sizeof(struct IN_ADDR);
#endif
	    }
#ifndef _WIN32
	return (query_name(ipbuf, C_IN, T_PTR, rptr));
#else
	rptr->id = _beginthread(async_dns, 0, (void *)rptr);
	rptr->sends++;
	return 0;
#endif
}

#ifndef _WIN32
/*
 * generate a query based on class, type and name.
 */
static	int	query_name(name, class, type, rptr)
char	*name;
int	class, type;
ResRQ	*rptr;
{
	struct	timeval	tv;
	char	buf[MAXPACKET];
	int	r,s,k = 0;
	HEADER	*hptr;

        Debug((DEBUG_DNS,"query_name: na %s cl %d ty %d", name, class, type));
	bzero(buf, sizeof(buf));
	r = res_mkquery(QUERY, name, class, type, NULL, 0, NULL,
			buf, sizeof(buf));
	if (r <= 0)
	    {
		h_errno = NO_RECOVERY;
		return r;
	    }
	hptr = (HEADER *)buf;
#ifdef LRAND48
        do {
		hptr->id = htons(ntohs(hptr->id) + k + lrand48() & 0xffff);
#else
	(void) gettimeofday(&tv, NULL);
	do {
		/* htons/ntohs can be assembler macros, which cannot
		   be nested. Thus two lines.	-Vesa		    */
		u_short nstmp = ntohs(hptr->id) + k +
				(u_short)(tv.tv_usec & 0xffff);
		hptr->id = htons(nstmp);
#endif /* LRAND48 */
		k++;
	} while (find_id(ntohs(hptr->id)));
	rptr->id = ntohs(hptr->id);
	rptr->sends++;
	s = send_res_msg(buf, r, rptr->sends);
	if (s == -1)
	    {
		h_errno = TRY_AGAIN;
		return -1;
	    }
	else
		rptr->sent += s;
	return 0;
}

static	void	resend_query(rptr)
ResRQ	*rptr;
{
	if (rptr->resend == 0)
		return;
	reinfo.re_resends++;
	switch(rptr->type)
	{
	case T_PTR:
		(void)do_query_number(NULL, &rptr->addr, rptr);
		break;
	case T_A:
		(void)do_query_name(NULL, rptr->name, rptr);
		break;

	default:
		break;
	}
	return;
}

/*
 * process name server reply.
 */
static	int	proc_answer(rptr, hptr, buf, eob)
ResRQ	*rptr;
char	*buf, *eob;
HEADER	*hptr;
{
		char	*cp, **alias;
	struct	hent	*hp;
	int	class, type, dlen, len, ans = 0, n;
	struct	IN_ADDR	dr, *adr;

	cp = buf + sizeof(HEADER);
	hp = (struct hent *)&(rptr->he);
	adr = &hp->h_addr;
	while (adr->S_ADDR)
		adr++;
	alias = hp->h_aliases;
	while (*alias)
		alias++;
#ifdef	_SOLARIS		/* brain damaged compiler (Solaris2) it seems */
	for (; hptr->qdcount > 0; hptr->qdcount--)
#else
	while (hptr->qdcount-- > 0)
#endif
		if ((n = dn_skipname(cp, eob)) == -1)
			break;
		else
			cp += (n + QFIXEDSZ);
	/*
	 * proccess each answer sent to us blech.
	 */
	while (hptr->ancount-- > 0 && cp && cp < eob) {
		n = dn_expand(buf, eob, cp, hostbuf, sizeof(hostbuf));
		if (n <= 0)
			break;

		cp += n;
		type = (int)_getshort(cp);
		cp += sizeof(short);
		class = (int)_getshort(cp);
		cp += sizeof(short);
		rptr->ttl = _getlong(cp);
       /* This should really use the GETLONG macro which advances
        * the pointer for us, but I don't know if that'll break other
        * systems. sizeof(_getlong) does not always equal sizeof(time_t).
        * This is the case on Linux alpha. 4 is the current standard
        * for this portion of the resolver reply it would seem.
	* heydowns@borg.com
        */
#ifdef __alpha
                cp += 4;
#else
#ifndef _WIN32
		cp += sizeof(rptr->ttl);
#else
		cp += 4;
#endif
#endif
   
		dlen =  (int)_getshort(cp);
		cp += sizeof(short);
		rptr->type = type;

		len = strlen(hostbuf);
		/* name server never returns with trailing '.' */
		if (!index(hostbuf,'.') && (_res.options & RES_DEFNAMES))
		    {
			(void)strcat(hostbuf, dot);
			len++;
			(void)strncat(hostbuf, _res.defdname,
				sizeof(hostbuf) - 1 - len);
			len = MIN(len + strlen(_res.defdname),
				  sizeof(hostbuf) - 1);
		    }

		switch(type)
		{
		case T_A :
			hp->h_length = dlen;
			if (ans == 1)
				hp->h_addrtype =  (class == C_IN) ?
							AFINET : AF_UNSPEC;
			bcopy(cp, (char *)&dr, dlen);

			adr->S_ADDR = dr.S_ADDR;
			Debug((DEBUG_INFO,"got ip # %s for %s",
				inetntoa((char *)adr), hostbuf));
			if (!hp->h_name)
			    {
				hp->h_name =(char *)MyMalloc(len+1);
				(void)strcpy(hp->h_name, hostbuf);
			    }
			ans++;
			adr++;
			cp += dlen;
 			break;
		case T_PTR :
			if((n = dn_expand(buf, eob, cp, hostbuf,
					  sizeof(hostbuf) )) < 0)
			    {
				cp = NULL;
				break;
			    }
			cp += n;
			len = strlen(hostbuf);
			Debug((DEBUG_INFO,"got host %s",hostbuf));
			/*
			 * copy the returned hostname into the host name
			 * or alias field if there is a known hostname
			 * already.
			 */
			if (hp->h_name)
			    {
				if (alias >= &(hp->h_aliases[MAXALIASES-1]))
					break;
				*alias = (char *)MyMalloc(len + 1);
				(void)strcpy(*alias++, hostbuf);
				*alias = NULL;
			    }
			else
			    {
				hp->h_name = (char *)MyMalloc(len + 1);
				(void)strcpy(hp->h_name, hostbuf);
			    }
			ans++;
			break;
		case T_CNAME :
			cp += dlen;
			Debug((DEBUG_INFO,"got cname %s", hostbuf));
			if (alias >= &(hp->h_aliases[MAXALIASES-1]))
				break;
			*alias = (char *)MyMalloc(len + 1);
			(void)strcpy(*alias++, hostbuf);
			*alias = NULL;
			ans++;
			break;
		default :
#ifdef DEBUG
			Debug((DEBUG_INFO,"proc_answer: type:%d for:%s",
			      type, hostbuf));
#endif
			break;
		}
	}
	return ans;
}
#endif /*_WIN32*/

/*
 * read a dns reply from the nameserver and process it.
 */
#ifndef _WIN32
struct	hostent	*get_res(lp)
char	*lp;
#else
struct	hostent	*get_res(lp, id)
char	*lp;
long   id;
#endif
{
#ifndef _WIN32
	static	char	buf[sizeof(HEADER) + MAXPACKET];
		HEADER	*hptr;
	struct	SOCKADDR_IN	sin;
	int	rc, a, len = sizeof(sin), max;
#else
		struct hostent	*he;
#endif
		ResRQ	*rptr = NULL;
	aCache	*cp;

#ifndef _WIN32
	rc = recvfrom(resfd, buf, sizeof(buf), 0, (struct SOCKADDR *)&sin,
		      &len);
	if (rc == -1 || rc <= sizeof(HEADER))
		goto getres_err;
	/*
	 * convert DNS reply reader from Network byte order to CPU byte order.
	 */
	hptr = (HEADER *)buf;
	hptr->id = ntohs(hptr->id);
	hptr->ancount = ntohs(hptr->ancount);
	hptr->qdcount = ntohs(hptr->qdcount);
	hptr->nscount = ntohs(hptr->nscount);
	hptr->arcount = ntohs(hptr->arcount);
#ifdef	DEBUG
	Debug((DEBUG_NOTICE, "get_res:id = %d rcode = %d ancount = %d",
		hptr->id, hptr->rcode, hptr->ancount));
#endif
#endif
	reinfo.re_replies++;
	/*
	 * response for an id which we have already received an answer for
	 * just ignore this response.
	 */
#ifndef _WIN32
	rptr = find_id(hptr->id);
#else
	rptr = find_id(id);
#endif
	if (!rptr)
		goto getres_err;
#ifndef _WIN32
	/*
	 * check against possibly fake replies
	 */
	max = MIN(_res.nscount, rptr->sends);
	if (!max)
		max = 1;

	for (a = 0; a < max; a++)
		if (!_res.nsaddr_list[a].SIN_ADDR.S_ADDR ||
		    !bcmp((char *)&sin.SIN_ADDR,
			  (char *)&_res.nsaddr_list[a].SIN_ADDR,
			  sizeof(struct IN_ADDR)))
			break;
	if (a == max)
	    {
		reinfo.re_unkrep++;
		goto getres_err;
	    }

	if ((hptr->rcode != NOERROR) || (hptr->ancount == 0))
	    {
		switch (hptr->rcode)
		{
		case NXDOMAIN:
			h_errno = TRY_AGAIN;
			break;
		case SERVFAIL:
			h_errno = TRY_AGAIN;
			break;
		case NOERROR:
			h_errno = NO_DATA;
			break;
		case FORMERR:
		case NOTIMP:
		case REFUSED:
		default:
			h_errno = NO_RECOVERY;
			break;
		}
		reinfo.re_errors++;
		/*
		** If a bad error was returned, we stop here and dont send
		** send any more (no retries granted).
		*/
		if (h_errno != TRY_AGAIN)
		    {
			Debug((DEBUG_DNS, "Fatal DNS error %d for %d",
				h_errno, hptr->rcode));
			rptr->resend = 0;
			rptr->retries = 0;
		    }
		goto getres_err;
	    }
	a = proc_answer(rptr, hptr, buf, buf+rc);
#ifdef DEBUG
	Debug((DEBUG_INFO,"get_res:Proc answer = %d",a));
#endif
	if (a && rptr->type == T_PTR)
	    {
		struct	hostent	*hp2 = NULL;
		Debug((DEBUG_DNS, "relookup %s <-> %s",
			rptr->he.h_name, inetntoa((char *)&rptr->he.h_addr)));
		/*
		 * Lookup the 'authoritive' name that we were given for the
		 * ip#.  By using this call rather than regenerating the
		 * type we automatically gain the use of the cache with no
		 * extra kludges.
		 */
		if ((hp2 = gethost_byname(rptr->he.h_name, &rptr->cinfo)))
			if (lp)
				bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
		/*
		 * If name wasn't found, a request has been queued and it will
		 * be the last one queued.  This is rather nasty way to keep
		 * a host alias with the query. -avalon
		 */
		if (!hp2 && rptr->he.h_aliases[0])
			for (a = 0; rptr->he.h_aliases[a]; a++)
			    {
				Debug((DEBUG_DNS, "Copied CNAME %s for %s",
					rptr->he.h_aliases[a],
					rptr->he.h_name));
				last->he.h_aliases[a] = rptr->he.h_aliases[a];
				rptr->he.h_aliases[a] = NULL;
			    }

		rem_request(rptr);
		return hp2;
	    }

	if (a > 0)
	    {
		if (lp)
			bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
		cp = make_cache(rptr);
#ifdef	DEBUG
	Debug((DEBUG_INFO,"get_res:cp=%#x rptr=%#x (made)",cp,rptr));
#endif

		rem_request(rptr);
	    }
	else
		if (!rptr->sent)
			rem_request(rptr);
	return cp ? (struct hostent *)&cp->he : NULL;

getres_err:
	/*
	 * Reprocess an error if the nameserver didnt tell us to "TRY_AGAIN".
	 */
	if (rptr)
	    {
		if (h_errno != TRY_AGAIN)
		    {
			/*
			 * If we havent tried with the default domain and its
			 * set, then give it a try next.
			 */
			if (_res.options & RES_DEFNAMES && ++rptr->srch == 0)
			    {
				rptr->retries = _res.retry;
				rptr->sends = 0;
				rptr->resend = 1;
				resend_query(rptr);
			    }
			else
			{
				resend_query(rptr);
			}
		    }
		else if (lp)
			bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
	    }
#else /*_WIN32*/
	he = rptr->he;

	if (he && he->h_name && ((struct IN_ADDR *)he->h_addr)->S_ADDR &&
	    rptr->locked < 2)
	    {
		/*
		 * We only need to re-check the DNS if its a "byaddr" call,
		 * the "byname" calls will work correctly. -Cabal95
		 */
		char	tempname[120];
		int	i;
		long	amt;
		struct	hostent	*hp, *he = rptr->he;

		strcpy(tempname, he->h_name);
		hp = gethostbyname(tempname);
		if (hp && !bcmp(hp->h_addr, he->h_addr, sizeof(struct IN_ADDR)))
		    {
		    }
		else
			rptr->he->h_name = NULL;
	    }
	if (lp)
		bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
	cp = make_cache(rptr);
# ifdef DEBUG
	Debug((DEBUG_INFO,"get_res:cp=%#x rptr=%#x (made)", cp, rptr));
# endif
	rptr->locked = 0;
	rem_request(rptr);
	return cp ? (struct hostent *)cp->he : NULL;

getres_err:
	if (lp && rptr)
		bcopy((char *)&rptr->cinfo, lp, sizeof(Link));
#endif
	return (struct hostent *)NULL;
}

static	int	hash_number(ip)
 unsigned char *ip;
{
	u_int	hashv = 0;

	/* could use loop but slower */
	hashv += (int)*ip++;
	hashv += hashv + (int)*ip++;
	hashv += hashv + (int)*ip++;
	hashv += hashv + (int)*ip++;

	hashv %= ARES_CACSIZE;
	return (hashv);
}

static	int	hash_name(name)
register	char	*name;
{
		u_int	hashv = 0;

	if (name == NULL) {
		sendto_realops ("Caught NULL pointer in hash_name().  (Bad thing -- tell rwg.)");
		return (0);
	}

	for (; *name && *name != '.'; name++)
		hashv += *name;
	hashv %= ARES_CACSIZE;
	return (hashv);
}

/*
** Add a new cache item to the queue and hash table.
*/
static	aCache	*add_to_cache(ocp)
	aCache	*ocp;
{
		aCache	*cp = NULL;
		int	hashv;

#ifdef DEBUG
	Debug((DEBUG_INFO,
	      "add_to_cache:ocp %#x he %#x name %#x addrl %#x 0 %#x",
		ocp, &ocp->he, ocp->he.h_name, ocp->he.h_addr_list,
		ocp->he.h_addr_list[0]));
#endif
	ocp->list_next = cachetop;
	cachetop = ocp;

#ifndef _WIN32
	hashv = hash_name(ocp->he.h_name);
#else
	hashv = hash_name(ocp->he->h_name);
#endif
	ocp->hname_next = hashtable[hashv].name_list;
	hashtable[hashv].name_list = ocp;

#ifndef _WIN32
	hashv = hash_number((u_char *)ocp->he.h_addr);
#else
	hashv = hash_number((u_char *)ocp->he->h_addr);
#endif
	ocp->hnum_next = hashtable[hashv].num_list;
	hashtable[hashv].num_list = ocp;

#ifdef	DEBUG
	Debug((DEBUG_INFO, "add_to_cache:added %s[%08x] cache %#x.",
# ifndef _WIN32
		ocp->he.h_name, ocp->he.h_addr_list[0], ocp));
# else
		ocp->he->h_name, ocp->he->h_addr_list[0], ocp));
# endif
 	Debug((DEBUG_INFO,
 		"add_to_cache:h1 %d h2 %x lnext %#x namnext %#x numnext %#x",
# ifndef _WIN32
		hash_name(ocp->he.h_name), hashv, ocp->list_next,
# else
		hash_name(ocp->he->h_name), hashv, ocp->list_next,
# endif
		ocp->hname_next, ocp->hnum_next));
#endif

	/*
	 * LRU deletion of excessive cache entries.
	 */
	if (++incache > MAXCACHED)
	    {
		for (cp = cachetop; cp->list_next; cp = cp->list_next)
			;
		rem_cache(cp);
	    }
	cainfo.ca_adds++;

	return ocp;
}

/*
** update_list does not alter the cache structure passed. It is assumed that
** it already contains the correct expire time, if it is a new entry. Old
** entries have the expirey time updated.
*/
static	void	update_list(rptr, cachep)
ResRQ	*rptr;
aCache	*cachep;
{
		aCache	**cpp, *cp = cachep;
		char	*s, *t, **base;
	int	i, j;
	int	addrcount;

	/*
	** search for the new cache item in the cache list by hostname.
	** If found, move the entry to the top of the list and return.
	*/
	cainfo.ca_updates++;

	for (cpp = &cachetop; *cpp; cpp = &((*cpp)->list_next))
		if (cp == *cpp)
			break;
	if (!*cpp)
		return;
	*cpp = cp->list_next;
	cp->list_next = cachetop;
	cachetop = cp;
#ifndef _WIN32
	if (!rptr)
		return;

#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"u_l:cp %#x na %#x al %#x ad %#x",
		cp,cp->he.h_name,cp->he.h_aliases,cp->he.h_addr));
	Debug((DEBUG_DEBUG,"u_l:rptr %#x h_n %#x", rptr, rptr->he.h_name));
#endif
	/*
	 * Compare the cache entry against the new record.  Add any
	 * previously missing names for this entry.
	 */
	for (i = 0; cp->he.h_aliases[i]; i++)
		;
	addrcount = i;
	for (i = 0, s = rptr->he.h_name; s && i < MAXALIASES;
	     s = rptr->he.h_aliases[i++])
	    {
		for (j = 0, t = cp->he.h_name; t && j < MAXALIASES;
		     t = cp->he.h_aliases[j++])
			if (!mycmp(t, s))
				break;
		if (!t && j < MAXALIASES-1)
		    {
			base = cp->he.h_aliases;

			addrcount++;
			base = (char **)MyRealloc((char *)base,
					sizeof(char *) * (addrcount + 1));
			cp->he.h_aliases = base;
#ifdef	DEBUG
			Debug((DEBUG_DNS,"u_l:add name %s hal %x ac %d",
				s, cp->he.h_aliases, addrcount));
#endif
			base[addrcount-1] = s;
			base[addrcount] = NULL;
			if (i)
				rptr->he.h_aliases[i-1] = NULL;
			else
				rptr->he.h_name = NULL;
		    }
	    }
	for (i = 0; cp->he.h_addr_list[i]; i++)
		;
	addrcount = i;

	/*
	 * Do the same again for IP#'s.
	 */
	for (s = (char *)&rptr->he.h_addr.S_ADDR;
	     ((struct IN_ADDR *)s)->S_ADDR; s += sizeof(struct IN_ADDR))
	    {
		for (i = 0; (t = cp->he.h_addr_list[i]); i++)
			if (!bcmp(s, t, sizeof(struct IN_ADDR)))
				break;
		if (i >= MAXADDRS || addrcount >= MAXADDRS)
			break;
		/*
		 * Oh man this is bad...I *HATE* it. -avalon
		 *
		 * Whats it do ?  Reallocate two arrays, one of pointers
		 * to "char *" and the other of IP addresses.  Contents of
		 * the IP array *MUST* be preserved and the pointers into
		 * it recalculated.
		 */
		if (!t)
		    {
			base = cp->he.h_addr_list;
			addrcount++;
			t = (char *)MyRealloc(*base,
					addrcount * sizeof(struct IN_ADDR));
			base = (char **)MyRealloc((char *)base,
					(addrcount + 1) * sizeof(char *));
			cp->he.h_addr_list = base;
#ifdef	DEBUG
			Debug((DEBUG_DNS,"u_l:add IP %x hal %x ac %d",
				ntohl(((struct IN_ADDR *)s)->S_ADDR),
				cp->he.h_addr_list,
				addrcount));
#endif
			for (; addrcount; addrcount--)
			    {
				*base++ = t;
				t += sizeof(struct IN_ADDR);
			    }
			*base = NULL;
			bcopy(s, *--base, sizeof(struct IN_ADDR));
		    }
	    }
#endif /*_WIN32*/
	return;
}

static	aCache	*find_cache_name(name)
char	*name;
{
		aCache	*cp;
		char	*s;
		int	hashv, i;

	hashv = hash_name(name);

	cp = hashtable[hashv].name_list;
#ifdef	DEBUG
	Debug((DEBUG_DNS,"find_cache_name:find %s : hashv = %d",name,hashv));
#endif

	for (; cp; cp = cp->hname_next)
#ifndef _WIN32
		for (i = 0, s = cp->he.h_name; s; s = cp->he.h_aliases[i++])
#else
		for (i = 0, s = cp->he->h_name; s; s = cp->he->h_aliases[i++])
#endif
			if (mycmp(s, name) == 0)
			    {
				cainfo.ca_na_hits++;
				update_list(NULL, cp);
				return cp;
			    }

	for (cp = cachetop; cp; cp = cp->list_next)
	    {
		/*
		 * if no aliases or the hash value matches, we've already
		 * done this entry and all possiblilities concerning it.
		 */
#ifndef _WIN32
		if (!*cp->he.h_aliases)
			continue;
		if (hashv == hash_name(cp->he.h_name))
			continue;
		for (i = 0, s = cp->he.h_aliases[i]; s && i < MAXALIASES; i++)
			if (!mycmp(name, s)) {
#else
		if (!cp->he->h_aliases)
 			continue;
		if (hashv == hash_name(cp->he->h_name))
 			continue;
		for (i = 0, s = cp->he->h_aliases[i]; s && i < MAXALIASES; i++)
			if (!mycmp(name, s)) {
#endif
				cainfo.ca_na_hits++;
				update_list(NULL, cp);
				return cp;
			    }
	    }
	return NULL;
}

/*
 * find a cache entry by ip# and update its expire time
 */
static	aCache	*find_cache_number(rptr, numb)
ResRQ	*rptr;
char	*numb;
{
		aCache	*cp;
		int	hashv,i;
#ifdef	DEBUG
	struct	IN_ADDR	*ip = (struct IN_ADDR *)numb;
#endif

	hashv = hash_number((u_char *)numb);

	cp = hashtable[hashv].num_list;
#if defined(DEBUG) && !defined(INET6)
	Debug((DEBUG_DNS,"find_cache_number:find %s[%08x]: hashv = %d",
		inetntoa(numb), ntohl(ip->S_ADDR), hashv));
#endif

	for (; cp; cp = cp->hnum_next)
#ifndef _WIN32
		for (i = 0; cp->he.h_addr_list[i]; i++)
			if (!bcmp(cp->he.h_addr_list[i], numb,
				  sizeof(struct IN_ADDR)))
#else
		for (i = 0; cp->he->h_addr_list && cp->he->h_addr_list[i]; i++)
			if (!bcmp(cp->he->h_addr_list[i], numb,
				  sizeof(struct IN_ADDR)))
#endif
			    {
				cainfo.ca_nu_hits++;
				update_list(rptr, cp);
				return cp;
			    }

	for (cp = cachetop; cp; cp = cp->list_next)
	    {
		/*
		 * single address entry...would have been done by hashed
		 * search above...
		 */
#ifndef _WIN32
		if (!cp->he.h_addr_list[1])
#else
		if (!cp->he->h_addr_list[1])
#endif
			continue;
		/*
		 * if the first IP# has the same hashnumber as the IP# we
		 * are looking for, its been done already.
		 */
#ifndef _WIN32
		if (hashv == hash_number((u_char *)cp->he.h_addr_list[0]))
			continue;
		for (i = 1; cp->he.h_addr_list[i]; i++)
			if (!bcmp(cp->he.h_addr_list[i], numb,
				  sizeof(struct IN_ADDR)))
#else
		if (hashv == hash_number((u_char *)cp->he->h_addr_list[0]))
 			continue;
		for (i = 1; cp->he->h_addr_list && cp->he->h_addr_list[i]; i++)
			if (!bcmp(cp->he->h_addr_list[i], numb,
				  sizeof(struct IN_ADDR)))
#endif
			    {
				cainfo.ca_nu_hits++;
				update_list(rptr, cp);
				return cp;
			    }
	    }
	return NULL;
}

static	aCache	*make_cache(rptr)
ResRQ	*rptr;
{
		aCache	*cp;
	int	i, n;
	struct	hostent	*hp;
	char	*s, **t;

	/*
	** shouldn't happen but it just might...
	*/
#ifndef _WIN32
	if (!rptr->he.h_name || !rptr->he.h_addr.S_ADDR)
#else
#endif
		return NULL;
	/*
	** Make cache entry.  First check to see if the cache already exists
	** and if so, return a pointer to it.
	*/
#ifndef _WIN32
	if ((cp = find_cache_number(rptr, (char *)&rptr->he.h_addr.S_ADDR)))
		return cp;
	for (i = 1; rptr->he.h_addr_list[i].S_ADDR; i++)
		if ((cp = find_cache_number(rptr,
				(char *)&(rptr->he.h_addr_list[i].S_ADDR))))
#else
	if ((cp = find_cache_number(rptr, (char *)&((struct IN_ADDR *)rptr->he->h_addr)->S_ADDR)))
		return cp;
	for (i = 1; rptr->he->h_addr_list[i] &&
	     ((struct IN_ADDR *)rptr->he->h_addr_list[i])->S_ADDR; i++)
 		if ((cp = find_cache_number(rptr,
				(char *)&((struct IN_ADDR *)rptr->he->h_addr_list[i])->S_ADDR )))
#endif
			return cp;

	/*
	** a matching entry wasnt found in the cache so go and make one up.
	*/ 
	cp = (aCache *)MyMalloc(sizeof(aCache));
	bzero((char *)cp, sizeof(aCache));
#ifdef _WIN32
	cp->he = (struct hostent *)MyMalloc(MAXGETHOSTSTRUCT);
	res_copyhostent(rptr->he, cp->he);
#else
	hp = &cp->he;
	for (i = 0; i < MAXADDRS; i++)
#ifdef INET6
		if (!WHOSTENTP(rptr->he.h_addr_list[i].S_ADDR))
#else
		if (!rptr->he.h_addr_list[i].S_ADDR)
#endif
			break;

	/*
	** build two arrays, one for IP#'s, another of pointers to them.
	*/
	t = hp->h_addr_list = (char **)MyMalloc(sizeof(char *) * (i+1));
	bzero((char *)t, sizeof(char *) * (i+1));

	s = (char *)MyMalloc(sizeof(struct IN_ADDR) * i);
	bzero(s, sizeof(struct IN_ADDR) * i);

	for (n = 0; n < i; n++, s += sizeof(struct IN_ADDR))
	    {
		*t++ = s;
		bcopy((char *)&(rptr->he.h_addr_list[n].S_ADDR), s,
		      sizeof(struct IN_ADDR));
	    }
	*t = (char *)NULL;

	/*
	** an array of pointers to CNAMEs.
	*/
	for (i = 0; i < MAXALIASES; i++)
		if (!rptr->he.h_aliases[i])
			break;
	i++;
	t = hp->h_aliases = (char **)MyMalloc(sizeof(char *) * i);
	for (n = 0; n < i; n++, t++)
	    {
		*t = rptr->he.h_aliases[n];
		rptr->he.h_aliases[n] = NULL;
	    }

	hp->h_addrtype = rptr->he.h_addrtype;
	hp->h_length = rptr->he.h_length;
	hp->h_name = rptr->he.h_name;
#endif
	if (rptr->ttl < 600)
	    {
		reinfo.re_shortttl++;
		cp->ttl = 600;
	    }
	else
		cp->ttl = rptr->ttl;
	cp->expireat = TStime() + cp->ttl;
#ifndef _WIN32
	rptr->he.h_name = NULL;
#else
	rptr->he->h_name = NULL;
#endif
#ifdef DEBUG
	Debug((DEBUG_INFO,"make_cache:made cache %#x", cp));
#endif
	return add_to_cache(cp);
}

/*
 * rem_cache
 *     delete a cache entry from the cache structures and lists and return
 *     all memory used for the cache back to the memory pool.
 */
static	void	rem_cache(ocp)
aCache	*ocp;
{
		aCache	**cp;
#ifndef _WIN32
		struct	hostent *hp = &ocp->he;
#else
		struct	hostent *hp = ocp->he;
#endif
		int	hashv;
		aClient	*cptr;

#ifdef	DEBUG
	Debug((DEBUG_DNS, "rem_cache: ocp %#x hp %#x l_n %#x aliases %#x",
		ocp, hp, ocp->list_next, hp->h_aliases));
#endif
	/*
	** Cleanup any references to this structure by destroying the
	** pointer.
	*/
	for (hashv = highest_fd; hashv >= 0; hashv--)
		if ((cptr = local[hashv]) && (cptr->hostp == hp))
			cptr->hostp = NULL;
	/*
	 * remove cache entry from linked list
	 */
	for (cp = &cachetop; *cp; cp = &((*cp)->list_next))
		if (*cp == ocp)
		    {
			*cp = ocp->list_next;
			break;
		    }
	/*
	 * remove cache entry from hashed name lists
	 */
	hashv = hash_name(hp->h_name);
#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"rem_cache: h_name %s hashv %d next %#x first %#x",
		hp->h_name, hashv, ocp->hname_next,
		hashtable[hashv].name_list));
#endif
	for (cp = &hashtable[hashv].name_list; *cp; cp = &((*cp)->hname_next))
		if (*cp == ocp)
		    {
			*cp = ocp->hname_next;
			break;
		    }
	/*
	 * remove cache entry from hashed number list
	 */
	hashv = hash_number((u_char *)hp->h_addr);
#ifdef	DEBUG
	Debug((DEBUG_DEBUG,"rem_cache: h_addr %s hashv %d next %#x first %#x",
		inetntoa(hp->h_addr), hashv, ocp->hnum_next,
		hashtable[hashv].num_list));
#endif
	for (cp = &hashtable[hashv].num_list; *cp; cp = &((*cp)->hnum_next))
		if (*cp == ocp)
		    {
			*cp = ocp->hnum_next;
			break;
		    }

#ifdef _WIN32
	MyFree((char *)hp);
#else
	/*
	 * free memory used to hold the various host names and the array
	 * of alias pointers.
	 */
	if (hp->h_name)
		MyFree(hp->h_name);
	if (hp->h_aliases)
	    {
		for (hashv = 0; hp->h_aliases[hashv]; hashv++)
			MyFree(hp->h_aliases[hashv]);
		MyFree((char *)hp->h_aliases);
	    }

	/*
	 * free memory used to hold ip numbers and the array of them.
	 */
	if (hp->h_addr_list)
	    {
		if (*hp->h_addr_list)
			MyFree((char *)*hp->h_addr_list);
		MyFree((char *)hp->h_addr_list);
	    }
#endif /*_WIN32*/
	MyFree((char *)ocp);

	incache--;
	cainfo.ca_dels++;

	return;
}

/*
 * removes entries from the cache which are older than their expirey times.
 * returns the time at which the server should next poll the cache.
 */
time_t	expire_cache(now)
time_t	now;
{
		aCache	*cp, *cp2;
		time_t	next = 0;

	for (cp = cachetop; cp; cp = cp2)
	    {
		cp2 = cp->list_next;

		if (now >= cp->expireat)
		    {
			cainfo.ca_expires++;
			rem_cache(cp);
		    }
		else if (!next || next > cp->expireat)
			next = cp->expireat;
	    }
	return (next > now) ? next : (now + AR_TTL);
}

/*
 * remove all dns cache entries.
 */
void	flush_cache()
{
		aCache	*cp;

	while ((cp = cachetop))
		rem_cache(cp);
}

int	m_dns(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int	parc;
char	*parv[];
{
		aCache	*cp;
	int	i;

	if (parv[1] && *parv[1] == 'l') {
		for(cp = cachetop; cp; cp = cp->list_next)
		    {
			sendto_one(sptr, "NOTICE %s :Ex %d ttl %d host %s(%s)",
				   parv[0], cp->expireat - TStime(), cp->ttl,
#ifndef _WIN32
				   cp->he.h_name, inetntoa(cp->he.h_addr));
			for (i = 0; cp->he.h_aliases[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (CN)",
					   parv[0], cp->he.h_name,
					   cp->he.h_aliases[i]);
			for (i = 1; cp->he.h_addr_list[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (IP)",
					   parv[0], cp->he.h_name,
					   inetntoa(cp->he.h_addr_list[i]));
#else
				   cp->he->h_name, inetntoa(cp->he->h_addr));
			for (i = 0; cp->he->h_aliases[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (CN)",
					   parv[0], cp->he->h_name,
					   cp->he->h_aliases[i]);
			for (i = 1; cp->he->h_addr_list[i]; i++)
				sendto_one(sptr,"NOTICE %s : %s = %s (IP)",
					   parv[0], cp->he->h_name,
					   inetntoa(cp->he->h_addr_list[i]));
#endif
		    }
		return 0;
	}
	sendto_one(sptr,"NOTICE %s :Ca %d Cd %d Ce %d Cl %d Ch %d:%d Cu %d",
		   sptr->name,
		   cainfo.ca_adds, cainfo.ca_dels, cainfo.ca_expires,
		   cainfo.ca_lookups,
		   cainfo.ca_na_hits, cainfo.ca_nu_hits, cainfo.ca_updates);

	sendto_one(sptr,"NOTICE %s :Re %d Rl %d/%d Rp %d Rq %d",
		   sptr->name, reinfo.re_errors, reinfo.re_nu_look,
		   reinfo.re_na_look, reinfo.re_replies, reinfo.re_requests);
	sendto_one(sptr,"NOTICE %s :Ru %d Rsh %d Rs %d(%d) Rt %d", sptr->name,
		   reinfo.re_unkrep, reinfo.re_shortttl, reinfo.re_sent,
		   reinfo.re_resends, reinfo.re_timeouts);
	return 0;
}

u_long	cres_mem(sptr)
aClient	*sptr;
{
	register aCache	*c = cachetop;
	register struct	hostent	*h;
	register int	i;
	u_long	nm = 0, im = 0, sm = 0, ts = 0;

	for ( ;c ; c = c->list_next)
	    {
		sm += sizeof(*c);
#ifndef _WIN32
		h = &c->he;
#else
		h = c->he;
#endif
		for (i = 0; h->h_addr_list[i]; i++)
		    {
			im += sizeof(char *);
			im += sizeof(struct IN_ADDR);
		    }
		im += sizeof(char *);
		for (i = 0; h->h_aliases[i]; i++)
		    {
			nm += sizeof(char *);
			nm += strlen(h->h_aliases[i]);
		    }
		nm += i - 1;
		nm += sizeof(char *);
		if (h->h_name)
			nm += strlen(h->h_name);
	    }
	ts = ARES_CACSIZE * sizeof(CacheTable);
	sendto_one(sptr, ":%s %d %s :RES table %d",
		   me.name, RPL_STATSDEBUG, sptr->name, ts);
	sendto_one(sptr, ":%s %d %s :Structs %d IP storage %d Name storage %d",
		   me.name, RPL_STATSDEBUG, sptr->name, sm, im, nm);
	return ts + sm + im + nm;
}

#ifdef _WIN32
/*
 * Main thread function for handling DNS requests.
 */
void	async_dns(void *parm)
{
	ResRQ	*rptr = (ResRQ *)parm;
	struct hostent	*hp, *he = rptr->he;
	int	i, x;
	long	amt;

	if (rptr->type == T_A)
	    {
		rptr->locked = 2;
		hp = gethostbyname(rptr->name);
	    }
	else
	    {
		rptr->locked = 1;
		hp = gethostbyaddr((char *)(&rptr->addr.S_ADDR), 4, PF_INET);
	    }
	if ( !hp )
	    {
		/*
		 * Now heres a stupid check to forget, this apprently is
		 * what hasbeen causing most of the crashes.  I hope anyway.
		 */
		do_dns_async(rptr->id);
		_endthread();
	    }
	if ( (hp->h_aliases[0] && (hp->h_aliases[0]-(char *)hp)>MAXGETHOSTSTRUCT) ||
	     (hp->h_addr_list[0] && (hp->h_addr_list[0]-(char *)hp)>MAXGETHOSTSTRUCT))
	    {
		/*
		 * Seems windows does some weird, aka stupid, stuff with DNS.
		 * If the address is resolved from the HOSTS file, then the
		 * pointers will exceed MAXGETHOSTSTRUCT. Good and bad. Good
		 * because its an easy way to tell if the Admin is spoofing
		 * with his HOSTS file, bad because it also causes invalid
		 * pointers without this check. -Cabal95
		 */
		do_dns_async(rptr->id);
		_endthread();
	    }

	res_copyhostent(hp, rptr->he);
	do_dns_async(rptr->id);
	_endthread();
}

int	res_copyhostent(struct hostent *from, struct hostent *to)
{
	int	amt, x, i;

	to->h_addrtype = from->h_addrtype;
	to->h_length = from->h_length;
	/*
	 * Get to "primary" offset in to hostent buffer and copy over
	 * to hostname.
	 */
	amt = (long)to + sizeof(struct hostent);
	to->h_name = (char *)amt;
	strcpy(to->h_name, from->h_name);
	amt += strlen(to->h_name)+1;
	/* Setup tto alias list */
	if (amt&0x3)
		amt = (amt&0xFFFFFFFC)+4;	
	to->h_aliases = (char **)amt;
	for (x = 0; from->h_aliases[x]; x++)
		;
	x *= sizeof(char *);
	amt += sizeof(char *);
	for (i = 0; from->h_aliases[i]; i++)
	    {
		to->h_aliases[i] = (char *)(amt+x);
		strcpy(to->h_aliases[i], from->h_aliases[i]);
		amt += strlen(to->h_aliases[i])+1;
		if (amt&0x3)
			amt = (amt&0xFFFFFFFC)+4;	
	    }
	to->h_aliases[i] = NULL;
	/* Setup tto IP address list */
	to->h_addr_list = (char **)amt;
	for (x = 0; from->h_addr_list[x]; x++)
		;
	x *= sizeof(char *);
	for (i = 0; from->h_addr_list[i]; i++)
	    {
		amt += 4;
		to->h_addr_list[i] = (char *)(amt+x);
		((struct IN_ADDR *)to->h_addr_list[i])->S_ADDR = ((struct IN_ADDR *)from->h_addr_list[i])->S_ADDR;
	    }
	to->h_addr_list[i] = NULL;
}
#endif /*_WIN32*/
