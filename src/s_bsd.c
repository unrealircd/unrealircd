/*
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

#ifndef lint
static char sccsid[] =
    "@(#)s_bsd.c	2.78 2/7/94 (C) 1988 University of Oulu, \
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
#include <utmp.h>
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
#include "resolv.h"
#include "sock.h"		/* If FD_ZERO isn't define up to this point,  */
			/* define it (BSD4.2 needs this) */
#include "h.h"
#ifndef NO_FDLIST
#include  "fdlist.h"
#endif
#ifdef USE_POLL
#include <sys/poll.h>
int  rr;

#endif


#ifndef IN_LOOPBACKNET
#define IN_LOOPBACKNET	0x7f
#endif

#define INADDRSZ sizeof(struct IN_ADDR)
#define IN6ADDRSZ sizeof(struct IN_ADDR)

#ifdef _WIN32
extern HWND hwIRCDWnd;
#endif
extern char backupbuf[8192];
aClient *local[MAXCONNECTIONS];
int  highest_fd = 0, readcalls = 0, resfd = -1;
static struct SOCKADDR_IN mysk;

static struct SOCKADDR *connect_inet PROTO((aConfItem *, aClient *, int *));
static int completed_connection PROTO((aClient *));
static int check_init PROTO((aClient *, char *));
#ifndef _WIN32
static void do_dns_async PROTO(()), set_sock_opts PROTO((int, aClient *));
#else
static void set_sock_opts PROTO((int, aClient *));
#endif
static char readbuf[8192];
char zlinebuf[BUFSIZE];
extern char *version;
extern ircstats IRCstats;

#ifndef NO_FDLIST
extern fdlist default_fdlist;
extern fdlist busycli_fdlist;
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;
extern fdlist socks_fdlist;
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

/*
** add_local_domain()
** Add the domain to hostname, if it is missing
** (as suggested by eps@TOASTER.SFSU.EDU)
*/

void add_local_domain(hname, size)
	char *hname;
	int  size;
{
#ifdef RES_INIT
	/* try to fix up unqualified names */
	if (!index(hname, '.'))
	{
		if (!(_res.options & RES_INIT))
		{
			Debug((DEBUG_DNS, "res_init()"));
			res_init();
		}
		if (_res.defdname[0])
		{
			(void)strncat(hname, ".", size - 1);
			(void)strncat(hname, _res.defdname, size - 2);
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
**		be taken from sys_errlist[errno].
**
**	cptr	if not NULL, is the *LOCAL* client associated with
**		the error.
*/
void report_error(text, cptr)
	char *text;
	aClient *cptr;
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
	Debug((DEBUG_ERROR, text, host, strerror(errtmp)));

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
	sendto_realops(text, host, strerror(errtmp));
	ircd_log(text,host,strerror(errtmp));
#ifdef USE_SYSLOG
	syslog(LOG_WARNING, text, host, strerror(errtmp));
#endif
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
int  inetport(cptr, name, port)
	aClient *cptr;
	char *name;
	int  port;
{
	static struct SOCKADDR_IN server;
	int  ad[4], len = sizeof(server);
	char ipname[20];

	if (BadPtr(name))
		name = "*";
	ad[0] = ad[1] = ad[2] = ad[3] = 0;

	/*
	 * do it this way because building ip# from separate values for each
	 * byte requires endian knowledge or some nasty messing. Also means
	 * easy conversion of "*" 0.0.0.0 or 134.* to 134.0.0.0 :-)
	 */
	(void)sscanf(name, "%d.%d.%d.%d", &ad[0], &ad[1], &ad[2], &ad[3]);
	(void)ircsprintf(ipname, "%d.%d.%d.%d", ad[0], ad[1], ad[2], ad[3]);

	if (cptr != &me)
	{
		(void)ircsprintf(cptr->sockhost, "%-.42s.%.u",
		    name, (unsigned int)port);
		(void)strcpy(cptr->name, me.name);
	}
	/*
	 * At first, open a new socket
	 */
	if (cptr->fd == -1)
		cptr->fd = socket(AFINET, SOCK_STREAM, 0);

	if (cptr->fd < 0)
	{
#if !defined(DEBUGMODE) && !defined(_WIN32)
#endif
		report_error("Cannot open stream socket() %s:%s", cptr);
		return -1;
	}
	else if (cptr->fd >= MAXCLIENTS)
	{
		sendto_ops("No more connections allowed (%s)", cptr->name);
#ifndef _WIN32
		(void)close(cptr->fd);
#else
		(void)closesocket(cptr->fd);
#endif
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
			strcat(backupbuf, "- %s:%s");
			report_error(backupbuf, cptr);
#ifndef _WIN32
			(void)close(cptr->fd);
#else
			(void)closesocket(cptr->fd);
#endif
			return -1;
		}
	}
	if (getsockname(cptr->fd, (struct SOCKADDR *)&server, &len))
	{
		report_error("getsockname failed for %s:%s", cptr);
#ifndef _WIN32
		(void)close(cptr->fd);
#else
		(void)closesocket(cptr->fd);
#endif
		return -1;
	}

	if (cptr == &me)	/* KLUDGE to get it work... */
	{
		char buf[1024];

		(void)ircsprintf(buf, rpl_str(RPL_MYPORTIS), me.name, "*",
		    ntohs(server.SIN_PORT));
		(void)write(0, buf, strlen(buf));
	}

	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
#ifdef INET6
	bcopy(server.sin6_addr.s6_addr, cptr->ip.s6_addr, IN6ADDRSZ);
#else
	cptr->ip.S_ADDR = name ? inet_addr(ipname) : me.ip.S_ADDR;
#endif
	cptr->port = (int)ntohs(server.SIN_PORT);
	(void)listen(cptr->fd, LISTEN_SIZE);
	local[cptr->fd] = cptr;

	return 0;
}

/*
 * add_listener
 *
 * Create a new client which is essentially the stub like 'me' to be used
 * for a socket that is passive (listen'ing for connections to be accepted).
 */
int  add_listener(aconf)
	aConfItem *aconf;
{
	aClient *cptr;
	char *p;

	cptr = make_client(NULL, NULL);
	cptr->flags = FLAGS_LISTEN;
	cptr->acpt = cptr;
	cptr->from = cptr;
	SetMe(cptr);
	strncpyzt(cptr->name, aconf->host, sizeof(cptr->name));
	if (inetport(cptr, aconf->host, aconf->port))
		cptr->fd = -2;

	p = aconf->passwd;
	if (*p == '*')
		cptr->umodes = LISTENER_NORMAL;
	else
	{
		for (; *p; p++)
		{
			switch (*p)
			{
			  case 'C':
				  cptr->umodes |= LISTENER_CLIENTSONLY;
				  break;
			  case 'S':
				  cptr->umodes |= LISTENER_SERVERSONLY;
				  break;
			  case 'R':
				  cptr->umodes |= LISTENER_REMOTEADMIN;
				  break;
			  case 'J':
				  cptr->umodes |= LISTENER_JAVACLIENT;
				  break;
			  case 'I':
			  {
				  cptr->umodes |= LISTENER_MASK;
				  p++;
				  /* */
				  strcpy(cptr->info, p);
			  }
			}
		}
	}
	strcpy(cptr->name, aconf->name);

	if (cptr->fd >= 0)
	{
		cptr->confs = make_link();
		cptr->confs->next = NULL;
		cptr->confs->value.aconf = aconf;
		set_non_blocking(cptr->fd, cptr);
	}
	else
		free_client(cptr);
	return 0;
}

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections. 
 */
void close_listeners()
{
	aClient *cptr;
	int  i;
	aConfItem *aconf;

	/*
	 * close all 'extra' listening ports we have
	 */
	for (i = highest_fd; i >= 0; i--)
	{
		if (!(cptr = local[i]))
			continue;
		if (!IsMe(cptr) || cptr == &me || !IsListening(cptr))
			continue;
		aconf = cptr->confs->value.aconf;

		if (IsIllegal(aconf) && aconf->clients == 0)
		{
			close_connection(cptr);
		}
	}
}

/*
 * init_sys
 */
void init_sys()
{
	int  fd;
#ifndef USE_POLL
#ifdef RLIMIT_FD_MAX
	struct rlimit limit;

	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	{
		if (limit.rlim_max < MAXCONNECTIONS)
		{
			(void)fprintf(stderr, "ircd fd table too big\n");
			(void)fprintf(stderr, "Hard Limit: %d IRC max: %d\n",
			    limit.rlim_max, MAXCONNECTIONS);
			(void)fprintf(stderr, "Fix MAXCONNECTIONS\n");
			exit(-1);
		}
	limit.rlim_cur = limit.rlim_max;	/* make soft limit the max */
	if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
	{
		(void)fprintf(stderr, "error setting max fd's to %d\n",
		    limit.rlim_cur);
		exit(-1);
	}
}

#endif
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
for (fd = 3; fd < MAXCONNECTIONS; fd++)
{
	(void)close(fd);
	local[fd] = NULL;
}

local[1] = NULL;
(void)close(1);

if (bootopt & BOOT_TTY)		/* debugging is going to a tty */
	goto init_dgram;
if (!(bootopt & BOOT_DEBUG))
	(void)close(2);

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
		(void)close(fd);
	}
#endif

#if defined(HPUX) || defined(_SOLARIS) || \
    defined(_POSIX_SOURCE) || defined(SVR4) || defined(SGI)
	(void)setsid();
#else
	(void)setpgrp(0, (int)getpid());
#endif
	(void)close(0);		/* fd 0 opened by inetd */
	local[0] = NULL;
}
init_dgram:
#endif /*_WIN32*/
resfd = init_resolver(0x1f);

return;
}

