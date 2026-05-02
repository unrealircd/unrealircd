/*
 * src/modules/filehost.c
 * Link previews via message tags + IRCv3 draft/FILEHOST support.
 *
 * Detects URLs in channel PRIVMSGs, fetches the page asynchronously,
 * and emits a TAGMSG carrying the title, snippet, and (optionally) a
 * meta-image that has been re-uploaded to a configured filehost.
 *
 * Config (obbyircd.conf):
 *   filehosts {
 *       host "https://your-filehost.example/";
 *   };
 *
 * Originally written by the ObsidianIRC team as the third-party module
 * "third/o-filehost"; integrated as a native ObbyIRCd module.
 *
 * License: GPLv3
 */

#include "unrealircd.h"

#define CONF_FILEHOST "filehosts"

/* FILEHOST config struct */
struct
{
	char *isupport_line;
	MultiLine *hosts;
	unsigned short int has_hosts;
} cfg;

ModuleHeader MOD_HEADER = {
	"filehost",
	"1.0",
	"Link previews via message tags and IRCv3 draft/FILEHOST support",
	"ObbyIRCd Team",
	"unrealircd-6",
};

/* Maximum sizes for safety */
#define MAX_DOWNLOAD_SIZE 1048576  /* 1MB */
#define MAX_TITLE_LENGTH 500
#define MAX_SNIPPET_LENGTH 500
#define MAX_META_LENGTH 2048
#define MAX_URL_LENGTH 2048

/* Structure to hold context for async callback */
typedef struct {
	char *channel;
	char *msgid;
	char *url;
} LinkPreviewContext;

/* Structure to hold context for image upload callback */
typedef struct {
	char *channel;
	char *msgid;
	char *title;
	char *snippet;
} ImageUploadContext;

/* Function prototypes */
void link_preview_download_complete(OutgoingWebRequest *request, OutgoingWebResponse *response);
void image_upload_complete(OutgoingWebRequest *request, OutgoingWebResponse *response);
int link_preview_chanmsg(Client *client, Channel *channel, int sendflags, const char *member_modes, const char *target, MessageTag *mtags, const char *text, SendType sendtype);
char *extract_url_from_message(const char *text);
char *extract_title_from_html(const char *html);
char *extract_snippet_from_html(const char *html);
char *extract_meta_image_from_html(const char *html);
void send_link_preview(const char *channel, const char *msgid, const char *title, const char *snippet, const char *meta_image);
int link_preview_mtag_is_ok(Client *client, const char *name, const char *value);
int filehost_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int filehost_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void setconf(void);
void freeconf(void);

/* Module test */
MOD_TEST()
{
	setconf();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, filehost_configtest);
	return MOD_SUCCESS;
}

/* Module init */
MOD_INIT()
{
	MessageTagHandlerInfo mtag;

	RegisterApiCallbackWebResponse(modinfo->handle, "link_preview_download_complete", link_preview_download_complete);
	RegisterApiCallbackWebResponse(modinfo->handle, "image_upload_complete", image_upload_complete);
	HookAdd(modinfo->handle, HOOKTYPE_CHANMSG, 0, link_preview_chanmsg);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, filehost_configrun);

	/* Register our custom message tags */
	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "+reply";
	mtag.is_ok = link_preview_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "obsidianirc/link-preview-title";
	mtag.is_ok = link_preview_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "obsidianirc/link-preview-snippet";
	mtag.is_ok = link_preview_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	memset(&mtag, 0, sizeof(mtag));
	mtag.name = "obsidianirc/link-preview-meta";
	mtag.is_ok = link_preview_mtag_is_ok;
	mtag.flags = MTAG_HANDLER_FLAGS_NO_CAP_NEEDED;
	MessageTagHandlerAdd(modinfo->handle, &mtag);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (cfg.has_hosts)
	{
		ISupport *is;
		if (!(is = ISupportAdd(modinfo->handle, "FILEHOST", cfg.isupport_line)))
			return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	freeconf();
	return MOD_SUCCESS;
}

