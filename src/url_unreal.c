/*
 *   Unreal Internet Relay Chat Daemon, src/url.c
 *   (C) 2021 Bram Matthys and the UnrealIRCd team
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

#include "unrealircd.h"

/* Structs */

/* Stores information about the async transfer.
 * Used to maintain information about the transfer
 * to trigger the callback upon completion.
 */
typedef struct Download Download;

struct Download
{
	Download *prev, *next;
	OutgoingWebRequest *request;
	FILE *file_fd;		/**< File open for writing (otherwise NULL) */
	char *filename;
	char *memory_data; /**< Memory for writing response (otherwise NULL) */
	int memory_data_len; /**< Size of memory_data */
	int memory_data_allocated; /**< Total allocated memory for 'memory_data' */
	char errorbuf[512];
	char *hostname;		/**< Parsed hostname (from 'url') */
	int port;		/**< Parsed port (from 'url') */
	char *username;
	char *password;
	char *document;		/**< Parsed document (from 'url') */
	char *ip;		/**< Resolved IP */
	int ipv6;
	SSL *ssl;
	int fd;			/**< Socket */
	int connected;
	int got_response;
	int http_status_code;
	char *lefttoparse;
	long long lefttoparselen; /* size of data in lefttoparse (note: not used for first header parsing) */
	time_t last_modified;
	time_t download_started;
	int dns_refcnt;
	TransferEncoding transfer_encoding;
	long chunk_remaining;
	char *redirect_new_location;
};

/* Variables */
Download *downloads = NULL;
SSL_CTX *https_ctx = NULL;

/* Forward declarations */
void url_resolve_cb(void *arg, int status, int timeouts, struct hostent *he);
void unreal_https_initiate_connect(Download *handle);
int url_parse(const char *url, char **host, int *port, char **username, char **password, char **document);
SSL_CTX *https_new_ctx(void);
void unreal_https_connect_handshake(int fd, int revents, void *data);
int https_connect(Download *handle);
int https_fatal_tls_error(int ssl_error, int my_errno, Download *handle);
int https_connect_send_header(Download *handle);
void https_receive_response(int fd, int revents, void *data);
int https_handle_response_header(Download *handle, char *readbuf, int n);
int https_handle_response_body(Download *handle, char *readbuf, int n);
void https_done(Download *handle);
void https_done_cached(Download *handle);
void https_redirect(Download *handle);
int https_parse_header(char *buffer, int len, char **key, char **value, char **lastloc, int *end_of_request);
char *url_find_end_of_request(char *header, int totalsize, int *remaining_bytes);
int https_cancel(Download *handle, FORMAT_STRING(const char *pattern), ...)  __attribute__((format(printf,2,3)));

void url_free_handle(Download *handle)
{
	DelListItem(handle, downloads);
	if (handle->fd > 0)
	{
		fd_close(handle->fd);
		fd_unnotify(handle->fd);
	}
	if (handle->file_fd)
		fclose(handle->file_fd);
	if (handle->filename && !handle->request->keep_file)
		remove(handle->filename);
	safe_free(handle->filename);
	safe_free(handle->memory_data);
	safe_free(handle->hostname);
	safe_free(handle->username);
	safe_free(handle->password);
	safe_free(handle->document);
	safe_free(handle->ip);
	if (handle->ssl)
		SSL_free(handle->ssl);
	safe_free(handle->lefttoparse);
	safe_free(handle->redirect_new_location);
	safe_free_outgoingwebrequest(handle->request);
	safe_free(handle);
}

void url_cancel_handle_by_callback_data(void *ptr)
{
	Download *d, *d_next;

	for (d = downloads; d; d = d_next)
	{
		d_next = d->next;
		if (d->request->callback_data == ptr)
		{
			d->request->callback = NULL;
			d->request->callback_data = NULL;
		}
	}
}

/** Cancel and free the HTTPS request->
 * @returns Always returns -1
 */
int https_cancel(Download *handle, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsnprintf(handle->errorbuf, sizeof(handle->errorbuf), pattern, vl);
	va_end(vl);
	url_callback(handle->request, NULL, NULL, 0, handle->errorbuf, 0, handle->request->callback_data);
	url_free_handle(handle);
	return -1;
}

