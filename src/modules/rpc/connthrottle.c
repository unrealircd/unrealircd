/* connthrottle.* RPC calls
 * (C) Copyright 2025-.. Valware and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/connthrottle",
	"1.0.0",
	"connthrottle.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_connthrottle_status);
RPC_CALL_FUNC(rpc_connthrottle_set);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "connthrottle.status";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_connthrottle_status;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/connthrottle] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "connthrottle.set";
	r.call = rpc_connthrottle_set;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/connthrottle] Could not register RPC handler");
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

/** Check if connthrottle module is loaded */
static int connthrottle_module_loaded(void)
{
	/* Check if the THROTTLE command exists */
	if (find_command_simple("THROTTLE"))
		return 1;
	return 0;
}

/** Get connthrottle status via callback */
static ConnthrottleStatus *get_connthrottle_status(void)
{
	if (RCallbacks[CALLBACKTYPE_CONNTHROTTLE_STATUS] == NULL)
		return NULL;
	return (ConnthrottleStatus *)RCallbacks[CALLBACKTYPE_CONNTHROTTLE_STATUS]->func.pvoidfunc();
}

RPC_CALL_FUNC(rpc_connthrottle_status)
{
	json_t *result, *counters, *config, *stats;
	ConnthrottleStatus *st;

	if (!connthrottle_module_loaded())
	{
		rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, 
		          "The connthrottle module is not loaded");
		return;
	}

	st = get_connthrottle_status();
	if (!st)
	{
		/* Fallback for older connthrottle module without callback */
		result = json_object();
		json_object_set_new(result, "module_loaded", json_boolean(1));
		json_object_set_new(result, "server_boot_time", json_timestamp(me.local->creationtime));
		json_object_set_new(result, "server_uptime_seconds", json_integer(TStime() - me.local->creationtime));
		json_object_set_new(result, "note", json_string_unreal(
			"Connthrottle module does not export status callback. Update the connthrottle module."
		));
		rpc_response(client, request, result);
		json_decref(result);
		return;
	}

	result = json_object();

	/* Basic state */
	json_object_set_new(result, "enabled", json_boolean(st->enabled));
	json_object_set_new(result, "throttling_this_minute", json_boolean(st->throttling_this_minute));
	json_object_set_new(result, "throttling_previous_minute", json_boolean(st->throttling_previous_minute));

	/* Determine overall state */
	if (!st->enabled)
		json_object_set_new(result, "state", json_string_unreal("disabled_by_oper"));
	else if (st->start_delay_remaining > 0)
		json_object_set_new(result, "state", json_string_unreal("start_delay"));
	else if (st->reputation_gathering)
		json_object_set_new(result, "state", json_string_unreal("reputation_gathering"));
	else if (st->throttling_this_minute)
		json_object_set_new(result, "state", json_string_unreal("throttling"));
	else
		json_object_set_new(result, "state", json_string_unreal("active"));

	json_object_set_new(result, "start_delay_remaining", json_integer(st->start_delay_remaining));
	json_object_set_new(result, "reputation_gathering", json_boolean(st->reputation_gathering));

	/* Current counters */
	counters = json_object();
	json_object_set_new(counters, "local_count", json_integer(st->local_count));
	json_object_set_new(counters, "global_count", json_integer(st->global_count));
	if (st->local_period_start > 0)
		json_object_set_new(counters, "local_period_start", json_timestamp(st->local_period_start));
	if (st->global_period_start > 0)
		json_object_set_new(counters, "global_period_start", json_timestamp(st->global_period_start));
	json_object_set_new(result, "counters", counters);

	/* Statistics for last minute */
	stats = json_object();
	json_object_set_new(stats, "rejected_clients", json_integer(st->rejected_clients));
	json_object_set_new(stats, "allowed_except", json_integer(st->allowed_except));
	json_object_set_new(stats, "allowed_unknown_users", json_integer(st->allowed_unknown_users));
	json_object_set_new(result, "stats_last_minute", stats);

	/* Configuration */
	config = json_object();
	json_object_set_new(config, "local_throttle_count", json_integer(st->cfg_local_count));
	json_object_set_new(config, "local_throttle_period", json_integer(st->cfg_local_period));
	json_object_set_new(config, "global_throttle_count", json_integer(st->cfg_global_count));
	json_object_set_new(config, "global_throttle_period", json_integer(st->cfg_global_period));
	json_object_set_new(config, "start_delay", json_integer(st->cfg_start_delay));
	json_object_set_new(config, "except_reputation_score", json_integer(st->cfg_except_reputation));
	json_object_set_new(config, "except_sasl_bypass", json_boolean(st->cfg_except_identified));
	json_object_set_new(config, "except_webirc_bypass", json_boolean(st->cfg_except_webirc));
	json_object_set_new(result, "config", config);

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_connthrottle_set)
{
	json_t *result;
	const char *action;
	const char *parv[3];

	REQUIRE_PARAM_STRING("action", action);

	if (!connthrottle_module_loaded())
	{
		rpc_error(client, request, JSON_RPC_ERROR_INTERNAL_ERROR, 
		          "The connthrottle module is not loaded");
		return;
	}

	/* Validate action */
	if (strcasecmp(action, "on") && strcasecmp(action, "off") && strcasecmp(action, "reset"))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS,
		          "Invalid action. Must be 'on', 'off', or 'reset'");
		return;
	}

	/* Find the THROTTLE command and execute it
	 * We'll use do_cmd to run the command as the server
	 */
	parv[0] = NULL;
	parv[1] = action;
	parv[2] = NULL;

	/* Execute the THROTTLE command as the server */
	do_cmd(&me, NULL, "THROTTLE", 2, parv);

	result = json_object();
	json_object_set_new(result, "success", json_boolean(1));
	json_object_set_new(result, "action", json_string_unreal(action));

	if (!strcasecmp(action, "on"))
		json_object_set_new(result, "message", json_string_unreal("Connection throttling enabled"));
	else if (!strcasecmp(action, "off"))
		json_object_set_new(result, "message", json_string_unreal("Connection throttling disabled"));
	else if (!strcasecmp(action, "reset"))
		json_object_set_new(result, "message", json_string_unreal("Connection throttle counters reset"));

	rpc_response(client, request, result);
	json_decref(result);
}
