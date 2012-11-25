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

#ifdef _WIN32
#include <WinSock2.h>
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
#include "sock.h"		/* If FD_ZERO isn't define up to this point,  */
#include <string.h>
#include "proto.h"
			/* define it (BSD4.2 needs this) */
#include "h.h"
#include "fdlist.h"

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
void completed_connection(int, int, void *);
static int check_init(aClient *, char *, size_t);
static void do_dns_async();
void set_sock_opts(int, aClient *);
static char readbuf[READBUF_SIZE];
char zlinebuf[BUFSIZE];
extern char *version;
extern ircstats IRCstats;
MODVAR TS last_allinuse = 0;

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

	list_for_each_entry(cptr, &lclient_list, lclient_node)
	{
		if (cptr->fd >= 0)
		{
			fd_close(cptr->fd);
			cptr->fd = -2;
		}
	}

	list_for_each_entry(cptr, &unknown_list, lclient_node)
	{
		if (cptr->fd >= 0)
		{
			fd_close(cptr->fd);
			cptr->fd = -2;
		}

		if (cptr->authfd >= 0)
		{
			fd_close(cptr->authfd);
			cptr->fd = -1;
		}
	}

	close_listeners();

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
static void listener_accept(int fd, int revents, void *data)
{
	ConfigItem_listen *cptr = data;
	int cli_fd;

	if ((cli_fd = fd_accept(cptr->fd)) < 0)
	{
	        if ((ERRNO != P_EWOULDBLOCK) && (ERRNO != P_ECONNABORTED))
			report_baderror("Cannot accept connections %s:%s", NULL);
		return;
	}

	ircstp->is_ac++;

	if ((++OpenFiles >= MAXCLIENTS) || (fd >= MAXCLIENTS))
	{
		ircstp->is_ref++;
		if (last_allinuse < TStime() - 15)
		{
			sendto_realops("All connections in use. ([@%s/%u])", cptr->ip, cptr->port);
			last_allinuse = TStime();
		}

		(void)send(cli_fd, "ERROR :All connections in use\r\n", 31, 0);

		fd_close(cli_fd);
		--OpenFiles;
		return;
	}

	 /*
	  * Use of add_connection (which never fails :) meLazy
	  */
	(void) add_connection(cptr, cli_fd);
}

int  inetport(ConfigItem_listen *listener, char *name, int port)
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

	/*
	 * At first, open a new socket
	 */
	if (listener->fd == -1)
	{
		listener->fd = fd_socket(AFINET, SOCK_STREAM, 0, "Listener socket");
	}
	if (listener->fd < 0)
	{
		report_baderror("Cannot open stream socket() %s:%s", NULL);
		return -1;
	}
	else if (++OpenFiles >= MAXCLIENTS)
	{
		sendto_ops("No more connections allowed (%s)", listener->ip);
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	set_sock_opts(listener->fd, NULL);
	set_non_blocking(listener->fd, NULL);

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
		if (bind(listener->fd, (struct SOCKADDR *)&server,
		    sizeof(server)) == -1)
		{
			ircsprintf(backupbuf, "Error binding stream socket to IP %s port %i",
				ipname, port);
			strlcat(backupbuf, " - %s:%s", sizeof backupbuf);
			report_baderror(backupbuf, NULL);
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
			fd_close(listener->fd);
			listener->fd = -1;
			--OpenFiles;
			return -1;
		}
	}
	if (getsockname(listener->fd, (struct SOCKADDR *)&server, &len))
	{
		report_error("getsockname failed for %s:%s", NULL);
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	(void)listen(listener->fd, LISTEN_SIZE);
	fd_setselect(listener->fd, FD_SELECT_READ, listener_accept, listener);

	return 0;
}

int add_listener2(ConfigItem_listen *conf)
{
	if (inetport(conf, conf->ip, conf->port))
	{
		ircd_log(LOG_ERROR, "inetport failed for %s:%u", conf->ip, conf->port);
		conf->fd = -2;
	}
	if (conf->fd >= 0)
	{
		conf->options |= LISTENER_BOUND;
		return 1;
	}
	else
	{
		conf->fd = -1;
		return -1;
	}
}

/*
 * close_listeners
 *
 * Close and free all clients which are marked as having their socket open
 * and in a state where they can accept connections.
 */
void close_listener(ConfigItem_listen *listener)
{
	fd_close(listener->fd);

	listener->options &= ~LISTENER_BOUND;
	listener->fd = -1;
}

