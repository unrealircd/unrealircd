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
RPC_CALL_FUNC(rpc_security_group_match);

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

	memset(&r, 0, sizeof(r));
	r.method = "security_group.match";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_security_group_match;
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

/** Helper structure for virtual user matching */
typedef struct VirtualUser {
	const char *ip;
	const char *hostname;
	const char *account;
	int reputation_score;
	long connect_time;
	int identified;
	int tls;
	int webirc;
	int websocket;
	int server_port;
} VirtualUser;

/** Check if a virtual user matches a security group's IP list */
static int virtual_match_iplist(VirtualUser *vu, NameList *iplist)
{
	NameList *n;

	if (!vu->ip || !iplist)
		return 0;

	for (n = iplist; n; n = n->next)
	{
		if (match_simple(n->name, vu->ip))
			return 1;
	}
	return 0;
}

/** Check if a virtual user matches a security group's mask list */
static int virtual_match_mask(VirtualUser *vu, ConfigItem_mask *mask)
{
	ConfigItem_mask *m;
	int retval = 1;
	char fullmask[512];

	if (!mask)
		return 0;

	/* Build a pseudo n!u@h mask from what we have */
	snprintf(fullmask, sizeof(fullmask), "*!*@%s", 
	         vu->hostname ? vu->hostname : (vu->ip ? vu->ip : "*"));

	/* First check normal matches (without ! prefix) */
	for (m = mask; m; m = m->next)
	{
		if (m->mask[0] != '!')
		{
			retval = 0; /* no implicit * */
			if (match_simple(m->mask, fullmask))
			{
				retval = 1;
				break;
			}
		}
	}

	if (retval)
	{
		/* We matched. Check for exceptions (with ! prefix) */
		for (m = mask; m; m = m->next)
		{
			if ((m->mask[0] == '!') && match_simple(m->mask + 1, fullmask))
				return 0;
		}
	}

	return retval;
}

/** Check if a virtual user matches a security group */
static int virtual_user_matches_security_group(VirtualUser *vu, SecurityGroup *s)
{
	/* Allow NULL securitygroup */
	if (!s)
		return 0;

	/* Process EXCLUSION criteria first... */
	if (s->exclude_identified && vu->identified)
		return 0;
	if (s->exclude_webirc && vu->webirc)
		return 0;
	if (s->exclude_websocket && vu->websocket)
		return 0;
	if ((s->exclude_reputation_score > 0) && (vu->reputation_score >= s->exclude_reputation_score))
		return 0;
	if ((s->exclude_reputation_score < 0) && (vu->reputation_score < 0 - s->exclude_reputation_score))
		return 0;
	if (s->exclude_connect_time != 0)
	{
		if ((s->exclude_connect_time > 0) && (vu->connect_time >= s->exclude_connect_time))
			return 0;
		if ((s->exclude_connect_time < 0) && (vu->connect_time < 0 - s->exclude_connect_time))
			return 0;
	}
	if (s->exclude_tls && vu->tls)
		return 0;
	if (s->exclude_mask && virtual_match_mask(vu, s->exclude_mask))
		return 0;
	if (s->exclude_ip && virtual_match_iplist(vu, s->exclude_ip))
		return 0;
	if (s->exclude_server_port && vu->server_port && find_name_list_integer(s->exclude_server_port, vu->server_port))
		return 0;
	/* Note: exclude_rule, exclude_extended, exclude_security_group, exclude_destination
	 * are harder to evaluate without a real client - skipping for virtual users */

	/* Then process INCLUSION criteria... */
	if (s->identified && vu->identified)
		return 1;
	if (s->webirc && vu->webirc)
		return 1;
	if (s->websocket && vu->websocket)
		return 1;
	if ((s->reputation_score > 0) && (vu->reputation_score >= s->reputation_score))
		return 1;
	if ((s->reputation_score < 0) && (vu->reputation_score < 0 - s->reputation_score))
		return 1;
	if (s->connect_time != 0)
	{
		if ((s->connect_time > 0) && (vu->connect_time >= s->connect_time))
			return 1;
		if ((s->connect_time < 0) && (vu->connect_time < 0 - s->connect_time))
			return 1;
	}
	if (s->tls && vu->tls)
		return 1;
	if (s->mask && virtual_match_mask(vu, s->mask))
		return 1;
	if (s->ip && virtual_match_iplist(vu, s->ip))
		return 1;
	if (s->server_port && vu->server_port && find_name_list_integer(s->server_port, vu->server_port))
		return 1;
	/* Note: rule, extended, security_group, destination harder without real client */

	return 0;
}