void url_start_async(OutgoingWebRequest *request)
{
	char *file;
	const char *filename;
	char *tmp;
	Download *handle = NULL;
	int ipv6 = 0;
	const char *actual_url = request->actual_url ? request->actual_url : request->url;
	char *host;
	int port;
	char *username;
	char *password;
	char *document;

	/* Check for the bare minimum */
	if (!request->url || !request->http_method)
		abort();

	/* Set request defaults */
	if (request->connect_timeout == 0)
		request->connect_timeout = DOWNLOAD_CONNECT_TIMEOUT;
	if (request->transfer_timeout == 0)
		request->transfer_timeout = DOWNLOAD_TRANSFER_TIMEOUT;

	handle = safe_alloc(sizeof(Download));
	handle->download_started = TStime();
	handle->request = request;
	AddListItem(handle, downloads);

	if (strncmp(actual_url, "https://", 8))
	{
		https_cancel(handle, "Only https:// is supported (either rebuild UnrealIRCd with curl support or use https)");
		return;
	}
	if (!url_parse(actual_url, &host, &port, &username, &password, &document))
	{
		https_cancel(handle, "Failed to parse HTTP url");
		return;
	}

	safe_strdup(handle->hostname, host);
	handle->port = port;
	safe_strdup(handle->username, username);
	safe_strdup(handle->password, password);
	safe_strdup(handle->document, document);

	if (request->store_in_file)
	{
		file = url_getfilename(handle->request->url);
		filename = unreal_getfilename(file);
		tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");

		handle->file_fd = fopen(tmp, "wb");
		if (!handle->file_fd)
		{
			https_cancel(handle, "Cannot create '%s': %s", tmp, strerror(ERRNO));
			safe_free(file);
			return;
		}

		safe_strdup(handle->filename, tmp);
		safe_free(file);
	} else {
		handle->memory_data_allocated = URL_MEMORY_BACKED_CHUNK_SIZE;
		handle->memory_data = safe_alloc(URL_MEMORY_BACKED_CHUNK_SIZE);
	}

	if (is_valid_ip(handle->hostname))
	{
		/* Nothing to resolve, eg https://127.0.0.1/ */
		safe_strdup(handle->ip, handle->hostname);
		unreal_https_initiate_connect(handle);
	} else {
		/* Hostname, so start resolving... */
		handle->dns_refcnt++;
		ares_gethostbyname(resolver_channel_client, handle->hostname, AF_INET, url_resolve_cb, handle);
		// TODO: check return value?
	}
}

void url_resolve_cb(void *arg, int status, int timeouts, struct hostent *he)
{
	Download *handle = (Download *)arg;
	int n;
	struct hostent *he2;
	char ipbuf[HOSTLEN+1];
	const char *ip = NULL;

	handle->dns_refcnt--;

	if ((status != 0) || !he->h_addr_list || !he->h_addr_list[0])
	{
		https_cancel(handle, "Unable to resolve hostname '%s'", handle->hostname);
		return;
	}

	if (!he->h_addr_list[0] || (he->h_length != (handle->ipv6 ? 16 : 4)) ||
	    !(ip = inetntop(handle->ipv6 ? AF_INET6 : AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf))))
	{
		/* Illegal response -- fatal */
		https_cancel(handle, "Unable to resolve hostname '%s'", handle->hostname);
		return;
	}

	/* Ok, since we got here, it seems things were actually succesfull */

	safe_strdup(handle->ip, ip);

	unreal_https_initiate_connect(handle);
}

void unreal_https_initiate_connect(Download *handle)
{
	// todo: allocate handle, select en weetikt allemaal
	// add to some global struct linkedlist, for timeouts
	// register in i/o

	if (!handle->ip)
	{
		https_cancel(handle, "No IP address found to connect to");
		return;
	}

	handle->fd = fd_socket(handle->ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0, "HTTPS");
	if (handle->fd < 0)
	{
		https_cancel(handle, "Could not create socket: %s", strerror(ERRNO));
		return;
	}
	set_sock_opts(handle->fd, NULL, handle->ipv6);
	if (!unreal_connect(handle->fd, handle->ip, handle->port, handle->ipv6))
	{
		https_cancel(handle, "Could not connect: %s", strerror(ERRNO));
		return;
	}

	fd_setselect(handle->fd, FD_SELECT_WRITE, unreal_https_connect_handshake, handle);
}

// based on unreal_tls_client_handshake()
void unreal_https_connect_handshake(int fd, int revents, void *data)
{
	Download *handle = data;
	handle->ssl = SSL_new(https_ctx);
	if (!handle->ssl)
	{
		https_cancel(handle, "Failed to setup SSL");
		return;
	}
	SSL_set_fd(handle->ssl, handle->fd);
	SSL_set_connect_state(handle->ssl);
	SSL_set_nonblocking(handle->ssl);
	SSL_set_tlsext_host_name(handle->ssl, handle->hostname);

	if (https_connect(handle) < 0)
		return; /* fatal error, handle is freed */

	/* Is now connecting... */
}

