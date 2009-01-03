/*
 *   Unreal Internet Relay Chat Daemon, src/s_bsd.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/* -- Jto -- 07 Jul 1990
 * Added jlp@hamblin.byu.edu's debugtty fix
 */

/* -- Armin -- Jun 18 1990
 * Added setdtablesize() for more socket connections
 */

/* -- Jto -- 13 May 1990
 * Added several fixes from msa:
 *   Better error messages
 *   Changes in check_access
 * Added SO_REUSEADDR fix from zessel@informatik.uni-kl.de
 */

#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)	2.78 2/7/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "res.h"
#include "numeric.h"
#include "version.h"
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#else
#include <io.h>
#endif
#if defined(_SOLARIS)
#include <sys/filio.h>
#endif
#include "inet.h"
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#ifdef	AIX
# include <time.h>
# include <arpa/nameser.h>
#else
# include "nameser.h"
#endif
#include "sock.h"		/* If FD_ZERO isn't define up to this point,  */
#include <string.h>
#include "proto.h"
			/* define it (BSD4.2 needs this) */
#include "h.h"
#ifndef NO_FDLIST
#include  "fdlist.h"
#endif
#ifdef USE_POLL
#include <sys/poll.h>
int  rr;

#endif
#ifdef INET6
static unsigned char minus_one[] =
    { 255, 255, 255, 255, 255, 255, 255, 255, 255,
        255, 255, 255, 255, 255, 255, 255, 0
};
#endif

#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET	0x7f
#endif

#ifndef INADDRSZ
#define INADDRSZ sizeof(struct IN_ADDR)
#define IN6ADDRSZ sizeof(struct IN_ADDR)
#endif

#ifndef _WIN32
#define SET_ERRNO(x) errno = x
#else
#define SET_ERRNO(x) WSASetLastError(x)
#endif /* _WIN32 */

extern char backupbuf[8192];
aClient *local[MAXCONNECTIONS];
short    LastSlot = -1;    /* GLOBAL - last used slot in local */
int      OpenFiles = 0;    /* GLOBAL - number of files currently open */
int readcalls = 0;
static struct SOCKADDR_IN mysk;

static struct SOCKADDR *connect_inet(ConfigItem_link *, aClient *, int *);
int completed_connection(aClient *);
static int check_init(aClient *, char *, size_t);
static void do_dns_async();
void set_sock_opts(int, aClient *);
static char readbuf[READBUF_SIZE];
char zlinebuf[BUFSIZE];
extern char *version;
extern ircstats IRCstats;
MODVAR TS last_allinuse = 0;

#ifndef NO_FDLIST
extern fdlist default_fdlist;
extern fdlist busycli_fdlist;
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;
extern fdlist socks_fdlist;
#endif

#ifdef USE_LIBCURL
extern void url_do_transfers_async(void);
#endif

/*
 * Try and find the correct name to use with getrlimit() for setting the max.
 * number of files allowed to be open by this process.
 */
#ifdef RLIMIT_FDMAX
# define RLIMIT_FD_MAX   RLIMIT_FDMAX
#else
# ifdef RLIMIT_NOFILE
#  define RLIMIT_FD_MAX RLIMIT_NOFILE
# else
#  ifdef RLIMIT_OPEN_MAX
#   define RLIMIT_FD_MAX RLIMIT_OPEN_MAX
#  else
#   undef RLIMIT_FD_MAX
#  endif
# endif
#endif
void start_of_normal_client_handshake(aClient *acptr);
void proceed_normal_client_handshake(aClient *acptr, struct hostent *he);

/* winlocal */
void add_local_client(aClient* cptr)
{
	int		i;
	if (LastSlot >= (MAXCONNECTIONS-1))
	{
		Debug((DEBUG_ERROR, "add_local_client() called when LastSlot >= MAXCONNECTIONS!"));
		cptr->slot = -1;
		return;
	}
	i = 0;
	while (local[i])
		i++;
	cptr->slot = i;
	local[cptr->slot] = cptr;
	if (i > LastSlot)
		LastSlot = i;
}

void remove_local_client(aClient* cptr)
{

	if (LastSlot < 0)
	{
		Debug((DEBUG_ERROR, "remove_local_client() called when LastSlot < 0!"));
		cptr->slot = -1;
		return;
	}

	/* Keep LastSlot as the last one
	 */
	local[cptr->slot] = NULL;
	cptr->slot = -1;
	while (!local[LastSlot])
		LastSlot--;
}

void close_connections(void)
{
  aClient* cptr;
  int i = LastSlot;

  for ( ; i >= 0; --i)
  {
    if ((cptr = local[i]) != 0)
    {
      if (cptr->fd >= 0) {
        CLOSE_SOCK(cptr->fd);
        cptr->fd = -2;
      }
      if (cptr->authfd >= 0)
      {
        CLOSE_SOCK(cptr->authfd);
        cptr->authfd = -1;
      }
    }
  }
  OpenFiles = 0;
  LastSlot = -1;
#ifdef _WIN32
	  WSACleanup();
#endif
}

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

void add_local_domain(char *hname, int size)
{
#if 0
	/* try to fix up unqualified names */
	if (!index(hname, '.'))
	{
		if (!(ircd_res.options & RES_INIT))
		{
			Debug((DEBUG_DNS, "res_init()"));
			ircd_res_init();
		}
		if (ircd_res.defdname[0])
		{
			(void)strncat(hname, ".", size - 1);
			(void)strncat(hname, ircd_res.defdname, size - 2);
		}
	}
#endif
	return;
}

/*
** Cannot use perror() within daemon. stderr is closed in
** ircd and cannot be used. And, worse yet, it might have
** been reassigned to a normal connection...
*/

