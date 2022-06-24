/* server_ban.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/server_ban",
	"1.0.0",
	"server_ban.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_server_ban_list);
RPC_CALL_FUNC(rpc_server_ban_get);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "server_ban.list";
	r.call = rpc_server_ban_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban.get";
	r.call = rpc_server_ban_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban] Could not register RPC handler");
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

// TODO: right now returns everything for everyone,
// give the option to return a list of names only or
// certain options (hence the placeholder #define's above)
RPC_CALL_FUNC(rpc_server_ban_list)
{
	json_t *result, *list, *item;
	int index, index2;
	TKL *tkl;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				if (TKLIsServerBan(tkl))
				{
					item = json_object();
					json_expand_tkl(item, NULL, tkl, 1);
					json_array_append_new(list, item);
				}
			}
		}
	}
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			if (TKLIsServerBan(tkl))
			{
				item = json_object();
				json_expand_tkl(item, NULL, tkl, 1);
				json_array_append_new(list, item);
			}
		}
	}

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_server_ban_get)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	const char *error;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	char tkl_type_char;
	int tkl_type_int;

	name = json_object_get_string(params, "name");
	if (!name)
	{
		rpc_error(client, NULL, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}

	type_name = json_object_get_string(params, "type");
	if (!type_name)
	{
		rpc_error(client, NULL, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'type'");
		return;
	}

	tkl_type_char = tkl_configtypetochar(type_name);
	if (!tkl_type_char)
	{
		rpc_error_fmt(client, NULL, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s'", type_name);
		return;
	}
	tkl_type_int = tkl_chartotype(tkl_type_char);

	if (!server_ban_parse_mask(client, 0, tkl_type_int, name, &usermask, &hostmask, &soft, &error))
	{
		rpc_error_fmt(client, NULL, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return;
	}

	if (!(tkl = find_tkl_serverban(tkl_type_int, usermask, hostmask, soft)))
	{
		rpc_error(client, NULL, JSON_RPC_ERROR_NOT_FOUND, "Ban not found");
		return;
	}

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);
	rpc_response(client, request, result);
	json_decref(result);
}