void close_listeners(void)
{
	aClient *cptr;
	ConfigItem_listen *aconf, *aconf_next;

	/*
	 * close all 'extra' listening ports we have
	 */
	for (aconf = conf_listen; aconf != NULL; aconf = aconf_next)
	{
		aconf_next = (ConfigItem_listen *) aconf->next;

		if (aconf->flag.temporary && (aconf->clients == 0))
			close_listener(aconf);
	}
}

/*
 * init_sys
 */
void init_sys(void)
{
	int  fd, i;
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
#ifndef _WIN32
#ifdef BACKEND_SELECT
	if (MAXCONNECTIONS > FD_SETSIZE)
	{
		fprintf(stderr, "MAXCONNECTIONS (%d) is higher than FD_SETSIZE (%d)\n", MAXCONNECTIONS, FD_SETSIZE);
		fprintf(stderr, "You might need to recompile the IRCd, or if you're running Linux, read the release notes\n");
		exit(-1);
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
#ifdef HAVE_SYSLOG
closelog(); /* temporary close syslog, as we mass close() fd's below... */
#endif
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
    defined(_POSIX_SOURCE) || defined(SVR4) || defined(SGI) || \
    defined(OSXTIGER) || defined(__QNX__)
	(void)setsid();
#else
	(void)setpgrp(0, (int)getpid());
#endif
#ifndef NOCLOSEFD
	(void)close(0);		/* fd 0 opened by inetd */
#endif
}
init_dgram:
#else
#ifndef NOCLOSEFD
	close(fileno(stdin));
	close(fileno(stdout));
	if (!(bootopt & BOOT_DEBUG))
	close(fileno(stderr));
#endif
#ifdef HAVE_SYSLOG
openlog("ircd", LOG_PID | LOG_NDELAY, LOG_DAEMON); /* reopened now */
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
	if ((fd = open(conf_files->pid_file, O_CREAT | O_WRONLY, 0600)) >= 0)
	{
		bzero(buff, sizeof(buff));
		(void)ircsprintf(buff, "%5d\n", (int)getpid());
		if (write(fd, buff, strlen(buff)) == -1)
			Debug((DEBUG_NOTICE, "Error writing to pid file %s",
			    conf_files->pid_file));
		(void)close(fd);
		return;
	}
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "Error opening pid file %s",
		    conf_files->pid_file));
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
		strlcpy(sockn, me.sockhost, HOSTLEN);
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
		strlcpy(sockn, "localhost", HOSTLEN);
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
void completed_connection(int fd, int revents, void *data)
{
	aClient *cptr = data;
	ConfigItem_link *aconf = cptr->serv ? cptr->serv->conf : NULL;
	extern char serveropts[];
	SetHandshake(cptr);

	if (!aconf)
	{
		sendto_ops("Lost configuration for %s", get_client_name(cptr, FALSE));
		return;
	}
	if (!BadPtr(aconf->connpwd))
		sendto_one(cptr, "PASS :%s", aconf->connpwd);

	send_protoctl_servers(cptr, 0);
	send_proto(cptr, aconf);
	if (NEW_LINKING_PROTOCOL)
	{
		/* Sending SERVER message moved to m_protoctl, so it's send after the first PROTOCTL
		 * we receive from the remote server. Of course, this assumes that the remote server
		 * to which we are connecting will at least send one PROTOCTL... but since it's an
		 * outgoing connect, we can safely assume it's a remote UnrealIRCd server (or some
		 * other advanced server..). -- Syzop
		 */

		/* Use this nasty hack, to make 3.2.9<->pre-3.2.9 linking work */
		sendto_one(cptr, "__PANGPANG__");
	} else {
		send_server_message(cptr);
	}
	if (!IsDead(cptr))
		start_auth(cptr);
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
	}

	if (cptr->authfd >= 0)
	{
		fd_close(cptr->authfd);
		cptr->authfd = -1;
		--OpenFiles;
	}

	if (cptr->fd >= 0)
	{
		send_queued(cptr);
		remove_local_client(cptr);
#ifdef USE_SSL
		if (IsSSL(cptr) && cptr->ssl) {
			SSL_set_shutdown((SSL *)cptr->ssl, SSL_RECEIVED_SHUTDOWN);
			SSL_smart_shutdown((SSL *)cptr->ssl);
			SSL_free((SSL *)cptr->ssl);
			cptr->ssl = NULL;
		}
#endif
		fd_close(cptr->fd);
		cptr->fd = -2;
		--OpenFiles;
		DBufClear(&cptr->sendQ);
		DBufClear(&cptr->recvQ);

	}

	cptr->from = NULL;	/* ...this should catch them! >:) --msa */

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
#if defined(INET6) && defined(IPV6_V6ONLY)
	/* We deal with both IPv4 and IPv6 in one (listen) socket.
	 * This used to be on by default, but FreeBSD, and much later Linux
	 * sometimes as well, seem to default it to IPv6 only ('1').
	 * We now have this new fancy option to turn it off in Unreal,
	 * instead of requiring our users to sysctl.
	 */
	opt = 0;
	setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (OPT_TYPE *)&opt, sizeof(opt));
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
aClient *add_connection(ConfigItem_listen *cptr, int fd)
{
	aClient *acptr;
	ConfigItem_ban *bconf;
	int i, j;
	acptr = make_client(NULL, &me);

	/* Removed preliminary access check. Full check is performed in
	 * m_server and m_user instead. Also connection time out help to
	 * get rid of unwanted connections.
	 */
	{
		struct SOCKADDR_IN addr;
		int  len = sizeof(struct SOCKADDR_IN);

		if (getpeername(fd, (struct SOCKADDR *)&addr, &len) == -1)
		{
			/* On Linux 2.4 and FreeBSD the socket may just have been disconnected
			 * so it's not a serious error and can happen quite frequently -- Syzop
			 */
			if (ERRNO != P_ENOTCONN)
				report_error("Failed in connecting to %s :%s", acptr);
add_con_refuse:
			ircstp->is_ref++;
			acptr->fd = -2;
			free_client(acptr);
			fd_close(fd);
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
				!bcmp(local[i]->ip.S_ADDR, acptr->ip.S_ADDR, sizeof(acptr->ip.S_ADDR)))
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
	if (acptr->listener != NULL)
		acptr->listener->clients++;
	add_client_to_list(acptr);

	set_non_blocking(acptr->fd, acptr);
	set_sock_opts(acptr->fd, acptr);
	IRCstats.unknown++;
#ifdef USE_SSL
	if (cptr->options & LISTENER_SSL)
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
	list_add(&acptr->lclient_node, &unknown_list);

	RunHook(HOOKTYPE_HANDSHAKE, acptr);

	if (!DONT_RESOLVE)
	{
		if (SHOWCONNECTINFO && !acptr->serv && !IsServersOnlyListener(acptr->listener))
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
			if (SHOWCONNECTINFO && !acptr->serv && !IsServersOnlyListener(acptr->listener))
				sendto_one(acptr, "%s", REPORT_FIN_DNSC);
		}
	}

doauth:
	start_auth(acptr);
	fd_setselect(acptr->fd, FD_SELECT_READ, read_packet, acptr);
}

