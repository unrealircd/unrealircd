/*
 *   Unreal Internet Relay Chat Daemon, src/url.c
 *   (C) 2003 Dominick Meglio and the UnrealIRCd Team
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

#include "setup.h"
#include "config.h"

#ifdef USE_POLL
# ifndef _WIN32
#  include <poll.h>
#  ifndef POLLRDHUP
#   define POLLRDHUP 0
#  endif
# else
#  define poll WSAPoll
#  define POLLRDHUP POLLHUP
# endif
#endif

#include <string.h>
#include <stdio.h>
#include <struct.h>

#include "proto.h"
#include "h.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <curl/curl.h>

#ifdef USE_SSL
extern char *SSLKeyPasswd;
#endif

#ifndef _WIN32
extern uid_t irc_uid;
extern gid_t irc_gid;
#endif

CURLM *multihandle;

#ifdef USE_POLL
/* Maximum concurrent transfers. with poll support the remote include
 * transfers are no longer included in the MAXCONNECTIONS count.
 */
#define MAXTRANSFERS	256
static struct pollfd url_pollfds[MAXTRANSFERS];
static int url_pollfdcount = 0;
#endif


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
		return 0;
	return (strstr(string, "://") != NULL);
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
                        return strdup("-");
                start = c;
                while (*c && *c != '?')
                        c++;
                if (!*c)
                        return strdup(start);
                else
                {
                        char *file = malloc(c-start+1);
                        strlcpy(file, start, c-start+1);
                        return file;
                }
                return strdup("-");

        }
        return strdup("-");
}

#ifdef USE_SSL
/*
 * Sets up all of the SSL options necessary to support HTTPS/FTPS
 * transfers.
 */
static void set_curl_ssl_options(CURL *curl)
{
	if (USE_EGD)
		curl_easy_setopt(curl, CURLOPT_EGDSOCKET, EGD_PATH);
	curl_easy_setopt(curl, CURLOPT_SSLCERT, SSL_SERVER_CERT_PEM);
	if (SSLKeyPasswd)
		curl_easy_setopt(curl, CURLOPT_SSLKEYPASSWD, SSLKeyPasswd);
	curl_easy_setopt(curl, CURLOPT_SSLKEY, SSL_SERVER_KEY_PEM);
	curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
}
#endif

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
	char *tmp = unreal_mktemp("tmp", filename ? filename : "download.conf");
	FILE *fd;


	if (!curl)
	{
		if (file)
			free(file);
		strlcpy(errorbuf, "curl_easy_init() failed", sizeof(errorbuf));
		*error = errorbuf;
		return NULL;
	}

	fd = fopen(tmp, "wb");
	if (!fd)
	{
		snprintf(errorbuf, CURL_ERROR_SIZE, "Cannot write to %s: %s", tmp, strerror(errno));
		if (file)
			free(file);
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

#ifdef USE_SSL
	set_curl_ssl_options(curl);
#endif
	bzero(errorbuf, CURL_ERROR_SIZE);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);
	res = curl_easy_perform(curl);
	fclose(fd);
#if defined(IRC_USER) && defined(IRC_GROUP)
	if (!loop.ircd_booted)
		chown(tmp, irc_uid, irc_gid);
#endif
	if (file)
		free(file);
	if (res == CURLE_OK)
	{
		long last_mod;

		curl_easy_getinfo(curl, CURLINFO_FILETIME, &last_mod);
		curl_easy_cleanup(curl);

		if (last_mod != -1)
			unreal_setfilemodtime(tmp, last_mod);
		return strdup(tmp);
	}
	else
	{
		curl_easy_cleanup(curl);
		remove(tmp);
		*error = errorbuf;
		return NULL;
	}
}

