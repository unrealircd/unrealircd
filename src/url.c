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

extern char *SSLKeyPasswd;

#ifndef _WIN32
extern uid_t irc_uid;
extern gid_t irc_gid;
#endif

CURLM *multihandle;

/* Stores information about the async transfer.
 * Used to maintain information about the transfer
 * to trigger the callback upon completion.
 */
typedef struct
{
	vFP callback;
	void *callback_data;
	FILE *fd;
	char filename[PATH_MAX];
	char *url; /*< must be free()d by url_do_transfers_async() */
	char errorbuf[CURL_ERROR_SIZE];
	time_t cachetime;
} FileHandle;

/*
 * Determines if the given string is a valid URL. Since libcurl
 * supports telnet, ldap, and dict such strings are treated as
 * invalid URLs here since we don't want them supported in
 * unreal.
 */
int url_is_valid(const char *string)
{
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
	if (SSLKeyPasswd)
		curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, SSLKeyPasswd);
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
 * Handles synchronous downloading of a file. This function allows
 * a download to be made transparently without the caller having any
 * knowledge of how libcurl works. If the function succeeds, the
 * filename the file was downloaded to is returned. Otherwise NULL
 * is returned and the string pointed to by error contains the error
 * message. The returned filename is malloc'ed and must be freed by
 * the caller.
 */
char *download_file(const char *url, char **error)
{
	static char errorbuf[CURL_ERROR_SIZE];
	CURL *curl = curl_easy_init();
	CURLcode res;
	char *file = url_getfilename(url);
	char *filename = unreal_getfilename(file);
	char *tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");
	FILE *fd;


	if (!curl)
	{
		safe_free(file);
		strlcpy(errorbuf, "curl_easy_init() failed", sizeof(errorbuf));
		*error = errorbuf;
		return NULL;
	}

	fd = fopen(tmp, "wb");
	if (!fd)
	{
		snprintf(errorbuf, CURL_ERROR_SIZE, "Cannot write to %s: %s", tmp, strerror(errno));
		safe_free(file);
		*error = errorbuf;
		return NULL;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
	curl_easy_setopt(curl, CURLOPT_FILETIME, 1);
 	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
 	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45);
 	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15);
#if LIBCURL_VERSION_NUM >= 0x070f01
 	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
 	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 1);
#endif
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

	set_curl_tls_options(curl);
	memset(errorbuf, 0, CURL_ERROR_SIZE);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);
	res = curl_easy_perform(curl);
	fclose(fd);
	safe_free(file);
	if (res == CURLE_OK)
	{
		long last_mod;

		curl_easy_getinfo(curl, CURLINFO_FILETIME, &last_mod);
		curl_easy_cleanup(curl);

		if (last_mod != -1)
			unreal_setfilemodtime(tmp, last_mod);
		return raw_strdup(tmp);
	}
	else
	{
		curl_easy_cleanup(curl);
		remove(tmp);
		*error = errorbuf;
		return NULL;
	}
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
			FileHandle *handle;
			long code;
			long last_mod;
			CURL *easyhand = msg->easy_handle;

			curl_easy_getinfo(easyhand, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(easyhand, CURLINFO_PRIVATE, (char **) &handle);
			curl_easy_getinfo(easyhand, CURLINFO_FILETIME, &last_mod);
			fclose(handle->fd);

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

			safe_free(handle->url);
			safe_free(handle);
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
	Debug((DEBUG_DEBUG, "url_socket_cb: %d (%s)", (int)s, (what == CURL_POLL_REMOVE)?"remove":"add-or-modify"));
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
static EVENT(curl_socket_timeout)
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
	CURL *curl = curl_easy_init();
	if (curl)
	{
		char *file = url_getfilename(url);
		char *filename = unreal_getfilename(file);
		char *tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");
		FileHandle *handle = safe_alloc(sizeof(FileHandle));
		handle->fd = fopen(tmp, "wb");
		if (!handle->fd)
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
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle->fd);
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
}
