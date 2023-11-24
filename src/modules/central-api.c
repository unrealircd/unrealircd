/* Central API - API access to unrealircd.org
 * (C) Copyright 2023 Bram Matthys and The UnrealIRCd Team
 * License: GPLv2
 */

/*** <<<MODULE MANAGER START>>>
module
{
	documentation "https://www.unrealircd.org/docs/Central_API";

	// This is displayed in './unrealircd module info ..' and also if compilation of the module fails:
	troubleshooting "Please report at https://bugs.unrealircd.org/ if this module fails to compile";

	// Minimum version necessary for this module to work:
	min-unrealircd-version "6.1.2";

	// Maximum version
	max-unrealircd-version "6.*";

	post-install-text {
		"The module is installed. See https://www.unrealircd.org/docs/Central_API";
		"for the configuration that you need to add.";
	}
}
*** <<<MODULE MANAGER END>>>
*/

#include "unrealircd.h"

#ifndef HOOKTYPE_GET_CENTRAL_API_KEY
 #define HOOKTYPE_GET_CENTRAL_API_KEY 199
#endif
ModuleHeader MOD_HEADER
  = {
	"third/central-api",
	"1.0.1",
	"Acquire and set API key for unrealircd.org services",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

struct cfgstruct {
	char *request_key_challenge;
	char *request_key_response;
	char *api_key;
};

static struct cfgstruct cfg;

struct reqstruct {
	char api_key;
	char request_key;
};
static struct reqstruct req;

/* Forward declarations */
int capi_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int capi_config_posttest(int *errs);
int capi_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
CMD_FUNC(cmd_centralapisrv);
const char *capi_get_central_api_key(void);

static void free_config(void)
{
	safe_free(cfg.request_key_challenge);
	safe_free(cfg.request_key_response);
	safe_free(cfg.api_key);
	memset(&cfg, 0, sizeof(cfg)); /* needed! */
}

MOD_TEST()
{
	memset(&req, 0, sizeof(req));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, capi_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, capi_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	ModDataInfo mreq;

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, capi_config_run);
	HookAddConstString(modinfo->handle, HOOKTYPE_GET_CENTRAL_API_KEY, 0, capi_get_central_api_key);
	CommandAdd(modinfo->handle, "CENTRALAPISRV", cmd_centralapisrv, MAXPARA, CMD_UNREGISTERED);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	free_config();
	return MOD_SUCCESS;
}

/** Test the set::central-api configuration */
int capi_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::central-api.. */
	if (!ce || !ce->name || strcmp(ce->name, "central-api"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: set::central-api::%s with no value",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "request-key"))
		{
			char *p = strchr(cep->value, '-');
			if (!p)
			{
				config_error("%s:%i: set::central-api::request-key: Invalid format for. "
				             "Please check if you copy-pasted the key correctly.",
				             cep->file->filename, cep->line_number);
				errors++;
			}
			req.request_key = 1;
			
		} else
		if (!strcmp(cep->name, "api-key"))
		{
			req.api_key = 1;
		} else
		{
			config_error("%s:%i: unknown directive set::central-api::%s",
				cep->file->filename, cep->line_number, cep->name);
			errors++;
			continue;
		}
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int capi_config_posttest(int *errs)
{
	int errors = 0;

	if (!req.api_key && !req.request_key)
	{
		config_error("You need to set either set::central-api::request-key or set::central-api::api-key (not both or none).");
		config_error("See https://www.unrealircd.org/docs/Central_API for the documentation");
		errors++;
	} 

	*errs = errors;
	return errors ? -1 : 1;
}

/* These are prefix and suffix strings. We use sha256(prefix + text + suffix).
 * These are public. The only purpose they serve is that we don't do
 * simple sha256(hash) which a little bit too simple and there may be
 * precomputed rainbowtables like... ah well.. what am i saying, this is
 * totally over the top at the moment for our purpose...
 * One could even argue why we hash at all in this particular use-case.
 */

#define CAPI_HASH_STRING_PREFIX "7Wre2KPYLumXyi04I5T3QLlzbKVpYxlYGk8rI1M2ypWIoKZKINWnUiMrQ8fPWByw"
#define CAPI_HASH_STRING_SUFFIX "Q3KLNPyEla2F88TOcs11ZARfHpWJaZajNvzWYoadJA6MAKXMtOaR16EugTUi3Kja"

char *capi_hash(const char *in)
{
	char buf[512];
	static char hashbuf[128];

	snprintf(buf, sizeof(buf), "%s:%s:%s",
		CAPI_HASH_STRING_PREFIX,
		in,
		CAPI_HASH_STRING_SUFFIX);
	return sha256hash(hashbuf, buf, strlen(buf));
}

/* Configure ourselves based on the set::central-api settings */
int capi_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep, *cepp;

	if (type != CONFIG_SET)
		return 0;
	
	/* We are only interrested in set::central-api.. */
	if (!ce || !ce->name || strcmp(ce->name, "central-api"))
		return 0;
	
	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "request-key"))
		{
			char buf[512];
			char hashbuf[128];
			char *p;

			strlcpy(buf, cep->value, sizeof(buf));
			p = strchr(buf, '-');
			*p++ = '\0'; /* no null pointer, already validated by config test */

			safe_strdup(cfg.request_key_challenge, capi_hash(buf));
			safe_strdup(cfg.request_key_response, capi_hash(p));
		} else
		if (!strcmp(cep->name, "api-key"))
		{
			safe_strdup(cfg.api_key, cep->value);
		}
	}
	return 1;
}

CMD_FUNC(cmd_centralapisrv)
{
	if (!MyConnect(client) || !IsSecure(client) || (parc < 2))
		return;

	if (!strcmp(parv[1], "REQUEST_CHALLENGE") && (parc > 2))
	{
		if (cfg.request_key_challenge && !strcmp(parv[2], cfg.request_key_challenge))
		{
			json_t *j;
			char *json_serialized;

			unreal_log(ULOG_INFO, "central-api", "CENTRALAPI_HANDSHAKE", client,
			           "Received central-api key request handshake from $client.details");

			j = json_object();
			json_object_set_new(j, "response", json_string_unreal(cfg.request_key_response));
			json_object_set_new(j, "network", json_string_unreal(iConf.network_name_005));
			json_object_set_new(j, "lusers", json_integer(irccounts.me_clients));
			json_object_set_new(j, "gusers", json_integer(irccounts.clients));
			json_object_set_new(j, "servers", json_integer(irccounts.servers));
			json_serialized = json_dumps(j, JSON_COMPACT);
			if (!json_serialized)
			{
				unreal_log(ULOG_ERROR, "central-api", "CENTRALAPI_JSON_OUTPUT_ERROR", client,
				           "Error writing JSON response!?");
				json_decref(j);
				return;
			}
			sendto_one(client, NULL, ":%s CENTRALAPISRV REQUEST_RESPONSE :%s",
			           me.name, json_serialized);
			safe_free(json_serialized);
			json_decref(j);
			return;
		}
	}
}

const char *capi_get_central_api_key(void)
{
	if (cfg.api_key)
		return cfg.api_key;
	return NULL;
}