void write_pidfile()
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
#ifdef INET6
#undef IN6_IS_ADDR_LOOPBACK

int  IN6_IS_ADDR_LOOPBACK(u_int32_t * f)
{
	if ((*f == 0) && (*(f + 1) == 0)
	    && (*(f + 2) == 0) && (*(f + 3) == htonl(1)))
		return 1;

	return 0;
}

#define IN6_IS_ADDR_LOOPBACK(a) \
	((u_int32_t) (a)[0] == 0) && \
	((u_int32_t) (a)[1] == 0) && \
	((u_int32_t) (a)[2] == 0) && \
	((u_int32_t) (a)[3] == htonl(1))
#endif
/*
 * Initialize the various name strings used to store hostnames. This is set
 * from either the server's sockhost (if client fd is a tty or localhost)
 * or from the ip# converted into a string. 0 = success, -1 = fail.
 */
static int check_init(cptr, sockn)
	aClient *cptr;
	char *sockn;
{
	struct SOCKADDR_IN sk;
	int  len = sizeof(struct SOCKADDR_IN);


	/* If descriptor is a tty, special checking... */
#ifndef _WIN32
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
		report_error("connect failure: %s %s", cptr);
		return -1;
	}
#ifdef INET6
	inetntop(AF_INET6, (char *)&sk.sin6_addr, sockn, MYDUMMY_SIZE);
#else
	(void)strcpy(sockn, (char *)inetntoa((char *)&sk.SIN_ADDR));
#endif

#ifdef INET6
#undef IN6_IS_ADDR_LOOPBACK
	if (IN6_IS_ADDR_LOOPBACK(&sk.SIN_ADDR))
#else
	if (inet_netof(sk.SIN_ADDR) == IN_LOOPBACKNET)
#endif
	{
		cptr->hostp = NULL;
		strncpyzt(sockn, me.sockhost, HOSTLEN);
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
int  check_client(cptr)
	aClient *cptr;
{
	static char sockname[HOSTLEN + 1];
	struct hostent *hp = NULL;
	int  i;

	ClearAccess(cptr);
	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]",
	    cptr->name, inetntoa((char *)&cptr->ip)));

	if (check_init(cptr, sockname))
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
			sendto_ops("IP# Mismatch: %s != %s[%08x]",
			    inetntoa((char *)&cptr->ip), hp->h_name,
			    *((unsigned long *)hp->h_addr));
			hp = NULL;
		}
	}

	if ((i = attach_Iline(cptr, hp, sockname)))
	{
		Debug((DEBUG_DNS, "ch_cl: access denied: %s[%s]",
		    cptr->name, sockname));
		return i;
	}

	Debug((DEBUG_DNS, "ch_cl: access ok: %s[%s]", cptr->name, sockname));