#ifdef USE_POLL
/* Add a new socket to the poll array, or modify an existing entry */
void url_socket_addormod(curl_socket_t s, int what)
{
struct pollfd *pfd;
short events=0;
int i;

	/* convert 'what' from CURL language to 'events' in POSIX language... */
	if (what == CURL_POLL_IN)
		events |= POLLIN; /* read */
	else if (what == CURL_POLL_OUT)
		events |= POLLOUT; /* write */
	else if (what == CURL_POLL_INOUT)
		events |= (POLLIN|POLLOUT); /* both */

	/* Check if the entry already exists... */
	for (i=0; i < url_pollfdcount; i++)
	{
		pfd = &url_pollfds[i];
		if (pfd->fd == s)
		{
			/* Modify existing entry */
			pfd->events = events;
			return;
		}
	}
	
	/* If we get here, the socket does not exist yet. Add it! */
	if (url_pollfdcount+1 == MAXTRANSFERS)
	{
		/* There's no room for any new fd. Throw up an error.
		 * Since no data will ever be transfered for this session, it wil
		 * eventually timeout. That's ok, better than a crash...
		 */
		ircd_log(LOG_ERROR, "Remote includes: too many concurrent transfers (%d)!!", MAXTRANSFERS);
		sendto_realops("Remote includes: too many concurrent transfers (%d)!!", MAXTRANSFERS);
		return;
	}
	pfd = &url_pollfds[url_pollfdcount];
	pfd->fd = s;
	pfd->events = events;
	url_pollfdcount++;
}

void url_socket_remove(curl_socket_t s)
{
int i;
struct pollfd *pfd;

	for (i=0; i < url_pollfdcount; i++)
	{
		pfd = &url_pollfds[i];
		if (pfd->fd == s)
		{
			/* Tag for deletion. But don't actually delete it as we may be in the poll result loop... */
			pfd->fd = -1;
			pfd->events = 0;
			break;
		}
	}
}

static int url_socket_cb(CURL *e, curl_socket_t s, int what, void *cbp, void *sockp)
{
	Debug((DEBUG_DEBUG, "url_socket_cb: %d (%s)", (int)s, (what == CURL_POLL_REMOVE)?"remove":"add-or-modify"));
	if (what == CURL_POLL_REMOVE)
		url_socket_remove(s);
	else
		url_socket_addormod(s, what);

	return 0;
}
#endif

/*
 * Initializes the URL system
 */
void url_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	multihandle = curl_multi_init();
#ifdef USE_POLL
	curl_multi_setopt(multihandle, CURLMOPT_SOCKETFUNCTION, url_socket_cb);
	memset(&url_pollfds, 0, sizeof(url_pollfds));
#endif
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
 *        This file will be cleaned up after the callback returns, so save a copy to support caching.
 *  - errorbuf will contain the error message (if failed, NULL otherwise).
 *  - cached 1 if the specified cachetime is >= the current file on the server,
 *        if so, errorbuf will be NULL, filename will contain the path to the file.
 *  - data will be the value of callback_data, allowing you to figure
 *        out how to use the data contained in the downloaded file ;-).
 *        Make sure that if you access the contents of this pointer, you
 *        know that this pointer will persist. A download could take more
 *        than 10 seconds to happen and the config file can be rehashed
 *        multiple times during that time.
 */
void download_file_async(const char *url, time_t cachetime, vFP callback, void *callback_data)
{
	static char errorbuf[CURL_ERROR_SIZE];
	CURL *curl = curl_easy_init();
	if (curl)
	{
		char *file = url_getfilename(url);
		char *filename = unreal_getfilename(file);
        	char *tmp = unreal_mktemp("tmp", filename ? filename : "download.conf");
		FileHandle *handle = MyMallocEx(sizeof(FileHandle));
		handle->fd = fopen(tmp, "wb");
		if (!handle->fd)
		{
			snprintf(errorbuf, sizeof(errorbuf), "Cannot create '%s': %s", tmp, strerror(ERRNO));
			callback(url, NULL, errorbuf, 0, callback_data);
			if (file)
				MyFree(file);
			MyFree(handle);
			return;
		}
		handle->callback = callback;
		handle->callback_data = callback_data;
		handle->cachetime = cachetime;
		handle->url = strdup(url);
		strlcpy(handle->filename, tmp, sizeof(handle->filename));
		if (file)
			free(file);

		curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle->fd);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
#ifdef USE_SSL
		set_curl_ssl_options(curl);
#endif
		bzero(handle->errorbuf, CURL_ERROR_SIZE);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, handle->errorbuf);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, (char *)handle);
		curl_easy_setopt(curl, CURLOPT_FILETIME, 1);
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

