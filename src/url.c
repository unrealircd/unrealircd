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
 * URL does not contain a filename, NULL is returned.
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
                        return NULL;
                start = c;
                while (*c && *c != '?')
                        c++;
                if (!*c)
                        return strdup(start);
                else
                {
                        char *file = malloc(c-start+1);
                        strncpy(file, start, c-start);
                        return file;
                }
                return NULL;

        }
        return NULL;
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
char *download_file(char *url, char **error)
{
	static char errorbuf[CURL_ERROR_SIZE];
	CURL *curl = curl_easy_init();
	CURLcode res;
	char *file = url_getfilename(url);
	char *filename = unreal_getfilename(file);
	char *tmp = unreal_mktemp("tmp", filename ? filename : "download.conf");
	if (curl)
	{
		FILE *fd = fopen(tmp, "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
		bzero(errorbuf, CURL_ERROR_SIZE);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);
		res = curl_easy_perform(curl);
		fclose(fd);
	}
	if (file)
		free(file);
	curl_easy_cleanup(curl);
	if (res == CURLE_OK)
		return strdup(tmp);
	else
	{
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
	CURL *curl = curl_easy_init();
	if (curl)
	{
	        char *file = url_getfilename(url);
		char *filename = unreal_getfilename(file);
        	char *tmp = unreal_mktemp("tmp", filename ? filename : "download.conf");
		FileHandle *handle = malloc(sizeof(FileHandle));
		if (file)
			free(file);
		handle->fd = fopen(tmp, "wb");
		handle->callback = callback;
		strcpy(handle->filename, tmp);
		curl_easy_setopt(curl, CURLOPT_URL, url);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle->fd);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
		bzero(handle->errorbuf, CURL_ERROR_SIZE);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, handle->errorbuf);
		curl_easy_setopt(curl, CURLOPT_PRIVATE, (char *)handle);
		if (cachetime)
		{
			curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
			curl_easy_setopt(curl, CURLOPT_TIMEVALUE, cachetime);
		}

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
			curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &code);
			curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, (char*)&handle);
			curl_easy_getinfo(msg->easy_handle, CURLINFO_EFFECTIVE_URL, &url);
			fclose(handle->fd);
			if (msg->data.result == CURLE_OK)
			{
				if (code == 304)
				{
					handle->callback(url, NULL, NULL, 1);
					remove(handle->filename);
				}
				else
					handle->callback(url, handle->filename, NULL, 0);
			}
			else
			{
				handle->callback(url, NULL, handle->errorbuf, 0);
				remove(handle->filename);
			}
			free(handle);
			curl_easy_cleanup(msg->easy_handle);

		}

	}
}