#ifdef INET6
	if (IN6_IS_ADDR_LOOPBACK(&cptr->ip) ||
	    (cptr->ip.s6_laddr[0] == mysk.sin6_addr.s6_laddr[0] &&
	    cptr->ip.s6_laddr[1] == mysk.sin6_addr.s6_laddr[1])
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

#define	CFLAG	CONF_CONNECT_SERVER
#define	NFLAG	CONF_NOCONNECT_SERVER
/*
 * check_server_init(), check_server()
 *	check access for a server given its name (passed in cptr struct).
 *	Must check for all C/N lines which have a name which matches the
 *	name given and a host which matches. A host alias which is the
 *	same as the server name is also acceptable in the host field of a
 *	C/N line.
 *  0 = Success
 * -1 = Access denied
 * -2 = Bad socket.
 */
int  check_server_init(cptr)
	aClient *cptr;
{
	char *name;
	aConfItem *c_conf = NULL, *n_conf = NULL;
	struct hostent *hp = NULL;
	Link *lp;

	name = cptr->name;
	Debug((DEBUG_DNS, "sv_cl: check access for %s[%s]",
	    name, cptr->sockhost));

	if (IsUnknown(cptr) && !attach_confs(cptr, name, CFLAG | NFLAG))
	{
		Debug((DEBUG_DNS, "No C/N lines for %s", name));
		return -1;
	}
	lp = cptr->confs;
	/*
	 * We initiated this connection so the client should have a C and N
	 * line already attached after passing through the connec_server()
	 * function earlier.
	 */
	if (IsConnecting(cptr) || IsHandshake(cptr))
	{
		c_conf = find_conf(lp, name, CFLAG);
		n_conf = find_conf(lp, name, NFLAG);
		if (!c_conf || !n_conf)
		{
			sendto_ops("Connecting Error: %s[%s]", name,
			    cptr->sockhost);
			det_confs_butmask(cptr, 0);
			return -1;
		}
	}

	/*
	   ** If the servername is a hostname, either an alias (CNAME) or
	   ** real name, then check with it as the host. Use gethostbyname()
	   ** to check for servername as hostname.
	 */
	if (!cptr->hostp)
	{
		aConfItem *aconf;

		aconf = count_cnlines(lp);
		if (aconf)
		{
			char *s;
			Link lin;

			/*
			   ** Do a lookup for the CONF line *only* and not
			   ** the server connection else we get stuck in a
			   ** nasty state since it takes a SERVER message to
			   ** get us here and we cant interrupt that very
			   ** well.
			 */
			ClearAccess(cptr);
			lin.value.aconf = aconf;
			lin.flags = ASYNC_CONF;
			nextdnscheck = 1;
			if ((s = index(aconf->host, '@')))
				s++;
			else
				s = aconf->host;
			Debug((DEBUG_DNS, "sv_ci:cache lookup (%s)", s));
			hp = gethost_byname(s, &lin);
		}
	}
	return check_server(cptr, hp, c_conf, n_conf, 0);

}

int  check_server(cptr, hp, c_conf, n_conf, estab)
	aClient *cptr;
	aConfItem *n_conf, *c_conf;
	struct hostent *hp;
	int  estab;
{
	char *name;
	char abuff[HOSTLEN + USERLEN + 2];
	char sockname[HOSTLEN + 1], fullname[HOSTLEN + 1];
	Link *lp = cptr->confs;
	int  i;

	ClearAccess(cptr);
	if (check_init(cptr, sockname))
		return -2;

      check_serverback:
	if (hp)
	{
		for (i = 0; hp->h_addr_list[i]; i++)
			if (!bcmp(hp->h_addr_list[i], (char *)&cptr->ip,
			    sizeof(struct IN_ADDR)))
				break;
		if (!hp->h_addr_list[i])
		{
			sendto_ops("IP# Mismatch: %s != %s[%08x]",
			    inetntoa((char *)&cptr->ip), hp->h_name,
			    *((unsigned long *)hp->h_addr));
			hp = NULL;
		}
	}
	else if (cptr->hostp)
	{
		hp = cptr->hostp;
		goto check_serverback;
	}

	if (hp)
		/*
		 * if we are missing a C or N line from above, search for
		 * it under all known hostnames we have for this ip#.
		 */
		for (i = 0, name = hp->h_name; name; name = hp->h_aliases[i++])
		{
			strncpyzt(fullname, name, sizeof(fullname));
			add_local_domain(fullname, HOSTLEN - strlen(fullname));
			Debug((DEBUG_DNS, "sv_cl: gethostbyaddr: %s->%s",
			    sockname, fullname));
			(void)ircsprintf(abuff, "%s@%s",
			    cptr->username, fullname);
			if (!c_conf)
				c_conf = find_conf_host(lp, abuff, CFLAG);
			if (!n_conf)
				n_conf = find_conf_host(lp, abuff, NFLAG);
			if (c_conf && n_conf)
			{
				get_sockhost(cptr, fullname);
				break;
			}
		}
	name = cptr->name;

	/*
	 * Check for C and N lines with the hostname portion the ip number
	 * of the host the server runs on. This also checks the case where
	 * there is a server connecting from 'localhost'.
	 */
	if (IsUnknown(cptr) && (!c_conf || !n_conf))
	{
		(void)ircsprintf(abuff, "%s@%s", cptr->username, sockname);
		if (!c_conf)
			c_conf = find_conf_host(lp, abuff, CFLAG);
		if (!n_conf)
			n_conf = find_conf_host(lp, abuff, NFLAG);
	}
	/*
	 * Attach by IP# only if all other checks have failed.
	 * It is quite possible to get here with the strange things that can
	 * happen when using DNS in the way the irc server does. -avalon
	 */
	if (!hp)
	{
		if (!c_conf)
			c_conf = find_conf_ip(lp, (char *)&cptr->ip,
			    cptr->username, CFLAG);
		if (!n_conf)
			n_conf = find_conf_ip(lp, (char *)&cptr->ip,
			    cptr->username, NFLAG);
	}
	else
		for (i = 0; hp->h_addr_list[i]; i++)
		{
			if (!c_conf)
				c_conf = find_conf_ip(lp, hp->h_addr_list[i],
				    cptr->username, CFLAG);
			if (!n_conf)
				n_conf = find_conf_ip(lp, hp->h_addr_list[i],
				    cptr->username, NFLAG);
		}
	/*
	 * detach all conf lines that got attached by attach_confs()
	 */
	det_confs_butmask(cptr, 0);
	/*
	 * if no C or no N lines, then deny access
	 */
	if (!c_conf || !n_conf)
	{
		get_sockhost(cptr, sockname);
		Debug((DEBUG_DNS, "sv_cl: access denied: %s[%s@%s] c %x n %x",
		    name, cptr->username, cptr->sockhost, c_conf, n_conf));
		return -1;
	}
	/*
	 * attach the C and N lines to the client structure for later use.
	 */
	(void)attach_conf(cptr, n_conf);
	(void)attach_conf(cptr, c_conf);
	(void)attach_confs(cptr, name, CONF_HUB | CONF_LEAF | CONF_UWORLD);
#ifdef INET6
	if ((AND16(c_conf->ipnum.s6_addr) == 255))
#else
	if (c_conf->ipnum.S_ADDR == -1)
#endif
		bcopy((char *)&cptr->ip, (char *)&c_conf->ipnum,
		    sizeof(struct IN_ADDR));
	get_sockhost(cptr, c_conf->host);

	Debug((DEBUG_DNS, "sv_cl: access ok: %s[%s]", name, cptr->sockhost));
	if (estab)
		return m_server_estab(cptr);
	return 0;
}
#undef	CFLAG
#undef	NFLAG

/*
** completed_connection
**	Complete non-blocking connect()-sequence. Check access and
**	terminate connection, if trouble detected.
**
**	Return	TRUE, if successfully completed
**		FALSE, if failed and ClientExit
*/
static int completed_connection(cptr)
	aClient *cptr;
{
	aConfItem *aconf;
	extern char serveropts[];
	SetHandshake(cptr);

	aconf = find_conf(cptr->confs, cptr->name, CONF_CONNECT_SERVER);
	if (!aconf)
	{
		sendto_ops("Lost C-Line for %s", get_client_name(cptr, FALSE));
		return -1;
	}
	if (!BadPtr(aconf->passwd))
		sendto_one(cptr, "PASS :%s", aconf->passwd);

	aconf = find_conf(cptr->confs, cptr->name, CONF_NOCONNECT_SERVER);
	if (!aconf)
	{
		sendto_ops("Lost N-Line for %s", get_client_name(cptr, FALSE));
		return -1;
	}
	sendto_one(cptr, "PROTOCTL %s", PROTOCTL_SERVER);
	sendto_one(cptr, "SERVER %s 1 :U%d-%s-%i %s",
	    my_name_for_link(me.name, aconf), UnrealProtocol, serveropts, me.serv->numeric,
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
void close_connection(cptr)
	aClient *cptr;
{
	aConfItem *aconf;
	int  i, j;
	int  empty = cptr->fd;

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
	del_queries((char *)cptr);
	/*
	 * If the connection has been up for a long amount of time, schedule
	 * a 'quick' reconnect, else reset the next-connect cycle.
	 *
	 * Now just hold on a minute.  We're currently doing this when a
	 * CLIENT exits too?  I don't think so!  If its not a server, or
	 * the SQUIT flag has been set, then we don't schedule a fast
	 * reconnect.  Pisses off too many opers. :-)  -Cabal95
	 */
	if (IsServer(cptr) && !(cptr->flags & FLAGS_SQUIT) &&
	    (aconf = find_conf_exact(cptr->name, cptr->username,
	    cptr->sockhost, CONF_CONNECT_SERVER)))
	{
		/*
		 * Reschedule a faster reconnect, if this was a automaticly
		 * connected configuration entry. (Note that if we have had
		 * a rehash in between, the status has been changed to
		 * CONF_ILLEGAL). But only do this if it was a "good" link.
		 */
		aconf->hold = TStime();
		aconf->hold += (aconf->hold - cptr->since > HANGONGOODLINK) ?
		    HANGONRETRYDELAY : ConfConFreq(aconf);
		if (nextconnect > aconf->hold)
			nextconnect = aconf->hold;
	}

	if (cptr->authfd >= 0)
#ifndef _WIN32
		(void)close(cptr->authfd);
#else
		(void)closesocket(cptr->authfd);
#endif

#ifdef SOCKSPORT
	if (cptr->socksfd >= 0)
#ifndef _WIN32
		(void)close(cptr->socksfd);
#else
		(void)closesocket(cptr->socksfd);
#endif /* _WIN32 */
#endif /* SOCKSPORT */



	if (cptr->fd >= 0)
	{
		flush_connections(cptr->fd);
		local[cptr->fd] = NULL;
#ifndef _WIN32
		(void)close(cptr->fd);
#else
		(void)closesocket(cptr->fd);
#endif
		cptr->fd = -2;
		DBufClear(&cptr->sendQ);
		DBufClear(&cptr->recvQ);
	
		/*
		 * clean up extra sockets from P-lines which have been
		 * discarded.
		 */
		if (cptr->acpt != &me && cptr->acpt != cptr)
		{
			aconf = cptr->acpt->confs->value.aconf;
			if (aconf->clients > 0)
				aconf->clients--;
			if (!aconf->clients && IsIllegal(aconf))
				close_connection(cptr->acpt);
		}
	}
	for (; highest_fd > 0; highest_fd--)
		if (local[highest_fd])
			break;

	det_confs_butmask(cptr, 0);
	cptr->from = NULL;	/* ...this should catch them! >:) --msa */

	/*
	 * fd remap to keep local[i] filled at the bottom.
	 */
	if (empty > 0)
		if ((j = highest_fd) > (i = empty) &&
		    (local[j]->status != STAT_LOG))
		{
			if (dup2(j, i) == -1)
				return;
			local[i] = local[j];
			local[i]->fd = i;
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
#ifndef _WIN32
			(void)close(j);
#else
			(void)closesocket(j);
#endif
			while (!local[highest_fd])
				highest_fd--;
		}
	return;
}

/*
** set_sock_opts
*/
static void set_sock_opts(fd, cptr)
	int  fd;
	aClient *cptr;
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
#if !defined(_SOLARIS) || defined(USE_SYSLOG)
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
#if defined(IP_OPTIONS) && defined(IPPROTO_IP) && !defined(_WIN32)
	{
		char *s = readbuf, *t = readbuf + sizeof(readbuf) / 2;

		opt = sizeof(readbuf) / 8;
		if (getsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)t,
		    &opt) < 0)
			report_error("getsockopt(IP_OPTIONS) %s:%s", cptr);
		else if (opt > 0 && opt != sizeof(readbuf) / 8)
		{
			for (*readbuf = '\0'; opt > 0; opt--, s += 3)
				(void)ircsprintf(s, "%02.2x:", *t++);
			*s = '\0';
			sendto_ops("Connection %s using IP opts: (%s)",
			    get_client_name(cptr, TRUE), readbuf);
		}
		if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, (OPT_TYPE *)NULL,
		    0) < 0)
			report_error("setsockopt(IP_OPTIONS) %s:%s", cptr);
	}