SSL_CTX *https_new_ctx(void)
{
	SSL_CTX *ctx_client;
	char buf1[512], buf2[512];
	char *curl_ca_bundle = buf1;

	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	ctx_client = SSL_CTX_new(SSLv23_client_method());
	if (!ctx_client)
		return NULL;
#ifdef HAS_SSL_CTX_SET_MIN_PROTO_VERSION
	SSL_CTX_set_min_proto_version(ctx_client, TLS1_2_VERSION);
#endif
	SSL_CTX_set_options(ctx_client, SSL_OP_NO_SSLv2|SSL_OP_NO_SSLv3|SSL_OP_NO_TLSv1|SSL_OP_NO_TLSv1_1);

	/* Verify peer certificate */
	snprintf(buf1, sizeof(buf1), "%s/tls/curl-ca-bundle.crt", CONFDIR);
	if (!file_exists(buf1))
	{
		snprintf(buf2, sizeof(buf2), "%s/doc/conf/tls/curl-ca-bundle.crt", BUILDDIR);
		if (!file_exists(buf2))
		{
			unreal_log(ULOG_ERROR, "url", "CA_BUNDLE_NOT_FOUND", NULL,
			           "Neither $filename1 nor $filename2 exist.\n"
			           "Cannot use built-in https client without curl-ca-bundle.crt\n",
			           log_data_string("filename1", buf1),
			           log_data_string("filename2", buf2));
			exit(-1);
		}
		curl_ca_bundle = buf2;
	}
	SSL_CTX_load_verify_locations(ctx_client, curl_ca_bundle, NULL);
	SSL_CTX_set_verify(ctx_client, SSL_VERIFY_PEER, NULL);

	/* Limit ciphers as well */
	SSL_CTX_set_cipher_list(ctx_client, UNREALIRCD_DEFAULT_CIPHERS);

	return ctx_client;
}

// Based on unreal_tls_connect_retry
void https_connect_retry(int fd, int revents, void *data)
{
	Download *handle = data;
	https_connect(handle);
}

/* Actually do the SSL_connect()
 * Based on unreal_tls_connect() but different return values.
 * @retval 1 connected
 * @retval 0 in progress
 * @retval -1 error, handle freed!
 */
int https_connect(Download *handle)
{
	int ssl_err;
	char *errstr;

	if ((ssl_err = SSL_connect(handle->ssl)) <= 0)
	{
		ssl_err = SSL_get_error(handle->ssl, ssl_err);
		switch(ssl_err)
		{
			case SSL_ERROR_SYSCALL:
				if (ERRNO == P_EINTR || ERRNO == P_EWOULDBLOCK || ERRNO == P_EAGAIN)
				{
					/* Hmmm. This implementation is different than in unreal_tls_accept().
					 * One of them must be wrong -- better check! (TODO)
					 */
					fd_setselect(handle->fd, FD_SELECT_READ|FD_SELECT_WRITE, https_connect_retry, handle);
					return 0;
				}
				return https_fatal_tls_error(ssl_err, ERRNO, handle);
			case SSL_ERROR_WANT_READ:
				fd_setselect(handle->fd, FD_SELECT_READ, https_connect_retry, handle);
				fd_setselect(handle->fd, FD_SELECT_WRITE, NULL, handle);
				return 0;
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(handle->fd, FD_SELECT_READ, NULL, handle);
				fd_setselect(handle->fd, FD_SELECT_WRITE, https_connect_retry, handle);
				return 0;
			default:
				return https_fatal_tls_error(ssl_err, ERRNO, handle); /* -1 */
		}
		/* NOTREACHED */
		return 0;
	}

	/* We are connected now. */

	if (!verify_certificate(handle->ssl, handle->hostname, &errstr))
		return https_cancel(handle, "TLS Certificate error for server: %s", errstr); /* -1 */

	return https_connect_send_header(handle);
}

/**
 * Report a fatal TLS error and terminate the download.
 *
 * @param ssl_error The error as from OpenSSL.
 * @param where The location, one of the SAFE_SSL_* defines.
 * @param my_errno A preserved value of errno to pass to ssl_error_str().
 * @param client The client the error is associated with.
 * @returns Always -1
 */
int https_fatal_tls_error(int ssl_error, int my_errno, Download *handle)
{
	const char *ssl_errstr;
	unsigned long additional_errno = ERR_get_error();
	char additional_info[256];
	const char *one, *two;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	/* Fetch additional error information from OpenSSL 3.0.0+ */
	two = ERR_reason_error_string(additional_errno);
	if (two && *two)
	{
		snprintf(additional_info, sizeof(additional_info), ": %s", two);
	} else {
		*additional_info = '\0';
	}
#else
	/* Fetch additional error information from OpenSSL. This is new as of Nov 2017 (4.0.16+) */
	one = ERR_func_error_string(additional_errno);
	two = ERR_reason_error_string(additional_errno);
	if (one && *one && two && *two)
	{
		snprintf(additional_info, sizeof(additional_info), ": %s: %s", one, two);
	} else {
		*additional_info = '\0';
	}
#endif

	ssl_errstr = ssl_error_str(ssl_error, my_errno);

	https_cancel(handle, "%s [%s]", ssl_errstr, additional_info);
	return -1;
}