/**
 * Hook function called when a message is sent to a channel
 */
int link_preview_chanmsg(Client *client, Channel *channel, int sendflags, const char *member_modes, const char *target, MessageTag *mtags, const char *text, SendType sendtype)
{
	char *url;
	const char *msgid = NULL;
	MessageTag *mtag;
	LinkPreviewContext *context;
	OutgoingWebRequest *request;

	/* Only process PRIVMSG, not NOTICE or TAGMSG */
	if (sendtype != SEND_TYPE_PRIVMSG)
	{
		return 0;
	}

	/* Don't process messages from servers or ulines */
	if (IsServer(client) || IsULine(client))
	{
		return 0;
	}

	/* Extract URL from message */
	url = extract_url_from_message(text);
	if (!url)
	{
		return 0; /* No URL found */
	}

	/* Find the msgid tag from the message */
	mtag = find_mtag(mtags, "msgid");
	if (mtag)
		msgid = mtag->value;

	/* If no msgid, we can't send a reply, so skip */
	if (!msgid)
	{
		safe_free(url);
		return 0;
	}

	/* Create context for the async callback */
	context = safe_alloc(sizeof(LinkPreviewContext));
	safe_strdup(context->channel, channel->name);
	safe_strdup(context->msgid, msgid);
	context->url = url; /* Transfer ownership */

	/* Start async web request */
	request = safe_alloc(sizeof(OutgoingWebRequest));
	safe_strdup(request->url, url);
	request->http_method = HTTP_METHOD_GET;
	safe_strdup(request->apicallback, "link_preview_download_complete");
	request->max_redirects = 3;
	request->callback_data = context;
	add_nvplist(&request->headers, 0, "User-Agent", "ObbyIRCd-LinkPreview/1.0");

	url_start_async(request);

	return 0; /* Don't modify the message */
}

/**
 * Callback when web request completes
 */
void link_preview_download_complete(OutgoingWebRequest *request, OutgoingWebResponse *response)
{
	LinkPreviewContext *context = (LinkPreviewContext *)request->callback_data;
	char *title = NULL;
	char *snippet = NULL;
	char *meta_image = NULL;

	if (!context)
	{
		return;
	}

	/* Check for errors */
	if (response->errorbuf || !response->memory)
	{
		unreal_log(ULOG_DEBUG, "filehost", "DOWNLOAD_ERROR", NULL,
				   "Error downloading $url: $error",
				   log_data_string("url", context->url),
				   log_data_string("error", response->errorbuf ? response->errorbuf : "No data"));
		goto cleanup;
	}

	/* Safety check: limit download size */
	if (response->memory_len > MAX_DOWNLOAD_SIZE)
	{
		unreal_log(ULOG_DEBUG, "filehost", "DOWNLOAD_TOO_LARGE", NULL,
				   "Download from $url exceeded size limit ($size bytes)",
				   log_data_string("url", context->url),
				   log_data_integer("size", response->memory_len));
		goto cleanup;
	}

	/* Extract title and snippet from HTML */
	title = extract_title_from_html(response->memory);
	snippet = extract_snippet_from_html(response->memory);
	meta_image = extract_meta_image_from_html(response->memory);

	/* Only send if we got at least a title */
	if (title && *title)
	{
		/* If we have a meta image, upload it to configured filehost first */
		if (meta_image && *meta_image && cfg.has_hosts && cfg.hosts)
		{
			ImageUploadContext *upload_ctx;
			OutgoingWebRequest *upload_req;
			char *json_payload;
			char upload_url[512];

			snprintf(upload_url, sizeof(upload_url), "%s/upload", cfg.hosts->line);

			/* Create context for image upload callback */
			upload_ctx = safe_alloc(sizeof(ImageUploadContext));
			safe_strdup(upload_ctx->channel, context->channel);
			safe_strdup(upload_ctx->msgid, context->msgid);
			safe_strdup(upload_ctx->title, title);
			safe_strdup(upload_ctx->snippet, snippet ? snippet : "");

			/* Build JSON payload: {"url": "image_url"} */
			json_payload = safe_alloc(strlen(meta_image) + 50);
			snprintf(json_payload, strlen(meta_image) + 50, "{\"url\":\"%s\"}", meta_image);

			/* Start async upload request */
			upload_req = safe_alloc(sizeof(OutgoingWebRequest));
			safe_strdup(upload_req->url, upload_url);
			upload_req->http_method = HTTP_METHOD_POST;
			safe_strdup(upload_req->apicallback, "image_upload_complete");
			upload_req->callback_data = upload_ctx;
			safe_strdup(upload_req->body, json_payload);
			add_nvplist(&upload_req->headers, 0, "Content-Type", "application/json");
			add_nvplist(&upload_req->headers, 0, "User-Agent", "ObbyIRCd-LinkPreview/1.0");

			url_start_async(upload_req);
			safe_free(json_payload);
		}
		else
		{
			/* No image or no filehost configured, send preview directly */
			send_link_preview(context->channel, context->msgid, title, snippet, meta_image && *meta_image ? meta_image : NULL);
		}
	}

cleanup:
	safe_free(title);
	safe_free(snippet);
	safe_free(meta_image);
	safe_free(context->channel);
	safe_free(context->msgid);
	safe_free(context->url);
	safe_free(context);
}