#endif
}


int  get_sockerr(cptr)
	aClient *cptr;
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
** set_non_blocking
**	Set the client connection into non-blocking mode. If your
**	system doesn't support this, you can make this a dummy
**	function (and get all the old problems that plagued the
**	blocking version of IRC--not a problem if you are a
**	lightly loaded node...)
*/
void set_non_blocking(fd, cptr)
	int  fd;
	aClient *cptr;
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
		report_error("ioctl(fd,FIONBIO) failed for %s:%s", cptr);
#else
# if !defined(_WIN32)
	if ((res = fcntl(fd, F_GETFL, 0)) == -1)
		report_error("fcntl(fd, F_GETFL) failed for %s:%s", cptr);
	else if (fcntl(fd, F_SETFL, res | nonb) == -1)
		report_error("fcntl(fd, F_SETL, nonb) failed for %s:%s", cptr);
# else
	nonb = 1;
	if (ioctlsocket(fd, FIONBIO, &nonb) < 0)
		report_error("ioctlsocket(fd,FIONBIO) failed for %s:%s", cptr);
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
aClient *add_connection(cptr, fd)
	aClient *cptr;
	int  fd;
{
	Link lin;
	aClient *acptr;
	aConfItem *aconf = NULL;
	acptr = make_client(NULL, &me);

	if (cptr != &me)
		aconf = cptr->confs->value.aconf;
	/* Removed preliminary access check. Full check is performed in
	 * m_server and m_user instead. Also connection time out help to
	 * get rid of unwanted connections.
	 */
#ifndef _WIN32
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
			report_error("Failed in connecting to %s :%s", cptr);
		      add_con_refuse:
			ircstp->is_ref++;
			acptr->fd = -2;
			free_client(acptr);
#ifndef _WIN32
			(void)close(fd);
#else
			(void)closesocket(fd);
#endif
			return NULL;
		}
		/* don't want to add "Failed in connecting to" here.. */
		if (aconf && IsIllegal(aconf))
			goto add_con_refuse;
		/* Copy ascii address to 'sockhost' just in case. Then we
		 * have something valid to put into error messages...
		 */
#ifdef INET6
		inetntop(AF_INET6, (char *)&addr.sin6_addr, mydummy,
		    MYDUMMY_SIZE);
		get_sockhost(acptr, (char *)mydummy);
#else
		get_sockhost(acptr, (char *)inetntoa((char *)&addr.SIN_ADDR));
#endif
		bcopy((char *)&addr.SIN_ADDR, (char *)&acptr->ip,
		    sizeof(struct IN_ADDR));
		/* Check for zaps -- Barubary */
		if (find_zap(acptr, 0))
		{
			set_non_blocking(fd, acptr);
			set_sock_opts(fd, acptr);
			send(fd, zlinebuf, strlen(zlinebuf), 0);
			goto add_con_refuse;
		}
		else if (find_tkline_match_zap(acptr) != -1)
		{
			set_non_blocking(fd, acptr);
			set_sock_opts(fd, acptr);
			send(fd, zlinebuf, strlen(zlinebuf), 0);
			goto add_con_refuse;
		}
		acptr->port = ntohs(addr.SIN_PORT);
#if 0
		/*
		 * Some genious along the lines of ircd took out the code
		 * where ircd loads the IP mask from the P:Lines, so this
		 * is useless untill that's added back. :)
		 */
		/*
		 * Check that this socket (client) is allowed to accept
		 * connections from this IP#.
		 */
		for (s = (char *)&cptr->ip, t = (char *)&acptr->ip, len = 4;
		    len > 0; len--, s++, t++)
		{
			if (!*s)
				continue;
			if (*s != *t)
				break;
		}

		if (len)
			goto add_con_refuse;
#endif
#ifdef SHOWCONNECTINFO
#ifndef _WIN32
		write(fd, REPORT_DO_DNS, R_do_dns);
#else
		send(fd, REPORT_DO_DNS, R_do_dns, 0);
