/*
 *   Unreal Internet Relay Chat Daemon, src/url.c
 *   (C) 2003 Dominick Meglio and the UnrealIRCd Team
 *   (C) 2004-2021 Bram Matthys <syzop@vulnscan.org>
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

/* Stores information about the async transfer.
 * Used to maintain information about the transfer
 * to trigger the callback upon completion.
 */
typedef struct Download Download;

struct Download
{
	Download *prev, *next;
	OutgoingWebRequest *request;
	struct curl_slist *request_headers_curl;
	char errorbuf[CURL_ERROR_SIZE];
	FILE *file_fd;		/**< File open for writing (otherwise NULL) */
	char *filename;
	char *memory_data; /**< Memory for writing response (otherwise NULL) */
	int memory_data_len; /**< Size of memory_data */
	int memory_data_allocated; /**< Total allocated memory for 'memory_data' */
};

CURLM *multihandle = NULL;

Download *downloads = NULL;

void url_free_handle(Download *handle)
{
	DelListItem(handle, downloads);
	if (handle->file_fd)
		fclose(handle->file_fd);
	safe_free(handle->memory_data);
	safe_free(handle->filename);
	if (handle->request_headers_curl)
		curl_slist_free_all(handle->request_headers_curl);
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
 * a stream. - File backed
 */
static size_t do_download_file(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return fwrite(ptr, size, nmemb, (FILE *)stream);
}

/*
 * Used by CURLOPT_WRITEFUNCTION to actually write the data to
 * a stream. - Memory backed
 */
static size_t do_download_memory(void *ptr, size_t size, size_t nmemb, void *stream)
{
	// DUPLICATE CODE: same as src/url_unreal.c, well.. sortof
	Download *handle = (Download *)stream;
	int write_sz = size * nmemb;
	int size_required = handle->memory_data_len + write_sz;

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
			if (handle->file_fd)
			{
				fclose(handle->file_fd);
				handle->file_fd = NULL;
			}

			if (handle->request->callback == NULL)
			{
				/* Request is already canceled, we don't care about the result, just clean up */
				if (handle->filename)
					remove(handle->filename);
			} else
			if (msg->data.result == CURLE_OK)
			{
				if (code == 304 || (last_mod != -1 && last_mod <= handle->request->cachetime))
				{
					handle->request->callback(handle->request->url, NULL, handle->memory_data, handle->memory_data_len, NULL, 1, handle->request->callback_data);
					if (handle->filename)
						remove(handle->filename);
				}
				else
				{
					if ((last_mod != -1) && handle->filename)
						unreal_setfilemodtime(handle->filename, last_mod);

					handle->request->callback(handle->request->url, handle->filename, handle->memory_data, handle->memory_data_len, NULL, 0, handle->request->callback_data);
					if (handle->filename)
						remove(handle->filename);
				}
			}
			else
			{
				handle->request->callback(handle->request->url, NULL, NULL, 0, handle->errorbuf, 0, handle->request->callback_data);
				if (handle->filename)
					remove(handle->filename);
			}

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
EVENT(url_socket_timeout)
{
	int dummy;

	curl_multi_socket_action(multihandle, CURL_SOCKET_TIMEOUT, 0, &dummy);
	url_check_multi_handles();
}

static Event *url_socket_timeout_hdl = NULL;

/*
 * Initializes the URL system
 */
void url_init(void)
{
	curl_global_init(CURL_GLOBAL_ALL);
	multihandle = curl_multi_init();

	curl_multi_setopt(multihandle, CURLMOPT_SOCKETFUNCTION, url_socket_cb);
	url_socket_timeout_hdl = EventAdd(NULL, "url_socket_timeout", url_socket_timeout, NULL, 500, 0);
}

void url_start_async(OutgoingWebRequest *request)
{
	static char errorbuf[CURL_ERROR_SIZE];
	char user_agent[256];
	CURL *curl;
	char *file;
	const char *filename;
	char *tmp;
	Download *handle;

	/* Check for the bare minimum */
	if (!request->url || !request->http_method)
		abort();

	curl = curl_easy_init();
	if (!curl)
	{
		unreal_log(ULOG_ERROR, "main", "CURL_INTERNAL_FAILURE", NULL,
		           "Could not initialize curl handle. Maybe out of memory/resources?");
		snprintf(errorbuf, sizeof(errorbuf), "Could not initialize curl handle");
		return;
	}

	handle = safe_alloc(sizeof(Download));
	handle->request = request;

	if (handle->request->store_in_file)
	{
		file = url_getfilename(handle->request->url);
		filename = unreal_getfilename(file);
		tmp = unreal_mktemp(TMPDIR, filename ? filename : "download.conf");
		handle->file_fd = fopen(tmp, "wb");
		if (!handle->file_fd)
		{
			snprintf(errorbuf, sizeof(errorbuf), "Cannot create '%s': %s", tmp, strerror(ERRNO));
			handle->request->callback(handle->request->url, NULL, NULL, 0, errorbuf, 0, handle->request->callback_data);
			safe_free(file);
			safe_free(handle);
			return;
		}
		safe_strdup(handle->filename, tmp);
		safe_free(file);
	}

	AddListItem(handle, downloads);

	curl_easy_setopt(curl, CURLOPT_URL, handle->request->url);
	snprintf(user_agent, sizeof(user_agent), "UnrealIRCd %s", VERSIONONLY);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
	if (handle->request->store_in_file)
	{
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download_file);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle->file_fd);
	} else {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, do_download_memory);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)handle);
		handle->memory_data_allocated = URL_MEMORY_BACKED_CHUNK_SIZE;
		handle->memory_data = safe_alloc(URL_MEMORY_BACKED_CHUNK_SIZE+1);
	}
	if (handle->request->http_method == HTTP_METHOD_POST)
	{
		curl_easy_setopt(curl, CURLOPT_POST, 1);
#if LIBCURL_VERSION_NUM >= 0x071301
		curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
#endif
		if (handle->request->body && strlen(handle->request->body))
			curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, handle->request->body);
	}

	if (handle->request->headers)
	{
		NameValuePrioList *n;
		char buf[512];

		for (n = handle->request->headers; n; n = n->next)
		{
			if (n->value)
				snprintf(buf, sizeof(buf), "%s: %s", n->name, n->value);
			else
				snprintf(buf, sizeof(buf), "%s:", n->name);
			handle->request_headers_curl = curl_slist_append(handle->request_headers_curl, buf);
		}
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, handle->request_headers_curl);
	}

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
	if (handle->request->cachetime)
	{
		curl_easy_setopt(curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
		curl_easy_setopt(curl, CURLOPT_TIMEVALUE, handle->request->cachetime);
	}
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, DOWNLOAD_TRANSFER_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, DOWNLOAD_CONNECT_TIMEOUT);
#if LIBCURL_VERSION_NUM >= 0x070f01
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, handle->request->max_redirects);
#endif

	curl_multi_add_handle(multihandle, curl);
}

char *urlencode(const char *s, char *wbuf, int wlen)
{
	char *ret = curl_easy_escape(NULL, s, strlen(s));
	if (ret == NULL)
	{
		if (wlen > 0)
			*wbuf = '\0';
		return NULL;
	}
	strlcpy(wbuf, ret, wlen);
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
