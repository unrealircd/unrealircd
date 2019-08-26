/*
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

ModuleHeader MOD_HEADER(webredir)
  = {
	"webredir",
	"v1.0",
	"Do 301 redirect for HEAD/GET/POST/PUT commands", 
	"UnrealIRCd Team",
	"unrealircd-5",
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

MOD_TEST(webredir)
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, webredir_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, webredir_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT(webredir)
{
	CommandAdd(modinfo->handle, "HEAD", webredir, MAXPARA, M_UNREGISTERED);
	CommandAdd(modinfo->handle, "GET", webredir, MAXPARA, M_UNREGISTERED);
	CommandAdd(modinfo->handle, "POST", webredir, MAXPARA, M_UNREGISTERED);
	CommandAdd(modinfo->handle, "PUT", webredir, MAXPARA, M_UNREGISTERED);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, webredir_config_run);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	init_config();
	return MOD_SUCCESS;
}

MOD_LOAD(webredir)
{
	if (SHOWCONNECTINFO)
	{
		config_warn("I'm disabling set::options::show-connect-info for you "
			    "as this setting is incompatible with the webredir module.");
		SHOWCONNECTINFO = 0;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD(webredir)
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
	if (cfg.url)
		MyFree(cfg.url);

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
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "webredir"))
		return 0;

	nowebredir = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata)
		{
			config_error("%s:%i: set::webredir::%s with no value",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
		else if (!strcmp(cep->ce_varname, "url"))
		{
			if (!*cep->ce_vardata || strchr(cep->ce_vardata, ' '))
			{
				config_error("%s:%i: set::webredir::%s with empty value",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				errors++;
			}
			if (!url_is_valid(cep->ce_vardata) || !strcmp(cep->ce_vardata, "https://..."))
			{
				config_error("%s:%i: set::webredir::url needs to be a valid URL",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
				errors++;
			}
			if (has_url)
			{
				config_warn_duplicate(cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, "set::webredir::url");
				continue;
			}
			has_url = 1;
		}
		else
		{
			config_error("%s:%i: unknown directive set::webredir::%s",
				cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
		}
	}

	if (!has_url)
	{
		config_error_missing(ce->ce_fileptr->cf_filename, ce->ce_varlinenum,
			"set::webredir::url");
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int webredir_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cep2;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::webredir... */
	if (!ce || !ce->ce_varname || strcmp(ce->ce_varname, "webredir"))
		return 0;
	
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "url"))
		{
			if (cfg.url)
				MyFree(cfg.url);
			cfg.url = strdup(cep->ce_vardata);
		}
	}
	return 1;
}

CMD_FUNC(webredir)
{
	if (MyConnect(sptr))
	{
		sendto_one(sptr, NULL, "HTTP/1.1 301 Moved Permanently");
		sendto_one(sptr, NULL, "Location: %s\r\n\r\n", cfg.url);
		dead_link(sptr, "Connection closed");
	}
	return 0;
}