#endif
#endif
		lin.flags = ASYNC_CLIENT;
		lin.value.cptr = acptr;
		Debug((DEBUG_DNS, "lookup %s",
		    inetntoa((char *)&addr.SIN_ADDR)));
		acptr->hostp = gethost_byaddr((char *)&acptr->ip, &lin);
		if (!acptr->hostp)
			SetDNS(acptr);
#ifdef SHOWCONNECTINFO
		else
#ifndef _WIN32
			write(fd, REPORT_FIN_DNSC, R_fin_dnsc);
#else
			send(fd, REPORT_FIN_DNSC, R_fin_dnsc, 0);
#endif

#endif
		nextdnscheck = 1;
	}

	if (aconf)
		aconf->clients++;
	acptr->fd = fd;
	if (fd > highest_fd)
		highest_fd = fd;
	local[fd] = acptr;
	acptr->acpt = cptr;
	add_client_to_list(acptr);
	set_non_blocking(acptr->fd, acptr);
	set_sock_opts(acptr->fd, acptr);
	IRCstats.unknown++;
	start_auth(acptr);

#ifdef SOCKSPORT

	start_socks(acptr);
#endif
	return acptr;
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
static int read_packet(cptr, rfd)
	aClient *cptr;
	fd_set *rfd;
{
	int  dolen = 0, length = 0, done;
	time_t now = TStime();
	if (FD_ISSET(cptr->fd, rfd) &&
	    !(IsPerson(cptr) && DBufLength(&cptr->recvQ) > 6090))
	{
#ifndef _WIN32
		errno = 0;
#else
		WSASetLastError(0);
#endif
#ifdef INET6
		length = recvfrom(cptr->fd, readbuf, sizeof(readbuf), 0, 0, 0);
#else
		length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);
#endif

		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);
		/*
		 * If not ready, fake it so it isnt closed
		 */
		if (length == -1 &&
#ifndef _WIN32
		    ((errno == EWOULDBLOCK) || (errno == EAGAIN)))
#else
		    (WSAGetLastError() == WSAEWOULDBLOCK))
#endif
		    return 1;
		if (length <= 0)
			return length;
	}
	/*
	   ** For server connections, we process as many as we can without
	   ** worrying about the time of day or anything :)
	 */
	if (IsServer(cptr) || IsConnecting(cptr) || IsHandshake(cptr) 
#ifdef CRYPTOIRCD
		|| IsSecure(cptr)
#endif	
		)
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
		if (dbuf_put(&cptr->recvQ, readbuf, length) < 0)
			return exit_client(cptr, cptr, cptr, "dbuf_put fail");

		if (IsPerson(cptr) && DBufLength(&cptr->recvQ) > CLIENT_FLOOD)
		{
			sendto_umode(UMODE_FLOOD | UMODE_OPER,
			    "*** Flood -- %s!%s@%s (%d) exceeds %d recvQ",
			    cptr->name[0] ? cptr->name : "*",
			    cptr->user ? cptr->user->username : "*",
			    cptr->user ? cptr->user->realhost : "*",
			    DBufLength(&cptr->recvQ), CLIENT_FLOOD);
			return exit_client(cptr, cptr, cptr, "Excess Flood");
		}

		while (DBufLength(&cptr->recvQ) && !NoNewLine(cptr) &&
		    ((cptr->status < STAT_UNKNOWN) || (cptr->since - now < 10)))
		{
			/*
			   ** If it has become registered as a Service or Server
			   ** then skip the per-message parsing below.
			 */
			if (IsServer(cptr) 
#ifdef CRYPTOIRCD
				|| IsSecure(cptr)
#endif
				) 
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
		if (IsServer(cptr) 
#ifdef CRYPTOIRCD
			|| IsSecure(cptr)
#endif
		)
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

		length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);

		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);
		/* 
		 * If not ready, fake it so it isnt closed
		 */
		if (length == -1 && ((errno == EWOULDBLOCK)
		    || (errno == EAGAIN)))
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
		if (dbuf_put(&cptr->recvQ, readbuf, length) < 0)
			return exit_client(cptr, cptr, cptr, "dbuf_put fail");

		if (IsPerson(cptr) &&
#ifdef NO_OPER_FLOOD
		    !IsAnOper(cptr) &&
#endif
		    DBufLength(&cptr->recvQ) > CLIENT_FLOOD)
		{
			sendto_umode(UMODE_FLOOD,
			    "Flood -- %s!%s@%s (%d) Exceeds %d RecvQ",
			    cptr->name[0] ? cptr->name : "*",
			    cptr->user ? cptr->user->username : "*",
			    cptr->user ? cptr->user->realhost : "*",
			    DBufLength(&cptr->recvQ), CLIENT_FLOOD);
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
int  read_message(delay)
#else
int  read_message(delay, listp)
#endif
	time_t delay;		/* Don't ever use ZERO here, unless you mean to poll and then
				 * you have to have sleep/wait somewhere else in the code.--msa
				 */
#ifndef NO_FDLIST
	fdlist *listp;
#endif
{
	aClient *cptr;
	int  nfds;
	struct timeval wait;
#ifndef _WIN32
	fd_set read_set, write_set;
#else
	fd_set read_set, write_set, excpt_set;
#endif
	int  j;
	time_t delay2 = delay, now;
	u_long usec = 0;
	int  res, length, fd, i;
	int  auth = 0;
#ifdef SOCKSPORT
	int  socks = 0;
#endif
	int  sockerr;

#ifndef NO_FDLIST
	/* if it is called with NULL we check all active fd's */
	if (!listp)
	{
		listp = &default_fdlist;
		listp->last_entry = highest_fd + 1;	/* remember the 0th entry isnt used */
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

#ifdef NO_FDLIST
		for (i = highest_fd; i >= 0; i--)
#else
		for (i = listp->entry[j = 1]; j <= listp->last_entry;
		    i = listp->entry[++j])
#endif
		{
			if (!(cptr = local[i]))
				continue;
			if (IsLog(cptr))
				continue;
#ifdef SOCKSPORT
			if (DoingSocks(cptr))
			{
				socks++;
				FD_SET(cptr->socksfd, &read_set);
#ifdef _WIN32
				FD_SET(cptr->socksfd, &excpt_set);
#endif
				if (cptr->flags & FLAGS_WRSOCKS)
					FD_SET(cptr->socksfd, &write_set);
			}
#endif /* SOCKSPORT */

			if (DoingAuth(cptr))
			{
				auth++;
				Debug((DEBUG_NOTICE, "auth on %x %d", cptr, i));
				FD_SET(cptr->authfd, &read_set);
#ifdef _WIN32
				FD_SET(cptr->authfd, &excpt_set);
#endif
				if (cptr->flags & FLAGS_WRAUTH)
					FD_SET(cptr->authfd, &write_set);
			}
			if (DoingDNS(cptr) || DoingAuth(cptr)
#ifdef SOCKSPORT
			    || DoingSocks(cptr)
#endif
			    )
				continue;
			if (IsMe(cptr) && IsListening(cptr))
			{
				FD_SET(i, &read_set);
			}
			else if (!IsMe(cptr))
			{
				if (DBufLength(&cptr->recvQ) && delay2 > 2)
					delay2 = 1;
				if (DBufLength(&cptr->recvQ) < 4088)
					FD_SET(i, &read_set);
			}

			if (DBufLength(&cptr->sendQ) || IsConnecting(cptr) ||
			    (DoList(cptr) && IsSendable(cptr)))
				FD_SET(i, &write_set);
		}

#ifdef SOCKSPORT
		if (me.socksfd >= 0)
			FD_SET(me.socksfd, &read_set);
#endif
#ifndef _WIN32
		if (resfd >= 0)
			FD_SET(resfd, &read_set);
#endif

		wait.tv_sec = MIN(delay2, delay);
		wait.tv_usec = usec;
#ifdef	HPUX
		nfds = select(FD_SETSIZE, (int *)&read_set, (int *)&write_set,
		    0, &wait);
#else
# ifndef _WIN32
		nfds = select(FD_SETSIZE, &read_set, &write_set, 0, &wait);
# else
		nfds =
		    select(FD_SETSIZE, &read_set, &write_set, &excpt_set,
		    &wait);
# endif
#endif
#ifndef _WIN32
		if (nfds == -1 && errno == EINTR)
#else
		if (nfds == -1 && WSAGetLastError() == WSAEINTR)
#endif
			return -1;
		else if (nfds >= 0)
			break;
		report_error("select %s:%s", &me);
		res++;
		if (res > 5)
			restart("too many select errors");
#ifndef _WIN32
		sleep(10);
#else
		Sleep(10);
#endif
	}
#ifdef SOCKSPORT
	if (me.socksfd >= 0 && FD_ISSET(me.socksfd, &read_set))
	{
		int  tmpsock;

		tmpsock = accept(me.socksfd, NULL, NULL);
		if (tmpsock >= 0)
#ifdef _WIN32
			closesocket(tmpsock);
#else
			close(tmpsock);
#endif /* _WIN32 */
		FD_CLR(me.socksfd, &read_set);
	}
#endif /* SOCKSPORT */
#ifndef _WIN32
	if (resfd >= 0 && FD_ISSET(resfd, &read_set))
	{
		do_dns_async();
		nfds--;
		FD_CLR(resfd, &read_set);
	}
#endif
	/*
	 * Check fd sets for the auth fd's (if set and valid!) first
	 * because these can not be processed using the normal loops below.
	 * -avalon
	 */
#ifdef NO_FDLIST
	for (i = highest_fd; (auth > 0) && (i >= 0); i--)
#else
	for (i = listp->entry[j = 1]; j <= listp->last_entry;
	    i = listp->entry[++j])
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
				if (cptr->authfd == highest_fd)
					while (!local[highest_fd])
						highest_fd--;
				cptr->authfd = -1;
				cptr->flags &= ~(FLAGS_AUTH | FLAGS_WRAUTH);
				if (!DoingDNS(cptr))
					SetAccess(cptr);
				if (nfds > 0)
					nfds--;
				continue;
			}
		}
