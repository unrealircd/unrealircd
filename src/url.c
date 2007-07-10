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

#include <string.h>
#include <stdio.h>
#include <curl/curl.h>
#include <struct.h>
#include <h.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef USE_SSL
extern char *SSLKeyPasswd;
#endif

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
	FILE *fd;
	char filename[PATH_MAX];
	char errorbuf[CURL_ERROR_SIZE];
	time_t cachetime;
} FileHandle;

/*
 * Determines if the given string is a valid URL. Since libcurl
 * supports telnet, ldap, and dict such strings are treated as
 * invalid URLs here since we don't want them supported in
 * unreal.
 */
int url_is_valid(char *string)
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
char *url_getfilename(char *url)
{
        char *c, *start;

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
char *download_file(char *url, char **error)
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

/*
 * Initializes the URL system
 */
void url_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	multihandle = curl_multi_init();
}

/*
 * Handles asynchronous downloading of a file. This function allows
 * a download to be made transparently without the caller having any
 * knowledge of how libcurl works. The specified callback function is
 * called when the download completes, or the download fails. The 
 * callback function is defined as:
 *
 * void callback(char *url, char *filename, char *errorbuf, int cached);
 * url will contain the URL that was downloaded
 * filename will contain the name of the file (if successful, NULL otherwise)
 * errorbuf will contain the error message (if failed, NULL otherwise)
 * cached 1 if the specified cachetime is >= the current file on the server,
 *        if so, both filename and errorbuf will be NULL
 */
void download_file_async(char *url, time_t cachetime, vFP callback)
{
	static char errorbuf[CURL_ERROR_SIZE];
	CURL *curl = curl_easy_init();
	if (curl)
	{
		char *file = url_getfilename(url);
		char *filename = unreal_getfilename(file);
        	char *tmp = unreal_mktemp("tmp", filename ? filename : "download.conf");
		FileHandle *handle = malloc(sizeof(FileHandle));
		handle->fd = fopen(tmp, "wb");
		if (!handle->fd)
		{
			snprintf(errorbuf, sizeof(errorbuf), "Cannot create '%s': %s", tmp, strerror(ERRNO));
			callback(url, NULL, errorbuf, 0);
			if (file)
				MyFree(file);
			MyFree(handle);
			return;
		}
		handle->callback = callback;
		handle->cachetime = cachetime;
		strcpy(handle->filename, tmp);
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

/*
 * Called in the select loop. Handles the transferring of any
 * queued asynchronous transfers.
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
			char *url;
			long code;
			long last_mod;
			CURL *easyhand = msg->easy_handle;
			curl_easy_getinfo(easyhand, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(easyhand, CURLINFO_PRIVATE, (char*)&handle);
			curl_easy_getinfo(easyhand, CURLINFO_EFFECTIVE_URL, &url);
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
					handle->callback(url, NULL, NULL, 1);
					remove(handle->filename);

				}
				else
				{
					if (last_mod != -1)
						unreal_setfilemodtime(handle->filename, last_mod);

					handle->callback(url, handle->filename, NULL, 0);
				}
			}
			else
			{
				handle->callback(url, NULL, handle->errorbuf, 0);
				remove(handle->filename);
			}
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


