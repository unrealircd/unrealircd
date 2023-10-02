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
void set_sock_opts(int, Client *, SocketType);
void set_ipv6_opts(int);
void close_all_listeners(void);
void close_listener(ConfigItem_listen *listener);
static char readbuf[BUFSIZE];
char zlinebuf[BUFSIZE];
extern char *version;
MODVAR time_t last_allinuse = 0;

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

	list_for_each_entry(client, &control_list, lclient_node)
	{
		if (client->local->fd >= 0)
		{
			fd_close(client->local->fd);
			client->local->fd = -2;
		}
	}

	close_all_listeners();

	OpenFiles = 0;

#ifdef _WIN32
	WSACleanup();
#endif
}

/** Accept an incoming connection.
 * @param listener	The listen { } block configuration data.
 * @returns 1 if the connection was accepted (even if it was rejected),
 * 0 if there is no more work to do (accept returned an error).
 */
static int listener_accept_wrapper(ConfigItem_listen *listener)
{
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
			if (listener->file)
			{
				unreal_log(ULOG_FATAL, "listen", "ACCEPT_ERROR", NULL, "Cannot accept incoming connection on file $file: $socket_error",
					   log_data_socket_error(listener->fd),
					   log_data_string("file", listener->file));
			} else {
				unreal_log(ULOG_FATAL, "listen", "ACCEPT_ERROR", NULL, "Cannot accept incoming connection on IP \"$listen_ip\" port $listen_port: $socket_error",
					   log_data_socket_error(listener->fd),
					   log_data_string("listen_ip", listener->ip),
					   log_data_integer("listen_port", listener->port));
			}
			close_listener(listener);
			start_listeners();
		}
		return 0;
	}

	ircstats.is_ac++;

	set_sock_opts(cli_fd, NULL, listener->socket_type);

	/* Allow connections to the control socket, even if maxclients is reached */
	if (listener->options & LISTENER_CONTROL)
	{
		/* ... but not unlimited ;) */
		if ((++OpenFiles >= maxclients+(CLIENTS_RESERVE/2)) || (cli_fd >= maxclients+(CLIENTS_RESERVE/2)))
		{
			ircstats.is_ref++;
			if (last_allinuse < TStime() - 15)
			{
				unreal_log(ULOG_FATAL, "listen", "ACCEPT_ERROR_MAXCLIENTS", NULL, "Cannot accept incoming connection on file $file: All connections in use",
					   log_data_string("file", listener->file));
				last_allinuse = TStime();
			}
			fd_close(cli_fd);
			--OpenFiles;
			return 1;
		}
	} else
	{
		if ((++OpenFiles >= maxclients) || (cli_fd >= maxclients))
		{
			ircstats.is_ref++;
			if (last_allinuse < TStime() - 15)
			{
				if (listener->file)
				{
					unreal_log(ULOG_FATAL, "listen", "ACCEPT_ERROR_MAXCLIENTS", NULL, "Cannot accept incoming connection on file $file: All connections in use",
						   log_data_string("file", listener->file));
				} else {
					unreal_log(ULOG_FATAL, "listen", "ACCEPT_ERROR_MAXCLIENTS", NULL, "Cannot accept incoming connection on IP \"$listen_ip\" port $listen_port: All connections in use",
						   log_data_string("listen_ip", listener->ip),
						   log_data_integer("listen_port", listener->port));
				}
				last_allinuse = TStime();
			}

			(void)send(cli_fd, "ERROR :All connections in use\r\n", 31, 0);

			fd_close(cli_fd);
			--OpenFiles;
			return 1;
		}
	}

	/* add_connection() may fail. we just don't care. */
	add_connection(listener, cli_fd);
	return 1;
}

/** Accept an incoming connection.
 * @param listener_fd	The file descriptor of a listen() socket.
 * @param data		The listen { } block configuration data.
 */