void proceed_normal_client_handshake(aClient *acptr, struct hostent *he)
{
	ClearDNS(acptr);
	acptr->hostp = he;
	if (SHOWCONNECTINFO && !acptr->serv && !IsServersOnlyListener(acptr->listener))
		sendto_one(acptr, "%s", acptr->hostp ? REPORT_FIN_DNS : REPORT_FAIL_DNS);

	if (!dns_special_flag && !DoingAuth(acptr))
		finish_auth(acptr);
}

/*
** read_packet
**
** Read a 'packet' of data from a connection and process it.  Read in 8k
** chunks to give a better performance rating (for server connections).
** Do some tricky stuff for client connections to make sure they don't do
** any flooding >:-) -avalon
** If 'doread' is set to 0 then we don't actually read (no recv()),
** however we still check if we need to dequeue anything from the recvQ.
** This is necessary, since we may have put something on the recvQ due
** to fake lag. -- Syzop
** With new I/O code, things work differently.  Surprise!
** read_one_packet() reads packets in and dumps them as quickly as
** possible into the client's DBuf.  Then we parse data out of the DBuf,
** after we're done reading crap.
**    -- nenolod
*/
static void parse_client_queued(aClient *cptr)
{
	int dolen = 0;
	int allow_read;
	int done;
	time_t now = TStime();

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
				return;
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
			{
				exit_client(cptr, cptr, cptr,
				    "dbuf_getmsg fail");
				return;
			}
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
			return;
	}
}