// copied 100% from modulemanager parse_url()
int url_parse(const char *url, char **hostname, int *port, char **username, char **password, char **document)
{
	char *p, *p2;
	static char hostbuf[256];
	static char documentbuf[512];

	*hostname = *username = *password = *document = NULL;
	*port = 443;

	if (strncmp(url, "https://", 8))
		return 0;
	url += 8; /* skip over https:// part */

	p = strchr(url, '/');
	if (!p)
		return 0;

	strlncpy(hostbuf, url, sizeof(hostbuf), p - url);

	strlcpy(documentbuf, p, sizeof(documentbuf));

	*hostname = hostbuf;
	*document = documentbuf;

	/* Actually we may still need to extract the port */
	p = strchr(hostbuf, '@');
	if (p)
	{
		*p++ = '\0';

		*username = hostbuf;
		p2 = strchr(hostbuf, ':');
		if (p2)
		{
			*p2++ = '\0';
			*password = p2;
		}
		*hostname = p;
	}
	p = strchr(*hostname, ':');
	if (p)
	{
		*p++ = '\0';
		*port = atoi(p);
	}

	return 1;
}

int https_connect_send_header(Download *handle)
{
	char buf[8192];
	char hostandport[512];
	int ssl_err;
	char *host;
	int port;
	char *document;

	handle->connected = 1;
	snprintf(hostandport, sizeof(hostandport), "%s:%d", handle->hostname, handle->port);

	/* Prepare the header */
	if (handle->request->http_method == HTTP_METHOD_GET)
	{
		snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\n"
				    "User-Agent: UnrealIRCd %s\r\n"
				    "Host: %s\r\n"
				    "Connection: close\r\n",
				    handle->document,
				    VERSIONONLY,
				    hostandport);
	} else
	if (handle->request->http_method == HTTP_METHOD_POST)
	{
		if (!handle->request->body || !strlen(handle->request->body))
		{
			snprintf(buf, sizeof(buf), "POST %s HTTP/1.1\r\n"
					    "User-Agent: UnrealIRCd %s\r\n"
					    "Host: %s\r\n"
					    "Connection: close\r\n",
					    handle->document,
					    VERSIONONLY,
					    hostandport);
		} else {
			char add_default_content_type = 0;
			if (!find_nvplist(handle->request->headers, "Content-Type"))
				add_default_content_type = 1;

			snprintf(buf, sizeof(buf), "POST %s HTTP/1.1\r\n"
					    "User-Agent: UnrealIRCd %s\r\n"
					    "Host: %s\r\n"
					    "%s"
					    "Content-Length: %ld\r\n"
					    "Connection: close\r\n",
					    handle->document,
					    VERSIONONLY,
					    hostandport,
					    add_default_content_type ? "Content-Type: application/x-www-form-urlencoded\r\n" : "",
					    (long)strlen(handle->request->body));
		}
	} else
		abort();
	if (handle->username && handle->password)
	{
		char wbuf[128];
		char obuf[256];
		char header[512];

		snprintf(wbuf, sizeof(wbuf), "%s:%s", handle->username, handle->password);
		if (b64_encode(wbuf, strlen(wbuf), obuf, sizeof(obuf)-1) > 0)
		{
			snprintf(header, sizeof(header), "Authorization: Basic %s\r\n", obuf);
			strlcat(buf, header, sizeof(buf));
		}
	}
	if (handle->request->cachetime > 0)
	{
		const char *datestr = rfc2616_time(handle->request->cachetime);
		if (datestr)
		{
			// snprintf_append...
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
				 "If-Modified-Since: %s\r\n", datestr);
		}
	}
	if (handle->request->headers)
	{
		NameValuePrioList *n;
		char nbuf[256];

		for (n = handle->request->headers; n; n = n->next)
		{
			if (n->value)
				snprintf(nbuf, sizeof(nbuf), "%s: %s\r\n", n->name, n->value);
			else
				snprintf(nbuf, sizeof(nbuf), "%s:\r\n", n->name);
			if (strlen(buf)+strlen(nbuf) > sizeof(buf)-8)
				break;
			strlcat(buf, nbuf, sizeof(buf));
		}
	}
	strlcat(buf, "\r\n", sizeof(buf));
	if (handle->request->body)
		strlcat(buf, handle->request->body, sizeof(buf));

	ssl_err = SSL_write(handle->ssl, buf, strlen(buf));
	if (ssl_err < 0)
		return https_fatal_tls_error(ssl_err, ERRNO, handle);
	fd_setselect(handle->fd, FD_SELECT_WRITE, NULL, handle);
	fd_setselect(handle->fd, FD_SELECT_READ, https_receive_response, handle);
	return 1;
}