RPC_CALL_FUNC(rpc_security_group_match)
{
	json_t *result, *groups;
	const char *security_group_name;
	SecurityGroup *s;
	VirtualUser vu;
	json_t *j;

	/* Initialize virtual user with defaults */
	memset(&vu, 0, sizeof(vu));

	/* Extract optional parameters */
	vu.ip = json_object_get_string(params, "ip");
	vu.hostname = json_object_get_string(params, "hostname");
	vu.account = json_object_get_string(params, "account");

	j = json_object_get(params, "reputation_score");
	if (j && json_is_integer(j))
		vu.reputation_score = json_integer_value(j);

	j = json_object_get(params, "connect_time");
	if (j && json_is_integer(j))
		vu.connect_time = json_integer_value(j);

	j = json_object_get(params, "server_port");
	if (j && json_is_integer(j))
		vu.server_port = json_integer_value(j);

	j = json_object_get(params, "identified");
	if (j)
		vu.identified = json_is_true(j) ? 1 : 0;
	else if (vu.account && *vu.account)
		vu.identified = 1; /* If account is set, assume identified */

	j = json_object_get(params, "tls");
	if (j)
		vu.tls = json_is_true(j) ? 1 : 0;

	j = json_object_get(params, "webirc");
	if (j)
		vu.webirc = json_is_true(j) ? 1 : 0;

	j = json_object_get(params, "websocket");
	if (j)
		vu.websocket = json_is_true(j) ? 1 : 0;

	result = json_object();

	/* Echo back the input parameters for clarity */
	{
		json_t *input = json_object();
		json_object_set_new(result, "input", input);
		if (vu.ip)
			json_object_set_new(input, "ip", json_string_unreal(vu.ip));
		if (vu.hostname)
			json_object_set_new(input, "hostname", json_string_unreal(vu.hostname));
		if (vu.account)
			json_object_set_new(input, "account", json_string_unreal(vu.account));
		json_object_set_new(input, "reputation_score", json_integer(vu.reputation_score));
		json_object_set_new(input, "connect_time", json_integer(vu.connect_time));
		json_object_set_new(input, "server_port", json_integer(vu.server_port));
		json_object_set_new(input, "identified", json_boolean(vu.identified));
		json_object_set_new(input, "tls", json_boolean(vu.tls));
		json_object_set_new(input, "webirc", json_boolean(vu.webirc));
		json_object_set_new(input, "websocket", json_boolean(vu.websocket));
	}

	/* If a specific security group was requested, just check that one */
	security_group_name = json_object_get_string(params, "security_group");
	if (security_group_name)
	{
		int matches;

		/* Handle the magic 'unknown-users' case */
		if (!strcmp(security_group_name, "unknown-users"))
		{
			s = find_security_group("known-users");
			matches = s ? !virtual_user_matches_security_group(&vu, s) : 0;
		}
		else
		{
			s = find_security_group(security_group_name);
			if (!s)
			{
				rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Security group not found");
				json_decref(result);
				return;
			}
			matches = virtual_user_matches_security_group(&vu, s);
		}

		json_object_set_new(result, "security_group", json_string_unreal(security_group_name));
		json_object_set_new(result, "matches", json_boolean(matches));
	}
	else
	{
		/* Return all matching security groups */
		groups = json_array();
		json_object_set_new(result, "matching_groups", groups);

		/* Check known-users / unknown-users */
		s = find_security_group("known-users");
		if (s && virtual_user_matches_security_group(&vu, s))
			json_array_append_new(groups, json_string_unreal("known-users"));
		else
			json_array_append_new(groups, json_string_unreal("unknown-users"));

		/* Check all other security groups */
		for (s = securitygroups; s; s = s->next)
		{
			if (strcmp(s->name, "known-users") && virtual_user_matches_security_group(&vu, s))
				json_array_append_new(groups, json_string_unreal(s->name));
		}
	}

	rpc_response(client, request, result);
	json_decref(result);
}