#ifdef USE_POLL
/** This function scans through the poll array and moves data around so any
 * entries with -1 fd's will no longer be there.
 * There's room for improvement here ;)
 */
void fix_poll_array(void)
{
int i;
struct pollfd *pfd;
size_t sz;

	for (i = 0; i < url_pollfdcount; i++)
	{
		if (url_pollfds[i].fd == -1)
		{
			if (i < MAXTRANSFERS-1)
			{
				/* move the memory to the left, so we overwrite the -1 entry... */
				sz = &url_pollfds[MAXTRANSFERS-1] - &url_pollfds[i];
				memmove(&url_pollfds[i], &url_pollfds[i+1], sz);
				url_pollfdcount--;
			}
			/* nullify last entry */
			memset(&url_pollfds[MAXTRANSFERS-1], 0, sizeof(struct pollfd));
		}
	}
}

/*
 * Called in the select loop. Handles the transferring of any
 * queued asynchronous transfers.
 * This function is for when poll support is enabled.
 * Some extra logic has been implemented to run the loop again if we
 * expect more data (similar to the original non-poll version), but
 * now we ensure that we will not spend too much time in this function
 * since otherwise with a fast data transfer we would still stall the
 * ircd, which defeats the whole asynchronous idea... -- Syzop
 */
void url_do_transfers_async(void)
{
int nfds; /* number of file descriptors active (result) */
int i;
struct pollfd *pfd; /* just a pointer to make it easy to work with individual pollfd structs */
int dummy;
int r;
int msgs_left;
CURLMsg *msg;
long timeout;
int didanything;
int iterations = 0;
#ifndef _WIN32
struct timeval tv_start, tv_now;
#else
struct _timeb tv_start, tv_now;
#endif
long totalduration = 0;

#ifndef _WIN32
	gettimeofday(&tv_start, NULL);
#else
	_ftime(&tv_start);
#endif

	do {
		didanything = 0;
		iterations++;

		if (url_pollfdcount > 0)
			nfds = poll(url_pollfds, url_pollfdcount, 50);
		else
			nfds = 0;

		if (nfds > 0)
		{
			for (i = 0; i < url_pollfdcount; i++)
			{
				int action = 0;
				int dummy = 0; /* not interested.. */

				pfd = &url_pollfds[i];
				
				if (pfd->fd == -1)
					continue; /* race condition */
				
				if (pfd->revents & POLLIN)
					action |= CURL_CSELECT_IN;
				if (pfd->revents & POLLOUT)
					action |= CURL_CSELECT_OUT;
				
				Debug((DEBUG_DEBUG, "url_do_transfers_async() -> curl_multi_socket_action: socket %d (act: 0x%x)", (int)pfd->fd, (int)action));
				r = curl_multi_socket_action(multihandle, pfd->fd, action, &dummy);
				if (r == CURLM_OK)
					didanything = 1;
			}
		}

		fix_poll_array();

		/* Handle timeouts. This also initiates the whole transfer process. */
		r = curl_multi_socket_action(multihandle, CURL_SOCKET_TIMEOUT, 0, &dummy);

		/* now, check the status of things & deal with the results.
		 * this could also be called in each socket callbacks, but I figured
		 * we could just as well do them here all at once...
		 */
		
		while ((msg = curl_multi_info_read(multihandle, &msgs_left))) {
			if (msg->msg == CURLMSG_DONE)
			{
				FileHandle *handle;
				long code;
				long last_mod;
				CURL *easyhand = msg->easy_handle;
				curl_easy_getinfo(easyhand, CURLINFO_RESPONSE_CODE, &code);
				curl_easy_getinfo(easyhand, CURLINFO_PRIVATE, (char*)&handle);
				curl_easy_getinfo(easyhand, CURLINFO_FILETIME, &last_mod);
				fclose(handle->fd);
	#if defined(IRC_USER) && defined(IRC_GROUP)
				if (!loop.ircd_booted)
					chown(handle->filename, irc_uid, irc_gid);
	#endif
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
				free(handle->url);
				free(handle);
				curl_multi_remove_handle(multihandle, easyhand);
				/* NOTE: after curl_multi_remove_handle() you cannot use
				 * 'msg' anymore because it has freed by curl (as of v7.11.0),
				 * therefore 'easyhand' is used... fun! -- Syzop
				 */
				curl_easy_cleanup(easyhand);
			}
		}
#ifdef DEBUGMODE
		if (didanything == 1)
			Debug((DEBUG_DEBUG, "another url_do_transfers_async iteration.. yay!"));
#endif

	if (didanything)
	{
#ifndef _WIN32
		gettimeofday(&tv_now, NULL);
		totalduration += ((tv_now.tv_sec - tv_start.tv_sec) * 1000000) + tv_now.tv_usec - tv_start.tv_usec;
#else
		_ftime(&tv_now);
		totalduration += ((tv_now.tv_time - tv_start.tv_time) * 1000000) + ((tv_now.millitm - tv_start.millitm) * 1000); /* note: reduced accuracy */
#endif
		Debug((DEBUG_DEBUG, "url_do_transfers_async: totalduration is now %ld", totalduration));
	}

	} while(didanything && (totalduration < 250000)); /* repeat our loop if we did anything AND if we have spent less than 0.25s (250000usec) */
}
#else
/* NON-POLL */

