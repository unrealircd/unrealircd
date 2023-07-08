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

/* Structs */
typedef struct Spamreport Spamreport;
struct Spamreport {
	Spamreport *prev, *next;
	char *name; /**< Name of the block, spamreport <this> { } */
	char *url; /**< URL to use */
	int type;
	HttpMethod http_method;
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

/* Variables */
Spamreport *spamreports = NULL;

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
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
	ConfigEntry *cep;
	int errors = 0;
	char has_url=0, has_type=0, has_http_method=0;

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
			if (strcmp(cep->value, "simple"))
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
			if (strcmp(cep->value, "get"))
			{
				config_error("%s:%i: spamreport::http-method: only 'get' is supported at the moment",
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

	if (!has_url)
	{
		config_error_missing(ce->file->filename, ce->line_number, "spamreport::url");
		errors++;
	}

	if (!has_type)
	{
		config_error_missing(ce->file->filename, ce->line_number, "spamreport::type");
		errors++;
	}

	if (!has_http_method)
	{
		config_error_missing(ce->file->filename, ce->line_number, "spamreport::http-method");
		errors++;
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

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "url"))
		{
			safe_strdup(s->url, cep->value);
		}
		else if (!strcmp(cep->name, "type"))
		{
			// TODO
		}
		else if (!strcmp(cep->name, "http-method"))
		{
			s->http_method = HTTP_METHOD_GET;
		}
		else if (!strcmp(cep->name, "rate-limit"))
		{
			// TODO
		}
		else if (!strcmp(cep->name, "except"))
		{
			conf_match_block(cf, cep, &s->except);
		}
	}

	AddListItem(s, spamreports);
	return 1;
}

void free_spamreport_block(Spamreport *s)
{
	DelListItem(s, spamreports);
	safe_free(s->name);
	safe_free(s->url);
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

int report_spam(Client *client, const char *ip, NameValuePrioList *details, Spamreport *s)
{
	NameValuePrioList *list;
	char url[512];

	if (s == NULL)
	{
		int ret = 0;
		for (s = spamreports; s; s = s->next)
			ret += report_spam(client, ip, details, s);
		return ret;
	}

	if (s->except && client && user_allowed_by_security_group(client, s->except))
		return 0;
	// NOTE: 'except' is bypassed for manual SPAMREPORT with an ip and no client.

	list = duplicate_nvplist(details);
	add_nvplist(&list, -1, "ip", ip);
	buildvarstring_nvp(s->url, url, sizeof(url), list);
	safe_free_nvplist(list);

	url_start_async(url, HTTP_METHOD_GET, NULL, 0, 0, download_complete_dontcare, NULL, url, 3);
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

	if (!((n = report_spam(target, ip, NULL, to))))
		sendnotice(client, "Could not report spam. No spamreport { } blocks configured, or all filtered out/exempt.");
	else
		sendnotice(client, "Sending spam report to %d target(s)", n);
}
