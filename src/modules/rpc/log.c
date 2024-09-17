/* log.* RPC calls
 * (C) Copyright 2023-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/log",
	"1.0.2",
	"log.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_log_hook_subscribe(Client *client, json_t *request, json_t *params);
void rpc_log_hook_unsubscribe(Client *client, json_t *request, json_t *params);
void rpc_log_list(Client *client, json_t *request, json_t *params);
void rpc_log_send(Client *client, json_t *request, json_t *params);
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

	memset(&r, 0, sizeof(r));
	r.method = "log.list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_log_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/log] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "log.send";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_log_send;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/send] Could not register RPC handler");
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

	if (!strcmp(subsystem, "rawtraffic") || (loglevel == ULOG_DEBUG))
		return 0;

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

void rpc_log_list(Client *client, json_t *request, json_t *params)
{
	json_t *sources;
	size_t index;
	json_t *value;
	const char *str;
	json_t *result = json_object();
	json_t *list = json_array();
	LogEntry *e;
	int i;
	LogSource *log_sources = NULL;
	LogSource *s;

	/* Optionally filter on sources: */
	sources = json_object_get(params, "sources");
	if (sources && json_is_array(sources))
	{
		json_array_foreach(sources, index, value)
		{
			str = json_get_value(value);
			if (!str)
				continue;

			s = add_log_source(str);
			AddListItem(s, log_sources);
		}
	}

	json_object_set_new(result, "list", list);

	for (e = memory_log; e; e = e->next)
	{
		if (log_sources &&
		    !log_sources_match(log_sources, e->loglevel, e->subsystem, e->event_id, 0))
		{
			continue;
		}
		json_array_append(list, e->json);
	}

	rpc_response(client, request, result);
	json_decref(result);

	free_log_sources(log_sources);
}


void rpc_log_send(Client *client, json_t *request, json_t *params)
{
	json_t *result;
	json_error_t *jerr;
	const char *msg, *level, *subsystem, *event_id, *log_source, *timestamp;
	const char *serialized;
	MessageTag *mtags = NULL;

	REQUIRE_PARAM_STRING("msg", msg);
	REQUIRE_PARAM_STRING("level", level);
	REQUIRE_PARAM_STRING("subsystem", subsystem);
	REQUIRE_PARAM_STRING("event_id", event_id);
	OPTIONAL_PARAM_STRING("timestamp", timestamp);

	new_message(&me, NULL, &mtags);

	serialized = json_dumps(params, JSON_COMPACT);
	if (!serialized)
	{
		unreal_log(ULOG_INFO, "log", "RPC_LOG_INVALID", client,
		           "Received malformed JSON in RPC log message (log.send) from $client.name");
		return;
	}
	
	MessageTag *json_mtag = safe_alloc(sizeof(MessageTag)); 
	safe_strdup(json_mtag->name, "unrealircd.org/json-log");
	safe_strdup(json_mtag->value, serialized);
	AddListItem(json_mtag, mtags);

	const char *cmd_params[6] = {
		me.name,
		level,
		subsystem,
		event_id,
		msg,
		NULL
	};

	do_cmd(&me, mtags, "SLOG", 5, cmd_params);
	safe_free_message_tags(mtags);
	
	/* Simply return success */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