/*
** report_error
**	This a replacement for perror(). Record error to log and
**	also send a copy to all *LOCAL* opers online.
**
**	text	is a *format* string for outputting error. It must
**		contain only two '%s', the first will be replaced
**		by the sockhost from the cptr, and the latter will
**		be taken from strerror(errno).
**
**	cptr	if not NULL, is the *LOCAL* client associated with
**		the error.
*/
void report_error(char *text, aClient *cptr)
{
	int errtmp = ERRNO, origerr = ERRNO;
	char *host, xbuf[256];
	int  err, len = sizeof(err), n;
	
	host = (cptr) ? get_client_name(cptr, FALSE) : "";

	Debug((DEBUG_ERROR, text, host, STRERROR(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (cptr && !IsMe(cptr) && cptr->fd >= 0)
		if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR,
		    (OPT_TYPE *)&err, &len))
			if (err)
				errtmp = err;
#endif
	if (origerr != errtmp) {
		/* Socket error is different than original error,
		 * some tricks are needed because of 2x strerror() (or at least
		 * according to the man page) -- Syzop.
		 */
		snprintf(xbuf, 200, "[syserr='%s'", STRERROR(origerr));
		n = strlen(xbuf);
		snprintf(xbuf+n, 256-n, ", sockerr='%s']", STRERROR(errtmp));
		sendto_snomask(SNO_JUNK, text, host, xbuf);
		ircd_log(LOG_ERROR, text, host, xbuf);
	} else {
		sendto_snomask(SNO_JUNK, text, host, STRERROR(errtmp));
		ircd_log(LOG_ERROR, text,host,STRERROR(errtmp));
	}
	return;
}

void report_baderror(char *text, aClient *cptr)
{
#ifndef _WIN32
	int  errtmp = errno;	/* debug may change 'errno' */
#else
	int  errtmp = WSAGetLastError();	/* debug may change 'errno' */
#endif
	char *host;
	int  err, len = sizeof(err);

	host = (cptr) ? get_client_name(cptr, FALSE) : "";

/*	fprintf(stderr, text, host, strerror(errtmp));
	fputc('\n', stderr); */
	Debug((DEBUG_ERROR, text, host, STRERROR(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (cptr && !IsMe(cptr) && cptr->fd >= 0)
		if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR,
		    (OPT_TYPE *)&err, &len))
			if (err)
				errtmp = err;
#endif
	sendto_umode(UMODE_OPER, text, host, STRERROR(errtmp));
	ircd_log(LOG_ERROR, text, host, STRERROR(errtmp));
	return;
}

/*
 * inetport
 *
 * Create a socket in the AFINET domain, bind it to the port given in
 * 'port' and listen to it.  Connections are accepted to this socket
 * depending on the IP# mask given by 'name'.  Returns the fd of the
 * socket created or -1 on error.
 */
int  inetport(aClient *cptr, char *name, int port)
{
	static struct SOCKADDR_IN server;
	int  ad[4], len = sizeof(server);
	char ipname[64];

	if (BadPtr(name))
		name = "*";
	ad[0] = ad[1] = ad[2] = ad[3] = 0;

	/*
	 * do it this way because building ip# from separate values for each
	 * byte requires endian knowledge or some nasty messing. Also means
	 * easy conversion of "*" 0.0.0.0 or 134.* to 134.0.0.0 :-)
	 */
#ifndef INET6
	(void)sscanf(name, "%d.%d.%d.%d", &ad[0], &ad[1], &ad[2], &ad[3]);
	(void)ircsprintf(ipname, "%d.%d.%d.%d", ad[0], ad[1], ad[2], ad[3]);
#else
	if (*name == '*')
		ircsprintf(ipname, "::");
	else
		strlcpy(ipname, name, sizeof(ipname));
#endif

	if (cptr != &me)
	{
		(void)ircsprintf(cptr->sockhost, "%-.42s.%.u",
		    name, (unsigned int)port);
		(void)strlcpy(cptr->name, me.name, sizeof cptr->name);
	}
	/*
	 * At first, open a new socket
	 */
	if (cptr->fd == -1)
	{
		cptr->fd = socket(AFINET, SOCK_STREAM, 0);
	}
	if (cptr->fd < 0)
	{
#if !defined(DEBUGMODE) && !defined(_WIN32)
#endif
		report_baderror("Cannot open stream socket() %s:%s", cptr);
		return -1;
	}
	else if (++OpenFiles >= MAXCLIENTS)
	{
		sendto_ops("No more connections allowed (%s)", cptr->name);
		CLOSE_SOCK(cptr->fd);
		cptr->fd = -1;
		--OpenFiles;
		return -1;
	}
	set_sock_opts(cptr->fd, cptr);
	/*
	 * Bind a port to listen for new connections if port is non-null,
	 * else assume it is already open and try get something from it.
	 */
	if (port)
	{
		server.SIN_FAMILY = AFINET;
		/* per-port bindings, fixes /stats l */
		
#ifndef INET6
		server.SIN_ADDR.S_ADDR = inet_addr(ipname);
#else
		inet_pton(AFINET, ipname, server.SIN_ADDR.S_ADDR);
#endif
		server.SIN_PORT = htons(port);
		/*
		 * Try 10 times to bind the socket with an interval of 20
		 * seconds. Do this so we dont have to keepp trying manually
		 * to bind. Why ? Because a port that has closed often lingers
		 * around for a short time.
		 * This used to be the case.  Now it no longer is.
		 * Could cause the server to hang for too long - avalon
		 */
		if (bind(cptr->fd, (struct SOCKADDR *)&server,
		    sizeof(server)) == -1)
		{
			ircsprintf(backupbuf, "Error binding stream socket to IP %s port %i",
				ipname, port);
			strlcat(backupbuf, " - %s:%s", sizeof backupbuf);
			report_baderror(backupbuf, cptr);
#if !defined(_WIN32) && defined(INET6)
			/* Check if ipv4-over-ipv6 (::ffff:a.b.c.d, RFC2553
			 * section 3.7) is disabled, like at newer FreeBSD's. -- Syzop
			 */
			if (!strncasecmp(ipname, "::ffff:", 7))
			{
				ircd_log(LOG_ERROR, "You are trying to bind to an IPv4 address, "
				                    "make sure the address exists at your machine. "
				                    "If you are using *BSD you might need to "
				                    "enable ipv6_ipv4mapping in /etc/rc.conf "
				                    "and/or via sysctl.");
			}
#endif
			CLOSE_SOCK(cptr->fd);
			cptr->fd = -1;
			--OpenFiles;
			return -1;
		}
	}
	if (getsockname(cptr->fd, (struct SOCKADDR *)&server, &len))
	{
		report_error("getsockname failed for %s:%s", cptr);
		CLOSE_SOCK(cptr->fd);
		cptr->fd = -1;
		--OpenFiles;
		return -1;
	}

#ifdef INET6
	bcopy(server.sin6_addr.s6_addr, cptr->ip.s6_addr, IN6ADDRSZ);
#else
	cptr->ip.S_ADDR = name ? inet_addr(ipname) : me.ip.S_ADDR;
#endif
	cptr->port = (int)ntohs(server.SIN_PORT);
	(void)listen(cptr->fd, LISTEN_SIZE);
	add_local_client(cptr);
	return 0;
}

int add_listener2(ConfigItem_listen *conf)
{
	aClient *cptr;

	cptr = make_client(NULL, NULL);
	cptr->flags = FLAGS_LISTEN;
	cptr->listener = cptr;
	cptr->from = cptr;
	SetMe(cptr);
	strncpyzt(cptr->name, conf->ip, sizeof(cptr->name));
	if (inetport(cptr, conf->ip, conf->port))
		cptr->fd = -2;
	cptr->class = (ConfigItem_class *)conf;
	cptr->umodes = conf->options ? conf->options : LISTENER_NORMAL;
	if (cptr->fd >= 0)
	{	
		cptr->umodes |= LISTENER_BOUND;
		conf->options |= LISTENER_BOUND;
		conf->listener = cptr;
		set_non_blocking(cptr->fd, cptr);
		return 1;
	}
	else
	{
		free_client(cptr);
		return -1;
	}

}

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections.
 */

void close_listeners(void)
{
	aClient *cptr;
	int  i, reloop = 1;
	ConfigItem_listen *aconf;

	/*
	 * close all 'extra' listening ports we have
	 */
	while (reloop)
	{
		reloop = 0;
		for (i = LastSlot; i >= 0; i--)
		{
			if (!(cptr = local[i]))
				continue;
			if (!IsMe(cptr) || cptr == &me || !IsListening(cptr))
				continue;
			aconf = (ConfigItem_listen *) cptr->class;

			if (aconf->flag.temporary && (aconf->clients == 0))
			{
				close_connection(cptr);
				/* need to start over because close_connection() may have 
				** rearranged local[]!
				*/
				reloop = 1;
			}
		}
	}
}

/*
 * init_sys
 */
void init_sys(void)
{
	int  fd;
#ifndef USE_POLL
#ifdef RLIMIT_FD_MAX
	struct rlimit limit;

	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	{
		if (limit.rlim_max < MAXCONNECTIONS)
		{
			(void)fprintf(stderr, "The OS enforces a limit on max open files\n");
#ifndef LONG_LONG_RLIM_T
			(void)fprintf(stderr, "Hard Limit: %ld MAXCONNECTIONS: %d\n",
#else
			(void)fprintf(stderr, "Hard Limit: %lld MAXCONNECTIONS: %d\n",
#endif
			    limit.rlim_max, MAXCONNECTIONS);
			(void)fprintf(stderr, "Fix MAXCONNECTIONS\n");
			exit(-1);
		}
	limit.rlim_cur = limit.rlim_max;	/* make soft limit the max */
	if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
	{
/* HACK: if it's mac os X then don't error... */
#ifndef OSXTIGER
#ifndef LONG_LONG_RLIM_T
		(void)fprintf(stderr, "error setting max fd's to %ld\n",
#else
		(void)fprintf(stderr, "error setting max fd's to %lld\n",
#endif
		    limit.rlim_cur);
		exit(-1);
#endif
	}
}
#endif
#endif
#ifndef _WIN32
	if (MAXCONNECTIONS > FD_SETSIZE)
	{
		fprintf(stderr, "MAXCONNECTIONS (%d) is higher than FD_SETSIZE (%d)\n", MAXCONNECTIONS, FD_SETSIZE);
		fprintf(stderr, "You might need to recompile the IRCd, or if you're running Linux, read the release notes\n");
		exit(-1);
	}
#endif
	/* Startup message
	   pid = getpid();
	   pid++;
	   fprintf(stderr, "|---------------------------------------------\n");
	   fprintf(stderr, "| UnrealIRCD has successfully loaded.\n");
	   fprintf(stderr, "| Config Directory: %s\n", DPATH);
	   fprintf(stderr, "| MAXCONNECTIONS set at %d\n", MAXCONNECTIONS);
	   fprintf(stderr, "| Process ID: %d\n", pid);
	   fprintf(stderr, "|---------------------------------------------\n"); */
#if defined(PCS) || defined(SVR3)
char logbuf[BUFSIZ];

(void)setvbuf(stderr, logbuf, _IOLBF, sizeof(logbuf));
#else
# if defined(HPUX)
(void)setvbuf(stderr, NULL, _IOLBF, 0);
# else
#  if !defined(_SOLARIS) && !defined(_WIN32)
(void)setlinebuf(stderr);
#  endif
# endif
#endif
#ifndef _WIN32
#ifndef NOCLOSEFD
for (fd = 3; fd < MAXCONNECTIONS; fd++)
{
	(void)close(fd);
}
(void)close(1);
#endif

if (bootopt & BOOT_TTY)		/* debugging is going to a tty */
	goto init_dgram;
#ifndef NOCLOSEFD
if (!(bootopt & BOOT_DEBUG))
	(void)close(2);
#endif

if ((bootopt & BOOT_CONSOLE) || isatty(0))
{
#ifndef _AMIGA
/*		if (fork())
			exit(0);
*/
#endif
#ifdef TIOCNOTTY
	if ((fd = open("/dev/tty", O_RDWR)) >= 0)
	{
		(void)ioctl(fd, TIOCNOTTY, (char *)NULL);
#ifndef NOCLOSEFD
		(void)close(fd);
#endif
	}
#endif

#if defined(HPUX) || defined(_SOLARIS) || \
    defined(_POSIX_SOURCE) || defined(SVR4) || defined(SGI) || defined(OSXTIGER)
	(void)setsid();
#else
	(void)setpgrp(0, (int)getpid());
#endif
#ifndef NOCLOSEFD
	(void)close(0);		/* fd 0 opened by inetd */
#endif
	local[0] = NULL;
}
init_dgram:
#else
#ifndef NOCLOSEFD
	close(fileno(stdin));
	close(fileno(stdout));
	if (!(bootopt & BOOT_DEBUG))
	close(fileno(stderr));
#endif
	memset(local, 0, sizeof(aClient*) * MAXCONNECTIONS);
	LastSlot = -1;

#endif /*_WIN32*/

#ifndef CHROOTDIR
	init_resolver(1);
#endif
	return;
}

void write_pidfile(void)
{
#ifdef IRCD_PIDFILE
	int  fd;
	char buff[20];
	if ((fd = open(IRCD_PIDFILE, O_CREAT | O_WRONLY, 0600)) >= 0)
	{
		bzero(buff, sizeof(buff));
		(void)ircsprintf(buff, "%5d\n", (int)getpid());
		if (write(fd, buff, strlen(buff)) == -1)
			Debug((DEBUG_NOTICE, "Error writing to pid file %s",
			    IRCD_PIDFILE));
		(void)close(fd);
		return;
	}
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "Error opening pid file %s",
		    IRCD_PIDFILE));
#endif
#endif
}

/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static int check_init(aClient *cptr, char *sockn, size_t size)
{
	struct SOCKADDR_IN sk;
	int  len = sizeof(struct SOCKADDR_IN);

	if (IsCGIIRC(cptr))
	{
		strlcpy(sockn, GetIP(cptr), size); /* use already set value */
		return 0;
	}

	/* If descriptor is a tty, special checking... */
#if defined(DEBUGMODE) && !defined(_WIN32)
	if (isatty(cptr->fd))
#else
	if (0)
#endif
	{
		strncpyzt(sockn, me.sockhost, HOSTLEN);
		bzero((char *)&sk, sizeof(struct SOCKADDR_IN));
	}
	else if (getpeername(cptr->fd, (struct SOCKADDR *)&sk, &len) == -1)
	{
		/* On Linux 2.4 and FreeBSD the socket may just have been disconnected
		 * so it's not a serious error and can happen quite frequently -- Syzop
		 */
		if (ERRNO != P_ENOTCONN)
			report_error("connect failure: %s %s", cptr);
		return -1;
	}
	(void)strlcpy(sockn, (char *)Inet_si2p(&sk), size);

#ifdef INET6
	if (IN6_IS_ADDR_LOOPBACK(&sk.SIN_ADDR) || !strcmp(sockn, "127.0.0.1"))
#else
	if (inet_netof(sk.SIN_ADDR) == IN_LOOPBACKNET)
#endif
	{
		if (cptr->hostp)
		{
			unreal_free_hostent(cptr->hostp);
			cptr->hostp = NULL;
		}
		strncpyzt(sockn, "localhost", HOSTLEN);
	}
	bcopy((char *)&sk.SIN_ADDR, (char *)&cptr->ip, sizeof(struct IN_ADDR));

	cptr->port = (int)ntohs(sk.SIN_PORT);

	return 0;
}

/*
 * Ordinary client access check. Look for conf lines which have the same
 * status as the flags passed.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int  check_client(aClient *cptr, char *username)
{
	static char sockname[HOSTLEN + 1];
	struct hostent *hp = NULL;
	int  i;
	
	ClearAccess(cptr);
	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
	    cptr->name, inetntoa((char *)&cptr->ip)));

	if (check_init(cptr, sockname, sizeof(sockname)))
		return -2;

	hp = cptr->hostp;
	/*
	 * Verify that the host to ip mapping is correct both ways and that
	 * the ip#(s) for the socket is listed for the host.
	 */
	if (hp)
	{
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!bcmp(hp->h_addr_list[i], (char *)&cptr->ip,
			    sizeof(struct IN_ADDR)))
				break;
		if (!hp->h_addr_list[i])
		{
			sendto_snomask(SNO_JUNK, "IP# Mismatch: %s != %s[%08lx]",
			    Inet_ia2p((struct IN_ADDR *)&cptr->ip), hp->h_name,
			    *((unsigned long *)hp->h_addr));
			hp = NULL;
		}
	}

	if ((i = AllowClient(cptr, hp, sockname, username)))
	{
		return i;
	}

	Debug((DEBUG_DNS, "ch_cl: access ok: %s[%s]", cptr->name, sockname));

#ifdef INET6
	if (IN6_IS_ADDR_LOOPBACK(&cptr->ip) ||
	    (cptr->ip.s6_addr[0] == mysk.sin6_addr.s6_addr[0] &&
	    cptr->ip.s6_addr[1] == mysk.sin6_addr.s6_addr[1])
/* ||
           IN6_ARE_ADDR_SAMEPREFIX(&cptr->ip, &mysk.SIN_ADDR))
 about the same, I think              NOT */
	    )
#else
	if (inet_netof(cptr->ip) == IN_LOOPBACKNET ||
	    inet_netof(cptr->ip) == inet_netof(mysk.SIN_ADDR))
#endif
	{
		ircstp->is_loc++;
		cptr->flags |= FLAGS_LOCAL;
	}
	return 0;
}

