/*
 *   Unreal Internet Relay Chat Daemon, src/url.c
 *   (C) 2003 Dominick Meglio and the UnrealIRCd Team
 *   (C) 2012 William Pitcock <nenolod@dereferenced.org>
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
#include "dns.h"

extern char *TLSKeyPasswd;

#ifndef CURL_ERROR_SIZE
 #define CURL_ERROR_SIZE 512
#endif

/* Stores information about the async transfer.
 * Used to maintain information about the transfer
 * to trigger the callback upon completion.
 */
typedef struct Download Download;

struct Download
{
#ifndef USE_LIBCURL
	Download *prev, *next;
#endif
	vFP callback;
	void *callback_data;
	FILE *file_fd;		/**< File open for writing (otherwise NULL) */
	char filename[PATH_MAX];
	char *url; /*< must be free()d by url_do_transfers_async() */
	char errorbuf[CURL_ERROR_SIZE];
	time_t cachetime;
#ifndef USE_LIBCURL
	char *hostname;		/**< Parsed hostname (from 'url') */
	int port;		/**< Parsed port (from 'url') */
	char *document;		/**< Parsed document (from 'url') */
	char *ip;		/**< Resolved IP */
	int ipv6;
	SSL *ssl;
	int fd;			/**< Socket */
	int got_response;
	char *lefttoparse;
	time_t last_modified;
#endif
};

#ifdef USE_LIBCURL
CURLM *multihandle = NULL;
#else
Download *downloads = NULL;
#endif

/*
 * Determines if the given string is a valid URL. Since libcurl
 * supports telnet, ldap, and dict such strings are treated as
 * invalid URLs here since we don't want them supported in
 * unreal.
 */
int url_is_valid(const char *string)
{
	if (strstr(string, " ") || strstr(string, "\t"))
		return 0;

	if (strstr(string, "telnet://") == string ||
	    strstr(string, "ldap://") == string ||
	    strstr(string, "dict://") == string)
	{
		return 0;
	}
	return (strstr(string, "://") != NULL);
}

/** A displayable URL for in error messages and such.
 * This leaves out any authentication information (user:pass)
 * the URL may contain.
 */
const char *displayurl(const char *url)
{
	static char buf[512];
	char *proto, *rest;

	/* protocol://user:pass@host/etc.. */
	rest = strchr(url, '@');

	if (!rest)
		return url; /* contains no auth information */

	rest++; /* now points to the rest (remainder) of the URL */

	proto = strstr(url, "://");
	if (!proto || (proto > rest) || (proto == url))
		return url; /* incorrectly formatted, just show entire URL. */

	/* funny, we don't ship strlncpy.. */
	*buf = '\0';
	strlncat(buf, url, sizeof(buf), proto - url);
	strlcat(buf, "://***:***@", sizeof(buf));
	strlcat(buf, rest, sizeof(buf));

	return buf;
}

/*
 * Returns the filename portion of the URL. The returned string
 * is malloc()'ed and must be freed by the caller. If the specified
 * URL does not contain a filename, a '-' is allocated and returned.
 */
char *url_getfilename(const char *url)
{
	const char *c, *start;

	if ((c = strstr(url, "://")))
		c += 3;
	else
		c = url;

	while (*c && *c != '/')
		c++;

	if (*c == '/')
	{
		c++;
		if (!*c || *c == '?')
			return raw_strdup("-");
		start = c;
		while (*c && *c != '?')
			c++;
		if (!*c)
			return raw_strdup(start);
		else
			return raw_strldup(start, c-start+1);

	}
	return raw_strdup("-");
}

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
	safe_free(handle->url);
	safe_free(handle->lefttoparse);
	safe_free(handle);
}

#ifdef USE_LIBCURL
/*
 * Sets up all of the SSL options necessary to support HTTPS/FTPS
 * transfers.
 */
static void set_curl_tls_options(CURL *curl)
{
	char buf[512];

#if 0
	/* This would only be necessary if you use client certificates over HTTPS and such.
	 * But this information is not known yet since the configuration file has not been
	 * parsed yet at this point.
	 */
	curl_easy_setopt(curl, CURLOPT_SSLCERT, iConf.tls_options->certificate_file);
	if (TLSKeyPasswd)
		curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, TLSKeyPasswd);
	curl_easy_setopt(curl, CURLOPT_SSLKEY, iConf.tls_options->key_file);