#endif
		if ((nfds > 0) && FD_ISSET(cptr->authfd, &write_set))
		{
			nfds--;
			send_authports(cptr);
		}
		else if ((nfds > 0) && FD_ISSET(cptr->authfd, &read_set))
		{
			nfds--;
			read_authports(cptr);
		}
	}
#ifdef SOCKSPORT
	/*
	 * I really hate to do this.. but another loop
	 * to check to see if we have any socks fd's.. - darkrot
	 */
	for (i = highest_fd; (socks > 0) && (i >= 0); i--)
	{
		if (!(cptr = local[i]))
			continue;
		if (cptr->socksfd < 0 || IsMe(cptr))
			continue;
		socks--;
#ifdef _WIN32
		/*
		 * Because of the way windows uses select(), we have to use
		 * the exception FD set to find out when a connection is
		 * refused.  ie Auth ports and /connect's.  -Cabal95
		 */
		if (FD_ISSET(cptr->socksfd, &excpt_set))
		{
			int  err, len = sizeof(err);

			if (getsockopt(cptr->socksfd, SOL_SOCKET, SO_ERROR,
			    (OPT_TYPE *)&err, &len) || err)
			{
				ircstp->is_abad++;
				closesocket(cptr->socksfd);
				if (cptr->socksfd == highest_fd)
					while (!local[highest_fd])
						highest_fd--;
				cptr->socksfd = -1;
				cptr->flags &= ~(FLAGS_SOCKS | FLAGS_WRSOCKS);
				if (nfds > 0)
					nfds--;
				continue;
			}
		}
#endif /* _WIN32 */
		if ((nfds > 0) && FD_ISSET(cptr->socksfd, &write_set))
		{
			nfds--;
			send_socksquery(cptr);
		}
		else if ((nfds > 0) && FD_ISSET(cptr->socksfd, &read_set))
		{
			nfds--;
			read_socks(cptr);
		}
	}
#endif /* SOCKSPORT */

	for (i = highest_fd; i >= 0; i--)
		if ((cptr = local[i]) && FD_ISSET(i, &read_set) &&
		    IsListening(cptr))
		{
			FD_CLR(i, &read_set);
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
			 */
			if ((fd = accept(i, NULL, NULL)) < 0)
			{
				report_error("Cannot accept connections %s:%s",
				    cptr);
				break;
			}
			ircstp->is_ac++;
			if (fd >= MAXCLIENTS)
			{
				ircstp->is_ref++;
				sendto_ops("All connections in use. (%s)",
				    get_client_name(cptr, TRUE));
#ifndef INET6
				(void)send(fd,
				    "ERROR :All connections in use\r\n", 32, 0);
#else
				(void)sendto(fd,
				    "ERROR :All connections in use\r\n",
				    32, 0, 0, 0);
#endif
#ifndef _WIN32
				(void)close(fd);
#else
				(void)closesocket(fd);
#endif
				break;
			}
			/*
			 * Use of add_connection (which never fails :) meLazy
			 */
			(void)add_connection(cptr, fd);
			nextping = TStime();
			if (!cptr->acpt)
				cptr->acpt = &me;
		}

	for (i = highest_fd; i >= 0; i--)
	{
		if (!(cptr = local[i]) || IsMe(cptr))
			continue;
		if (FD_ISSET(i, &write_set))
		{
			int  write_err = 0;
			nfds--;
			/*
			   ** ...room for writing, empty some queue then...
			 */
			ClearBlocked(cptr);
			if (IsConnecting(cptr))
				write_err = completed_connection(cptr);
			if (!write_err)
			{
				if (DoList(cptr) && IsSendable(cptr))
					send_list(cptr, 32);
				(void)send_queued(cptr);
			}

			if (IsDead(cptr) || write_err)
			{
			      deadsocket:
				if (FD_ISSET(i, &read_set))
				{
					nfds--;
					FD_CLR(i, &read_set);
				}
				(void)exit_client(cptr, cptr, &me,
				    ((sockerr = get_sockerr(cptr))
				    ? strerror(sockerr) : "Client exited"));
				continue;
			}
		}
		length = 1;	/* for fall through case */
		if (!NoNewLine(cptr) || FD_ISSET(i, &read_set))
			length = read_packet(cptr, &read_set);
		if (length > 0)
			flush_connections(i);
		if ((length != FLUSH_BUFFER) && IsDead(cptr))
			goto deadsocket;
		if (!FD_ISSET(i, &read_set) && length > 0)
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
#ifndef _WIN32
		Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
		    i, errno, length));
#else
		Debug((DEBUG_ERROR, "READ ERROR: fd = %d %d %d",
		    i, WSAGetLastError(), length));