/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
int completed_connection(aClient *cptr)
{
	ConfigItem_link *aconf = cptr->serv ? cptr->serv->conf : NULL;
	extern char serveropts[];
	SetHandshake(cptr);

	if (!aconf)
	{
		sendto_ops("Lost configuration for %s", get_client_name(cptr, FALSE));
		return -1;
	}
	if (!BadPtr(aconf->connpwd))
		sendto_one(cptr, "PASS :%s", aconf->connpwd);

	send_proto(cptr, aconf);
	sendto_one(cptr, "SERVER %s 1 :U%d-%s%s-%i %s",
	    me.name, UnrealProtocol, serveropts, extraflags ? extraflags : "", me.serv->numeric,
	    me.info);
	if (!IsDead(cptr))
		start_auth(cptr);

	return (IsDead(cptr)) ? -1 : 0;
}

/*
** close_connection
**	Close the physical connection. This function must make
**	MyConnect(cptr) == FALSE, and set cptr->from == NULL.
*/
void close_connection(aClient *cptr)
{
	ConfigItem_link *aconf;
#ifdef DO_REMAPPING
	int  i, j;
	int  empty = cptr->fd;
#endif

	if (IsServer(cptr))
	{
		ircstp->is_sv++;
		ircstp->is_sbs += cptr->sendB;
		ircstp->is_sbr += cptr->receiveB;
		ircstp->is_sks += cptr->sendK;
		ircstp->is_skr += cptr->receiveK;
		ircstp->is_sti += TStime() - cptr->firsttime;
		if (ircstp->is_sbs > 1023)
		{
			ircstp->is_sks += (ircstp->is_sbs >> 10);
			ircstp->is_sbs &= 0x3ff;
		}
		if (ircstp->is_sbr > 1023)
		{
			ircstp->is_skr += (ircstp->is_sbr >> 10);
			ircstp->is_sbr &= 0x3ff;
		}
	}
	else if (IsClient(cptr))
	{
		ircstp->is_cl++;
		ircstp->is_cbs += cptr->sendB;
		ircstp->is_cbr += cptr->receiveB;
		ircstp->is_cks += cptr->sendK;
		ircstp->is_ckr += cptr->receiveK;
		ircstp->is_cti += TStime() - cptr->firsttime;
		if (ircstp->is_cbs > 1023)
		{
			ircstp->is_cks += (ircstp->is_cbs >> 10);
			ircstp->is_cbs &= 0x3ff;
		}
		if (ircstp->is_cbr > 1023)
		{
			ircstp->is_ckr += (ircstp->is_cbr >> 10);
			ircstp->is_cbr &= 0x3ff;
		}
	}
	else
		ircstp->is_ni++;

	/*
	 * remove outstanding DNS queries.
	 */
	unrealdns_delreq_bycptr(cptr);
	/*
	 * If the connection has been up for a long amount of time, schedule
	 * a 'quick' reconnect, else reset the next-connect cycle.
	 *
	 * Now just hold on a minute.  We're currently doing this when a
	 * CLIENT exits too?  I don't think so!  If its not a server, or
	 * the SQUIT flag has been set, then we don't schedule a fast
	 * reconnect.  Pisses off too many opers. :-)  -Cabal95
	 */
	if (IsServer(cptr) && !(cptr->flags & FLAGS_SQUIT) && cptr->serv->conf &&
	    (!cptr->serv->conf->flag.temporary &&
	      (cptr->serv->conf->options & CONNECT_AUTO)))
	{
		aconf = cptr->serv->conf;
		/*
		 * Reschedule a faster reconnect, if this was a automaticly
		 * connected configuration entry. (Note that if we have had
		 * a rehash in between, the status has been changed to
		 * CONF_ILLEGAL). But only do this if it was a "good" link.
		 */
		aconf->hold = TStime();
		aconf->hold += (aconf->hold - cptr->since > HANGONGOODLINK) ?
		    HANGONRETRYDELAY : aconf->class->connfreq;
		if (nextconnect > aconf->hold)
			nextconnect = aconf->hold;
	}

	if (cptr->authfd >= 0)
	{
		CLOSE_SOCK(cptr->authfd);
		cptr->authfd = -1;
		--OpenFiles;
	}

	if (cptr->fd >= 0)
	{
		flush_connections(cptr);
		remove_local_client(cptr);
#ifdef USE_SSL
		if (IsSSL(cptr) && cptr->ssl) {
			SSL_set_shutdown((SSL *)cptr->ssl, SSL_RECEIVED_SHUTDOWN);
			SSL_smart_shutdown((SSL *)cptr->ssl);
			SSL_free((SSL *)cptr->ssl);
			cptr->ssl = NULL;
		}
#endif
		CLOSE_SOCK(cptr->fd);
		cptr->fd = -2;
		--OpenFiles;
		DBufClear(&cptr->sendQ);
		DBufClear(&cptr->recvQ);

	}

	cptr->from = NULL;	/* ...this should catch them! >:) --msa */
	/*
	 * fd remap to keep local[i] filled at the bottom.
	 */
#if 0
#ifdef DO_REMAPPING
	if (empty > 0)
		if ((j = LastSlot) > (i = empty) &&
		    (local[j]->status != STAT_LOG))
		{
			if (dup2(j, i) == -1)
				return;
			local[i] = local[j];
			local[i]->fd = i;
#ifdef USE_SSL
			/* I didn't know the code above existed, which
			   fucked up SSL -Stskeeps
			*/
			if ((local[i]->flags & FLAGS_SSL) && local[i]->ssl)
			{
				/* !! RISKY !! --Stskeeps */
				SSL_change_fd((SSL *) local[i]->ssl, local[i]->fd);
			}
#endif
			local[j] = NULL;
#ifndef NO_FDLIST
			/* update server list */
			if (IsServer(local[i]))
			{
				delfrom_fdlist(j, &busycli_fdlist);
				delfrom_fdlist(j, &serv_fdlist);
				addto_fdlist(i, &busycli_fdlist);
				addto_fdlist(i, &serv_fdlist);
			}
			if (IsAnOper(local[i]))
			{
				delfrom_fdlist(j, &busycli_fdlist);
				delfrom_fdlist(j, &oper_fdlist);
				addto_fdlist(i, &busycli_fdlist);
				addto_fdlist(i, &oper_fdlist);
			}
#endif
			CLOSE_SOCK(j);
			--OpenFiles;
		}
#endif
#endif
	return;
}

/*
** set_sock_opts
*/
void set_sock_opts(int fd, aClient *cptr)
{
	int  opt;
#ifdef SO_REUSEADDR
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (OPT_TYPE *)&opt,
	    sizeof(opt)) < 0)
			report_error("setsockopt(SO_REUSEADDR) %s:%s", cptr);
#endif
#if  defined(SO_DEBUG) && defined(DEBUGMODE) && 0
/* Solaris with SO_DEBUG writes to syslog by default */
#if !defined(_SOLARIS) || defined(HAVE_SYSLOG)
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_DEBUG, (OPT_TYPE *)&opt,
	    sizeof(opt)) < 0)
		
		report_error("setsockopt(SO_DEBUG) %s:%s", cptr);
#endif /* _SOLARIS */
#endif
#if defined(SO_USELOOPBACK) && !defined(_WIN32)
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_USELOOPBACK, (OPT_TYPE *)&opt,
	    sizeof(opt)) < 0)
		report_error("setsockopt(SO_USELOOPBACK) %s:%s", cptr);
