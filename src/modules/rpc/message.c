/* message.* RPC calls
 * (C) Copyright 2025 Valware and the UnrealIRCd Team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/message",
	"1.0.0",
	"message.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

void rpc_send_privmsgnotice(json_t *request, json_t *params, Client *client, int is_notice)
{
	json_t *result;
	Client *acptr;
	const char *nick, *message;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("message", message);
	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	sendto_one(acptr, NULL, ":%s %s %s :%s", me.name, is_notice ? "NOTICE" : "PRIVMSG", acptr->name, message);
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
	return;
}

/* Forward declarations */
RPC_CALL_FUNC(rpc_message_privmsg);
RPC_CALL_FUNC(rpc_message_notice);
RPC_CALL_FUNC(rpc_message_numeric);
RPC_CALL_FUNC(rpc_message_standardreply);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "message.privmsg";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_message_privmsg;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/message] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "message.notice";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_message_notice;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/message] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "message.numeric";
	r.call = rpc_message_numeric;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/message] Could not register RPC handler");
		return MOD_FAILED;
	}

	memset(&r, 0, sizeof(r));
	r.method = "message.standardreply";
	r.call = rpc_message_standardreply;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/message] Could not register RPC handler");
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

RPC_CALL_FUNC(rpc_message_privmsg)
{
	rpc_send_privmsgnotice(request, params, client, 0);
}

RPC_CALL_FUNC(rpc_message_notice)
{
	rpc_send_privmsgnotice(request, params, client, 1);
}

RPC_CALL_FUNC(rpc_message_numeric)
{
	json_t *result;
	Client *acptr;
	const char *nick, *message;
	int numeric;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_INTEGER("numeric", numeric);
	REQUIRE_PARAM_STRING("message", message);
	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}
	if (numeric < 1 || numeric > 999)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Numeric out of range");
		return;
	}

	sendnumericfmt(acptr, numeric, "%s", message);
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

/* We don't ever include the "command" field because this
 * will never be in response to a user-initiated command,
 * so we just use "*" as the command.
 * https://ircv3.net/specs/extensions/standard-replies
*/
RPC_CALL_FUNC(rpc_message_standardreply)
{
	json_t *result;
	Client *acptr;
	const char *nick, *type, *code, *context, *description;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("type", type);
	REQUIRE_PARAM_STRING("code", code);
	OPTIONAL_PARAM_STRING("context", context);
	REQUIRE_PARAM_STRING("description", description);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if (strcasecmp(type, "FAIL") && strcasecmp(type, "WARN") && strcasecmp(type, "NOTE"))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Invalid type, must be one of FAIL, WARN, NOTE");
		return;
	}

	if (context)
		sendto_one(acptr, NULL, ":%s %s * %s %s :%s", me.name, type, code, context, description);
	
	else
		sendto_one(acptr, NULL, ":%s %s * %s :%s", me.name, type, code, description);

	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