/**
 * Callback when image upload to configured filehost completes
 */
void image_upload_complete(OutgoingWebRequest *request, OutgoingWebResponse *response)
{
	ImageUploadContext *context = (ImageUploadContext *)request->callback_data;
	json_t *result;
	json_error_t jerr;
	const char *saved_url = NULL;

	if (!context)
	{
		return;
	}

	/* Check for errors */
	if (response->errorbuf || !response->memory)
	{
		/* Send preview without image */
		send_link_preview(context->channel, context->msgid, context->title, context->snippet, NULL);
		goto cleanup;
	}

	/* Parse JSON response to extract saved_url */
	result = json_loads(response->memory, JSON_REJECT_DUPLICATES, &jerr);
	if (!result)
	{
		/* Send preview without image */
		send_link_preview(context->channel, context->msgid, context->title, context->snippet, NULL);
		goto cleanup;
	}

	/* Extract saved_url from JSON */
	json_t *saved_url_obj = json_object_get(result, "saved_url");
	if (saved_url_obj && json_is_string(saved_url_obj))
	{
		saved_url = json_string_value(saved_url_obj);

		/* Send preview with local image URL */
		send_link_preview(context->channel, context->msgid, context->title, context->snippet, saved_url);
	}
	else
	{
		/* Send preview without image */
		send_link_preview(context->channel, context->msgid, context->title, context->snippet, NULL);
	}

	json_decref(result);

cleanup:
	safe_free(context->channel);
	safe_free(context->msgid);
	safe_free(context->title);
	safe_free(context->snippet);
	safe_free(context);
}

/**
 * Extract first URL from message text
 */
char *extract_url_from_message(const char *text)
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	PCRE2_SPTR pattern = (PCRE2_SPTR)"https?://[^\\s<>\"]+";
	PCRE2_SPTR subject = (PCRE2_SPTR)text;
	int errornumber;
	PCRE2_SIZE erroroffset;
	int rc;
	char *result = NULL;

	/* Compile regex */
	re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, 0, &errornumber, &erroroffset, NULL);
	if (!re)
		return NULL;

	/* Match */
	match_data = pcre2_match_data_create_from_pattern(re, NULL);
	rc = pcre2_match(re, subject, strlen(text), 0, 0, match_data, NULL);

	if (rc > 0)
	{
		PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
		size_t url_len = ovector[1] - ovector[0];

		/* Limit URL length for safety */
		if (url_len > MAX_URL_LENGTH)
			url_len = MAX_URL_LENGTH;

		result = safe_alloc(url_len + 1);
		memcpy(result, text + ovector[0], url_len);
		result[url_len] = '\0';
	}

	pcre2_match_data_free(match_data);
	pcre2_code_free(re);

	return result;
}