static void listener_accept(int listener_fd, int revents, void *data)
{
	int i;

	/* Accept clients, but only up to a maximum in each run,
	 * as to allow some CPU available to existing clients.
	 * Better refuse or lag a few new clients than become
	 * unresponse to existing clients.
	 */
	for (i=0; i < 100; i++)
		if (!listener_accept_wrapper((ConfigItem_listen *)data))
			break;
}

int unreal_listen_inet(ConfigItem_listen *listener)
{
	const char *ip = listener->ip;
	int port = listener->port;

	if (BadPtr(ip))
		ip = "*";

	if (*ip == '*')
	{
		if (listener->socket_type == SOCKET_TYPE_IPV6)
			ip = "::";
		else
			ip = "0.0.0.0";
	}

	/* At first, open a new socket */
	if (listener->fd >= 0)
		abort(); /* Socket already exists but we are asked to create and listen on one. Bad! */

	if (port == 0)
		abort(); /* Impossible as well, right? */

	listener->fd = fd_socket(listener->socket_type == SOCKET_TYPE_IPV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0, "Listener socket");
	if (listener->fd < 0)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_SOCKET_ERROR", NULL,
		           "Could not listen on IP \"$listen_ip\" on port $listen_port: $socket_error",
			   log_data_socket_error(-1),
			   log_data_string("listen_ip", ip),
			   log_data_integer("listen_port", port));
		return -1;
	}

	if (++OpenFiles >= maxclients)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_ERROR_MAXCLIENTS", NULL,
		           "Could not listen on IP \"$listen_ip\" on port $listen_port: all connections in use",
		           log_data_string("listen_ip", ip),
		           log_data_integer("listen_port", port));
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	set_sock_opts(listener->fd, NULL, listener->socket_type);

	if (!unreal_bind(listener->fd, ip, port, listener->socket_type))
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_BIND_ERROR", NULL,
		           "Could not listen on IP \"$listen_ip\" on port $listen_port: $socket_error",
		           log_data_socket_error(listener->fd),
		           log_data_string("listen_ip", ip),
		           log_data_integer("listen_port", port));
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	if (listen(listener->fd, LISTEN_SIZE) < 0)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_LISTEN_ERROR", NULL,
		           "Could not listen on IP \"$listen_ip\" on port $listen_port: $socket_error",
		           log_data_socket_error(listener->fd),
		           log_data_string("listen_ip", ip),
		           log_data_integer("listen_port", port));
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

int unreal_listen_unix(ConfigItem_listen *listener)
{
	if (listener->socket_type != SOCKET_TYPE_UNIX)
		abort(); /* "impossible" */

	/* At first, open a new socket */
	if (listener->fd >= 0)
		abort(); /* Socket already exists but we are asked to create and listen on one. Bad! */

	listener->fd = fd_socket(AF_UNIX, SOCK_STREAM, 0, "Listener socket (UNIX)");
	if (listener->fd < 0)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_SOCKET_ERROR", NULL,
		           "Could not create UNIX domain socket for $file: $socket_error",
			   log_data_socket_error(-1),
			   log_data_string("file", listener->file));
		return -1;
	}

	if (++OpenFiles >= maxclients)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_ERROR_MAXCLIENTS", NULL,
		           "Could not create UNIX domain socket for $file: all connections in use",
		           log_data_string("file", listener->file));
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	set_sock_opts(listener->fd, NULL, listener->socket_type);

	if (!unreal_bind(listener->fd, listener->file, listener->mode, SOCKET_TYPE_UNIX))
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_BIND_ERROR", NULL,
		           "Could not listen on UNIX domain socket $file: $socket_error",
		           log_data_socket_error(listener->fd),
		           log_data_string("file", listener->file));
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	if (listen(listener->fd, LISTEN_SIZE) < 0)
	{
		unreal_log(ULOG_FATAL, "listen", "LISTEN_LISTEN_ERROR", NULL,
		           "Could not listen on UNIX domain socket $file: $socket_error",
		           log_data_socket_error(listener->fd),
		           log_data_string("file", listener->file));
		fd_close(listener->fd);
		listener->fd = -1;
		--OpenFiles;
		return -1;
	}

	fd_setselect(listener->fd, FD_SELECT_READ, listener_accept, listener);

	return 0;
}

