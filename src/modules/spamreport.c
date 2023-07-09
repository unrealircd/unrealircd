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

/* Enums and structs */
typedef enum SpamreportType {
	SPAMREPORT_TYPE_SIMPLE = 1,
	SPAMREPORT_TYPE_DRONEBL = 2,
} SpamReportType;

typedef struct Spamreport Spamreport;
struct Spamreport {
	Spamreport *prev, *next;
	char *name; /**< Name of the block, spamreport <this> { } */
	char *url; /**< URL to use */
	SpamReportType type;
	HttpMethod http_method;
	NameValuePrioList *parameters;
	SecurityGroup *except;
	int rate_limit_num;
	int rate_limit_per;
	int rate_limit_counter;
	time_t rate_limit_time;
};

/* Forward declarations */
CMD_FUNC(cmd_spamreport);
int tkl_config_test_spamreport(ConfigFile *, ConfigEntry *, int, int *);
int tkl_config_run_spamreport(ConfigFile *, ConfigEntry *, int);
void free_spamreport_blocks(void);
int _spamreport(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block);

/* Variables */
Spamreport *spamreports = NULL;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_SPAMREPORT, _spamreport);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkl_config_test_spamreport);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, "SPAMREPORT", cmd_spamreport, MAXPARA, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkl_config_run_spamreport);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
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
			has_type = 1;
			if (!strcmp(cep->value, "simple"))
				;
			else if (!strcmp(cep->value, "dronebl"))
				has_type_dronebl = 1;
			else
			{
				config_error("%s:%i: spamreport::type: only 'simple' is supported at the moment",
					cep->file->filename, cep->line_number);
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
			config_error("%s:%i: spamreport::rate-limit: not implemented yet",
				cep->file->filename, cep->line_number);
			errors++;
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

	if (has_type_dronebl)
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
			if (!strcmp(cep->value, "simple"))
				s->type = SPAMREPORT_TYPE_SIMPLE;
			else if (!strcmp(cep->value, "dronebl"))
				s->type = SPAMREPORT_TYPE_DRONEBL;
			else
				abort();
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
			// TODO
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

int _spamreport(Client *client, const char *ip, NameValuePrioList *details, const char *spamreport_block)
{
	Spamreport *s;
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
			ret += spamreport(client, ip, details, s->name);
		return ret;
	}

	s = find_spamreport_block(spamreport_block);
	if (!s)
		return -1; /* NOTFOUND */

	if (s->except && client && user_allowed_by_security_group(client, s->except))
		return 0;
	// NOTE: 'except' is bypassed for manual SPAMREPORT with an ip and no client.

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
	} else
		abort();

#ifdef DEBUGMODE
	unreal_log(ULOG_DEBUG, "spamreport", "SPAMREPORT_SEND_REQUEST", NULL,
	           "Calling url '$url' with body '$body'",
	           log_data_string("url", url),
	           log_data_string("body", (body ? body : "")));
#endif
	url_start_async(url, s->http_method, body, headers, 0, 0, download_complete_dontcare, NULL, url, 3);
	safe_free_nvplist(headers);
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

	if ((parc > 2) && !BadPtr(parv[2]))
	{
		to = find_spamreport_block(parv[2]);
		if (!to)
		{
			sendnotice(client, "Could not find spamreport block '%s'", parv[2]);
			return;
		}
	}

	if ((target = find_user(parv[1], NULL)) && target->ip)
		ip = target->ip;

	if (!is_valid_ip(ip))
	{
		sendnotice(client, "Not a valid IP: %s", ip);
		return;
	}

	if (!((n = spamreport(target, ip, NULL, to ? to->name : NULL))))
		sendnotice(client, "Could not report spam. No spamreport { } blocks configured, or all filtered out/exempt.");
	else
		sendnotice(client, "Sending spam report to %d target(s)", n);
}