void https_receive_response(int fd, int revents, void *data)
{
	Download *handle = data;
	int n;
	char readbuf[2048];

	n = SSL_read(handle->ssl, readbuf, sizeof(readbuf)-1);
	if (n == 0)
	{
		/* Graceful close */
		https_done(handle);
		return;
	}
	if (n < 0)
	{
		int ssl_err = SSL_get_error(handle->ssl, n);
		switch (ssl_err)
		{
			case SSL_ERROR_WANT_WRITE:
				fd_setselect(fd, FD_SELECT_READ, NULL, handle);
				fd_setselect(fd, FD_SELECT_WRITE, https_receive_response, handle);
				return;
			case SSL_ERROR_WANT_READ:
				/* Wants to read more data; let it call us next time again */
				return;
			case SSL_ERROR_SYSCALL:
			case SSL_ERROR_SSL:
			default:
				https_fatal_tls_error(ssl_err, ERRNO, handle);
				return;
		}
		return;
	}
	readbuf[n] = '\0';

	//fprintf(stderr, "Got: '%s'\n", readbuf);

	if (!handle->got_response)
	{
		https_handle_response_header(handle, readbuf, n);
		return;
	} else
	if (handle->got_response)
	{
		if (!https_handle_response_body(handle, readbuf, n))
			return; /* handle is already freed! */
	}
}

// Based on websocket_handle_handshake()
int https_handle_response_header(Download *handle, char *readbuf, int n)
{
	char *key, *value;
	int r, end_of_request;
	char netbuf[4096], netbuf2[4096];
	char *lastloc = NULL;
	int maxcopy, nprefix=0;
	int totalsize;

	/* Yeah, totally paranoid: */
	memset(netbuf, 0, sizeof(netbuf));
	memset(netbuf2, 0, sizeof(netbuf2));

	/** Frame re-assembling starts here **/
	*netbuf = '\0';
	if (handle->lefttoparse)
	{
		strlcpy(netbuf, handle->lefttoparse, sizeof(netbuf));
		nprefix = strlen(netbuf);
	}
	maxcopy = sizeof(netbuf) - nprefix - 1;
	/* (Need to some manual checking here as strlen() can't be safely used
	 *  on readbuf. Same is true for strlncat since it uses strlen().)
	 */
	if (n > maxcopy)
		n = maxcopy;
	if (n <= 0)
	{
		https_cancel(handle, "Oversized line in HTTP response");
		return 0;
	}
	memcpy(netbuf+nprefix, readbuf, n); /* SAFE: see checking above */
	totalsize = n + nprefix;
	netbuf[totalsize] = '\0';
	memcpy(netbuf2, netbuf, totalsize+1); // copy, including the "always present \0 at the end just in case we use strstr etc".
	safe_free(handle->lefttoparse);

	/** Now step through the lines.. **/
	for (r = https_parse_header(netbuf, strlen(netbuf), &key, &value, &lastloc, &end_of_request);
	     r;
	     r = https_parse_header(NULL, 0, &key, &value, &lastloc, &end_of_request))
	{
		// do something actually with the header here ;)
		if (!strcasecmp(key, "RESPONSE"))
		{
			handle->http_status_code = atoi(value);
			if (handle->http_status_code == 304)
			{
				/* 304 Not Modified: cache hit */
				https_done_cached(handle);
				return 0;
			}
			else if ((handle->http_status_code >= 301) && (handle->http_status_code <= 308))
			{
				/* Redirect */
				if (handle->request->max_redirects == 0)
				{
					https_cancel(handle, "Too many HTTP redirects (%d)", DOWNLOAD_MAX_REDIRECTS);
					return 0;
				}
				/* Let it continue.. we handle it later, as we need to
				 * receive the "Location" header as well.
				 */
			}
			else if (handle->http_status_code != 200)
			{
				/* HTTP Failure code */
				https_cancel(handle, "HTTP Error: %s", value);
				return 0;
			}
		} else
		if (!strcasecmp(key, "Last-Modified") && value)
		{
			handle->last_modified = rfc2616_time_to_unix_time(value);
		} else
		if (!strcasecmp(key, "Location") && value)
		{
			safe_strdup(handle->redirect_new_location, value);
		} else
		if (!strcasecmp(key, "Transfer-Encoding") && value)
		{
			if (value && !strcasecmp(value, "chunked"))
				handle->transfer_encoding = TRANSFER_ENCODING_CHUNKED;
		}
		//fprintf(stderr, "\nHEADER '%s'\n\n", key);
	}

	if (end_of_request)
	{
		int remaining_bytes = 0;
		char *nextframe;

		safe_free(handle->lefttoparse);
		handle->got_response = 1;

		if (handle->http_status_code == 0)
		{
			https_cancel(handle, "Invalid HTTP response");
			return 0;
		}
		if (handle->http_status_code != 200)
		{
			if (handle->redirect_new_location)
			{
				https_redirect(handle);
				return 0; /* this old request dies */
			} else {
				https_cancel(handle, "HTTP Redirect encountered but no URL specified!?");
				return 0;
			}
		}

		nextframe = url_find_end_of_request(netbuf2, totalsize, &remaining_bytes);
		if (nextframe)
		{
			if (!https_handle_response_body(handle, nextframe, remaining_bytes))
				return 0;
		}
	}

	if (lastloc)
	{
		/* Last line was cut somewhere, save it for next round. */
		safe_strdup(handle->lefttoparse, lastloc);
	}

	return 1;
}