#endif

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
			    ? strerror(sockerr) : "Client exited"));
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
int  read_message(delay, listp)
#endif
	time_t delay;		/* Don't ever use ZERO here, unless you mean to poll and then
				 * you have to have sleep/wait somewhere else in the code.--msa
				 */
	fdlist *listp;
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
		listp->last_entry = highest_fd + 1;
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
		for (i = listp->entry[j = 1]; j <= listp->last_entry;
		    i = listp->entry[++j])
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
#ifdef SOCKSPORT
			if (DoingSocks(cptr))
			{
				if (socks == 0)
					memset((char *)&socksclnts, '\0',
					    sizeof(authclnts));
				socks++;
				Debug((DEBUG_NOTICE, "socks on %x %d", cptr,
				    i));
				PFD_SETR(cptr->socksfd);
				if (cptr->flags & FLAGS_WRSOCKS)
					PFD_SETW(cptr->socksfd);
				socksclnts[cptr->socksfd] = cptr;
				continue;
			}
#endif
			if (DoingDNS(cptr) || DoingAuth(cptr)
#ifdef SOCKSPORT
			    || DoingSocks(cptr)
#endif
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

		if (resfd >= 0)
		{
			PFD_SETR(resfd);
			res_pfd = pfd;
		}

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
#ifdef SOCKSPORT
		if ((socks > 0) && ((cptr = socksclnts[fd]) != NULL) &&
		    (cptr->socksfd == fd))
		{
			socks--;
			if (rr)
			{
				read_socks(cptr);
				continue;
			}
			if (rw)
			{
				send_socksquery(cptr);
			}
			continue;
		}
#endif
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
				sendto_ops("All connections in use. (%s)",
				    get_client_name(cptr, TRUE));
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
			if (!cptr->acpt)
				cptr->acpt = &me;

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
				    get_sockerr(cptr)) ? strerror(sockerr) :
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
			    strerror(get_sockerr(cptr)));
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
			    ? strerror(sockerr) : "Client exited"));

	}
	return 0;
}

#endif
/*
 * connect_server
 */
int  connect_server(aconf, by, hp)
	aConfItem *aconf;
	aClient *by;
	struct hostent *hp;
{
	struct SOCKADDR *svp;
	aClient *cptr, *c2ptr;
	char *s;
	int  errtmp, len;

	Debug((DEBUG_NOTICE, "Connect to %s[%s] @%s",
	    aconf->name, aconf->host, inetntoa((char *)&aconf->ipnum)));

	if ((c2ptr = find_server_quick(aconf->name)))
	{
		sendto_ops("Server %s already present from %s",
		    aconf->name, get_client_name(c2ptr, TRUE));
		if (by && IsPerson(by) && !MyClient(by))
			sendto_one(by,
			    ":%s NOTICE %s :Server %s already present from %s",
			    me.name, by->name, aconf->name,
			    get_client_name(c2ptr, TRUE));
		return -1;
	}

	/*
	 * If we dont know the IP# for this host and itis a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	if ((!aconf->ipnum.S_ADDR))
	{
		Link lin;

		lin.flags = ASYNC_CONNECT;
		lin.value.aconf = aconf;
		nextdnscheck = 1;
		s = (char *)index(aconf->host, '@');
		s++;		/* should NEVER be NULL */
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
			hp = gethost_byname(s, &lin);
			Debug((DEBUG_NOTICE, "co_sv: hp %x ac %x na %s ho %s",
			    hp, aconf, aconf->name, s));
			if (!hp)
				return 0;
			bcopy(hp->h_addr, (char *)&aconf->ipnum,
			    sizeof(struct IN_ADDR));
		}
	}
	cptr = make_client(NULL, NULL);
	cptr->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strncpyzt(cptr->name, aconf->name, sizeof(cptr->name));
	strncpyzt(cptr->sockhost, aconf->host, HOSTLEN + 1);

	svp = connect_inet(aconf, cptr, &len);

	if (!svp)
	{
		if (cptr->fd != -1)
#ifndef _WIN32
			(void)close(cptr->fd);
#else
			(void)closesocket(cptr->fd);
#endif
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
		errtmp = errno;	/* other system calls may eat errno */
#else
	if (connect(cptr->fd, svp, len) < 0 &&
	    WSAGetLastError() != WSAEINPROGRESS &&
	    WSAGetLastError() != WSAEWOULDBLOCK)
	{
		errtmp = WSAGetLastError();	/* other system calls may eat errno */
#endif
		report_error("Connect to host %s failed: %s", cptr);
		if (by && IsPerson(by) && !MyClient(by))
			sendto_one(by,
			    ":%s NOTICE %s :Connect to host %s failed.",
			    me.name, by->name, cptr);
#ifndef _WIN32
		(void)close(cptr->fd);
#else
		(void)closesocket(cptr->fd);
#endif
		cptr->fd = -2;
		free_client(cptr);
#ifndef _WIN32
		errno = errtmp;
		if (errno == EINTR)
			errno = ETIMEDOUT;
#else
		WSASetLastError(errtmp);
		if (errtmp == WSAEINTR)
			WSASetLastError(WSAETIMEDOUT);
#endif
		return -1;
	}

	/* Attach config entries to client here rather than in
	 * completed_connection. This to avoid null pointer references
	 * when name returned by gethostbyaddr matches no C lines
	 * (could happen in 2.6.1a when host and servername differ).
	 * No need to check access and do gethostbyaddr calls.
	 * There must at least be one as we got here C line...  meLazy
	 */
	(void)attach_confs_host(cptr, aconf->host,
	    CONF_NOCONNECT_SERVER | CONF_CONNECT_SERVER);

	if (!find_conf_host(cptr->confs, aconf->host, CONF_NOCONNECT_SERVER) ||
	    !find_conf_host(cptr->confs, aconf->host, CONF_CONNECT_SERVER))
	{
		sendto_ops("Host %s is not enabled for connecting:no C/N-line",
		    aconf->host);
		if (by && IsPerson(by) && !MyClient(by))
			sendto_one(by,
			    ":%s NOTICE %s :Connect to host %s failed.",
			    me.name, by->name, cptr);
		det_confs_butmask(cptr, 0);
#ifndef _WIN32
		(void)close(cptr->fd);
#else
		(void)closesocket(cptr->fd);
#endif
		cptr->fd = -2;
		free_client(cptr);
		return (-1);
	}
	/*
	   ** The socket has been connected or connect is in progress.
	 */
	(void)make_server(cptr);
	if (by && IsPerson(by))
	{
		(void)strcpy(cptr->serv->by, by->name);
		if (cptr->serv->user)
			free_user(cptr->serv->user, NULL);
		cptr->serv->user = by->user;
		by->user->refcnt++;
	}
	else
	{
		(void)strcpy(cptr->serv->by, "AutoConn.");
		if (cptr->serv->user)
			free_user(cptr->serv->user, NULL);
		cptr->serv->user = NULL;
	}
	cptr->serv->up = me.name;
	if (cptr->fd > highest_fd)
		highest_fd = cptr->fd;
	local[cptr->fd] = cptr;
	cptr->acpt = &me;
	SetConnecting(cptr);
	IRCstats.unknown++;
	get_sockhost(cptr, aconf->host);
	add_client_to_list(cptr);
	nextping = TStime();

	return 0;
}

