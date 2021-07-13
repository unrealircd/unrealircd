/*
 *   Unreal Internet Relay Chat Daemon, src/socket.c
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

/** @file
 * @brief Socket functions such as reading, writing, connecting.
 *
 * The actual data parsing functions (for incoming data) are in
 * src/parse.c.
 */

#include "unrealircd.h"
#include "dns.h"

int OpenFiles = 0;    /* GLOBAL - number of files currently open */
int readcalls = 0;

void completed_connection(int, int, void *);
void set_sock_opts(int, Client *, int);
void set_ipv6_opts(int);
void close_listener(ConfigItem_listen *listener);
static char readbuf[BUFSIZE];
char zlinebuf[BUFSIZE];
extern char *version;
MODVAR time_t last_allinuse = 0;

#ifdef USE_LIBCURL
extern void url_do_transfers_async(void);
#endif

void start_of_normal_client_handshake(Client *client);
void proceed_normal_client_handshake(Client *client, struct hostent *he);

/** Close all connections - only used when we terminate the server (eg: /DIE or SIGTERM) */
void close_connections(void)
{
	Client *client;

	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (client->local->fd >= 0)
		{
			fd_close(client->local->fd);
			client->local->fd = -2;
		}
	}

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (client->local->fd >= 0)
		{
			fd_close(client->local->fd);
			client->local->fd = -2;
		}

		if (client->local->authfd >= 0)
		{
			fd_close(client->local->authfd);
			client->local->fd = -1;
		}
	}

	close_unbound_listeners();

	OpenFiles = 0;

#ifdef _WIN32
	WSACleanup();
#endif
}

/** Report an error to the log and also send to all local opers.
 * @param text		Format string for outputting the error.
 *			It must contain only two '%s'. The first
 *			one is replaced by the sockhost from the
 *			client, and the latter will be the error
 *			message from strerror(errno).
 * @param client	The client - ALWAYS locally connected.
 */
