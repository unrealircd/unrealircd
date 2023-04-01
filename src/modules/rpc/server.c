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
RPC_CALL_FUNC(rpc_server_connect);
RPC_CALL_FUNC(rpc_server_disconnect);
RPC_CALL_FUNC(rpc_server_module_list);

int rpc_server_rehash_log(int failure, json_t *rehash_log);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "server.list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_server_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "server.get";
	r.loglevel = ULOG_DEBUG;
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
	memset(&r, 0, sizeof(r));
	r.method = "server.connect";
	r.call = rpc_server_connect;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "server.disconnect";
	r.call = rpc_server_disconnect;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "server.module_list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_server_module_list;
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
		json_expand_client(item, NULL, acptr, 99);
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
	json_expand_client(result, "server", acptr, 99);
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
		/* Forward to remote */
		if (rrpc_supported_simple(acptr, NULL))
		{
			/* Server supports RRPC and will handle the response */
			rpc_send_request_to_remote(client, acptr, request);
		} else {
			/* Server does not support RRPC, so we can only do best effort: */
			sendto_one(acptr, NULL, ":%s REHASH %s", me.id, acptr->name);
			result = json_boolean(1);
			rpc_response(client, request, result);
			json_decref(result);
		}
		return;
	}

	if (client->rpc->rehash_request)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_REQUEST, "A rehash initiated by you is already in progress");
		return;
	}

	/* It's for me... */

	SetMonitorRehash(client);
	SetAsyncRPC(client);
	client->rpc->rehash_request = json_copy(request); // or json_deep_copy ??

	if (!loop.rehashing)
	{
		unreal_log(ULOG_INFO, "config", "CONFIG_RELOAD", client, "Rehashing server configuration file [by: $client.details]");
		request_rehash(client);
	} /* else.. we simply joined the rehash request so we are notified as well ;) */

	/* Do NOT send the JSON Response here, it is done by rpc_server_rehash_log()
	 * after the REHASH completed (which may take several seconds).
	 */
}

int rpc_server_rehash_log(int failure, json_t *rehash_log)
{
	Client *client, *next;

	list_for_each_entry(client, &unknown_list, lclient_node)
	{
		if (IsRPC(client) && IsMonitorRehash(client) && client->rpc && client->rpc->rehash_request)
		{
			rpc_response(client, client->rpc->rehash_request, rehash_log);
			json_decref(client->rpc->rehash_request);
			client->rpc->rehash_request = NULL;
		}
	}
	list_for_each_entry_safe(client, next, &rpc_remote_list, client_node)
	{
		if (IsMonitorRehash(client) && client->rpc && client->rpc->rehash_request)
		{
			rpc_response(client, client->rpc->rehash_request, rehash_log);
			json_decref(client->rpc->rehash_request);
			client->rpc->rehash_request = NULL;
			free_client(client);
		}
	}
	return 0;
}

RPC_CALL_FUNC(rpc_server_connect)
{
	json_t *result, *list, *item;
	const char *server, *link_name;
	Client *acptr;
	ConfigItem_link *link;
	const char *err;

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
	REQUIRE_PARAM_STRING("link", link_name);

	if (acptr != &me)
	{
		/* Not supported atm */
		result = json_boolean(0);
		rpc_response(client, request, result);
		json_decref(result);
		return;
	}

	if (find_server_quick(link_name))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Server is already linked");
		return;
	}

	link = find_link(link_name);
	if (!link)
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Server with that name does not exist in any link block");
		return;
	}
	if (!link->outgoing.hostname && !link->outgoing.file)
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Server with that name exists but is not configured as an OUTGOING server.");
		return;
	}

	if ((err = check_deny_link(link, 0)))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_DENIED, "Server linking is denied via a deny link { } block: %s", err);
		return;
	}

	unreal_log(ULOG_INFO, "link", "LINK_REQUEST", client,
		   "CONNECT: Link to $link_block requested by $client",
		   log_data_link_block(link));

	connect_server(link, client, NULL);
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_server_disconnect)
{
	json_t *result, *list, *item;
	const char *server, *link_name, *reason;
	Client *acptr, *target;
	MessageTag *mtags = NULL;

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
	REQUIRE_PARAM_STRING("link", link_name);
	REQUIRE_PARAM_STRING("reason", reason);

	if (acptr != &me)
	{
		/* Not supported atm */
		result = json_boolean(0);
		rpc_response(client, request, result);
		json_decref(result);
		return;
	}

	target = find_server_quick(link_name);
	if (!target)
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Server link not found");
		return;
	}

	if (target == &me)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "We cannot disconnect ourselves");
		return;
	}

	unreal_log(ULOG_INFO, "link", "SQUIT", client,
	           "SQUIT: Forced server disconnect of $target by $client ($reason)",
	           log_data_client("target", target),
	           log_data_string("reason", reason));

	/* The actual SQUIT: */
	new_message(client, NULL, &mtags);
	exit_client_ex(target, NULL, mtags, reason);
	free_message_tags(mtags);

	/* Return true */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

void json_expand_module(json_t *j, const char *key, Module *m, int detail)
{
	char buf[BUFSIZE+1];
	json_t *child;

	if (key)
	{
		child = json_object();
		json_object_set_new(j, key, child);
	} else {
		child = j;
	}

	json_object_set_new(child, "name", json_string_unreal(m->header->name));
	json_object_set_new(child, "version", json_string_unreal(m->header->version));
	json_object_set_new(child, "author", json_string_unreal(m->header->author));
	json_object_set_new(child, "description", json_string_unreal(m->header->description));
	json_object_set_new(child, "third_party", json_boolean(m->options & MOD_OPT_OFFICIAL ? 0 : 1));
	json_object_set_new(child, "permanent", json_boolean(m->options & MOD_OPT_PERM ? 1 : 0));
	json_object_set_new(child, "permanent_but_reloadable", json_boolean(m->options & MOD_OPT_PERM_RELOADABLE ? 1 : 0));
}

RPC_CALL_FUNC(rpc_server_module_list)
{
	json_t *result, *list, *item;
	const char *server;
	Client *acptr;
	Module *m;

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
		/* Forward to remote */
		rpc_send_request_to_remote(client, acptr, request);
		return;
	}

	/* Return true */
	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (m = Modules; m; m = m->next)
	{
		item = json_object();
		json_expand_module(item, NULL, m, 1);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}