/**
 * Extract title from HTML using regex
 */
char *extract_title_from_html(const char *html)
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	PCRE2_SPTR pattern = (PCRE2_SPTR)"<title[^>]*>([^<]+)</title>";
	PCRE2_SPTR subject = (PCRE2_SPTR)html;
	int errornumber;
	PCRE2_SIZE erroroffset;
	int rc;
	char *result = NULL;

	/* Compile regex (case insensitive) */
	re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);
	if (!re)
		return NULL;

	/* Match */
	match_data = pcre2_match_data_create_from_pattern(re, NULL);
	rc = pcre2_match(re, subject, strlen(html), 0, 0, match_data, NULL);

	if (rc > 1)
	{
		PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
		size_t title_len = ovector[3] - ovector[2];

		/* Limit title length */
		if (title_len > MAX_TITLE_LENGTH)
			title_len = MAX_TITLE_LENGTH;

		result = safe_alloc(title_len + 1);
		memcpy(result, html + ovector[2], title_len);
		result[title_len] = '\0';

		/* Trim whitespace */
		char *p = result;
		while (*p && isspace(*p)) p++;
		if (p != result)
			memmove(result, p, strlen(p) + 1);

		p = result + strlen(result) - 1;
		while (p > result && isspace(*p))
			*p-- = '\0';
	}

	pcre2_match_data_free(match_data);
	pcre2_code_free(re);

	return result;
}

/**
 * Extract description/snippet from HTML meta tags or first paragraph
 */
char *extract_snippet_from_html(const char *html)
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	PCRE2_SPTR pattern;
	PCRE2_SPTR subject = (PCRE2_SPTR)html;
	int errornumber;
	PCRE2_SIZE erroroffset;
	int rc;
	char *result = NULL;

	/* Try meta description first */
	pattern = (PCRE2_SPTR)"<meta[^>]+name=[\"']description[\"'][^>]+content=[\"']([^\"']+)[\"']";
	re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);
	if (!re)
	{
		/* Try alternative format */
		pattern = (PCRE2_SPTR)"<meta[^>]+content=[\"']([^\"']+)[\"'][^>]+name=[\"']description[\"']";
		re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);
	}

	if (re)
	{
		match_data = pcre2_match_data_create_from_pattern(re, NULL);
		rc = pcre2_match(re, subject, strlen(html), 0, 0, match_data, NULL);

		if (rc > 1)
		{
			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
			size_t snippet_len = ovector[3] - ovector[2];

			/* Limit snippet length */
			if (snippet_len > MAX_SNIPPET_LENGTH)
				snippet_len = MAX_SNIPPET_LENGTH;

			result = safe_alloc(snippet_len + 1);
			memcpy(result, html + ovector[2], snippet_len);
			result[snippet_len] = '\0';

			/* Trim whitespace */
			char *p = result;
			while (*p && isspace(*p)) p++;
			if (p != result)
				memmove(result, p, strlen(p) + 1);

			p = result + strlen(result) - 1;
			while (p > result && isspace(*p))
				*p-- = '\0';
		}

		pcre2_match_data_free(match_data);
		pcre2_code_free(re);
	}

	/* If no meta description, try Open Graph description */
	if (!result)
	{
		pattern = (PCRE2_SPTR)"<meta[^>]+property=[\"']og:description[\"'][^>]+content=[\"']([^\"']+)[\"']";
		re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);

		if (re)
		{
			match_data = pcre2_match_data_create_from_pattern(re, NULL);
			rc = pcre2_match(re, subject, strlen(html), 0, 0, match_data, NULL);

			if (rc > 1)
			{
				PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
				size_t snippet_len = ovector[3] - ovector[2];

				if (snippet_len > MAX_SNIPPET_LENGTH)
					snippet_len = MAX_SNIPPET_LENGTH;

				result = safe_alloc(snippet_len + 1);
				memcpy(result, html + ovector[2], snippet_len);
				result[snippet_len] = '\0';

				/* Trim whitespace */
				char *p = result;
				while (*p && isspace(*p)) p++;
				if (p != result)
					memmove(result, p, strlen(p) + 1);

				p = result + strlen(result) - 1;
				while (p > result && isspace(*p))
					*p-- = '\0';
			}

			pcre2_match_data_free(match_data);
			pcre2_code_free(re);
		}
	}

	return result;
}

