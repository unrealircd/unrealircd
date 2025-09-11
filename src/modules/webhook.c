/*
 * webhook - HTTP webhook support for log events
 * (C) Copyright 2025 Valware and the UnrealIRCd team
 * License: GPLv3 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"webhook",
	"1.0.0",
	"HTTP webhook support for log events",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Structures */
typedef struct WebhookConfig WebhookConfig;
struct WebhookConfig {
	WebhookConfig *prev, *next;
	char *name;
	char *url;
	LogSource *sources;
};

/* Forward declarations */
int webhook_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int webhook_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
int webhook_log_hook(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, json_t *json, const char *json_serialized, const char *timebuf);
void webhook_send_async(const char *url, const char *json_data);
void webhook_add_log_source(LogSource **sources, const char *str);
void webhook_free_config(void);

/* Global variables */
static WebhookConfig *webhook_list = NULL;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, webhook_config_test);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, webhook_config_run);
	HookAdd(modinfo->handle, HOOKTYPE_LOG, 0, webhook_log_hook);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	webhook_free_config();
	return MOD_SUCCESS;
}

void webhook_free_config(void)
{
	WebhookConfig *w, *w_next;
	
	for (w = webhook_list; w; w = w_next)
	{
		w_next = w->next;
		safe_free(w->name);
		safe_free(w->url);
		free_log_sources(w->sources);
		safe_free(w);
	}
	webhook_list = NULL;
}

/* Add a single log source to the list */
void webhook_add_log_source(LogSource **sources, const char *str)
{
	LogSource *s;
	
	if (!str)
		return;
		
	s = add_log_source(str);
	if (s)
		AddListItem(s, *sources);
}

int webhook_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_url = 0;
	int has_events = 0;
	
	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->name || strcmp(ce->name, "webhook"))
		return 0;
	
	if (!ce->value)
	{
		config_error("%s:%i: webhook block needs a name",
			ce->file->filename, ce->line_number);
		errors++;
		goto end;
	}
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "url"))
		{
			if (has_url)
			{
				config_error("%s:%i: duplicate webhook::url",
					cep->file->filename, cep->line_number);
				errors++;
				continue;
			}
			has_url = 1;
			if (!cep->value)
			{
				config_error("%s:%i: webhook::url with no value",
					cep->file->filename, cep->line_number);
				errors++;
			} else if (strncmp(cep->value, "http://", 7) != 0 && strncmp(cep->value, "https://", 8) != 0)
			{
				config_error("%s:%i: webhook::url must be a HTTP/HTTPS URL (%s)",
					cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "events"))
		{
			has_events = 1;
			if (!cep->items)
			{
				config_error("%s:%i: webhook::events block is empty",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else
		{
			config_error("%s:%i: unknown directive webhook::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}
	
	if (!has_url)
	{
		config_error("%s:%i: webhook block '%s' has no url",
			ce->file->filename, ce->line_number, ce->value);
		errors++;
	}
	
	if (!has_events)
	{
		config_error("%s:%i: webhook block '%s' has no events",
			ce->file->filename, ce->line_number, ce->value);
		errors++;
	}

end:
	*errs = errors;
	return errors ? -1 : 1;
}

int webhook_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	WebhookConfig *w;
	
	if (type != CONFIG_MAIN)
		return 0;
	
	if (!ce || !ce->name || strcmp(ce->name, "webhook"))
		return 0;
	
	w = safe_alloc(sizeof(WebhookConfig));
	safe_strdup(w->name, ce->value);
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "url"))
		{
			safe_strdup(w->url, cep->value);
		}
		else if (!strcmp(cep->name, "events"))
		{
			ConfigEntry *cepp;
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				webhook_add_log_source(&w->sources, cepp->name);
			}
		}
	}
	
	AddListItem(w, webhook_list);
	return 1;
}


int webhook_log_hook(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, json_t *json, const char *json_serialized, const char *timebuf)
{
	WebhookConfig *w;
	
	/* Skip debug and rawtraffic like the RPC module does */
	if (!strcmp(subsystem, "rawtraffic") || (loglevel == ULOG_DEBUG))
		return 0;
	
	for (w = webhook_list; w; w = w->next)
	{
		if (w->sources && log_sources_match(w->sources, loglevel, subsystem, event_id, 0))
		{
			if (json_serialized && *json_serialized)
			{
				webhook_send_async(w->url, json_serialized);
			}
		}
	}
	
	return 0;
}

void webhook_send_async(const char *url, const char *json_data)
{
	OutgoingWebRequest *request;
	NameValuePrioList *headers = NULL;
	
	request = safe_alloc(sizeof(OutgoingWebRequest));
	safe_strdup(request->url, url);
	request->http_method = HTTP_METHOD_POST;

    add_nvplist(&headers, 0, "Content-Type", "application/json");
	add_nvplist(&headers, 0, "User-Agent", "UnrealIRCd-Webhook/1.0");
	request->headers = headers;
	
	if (json_data && *json_data)
	{
		safe_strdup(request->body, json_data);
	}
	
	/* Use the built-in callback that doesn't care about the response */
	request->callback = download_complete_dontcare;
	request->max_redirects = 3;
	
	url_start_async(request);
}