int https_handle_response_body_memory(Download *handle, const char *ptr, int write_sz)
{
	// DUPLICATE CODE: same as src/url_curl.c, well... sortof
	int size_required = handle->memory_data_len + write_sz;

	if (handle->memory_data == NULL)
		return 0; /* Normally does not happen as it is preallocated, but could happen upon unwinding cancels.. */

	if (size_required >= handle->memory_data_allocated - 1) // the -1 is for zero termination, even though it is binary..
	{
		int newsize = ((size_required / URL_MEMORY_BACKED_CHUNK_SIZE)+1)*URL_MEMORY_BACKED_CHUNK_SIZE;
		char *newptr = realloc(handle->memory_data, newsize);
		if (!newptr)
		{
			unreal_log(ULOG_ERROR, "url", "URL_DOWNLOAD_MEMORY", NULL, "Async URL callback failed when reading returned data: out of memory?");
			safe_free(handle->memory_data);
			handle->memory_data_len = 0;
			handle->memory_data_allocated = 0;
			return 0;
		}
		handle->memory_data = newptr;
		handle->memory_data_allocated = newsize;
		/* fill rest with zeroes, yeah.. no trust! ;D */
		memset(handle->memory_data + handle->memory_data_len, 0, handle->memory_data_allocated - handle->memory_data_len);
	}

	memcpy(handle->memory_data + handle->memory_data_len, ptr, write_sz);
	handle->memory_data_len += write_sz;
	handle->memory_data[handle->memory_data_len] = '\0';
	return write_sz;
}

int https_handle_response_body(Download *handle, char *readbuf, int pktsize)
{
	char *buf;
	long long n;
	char *free_this_buffer = NULL;

	// TODO we fail to check for write errors ;)
	// TODO: Makes sense to track if we got everything? :D

	if (handle->transfer_encoding == TRANSFER_ENCODING_NONE)
	{
		/* Ohh.. so easy! */
		if (handle->request->store_in_file == 0)
			https_handle_response_body_memory(handle, readbuf, pktsize);
		else if (handle->file_fd)
			fwrite(readbuf, 1, pktsize, handle->file_fd);
		return 1;
	}

	/* Fill 'buf' nd set 'buflen' with what we had + what we have now.
	 * Makes things easy.
	 */
	if (handle->lefttoparse)
	{
		n = handle->lefttoparselen + pktsize;
		free_this_buffer = buf = safe_alloc(n);
		memcpy(buf, handle->lefttoparse, handle->lefttoparselen);
		memcpy(buf+handle->lefttoparselen, readbuf, pktsize);
		safe_free(handle->lefttoparse);
		handle->lefttoparselen = 0;
	} else {
		n = pktsize;
		buf = readbuf;
	}

	/* Chunked transfers.. yayyyy.. */
	while (n > 0)
	{
		if (handle->chunk_remaining > 0)
		{
			/* Eat it */
			int eat = MIN(handle->chunk_remaining, n);
			if (handle->request->store_in_file == 0)
				https_handle_response_body_memory(handle, buf, eat);
			else if (handle->file_fd)
				fwrite(buf, 1, eat, handle->file_fd);
			n -= eat;
			buf += eat;
			handle->chunk_remaining -= eat;
		} else
		{
			int gotlf = 0;
			int i;

			/* First check if it is a (trailing) empty line,
			 * eg from a previous chunk. Skip over.
			 */
			if ((n >= 2) && !strncmp(buf, "\r\n", 2))
			{
				buf += 2;
				n -= 2;
			} else
			if ((n >= 1) && !strncmp(buf, "\n", 1))
			{
				buf++;
				n--;
			}

			/* Now we are (possibly) at the chunk size line,
			 * this is or example '7f' + newline.
			 * So first, check if we have a newline at all.
			 */
			for (i=0; i < n; i++)
			{
				if (buf[i] == '\n')
				{
					gotlf = 1;
					break;
				}
			}
			if (!gotlf)
			{
				/* The line telling us the chunk size is incomplete,
				 * as it does not contain an \n. Wait for more data
				 * from the network socket.
				 */
				if (n > 0)
				{
					/* Store what we have first.. */
					handle->lefttoparselen = n;
					handle->lefttoparse = safe_alloc(n);
					memcpy(handle->lefttoparse, buf, n);
				}
				safe_free(free_this_buffer);
				return 1; /* WE WANT MORE! */
			}
			buf[i] = '\0'; /* cut at LF */
			i++; /* point to next data */
			handle->chunk_remaining = strtol(buf, NULL, 16);
			if (handle->chunk_remaining < 0)
			{
				https_cancel(handle, "Negative chunk encountered (%ld)", handle->chunk_remaining);
				safe_free(free_this_buffer);
				return 0;
			}
			if (handle->chunk_remaining == 0)
			{
				https_done(handle);
				safe_free(free_this_buffer);
				return 0;
			}
			buf += i;
			n -= i;
		}
	}

	safe_free(free_this_buffer);
	return 1;
}

