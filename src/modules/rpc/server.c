/* server.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/server",
	"1.0.0",
	"server.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_server_list);
RPC_CALL_FUNC(rpc_server_get);
RPC_CALL_FUNC(rpc_server_rehash);

int rpc_server_rehash_log(int failure, json_t *rehash_log);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "server.list";
	r.call = rpc_server_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "server.get";
	r.call = rpc_server_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "server.rehash";
	r.call = rpc_server_rehash;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}

	HookAdd(modinfo->handle, HOOKTYPE_REHASH_LOG, 0, rpc_server_rehash_log);

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

RPC_CALL_FUNC(rpc_server_list)
{
	json_t *result, *list, *item;
	Client *acptr;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (!IsServer(acptr) && !IsMe(acptr))
			continue;

		item = json_object();
		json_expand_client(item, NULL, acptr, 1);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_server_get)
{
	json_t *result, *list, *item;
	const char *server;
	Client *acptr;

	OPTIONAL_PARAM_STRING("server", server);
	if (server)
	{
		if (!(acptr = find_server(server, NULL)))
		{
			rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Server not found");
			return;
		}
	} else {
		acptr = &me;
	}

	result = json_object();
	json_expand_client(result, "server", acptr, 1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_server_rehash)
{
	json_t *result, *list, *item;
	const char *server;
	Client *acptr;

	OPTIONAL_PARAM_STRING("server", server);
	if (server)
	{
		if (!(acptr = find_server(server, NULL)))
		{
			rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Server not found");
			return;
		}
	} else {
		acptr = &me;
	}

	if (acptr != &me)
	{
		/* Not implemented yet */
		// TODO: forward, simply with no true response?
		result = json_boolean(0);
		rpc_response(client, request, result);
		json_decref(result);
		return;
	}

	if (client->local->rpc->rehash_request)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "A rehash initiated by you is already in progress");
		return;
	}

	/* It's for me... */
	if (loop.rehashing)
	{
		/* simply join an existing rehash? */
		SetMonitorRehash(client);
	} else {
		unreal_log(ULOG_INFO, "config", "CONFIG_RELOAD", client, "Rehashing server configuration file [by: $client.details]");
		client->local->rpc->rehash_request = json_copy(request); // or json_deep_copy ??
		SetMonitorRehash(client);
		request_rehash(client);
	}

	/* Do NOT send the JSON Response here, it is done by rpc_server_rehash_log()
	 * after the REHASH completed (which may take several seconds).
	 */
}

int rpc_server_rehash_log(int failure, json_t *rehash_log)
{
	Client *client;

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (IsRPC(client) && IsMonitorRehash(client) && client->local->rpc && client->local->rpc->rehash_request)
		{
			rpc_response(client, client->local->rpc->rehash_request, rehash_log);
			json_decref(client->local->rpc->rehash_request);
			client->local->rpc->rehash_request = NULL;
		}
	}
	return 0;
}