/**
 * Extract meta image from HTML (Open Graph or Twitter Card)
 */
char *extract_meta_image_from_html(const char *html)
{
	pcre2_code *re;
	pcre2_match_data *match_data;
	PCRE2_SPTR pattern;
	PCRE2_SPTR subject = (PCRE2_SPTR)html;
	int errornumber;
	PCRE2_SIZE erroroffset;
	int rc;
	char *result = NULL;

	/* Try Open Graph image first (og:image) */
	pattern = (PCRE2_SPTR)"<meta[^>]+property=[\"']og:image[\"'][^>]+content=[\"']([^\"']+)[\"']";
	re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);
	if (!re)
	{
		/* Try alternative format */
		pattern = (PCRE2_SPTR)"<meta[^>]+content=[\"']([^\"']+)[\"'][^>]+property=[\"']og:image[\"']";
		re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);
	}

	if (re)
	{
		match_data = pcre2_match_data_create_from_pattern(re, NULL);
		rc = pcre2_match(re, subject, strlen(html), 0, 0, match_data, NULL);

		if (rc > 1)
		{
			PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
			size_t image_len = ovector[3] - ovector[2];

			/* Limit image URL length */
			if (image_len > MAX_META_LENGTH)
				image_len = MAX_META_LENGTH;

			result = safe_alloc(image_len + 1);
			memcpy(result, html + ovector[2], image_len);
			result[image_len] = '\0';

			/* Trim whitespace */
			char *p = result;
			while (*p && isspace(*p)) p++;
			if (p != result)
				memmove(result, p, strlen(p) + 1);

			p = result + strlen(result) - 1;
			while (p > result && isspace(*p))
				*p-- = '\0';
		}

		pcre2_match_data_free(match_data);
		pcre2_code_free(re);
	}

	/* If no og:image, try Twitter Card image (twitter:image or twitter:image:src) */
	if (!result)
	{
		pattern = (PCRE2_SPTR)"<meta[^>]+name=[\"']twitter:image(:src)?[\"'][^>]+content=[\"']([^\"']+)[\"']";
		re = pcre2_compile(pattern, PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &errornumber, &erroroffset, NULL);

		if (re)
		{
			match_data = pcre2_match_data_create_from_pattern(re, NULL);
			rc = pcre2_match(re, subject, strlen(html), 0, 0, match_data, NULL);

			if (rc > 1)
			{
				PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
				/* The captured group for the URL will be at index 2 (group 1 is :src optional) */
				size_t image_len;
				int url_group = (rc > 2) ? 5 : 3; /* If optional :src matched, URL is in group 2, otherwise group 1 */

				image_len = ovector[url_group] - ovector[url_group - 1];

				if (image_len > MAX_META_LENGTH)
					image_len = MAX_META_LENGTH;

				result = safe_alloc(image_len + 1);
				memcpy(result, html + ovector[url_group - 1], image_len);
				result[image_len] = '\0';

				/* Trim whitespace */
				char *p = result;
				while (*p && isspace(*p)) p++;
				if (p != result)
					memmove(result, p, strlen(p) + 1);

				p = result + strlen(result) - 1;
				while (p > result && isspace(*p))
					*p-- = '\0';
			}

			pcre2_match_data_free(match_data);
			pcre2_code_free(re);
		}
	}

	return result;
}

/**
 * Validate our custom message tags
 */
int link_preview_mtag_is_ok(Client *client, const char *name, const char *value)
{
	/* Only allow from servers (our module) */
	if (IsServer(client) || IsMe(client))
		return 1;

	return 0;
}

/**
 * Send TAGMSG with link preview to channel
 */