void https_done(Download *handle)
{
	if (handle->file_fd)
	{
		fclose(handle->file_fd);
		handle->file_fd = NULL;
	}

	if (!handle->got_response)
		url_callback(handle->request, NULL, NULL, 0, "HTTPS response not received", 0, handle->request->callback_data);
	else
	{
		if ((handle->last_modified > 0) && handle->filename)
			unreal_setfilemodtime(handle->filename, handle->last_modified);
		url_callback(handle->request, handle->filename, handle->memory_data, handle->memory_data_len, NULL, 0, handle->request->callback_data);
	}
	url_free_handle(handle);
	return;
}

void https_done_cached(Download *handle)
{
	if (handle->file_fd)
	{
		fclose(handle->file_fd);
		handle->file_fd = NULL;
	}
	url_callback(handle->request, NULL, NULL, 0, NULL, 1, handle->request->callback_data);
	url_free_handle(handle);
}

void https_redirect(Download *handle)
{
	OutgoingWebRequest *r;

	if (handle->request->max_redirects == 0)
	{
		https_cancel(handle, "Too many HTTP redirects (%d)", DOWNLOAD_MAX_REDIRECTS);
		return;
	}

	r = duplicate_outgoingwebrequest(handle->request);
	safe_strdup(r->actual_url, handle->redirect_new_location); // override actual url
	r->max_redirects--; // safe, checked to be >0 a few lines up
	url_free_handle(handle); // free old handle
	url_start_async(r); // create new one
}

/** Helper function to parse the HTTP header consisting of multiple 'Key: value' pairs */
int https_parse_header(char *buffer, int len, char **key, char **value, char **lastloc, int *end_of_request)
{
	static char buf[4096], *nextptr;
	char *p;
	char *k = NULL, *v = NULL;
	int foundlf = 0;

	if (buffer)
	{
		/* Initialize */
		if (len > sizeof(buf) - 1)
			len = sizeof(buf) - 1;

		memcpy(buf, buffer, len);
		buf[len] = '\0';
		nextptr = buf;
	}

	*end_of_request = 0;

	p = nextptr;

	if (!p)
	{
		*key = *value = NULL;
		return 0; /* done processing data */
	}

	if (!strncmp(p, "\n", 1) || !strncmp(p, "\r\n", 2))
	{
		*key = *value = NULL;
		*end_of_request = 1;
		// new compared to websocket handling:
		if (*p == '\n')
			*lastloc = p + 1;
		else
			*lastloc = p + 2;
		return 0;
	}

	/* Note: p *could* point to the NUL byte ('\0') */

	/* Special handling for response line itself. */
	if (!strncmp(p, "HTTP/1", 6) && (strlen(p)>=13))
	{
		k = "RESPONSE";
		p += 9;
		v = p; /* SET VALUE */
		nextptr = NULL; /* set to "we are done" in case next for loop fails */
		for (; *p; p++)
		{
			if (*p == '\r')
			{
				*p = '\0'; /* eat silently, but don't consider EOL */
			}
			else if (*p == '\n')
			{
				*p = '\0';
				nextptr = p+1; /* safe, there is data or at least a \0 there */
				break;
			}
		}
		*key = k;
		*value = v;
		return 1;
	}

	/* Header parsing starts here.
	 * Example line "Host: www.unrealircd.org"
	 */
	k = p; /* SET KEY */

	/* First check if the line contains a terminating \n. If not, don't process it
	 * as it may have been a cut header.
	 */
	for (; *p; p++)
	{
		if (*p == '\n')
		{
			foundlf = 1;
			break;
		}
	}

	if (!foundlf)
	{
		*key = *value = NULL;
		*lastloc = k;
		return 0;
	}

	p = k;

	for (; *p; p++)
	{
		if ((*p == '\n') || (*p == '\r'))
		{
			/* Reached EOL but 'value' not found */
			*p = '\0';
			break;
		}
		if (*p == ':')
		{
			*p++ = '\0';
			if (*p++ != ' ')
				break; /* missing mandatory space after ':' */

			v = p; /* SET VALUE */
			nextptr = NULL; /* set to "we are done" in case next for loop fails */
			for (; *p; p++)
			{
				if (*p == '\r')
				{
					*p = '\0'; /* eat silently, but don't consider EOL */
				}
				else if (*p == '\n')
				{
					*p = '\0';
					nextptr = p+1; /* safe, there is data or at least a \0 there */
					break;
				}
			}
			/* A key-value pair was succesfully parsed, return it */
			*key = k;
			*value = v;
			return 1;
		}
	}

	/* Fatal parse error */
	*key = *value = NULL;
	return 0;
}

