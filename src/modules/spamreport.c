/* src/modules/spamreport.c - spamreport { } and /SPAMREPORT cmd
 * (C) Copyright 2023 Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */
#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"spamreport",
	"1.0.0",
	"Send spam reports via SPAMREPORT and spamreport { } blocks",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/** For rate limiting the rate limit message :D */
#define SPAMREPORT_RATE_LIMIT_WARNING_EVERY 15

/* Enums and structs */
typedef enum SpamreportType {
	SPAMREPORT_TYPE_SIMPLE = 1,
	SPAMREPORT_TYPE_DRONEBL = 2,
	SPAMREPORT_TYPE_CENTRAL_SPAMREPORT = 3,
} SpamreportType;

typedef struct Spamreport Spamreport;
struct Spamreport {
	Spamreport *prev, *next;
	char *name; /**< Name of the block, spamreport <this> { } */
	char *url; /**< URL to use */
	SpamreportType type;
	HttpMethod http_method;
	NameValuePrioList *parameters;
	SecurityGroup *except;
	int rate_limit_count;
	int rate_limit_period;
};

typedef struct SpamreportCounter SpamreportCounter;
struct SpamreportCounter {
	SpamreportCounter *prev, *next;
	char *name;
	long long rate_limit_time;
	int rate_limit_count;
	time_t last_warning_sent;
};

/* Forward declarations */
CMD_FUNC(cmd_spamreport);
int tkl_config_test_spamreport(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_spamreport(ConfigFile *, ConfigEntry *, int);
Spamreport *find_spamreport_block(const char *name);
void free_spamreport_blocks(void);
int _spamreport(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block, Client *by);
int _central_spamreport_enabled(void);
void spamreportcounters_free_all(ModData *m);
SpamreportType parse_spamreport_type(const char *s);

/* Variables */
Spamreport *spamreports = NULL;
SpamreportCounter *spamreportcounters = NULL;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_SPAMREPORT, _spamreport);
	EfunctionAdd(modinfo->handle, EFUNC_CENTRAL_SPAMREPORT_ENABLED, _central_spamreport_enabled);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_spamreport);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "SPAMREPORT", cmd_spamreport, MAXPARA, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_spamreport);
	LoadPersistentPointer(modinfo, spamreportcounters, spamreportcounters_free_all);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	SavePersistentPointer(modinfo, spamreportcounters);
	free_spamreport_blocks();
	return MOD_SUCCESS;
}