#endif
#ifdef	SO_RCVBUF
	opt = 8192;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (OPT_TYPE *)&opt,
	    sizeof(opt)) < 0)
		report_error("setsockopt(SO_RCVBUF) %s:%s", cptr);
#endif
#ifdef	SO_SNDBUF
# ifdef	_SEQUENT_
/* seems that Sequent freezes up if the receving buffer is a different size
 * to the sending buffer (maybe a tcp window problem too).
 */
	opt = 8192;
# else
	opt = 8192;
# endif
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (OPT_TYPE *)&opt,
	    sizeof(opt)) < 0)
		report_error("setsockopt(SO_SNDBUF) %s:%s", cptr);
#endif
#if defined(IP_OPTIONS) && defined(IPPROTO_IP) && !defined(_WIN32) && !defined(INET6)
	{
		char *s = readbuf, *t = readbuf + sizeof(readbuf) / 2;

		opt = sizeof(readbuf) / 8;
		if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)t, &opt) < 0)
		{
		    if (ERRNO != P_ECONNRESET) /* FreeBSD can generate this -- Syzop */
		        report_error("getsockopt(IP_OPTIONS) %s:%s", cptr);
		}
		else if (opt > 0 && opt != sizeof(readbuf) / 8)
		{
			for (*readbuf = '\0'; opt > 0; opt--, s += 3)
				(void)ircsprintf(s, "%2.2x:", *t++);
			*s = '\0';
			sendto_realops("Connection %s using IP opts: (%s)",
			    get_client_name(cptr, TRUE), readbuf);
		}
		if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)NULL,
		    0) < 0)
		    if (ERRNO != P_ECONNRESET) /* FreeBSD can generate this -- Syzop */
			    report_error("setsockopt(IP_OPTIONS) %s:%s", cptr);
	}
#endif
}


int  get_sockerr(aClient *cptr)
{
#ifndef _WIN32
	int  errtmp = errno, err = 0, len = sizeof(err);
#else
	int  errtmp = WSAGetLastError(), err = 0, len = sizeof(err);
#endif
#ifdef	SO_ERROR
	if (cptr->fd >= 0)
		if (!getsockopt(cptr->fd, SOL_SOCKET, SO_ERROR,
		    (OPT_TYPE *)&err, &len))
			if (err)
				errtmp = err;
#endif
	return errtmp;
}

/*
 * set_blocking - Set the client connection into non-blocking mode.
 * If your system doesn't support this, you're screwed, ircd will run like
 * crap.
 * returns true (1) if successful, false (0) otherwise
 */
int set_blocking(int fd)
{
   int flags;
#ifdef _WIN32
   int nonb;
#endif

#ifndef _WIN32
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0
        || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) < 0)
#else
    nonb = 0;
    if (ioctlsocket(fd, FIONBIO, &nonb) < 0)
#endif
       return 0;
    else
        return 1;
}



/*
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
void set_non_blocking(int fd, aClient *cptr)
{
	int  res, nonb = 0;

	/*
	   ** NOTE: consult ALL your relevant manual pages *BEFORE* changing
	   **    these ioctl's.  There are quite a few variations on them,
	   **    as can be seen by the PCS one.  They are *NOT* all the same.
	   **    Heed this well. - Avalon.
	 */
#ifdef	NBLOCK_POSIX
	nonb |= O_NONBLOCK;
#endif
#ifdef	NBLOCK_BSD
	nonb |= O_NDELAY;
#endif
#ifdef	NBLOCK_SYSV
	/* This portion of code might also apply to NeXT.  -LynX */
	res = 1;

	if (ioctl(fd, FIONBIO, &res) < 0)
	{
		if (cptr) 
			report_error("ioctl(fd,FIONBIO) failed for %s:%s", cptr);
		
	}
#else
# if !defined(_WIN32)
	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
	{
		if (cptr)
		{
			report_error("fcntl(fd, F_GETFL) failed for %s:%s", cptr);
		}
	}
	else if (fcntl(fd, F_SETFL, res | nonb) == -1)
	{
		if (cptr)
		{
			report_error("fcntl(fd, F_SETL, nonb) failed for %s:%s", cptr);
		}
	}
# else
	nonb = 1;
	if (ioctlsocket(fd, FIONBIO, &nonb) < 0)
	{
		if (cptr)
		{
			report_error("ioctlsocket(fd,FIONBIO) failed for %s:%s", cptr);
		}
	}
# endif
#endif
	return;
}

/*
 * Creates a client which has just connected to us on the given fd.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yuet since it doesnt have a name.
 */
aClient *add_connection(aClient *cptr, int fd)
{
	aClient *acptr;
	ConfigItem_ban *bconf;
	int i, j;
	acptr = make_client(NULL, &me);

	/* Removed preliminary access check. Full check is performed in
	 * m_server and m_user instead. Also connection time out help to
	 * get rid of unwanted connections.
	 */
#if defined(DEBUGMODE) && !defined(_WIN32)
	if (isatty(fd))		/* If descriptor is a tty, special checking... */
#else
	if (0)
#endif
		get_sockhost(acptr, cptr->sockhost);
	else
	{
		struct SOCKADDR_IN addr;
		int  len = sizeof(struct SOCKADDR_IN);

		if (getpeername(fd, (struct SOCKADDR *)&addr, &len) == -1)
		{
			/* On Linux 2.4 and FreeBSD the socket may just have been disconnected
			 * so it's not a serious error and can happen quite frequently -- Syzop
			 */
			if (ERRNO != P_ENOTCONN)
				report_error("Failed in connecting to %s :%s", cptr);
add_con_refuse:
			ircstp->is_ref++;
			acptr->fd = -2;
			free_client(acptr);
			CLOSE_SOCK(fd);
			--OpenFiles;
			return NULL;
		}
		/* don't want to add "Failed in connecting to" here.. */
		/* Copy ascii address to 'sockhost' just in case. Then we
		 * have something valid to put into error messages...
		 */
		get_sockhost(acptr, Inet_si2p(&addr));
		bcopy((char *)&addr.SIN_ADDR, (char *)&acptr->ip, sizeof(struct IN_ADDR));
		j = 1;
		for (i = LastSlot; i >= 0; i--)
		{
			if (local[i] && IsUnknown(local[i]) &&
#ifndef INET6
				local[i]->ip.S_ADDR == acptr->ip.S_ADDR)
#else
				!bcmp(local[i]->ip.S_ADDR, cptr->ip.S_ADDR, sizeof(cptr->ip.S_ADDR)))
#endif
			{
				j++;
				if (j > MAXUNKNOWNCONNECTIONSPERIP)
				{
					ircsprintf(zlinebuf,
						"ERROR :Closing Link: [%s] (Too many unknown connections from your IP)"
						"\r\n",
						Inet_ia2p(&acptr->ip));
					set_non_blocking(fd, acptr);
					set_sock_opts(fd, acptr);
					send(fd, zlinebuf, strlen(zlinebuf), 0);
					goto add_con_refuse;
				}
			}
		}

		if ((bconf = Find_ban(acptr, Inet_ia2p(&acptr->ip), CONF_BAN_IP))) {
			if (bconf)
			{
				ircsprintf(zlinebuf,
					"ERROR :Closing Link: [%s] (You are not welcome on "
					"this server: %s. Email %s for more information.)\r\n",
					Inet_ia2p(&acptr->ip),
					bconf->reason ? bconf->reason : "no reason",
					KLINE_ADDRESS);
				set_non_blocking(fd, acptr);
				set_sock_opts(fd, acptr);
				send(fd, zlinebuf, strlen(zlinebuf), 0);
				goto add_con_refuse;
			}
		}
		else if (find_tkline_match_zap(acptr) != -1)
		{
			set_non_blocking(fd, acptr);
			set_sock_opts(fd, acptr);
			send(fd, zlinebuf, strlen(zlinebuf), 0);
			goto add_con_refuse;
		}
#ifdef THROTTLING
		else
		{
			int val;
			if (!(val = throttle_can_connect(acptr, &acptr->ip)))
			{
				ircsprintf(zlinebuf,
					"ERROR :Closing Link: [%s] (Throttled: Reconnecting too fast) -"
						"Email %s for more information.\r\n",
						Inet_ia2p(&acptr->ip),
						KLINE_ADDRESS);
				set_non_blocking(fd, acptr);
				set_sock_opts(fd, acptr);
				send(fd, zlinebuf, strlen(zlinebuf), 0);
				goto add_con_refuse;
			}
			else if (val == 1)
				add_throttling_bucket(&acptr->ip);
		}
#endif
		acptr->port = ntohs(addr.SIN_PORT);
	}

	acptr->fd = fd;
    add_local_client(acptr);
	acptr->listener = cptr;
	if (!acptr->listener->class)
	{
		sendto_ops("ERROR: !acptr->listener->class");
	}
	else
	{
		((ConfigItem_listen *) acptr->listener->class)->clients++;
	}
	add_client_to_list(acptr);
	set_non_blocking(acptr->fd, acptr);
	set_sock_opts(acptr->fd, acptr);
	IRCstats.unknown++;
#ifdef USE_SSL
	if (cptr->umodes & LISTENER_SSL)
	{
		SetSSLAcceptHandshake(acptr);
		Debug((DEBUG_DEBUG, "Starting SSL accept handshake for %s", acptr->sockhost));
		if ((acptr->ssl = SSL_new(ctx_server)) == NULL)
		{
			goto add_con_refuse;
		}
		acptr->flags |= FLAGS_SSL;
		SSL_set_fd(acptr->ssl, fd);
		SSL_set_nonblocking(acptr->ssl);
		if (!ircd_SSL_accept(acptr, fd)) {
			Debug((DEBUG_DEBUG, "Failed SSL accept handshake in instance 1: %s", acptr->sockhost));
			SSL_set_shutdown(acptr->ssl, SSL_RECEIVED_SHUTDOWN);
			SSL_smart_shutdown(acptr->ssl);
  	                SSL_free(acptr->ssl);
	  	        goto add_con_refuse;
	  	}
	}
	else