/** Create a listener port.
 * @param listener	The listen { } block configuration
 * @returns 0 on success and <0 on error. Yeah, confusing.
 */
int unreal_listen(ConfigItem_listen *listener)
{
	if ((listener->socket_type == SOCKET_TYPE_IPV4) || (listener->socket_type == SOCKET_TYPE_IPV6))
		return unreal_listen_inet(listener);
	return unreal_listen_unix(listener);
}

/** Activate a listen { } block */
int add_listener(ConfigItem_listen *listener)
{
	if (unreal_listen(listener))
	{
		/* Error is already handled upstream */
		listener->fd = -2;
	}

	if (listener->fd >= 0)
	{
		listener->options |= LISTENER_BOUND;
		return 1;
	}
	else
	{
		listener->fd = -1;
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
		unreal_log(ULOG_INFO, "listen", "LISTEN_REMOVED", NULL,
			   "UnrealIRCd is now no longer listening on $listen_ip:$listen_port",
			   log_data_string("listen_ip", listener->ip),
			   log_data_integer("listen_port", listener->port));
		fd_close(listener->fd);
		--OpenFiles;
	}

	listener->options &= ~LISTENER_BOUND;
	listener->fd = -1;
	/* We can already free the TLS context, since it is only
	 * used for new connections, which we no longer accept.
	 */
	if (listener->ssl_ctx)
	{
		SSL_CTX_free(listener->ssl_ctx);
		listener->ssl_ctx = NULL;
	}
}

/** Close all listeners - eg on DIE or RESTART */
void close_all_listeners(void)
{
	ConfigItem_listen *aconf, *aconf_next;

	/* close all 'extra' listening ports we have */
	for (aconf = conf_listen; aconf != NULL; aconf = aconf->next)
		close_listener(aconf);
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

/** Do an ident lookup if necessary.
 * @param client	The incoming client
 */
void consider_ident_lookup(Client *client)
{
	char buf[BUFSIZE];

	/* If ident checking is disabled or it's an outgoing connect, then no ident check */
	if ((IDENT_CHECK == 0) || (client->server && IsHandshake(client)) || IsUnixSocket(client))
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
	ConfigItem_link *aconf = client->server ? client->server->conf : NULL;

	if (IsHandshake(client))
	{
		/* Due to delayed unreal_tls_connect call */
		start_server_handshake(client);
		fd_setselect(fd, FD_SELECT_READ, read_packet, client);
		return;
	}

	SetHandshake(client);

	if (!aconf)
	{
		unreal_log(ULOG_ERROR, "link", "BUG_LOST_CONFIGURATION_ON_CONNECT", client,
		           "Lost configuration while connecting to $client.details");
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
		ircstats.is_sti += TStime() - client->local->creationtime;
	}
	else if (IsUser(client))
	{
		ircstats.is_cl++;
		ircstats.is_cti += TStime() - client->local->creationtime;
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
	SetDeadSocket(client); /* stop trying to send to this destination */
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
void set_sock_opts(int fd, Client *client, SocketType socket_type)
{
	int opt;

	if (socket_type == SOCKET_TYPE_IPV6)
		set_ipv6_opts(fd);

	if ((socket_type == SOCKET_TYPE_IPV4) || (socket_type == SOCKET_TYPE_IPV6))
	{
#ifdef SO_REUSEADDR
		opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&opt, sizeof(opt)) < 0)
		{
			unreal_log(ULOG_WARNING, "socket", "SOCKET_ERROR_SETSOCKOPTS", client,
				   "Could not setsockopt(SO_REUSEADDR): $socket_error",
				   log_data_socket_error(-1));
		}
#endif

#if defined(SO_USELOOPBACK) && !defined(_WIN32)
		opt = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_USELOOPBACK, (void *)&opt, sizeof(opt)) < 0)
		{
			unreal_log(ULOG_WARNING, "socket", "SOCKET_ERROR_SETSOCKOPTS", client,
				   "Could not setsockopt(SO_USELOOPBACK): $socket_error",
				   log_data_socket_error(-1));
		}
#endif

	}

	/* The following code applies to all socket types: IPv4, IPv6, UNIX domain sockets */

	/* Set to non blocking: */