#endif

	snprintf(buf, sizeof(buf), "%s/tls/curl-ca-bundle.crt", CONFDIR);
	curl_easy_setopt(curl, CURLOPT_CAINFO, buf);
}

/*
 * Used by CURLOPT_WRITEFUNCTION to actually write the data to
 * a stream.
 */
static size_t do_download(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return fwrite(ptr, size, nmemb, (FILE *)stream);
}

/*
 * Interface for new-style evented I/O.
 *
 * url_socket_pollcb is the callback from our eventing system into
 * cURL.
 *
 * The other callbacks are for cURL notifying our event system what
 * it wants to do.
 */
static void url_check_multi_handles(void)
{
	CURLMsg *msg;
	int msgs_left;

	while ((msg = curl_multi_info_read(multihandle, &msgs_left)) != NULL)
	{
		if (msg->msg == CURLMSG_DONE)
		{
			Download *handle;
			long code;
			long last_mod;
			CURL *easyhand = msg->easy_handle;

			curl_easy_getinfo(easyhand, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(easyhand, CURLINFO_PRIVATE, (char **) &handle);
			curl_easy_getinfo(easyhand, CURLINFO_FILETIME, &last_mod);
			fclose(handle->file_fd);
			handle->file_fd = NULL;

			if (msg->data.result == CURLE_OK)
			{
				if (code == 304 || (last_mod != -1 && last_mod <= handle->cachetime))
				{
					handle->callback(handle->url, NULL, NULL, 1, handle->callback_data);
					remove(handle->filename);
				}
				else
				{
					if (last_mod != -1)
						unreal_setfilemodtime(handle->filename, last_mod);

					handle->callback(handle->url, handle->filename, NULL, 0, handle->callback_data);
					remove(handle->filename);
				}
			}
			else
			{
				handle->callback(handle->url, NULL, handle->errorbuf, 0, handle->callback_data);
				remove(handle->filename);
			}

			handle->fd = -1; /* curl will close it */
			url_free_handle(handle);
			curl_multi_remove_handle(multihandle, easyhand);

			/* NOTE: after curl_multi_remove_handle() you cannot use
			 * 'msg' anymore because it has freed by curl (as of v7.11.0),
			 * therefore 'easyhand' is used... fun! -- Syzop
			 */
			curl_easy_cleanup(easyhand);
		}
	}
}

static void url_socket_pollcb(int fd, int revents, void *data)
{
	int flags = 0;
	int dummy;

	if (revents & FD_SELECT_READ)
		flags |= CURL_CSELECT_IN;
	if (revents & FD_SELECT_WRITE)
		flags |= CURL_CSELECT_OUT;

	curl_multi_socket_action(multihandle, fd, flags, &dummy);
	url_check_multi_handles();
}

static int url_socket_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	if (what == CURL_POLL_REMOVE)
	{
		fd_close(s);
	}
	else
	{
		FDEntry *fde = &fd_table[s];
		int flags = 0;

		if (!fde->is_open)
		{
			/* NOTE: We use FDCLOSE_NONE here because cURL will take
			 * care of the closing of the socket. So *WE* must never
			 * close the socket ourselves.
			 */
			fd_open(s, "CURL transfer", FDCLOSE_NONE);
		}

		if (what == CURL_POLL_IN || what == CURL_POLL_INOUT)
			flags |= FD_SELECT_READ;

		if (what == CURL_POLL_OUT || what == CURL_POLL_INOUT)
			flags |= FD_SELECT_WRITE;

		fd_setselect(s, flags, url_socket_pollcb, NULL);
	}

	return 0;
}

/* Handle timeouts. */
EVENT(curl_socket_timeout)
{
	int dummy;

	curl_multi_socket_action(multihandle, CURL_SOCKET_TIMEOUT, 0, &dummy);
	url_check_multi_handles();
}

static Event *curl_socket_timeout_hdl = NULL;

/*
 * Initializes the URL system
 */