#endif
		start_of_normal_client_handshake(acptr);
	return acptr;
}

static int dns_special_flag = 0; /* This is for an "interesting" race condition / fuck up issue.. very ugly. */

void	start_of_normal_client_handshake(aClient *acptr)
{
struct hostent *he;

	acptr->status = STAT_UNKNOWN;

	if (!DONT_RESOLVE)
	{
		if (SHOWCONNECTINFO && !acptr->serv)
			sendto_one(acptr, "%s", REPORT_DO_DNS);
		dns_special_flag = 1;
		he = unrealdns_doclient(acptr);
		dns_special_flag = 0;

		if (acptr->hostp)
			goto doauth; /* Race condition detected, DNS has been done, continue with auth */

		if (!he)
		{
			/* Resolving in progress */
			SetDNS(acptr);
		} else {
			/* Host was in our cache */
			acptr->hostp = he;
			if (SHOWCONNECTINFO && !acptr->serv)
				sendto_one(acptr, "%s", REPORT_FIN_DNSC);
		}
	}

doauth:
	start_auth(acptr);
}

void proceed_normal_client_handshake(aClient *acptr, struct hostent *he)
{
	ClearDNS(acptr);
	acptr->hostp = he;
	if (SHOWCONNECTINFO && !acptr->serv)
		sendto_one(acptr, "%s", acptr->hostp ? REPORT_FIN_DNS : REPORT_FAIL_DNS);
	
	if (!dns_special_flag && !DoingAuth(acptr))
		SetAccess(acptr);
}

/*
** read_packet
**
** Read a 'packet' of data from a connection and process it.  Read in 8k
** chunks to give a better performance rating (for server connections).
** Do some tricky stuff for client connections to make sure they don't do
** any flooding >:-) -avalon
*/

#ifndef USE_POLL
static int read_packet(aClient *cptr, fd_set *rfd)
{
	int  dolen = 0, length = 0, done;
	time_t now = TStime();
	if (FD_ISSET(cptr->fd, rfd) &&
	    !(IsPerson(cptr) && DBufLength(&cptr->recvQ) > 6090))
	{
		Hook *h;
		SET_ERRNO(0);
#ifdef USE_SSL
		if (cptr->flags & FLAGS_SSL)
	    		length = ircd_SSL_read(cptr, readbuf, sizeof(readbuf));
		else
#endif
			length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);
		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);
		/*
		 * If not ready, fake it so it isnt closed
		 */
		if (length < 0 && ERRNO == P_EWOULDBLOCK)
		    return 1;
		if (length <= 0)
			return length;
		for (h = Hooks[HOOKTYPE_RAWPACKET_IN]; h; h = h->next)
		{
			int v = (*(h->func.intfunc))(cptr, readbuf, length);
			if (v <= 0)
				return v;
		}
	}
	/*
	   ** For server connections, we process as many as we can without
	   ** worrying about the time of day or anything :)
	 */
	if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
	{
		if (length > 0)
			if ((done = dopacket(cptr, readbuf, length)))
				return done;
	}
	else
	{
		/*
		   ** Before we even think of parsing what we just read, stick
		   ** it on the end of the receive queue and do it when its
		   ** turn comes around.
		 */
		if (!dbuf_put(&cptr->recvQ, readbuf, length))
			return exit_client(cptr, cptr, cptr, "dbuf_put fail");

		if (IsPerson(cptr) && DBufLength(&cptr->recvQ) > get_recvq(cptr))
		{
			sendto_snomask(SNO_FLOOD,
			    "*** Flood -- %s!%s@%s (%d) exceeds %d recvQ",
			    cptr->name[0] ? cptr->name : "*",
			    cptr->user ? cptr->user->username : "*",
			    cptr->user ? cptr->user->realhost : "*",
			    DBufLength(&cptr->recvQ), get_recvq(cptr));
			return exit_client(cptr, cptr, cptr, "Excess Flood");
		}

		while (DBufLength(&cptr->recvQ) && !NoNewLine(cptr) &&
		    ((cptr->status < STAT_UNKNOWN) || (cptr->since - now < 10)))
		{
			/*
			   ** If it has become registered as a Service or Server
			   ** then skip the per-message parsing below.
			 */
			if (IsServer(cptr))
			{
				dolen = dbuf_get(&cptr->recvQ, readbuf,
				    sizeof(readbuf));
				if (dolen <= 0)
					break;
				if ((done = dopacket(cptr, readbuf, dolen)))
					return done;
				break;
			}
			dolen = dbuf_getmsg(&cptr->recvQ, readbuf,
			    sizeof(readbuf));
			/*
			   ** Devious looking...whats it do ? well..if a client
			   ** sends a *long* message without any CR or LF, then
			   ** dbuf_getmsg fails and we pull it out using this
			   ** loop which just gets the next 512 bytes and then
			   ** deletes the rest of the buffer contents.
			   ** -avalon
			 */
			while (dolen <= 0)
			{
				if (dolen < 0)
					return exit_client(cptr, cptr, cptr,
					    "dbuf_getmsg fail");
				if (DBufLength(&cptr->recvQ) < 510)
				{
					cptr->flags |= FLAGS_NONL;
					break;
				}
				dolen = dbuf_get(&cptr->recvQ, readbuf, 511);
				if (dolen > 0 && DBufLength(&cptr->recvQ))
					DBufClear(&cptr->recvQ);
			}

			if (dolen > 0 &&
			    (dopacket(cptr, readbuf, dolen) == FLUSH_BUFFER))
				return FLUSH_BUFFER;
		}
	}
	return 1;
}
#else
/* handle taking care of the client's recvq here */
static int do_client_queue(aClient *cptr)
{
	int  dolen = 0, done;

	while (DBufLength(&cptr->recvQ) && !NoNewLine(cptr) &&
	    ((cptr->status < STAT_UNKNOWN) || (cptr->since - now < 10)))
	{
		/* If it's become registered as a server, just parse the whole block */
		if (IsServer(cptr))
		{
			dolen =
			    dbuf_get(&cptr->recvQ, readbuf, sizeof(readbuf));
			if (dolen <= 0)
				break;
			if ((done = dopacket(cptr, readbuf, dolen)))
				return done;
			break;
		}

#if defined(MAXBUFFERS)
		dolen =
		    dbuf_getmsg(&cptr->recvQ, readbuf,
		    rcvbufmax * sizeof(char));
#else
		dolen = dbuf_getmsg(&cptr->recvQ, readbuf, sizeof(readbuf));
#endif

		if (dolen <= 0)
		{
			if (dolen < 0)
				return exit_client(cptr, cptr, cptr,
				    "dbuf_getmsg fail");

			if (DBufLength(&cptr->recvQ) < 510)
			{
				cptr->flags |= FLAGS_NONL;
				break;
			}
			/* The buffer is full (more than 512 bytes) and it has no \n
			 * Some user is trying to trick us. Kill their recvq. */
			DBufClear(&cptr->recvQ);
			break;
		}
		else if (dopacket(cptr, readbuf, dolen) == FLUSH_BUFFER)
			return FLUSH_BUFFER;
	}
	return 1;
}

#define MAX_CLIENT_RECVQ 8192	/* 4 dbufs */

static int read_packet(aClient *cptr)
{
	int  length = 0, done;

	/* If data is ready, and the user is either not a person or
	 * is a person and has a recvq of less than MAX_CLIENT_RECVQ,
	 * read from this client
	 */
	if (!(IsPerson(cptr) && DBufLength(&cptr->recvQ) > MAX_CLIENT_RECVQ))
	{
		errno = 0;

#ifdef USE_SSL
		if (cptr->flags & FLAGS_SSL)
	    		length = ircd_SSL_read((SSL *)cptr->ssl, readbuf, sizeof(readbuf));
		else
#endif
			length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);
		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);
		/*
		 * If not ready, fake it so it isnt closed
		 */
	        if (length < 0 && ((ERRNO == P_EWOULDBLOCK) || ERRNO == P_EAGAIN)))
			return 1;
		if (length <= 0)
			return length;
	}

	/*
	 * For server connections, we process as many as we can without
	 * worrying about the time of day or anything :)
	 */
	if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr))
	{
		if (length > 0)
			if ((done = dopacket(cptr, readbuf, length)))
				return done;
	}
	else
	{
		/*
		 * Before we even think of parsing what we just read, stick
		 * it on the end of the receive queue and do it when its turn
		 * comes around. */
		if (!dbuf_put(&cptr->recvQ, readbuf, length))
			return exit_client(cptr, cptr, cptr, "dbuf_put fail");

		if (IsPerson(cptr) &&
#ifdef NO_OPER_FLOOD
		    !IsAnOper(cptr) &&
#endif
		    DBufLength(&cptr->recvQ) > get_recvq(cptr))
		{
			sendto_snomask(SNO_FLOOD,
			    "Flood -- %s!%s@%s (%d) Exceeds %d RecvQ",
			    cptr->name[0] ? cptr->name : "*",
			    cptr->user ? cptr->user->username : "*",
			    cptr->user ? cptr->user->realhost : "*",
			    DBufLength(&cptr->recvQ), get_recvq(cptr));
			return exit_client(cptr, cptr, cptr, "Excess Flood");
		}
		return do_client_queue(cptr);
	}
	return 1;
}


#endif


/*
 * Check all connections for new connections and input data that is to be
 * processed. Also check for connections with data queued and whether we can
 * write it out.
 */