#if !defined(_WIN32)
	if ((opt = fcntl(fd, F_GETFL, 0)) == -1)
	{
		if (client)
		{
			unreal_log(ULOG_WARNING, "socket", "SOCKET_ERROR_SETSOCKOPTS", client,
				   "Could not get socket options (F_GETFL): $socket_error",
				   log_data_socket_error(-1));
		}
	}
	else if (fcntl(fd, F_SETFL, opt | O_NONBLOCK) == -1)
	{
		if (client)
		{
			unreal_log(ULOG_WARNING, "socket", "SOCKET_ERROR_SETSOCKOPTS", client,
				   "Could not get socket options (F_SETFL): $socket_error",
				   log_data_socket_error(-1));
		}
	}
#else
	opt = 1;
	if (ioctlsocket(fd, FIONBIO, &opt) < 0)
	{
		if (client)
		{
			unreal_log(ULOG_WARNING, "socket", "SOCKET_ERROR_SETSOCKOPTS", client,
				   "Could not ioctlsocket FIONBIO: $socket_error",
				   log_data_socket_error(-1));
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
		if ((e->options & LISTENER_BOUND) && e->ip && !strcmp(ip, e->ip))
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
const char *getpeerip(Client *client, int fd, int *port)
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
	const char *ip;
	int port = 0;
	Hook *h;

	client = make_client(NULL, &me);
	client->local->socket_type = listener->socket_type;
	client->local->listener = listener;
	client->local->listener->clients++;

	if (listener->socket_type == SOCKET_TYPE_UNIX)
		ip = listener->spoof_ip ? listener->spoof_ip : "127.0.0.1";
	else
		ip = getpeerip(client, fd, &port);

	if (!ip)
	{
		/* On Linux 2.4 and FreeBSD the socket may just have been disconnected
		 * so it's not a serious error and can happen quite frequently -- Syzop
		 */
		if (ERRNO != P_ENOTCONN)
		{
			unreal_log(ULOG_ERROR, "listen", "ACCEPT_ERROR", NULL,
			           "Failed to accept new client: unable to get IP address: $socket_error",
				   log_data_socket_error(fd),
				   log_data_string("listen_ip", listener->ip),
				   log_data_integer("listen_port", listener->port));
		}
refuse_client:
			ircstats.is_ref++;
			client->local->fd = -2;
			if (!list_empty(&client->client_node))
				list_del(&client->client_node);
			if (!list_empty(&client->lclient_node))
				list_del(&client->lclient_node);
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

	add_client_to_list(client);
	irccounts.unknown++;
	client->status = CLIENT_STATUS_UNKNOWN;
	list_add(&client->lclient_node, &unknown_list);
	connections_past_period++;

	for (h = Hooks[HOOKTYPE_ACCEPT]; h; h = h->next)
	{
		int value = (*(h->func.intfunc))(client);
		if (value == 2) // HOOK_DENY_ALWAYS
		{
			deadsocket_exit(client, 1);
			irccounts.unknown--;
			goto refuse_client;
		} else
		if (value == HOOK_DENY)
		{
			if (quick_close || !(listener->options & LISTENER_TLS))
			{
				/* If we are under attack or the client is
				 * not using SSL/TLS then take the quick close
				 * code path which rejects the client immediately.
				 */
				deadsocket_exit(client, 1);
				irccounts.unknown--;
				goto refuse_client;
			} else {
				/* continue, and even do the SSL/TLS handshake */
			}
		}
		/* NOT in else block: */
		if (value != HOOK_CONTINUE)
			break;
	}

	if ((listener->options & LISTENER_TLS) && ctx_server)
	{
		SSL_CTX *ctx = listener->ssl_ctx ? listener->ssl_ctx : ctx_server;

		if (ctx)
		{
			SetTLSAcceptHandshake(client);
			if ((client->local->ssl = SSL_new(ctx)) == NULL)
			{
				irccounts.unknown--;
				goto refuse_client;
			}
			SetTLS(client);
			SSL_set_fd(client->local->ssl, fd);
			SSL_set_nonblocking(client->local->ssl);
			SSL_set_ex_data(client->local->ssl, tls_client_index, client);
			if (!unreal_tls_accept(client, fd))
			{
				SSL_set_shutdown(client->local->ssl, SSL_RECEIVED_SHUTDOWN);
				SSL_smart_shutdown(client->local->ssl);
				SSL_free(client->local->ssl);
				irccounts.unknown--;
				goto refuse_client;
			}
		}
	} else
	{
		listener->start_handshake(client);
	}
	return client;
}

/** Mark the socket as "dead".
 * This is used when exit_client() cannot be used from the
 * current code because doing so would be (too) unexpected.
 * The socket is closed later in the main loop.
 * NOTE: this function is becoming less important, now that
 *       exit_client() will not actively free the client.
 *       Still, sometimes we need to use dead_socket()
 *       since we don't want to be doing IsDead() checks after
 *       each and every sendto...().
 * @param to		Client to mark as dead
 * @param notice	The quit reason to use
 */
int dead_socket(Client *to, const char *notice)
{
	DBufClear(&to->local->recvQ);
	DBufClear(&to->local->sendQ);

	if (IsDeadSocket(to))
		return -1; /* already pending to be closed */

	SetDeadSocket(to);

	/* deregister I/O notification since we don't care anymore. the actual closing of socket will happen later. */
	if (to->local->fd >= 0)
		fd_unnotify(to->local->fd);

	/* We may get here because of the 'CPR' in check_deadsockets().
	 * In which case, we return -1 as well.
	 */
	if (to->local->error_str)
		return -1; /* don't overwrite & don't send multiple times */
	
	if (!IsUser(to) && !IsUnknown(to) && !IsRPC(to) && !IsControl(to) && !IsClosing(to))
	{
		/* Looks like a duplicate error message to me?
		 * If so, remove it here.
		 */
		unreal_log(ULOG_ERROR, "link", "LINK_CLOSING", to,
		           "Link to server $client.details closed: $reason",
		           log_data_string("reason", notice));
	}
	safe_strdup(to->local->error_str, notice);
	return -1;
}

void deadsocket_exit(Client *client, int special)
{
	/* First clear the deadsocket flag, so the sending routines are 'on' again */
	ClearDeadSocket(client);
	if (client->flags & CLIENT_FLAG_DEADSOCKET_IS_BANNED)
	{
		/* For this case we need to send some extra lines */
		sendnumeric(client, ERR_YOUREBANNEDCREEP, client->local->error_str);
		sendnotice(client, "%s", client->local->error_str);
	}

	if (special)
	{
		sendto_one(client, NULL, "ERROR :Closing Link: %s (%s)", get_client_name(client, FALSE),
			client->local->error_str ? client->local->error_str : "Dead socket");
		send_queued(client);
		/* Caller takes care of freeing 'client' - only used by HOOKTYPE_ACCEPT */
		return;
	} else {
		exit_client(client, NULL, client->local->error_str ? client->local->error_str : "Dead socket");
	}
}

typedef enum DNSFinishedType {
	DNS_FINISHED_NONE=0,            /**< We finished because DNS lookups are disabled */
	DNS_FINISHED_FAIL=1,            /**< DNS lookup failed (cached or uncached) */
	DNS_FINISHED_SUCCESS=2,         /**< DNS lookup succeeded (uncached) */
	DNS_FINISHED_SUCCESS_CACHED=3   /**< DNS lookup succeeded (cached DNS entry) */
} DNSFinishedType;

void dns_finished(Client *client, DNSFinishedType type)
{
	switch(type)
	{
		case DNS_FINISHED_FAIL:
			if (should_show_connect_info(client))
				sendto_one(client, NULL, ":%s %s", me.name, REPORT_FAIL_DNS);
			break;
		case DNS_FINISHED_SUCCESS:
			if (should_show_connect_info(client))
				sendto_one(client, NULL, ":%s %s", me.name, REPORT_FIN_DNS);
			break;
		case DNS_FINISHED_SUCCESS_CACHED:
			if (should_show_connect_info(client))
				sendto_one(client, NULL, ":%s %s", me.name, REPORT_FIN_DNSC);
			break;
		default:
			break;
	}

	/* Set sockhost to resolved hostname already */
	if (client->local->hostp)
	        set_sockhost(client, client->local->hostp->h_name);

	RunHook(HOOKTYPE_DNS_FINISHED, client);
}

void start_dns_and_ident_lookup(Client *client)
{
	struct hostent *he;

	/* First, reset, to be safe. Especially nowadays that we
	 * are called not only from start_of_normal_client_handshake()
	 * but also when IP gets updated due to a proxy.
	 */
	strlcpy(client->local->sockhost, GetIP(client), sizeof(client->local->sockhost));
	if (client->local->hostp)
	{
		unreal_free_hostent(client->local->hostp);
		client->local->hostp = NULL;
	}

	/* Remove any outstanding DNS requests */
	unrealdns_delreq_bycptr(client);
	ClearDNSLookup(client);
	cancel_ident_lookup(client);

	if (!DONT_RESOLVE && !IsUnixSocket(client))
	{
		if (should_show_connect_info(client))
			sendto_one(client, NULL, ":%s %s", me.name, REPORT_DO_DNS);
		he = unrealdns_doclient(client);

		if (client->local->hostp)
		{
			/* Race condition detected, DNS has been done.
			 * Hmmm.. actually I don't think this can be triggered?
			 * If this were a legit case then we need to figure out
			 * how to trigger this and if dns_finished() has been
			 * called already or not.
			 */
#ifdef DEBUGMODE
			abort();
#endif
			goto doauth;
		}

		if (!he)
		{
			/* Resolving in progress */
			SetDNSLookup(client);
		} else
		if (he->h_name == NULL)
		{
			/* Host was negatively cached */
			unreal_free_hostent(he);
			dns_finished(client, DNS_FINISHED_FAIL);
		} else
		{
			/* Host was in our cache */
			client->local->hostp = he;
			dns_finished(client, DNS_FINISHED_SUCCESS_CACHED);
		}
	} else {
		/* Still need to call this, so our hooks get called */
		dns_finished(client, DNS_FINISHED_NONE);
	}

doauth:
	consider_ident_lookup(client);
}

/** Start of normal client handshake - DNS and ident lookups, etc.
 * @param client	The client
 * @note This is called directly after accept() -> add_connection() for plaintext.
 *       For TLS connections this is called after the TLS handshake is completed.
 */
void start_of_normal_client_handshake(Client *client)
{
	client->status = CLIENT_STATUS_UNKNOWN; /* reset, to be sure (TLS handshake has ended) */

	RunHook(HOOKTYPE_HANDSHAKE, client);

	start_dns_and_ident_lookup(client);

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
	if (client->local->hostp)
		dns_finished(client, DNS_FINISHED_SUCCESS_CACHED);
	else
		dns_finished(client, DNS_FINISHED_FAIL);
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
	 * to handle (TLS) writes by read_packet(), see below under
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

			if (IsServer(client) || client->server) /* server or outgoing connection */
				lost_server_link(client, NULL);

			exit_client(client, NULL, ERRNO ? "Read error" : "Connection closed");
			return;
		}

		client->local->last_msg_received = now;
		if (client->local->last_msg_received > client->local->fake_lag)
			client->local->fake_lag = client->local->last_msg_received;
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

	do {
		list_for_each_entry(client, &control_list, lclient_node)
		{
			if ((client->local->fd >= 0) && DBufLength(&client->local->recvQ) && !IsDead(client))
			{
				parse_client_queued(client);
				if (IsDead(client))
					break;
			}
		}
	} while(&client->lclient_node != &control_list);


}

/** Check if 'ip' is a valid IP address, and if so what type.
 * @param ip	The IP address
 * @retval 4	Valid IPv4 address
 * @retval 6	Valid IPv6 address
 * @retval 0	Invalid IP address (eg: a hostname)
 */
int is_valid_ip(const char *ip)
{
	char scratch[64];

	if (BadPtr(ip))
		return 0;

	if (inet_pton(AF_INET, ip, scratch) == 1)
		return 4; /* IPv4 */

	if (inet_pton(AF_INET6, ip, scratch) == 1)
		return 6; /* IPv6 */

	return 0; /* not an IP address */
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

/** Return 1 if UNIX sockets of type SOCK_STREAM are supported, and 0 otherwise */
int unix_sockets_capable(void)
{
	int fd = fd_socket(AF_UNIX, SOCK_STREAM, 0, "Testing UNIX socket");
	if (fd < 0)
		return 0;
	fd_close(fd);
	return 1;
}

/** Attempt to deliver data to a client.
 * This function is only called from send_queued() and will deal
 * with sending to the TLS or plaintext connection.
 * @param cptr The client
 * @param str  The string to send
 * @param len  The length of the string
 * @param want_read In case of TLS it may happen that SSL_write()
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

	if (IsDeadSocket(client) ||
	    (!IsServer(client) && !IsUser(client) && !IsHandshake(client) &&
	     !IsTLSHandshake(client) && !IsUnknown(client) &&
	     !IsControl(client) && !IsRPC(client)))
	{
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
		client->local->traffic.bytes_sent += retval;
		me.local->traffic.bytes_sent += retval;
	}

	return (retval);
}

/** Initiate an outgoing connection, the actual connect() call. */
int unreal_connect(int fd, const char *ip, int port, SocketType socket_type)
{
	int n;

	if (socket_type == SOCKET_TYPE_IPV6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		inet_pton(AF_INET6, ip, &server.sin6_addr);
		server.sin6_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	}
	else if (socket_type == SOCKET_TYPE_IPV4)
	{
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		inet_pton(AF_INET, ip, &server.sin_addr);
		server.sin_port = htons(port);
		n = connect(fd, (struct sockaddr *)&server, sizeof(server));
	} else
	{
		struct sockaddr_un server;
		memset(&server, 0, sizeof(server));
		server.sun_family = AF_UNIX;
		strlcpy(server.sun_path, ip, sizeof(server.sun_path));
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
int unreal_bind(int fd, const char *ip, int port, SocketType socket_type)
{
	if (socket_type == SOCKET_TYPE_IPV4)
	{
		struct sockaddr_in server;
		memset(&server, 0, sizeof(server));
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		if (inet_pton(AF_INET, ip, &server.sin_addr.s_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	}
	else if (socket_type == SOCKET_TYPE_IPV6)
	{
		struct sockaddr_in6 server;
		memset(&server, 0, sizeof(server));
		server.sin6_family = AF_INET6;
		server.sin6_port = htons(port);
		if (inet_pton(AF_INET6, ip, &server.sin6_addr.s6_addr) != 1)
			return 0;
		return !bind(fd, (struct sockaddr *)&server, sizeof(server));
	} else
	{
		struct sockaddr_un server;
		mode_t saved_umask, new_umask;
		int ret;

		if (port == 0)
			new_umask = 077;
		else
			new_umask = port ^ 0777;

		unlink(ip); /* (ignore errors) */

		memset(&server, 0, sizeof(server));
		server.sun_family = AF_UNIX;
		strlcpy(server.sun_path, ip, sizeof(server.sun_path));
		saved_umask = umask(new_umask);
		ret = !bind(fd, (struct sockaddr *)&server, sizeof(server));
		umask(saved_umask);

		return ret;
	}
}

#ifdef _WIN32
void init_winsock(void)
{
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(1, 1), &WSAData) != 0)
	{
		MessageBox(NULL, "Unable to initialize WinSock", "UnrealIRCD Initalization Error", MB_OK);
		fprintf(stderr, "Unable to initialize WinSock\n");
		exit(1);
	}
}
#endif
