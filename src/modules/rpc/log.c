/* log.* RPC calls
 * (C) Copyright 2023-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/log",
	"1.0.0",
	"log.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_log_hook_subscribe(Client *client, json_t *request, json_t *params);
void rpc_log_hook_unsubscribe(Client *client, json_t *request, json_t *params);
int rpc_log_hook(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, json_t *json, const char *json_serialized, const char *timebuf);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "log.subscribe";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_log_hook_subscribe;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/log] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "log.unsubscribe";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_log_hook_unsubscribe;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/log] Could not register RPC handler");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_LOG, 0, rpc_log_hook);

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void rpc_log_hook_subscribe(Client *client, json_t *request, json_t *params)
{
	json_t *result;
	json_t *sources;
	size_t index;
	json_t *value;
	const char *str;
	LogSource *s;

	sources = json_object_get(params, "sources");
	if (!sources || !json_is_array(sources))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", "sources");
		return;
	}

	/* Erase the old subscriptions first */
	free_log_sources(client->rpc->log_sources);
	client->rpc->log_sources = NULL;

	/* Add subscriptions... */
	json_array_foreach(sources, index, value)
	{
		str = json_get_value(value);
		if (!str)
			continue;

		s = add_log_source(str);
		AddListItem(s, client->rpc->log_sources);
	}

	result = json_boolean(1);

	rpc_response(client, request, result);
	json_decref(result);
}

/** log.unsubscribe: unsubscribe from all log messages */
void rpc_log_hook_unsubscribe(Client *client, json_t *request, json_t *params)
{
	json_t *result;

	free_log_sources(client->rpc->log_sources);
	client->rpc->log_sources = NULL;
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

int rpc_log_hook(LogLevel loglevel, const char *subsystem, const char *event_id, MultiLine *msg, json_t *json, const char *json_serialized, const char *timebuf)
{
	Client *client;
	json_t *request = NULL;

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (IsRPC(client) && client->rpc->log_sources &&
		    log_sources_match(client->rpc->log_sources, loglevel, subsystem, event_id, 0))
		{
			if (request == NULL)
			{
				/* Lazy initalization */
				request = json_object();
				json_object_set_new(request, "method", json_string_unreal("log.event"));
			}
			rpc_response(client, request, json);
		}
	}

	if (request)
		json_decref(request);

	return 0;
}
