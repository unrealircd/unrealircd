/* security_group.* RPC calls
 * (C) Copyright 2025-.. Valware and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
    "rpc/security_group",
    "1.0.0",
    "security_group.* RPC calls",
    "UnrealIRCd Team",
    "unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_security_group_list);
RPC_CALL_FUNC(rpc_security_group_get);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "security_group.list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_security_group_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/security_group] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "security_group.get";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_security_group_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/security_group] Could not register RPC handler");
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

RPC_CALL_FUNC(rpc_security_group_list)
{
	json_t *result, *list;
	SecurityGroup *s;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	/* Add the magic 'unknown-users' and 'known-users' groups first */
	{
		json_t *item = json_object();
		json_object_set_new(item, "name", json_string_unreal("unknown-users"));
		json_object_set_new(item, "priority", json_integer(0));
		json_object_set_new(item, "public", json_boolean(1));
		json_object_set_new(item, "builtin", json_boolean(1));
		json_array_append_new(list, item);
	}

	for (s = securitygroups; s; s = s->next)
	{
		json_t *item = json_object();
		json_expand_security_group(item, NULL, s, 0);
		if (s->builtin)
			json_object_set_new(item, "builtin", json_boolean(1));
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_security_group_get)
{
	json_t *result;
	SecurityGroup *s;
	const char *name;

	REQUIRE_PARAM_STRING("name", name);

	/* Handle the magic 'unknown-users' case */
	if (!strcmp(name, "unknown-users"))
	{
		result = json_object();
		json_object_set_new(result, "name", json_string_unreal("unknown-users"));
		json_object_set_new(result, "priority", json_integer(0));
		json_object_set_new(result, "public", json_boolean(1));
		json_object_set_new(result, "builtin", json_boolean(1));
		json_object_set_new(result, "description", json_string_unreal("Users not matching the 'known-users' security group"));
		rpc_response(client, request, result);
		json_decref(result);
		return;
	}

	s = find_security_group(name);
	if (!s)
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Security group not found");
		return;
	}

	result = json_object();
	json_expand_security_group(result, NULL, s, 1);
	if (s->builtin)
		json_object_set_new(result, "builtin", json_boolean(1));

	rpc_response(client, request, result);
	json_decref(result);
}
