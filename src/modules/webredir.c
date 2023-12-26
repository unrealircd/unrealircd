/*
 * IRC - Internet Relay Chat, src/modules/webredir.c
 * webredir UnrealIRCd module
 * (C) Copyright 2019 i <info@servx.org> and the UnrealIRCd team
 *
 * This module will 301-redirect any clients issuing GET's/POST's during pre-connect stage.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

CMD_FUNC(webredir);

ModuleHeader MOD_HEADER
  = {
	"webredir",
	"1.0",
	"Do 301 redirect for HEAD/GET/POST/PUT commands", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

struct {
	char *url;
} cfg;

static int nowebredir = 1;

static void free_config(void);
static void init_config(void);
int webredir_config_posttest(int *errs);
int webredir_config_test(ConfigFile *, ConfigEntry *, int, int *);
int webredir_config_run(ConfigFile *, ConfigEntry *, int);

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, webredir_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, webredir_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	init_config();
	CommandAdd(modinfo->handle, "HEAD", webredir, MAXPARA, CMD_UNREGISTERED);
	CommandAdd(modinfo->handle, "GET", webredir, MAXPARA, CMD_UNREGISTERED);
	CommandAdd(modinfo->handle, "POST", webredir, MAXPARA, CMD_UNREGISTERED);
	CommandAdd(modinfo->handle, "PUT", webredir, MAXPARA, CMD_UNREGISTERED);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, webredir_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (SHOWCONNECTINFO)
	{
		config_warn("I'm disabling set::options::show-connect-info for you "
			    "as this setting is incompatible with the webredir module.");
		SHOWCONNECTINFO = 0;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

static void init_config(void)
{
	memset(&cfg, 0, sizeof(cfg));
}

static void free_config(void)
{
	safe_free(cfg.url);

	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

int webredir_config_posttest(int *errs)
{
	int errors = 0;

	if (nowebredir)
	{
		config_error("set::webredir is missing!");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int webredir_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	int has_url = 0;
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	/* We are only interrested in set::webredir... */
	if (!ce || !ce->name || strcmp(ce->name, "webredir"))
		return 0;

	nowebredir = 0;
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: set::webredir::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
		else if (!strcmp(cep->name, "url"))
		{
			if (!*cep->value || strchr(cep->value, ' '))
			{
				config_error("%s:%i: set::webredir::%s with empty value",
					cep->file->filename, cep->line_number, cep->name);
				errors++;
			}
			if (!strstr(cep->value, "://") || !strcmp(cep->value, "https://..."))
			{
				config_error("%s:%i: set::webredir::url needs to be a valid URL",
					cep->file->filename, cep->line_number);
				errors++;
			}
			if (has_url)
			{
				config_warn_duplicate(cep->file->filename,
					cep->line_number, "set::webredir::url");
				continue;
			}
			has_url = 1;
		}
		else
		{
			config_error("%s:%i: unknown directive set::webredir::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}

	if (!has_url)
	{
		config_error_missing(ce->file->filename, ce->line_number,
			"set::webredir::url");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int webredir_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::webredir... */
	if (!ce || !ce->name || strcmp(ce->name, "webredir"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "url"))
		{
			safe_strdup(cfg.url, cep->value);
		}
	}
	return 1;
}

CMD_FUNC(webredir)
{
	if (!MyConnect(client))
		return;

	sendto_one(client, NULL, "HTTP/1.1 301 Moved Permanently");
	sendto_one(client, NULL, "Location: %s\r\n\r\n", cfg.url);
	dead_socket(client, "Connection closed");
}