void url_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	multihandle = curl_multi_init();

	curl_multi_setopt(multihandle, CURLMOPT_SOCKETFUNCTION, url_socket_cb);
	curl_socket_timeout_hdl = EventAdd(NULL, "curl_socket_timeout", curl_socket_timeout, NULL, 500, 0);
}

/*
 * Handles asynchronous downloading of a file. This function allows
 * a download to be made transparently without the caller having any
 * knowledge of how libcurl works. The specified callback function is
 * called when the download completes, or the download fails. The 
 * callback function is defined as:
 *
 * void callback(const char *url, const char *filename, char *errorbuf, int cached, void *data);
 *  - url will contain the original URL used to download the file.
 *  - filename will contain the name of the file (if successful, NULL on error or if cached).
 *    This file will be cleaned up after the callback returns, so save a copy to support caching.
 *  - errorbuf will contain the error message (if failed, NULL otherwise).
 *  - cached 1 if the specified cachetime is >= the current file on the server,
 *    if so, errorbuf will be NULL, filename will contain the path to the file.
 *  - data will be the value of callback_data, allowing you to figure
 *    out how to use the data contained in the downloaded file ;-).
 *    Make sure that if you access the contents of this pointer, you
 *    know that this pointer will persist. A download could take more
 *    than 10 seconds to happen and the config file can be rehashed
 *    multiple times during that time.
 */
void download_file_async(const char *url, time_t cachetime, vFP callback, void *callback_data)
{
	static char errorbuf[CURL_ERROR_SIZE];
	CURL *curl;
	char *file;
	char *filename;
	char *tmp;
	Download *handle;

	curl = curl_easy_init();
	if (!curl)
	{
		unreal_log(ULOG_ERROR, "main", "CURL_INTERNAL_FAILURE", NULL,
		           "Could not initialize curl handle. Maybe out of memory/resources?");
		snprintf(errorbuf, sizeof(errorbuf), "Could not initialize curl handle");
		return;
	}

	file = url_getfilename(url);
	filename = unreal_getfilename(file);
	tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");

	handle = safe_alloc(sizeof(Download));
	handle->file_fd = fopen(tmp, "wb");
	if (!handle->file_fd)
	{
		snprintf(errorbuf, sizeof(errorbuf), "Cannot create '%s': %s", tmp, strerror(ERRNO));
		callback(url, NULL, errorbuf, 0, callback_data);
		safe_free(file);
		safe_free(handle);
		return;
	}

	handle->callback = callback;
	handle->callback_data = callback_data;
	handle->cachetime = cachetime;
	safe_strdup(handle->url, url);
	strlcpy(handle->filename, tmp, sizeof(handle->filename));
	safe_free(file);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle->file_fd);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	set_curl_tls_options(curl);
	memset(handle->errorbuf, 0, CURL_ERROR_SIZE);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, handle->errorbuf);
	curl_easy_setopt(curl, CURLOPT_PRIVATE, (char *)handle);
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1);
	/* We need to set CURLOPT_FORBID_REUSE because otherwise libcurl does not
	 * notify us (or not in time) about FD close/opens, thus we end up closing and
	 * screwing up another innocent FD, like a listener (BAD!). In my view a bug, but
	 * mailing list archives seem to indicate curl devs have a different opinion
	 * on these matters...
	 * Actually I don't know for sure if this option alone fixes 100% of the cases
	 * but at least I can't crash my server anymore.
	 * As a side-effect we also fix useless CLOSE_WAIT connections.
	 */
	curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1);
	if (cachetime)
	{
		curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(curl, CURLOPT_TIMEVALUE, cachetime);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15);
#if LIBCURL_VERSION_NUM >= 0x070f01
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);
#endif

	curl_multi_add_handle(multihandle, curl);
}

#else
/************************* START OF OWN IMPLEMENTION (NON-CURL CASE) **********************/

