/* user.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/user",
	"1.0.0",
	"user.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_user_list(Client *client, json_t *request, json_t *params);
void rpc_user_get(Client *client, json_t *request, json_t *params);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "user.list";
	r.call = rpc_user_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.get";
	r.call = rpc_user_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}

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

#define RPC_USER_LIST_EXPAND_NONE	0
#define RPC_USER_LIST_EXPAND_SELECT	1
#define RPC_USER_LIST_EXPAND_ALL	2

void rpc_user_list(Client *client, json_t *request, json_t *params)
{
	json_t *result, *list, *item;
	Client *acptr;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsUser(acptr))
			continue;

		item = json_object();
		json_expand_client(item, NULL, acptr, 1);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}

void rpc_user_get(Client *client, json_t *request, json_t *params)
{
	json_t *result, *list, *item;
	const char *nick;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, NULL, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		// FIXME: wrong error!
		// consider re-using IRC numerics? the positive ones, eg ERR_NOSUCHNICK
		rpc_error(client, NULL, JSON_RPC_ERROR_INVALID_REQUEST, "Nickname not found");
		return;
	}

	result = json_object();
	json_expand_client(result, "client", acptr, 1);
	rpc_response(client, request, result);
	json_decref(result);
}