void read_packet(int fd, int revents, void *data)
{
	aClient *cptr = data;
	int length = 0;
	time_t now = TStime();
	Hook *h;

	SET_ERRNO(0);

	while (1)
	{
#ifdef USE_SSL
		if (IsSSL(cptr) && cptr->ssl != NULL)
		{
			fd_setselect(fd, FD_SELECT_READ, read_packet, cptr);
			fd_setselect(fd, FD_SELECT_WRITE, NULL, cptr);

			length = SSL_read(cptr->ssl, readbuf, sizeof(readbuf));

			if (length < 0)
			{
				int err = SSL_get_error(cptr->ssl, length);
				ircd_log(LOG_ERROR, "error %d", err);

				switch (err)
				{
				case SSL_ERROR_WANT_WRITE:
					fd_setselect(fd, FD_SELECT_READ, NULL, cptr);
					fd_setselect(fd, FD_SELECT_WRITE, read_packet, cptr);
					break;
				case SSL_ERROR_WANT_READ:
					SET_ERRNO(P_EWOULDBLOCK);
					break;
				case SSL_ERROR_SYSCALL:
					break;
				case SSL_ERROR_SSL:
					if (ERRNO == P_EAGAIN)
						break;
				default:
					length = 0;
					SET_ERRNO(0);
					break;
				}
			}
		}
		else
#endif
			length = recv(cptr->fd, readbuf, sizeof(readbuf), 0);

		if (length <= 0)
		{
			if (length < 0 && (ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN || ERRNO == P_EINTR))
				return;

			exit_client(cptr, cptr, cptr, "Read error");
			return;
		}

		cptr->lasttime = now;
		if (cptr->lasttime > cptr->since)
			cptr->since = cptr->lasttime;
		cptr->flags &= ~(FLAGS_PINGSENT | FLAGS_NONL);

		for (h = Hooks[HOOKTYPE_RAWPACKET_IN]; h; h = h->next)
		{
			int v = (*(h->func.intfunc))(cptr, readbuf, length);
			if (v <= 0)
				return;
		}

		ircd_log(LOG_ERROR, "dbuf_put - length %d, ERRNO %d", length, ERRNO);
		dbuf_put(&cptr->recvQ, readbuf, length);

		/* parse some of what we have (inducing fakelag, etc) */
		if (!(DoingDNS(cptr) || DoingAuth(cptr)))
			parse_client_queued(cptr);

		/* excess flood check */
		if (IsPerson(cptr) && DBufLength(&cptr->recvQ) > get_recvq(cptr))
		{
			sendto_snomask(SNO_FLOOD,
			    "*** Flood -- %s!%s@%s (%d) exceeds %d recvQ",
			    cptr->name[0] ? cptr->name : "*",
			    cptr->user ? cptr->user->username : "*",
			    cptr->user ? cptr->user->realhost : "*",
			    DBufLength(&cptr->recvQ), get_recvq(cptr));
			exit_client(cptr, cptr, cptr, "Excess Flood");
			return;
		}

		/* bail on short read! */
		if (length < sizeof(readbuf))
			return;
	}
}

/* When auth is finished, go back and parse all prior input. */
void finish_auth(aClient *acptr)
{
	SetAccess(acptr);
	parse_client_queued(acptr);
}

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
	strlcpy(cptr->name, aconf->servername, sizeof(cptr->name));
	strlcpy(cptr->sockhost, aconf->hostname, HOSTLEN + 1);

	svp = connect_inet(aconf, cptr, &len);
	if (!svp)
	{
		if (cptr->fd >= 0)
		{
			fd_close(cptr->fd);
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
		fd_close(cptr->fd);
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
	SetConnecting(cptr);
	SetOutgoing(cptr);
	IRCstats.unknown++;
	get_sockhost(cptr, aconf->hostname);
	add_client_to_list(cptr);

#ifdef USE_SSL
	if (IsSSL(cptr) && (aconf->options & CONNECT_SSL))
		fd_setselect(cptr->fd, FD_SELECT_READ, ircd_SSL_client_handshake, cptr);
	else
#endif
		fd_setselect(cptr->fd, FD_SELECT_READ, completed_connection, cptr);
	return 0;
}

static struct SOCKADDR *connect_inet(ConfigItem_link *aconf, aClient *cptr, int *lenp)
{
	static struct SOCKADDR_IN server;
	struct hostent *hp;
	char buf[BUFSIZE];

	/*
	 * Might as well get sockhost from here, the connection is attempted
	 * with it so if it fails its useless.
	 */
	snprintf(buf, sizeof buf, "Outgoing connection: %s", get_client_name(cptr, TRUE));
	cptr->fd = fd_socket(AFINET, SOCK_STREAM, 0, buf);
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