/* Forward declarations */
void url_resolve_cb(void *arg, int status, int timeouts, struct hostent *he);
void unreal_https_initiate_connect(Download *handle);
int url_parse(const char *url, char **host, int *port, char **document);
SSL_CTX *https_new_ctx(void);
void unreal_https_connect_handshake(int fd, int revents, void *data);
int https_connect(Download *handle);
int https_fatal_tls_error(int ssl_error, int my_errno, Download *handle);
void https_connect_send_header(Download *handle);
void https_receive_response(int fd, int revents, void *data);
int https_handle_response_header(Download *handle, char *readbuf, int n);
int https_handle_response_file(Download *handle, char *readbuf, int n);
void https_done(Download *handle);
void https_done_cached(Download *handle);
int https_parse_header(char *buffer, int len, char **key, char **value, char **lastloc, int *end_of_request);
char *url_find_end_of_request(char *header, int totalsize, int *remaining_bytes);

void download_file_async(const char *url, time_t cachetime, vFP callback, void *callback_data)
{
	static char errorbuf[CURL_ERROR_SIZE];
	char *file;
	char *filename;
	char *tmp;
	Download *handle = NULL;
	int ipv6 = 0;
	char *host;
	int port;
	char *document;

	if (!url_parse(url, &host, &port, &document))
	{
		handle->callback(handle->url, NULL, "Failed to parse HTTP url", 0, handle->callback_data);
		url_free_handle(handle);
		return;
	}

	file = url_getfilename(url);
	filename = unreal_getfilename(file);
	tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");

	handle = safe_alloc(sizeof(Download));
	AddListItem(handle, downloads);
	handle->file_fd = fopen(tmp, "wb");
	if (!handle->file_fd)
	{
		snprintf(errorbuf, sizeof(errorbuf), "Cannot create '%s': %s", tmp, strerror(ERRNO));
		safe_free(file);
		goto fail;
	}

	handle->callback = callback;
	handle->callback_data = callback_data;
	handle->cachetime = cachetime;
	safe_strdup(handle->url, url);
	strlcpy(handle->filename, tmp, sizeof(handle->filename));
	safe_free(file);

	safe_strdup(handle->hostname, host);
	handle->port = port;
	safe_strdup(handle->document, document);

	// todo: allocate handle, select en weetikt allemaal
	// add to some global struct linkedlist, for timeouts
	// register in i/o

	if (is_valid_ip(handle->hostname))
	{
		/* Nothing to resolve, eg https://127.0.0.1/ */
		safe_strdup(handle->ip, handle->hostname);
		unreal_https_initiate_connect(handle);
	} else {
		/* Hostname, so start resolving... */
		ares_gethostbyname(resolver_channel, handle->hostname, AF_INET, url_resolve_cb, handle);
		// TODO: check return value?
	}
	return;

fail:
	callback(url, NULL, errorbuf, 0, callback_data);
	if (handle)
		url_free_handle(handle);
	return;
}

void url_resolve_cb(void *arg, int status, int timeouts, struct hostent *he)
{
	Download *handle = (Download *)arg;
	int n;
	struct hostent *he2;
	char ipbuf[HOSTLEN+1];
	char *ip = NULL;
	char errorbuf[512];

	if ((status != 0) || !he->h_addr_list || !he->h_addr_list[0])
	{
		snprintf(errorbuf, sizeof(errorbuf), "Unable to resolve hostname '%s'", handle->hostname);
		goto fail;
	}

	if (!he->h_addr_list[0] || (he->h_length != (handle->ipv6 ? 16 : 4)) ||
	    !(ip = inetntop(handle->ipv6 ? AF_INET6 : AF_INET, he->h_addr_list[0], ipbuf, sizeof(ipbuf))))
	{
		/* Illegal response -- fatal */
		snprintf(errorbuf, sizeof(errorbuf), "Unable to resolve hostname '%s'", handle->hostname);
		goto fail;
	}

	/* Ok, since we got here, it seems things were actually succesfull */

	safe_strdup(handle->ip, ip);

	unreal_https_initiate_connect(handle);
	return;

fail:
	handle->callback(handle->url, NULL, errorbuf, 0, handle->callback_data);
	url_free_handle(handle);
	return;
}