/** Test a spamreport { } block in the configuration file */
int tkl_config_test_spamreport(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep, *cepp;
	int errors = 0;
	char has_url=0, has_type=0, has_type_dronebl=0, has_http_method=0;
	char has_dronebl_type=0, has_dronebl_rpckey=0;

	/* We are only interested in spamreport { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "spamreport"))
		return 0;

	if (!ce->value)
	{
		config_error("%s:%i: spamreport block has no name, should be like: spamfilter <name> { }",
			ce->file->filename, ce->line_number);
		errors++;
	}

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "except"))
		{
			test_match_block(cf, cep, &errors);
		} else
		if (!strcmp(cep->name, "parameters"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!cepp->value)
				{
					config_error_empty(cepp->file->filename, cepp->line_number,
						"spamreport::parameters", cepp->name);
					errors++;
				}
				else if (!strcmp(cepp->name, "rpckey"))
					has_dronebl_rpckey = 1;
				else if (!strcmp(cepp->name, "type"))
					has_dronebl_type = 1;
				else if (!strcmp(cepp->name, "staging"))
					;
			}
		}
		else if (!cep->value)
		{
			config_error_empty(cep->file->filename, cep->line_number,
				"spamreport", cep->name);
			errors++;
			continue;
		} else
		if (!strcmp(cep->name, "url"))
		{
			if (has_url)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamreport::url");
				continue;
			}
			has_url = 1;
		}
		else if (!strcmp(cep->name, "type"))
		{
			if (has_type)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamreport::type");
				continue;
			}
			has_type = parse_spamreport_type(cep->value);
			if (!has_type)
			{
				config_error("%s:%i: spamreport::type: unknown type '%s', supported types are: simple, dronebl, central-spamreport.",
					cep->file->filename, cep->line_number, cep->value);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "http-method"))
		{
			if (has_http_method)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "spamreport::http-method");
				continue;
			}
			has_http_method = 1;
			if (strcmp(cep->value, "get") && strcmp(cep->value, "post"))
			{
				config_error("%s:%i: spamreport::http-method: only 'get' and 'post' are supported",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else if (!strcmp(cep->name, "rate-limit"))
		{
			int count = 0, period = 0;
			if (!config_parse_flood(cep->value, &count, &period))
			{
				config_error("%s:%i: spamreport::rate-limit: invalid format, must be count:time.",
					cep->file->filename, cep->line_number);
				errors++;
			}
		}
		else
		{
			config_error_unknown(cep->file->filename, cep->line_number,
				"spamreport", cep->name);
			errors++;
			continue;
		}
	}

	if (!has_type)
	{
		config_error_missing(ce->file->filename, ce->line_number, "spamreport::type");
		errors++;
	}

	if (has_type == SPAMREPORT_TYPE_CENTRAL_SPAMREPORT)
	{
		/* Nothing required */
	} else
	if (has_type == SPAMREPORT_TYPE_DRONEBL)
	{
		if (!has_dronebl_rpckey || !has_dronebl_type)
		{
			config_error("%s:%i: spamreport: type dronebl used, missing spamreport::parameters: rpckey and/or type",
			             ce->file->filename, ce->line_number);
			errors++;
		}
	} else
	{
		if (!has_url)
		{
			config_error_missing(ce->file->filename, ce->line_number, "spamreport::url");
			errors++;
		}

		if (!has_http_method)
		{
			config_error_missing(ce->file->filename, ce->line_number, "spamreport::http-method");
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

/** Process a spamreport { } block in the configuration file */
int tkl_config_run_spamreport(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	ConfigEntry *cepp;
	Spamreport *s;

	/* We are only interested in spamreport { } blocks */
	if ((type != CONFIG_MAIN) || strcmp(ce->name, "spamreport"))
		return 0;

	if (find_spamreport_block(ce->value))
	{
		config_error("%s:%d: spamreport block '%s' already exists, this duplicate one is ignored.",
		             ce->file->filename, ce->line_number, ce->value);
		return 1;
	}

	s = safe_alloc(sizeof(Spamreport));
	safe_strdup(s->name, ce->value);

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "url"))
		{
			safe_strdup(s->url, cep->value);
		}
		else if (!strcmp(cep->name, "type"))
		{
			s->type = parse_spamreport_type(cep->value);

			if ((s->type == SPAMREPORT_TYPE_CENTRAL_SPAMREPORT) &&
			    !is_module_loaded("central-blocklist"))
			{
				config_warn("%s:%d: blacklist block with type 'central-spamreport' but the 'central-blocklist' module is not loaded.",
					ce->file->filename, ce->line_number);
			}
		}
		else if (!strcmp(cep->name, "http-method"))
		{
			if (!strcmp(cep->value, "get"))
				s->http_method = HTTP_METHOD_GET;
			else if (!strcmp(cep->value, "post"))
				s->http_method = HTTP_METHOD_POST;
		}
		else if (!strcmp(cep->name, "rate-limit"))
		{
			config_parse_flood(cep->value, &s->rate_limit_count, &s->rate_limit_period);
		}
		else if (!strcmp(cep->name, "parameters"))
		{
			for (cepp = cep->items; cepp; cepp = cepp->next)
			{
				if (!strcmp(cepp->name, "staging"))
				{
					if (cepp->value && config_checkval(cepp->value, CFG_YESNO)==0)
						continue; /* skip on 'staging no;' */
				}
				add_nvplist(&s->parameters, 0, cepp->name, cepp->value);
			}
		}
		else if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &s->except);
		}
	}

	if (s->type == SPAMREPORT_TYPE_DRONEBL)
		s->http_method = HTTP_METHOD_POST;

	AddListItem(s, spamreports);
	return 1;
}

void free_spamreport_block(Spamreport *s)
{
	DelListItem(s, spamreports);
	safe_free(s->name);
	safe_free(s->url);
	safe_free_nvplist(s->parameters);
	free_security_group(s->except);
	safe_free(s);
}