/** Check if there is any data at the end of the request */
char *url_find_end_of_request(char *header, int totalsize, int *remaining_bytes)
{
	char *nextframe1;
	char *nextframe2;
	char *nextframe = NULL;

	// find first occurance, yeah this is just stupid, but it works.
	nextframe1 = strstr(header, "\r\n\r\n"); // = +4
	nextframe2 = strstr(header, "\n\n");     // = +2
	if (nextframe1 && nextframe2)
	{
		if (nextframe1 < nextframe2)
		{
			nextframe = nextframe1 + 4;
		} else {
			nextframe = nextframe2 + 2;
		}
	} else
	if (nextframe1)
	{
		nextframe = nextframe1 + 4;
	} else
	if (nextframe2)
	{
		nextframe = nextframe2 + 2;
	}
	if (nextframe)
	{
		*remaining_bytes = totalsize - (nextframe - header);
		if (*remaining_bytes > 0)
			return nextframe;
	}
	return NULL;
}

/* Handle timeouts. */
EVENT(url_socket_timeout)
{
	Download *d, *d_next;
	for (d = downloads; d; d = d_next)
	{
		d_next = d->next;
		if (d->dns_refcnt)
			continue; /* can't touch this... */
		if (!d->connected && (TStime() - d->download_started > d->request->connect_timeout))
		{
			https_cancel(d, "Connect or DNS timeout after %d seconds", d->request->connect_timeout);
			continue;
		}
		if (d->connected && (TStime() - d->download_started > d->request->transfer_timeout))
		{
			https_cancel(d, "Download timeout after %d seconds", d->request->transfer_timeout);
			continue;
		}
	}
}

void url_init(void)
{
	https_ctx = https_new_ctx();
	if (!https_ctx)
	{
		unreal_log(ULOG_ERROR, "url", "HTTPS_NEW_CTX_FAILED", NULL,
			   "Unable to initialize SSL context");
		exit(-1);
	}
	EventAdd(NULL, "url_socket_timeout", url_socket_timeout, NULL, 500, 0);
}

char *urlencode(const char *s, char *wbuf, int wlen)
{
	const char hexchars[16] = "0123456789abcdef";
	char *o = wbuf;

	if (s == NULL)
	{
		if (wlen > 0)
			*o = '\0';
		return NULL;
	}

	for (; *s; s++)
	{
		if (((*s >= 'a') && (*s <= 'z')) ||
		    ((*s >= 'A') && (*s <= 'Z')) ||
		    ((*s >= '0') && (*s <= '9')) ||
		    strchr("-._~", *s))
		{
			if (wlen-- <= 1)
				break;
			*o++ = *s;
		} else
		{
			if (wlen <= 3)
				break;
			wlen -= 3;
			*o++ = '%';
			*o++ = hexchars[(*s >> 4) & 0xF];
			*o++ = hexchars[*s & 0xF];
		}
	}
	/* And zero-terminate.. */
	if (wlen)
		*o = '\0';

	return wbuf;
}

int downloads_in_progress(void)
{
	Download *d;
	int count = 0;

	/* Bit stupid to do it this slow way, can't we maintain a counter? Needs to be accurate though */

	for (d = downloads; d; d = d->next)
		count++;

	return count;
}