#ifndef USE_POLL
#ifdef NO_FDLIST
int  read_message(time_t delay)
#else
int  read_message(time_t delay, fdlist *listp)
#endif
{
/* 
   #undef FD_SET(x,y) do { if (fcntl(x, F_GETFD, &sockerr) == -1) abort(); FD_SET(x,y); } while(0)
*/	aClient *cptr;
	int  nfds;
	struct timeval wait;
#ifndef _WIN32
	fd_set read_set, write_set;
#else
	fd_set read_set, write_set, excpt_set;
#endif
	int  j,k;
	time_t delay2 = delay, now;
	int  res, length, fd, i;
	int  auth = 0;

	int  sockerr;

#ifndef NO_FDLIST
	/* if it is called with NULL we check all active fd's */
	if (!listp)
	{
		listp = &default_fdlist;
		listp->last_entry = LastSlot == -1 ? LastSlot : LastSlot + 1;
	}
#endif

	now = TStime();
	for (res = 0;;)
	{
		FD_ZERO(&read_set);
		FD_ZERO(&write_set);
#ifdef _WIN32
		FD_ZERO(&excpt_set);
#endif
#ifdef USE_LIBCURL
		url_do_transfers_async();
#endif
#ifdef NO_FDLIST
		for (i = LastSlot; i >= 0; i--)
#else
		for (i = listp->entry[j = 1]; j <= listp->last_entry; i = listp->entry[++j])
#endif
		{
			if (!(cptr = local[i]))
				continue;
			if (IsLog(cptr))
				continue;
			
			if (DoingAuth(cptr))
			{
				int s = TStime() - cptr->firsttime;
				/* Maybe they should be timed out. -- Syzop. */
				if ( ((s > IDENT_CONNECT_TIMEOUT) && (cptr->flags & FLAGS_WRAUTH)) ||
				     (s > IDENT_READ_TIMEOUT))
				{
					Debug((DEBUG_NOTICE, "ident timed out (cptr %x, %d sec)", cptr, s));
					ident_failed(cptr);
				}
				else
				{
					auth++;
					Debug((DEBUG_NOTICE, "auth on %x %d %d", cptr, i, s));
					if (cptr->authfd >= 0)
					{
						FD_SET(cptr->authfd, &read_set);
#ifdef _WIN32	
						FD_SET(cptr->authfd, &excpt_set);
#endif	
						if (cptr->flags & FLAGS_WRAUTH)
							FD_SET(cptr->authfd, &write_set);
					}
				}
			}
			/* (warning: don't merge the DoingAuth() here with the check
			 *  above coz ident_failed() might have been called -- Syzop.)
			 */
			if (DoingDNS(cptr) || DoingAuth(cptr))
				continue;
			if (IsMe(cptr) && IsListening(cptr))
			{
				if (cptr->fd >= 0)
					FD_SET(cptr->fd, &read_set);
			}
			else if (!IsMe(cptr))
			{
				if (DBufLength(&cptr->recvQ) && delay2 > 2)
					delay2 = 1;
				if ((cptr->fd >= 0) && (DBufLength(&cptr->recvQ) < 4088))
					FD_SET(cptr->fd, &read_set);
			}
			if ((cptr->fd >= 0) && (DBufLength(&cptr->sendQ) || IsConnecting(cptr) ||
			    (DoList(cptr) && IsSendable(cptr))
#ifdef ZIP_LINKS
				|| ((IsZipped(cptr)) && (cptr->zip->outcount > 0))
#endif
			    ))
			{
				FD_SET(cptr->fd, &write_set);
			}
		}

		ares_fds(resolver_channel, &read_set, &write_set);
		
		if (me.fd >= 0)
			FD_SET(me.fd, &read_set);

		wait.tv_sec = MIN(delay, delay2);
		wait.tv_usec = 0;
#ifdef	HPUX
		nfds = select(MAXCONNECTIONS, (int *)&read_set, (int *)&write_set,
		    0, &wait);
#else
# ifndef _WIN32
		nfds = select(MAXCONNECTIONS, &read_set, &write_set, 0, &wait);
# else
		nfds = select(MAXCONNECTIONS, &read_set, &write_set, &excpt_set, &wait);
# endif
#endif
	    if (nfds == -1 && ((ERRNO == P_EINTR) || (ERRNO == P_ENOTSOCK)))
			return -1;
		else if (nfds >= 0)
			break;
		report_baderror("select %s:%s", &me);
		res++;
		if (res > 5)
			restart("too many select errors");
#ifndef _WIN32
		sleep(10);
#else
		Sleep(10000);
#endif
	}

	Debug((DEBUG_DNS, "Doing DNS async.."));
	ares_process(resolver_channel, &read_set, &write_set);

	/*
	 * Check fd sets for the auth fd's (if set and valid!) first
	 * because these can not be processed using the normal loops below.
	 * -avalon
	 */
#ifdef NO_FDLIST
	for (i = LastSlot; (auth > 0) && (i >= 0); i--)
#else
	for (i = listp->entry[j = 1]; j <= listp->last_entry; i = listp->entry[++j])
#endif
	{
		if (!(cptr = local[i]))
			continue;
		if (cptr->authfd < 0)
			continue;
		auth--;
#ifdef _WIN32
		/*
		 * Because of the way windows uses select(), we have to use
		 * the exception FD set to find out when a connection is
		 * refused.  ie Auth ports and /connect's.  -Cabal95
		 */
		if (FD_ISSET(cptr->authfd, &excpt_set))
		{
			int  err, len = sizeof(err);

			if (getsockopt(cptr->authfd, SOL_SOCKET, SO_ERROR,
			    (OPT_TYPE *)&err, &len) || err)
			{
				ircstp->is_abad++;
				closesocket(cptr->authfd);
				cptr->authfd = -1;
				--OpenFiles;
				cptr->flags &= ~(FLAGS_AUTH | FLAGS_WRAUTH);
				if (!DoingDNS(cptr))
					SetAccess(cptr);
				if (nfds > 0)
					nfds--;
				continue;
			}
		}
#endif
		if (nfds > 0)
		{
			if (FD_ISSET(cptr->authfd, &read_set) ||
				FD_ISSET(cptr->authfd, &write_set))
				nfds--;
			if ((cptr->authfd > 0) && FD_ISSET(cptr->authfd, &write_set))
			{
				send_authports(cptr);
			}
			if ((cptr->authfd > 0) && FD_ISSET(cptr->authfd, &read_set))
			{
				read_authports(cptr);
			}
		}
	}

#ifdef NO_FDLIST
	for (i = LastSlot; i >= 0; i--)
#else
	for (i = listp->entry[j = 1];  (j <= listp->last_entry); i = listp->entry[++j])
#endif
		if ((cptr = local[i]) && FD_ISSET(cptr->fd, &read_set) &&
		    IsListening(cptr))
		{
			FD_CLR(cptr->fd, &read_set);
			nfds--;
			cptr->lasttime = TStime();
			/*
			   ** There may be many reasons for error return, but
			   ** in otherwise correctly working environment the
			   ** probable cause is running out of file descriptors
			   ** (EMFILE, ENFILE or others?). The man pages for
			   ** accept don't seem to list these as possible,
			   ** although it's obvious that it may happen here.
			   ** Thus no specific errors are tested at this
			   ** point, just assume that connections cannot
			   ** be accepted until some old is closed first.
			 *
			 */
			for (k = 0; k < LISTEN_SIZE; k++)
{			{
			if ((fd = accept(cptr->fd, NULL, NULL)) < 0)
			{
		        if ((ERRNO != P_EWOULDBLOCK) && (ERRNO != P_ECONNABORTED))
					report_baderror("Cannot accept connections %s:%s", cptr);
				break;
			}
			ircstp->is_ac++;
			if (++OpenFiles >= MAXCLIENTS)
			{
				ircstp->is_ref++;
				if (last_allinuse < TStime() - 15)
				{
					sendto_realops("All connections in use. (%s)",
					    get_client_name(cptr, TRUE));
					last_allinuse = TStime();
				}
#ifndef INET6
				(void)send(fd,
				    "ERROR :All connections in use\r\n", 31, 0);
#else
				(void)sendto(fd,
				    "ERROR :All connections in use\r\n",
				    31, 0, 0, 0);
#endif
				CLOSE_SOCK(fd);
				--OpenFiles;
				break;
                          }
			  /*
			  * Use of add_connection (which never fails :) meLazy
			  */
			  (void)add_connection(cptr, fd);
			  }
			 }
			   nextping = TStime();
			   if (!cptr->listener)
				cptr->listener = &me;
		        
                      }
#ifndef NO_FDLIST
	for (i = listp->entry[j = 1];  (j <= listp->last_entry); i = listp->entry[++j])
#else
	for (i = LastSlot; i >= 0; i--)
#endif
	{
		if (!(cptr = local[i]) || IsMe(cptr))
			continue;

		if (FD_ISSET(cptr->fd, &write_set))
		{
			int  write_err = 0;
			/*
			   ** ...room for writing, empty some queue then...
			 */
			ClearBlocked(cptr);
			if (IsConnecting(cptr)) {
#ifdef USE_SSL
				if ((cptr->serv) && (cptr->serv->conf->options & CONNECT_SSL))
				{
					Debug((DEBUG_DEBUG, "ircd_SSL_client_handshake(%s)", cptr->name));
					write_err = ircd_SSL_client_handshake(cptr);
				}
				else
#endif
					write_err = completed_connection(cptr);
			}
			if (!write_err)
			{
				if (DoList(cptr) && IsSendable(cptr))
					send_list(cptr, 32);
				(void)send_queued(cptr);
			}

			if (IsDead(cptr) || write_err)
			{
deadsocket:
				if (FD_ISSET(cptr->fd, &read_set))
				{
					nfds--;
					FD_CLR(cptr->fd, &read_set);
				}
				(void)exit_client(cptr, cptr, &me,
				    ((sockerr = get_sockerr(cptr))
				    ? STRERROR(sockerr) : "Client exited"));
				continue;
			}
		}
		length = 1;	/* for fall through case */
		/* Note: these DoingDNS/DoingAuth checks are here because of a
		 * filedescriptor race condition, so don't remove them without
		 * being sure that has been fixed. -- Syzop
		 */
		if ((!NoNewLine(cptr) || FD_ISSET(cptr->fd, &read_set)) &&
		    !(DoingDNS(cptr) || DoingAuth(cptr))
#ifdef USE_SSL
			&& 
			!(IsSSLAcceptHandshake(cptr) || IsSSLConnectHandshake(cptr))
#endif		
			)
			length = read_packet(cptr, &read_set);
#ifdef USE_SSL
		if ((length != FLUSH_BUFFER) && (cptr->ssl != NULL) && 
			(IsSSLAcceptHandshake(cptr) || IsSSLConnectHandshake(cptr)) &&
			FD_ISSET(cptr->fd, &read_set))
		{
			if (!SSL_is_init_finished(cptr->ssl))
			{
				if (IsDead(cptr) || IsSSLAcceptHandshake(cptr) ? !ircd_SSL_accept(cptr, cptr->fd) : ircd_SSL_connect(cptr) < 0)
				{
					length = -1;
				}
			}
			if (SSL_is_init_finished(cptr->ssl))
			{
				if (IsSSLAcceptHandshake(cptr))
				{
					Debug((DEBUG_ERROR, "ssl: start_of_normal_client_handshake(%s)", cptr->sockhost));
					start_of_normal_client_handshake(cptr);
				}
				else
				{
					Debug((DEBUG_ERROR, "ssl: completed_connection", cptr->name));
					completed_connection(cptr);
				}

			}
		}
#endif
		if (length > 0)
			flush_connections(cptr);
		if ((length != FLUSH_BUFFER) && IsDead(cptr))
			goto deadsocket;
		if ((length > 0) && (cptr->fd >= 0) && !FD_ISSET(cptr->fd, &read_set))
			continue;
		nfds--;
		readcalls++;
		if (length > 0)
			continue;

		/*
		   ** ...hmm, with non-blocking sockets we might get
		   ** here from quite valid reasons, although.. why
		   ** would select report "data available" when there
		   ** wasn't... so, this must be an error anyway...  --msa
		   ** actually, EOF occurs when read() returns 0 and
		   ** in due course, select() returns that fd as ready
		   ** for reading even though it ends up being an EOF. -avalon
		 */
		Debug((DEBUG_ERROR, "READ ERROR: fd=%d, errno=%d, length=%d",
			length == FLUSH_BUFFER ? -2 : cptr->fd, ERRNO, length));
		/*
		   ** NOTE: if length == -2 then cptr has already been freed!
		 */
		if (length != -2 && (IsServer(cptr) || IsHandshake(cptr)))
		{
			if (length == 0)
			{
				sendto_locfailops
				    ("Server %s closed the connection",
				    get_client_name(cptr, FALSE));
				sendto_serv_butone(&me,
				    ":%s GLOBOPS :Server %s closed the connection",
				    me.name, get_client_name(cptr, FALSE));
			}
			else
				report_baderror("Lost connection to %s:%s", cptr);
		}
		if (length != FLUSH_BUFFER)
			(void)exit_client(cptr, cptr, &me,
			    ((sockerr = get_sockerr(cptr))
			    ? STRERROR(sockerr) : "Client exited"));
	}
	return 0;
}
#else
/* USE_POLL */
# ifdef AIX
#  define POLLREADFLAGS (POLLIN|POLLMSG)
# endif
# if defined(POLLMSG) && defined(POLLIN) && defined(POLLRDNORM)
#  define POLLREADFLAGS (POLLMSG|POLLIN|POLLRDNORM)
# endif
# if defined(POLLIN) && defined(POLLRDNORM) && !defined(POLLMSG)
#  define POLLREADFLAGS (POLLIN|POLLRDNORM)
# endif
# if defined(POLLIN) && !defined(POLLRDNORM) && !defined(POLLMSG)
#  define POLLREADFLAGS POLLIN
# endif
# if defined(POLLRDNORM) && !defined(POLLIN) && !defined(POLLMSG)
#  define POLLREADFLAGS POLLRDNORM
# endif

