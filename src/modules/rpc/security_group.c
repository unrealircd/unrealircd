/* security_group.* RPC calls
 * (C) Copyright 2025-.. Valware and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
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

/** Helper: Expand a mask list to JSON array */
static void json_expand_mask_list(json_t *parent, const char *key, ConfigItem_mask *mask)
{
	json_t *arr;
	ConfigItem_mask *m;

	if (!mask)
		return;

	arr = json_array();
	json_object_set_new(parent, key, arr);

	for (m = mask; m; m = m->next)
		json_array_append_new(arr, json_string_unreal(m->mask));
}

/** Helper: Expand a name list to JSON array */
static void json_expand_name_list_sg(json_t *parent, const char *key, NameList *list)
{
	json_t *arr;
	NameList *n;

	if (!list)
		return;

	arr = json_array();
	json_object_set_new(parent, key, arr);

	for (n = list; n; n = n->next)
		json_array_append_new(arr, json_string_unreal(n->name));
}

/** Helper: Expand a name-value list to JSON object */
static void json_expand_nvplist(json_t *parent, const char *key, NameValuePrioList *list)
{
	json_t *obj;
	NameValuePrioList *n;

	if (!list)
		return;

	obj = json_object();
	json_object_set_new(parent, key, obj);

	for (n = list; n; n = n->next)
		json_object_set_new(obj, n->name, json_string_unreal(n->value));
}

/** Helper: Expand security group details to JSON */
static void json_expand_security_group(json_t *j, const char *key, SecurityGroup *s, int detail)
{
	json_t *child;

	if (key)
	{
		child = json_object();
		json_object_set_new(j, key, child);
	}
	else
	{
		child = j;
	}

	json_object_set_new(child, "name", json_string_unreal(s->name));
	json_object_set_new(child, "priority", json_integer(s->priority));

	if (detail == 0)
		return;

	/* Inclusion criteria */
	if (s->identified)
		json_object_set_new(child, "identified", json_boolean(1));
	if (s->webirc)
		json_object_set_new(child, "webirc", json_boolean(1));
	if (s->websocket)
		json_object_set_new(child, "websocket", json_boolean(1));
	if (s->tls)
		json_object_set_new(child, "tls", json_boolean(1));
	if (s->reputation_score != 0)
		json_object_set_new(child, "reputation_score", json_integer(s->reputation_score));
	if (s->connect_time != 0)
		json_object_set_new(child, "connect_time", json_integer(s->connect_time));

	/* Mask lists */
	json_expand_mask_list(child, "mask", s->mask);
	json_expand_mask_list(child, "exclude_mask", s->exclude_mask);

	/* Name lists */
	json_expand_name_list_sg(child, "ip", s->ip);
	json_expand_name_list_sg(child, "exclude_ip", s->exclude_ip);
	json_expand_name_list_sg(child, "security_group", s->security_group);
	json_expand_name_list_sg(child, "exclude_security_group", s->exclude_security_group);
	json_expand_name_list_sg(child, "server_port", s->server_port);
	json_expand_name_list_sg(child, "exclude_server_port", s->exclude_server_port);

	/* Extended criteria (account, realname, etc) */
	json_expand_nvplist(child, "extended", s->extended);
	json_expand_nvplist(child, "exclude_extended", s->exclude_extended);

	/* Rules (as strings) */
	if (s->prettyrule)
		json_object_set_new(child, "rule", json_string_unreal(s->prettyrule));
	if (s->exclude_prettyrule)
		json_object_set_new(child, "exclude_rule", json_string_unreal(s->exclude_prettyrule));
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
		json_object_set_new(item, "builtin", json_boolean(1));
		json_array_append_new(list, item);
	}

	for (s = securitygroups; s; s = s->next)
	{
		json_t *item = json_object();
		json_expand_security_group(item, NULL, s, 0);
		if (!strcmp(s->name, "known-users"))
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
	if (!strcmp(s->name, "known-users"))
		json_object_set_new(result, "builtin", json_boolean(1));

	rpc_response(client, request, result);
	json_decref(result);
}