static struct SOCKADDR *connect_inet(aconf, cptr, lenp)
	aConfItem *aconf;
	aClient *cptr;
	int *lenp;
{
	static struct SOCKADDR_IN server;
	struct hostent *hp;

	/*
	 * Might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */
	cptr->fd = socket(AFINET, SOCK_STREAM, 0);
	if (cptr->fd >= MAXCLIENTS)
	{
		sendto_ops("No more connections allowed (%s)", cptr->name);
		return NULL;
	}
	mysk.SIN_PORT = 0;
	bzero((char *)&server, sizeof(server));
	server.SIN_FAMILY = AFINET;
	get_sockhost(cptr, aconf->host);

	if (cptr->fd == -1)
	{
		report_error("opening stream socket to server %s:%s", cptr);
		return NULL;
	}
	get_sockhost(cptr, aconf->host);
	server.SIN_PORT = 0;
	server.SIN_ADDR = me.ip;
	server.SIN_FAMILY = AFINET;
	/*
	   ** Bind to a local IP# (with unknown port - let unix decide) so
	   ** we have some chance of knowing the IP# that gets used for a host
	   ** with more than one IP#.
	 */
	/* No we don't bind it, not all OS's can handle connecting with
	   ** an already bound socket, different ip# might occur anyway
	   ** leading to a freezing select() on this side for some time.
	   ** I had this on my Linux 1.1.88 --Run
	 */
	/* We do now.  Virtual interface stuff --ns */
	if (me.ip.S_ADDR != INADDR_ANY)
		if (bind(cptr->fd, (struct SOCKADDR *)&server,
		    sizeof(server)) == -1)
		{
			report_error("error binding to local port for %s:%s",
			    cptr);
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
	if (isdigit(*aconf->host) && (AND16(aconf->ipnum.s6_addr) == 255))
		if (!inet_pton(AF_INET6, aconf->host, aconf->ipnum.s6_addr))
			bcopy(minus_one, aconf->ipnum.s6_addr, IN6ADDRSZ);
	if (AND16(aconf->ipnum.s6_addr) == 255)
#else
	if (isdigit(*aconf->host) && (aconf->ipnum.S_ADDR == -1))
		aconf->ipnum.S_ADDR = inet_addr(aconf->host);
	if (aconf->ipnum.S_ADDR == -1)
#endif
	{
		hp = cptr->hostp;
		if (!hp)
		{
			Debug((DEBUG_FATAL, "%s: unknown host", aconf->host));
			return NULL;
		}
		bcopy(hp->h_addr, (char *)&aconf->ipnum,
		    sizeof(struct IN_ADDR));
	}
	bcopy((char *)&aconf->ipnum, (char *)&server.SIN_ADDR,
	    sizeof(struct IN_ADDR));
	bcopy((char *)&aconf->ipnum, (char *)&cptr->ip, sizeof(struct IN_ADDR));
	server.SIN_PORT = htons(((aconf->port > 0) ? aconf->port : portnum));
	*lenp = sizeof(server);
	return (struct SOCKADDR *)&server;
}


/*
** find the real hostname for the host running the server (or one which
** matches the server's name) and its primary IP#.  Hostname is stored
** in the client structure passed as a pointer.
*/
void get_my_name(cptr, name, len)
	aClient *cptr;
	char *name;
	int  len;
{
	static char tmp[HOSTLEN + 1];
	struct hostent *hp;
	char *cname = cptr->name;

	/*
	   ** Setup local socket structure to use for binding to.
	 */
	bzero((char *)&mysk, sizeof(mysk));
	mysk.SIN_FAMILY = AFINET;

	if (gethostname(name, len) == -1)
		return;
	name[len] = '\0';

	/* assume that a name containing '.' is a FQDN */
	if (!index(name, '.'))
		add_local_domain(name, len - strlen(name));

	/*
	   ** If hostname gives another name than cname, then check if there is
	   ** a CNAME record for cname pointing to hostname. If so accept
	   ** cname as our name.   meLazy
	 */
	if (BadPtr(cname))
		return;
	if ((hp = gethostbyname(cname)) || (hp = gethostbyname(name)))
	{
		char *hname;
		int  i = 0;

		for (hname = hp->h_name; hname; hname = hp->h_aliases[i++])
		{
			strncpyzt(tmp, hname, sizeof(tmp));
			add_local_domain(tmp, sizeof(tmp) - strlen(tmp));

			/*
			   ** Copy the matching name over and store the
			   ** 'primary' IP# as 'myip' which is used
			   ** later for making the right one is used
			   ** for connecting to other hosts.
			 */
			if (!mycmp(me.name, tmp))
				break;
		}
		if (mycmp(me.name, tmp))
			strncpyzt(name, hp->h_name, len);
		else
			strncpyzt(name, tmp, len);
		bcopy(hp->h_addr, (char *)&mysk.SIN_ADDR,
		    sizeof(struct IN_ADDR));
		Debug((DEBUG_DEBUG, "local name is %s",
		    get_client_name(&me, TRUE)));
	}
	return;
}

/*
 * do_dns_async
 *
 * Called when the fd returned from init_resolver() has been selected for
 * reading.
 */
#ifndef _WIN32
static void do_dns_async()
#else
void do_dns_async(id)
	int  id;
#endif
{
	static Link ln;
	aClient *cptr;
	aConfItem *aconf;
	struct hostent *hp;

	ln.flags = -1;
#ifndef _WIN32
	hp = get_res((char *)&ln);
#else
	hp = get_res((char *)&ln, id);
#endif
	while (hp != NULL)
	{
		Debug((DEBUG_DNS, "%#x = get_res(%d,%#x)", hp, ln.flags,
		    ln.value.cptr));

		switch (ln.flags)
		{
		  case ASYNC_NONE:
			  /*
			   * no reply was processed that was outstanding or had a client
			   * still waiting.
			   */
			  break;
		  case ASYNC_CLIENT:
			  if ((cptr = ln.value.cptr))
			  {
				  del_queries((char *)cptr);
#ifdef SHOWCONNECTINFO
#ifndef _WIN32
				  write(cptr->fd, REPORT_FIN_DNS, R_fin_dns);
#else
				  send(cptr->fd, REPORT_FIN_DNS, R_fin_dns, 0);
#endif
#endif
				  ClearDNS(cptr);
				  if (!DoingAuth(cptr))
					  SetAccess(cptr);
				  cptr->hostp = hp;
			  }
			  break;
		  case ASYNC_CONNECT:
			  aconf = ln.value.aconf;
			  if (hp && aconf)
			  {
				  bcopy(hp->h_addr, (char *)&aconf->ipnum,
				      sizeof(struct IN_ADDR));
				  (void)connect_server(aconf, NULL, hp);
			  }
			  else
				  sendto_ops
				      ("Connect to %s failed: host lookup",
				      (aconf) ? aconf->host : "unknown");
			  break;
		  case ASYNC_CONF:
			  aconf = ln.value.aconf;
			  if (hp && aconf)
				  bcopy(hp->h_addr, (char *)&aconf->ipnum,
				      sizeof(struct IN_ADDR));
			  break;
		  case ASYNC_SERVER:
			  cptr = ln.value.cptr;
			  del_queries((char *)cptr);
			  ClearDNS(cptr);
			  if (check_server(cptr, hp, NULL, NULL, 1))
				  (void)exit_client(cptr, cptr, &me,
				      "No Authorization");
			  break;
		  default:
			  break;
		}

		ln.flags = -1;
#ifndef _WIN32
		hp = get_res((char *)&ln);
#else
		hp = get_res((char *)&ln, id);
#endif
	}			/* while (hp != NULL) */
}