# if defined(POLLOUT) && defined(POLLWRNORM)
#  define POLLWRITEFLAGS (POLLOUT|POLLWRNORM)
# else
#  if defined(POLLOUT)
#   define POLLWRITEFLAGS POLLOUT
#  else
#   if defined(POLLWRNORM)
#    define POLLWRITEFLAGS POLLWRNORM
#   endif
#  endif
# endif

# if defined(POLLERR) && defined(POLLHUP)
#  define POLLERRORS (POLLERR|POLLHUP)
# else
#  define POLLERRORS POLLERR
# endif

# define PFD_SETR(thisfd) { CHECK_PFD(thisfd);\
                            pfd->events |= POLLREADFLAGS; }
# define PFD_SETW(thisfd) { CHECK_PFD(thisfd);\
                            pfd->events |= POLLWRITEFLAGS; }
# define CHECK_PFD( thisfd )                    \
        if ( pfd->fd != thisfd ) {              \
                pfd = &poll_fdarray[nbr_pfds++];\
                pfd->fd     = thisfd;           \
                pfd->events = 0;                \
        }

#ifdef NO_FDLIST
#error You cannot set NO_FDLIST and USE_POLL at same time!
#else
int  read_message(time_t delay, fdlist *listp)
#endif
{
	aClient *cptr;
	int  nfds;
	static struct pollfd poll_fdarray[MAXCONNECTIONS];
	struct pollfd *pfd = poll_fdarray;
	struct pollfd *res_pfd = NULL;
	struct pollfd *socks_pfd = NULL;
	int  nbr_pfds = 0;
	u_long waittime;
	time_t delay2 = delay;
	int  res, length, fd;
	int  auth, rw, socks;
	int  sockerr;
	int  i, j;
	static char errmsg[512];
	static aClient *authclnts[MAXCONNECTIONS];
	static aClient *socksclnts[MAXCONNECTIONS];
	/* if it is called with NULL we check all active fd's */
	if (!listp)
	{
		listp = &default_fdlist;
		listp->last_entry = LastSlot == -1 ? LastSlot : LastSlot + 1;
	}

	for (res = 0;;)
	{
		nbr_pfds = 0;
		pfd = poll_fdarray;
		pfd->fd = -1;
		res_pfd = NULL;
		socks_pfd = NULL;
		auth = 0;
		socks = 0;
		for (i = listp->entry[j = 1]; j <= listp->last_entry; i = listp->entry[++j])
		{
			if (!(cptr = local[i]))
				continue;
			if (IsLog(cptr))
				continue;
			
			if (DoingAuth(cptr))
			{
				if (auth == 0)
					memset((char *)&authclnts, '\0',
					    sizeof(authclnts));
				auth++;
				Debug((DEBUG_NOTICE, "auth on %x %d", cptr, i));
				PFD_SETR(cptr->authfd);
				if (cptr->flags & FLAGS_WRAUTH)
					PFD_SETW(cptr->authfd);
				authclnts[cptr->authfd] = cptr;
				continue;
			}
			if (DoingDNS(cptr) || DoingAuth(cptr)
			    )
				continue;
			if (IsMe(cptr) && IsListening(cptr))
			{
#define CONNECTFAST
# ifdef CONNECTFAST
				/*
				 * This is VERY bad if someone tries to send a lot of
				 * clones to the server though, as mbuf's can't be
				 * allocated quickly enough... - Comstud
				 */
				PFD_SETR(i);
# else
				if (now > (cptr->lasttime + 2))
				{
					PFD_SETR(i);
				}
				else if (delay2 > 2)
					delay2 = 2;
# endif
			}
			else if (!IsMe(cptr))
			{
/*				if (DBufLength(&cptr->recvQ) && delay2 > 2)
					delay2 = 1; */
				if (DBufLength(&cptr->recvQ) < 4088)
					PFD_SETR(i);
			}

			length = DBufLength(&cptr->sendQ);
			if (DoList(cptr) && IsSendable(cptr))
			{
				send_list(cptr, 64);
				length = DBufLength(&cptr->sendQ);
			}

			if (length || IsConnecting(cptr))
				PFD_SETW(i);
		}

		__THIS__CODE__DOES__NOT__WORK__

/* FIXME: no ZIP link handling here, but this code doesnt work anyway -- Syzop */

		if (me.socksfd >= 0)
		{
			PFD_SETR(me.socksfd);
			socks_pfd = pfd;
		}

		waittime = MIN(delay2, delay) * 1000;
		nfds = poll(poll_fdarray, nbr_pfds, waittime);
		if (nfds == -1 && ((errno == EINTR) || (errno == EAGAIN)))
			return -1;
		else if (nfds >= 0)
			break;
		report_error("poll %s:%s", &me);
		res++;
		if (res > 5)
			restart("too many poll errors");
		sleep(10);
	}

	if (res_pfd && (res_pfd->revents & (POLLREADFLAGS | POLLERRORS)))
	{
		do_dns_async();
		nfds--;
	}

	if (socks_pfd && (socks_pfd->revents & (POLLREADFLAGS | POLLERRORS)))
	{
		int  tmpsock;
		nfds--;
		tmpsock = accept(me.socksfd, NULL, NULL);
		if (tmpsock >= 0)
			close(tmpsock);
	}

	for (pfd = poll_fdarray, i = 0; (nfds > 0) && (i < nbr_pfds);
	    i++, pfd++)
	{
		if (!pfd->revents)
			continue;
		if (pfd == res_pfd)
			continue;
		if (pfd == socks_pfd)
			continue;
		nfds--;
		fd = pfd->fd;
		rr = pfd->revents & POLLREADFLAGS;
		rw = pfd->revents & POLLWRITEFLAGS;
		if (pfd->revents & POLLERRORS)
		{
			if (pfd->events & POLLREADFLAGS)
				rr++;
			if (pfd->events & POLLWRITEFLAGS)
				rw++;
		}
		if ((auth > 0) && ((cptr = authclnts[fd]) != NULL) &&
		    (cptr->authfd == fd))
		{
			auth--;
			if (rr)
				read_authports(cptr);
			if (rw)
				send_authports(cptr);
			continue;
		}
		if (!(cptr = local[fd]))
			continue;
		if (rr && IsListening(cptr))
		{
			cptr->lasttime = TStime();
			/*
			 ** There may be many reasons for error return, but
			 ** in otherwise correctly working environment the
			 ** probable cause is running out of file descriptors
			 ** (EMFILE, ENFILE or others?). The man pages for
			 ** accept don't seem to list these as possible,
			 ** although it's obvious that it may happen here.
			 ** Thus no specific errors are tested at this
			 ** point, just assume that connections cannot
			 ** be accepted until some old is closed first.
			 */
			if ((fd = accept(fd, NULL, NULL)) < 0)
			{
				report_error("Cannot accept connections %s:%s",
				    cptr);
				break;
			}
			ircstp->is_ac++;
			if (fd >= MAXCLIENTS)
			{
				ircstp->is_ref++;
				if (last_allinuse < TStime() - 15)
				{
					sendto_realops("All connections in use. (%s)",
					    get_client_name(cptr, TRUE));
					last_allinuse = TStime();
				}
				(void)send(fd,
				    "ERROR :All connections in use\r\n", 32, 0);
				(void)close(fd);
				break;
			}
			/*
			 * Use of add_connection (which never fails :) meLazy
			 */
			(void)add_connection(cptr, fd);

			nextping = TStime();
			if (!cptr->listener)
				cptr->listener = &me;

			continue;
		}
		if (IsMe(cptr))
			continue;
		if (rw)		/* socket is marked for writing.. */
		{
			int  write_err = 0;

			if (IsConnecting(cptr))
				write_err = completed_connection(cptr);
			if (!write_err)
				(void)send_queued(cptr);
			if (IsDead(cptr) || write_err)
			{

				(void)exit_client(cptr, cptr, &me,
				    ((sockerr =
				    get_sockerr(cptr)) ? STRERROR(sockerr) :
				    "Client exited"));
				continue;
			}
		}

		length = 1;	/* for fall through case */

		if (rr)
			length = read_packet(cptr);
		else if (IsPerson(cptr) && !NoNewLine(cptr))
			length = do_client_queue(cptr);

# ifdef DEBUGMODE
		readcalls++;
# endif
		if (length == FLUSH_BUFFER)
			continue;

		if (IsDead(cptr))
		{
			ircsprintf(errmsg, "Read/Dead Error: %s",
			    STRERROR(get_sockerr(cptr)));
			exit_client(cptr, cptr, &me, errmsg);
			continue;
		}

		if (length > 0)
			continue;

		/* An error has occured reading from cptr, drop it. */
		/*
		   ** NOTE: if length == -2 then cptr has already been freed!
		 */
		if (length != -2 && (IsServer(cptr) || IsHandshake(cptr)))
		{
			if (length == 0)
			{
				sendto_locfailops
				    ("Server %s closed the connection",
				    get_client_name(cptr, FALSE));
				sendto_serv_butone(&me,
				    ":%s GLOBOPS :Server %s closed the connection",
				    me.name, get_client_name(cptr, FALSE));
			}
			else
				report_error("Lost connection to %s:%s", cptr);
		}
		if (length != FLUSH_BUFFER)
			(void)exit_client(cptr, cptr, &me,
			    ((sockerr = get_sockerr(cptr))
			    ? STRERROR(sockerr) : "Client exited"));

	}
	return 0;
}

