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
RPC_CALL_FUNC(rpc_server_ban_del);
RPC_CALL_FUNC(rpc_server_ban_add);

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
	r.method = "server_ban.del";
	r.call = rpc_server_ban_del;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban.add";
	r.call = rpc_server_ban_add;
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

// TODO: right now returns everything
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
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}

	type_name = json_object_get_string(params, "type");
	if (!type_name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'type'");
		return;
	}

	tkl_type_char = tkl_configtypetochar(type_name);
	if (!tkl_type_char)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s'", type_name);
		return;
	}
	tkl_type_int = tkl_chartotype(tkl_type_char);

	if (!server_ban_parse_mask(client, 0, tkl_type_int, name, &usermask, &hostmask, &soft, &error))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return;
	}

	if (!(tkl = find_tkl_serverban(tkl_type_int, usermask, hostmask, soft)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Ban not found");
		return;
	}

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);
	rpc_response(client, request, result);
	json_decref(result);
}

// Wonderful duplicate code atm
RPC_CALL_FUNC(rpc_server_ban_del)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	const char *error;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	char tkl_type_char;
	int tkl_type_int;
	const char *tkllayer[10];
	char tkl_type_str[2];

	name = json_object_get_string(params, "name");
	if (!name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}

	type_name = json_object_get_string(params, "type");
	if (!type_name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'type'");
		return;
	}

	tkl_type_char = tkl_configtypetochar(type_name);
	if (!tkl_type_char)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s'", type_name);
		return;
	}
	tkl_type_int = tkl_chartotype(tkl_type_char);
	tkl_type_str[0] = tkl_type_char;
	tkl_type_str[1] = '\0';

	if (!server_ban_parse_mask(client, 0, tkl_type_int, name, &usermask, &hostmask, &soft, &error))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return;
	}

	if (!(tkl = find_tkl_serverban(tkl_type_int, usermask, hostmask, soft)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Ban not found");
		return;
	}

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);

	tkllayer[1] = "-";
	tkllayer[2] = tkl_type_str;
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = client->name;
	tkllayer[6] = NULL;
	cmd_tkl(&me, NULL, 6, tkllayer);

	if (!find_tkl_serverban(tkl_type_int, usermask, hostmask, soft))
	{
		rpc_response(client, request, result);
	} else {
		/* Actually this may not be an internal error, it could be an
		 * incorrect request, such as asking to remove a config-based ban.
		 */
		rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, "Unable to remove item");
	}
	json_decref(result);
}

// Wonderful duplicate code atm
RPC_CALL_FUNC(rpc_server_ban_add)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	const char *error;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	char tkl_type_char;
	int tkl_type_int;
	char tkl_type_str[2];
	const char *reason;
	const char *str;
	time_t tkl_expire_at;
	time_t tkl_set_at = TStime();

	name = json_object_get_string(params, "name");
	if (!name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}

	type_name = json_object_get_string(params, "type");
	if (!type_name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'type'");
		return;
	}

	tkl_type_char = tkl_configtypetochar(type_name);
	if (!tkl_type_char)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s'", type_name);
		return;
	}
	tkl_type_int = tkl_chartotype(tkl_type_char);
	tkl_type_str[0] = tkl_type_char;
	tkl_type_str[1] = '\0';

	reason = json_object_get_string(params, "reason");
	if (!reason)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'reason'");
		return;
	}

	/* Duration / expiry time */
	if ((str = json_object_get_string(params, "duration_string")))
	{
		tkl_expire_at = config_checkval(str, CFG_TIME);
	} else
	if ((str = json_object_get_string(params, "expire_at")))
	{
		tkl_expire_at = server_time_to_unix_time(str);
	} else
	{
		/* Never expire */
		tkl_expire_at = 0;
	}

	if ((tkl_expire_at != 0) && (tkl_expire_at < TStime()))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: the specified expiry time is before current time (before now)");
		return;
	}

	if (!server_ban_parse_mask(client, 0, tkl_type_int, name, &usermask, &hostmask, &soft, &error))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return;
	}

	if (find_tkl_serverban(tkl_type_int, usermask, hostmask, soft))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "A ban with that mask already exists");
		return;
	}

	tkl = tkl_add_serverban(tkl_type_int, usermask, hostmask, reason,
	                        client->name, tkl_expire_at, tkl_set_at,
	                        soft, 0);

	if (!tkl)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, "Unable to add item");
		return;
	}

	tkl_added(client, tkl);

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);
	rpc_response(client, request, result);
	json_decref(result);
}