void unreal_https_initiate_connect(Download *handle)
{
	char errorbuf[512];

	// todo: allocate handle, select en weetikt allemaal
	// add to some global struct linkedlist, for timeouts
	// register in i/o

	if (!handle->ip)
	{
		snprintf(errorbuf, sizeof(errorbuf), "No IP address found to connect to");
		goto fail;
	}

	handle->fd = fd_socket(handle->ipv6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0, "HTTPS");
	if (handle->fd < 0)
	{
		snprintf(errorbuf, sizeof(errorbuf), "Could not create socket: %s", strerror(ERRNO));
		goto fail;
	}
	set_sock_opts(handle->fd, NULL, handle->ipv6);
	if (!unreal_connect(handle->fd, handle->ip, handle->port, handle->ipv6))
	{
		snprintf(errorbuf, sizeof(errorbuf), "Could not connect: %s", strerror(ERRNO));
		goto fail;
	}
	fd_setselect(handle->fd, FD_SELECT_WRITE, unreal_https_connect_handshake, handle);

	return;

fail:
	handle->callback(handle->url, NULL, errorbuf, 0, handle->callback_data);
	url_free_handle(handle);
	return;
}

// based on unreal_tls_client_handshake()
void unreal_https_connect_handshake(int fd, int revents, void *data)
{
	Download *handle = data;
	char errorbuf[CURL_ERROR_SIZE];
	SSL_CTX *ctx;

	ctx = https_new_ctx();
	if (!ctx)
	{
		strlcpy(errorbuf, "Failed to setup SSL CTX", sizeof(errorbuf));
		goto fail;
	}
	handle->ssl = SSL_new(ctx);
	if (!handle->ssl)
	{
		strlcpy(errorbuf, "Failed to setup SSL", sizeof(errorbuf));
		goto fail;
	}
	SSL_set_fd(handle->ssl, handle->fd);
	SSL_set_connect_state(handle->ssl);
	SSL_set_nonblocking(handle->ssl);
	// TODO SNI: SSL_set_tlsext_host_name(handle->ssl, hostname)

	if (https_connect(handle) < 0)
	{
		/* Some fatal error already */
		strlcpy(errorbuf, "TLS_connect() failed early", sizeof(errorbuf));
		goto fail;
	}
	/* Is connecting... */
	return;
fail:
	handle->callback(handle->url, NULL, errorbuf, 0, handle->callback_data);
	url_free_handle(handle);
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
			fprintf(stderr, "ERROR: Neither %s nor %s exist.\n"
			                "Cannot use module manager without curl-ca-bundle.crt\n",
			                buf1, buf2);
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

// Based on unreal_tls_connect()
int https_connect(Download *handle)
{
	int ssl_err;

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
				return https_fatal_tls_error(ssl_err, ERRNO, handle);
		}
		/* NOTREACHED */
		return -1;
	}

	https_connect_send_header(handle);
	return 1;
}

/**
 * Report a fatal TLS error and terminate the download.
 *
 * @param ssl_error The error as from OpenSSL.
 * @param where The location, one of the SAFE_SSL_* defines.
 * @param my_errno A preserved value of errno to pass to ssl_error_str().
 * @param client The client the error is associated with.
 */
int https_fatal_tls_error(int ssl_error, int my_errno, Download *handle)
{
	char *ssl_errstr;
	unsigned long additional_errno = ERR_get_error();
	char additional_info[256];
	char errorbuf[CURL_ERROR_SIZE];
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

	snprintf(errorbuf, sizeof(errorbuf), "%s [%s]", ssl_errstr, additional_info);
	handle->callback(handle->url, NULL, errorbuf, 0, handle->callback_data);
	url_free_handle(handle);

	return -1;
}

// copied 100% from modulemanager parse_url()
int url_parse(const char *url, char **host, int *port, char **document)
{
	char *p;
	static char hostbuf[256];
	static char documentbuf[512];

	if (strncmp(url, "https://", 8))
	{
		fprintf(stderr, "ERROR: URL Must start with https! URL: %s\n", url);
		return 0;
	}
	url += 8; /* skip over https:// part */

	p = strchr(url, '/');
	if (!p)
		return 0;

	*hostbuf = '\0';
	strlncat(hostbuf, url, sizeof(hostbuf), p - url);

	strlcpy(documentbuf, p, sizeof(documentbuf));

	*host = hostbuf;
	*document = documentbuf;

	/* Actually we may still need to extract the port */
	*port = 443;
	p = strchr(hostbuf, ':');
	if (p)
	{
		*p++ = '\0';
		*port = atoi(p);
	}

	return 1;
}