/*
 * Called in the select loop. Handles the transferring of any
 * queued asynchronous transfers.
 * This is the select (NON-POLL) variant, which is no longer the default!
 */
void url_do_transfers_async(void)
{
	int cont;
	int msgs_left;
	CURLMsg *msg;
	while(CURLM_CALL_MULTI_PERFORM == curl_multi_perform(multihandle, &cont))
		;

	while(cont) {
		struct timeval timeout;
		int rc;

		fd_set fdread, fdwrite, fdexcep;
		int maxfd;
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		curl_multi_fdset(multihandle, &fdread, &fdwrite, &fdexcep, &maxfd);

		rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

		switch(rc) {
			case -1:
			case 0:
				cont = 0;
				break;
			default:
				while(CURLM_CALL_MULTI_PERFORM == 
					curl_multi_perform(multihandle, &cont))
					;
		}
	}

	while ((msg = curl_multi_info_read(multihandle, &msgs_left))) {
		if (msg->msg == CURLMSG_DONE)
		{
			FileHandle *handle;
			long code;
			long last_mod;
			CURL *easyhand = msg->easy_handle;
			curl_easy_getinfo(easyhand, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(easyhand, CURLINFO_PRIVATE, (char*)&handle);
			curl_easy_getinfo(easyhand, CURLINFO_FILETIME, &last_mod);
			fclose(handle->fd);
#if defined(IRC_USER) && defined(IRC_GROUP)
			if (!loop.ircd_booted)
				chown(handle->filename, irc_uid, irc_gid);
#endif
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
			free(handle->url);
			free(handle);
			curl_multi_remove_handle(multihandle, easyhand);
			/* NOTE: after curl_multi_remove_handle() you cannot use
			 * 'msg' anymore because it has freed by curl (as of v7.11.0),
			 * therefore 'easyhand' is used... fun! -- Syzop
			 */
			curl_easy_cleanup(easyhand);
		}
	}
}
#endif