void free_spamreport_blocks(void)
{
	Spamreport *s, *s_next;
	for (s = spamreports; s; s = s_next)
	{
		s_next = s->next;
		free_spamreport_block(s);
	}
	spamreports = NULL;
}

Spamreport *find_spamreport_block(const char *name)
{
	Spamreport *s;

	for (s = spamreports; s; s = s->next)
		if (!strcmp(s->name, name))
			return s;

	return NULL;
}

SpamreportType parse_spamreport_type(const char *s)
{
	if (!strcmp(s, "simple"))
		return SPAMREPORT_TYPE_SIMPLE;
	else if (!strcmp(s, "dronebl"))
		return SPAMREPORT_TYPE_DRONEBL;
	else if (!strcmp(s, "central-spamreport"))
		return SPAMREPORT_TYPE_CENTRAL_SPAMREPORT;
	return 0;
}

/* Returns 1 if ratelimited (don't do the request), otherwise 0 */
int spamfilter_block_rate_limited(Spamreport *spamreport)
{
	SpamreportCounter *s;

	/* First for the case where there is no rate limit configured... */
	if (spamreport->rate_limit_count == 0)
		return 0;

	/* First find the block (allocate a new one if not found) */
	for (s = spamreportcounters; s; s = s->next)
		if (!strcmp(s->name, spamreport->name))
			break;
	if (s == NULL)
	{
		s = safe_alloc(sizeof(SpamreportCounter));
		safe_strdup(s->name, spamreport->name);
		AddListItem(s, spamreportcounters);
	}

	/* Now do the flood check */
	if (s->rate_limit_time + spamreport->rate_limit_period <= TStime())
	{
		/* Time exceeded, reset */
		s->rate_limit_count = 0;
		s->rate_limit_time = TStime();
	}
	if (s->rate_limit_count <= spamreport->rate_limit_count)
		s->rate_limit_count++;
	if (s->rate_limit_count > spamreport->rate_limit_count)
	{
		if (s->last_warning_sent + SPAMREPORT_RATE_LIMIT_WARNING_EVERY < TStime())
		{
			unreal_log(ULOG_WARNING, "spamreport", "SPAMREPORT_RATE_LIMIT", NULL,
				   "[spamreport] Rate limit of $rate_limit_count:$rate_limit_period hit "
				   "for block $spamreport_block -- further requests dropped (throttled).",
				   log_data_integer("rate_limit_count", spamreport->rate_limit_count),
				   log_data_integer("rate_limit_period", spamreport->rate_limit_period),
				   log_data_string("spamreport_block", spamreport->name));
			s->last_warning_sent = TStime();
		}
		return 1; /* Limit exceeded */
	}
	return 0; /* All OK */
}

/** Return 1 if there is a spamreport { type central-spamreport; } block */
int _central_spamreport_enabled(void)
{
	Spamreport *s;
	for (s = spamreports; s; s = s->next)
		if (s->type == SPAMREPORT_TYPE_CENTRAL_SPAMREPORT)
			return 1;
	return 0;
}