void https_connect_send_header(Download *handle)
{
	char buf[512];
	char hostandport[512];
	int ssl_err;
	char *host;
	int port;
	char *document;

	snprintf(hostandport, sizeof(hostandport), "%s:%d", handle->hostname, handle->port);

	// TODO: remove debug shit
	unreal_log(ULOG_DEBUG, "url", "URL_CONNECTED", NULL, "Connected!!!");

	/* Prepare the header */
	snprintf(buf, sizeof(buf), "GET %s HTTP/1.1\r\n"
	                    "User-Agent: UnrealIRCd %s\r\n"
	                    "Host: %s\r\n"
	                    "Connection: close\r\n",
	                    handle->document,
	                    VERSIONONLY,
	                    hostandport);
	if (handle->cachetime > 0)
	{
		char *datestr = rfc2616_time(handle->cachetime);
		if (datestr)
		{
			// snprintf_append...
			snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf),
				 "If-Modified-Since: %s\r\n", datestr);
		}
	}
	strlcat(buf, "\r\n", sizeof(buf));

	ssl_err = SSL_write(handle->ssl, buf, strlen(buf));
	if (ssl_err < 0)
	{
		https_fatal_tls_error(ssl_err, ERRNO, handle);
		return;
	}
	fd_setselect(handle->fd, FD_SELECT_WRITE, NULL, handle);
	fd_setselect(handle->fd, FD_SELECT_READ, https_receive_response, handle);
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
		if (!https_handle_response_file(handle, readbuf, n))
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
	char errorbuf[512];
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
		strlcpy(errorbuf, "Oversized line in HTTP response", sizeof(errorbuf));
		goto fail;
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
			int http_response = atoi(value);
			if (http_response == 304)
			{
				/* 304 Not Modified: cache hit */
				https_done_cached(handle);
				return 0;
			}
			if (http_response != 200)
			{
				/* HTTP Failure code */
				strlcpy(errorbuf, value, sizeof(errorbuf));
				goto fail;
			}
		} else
		if (!strcasecmp(key, "Last-Modified"))
		{
			handle->last_modified = rfc2616_time_to_unix_time(value);
		}
		//fprintf(stderr, "\nHEADER '%s'\n\n", key);
	}

	if (end_of_request)
	{
		int remaining_bytes = 0;
		char *nextframe;

		safe_free(handle->lefttoparse);
		handle->got_response = 1;

		nextframe = url_find_end_of_request(netbuf2, totalsize, &remaining_bytes);
		if (nextframe)
			https_handle_response_file(handle, nextframe, remaining_bytes);
	}

	if (lastloc)
	{
		/* Last line was cut somewhere, save it for next round. */
		safe_strdup(handle->lefttoparse, lastloc);
	}

	return 1;

fail:
	handle->callback(handle->url, NULL, errorbuf, 0, handle->callback_data);
	url_free_handle(handle);
	return 0;
}

int https_handle_response_file(Download *handle, char *readbuf, int n)
{
	fwrite(readbuf, 1, n, handle->file_fd);

	// TODO we fail to check for write errors ;)

	// TODO: Makes sense to track if we got everything? :D
	return 1;
}

void https_done(Download *handle)
{
	//handle->callback(handle->url, NULL, "SUCCESS!!!!", 0, handle->callback_data);
	fclose(handle->file_fd);
	handle->file_fd = NULL;

	if (!handle->got_response)
		handle->callback(handle->url, NULL, "HTTPS response not received", 0, handle->callback_data);
	else
	{
		if (handle->last_modified > 0)
			unreal_setfilemodtime(handle->filename, handle->last_modified);
		handle->callback(handle->url, handle->filename, NULL, 0, handle->callback_data);
	}
	url_free_handle(handle);
	return;
}

void https_done_cached(Download *handle)
{
	fclose(handle->file_fd);
	handle->file_fd = NULL;
	handle->callback(handle->url, NULL, NULL, 1, handle->callback_data);
	url_free_handle(handle);
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


#endif
