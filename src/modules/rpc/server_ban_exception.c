/* server_ban_exception.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/server_ban_exception",
	"1.0.0",
	"server_ban_exception.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_server_ban_exception_list);
RPC_CALL_FUNC(rpc_server_ban_exception_get);
RPC_CALL_FUNC(rpc_server_ban_exception_del);
RPC_CALL_FUNC(rpc_server_ban_exception_add);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "server_ban_exception.list";
	r.call = rpc_server_ban_exception_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban_exception] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban_exception.get";
	r.call = rpc_server_ban_exception_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban_exception] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban_exception.del";
	r.call = rpc_server_ban_exception_del;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban_exception] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = "server_ban_exception.add";
	r.call = rpc_server_ban_exception_add;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/server_ban_exception] Could not register RPC handler");
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

RPC_CALL_FUNC(rpc_server_ban_exception_list)
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
				if (TKLIsBanException(tkl))
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
			if (TKLIsBanException(tkl))
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
int server_ban_exception_select_criteria(Client *client, json_t *request, json_t *params, int add,
                               const char **name,
                               const char **exception_types,
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

	if (add)
	{
		*exception_types = json_object_get_string(params, "exception_types");
		if (!*exception_types)
		{
			rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'exception_types'");
			return 0;
		}
	} else {
		*exception_types = NULL;
	}

	if (!server_ban_exception_parse_mask(client, add, *exception_types, *name, usermask, hostmask, soft, &error))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: %s", error);
		return 0;
	}

	return 1;
}

RPC_CALL_FUNC(rpc_server_ban_exception_get)
{
	json_t *result, *list, *item;
	const char *name, *exception_types;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;

	if (!server_ban_exception_select_criteria(client, request, params, 0,
	                                &name, &exception_types,
	                                &usermask, &hostmask, &soft))
	{
		return;
	}

	if (!(tkl = find_tkl_banexception(TKL_EXCEPTION|TKL_GLOBAL, usermask, hostmask, soft)) &&
	    !(tkl = find_tkl_banexception(TKL_EXCEPTION, usermask, hostmask, soft)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Ban exception not found");
		return;
	}

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_server_ban_exception_del)
{
	json_t *result, *list, *item;
	const char *name, *exception_types;
	const char *error;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	const char *tkllayer[11];

	if (!server_ban_exception_select_criteria(client, request, params, 0,
	                                &name, &exception_types,
	                                &usermask, &hostmask, &soft))
	{
		return;
	}

	if (!(tkl = find_tkl_banexception(TKL_EXCEPTION|TKL_GLOBAL, usermask, hostmask, soft)) &&
	    !(tkl = find_tkl_banexception(TKL_EXCEPTION, usermask, hostmask, soft)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Ban exception not found");
		return;
	}

	result = json_object();
	json_expand_tkl(result, "tkl", tkl, 1);

	tkllayer[1] = "-";
	tkllayer[2] = "E";
	tkllayer[3] = usermask;
	tkllayer[4] = hostmask;
	tkllayer[5] = client->name;
	tkllayer[6] = "0";
	tkllayer[7] = "-";
	tkllayer[8] = "-";
	tkllayer[9] = "-";
	tkllayer[10] = NULL;
	cmd_tkl(&me, NULL, 6, tkllayer);

	if (!find_tkl_banexception(TKL_EXCEPTION|TKL_GLOBAL, usermask, hostmask, soft) &&
	    !find_tkl_banexception(TKL_EXCEPTION, usermask, hostmask, soft))
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

RPC_CALL_FUNC(rpc_server_ban_exception_add)
{
	json_t *result, *list, *item;
	const char *name, *exception_types;
	const char *error;
	char *usermask, *hostmask;
	int soft;
	TKL *tkl;
	const char *reason;
	const char *str;
	time_t tkl_expire_at;
	time_t tkl_set_at = TStime();

	if (!server_ban_exception_select_criteria(client, request, params, 1,
	                                &name, &exception_types,
	                                &usermask, &hostmask, &soft))
	{
		return;
	}

	// FIXME: where is set_by? is this missing in others as well? :p -> OPTIONAL!

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

	if ((tkl_expire_at != 0) && (tkl_expire_at < TStime()))
	{
		rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Error: the specified expiry time is before current time (before now)");
		return;
	}

	if (find_tkl_banexception(TKL_EXCEPTION|TKL_GLOBAL, usermask, hostmask, soft) ||
	    find_tkl_banexception(TKL_EXCEPTION, usermask, hostmask, soft))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "A ban exception with that mask already exists");
		return;
	}

	tkl = tkl_add_banexception(TKL_EXCEPTION|TKL_GLOBAL, usermask, hostmask,
	                           NULL, reason,
	                           client->name, tkl_expire_at, tkl_set_at,
	                           soft, exception_types, 0);

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