int _spamreport(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block, Client *by)
{
	Spamreport *s;
	OutgoingWebRequest *request;
	char urlbuf[512];
	char bodybuf[512];
	char *url = NULL;
	char *body = NULL;
	NameValuePrioList *headers = NULL;
	int num;

	num = downloads_in_progress();
	if (num > 100)
	{
		// TODO: throttle this error
		unreal_log(ULOG_WARNING, "spamreport", "SPAMREPORT_TOO_MANY_CONCURRENT_REQUESTS", NULL,
		           "Already $num_requests HTTP(S) requests in progress, new spamreport requests ignored.",
		           log_data_integer("num_requests", num));
		return 0;
	}

	if (!spamreport_block)
	{
		int ret = 0;
		for (s = spamreports; s; s = s->next)
			ret += spamreport(client, ip, details, s->name, by);
		return ret;
	}

	s = find_spamreport_block(spamreport_block);
	if (!s)
		return 0; /* NOTFOUND */

	if (s->except && client && user_allowed_by_security_group(client, s->except))
		return 0;
	// NOTE: 'except' is bypassed for manual SPAMREPORT with an ip and no client.

	if (spamfilter_block_rate_limited(s))
		return 0; /* spamreport::rate-limit exceeded */

	if (s->type == SPAMREPORT_TYPE_SIMPLE)
	{
		NameValuePrioList *list = NULL;
		list = duplicate_nvplist(details);
		add_nvplist(&list, -1, "ip", ip);
		buildvarstring_nvp(s->url, urlbuf, sizeof(urlbuf), list, BUILDVARSTRING_URLENCODE|BUILDVARSTRING_UNKNOWN_VAR_IS_EMPTY);
		url = urlbuf;
		safe_free_nvplist(list);
		if (s->http_method == HTTP_METHOD_POST)
		{
			body = strchr(url, '?');
			if (body)
				*body++ = '\0';
		}
	} else
	if (s->type == SPAMREPORT_TYPE_DRONEBL)
	{
		NameValuePrioList *list = NULL;
		NameValuePrioList *list2 = NULL;
		char fmtstring[512];
		url = "https://dronebl.org/rpc2";
		list = duplicate_nvplist(details);
		duplicate_nvplist_append(s->parameters, &list);
		add_nvplist(&list, -1, "ip", ip);
		snprintf(fmtstring, sizeof(fmtstring),
		         "<?xml version='1.0'?>\n"
		         "<request key='$rpckey'%s>\n"
		         " <add ip='$ip' type='$type' comment='$comment'>\n"
		         "</request>\n",
		         find_nvplist(s->parameters, "staging") ? " staging='1'" : "");
		buildvarstring_nvp(fmtstring, bodybuf, sizeof(bodybuf), list, BUILDVARSTRING_XML|BUILDVARSTRING_UNKNOWN_VAR_IS_EMPTY);
		body = bodybuf;
		safe_free_nvplist(list); // frees all the duplicated lists
		add_nvplist(&headers, 0, "Content-Type", "text/xml");
	} else
	if (s->type == SPAMREPORT_TYPE_CENTRAL_SPAMREPORT)
	{
		return central_spamreport(client, by);
	} else
	{
		abort();
	}

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "spamreport", "SPAMREPORT_SEND_REQUEST", NULL,
	           "Calling url '$url' with body '$body'",
	           log_data_string("url", url),
	           log_data_string("body", (body ? body : "")));
#endif
	/* Do the web request */
	request = safe_alloc(sizeof(OutgoingWebRequest));
	safe_strdup(request->url, url);
	request->http_method = s->http_method;
	safe_strdup(request->body, body);
	request->headers = headers;
	request->callback = download_complete_dontcare;
	request->max_redirects = 3;
	url_start_async(request);
	return 1;
}

CMD_FUNC(cmd_spamreport)
{
	Spamreport *to = NULL; /* default is NULL, meaning: all */
	Client *target = NULL;
	const char *ip;
	int n;

	if (!ValidatePermissionsForPath("server-ban:spamreport",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SPAMREPORT");
		return;
	}

	ip = parv[1];

	if ((target = find_user(parv[1], NULL)))
	{
		if (!MyUser(target))
		{
			/* Forward it to other server */
			if (parc > 2)
			{
				sendto_one(target, NULL, ":%s SPAMREPORT %s %s",
					   client->id, parv[1], parv[2]);
			} else {
				sendto_one(target, NULL, ":%s SPAMREPORT %s",
					   client->id, parv[1]);
			}
			return;
		}
		/* It's for us */
		if (target->ip)
			ip = target->ip;
	}

	if (!is_valid_ip(ip))
	{
		sendnotice(client, "Not a valid nick/IP: %s", ip);
		return;
	}

	if ((parc > 2) && !BadPtr(parv[2]))
	{
		to = find_spamreport_block(parv[2]);
		if (!to)
		{
			sendnotice(client, "Could not find spamreport block '%s'", parv[2]);
			return;
		}
	}

	if (!((n = spamreport(target, ip, NULL, to ? to->name : NULL, client))))
		sendnotice(client, "Could not report spam. No spamreport { } blocks configured, or all filtered out/exempt.");
	else
		sendnotice(client, "Sending spam report to %d target(s)", n);
}

void spamreportcounters_free_all(ModData *m)
{
	SpamreportCounter *s, *s_next;
	for (s = spamreportcounters; s; s = s_next)
	{
		s_next = s->next;
		safe_free(s->name);
		safe_free(s);
	}
}
