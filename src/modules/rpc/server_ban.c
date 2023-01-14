/* server_ban.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/server_ban",
	"1.0.3",
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
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_server_ban_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban.get";
	r.loglevel = ULOG_DEBUG;
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

/** Shared code for selecting a server ban, for .add/.del/.get */
int server_ban_select_criteria(Client *client, json_t *request, json_t *params,
                               const char **name,
                               const char **type_name,
                               char *tkl_type_char,
                               int *tkl_type_int,
                               char **usermask,
                               char **hostmask,
                               int *soft)
{
	const char *error;

	*name = json_object_get_string(params, "name");
	if (!*name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return 0;
	}

	*type_name = json_object_get_string(params, "type");
	if (!*type_name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'type'");
		return 0;
	}

	*tkl_type_char = tkl_configtypetochar(*type_name);
	if (!*tkl_type_char)
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s'", *type_name);
		return 0;
	}
	*tkl_type_int = tkl_chartotype(*tkl_type_char);
	if (!TKLIsServerBanType(*tkl_type_int))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type: '%s' (type exists but is not valid for in server_ban.*)", *type_name);
		return 0;
	}

	if (!server_ban_parse_mask(client, 0, *tkl_type_int, *name, usermask, hostmask, soft, &error))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return 0;
	}

	return 1;
}

RPC_CALL_FUNC(rpc_server_ban_get)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	char tkl_type_char;
	int tkl_type_int;

	if (!server_ban_select_criteria(client, request, params,
	                                &name, &type_name,
	                                &tkl_type_char, &tkl_type_int,
	                                &usermask, &hostmask, &soft))
	{
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

RPC_CALL_FUNC(rpc_server_ban_del)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	const char *set_by;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	char tkl_type_char;
	int tkl_type_int;
	const char *tkllayer[10];
	char tkl_type_str[2];

	if (!server_ban_select_criteria(client, request, params,
	                                &name, &type_name,
	                                &tkl_type_char, &tkl_type_int,
	                                &usermask, &hostmask, &soft))
	{
		return;
	}

	tkl_type_str[0] = tkl_type_char;
	tkl_type_str[1] = '\0';

	if (!(tkl = find_tkl_serverban(tkl_type_int, usermask, hostmask, soft)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Ban not found");
		return;
	}

	OPTIONAL_PARAM_STRING("set_by", set_by);
	if (!set_by)
		set_by = client->name;

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);

	tkllayer[1] = "-";
	tkllayer[2] = tkl_type_str;
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = set_by;
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

RPC_CALL_FUNC(rpc_server_ban_add)
{
	json_t *result, *list, *item;
	const char *name, *type_name;
	const char *set_by;
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

	if (!server_ban_select_criteria(client, request, params,
	                                &name, &type_name,
	                                &tkl_type_char, &tkl_type_int,
	                                &usermask, &hostmask, &soft))
	{
		return;
	}

	tkl_type_str[0] = tkl_type_char;
	tkl_type_str[1] = '\0';

	REQUIRE_PARAM_STRING("reason", reason);

	/* Duration / expiry time */
	if ((str = json_object_get_string(params, "duration_string")))
	{
		tkl_expire_at = config_checkval(str, CFG_TIME);
		if (tkl_expire_at > 0)
			tkl_expire_at = TStime() + tkl_expire_at;
	} else
	if ((str = json_object_get_string(params, "expire_at")))
	{
		tkl_expire_at = server_time_to_unix_time(str);
	} else
	{
		/* Never expire */
		tkl_expire_at = 0;
	}

	OPTIONAL_PARAM_STRING("set_by", set_by);
	if (!set_by)
		set_by = client->name;

	if ((tkl_expire_at != 0) && (tkl_expire_at < TStime()))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: the specified expiry time is before current time (before now)");
		return;
	}

	if (find_tkl_serverban(tkl_type_int, usermask, hostmask, soft))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "A ban with that mask already exists");
		return;
	}

	tkl = tkl_add_serverban(tkl_type_int, usermask, hostmask, reason,
	                        set_by, tkl_expire_at, tkl_set_at,
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