void send_link_preview(const char *channel, const char *msgid, const char *title, const char *snippet, const char *meta_image)
{
	char truncated_title[MAX_TITLE_LENGTH + 1];
	char truncated_snippet[MAX_SNIPPET_LENGTH + 1];
	char truncated_meta[MAX_META_LENGTH + 1];
	MessageTag *mtags = NULL;
	MessageTag *m;
	Channel *chan;
	Member *member;

	chan = find_channel(channel);
	if (!chan)
	{
		return;
	}

	/* Truncate title, snippet, and meta to max lengths (UnrealIRCd will handle escaping) */
	strlcpy(truncated_title, title, sizeof(truncated_title));

	if (snippet && *snippet)
		strlcpy(truncated_snippet, snippet, sizeof(truncated_snippet));
	else
		truncated_snippet[0] = '\0';

	if (meta_image && *meta_image)
		strlcpy(truncated_meta, meta_image, sizeof(truncated_meta));
	else
		truncated_meta[0] = '\0';

	/* Add +reply tag */
	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "+reply");
	safe_strdup(m->value, msgid);
	AddListItem(m, mtags);

	/* Add title tag */
	m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, "obsidianirc/link-preview-title");
	safe_strdup(m->value, truncated_title);
	AddListItem(m, mtags);

	/* Add snippet tag if available */
	if (truncated_snippet[0])
	{
		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "obsidianirc/link-preview-snippet");
		safe_strdup(m->value, truncated_snippet);
		AddListItem(m, mtags);
	}

	/* Add meta tag if available */
	if (truncated_meta[0])
	{
		m = safe_alloc(sizeof(MessageTag));
		safe_strdup(m->name, "obsidianirc/link-preview-meta");
		safe_strdup(m->value, truncated_meta);
		AddListItem(m, mtags);
	}

	/* Use new_message_special to add standard tags (msgid, time) and prepare for sending */
	new_message_special(&me, mtags, &mtags, ":%s TAGMSG %s", me.name, channel);

	/* Send TAGMSG to all channel members */
	for (member = chan->members; member; member = member->next)
	{
		Client *acptr = member->client;
		if (MyConnect(acptr))
		{
			sendto_one(acptr, mtags, ":%s TAGMSG %s", me.name, channel);
		}
	}

	free_message_tags(mtags);
}

void setconf(void)
{
	memset(&cfg, 0, sizeof(cfg));
	cfg.has_hosts = 0;
	safe_strdup(cfg.isupport_line, "");
}

void freeconf(void)
{
	freemultiline(cfg.hosts);
	cfg.has_hosts = 0;
	safe_free(cfg.isupport_line);
	memset(&cfg, 0, sizeof(cfg));
}


int filehost_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, CONF_FILEHOST))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
		{
			config_error("%s:%i: blank %s item", cep->file->filename, cep->line_number, CONF_FILEHOST);
			++errors;
			continue;
		}
		if (!strcasecmp(cep->name, "host"))
		{
			if (!BadPtr(cep->value))
				cfg.has_hosts = 1;

			else
				config_error("%s:%i: Empty host at %s::%s", cep->file->filename, cep->line_number, CONF_FILEHOST, cep->name);

			continue;
		}

		// Anything else is unknown to us =]
		config_warn("%s:%i: unknown item %s::%s", cep->file->filename, cep->line_number, CONF_FILEHOST, cep->name); // So display just a warning
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int filehost_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	char buf[BUFSIZE] = "\0";

	if (type != CONFIG_MAIN)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "filehosts"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->name)
			continue;

		if (!strcmp(cep->name, "host"))
			addmultiline(&cfg.hosts, cep->value);

	}

	for (MultiLine *m = cfg.hosts; m; m = m->next)
	{
		strlcat(buf, m->line, sizeof(buf));
		if (m->next)
			strlcat(buf, "\\x20", sizeof(buf));
	}
	if (strlen(buf))
		safe_strdup(cfg.isupport_line, buf);


	return 1; // We good
}