void report_error(char *text, Client *client)
{
	int errtmp = ERRNO, origerr = ERRNO;
	char *host, xbuf[256];
	int  err, len = sizeof(err), n;
	
	host = (client) ? get_client_name(client, FALSE) : "";

	Debug((DEBUG_ERROR, text, host, STRERROR(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (client && !IsMe(client) && client->local->fd >= 0)
		if (!getsockopt(client->local->fd, SOL_SOCKET, SO_ERROR, (void *)&err, &len))
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

/** Report a BAD error to the log and also send to all local opers.
 * TODO: Document the difference between report_error() and report_baderror()
 * @param text		Format string for outputting the error.
 *			It must contain only two '%s'. The first
 *			one is replaced by the sockhost from the
 *			client, and the latter will be the error
 *			message from strerror(errno).
 * @param client	The client - ALWAYS locally connected.
 */
void report_baderror(char *text, Client *client)
{
#ifndef _WIN32
	int  errtmp = errno;	/* debug may change 'errno' */
#else
	int  errtmp = WSAGetLastError();	/* debug may change 'errno' */
#endif
	char *host;
	int  err, len = sizeof(err);

	host = (client) ? get_client_name(client, FALSE) : "";

	Debug((DEBUG_ERROR, text, host, STRERROR(errtmp)));

	/*
	 * Get the *real* error from the socket (well try to anyway..).
	 * This may only work when SO_DEBUG is enabled but its worth the
	 * gamble anyway.
	 */
#ifdef	SO_ERROR
	if (client && !IsMe(client) && client->local->fd >= 0)
		if (!getsockopt(client->local->fd, SOL_SOCKET, SO_ERROR, (void *)&err, &len))
			if (err)
				errtmp = err;
#endif
	sendto_umode(UMODE_OPER, text, host, STRERROR(errtmp));
	ircd_log(LOG_ERROR, text, host, STRERROR(errtmp));
	return;
}

/** Accept an incoming connection.
 * @param listener_fd	The file descriptor of a listen() socket.
 * @param data		The listen { } block configuration data.
 */
static void listener_accept(int listener_fd, int revents, void *data)
{
	ConfigItem_listen *listener = data;
	int cli_fd;

	if ((cli_fd = fd_accept(listener->fd)) < 0)
	{
		if ((ERRNO != P_EWOULDBLOCK) && (ERRNO != P_ECONNABORTED))
		{
			/* Trouble! accept() returns a strange error.
			 * Previously in such a case we would just log/broadcast the error and return,
			 * causing this message to be triggered at a rate of XYZ per second (100% CPU).
			 * Now we close & re-start the listener.
			 * Of course the underlying cause of this issue should be investigated, as this
			 * is very much a workaround.
			 */
			report_baderror("Cannot accept connections %s:%s", NULL);
			sendto_realops("[BUG] Restarting listener on %s:%d due to fatal errors (see previous message)", listener->ip, listener->port);
			close_listener(listener);
			start_listeners();
		}
		return;
	}

	ircstats.is_ac++;

	set_sock_opts(cli_fd, NULL, listener->ipv6);

	if ((++OpenFiles >= maxclients) || (cli_fd >= maxclients))
	{
		ircstats.is_ref++;
		if (last_allinuse < TStime() - 15)
		{
			sendto_ops_and_log("All connections in use. ([@%s/%u])", listener->ip, listener->port);
			last_allinuse = TStime();
		}

		(void)send(cli_fd, "ERROR :All connections in use\r\n", 31, 0);

		fd_close(cli_fd);
		--OpenFiles;
		return;
	}

	/* add_connection() may fail. we just don't care. */
	add_connection(listener, cli_fd);
}

/** Create a listener port.
 * @param listener	The listen { } block configuration
 * @param ip		IP address to bind on
 * @param port		Port to bind on
 * @param ipv6		IPv6 (1) or IPv4 (0)
 * @returns 0 on success and <0 on error. Yeah, confusing.
 */
int unreal_listen(ConfigItem_listen *listener, char *ip, int port, int ipv6)
{
	if (BadPtr(ip))
		ip = "*";
	
	if (*ip == '*')
	{
		if (ipv6)
			ip = "::";
		else
			ip = "0.0.0.0";
	}

	/* At first, open a new socket */
	if (listener->fd >= 0)
		abort(); /* Socket already exists but we are asked to create and listen on one. Bad! */
	
	if (port == 0)
		abort(); /* Impossible as well, right? */

	listener->fd = fd_socket(ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0, "Listener socket");
	if (listener->fd < 0)
	{
		report_baderror("Cannot open stream socket() %s:%s", NULL);
		return -1;
	}

	if (++OpenFiles >= maxclients)
	{
		sendto_ops_and_log("No more connections allowed (%s)", listener->ip);
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	set_sock_opts(listener->fd, NULL, ipv6);

	if (!unreal_bind(listener->fd, ip, port, ipv6))
	{
		char buf[512];
		ircsnprintf(buf, sizeof(buf), "Error binding stream socket to IP %s port %d", ip, port);
		strlcat(buf, " - %s:%s", sizeof(buf));
		report_baderror(buf, NULL);
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	if (listen(listener->fd, LISTEN_SIZE) < 0)
	{
		report_error("listen failed for %s:%s", NULL);
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

#ifdef TCP_DEFER_ACCEPT
	if (listener->options & LISTENER_DEFER_ACCEPT)
	{
		int yes = 1;

		(void)setsockopt(listener->fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &yes, sizeof(int));
	}
#endif

#ifdef SO_ACCEPTFILTER
	if (listener->options & LISTENER_DEFER_ACCEPT)
	{
		struct accept_filter_arg afa;

		memset(&afa, '\0', sizeof afa);
		strlcpy(afa.af_name, "dataready", sizeof afa.af_name);
		(void)setsockopt(listener->fd, SOL_SOCKET, SO_ACCEPTFILTER, &afa, sizeof afa);
	}
#endif

	fd_setselect(listener->fd, FD_SELECT_READ, listener_accept, listener);

	return 0;
}

/** Activate a listen { } block */
int add_listener(ConfigItem_listen *conf)
{
	if (unreal_listen(conf, conf->ip, conf->port, conf->ipv6))
	{
		/* Error is already handled upstream */
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

/** Close the listener socket, but do not free it (yet).
 * This will only close the socket so no new clients are accepted.
 * It also marks the listener as no longer "bound".
 * Once the last client exits the listener will actually be freed.
 * @param listener	The listen { } block.
 */
void close_listener(ConfigItem_listen *listener)
{
	if (listener->fd >= 0)
	{
		ircd_log(LOG_ERROR, "IRCd no longer listening on %s:%d (%s)%s",
			listener->ip, listener->port,
			listener->ipv6 ? "IPv6" : "IPv4",
			listener->options & LISTENER_TLS ? " (SSL/TLS)" : "");
		fd_close(listener->fd);
		--OpenFiles;
	}

	listener->options &= ~LISTENER_BOUND;
	listener->fd = -1;
	/* We can already free the SSL/TLS context, since it is only
	 * used for new connections, which we no longer accept.
	 */
	if (listener->ssl_ctx)
	{
		SSL_CTX_free(listener->ssl_ctx);
		listener->ssl_ctx = NULL;
	}
}

/** Close all listeners that were pending to be closed. */
void close_unbound_listeners(void)
{
	ConfigItem_listen *aconf, *aconf_next;

	/* close all 'extra' listening ports we have */
	for (aconf = conf_listen; aconf != NULL; aconf = aconf_next)
	{
		aconf_next = aconf->next;
		if (aconf->flag.temporary)
			close_listener(aconf);
	}
}

int maxclients = 1024 - CLIENTS_RESERVE;

/** Check the maximum number of sockets (users) that we can handle - called on startup.
 */
void check_user_limit(void)
{
#ifdef RLIMIT_FD_MAX
	struct rlimit limit;
	long m;

	if (!getrlimit(RLIMIT_FD_MAX, &limit))
	{
		if (limit.rlim_max < MAXCONNECTIONS)
			m = limit.rlim_max;
		else
			m = MAXCONNECTIONS;

		/* Adjust soft limit (if necessary, which is often the case) */
		if (m != limit.rlim_cur)
		{
			limit.rlim_cur = limit.rlim_max = m;
			if (setrlimit(RLIMIT_FD_MAX, &limit) == -1)
			{
				/* HACK: if it's mac os X then don't error... */
#ifndef OSXTIGER
				fprintf(stderr, "error setting maximum number of open files to %ld\n",
					(long)limit.rlim_cur);
				exit(-1);
#endif // OSXTIGER
			}
		}
		/* This can only happen if it is due to resource limits (./Config already rejects <100) */
		if (m < 100)
		{
			fprintf(stderr, "\nERROR: Your OS has a limit placed on this account.\n"
			                "This machine only allows UnrealIRCd to handle a maximum of %ld open connections/files, which is VERY LOW.\n"
			                "Please check with your system administrator to bump this limit.\n"
			                "The recommended ulimit -n setting is at least 1024 and "
			                "preferably 4096.\n"
			                "Note that this error is often seen on small web shells that are not meant for running IRC servers.\n",
			                m);
			exit(-1);
		}
		maxclients = m - CLIENTS_RESERVE;
	}
#endif // RLIMIT_FD_MAX

#ifndef _WIN32
#ifdef BACKEND_SELECT
	if (MAXCONNECTIONS > FD_SETSIZE)
	{
		fprintf(stderr, "MAXCONNECTIONS (%d) is higher than FD_SETSIZE (%d)\n", MAXCONNECTIONS, FD_SETSIZE);
		fprintf(stderr, "You should not see this error on Linux or FreeBSD\n");
		fprintf(stderr, "You might need to recompile the IRCd and answer a lower value to the MAXCONNECTIONS question in ./Config\n");
		exit(-1);
	}
#endif
#endif
#ifdef _WIN32
	maxclients = MAXCONNECTIONS - CLIENTS_RESERVE;
#endif
}

/** Initialize some systems - called on startup */
void init_sys(void)
{
#ifndef _WIN32
	/* Create new session / set process group */
	(void)setsid();
#endif

	init_resolver(1);
	return;
}

/** Replace a file descriptor (*NIX only).
 * See close_std_descriptors() as for why.
 * @param oldfd: the old FD to close and re-use
 * @param name: descriptive string of the old fd, eg: "stdin".
 * @param mode: an open() mode, such as O_WRONLY.
 */
void replacefd(int oldfd, char *name, int mode)
{
#ifndef _WIN32
	int newfd = open("/dev/null", mode);
	if (newfd < 0)
	{
		fprintf(stderr, "Warning: could not open /dev/null\n");
		return;
	}
	if (oldfd < 0)
	{
		fprintf(stderr, "Warning: could not replace %s (invalid fd)\n", name);
		return;
	}
	if (dup2(newfd, oldfd) < 0)
	{
		fprintf(stderr, "Warning: could not replace %s (dup2 error)\n", name);
		return;
	}
#endif
}

/** Mass close standard file descriptors (stdin, stdout, stderr).
 * We used to really just close them here (or in init_sys() actually),
 * making the fd's available for other purposes such as internet sockets.
 * For safety we now dup2() them to /dev/null. This in case someone
 * accidentally does a fprintf(stderr,..) somewhere in the code or some
 * library outputs error messages to stderr (such as libc with heap
 * errors). We don't want any IRC client to receive such a thing!
 */
void close_std_descriptors(void)
{
#if !defined(_WIN32) && !defined(NOCLOSEFD)
	replacefd(fileno(stdin), "stdin", O_RDONLY);
	replacefd(fileno(stdout), "stdout", O_WRONLY);
	replacefd(fileno(stderr), "stderr", O_WRONLY);
#endif
}

/** Write PID file */
void write_pidfile(void)
{
#ifdef IRCD_PIDFILE
	int fd;
	char buff[20];
	if ((fd = open(conf_files->pid_file, O_CREAT | O_WRONLY, 0600)) < 0)
	{
		ircd_log(LOG_ERROR, "Error writing to pid file %s: %s", conf_files->pid_file, strerror(ERRNO));
		return;
	}
	ircsnprintf(buff, sizeof(buff), "%5d\n", (int)getpid());
	if (write(fd, buff, strlen(buff)) < 0)
		ircd_log(LOG_ERROR, "Error writing to pid file %s: %s", conf_files->pid_file, strerror(ERRNO));
	if (close(fd) < 0)
		ircd_log(LOG_ERROR, "Error writing to pid file %s: %s", conf_files->pid_file, strerror(ERRNO));
#endif
}

/** Reject an insecure (outgoing) server link that isn't SSL/TLS.
 * This function is void and not int because it can be called from other void functions
 */
void reject_insecure_server(Client *client)
{
	sendto_umode(UMODE_OPER, "Could not link with server %s with SSL/TLS enabled. "
	                         "Please check logs on the other side of the link. "
	                         "If you insist with insecure linking then you can set link::options::outgoing::insecure "
	                         "(NOT recommended!).",
	                         client->name);
	dead_socket(client, "Rejected link without SSL/TLS");
}

/** Start server handshake - called after the outgoing connection has been established.
 * @param client	The remote server
 */
void start_server_handshake(Client *client)
{
	ConfigItem_link *aconf = client->serv ? client->serv->conf : NULL;

	if (!aconf)
	{
		/* Should be impossible. */
		sendto_ops_and_log("Lost configuration for %s in start_server_handshake()", get_client_name(client, FALSE));
		return;
	}

	RunHook(HOOKTYPE_SERVER_HANDSHAKE_OUT, client);

	sendto_one(client, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");

	send_protoctl_servers(client, 0);
	send_proto(client, aconf);
	/* Sending SERVER message moved to cmd_protoctl, so it's send after the first PROTOCTL
	 * that we receive from the remote server. -- Syzop
	 */
}

/** Do an ident lookup if necessary.
 * @param client	The incoming client
 */
void consider_ident_lookup(Client *client)
{
	char buf[BUFSIZE];

	/* If ident checking is disabled or it's an outgoing connect, then no ident check */
	if ((IDENT_CHECK == 0) || (client->serv && IsHandshake(client)))
	{
		ClearIdentLookupSent(client);
		ClearIdentLookup(client);
		return;
	}
	RunHook(HOOKTYPE_IDENT_LOOKUP, client);

	return;
}


/** Called when TCP/IP connection is established (outgoing server connect) */
void completed_connection(int fd, int revents, void *data)
{
	Client *client = data;
	ConfigItem_link *aconf = client->serv ? client->serv->conf : NULL;

	if (IsHandshake(client))
	{
		/* Due to delayed ircd_SSL_connect call */
		start_server_handshake(client);
		fd_setselect(fd, FD_SELECT_READ, read_packet, client);
		return;
	}

	SetHandshake(client);

	if (!aconf)
	{
		sendto_ops_and_log("Lost configuration for %s", get_client_name(client, FALSE));
		return;
	}

	if (!client->local->ssl && !(aconf->outgoing.options & CONNECT_INSECURE))
	{
		sendto_one(client, NULL, "STARTTLS");
	} else
	{
		start_server_handshake(client);
	}

	if (!IsDeadSocket(client))
		consider_ident_lookup(client);

	fd_setselect(fd, FD_SELECT_READ, read_packet, client);
}

/** Close the physical connection.
 * @param client	The client connection to close (LOCAL!)
 */
void close_connection(Client *client)
{
	RunHook(HOOKTYPE_CLOSE_CONNECTION, client);
	/* This function must make MyConnect(client) == FALSE,
	 * and set client->direction == NULL.
	 */
	if (IsServer(client))
	{
		ircstats.is_sv++;
		ircstats.is_sbs += client->local->sendB;
		ircstats.is_sbr += client->local->receiveB;
		ircstats.is_sks += client->local->sendK;
		ircstats.is_skr += client->local->receiveK;
		ircstats.is_sti += TStime() - client->local->firsttime;
		if (ircstats.is_sbs > 1023)
		{
			ircstats.is_sks += (ircstats.is_sbs >> 10);
			ircstats.is_sbs &= 0x3ff;
		}
		if (ircstats.is_sbr > 1023)
		{
			ircstats.is_skr += (ircstats.is_sbr >> 10);
			ircstats.is_sbr &= 0x3ff;
		}
	}
	else if (IsUser(client))
	{
		ircstats.is_cl++;
		ircstats.is_cbs += client->local->sendB;
		ircstats.is_cbr += client->local->receiveB;
		ircstats.is_cks += client->local->sendK;
		ircstats.is_ckr += client->local->receiveK;
		ircstats.is_cti += TStime() - client->local->firsttime;
		if (ircstats.is_cbs > 1023)
		{
			ircstats.is_cks += (ircstats.is_cbs >> 10);
			ircstats.is_cbs &= 0x3ff;
		}
		if (ircstats.is_cbr > 1023)
		{
			ircstats.is_ckr += (ircstats.is_cbr >> 10);
			ircstats.is_cbr &= 0x3ff;
		}
	}
	else
		ircstats.is_ni++;

	/*
	 * remove outstanding DNS queries.
	 */
	unrealdns_delreq_bycptr(client);

	if (client->local->authfd >= 0)
	{
		fd_close(client->local->authfd);
		client->local->authfd = -1;
		--OpenFiles;
	}

	if (client->local->fd >= 0)
	{
		send_queued(client);
		if (IsTLS(client) && client->local->ssl) {
			SSL_set_shutdown(client->local->ssl, SSL_RECEIVED_SHUTDOWN);
			SSL_smart_shutdown(client->local->ssl);
			SSL_free(client->local->ssl);
			client->local->ssl = NULL;
		}
		fd_close(client->local->fd);
		client->local->fd = -2;
		--OpenFiles;
		DBufClear(&client->local->sendQ);
		DBufClear(&client->local->recvQ);

	}

	client->direction = NULL;
}

/** Set IPv6 socket options, if possible. */
void set_ipv6_opts(int fd)
{
#if defined(IPV6_V6ONLY)
	int opt = 1;
	(void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&opt, sizeof(opt));
#endif
}

/** This sets the *OS* socket buffers.
 * This shouldn't be needed anymore, but I've left the function here.
 */
void set_socket_buffers(int fd, int rcvbuf, int sndbuf)
{
	int opt;

	opt = rcvbuf;
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&opt, sizeof(opt));

	opt = sndbuf;
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&opt, sizeof(opt));
}

/** Set the appropriate socket options */
void set_sock_opts(int fd, Client *client, int ipv6)
{
	int opt;

	if (ipv6)
		set_ipv6_opts(fd);

#ifdef SO_REUSEADDR
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt)) < 0)
			report_error("setsockopt(SO_REUSEADDR) %s:%s", client);
#endif

#if defined(SO_USELOOPBACK) && !defined(_WIN32)
	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_USELOOPBACK, (void *)&opt, sizeof(opt)) < 0)
		report_error("setsockopt(SO_USELOOPBACK) %s:%s", client);
#endif

	/* Previously we also called set_socket_buffers() to set some
	 * specific buffer limits. This is no longer needed on modern OS's.
	 * Setting it explicitly actually slows things down.
	 */

	/* Set to non blocking: */
#if !defined(_WIN32)
	if ((opt = fcntl(fd, F_GETFL, 0)) == -1)
	{
		if (client)
		{
			report_error("fcntl(fd, F_GETFL) failed for %s:%s", client);
		}
	}
	else if (fcntl(fd, F_SETFL, opt | O_NONBLOCK) == -1)
	{
		if (client)
		{
			report_error("fcntl(fd, F_SETL, nonb) failed for %s:%s", client);
		}
	}
#else
	opt = 1;
	if (ioctlsocket(fd, FIONBIO, &opt) < 0)
	{
		if (client)
		{
			report_error("ioctlsocket(fd,FIONBIO) failed for %s:%s", client);
		}
	}
#endif
}

/** Returns 1 if using a loopback IP (127.0.0.1) or
 * using a local IP number on the same machine (effectively the same;
 * no network traffic travels outside this machine).
 * @param ip	The IP address to check
 * @returns 1 if loopback, 0 if not.
 */
int is_loopback_ip(char *ip)
{
	ConfigItem_listen *e;

	if (!strcmp(ip, "127.0.0.1") || !strcmp(ip, "0:0:0:0:0:0:0:1") || !strcmp(ip, "0:0:0:0:0:ffff:127.0.0.1"))
		return 1;

	for (e = conf_listen; e; e = e->next)
	{
		if ((e->options & LISTENER_BOUND) && !strcmp(ip, e->ip))
			return 1;
	}
	return 0;
}

/** Retrieve the remote IP address and port of a socket.
 * @param client	Client to check
 * @param fd		File descriptor
 * @param port		Remote port (will be written)
 * @returns The IP address
 */
char *getpeerip(Client *client, int fd, int *port)
{
	static char ret[HOSTLEN+1];

	if (IsIPV6(client))
	{
		struct sockaddr_in6 addr;
		int len = sizeof(addr);

		if (getpeername(fd, (struct sockaddr *)&addr, &len) < 0)
			return NULL;
		*port = ntohs(addr.sin6_port);
		return inetntop(AF_INET6, &addr.sin6_addr.s6_addr, ret, sizeof(ret));
	} else
	{
		struct sockaddr_in addr;
		int len = sizeof(addr);

		if (getpeername(fd, (struct sockaddr *)&addr, &len) < 0)
			return NULL;
		*port = ntohs(addr.sin_port);
		return inetntop(AF_INET, &addr.sin_addr.s_addr, ret, sizeof(ret));
	}
}

/** This checks set::max-unknown-connections-per-ip,
 * which is an important safety feature.
 */
static int check_too_many_unknown_connections(Client *client)
{
	int cnt = 1;
	Client *c;

	if (!find_tkl_exception(TKL_CONNECT_FLOOD, client))
	{
		list_for_each_entry(c, &unknown_list, lclient_node)
		{
			if (!strcmp(client->ip,GetIP(c)))
			{
				cnt++;
				if (cnt > iConf.max_unknown_connections_per_ip)
					return 1;
			}
		}
	}

	return 0;
}

/** Process the incoming connection which has just been accepted.
 * This creates a client structure for the user.
 * The sockhost field is initialized with the ip# of the host.
 * The client is added to the linked list of clients but isnt added to any
 * hash tables yuet since it doesnt have a name.
 * @param listener	The listen { } block on which the client was accepted.
 * @param fd		The file descriptor of the client
 * @returns The new client, or NULL in case of trouble.
 * @note  When NULL is returned, the client at socket 'fd' will be
 *        closed by this function and OpenFiles is adjusted appropriately.
 */
Client *add_connection(ConfigItem_listen *listener, int fd)
{
	Client *client;
	char *ip;
	int port = 0;
	
	client = make_client(NULL, &me);

	/* If listener is IPv6 then mark client (client) as IPv6 */
	if (listener->ipv6)
		SetIPV6(client);

	ip = getpeerip(client, fd, &port);
	
	if (!ip)
	{
		/* On Linux 2.4 and FreeBSD the socket may just have been disconnected
		 * so it's not a serious error and can happen quite frequently -- Syzop
		 */
		if (ERRNO != P_ENOTCONN)
		{
			report_error("Failed to accept new client %s :%s", client);
		}
refuse_client:
			ircstats.is_ref++;
			client->local->fd = -2;
			free_client(client);
			fd_close(fd);
			--OpenFiles;
			return NULL;
	}

	/* Fill in sockhost & ip ASAP */
	set_sockhost(client, ip);
	safe_strdup(client->ip, ip);
	client->local->port = port;
	client->local->fd = fd;

	/* Tag loopback connections */
	if (is_loopback_ip(client->ip))
	{
		ircstats.is_loc++;
		SetLocalhost(client);
	}

	/* Check set::max-unknown-connections-per-ip */
	if (check_too_many_unknown_connections(client))
	{
		ircsnprintf(zlinebuf, sizeof(zlinebuf),
		            "ERROR :Closing Link: [%s] (Too many unknown connections from your IP)\r\n",
		            client->ip);
		(void)send(fd, zlinebuf, strlen(zlinebuf), 0);
		goto refuse_client;
	}

	/* Check (G)Z-Lines and set::anti-flood::connect-flood */
	if (check_banned(client, NO_EXIT_CLIENT))
		goto refuse_client;

	client->local->listener = listener;
	if (client->local->listener != NULL)
		client->local->listener->clients++;
	add_client_to_list(client);

	irccounts.unknown++;
	client->status = CLIENT_STATUS_UNKNOWN;

	list_add(&client->lclient_node, &unknown_list);

	if ((listener->options & LISTENER_TLS) && ctx_server)
	{
		SSL_CTX *ctx = listener->ssl_ctx ? listener->ssl_ctx : ctx_server;

		if (ctx)
		{
			SetTLSAcceptHandshake(client);
			Debug((DEBUG_DEBUG, "Starting TLS accept handshake for %s", client->local->sockhost));
			if ((client->local->ssl = SSL_new(ctx)) == NULL)
			{
				goto refuse_client;
			}
			SetTLS(client);
			SSL_set_fd(client->local->ssl, fd);
			SSL_set_nonblocking(client->local->ssl);
			SSL_set_ex_data(client->local->ssl, ssl_client_index, client);
			if (!ircd_SSL_accept(client, fd))
			{
				Debug((DEBUG_DEBUG, "Failed TLS accept handshake in instance 1: %s", client->local->sockhost));
				SSL_set_shutdown(client->local->ssl, SSL_RECEIVED_SHUTDOWN);
				SSL_smart_shutdown(client->local->ssl);
				SSL_free(client->local->ssl);
				goto refuse_client;
			}
		}
	}
	else
		start_of_normal_client_handshake(client);
	return client;
}

static int dns_special_flag = 0; /* This is for an "interesting" race condition  very ugly. */

/** Start of normal client handshake - DNS and ident lookups, etc.
 * @param client	The client
 * @note This is called directly after accept() -> add_connection() for plaintext.
 *       For SSL/TLS connections this is called after the SSL/TLS handshake is completed.
 */
void start_of_normal_client_handshake(Client *client)
{
	struct hostent *he;

	client->status = CLIENT_STATUS_UNKNOWN; /* reset, to be sure (TLS handshake has ended) */

	RunHook(HOOKTYPE_HANDSHAKE, client);

	if (!DONT_RESOLVE)
	{
		if (should_show_connect_info(client))
			sendto_one(client, NULL, ":%s %s", me.name, REPORT_DO_DNS);
		dns_special_flag = 1;
		he = unrealdns_doclient(client);
		dns_special_flag = 0;

		if (client->local->hostp)
			goto doauth; /* Race condition detected, DNS has been done, continue with auth */

		if (!he)
		{
			/* Resolving in progress */
			SetDNSLookup(client);
		} else {
			/* Host was in our cache */
			client->local->hostp = he;
			if (should_show_connect_info(client))
				sendto_one(client, NULL, ":%s %s", me.name, REPORT_FIN_DNSC);
		}
	}

doauth:
	consider_ident_lookup(client);
	fd_setselect(client->local->fd, FD_SELECT_READ, read_packet, client);
}

/** Called when DNS lookup has been completed and we can proceed with the client handshake.
 * @param client	The client
 * @param he		The resolved or unresolved host
 */
void proceed_normal_client_handshake(Client *client, struct hostent *he)
{
	ClearDNSLookup(client);
	client->local->hostp = he;
	if (should_show_connect_info(client))
	{
		sendto_one(client, NULL, ":%s %s",
		           me.name,
		           client->local->hostp ? REPORT_FIN_DNS : REPORT_FAIL_DNS);
	}
}

/** Read a packet from a client.
 * @param fd		File descriptor
 * @param revents	Read events (ignored)
 * @param data		Associated data (the client)
 */
void read_packet(int fd, int revents, void *data)
{
	Client *client = data;
	int length = 0;
	time_t now = TStime();
	Hook *h;
	int processdata;

	/* Don't read from dead sockets */
	if (IsDeadSocket(client))
	{
		fd_setselect(fd, FD_SELECT_READ, NULL, client);
		return;
	}

	SET_ERRNO(0);

	fd_setselect(fd, FD_SELECT_READ, read_packet, client);
	/* Restore handling of writes towards send_queued_cb(), since
	 * it may be overwritten in an earlier call to read_packet(),
	 * to handle (SSL) writes by read_packet(), see below under
	 * SSL_ERROR_WANT_WRITE.
	 */
	fd_setselect(fd, FD_SELECT_WRITE, send_queued_cb, client);

	while (1)
	{
		if (IsTLS(client) && client->local->ssl != NULL)
		{
			length = SSL_read(client->local->ssl, readbuf, sizeof(readbuf));

			if (length < 0)
			{
				int err = SSL_get_error(client->local->ssl, length);

				switch (err)
				{
				case SSL_ERROR_WANT_WRITE:
					fd_setselect(fd, FD_SELECT_READ, NULL, client);
					fd_setselect(fd, FD_SELECT_WRITE, read_packet, client);
					length = -1;
					SET_ERRNO(P_EWOULDBLOCK);
					break;
				case SSL_ERROR_WANT_READ:
					fd_setselect(fd, FD_SELECT_READ, read_packet, client);
					length = -1;
					SET_ERRNO(P_EWOULDBLOCK);
					break;
				case SSL_ERROR_SYSCALL:
					break;
				case SSL_ERROR_SSL:
					if (ERRNO == P_EAGAIN)
						break;
				default:
					/*length = 0;
					SET_ERRNO(0);
					^^ why this? we should error. -- todo: is errno correct?
					*/
					break;
				}
			}
		}
		else
			length = recv(client->local->fd, readbuf, sizeof(readbuf), 0);

		if (length <= 0)
		{
			if (length < 0 && ((ERRNO == P_EWOULDBLOCK) || (ERRNO == P_EAGAIN) || (ERRNO == P_EINTR)))
				return;

			if (IsServer(client) || client->serv) /* server or outgoing connection */
				lost_server_link(client, "Read error or connection closed.");

			exit_client(client, NULL, "Read error");
			return;
		}

		client->local->lasttime = now;
		if (client->local->lasttime > client->local->since)
			client->local->since = client->local->lasttime;
		/* FIXME: Is this correct? I have my doubts. */
		ClearPingSent(client);

		ClearPingWarning(client);

		processdata = 1;
		for (h = Hooks[HOOKTYPE_RAWPACKET_IN]; h; h = h->next)
		{
			processdata = (*(h->func.intfunc))(client, readbuf, &length);
			if (processdata == 0)
				break; /* if hook tells to ignore the data, then break now */
			if (processdata < 0)
				return; /* if hook tells client is dead, return now */
		}

		if (processdata && !process_packet(client, readbuf, length, 0))
			return;

		/* bail on short read! */
		if (length < sizeof(readbuf))
			return;
	}
}

/** Process input from clients that may have been deliberately delayed due to fake lag */
void process_clients(void)
{
	Client *client;
        
	/* Problem:
	 * When processing a client, that current client may exit due to eg QUIT.
	 * Similarly, current->next may be killed due to /KILL.
	 * When a client is killed, in the past we were not allowed to touch it anymore
	 * so that was a bit problematic. Now we can touch current->next, but it may
	 * have been removed from the lclient_list or unknown_list.
	 * In other words, current->next->next may be NULL even though there are more
	 * clients on the list.
	 * This is why the whole thing is wrapped in an additional do { } while() loop
	 * to make sure we re-run the list if we ended prematurely.
	 * We could use some kind of 'tagging' to mark already processed clients.
	 * However, parse_client_queued() already takes care not to read (fake) lagged
	 * clients, and we don't actually read/recv anything in the meantime, so clients
	 * in the beginning of the list won't benefit, they won't get higher prio.
	 * Another alternative is not to run the loop again, but that WOULD be
	 * unfair to clients later in the list which wouldn't be processed then
	 * under a heavy (kill) load scenario.
	 * I think the chosen solution is best, though it remains silly. -- Syzop
	 */

	do {
		list_for_each_entry(client, &lclient_list, lclient_node)
		{
			if ((client->local->fd >= 0) && DBufLength(&client->local->recvQ) && !IsDead(client))
			{
				parse_client_queued(client);
				if (IsDead(client))
					break;
			}
		}
	} while(&client->lclient_node != &lclient_list);

	do {
		list_for_each_entry(client, &unknown_list, lclient_node)
		{
			if ((client->local->fd >= 0) && DBufLength(&client->local->recvQ) && !IsDead(client))
			{
				parse_client_queued(client);
				if (IsDead(client) || (client->status > CLIENT_STATUS_UNKNOWN))
					break;
			}
		}
	} while(&client->lclient_node != &unknown_list);
}

/** Returns 4 if 'str' is a valid IPv4 address
 * and 6 if 'str' is a valid IPv6 IP address.
 * Zero (0) is returned in any other case (eg: hostname).
 */
int is_valid_ip(char *str)
{
	char scratch[64];
	
	if (inet_pton(AF_INET, str, scratch) == 1)
		return 4; /* IPv4 */
	
	if (inet_pton(AF_INET6, str, scratch) == 1)
		return 6; /* IPv6 */
	
	return 0; /* not an IP address */
}

static int connect_server_helper(ConfigItem_link *, Client *);

/** Start an outgoing connection to a server, for server linking.
 * @param aconf		Configuration attached to this server
 * @param by		The user initiating the connection (can be NULL)
 * @param hp		The address to connect to.
 * @returns <0 on error, 0 on success. Rather confusing.
 */
int connect_server(ConfigItem_link *aconf, Client *by, struct hostent *hp)
{
	Client *client;

#ifdef DEBUGMODE
	sendto_realops("connect_server() called with aconf %p, refcount: %d, TEMP: %s",
		aconf, aconf->refcount, aconf->flag.temporary ? "YES" : "NO");
#endif

	if (!aconf->outgoing.hostname)
		return -1; /* This is an incoming-only link block. Caller shouldn't call us. */
		
	if (!hp)
	{
		/* Remove "cache" */
		safe_free(aconf->connect_ip);
	}
	/*
	 * If we dont know the IP# for this host and itis a hostname and
	 * not a ip# string, then try and find the appropriate host record.
	 */
	if (!aconf->connect_ip)
	{
		if (is_valid_ip(aconf->outgoing.hostname))
		{
			/* link::outgoing::hostname is an IP address. No need to resolve host. */
			safe_strdup(aconf->connect_ip, aconf->outgoing.hostname);
		} else
		{
			/* It's a hostname, let the resolver look it up. */
			int ipv4_explicit_bind = 0;

			if (aconf->outgoing.bind_ip && (is_valid_ip(aconf->outgoing.bind_ip) == 4))
				ipv4_explicit_bind = 1;
			
			/* We need this 'aconf->refcount++' or else there's a race condition between
			 * starting resolving the host and the result of the resolver (we could
			 * REHASH in that timeframe) leading to an invalid (freed!) 'aconf'.
			 * -- Syzop, bug #0003689.
			 */
			aconf->refcount++;
			unrealdns_gethostbyname_link(aconf->outgoing.hostname, aconf, ipv4_explicit_bind);
			return -2;
		}
	}
	client = make_client(NULL, &me);
	client->local->hostp = hp;
	/*
	 * Copy these in so we have something for error detection.
	 */
	strlcpy(client->name, aconf->servername, sizeof(client->name));
	strlcpy(client->local->sockhost, aconf->outgoing.hostname, HOSTLEN + 1);

	if (!connect_server_helper(aconf, client))
	{
		int errtmp = ERRNO;
		report_error("Connect to host %s failed: %s", client);
		if (by && IsUser(by) && !MyUser(by))
			sendnotice(by, "*** Connect to host %s failed.", client->name);
		fd_close(client->local->fd);
		--OpenFiles;
		client->local->fd = -2;
		free_client(client);
		SET_ERRNO(errtmp);
		if (ERRNO == P_EINTR)
			SET_ERRNO(P_ETIMEDOUT);
		return -1;
	}
	/* The socket has been connected or connect is in progress. */
	make_server(client);
	client->serv->conf = aconf;
	client->serv->conf->refcount++;
#ifdef DEBUGMODE
	sendto_realops("connect_server() CONTINUED (%s:%d), aconf %p, refcount: %d, TEMP: %s",
		__FILE__, __LINE__, aconf, aconf->refcount, aconf->flag.temporary ? "YES" : "NO");
#endif
	Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
		client->name, client->serv->conf->servername, client->serv->conf->refcount));
	if (by && IsUser(by))
		strlcpy(client->serv->by, by->name, sizeof(client->serv->by));
	else
		strlcpy(client->serv->by, "AutoConn.", sizeof client->serv->by);
	client->serv->up = me.name;
	SetConnecting(client);
	SetOutgoing(client);
	irccounts.unknown++;
	list_add(&client->lclient_node, &unknown_list);
	set_sockhost(client, aconf->outgoing.hostname);
	add_client_to_list(client);

	if (aconf->outgoing.options & CONNECT_TLS)
	{
		SetTLSConnectHandshake(client);
		fd_setselect(client->local->fd, FD_SELECT_WRITE, ircd_SSL_client_handshake, client);
	}
	else
		fd_setselect(client->local->fd, FD_SELECT_WRITE, completed_connection, client);

	return 0;
}

/** Helper function for connect_server() to prepare the actual bind()'ing and connect().
 * @param aconf		Configuration entry of the server.
 * @param client	The client entry that we will use and fill in.
 * @returns 1 on success, 0 on failure.
 */
static int connect_server_helper(ConfigItem_link *aconf, Client *client)
{
	char *bindip;
	char buf[BUFSIZE];

	if (!aconf->connect_ip)
		return 0; /* handled upstream or shouldn't happen */
	
	if (strchr(aconf->connect_ip, ':'))
		SetIPV6(client);
	
	safe_strdup(client->ip, aconf->connect_ip);
	
	snprintf(buf, sizeof buf, "Outgoing connection: %s", get_client_name(client, TRUE));
	client->local->fd = fd_socket(IsIPV6(client) ? AF_INET6 : AF_INET, SOCK_STREAM, 0, buf);
	if (client->local->fd < 0)
	{
		if (ERRNO == P_EMFILE)
		{
			sendto_realops("opening stream socket to server %s: No more sockets",
				get_client_name(client, TRUE));
			return 0;
		}
		report_baderror("opening stream socket to server %s:%s", client);
		return 0;
	}
	if (++OpenFiles >= maxclients)
	{
		sendto_ops_and_log("No more connections allowed (%s)", client->name);
		return 0;
	}

	set_sockhost(client, aconf->outgoing.hostname);

	if (!aconf->outgoing.bind_ip && iConf.link_bindip)
		bindip = iConf.link_bindip;
	else
		bindip = aconf->outgoing.bind_ip;

	if (bindip && strcmp("*", bindip))
	{
		if (!unreal_bind(client->local->fd, bindip, 0, IsIPV6(client)))
		{
			report_baderror("Error binding to local port for %s:%s -- "
			                "Your link::outgoing::bind-ip is probably incorrect.", client);
			return 0;
		}
	}

	set_sock_opts(client->local->fd, client, IsIPV6(client));

	return unreal_connect(client->local->fd, client->ip, aconf->outgoing.port, IsIPV6(client));
}

/** Checks if the system is IPv6 capable.
 * IPv6 is always available at compile time (libs, headers), but the OS may
 * not have IPv6 enabled (or ipv6 kernel module not loaded). So we better check..
 */
int ipv6_capable(void)
{
	int s = socket(AF_INET6, SOCK_STREAM, 0);
	if (s < 0)
		return 0; /* NO ipv6 */
	
	CLOSE_SOCK(s);
	return 1; /* YES */
}

/** Attempt to deliver data to a client.
 * This function is only called from send_queued() and will deal
 * with sending to the SSL/TLS or plaintext connection.
 * @param cptr The client
 * @param str  The string to send
 * @param len  The length of the string
 * @param want_read In case of SSL/TLS it may happen that SSL_write()
 *                  needs to READ data. If this happens then this
 *                  function will set *want_read to 1.
 *                  The upper layer should then call us again when
 *                  there is data ready to be READ.
 * @retval <0  Some fatal error occurred, (but not EWOULDBLOCK).
 *             This return is a request to close the socket and
 *             clean up the link.
 * @retval >=0 No real error occurred, returns the number of
 *             bytes actually transferred. EWOULDBLOCK and other
 *             possibly similar conditions should be mapped to
 *             zero return. Upper level routine will have to
 *             decide what to do with those unwritten bytes...
 */
int deliver_it(Client *client, char *str, int len, int *want_read)
{
	int  retval;

	*want_read = 0;

	if (IsDeadSocket(client) || (!IsServer(client) && !IsUser(client)
	    && !IsHandshake(client) 
	    && !IsTLSHandshake(client)
 
	    && !IsUnknown(client)))
	{
		str[len] = '\0';
		sendto_ops
		    ("* * * DEBUG ERROR * * * !!! Calling deliver_it() for %s, status %d %s, with message: %s",
		    client->name, client->status, IsDeadSocket(client) ? "DEAD" : "", str);
		return -1;
	}

	if (IsTLS(client) && client->local->ssl != NULL)
	{
		retval = SSL_write(client->local->ssl, str, len);

		if (retval < 0)
		{
			switch (SSL_get_error(client->local->ssl, retval))
			{
			case SSL_ERROR_WANT_READ:
				SET_ERRNO(P_EWOULDBLOCK);
				*want_read = 1;
				return 0;
			case SSL_ERROR_WANT_WRITE:
				SET_ERRNO(P_EWOULDBLOCK);
				break;
			case SSL_ERROR_SYSCALL:
				break;
			case SSL_ERROR_SSL:
				if (ERRNO == P_EAGAIN)
					break;
				/* FALLTHROUGH */
			default:
				return -1; /* hm.. why was this 0?? we have an error! */
			}
		}
	}
	else
		retval = send(client->local->fd, str, len, 0);
	/*
	   ** Convert WOULDBLOCK to a return of "0 bytes moved". This
	   ** should occur only if socket was non-blocking. Note, that
	   ** all is Ok, if the 'write' just returns '0' instead of an
	   ** error and errno=EWOULDBLOCK.
	   **
	   ** ...now, would this work on VMS too? --msa
	 */
# ifndef _WIN32
	if (retval < 0 && (errno == EWOULDBLOCK || errno == EAGAIN ||
	    errno == ENOBUFS))
# else
		if (retval < 0 && (WSAGetLastError() == WSAEWOULDBLOCK ||
		    WSAGetLastError() == WSAENOBUFS))
# endif
			retval = 0;

	if (retval > 0)
	{
		client->local->sendB += retval;
		me.local->sendB += retval;
		if (client->local->sendB > 1023)
		{
			client->local->sendK += (client->local->sendB >> 10);
			client->local->sendB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
		}
		if (me.local->sendB > 1023)
		{
			me.local->sendK += (me.local->sendB >> 10);
			me.local->sendB &= 0x03ff;
		}
	}

	return (retval);
}

/** Initiate an outgoing connection, the actual connect() call. */
int unreal_connect(int fd, char *ip, int port, int ipv6)
{
	int n;
	
	if (ipv6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		inet_pton(AF_INET6, ip, &server.sin6_addr);
		server.sin6_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	} else {
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &server.sin_addr);
		server.sin_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	}

#ifndef _WIN32
	if (n < 0 && (errno != EINPROGRESS))
#else
	if (n < 0 && (WSAGetLastError() != WSAEINPROGRESS) && (WSAGetLastError() != WSAEWOULDBLOCK))
#endif
	{
		return 0; /* FATAL ERROR */
	}
	
	return 1; /* SUCCESS (probably still in progress) */
}

/** Bind to an IP/port (port may be 0 for auto).
 * @returns 0 on failure, other on success.
 */
int unreal_bind(int fd, char *ip, int port, int ipv6)
{
	if (ipv6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		server.sin6_port = htons(port);
		if (inet_pton(AF_INET6, ip, &server.sin6_addr.s6_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	} else {
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &server.sin_addr.s_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	}
}