#endif
/*
 * connect_server
 */
int  connect_server(ConfigItem_link *aconf, aClient *by, struct hostent *hp)
{
	struct SOCKADDR *svp;
	aClient *cptr;
	char *s;
	int  errtmp, len;

#ifdef DEBUGMODE
	sendto_realops("connect_server() called with aconf %p, refcount: %d, TEMP: %s",
		aconf, aconf->refcount, aconf->flag.temporary ? "YES" : "NO");
#endif

	if (!hp && (aconf->options & CONNECT_NODNSCACHE)) {
		/* Remove "cache" if link::options::nodnscache is set */
		memset(&aconf->ipnum, '\0', sizeof(struct IN_ADDR));
	}
	/*
	 * If we dont know the IP# for this host and itis a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	 if (!WHOSTENTP(aconf->ipnum.S_ADDR))
	 {
		nextdnscheck = 1;
		s = aconf->hostname;
#ifndef INET6
		if ((aconf->ipnum.S_ADDR = inet_addr(s)) == -1)
#else
		if (!inet_pton(AF_INET6, s, aconf->ipnum.s6_addr))
#endif
		{
#ifdef INET6
			bzero(aconf->ipnum.s6_addr, IN6ADDRSZ);
#else
			aconf->ipnum.S_ADDR = 0;
#endif
			/* We need this 'aconf->refcount++' or else there's a race condition between
			 * starting resolving the host and the result of the resolver (we could
			 * REHASH in that timeframe) leading to an invalid (freed!) 'aconf'.
			 * -- Syzop, bug #0003689.
			 */
			aconf->refcount++;
			unrealdns_gethostbyname_link(aconf->hostname, aconf);
			return -2;
		}
	}
	cptr = make_client(NULL, NULL);
	cptr->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strncpyzt(cptr->name, aconf->servername, sizeof(cptr->name));
	strncpyzt(cptr->sockhost, aconf->hostname, HOSTLEN + 1);

	svp = connect_inet(aconf, cptr, &len);
	if (!svp)
	{
		if (cptr->fd >= 0)
		{
			CLOSE_SOCK(cptr->fd);
			--OpenFiles;
		}
		cptr->fd = -2;
		free_client(cptr);
		return -1;
	}
	set_non_blocking(cptr->fd, cptr);
	set_sock_opts(cptr->fd, cptr);
#ifndef _WIN32
	(void)signal(SIGALRM, dummy);
	if (connect(cptr->fd, svp, len) < 0 && errno != EINPROGRESS)
	{
#else
	if (connect(cptr->fd, svp, len) < 0 &&
	    WSAGetLastError() != WSAEINPROGRESS &&
	    WSAGetLastError() != WSAEWOULDBLOCK)
	{
#endif
		errtmp = ERRNO;
		report_error("Connect to host %s failed: %s", cptr);
		if (by && IsPerson(by) && !MyClient(by))
			sendto_one(by,
			    ":%s NOTICE %s :*** Connect to host %s failed.",
			    me.name, by->name, cptr->name);
		CLOSE_SOCK(cptr->fd);
		--OpenFiles;
		cptr->fd = -2;
		free_client(cptr);
		SET_ERRNO(errtmp);
		if (ERRNO == P_EINTR)
			SET_ERRNO(P_ETIMEDOUT);
		return -1;
	}

	/*
	   ** The socket has been connected or connect is in progress.
	 */
	(void)make_server(cptr);
	cptr->serv->conf = aconf;
	cptr->serv->conf->refcount++;
#ifdef DEBUGMODE
	sendto_realops("connect_server() CONTINUED (%s:%d), aconf %p, refcount: %d, TEMP: %s",
		__FILE__, __LINE__, aconf, aconf->refcount, aconf->flag.temporary ? "YES" : "NO");
#endif
	Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
		cptr->name, cptr->serv->conf->servername, cptr->serv->conf->refcount));
	if (by && IsPerson(by))
	{
		(void)strlcpy(cptr->serv->by, by->name, sizeof cptr->serv->by);
		if (cptr->serv->user)
			free_user(cptr->serv->user, NULL);
		cptr->serv->user = by->user;
		by->user->refcnt++;
	}
	else
	{
		(void)strlcpy(cptr->serv->by, "AutoConn.", sizeof cptr->serv->by);
		if (cptr->serv->user)
			free_user(cptr->serv->user, NULL);
		cptr->serv->user = NULL;
	}
	cptr->serv->up = me.name;
    add_local_client(cptr);
	cptr->listener = &me;
	SetConnecting(cptr);
	SetOutgoing(cptr);
	IRCstats.unknown++;
	get_sockhost(cptr, aconf->hostname);
	add_client_to_list(cptr);
	nextping = TStime();
	return 0;
}

static struct SOCKADDR *connect_inet(ConfigItem_link *aconf, aClient *cptr, int *lenp)
{
	static struct SOCKADDR_IN server;
	struct hostent *hp;

	/*
	 * Might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */
	cptr->fd = socket(AFINET, SOCK_STREAM, 0);
	if (cptr->fd < 0)
	{
		if (ERRNO == P_EMFILE)
		{
		  sendto_realops("opening stream socket to server %s: No more sockets",
					 get_client_name(cptr, TRUE));
		  return NULL;
		}
		report_baderror("opening stream socket to server %s:%s", cptr);
		return NULL;
	}
	if (++OpenFiles >= MAXCLIENTS)
	{
		sendto_realops("No more connections allowed (%s)", cptr->name);
		return NULL;
	}
	mysk.SIN_PORT = 0;
	bzero((char *)&server, sizeof(server));
	server.SIN_FAMILY = AFINET;
	get_sockhost(cptr, aconf->hostname);
	
	server.SIN_PORT = 0;
	server.SIN_ADDR = me.ip;
	server.SIN_FAMILY = AFINET;
	if (aconf->bindip && strcmp("*", aconf->bindip))
	{
#ifndef INET6
		server.SIN_ADDR.S_ADDR = inet_addr(aconf->bindip);	
#else
		inet_pton(AF_INET6, aconf->bindip, server.SIN_ADDR.S_ADDR);
#endif
	}
	if (bind(cptr->fd, (struct SOCKADDR *)&server, sizeof(server)) == -1)
	{
		report_baderror("error binding to local port for %s:%s", cptr);
		return NULL;
	}
	bzero((char *)&server, sizeof(server));
	server.SIN_FAMILY = AFINET;
	/*
	 * By this point we should know the IP# of the host listed in the
	 * conf line, whether as a result of the hostname lookup or the ip#
	 * being present instead. If we dont know it, then the connect fails.
	 */
#ifdef INET6
	if (!WHOSTENTP(aconf->ipnum.S_ADDR) &&
	    !inet_pton(AF_INET6, aconf->hostname, aconf->ipnum.s6_addr))
		bcopy(minus_one, aconf->ipnum.s6_addr, IN6ADDRSZ); /* IP->struct failed: make invalid */
	if (AND16(aconf->ipnum.s6_addr) == 255)
#else
	if (isdigit(*aconf->hostname) && (aconf->ipnum.S_ADDR == -1))
		aconf->ipnum.S_ADDR = inet_addr(aconf->hostname);
	if (aconf->ipnum.S_ADDR == -1)
#endif
	{
		hp = cptr->hostp;
		if (!hp)
		{
			Debug((DEBUG_FATAL, "%s: unknown host", aconf->hostname));
			return NULL;
		}
		bcopy(hp->h_addr, (char *)&aconf->ipnum, sizeof(struct IN_ADDR));
	}
	bcopy((char *)&aconf->ipnum, (char *)&server.SIN_ADDR, sizeof(struct IN_ADDR));
	bcopy((char *)&aconf->ipnum, (char *)&cptr->ip, sizeof(struct IN_ADDR));
	server.SIN_PORT = htons(((aconf->port > 0) ? aconf->port : portnum));
	*lenp = sizeof(server);
	return (struct SOCKADDR *)&server;
}
