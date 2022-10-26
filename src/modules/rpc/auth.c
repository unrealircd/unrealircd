/* auth.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/auth",
	"1.0.0",
	"Auth RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

#define RPC_CMD_LOGIN "auth.login"
#define RPC_CMD_LOGOUT "auth.logout"

/* Forward declarations */
RPC_CALL_FUNC(rpc_auth_log_in);
RPC_CALL_FUNC(rpc_auth_log_out);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));

	r.method = RPC_CMD_LOGIN;
	r.call = rpc_auth_log_in;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/auth.login] Could not register RPC handler");
		return MOD_FAILED;
	}
	r.method = RPC_CMD_LOGOUT;
	r.call = rpc_auth_log_out;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/auth.logout] Could not register RPC handler");
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

/** RPC Login
 * @param name Nick or UID of the user to lookup
 * @param account Account which the user should be logged into 
 * 
 * If successful returns a json client object with the updated account name as a result
 */
RPC_CALL_FUNC(rpc_auth_log_in)
{
	json_t *result;
	const char *name, *account;
	Client *target;

	name = json_object_get_string(params, "name");
	if (!name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}
	account = json_object_get_string(params, "account");
	if (BadPtr(account) || !strcasecmp(account,"0"))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'account'");
		return;
	}
	if (!(target = find_user(name, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "User not found");
		return;
	}
	if (!MyUser(target))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "User online but not on our server");
		return;
	}
	strlcpy(target->user->account, account, sizeof(target->user->account));
	user_account_login(NULL, target);
	result = json_object();
	json_expand_client(result, "client", target, 1);
	rpc_response(client, request, result);
	json_decref(result);
}

/** RPC Logout
 * @param name Nick or UID of the user to lookup
 * 
 * If successful returns a json client object with the updated account name as a result
 */
RPC_CALL_FUNC(rpc_auth_log_out)
{
	json_t *result;
	const char *name, *account;
	Client *target;

	name = json_object_get_string(params, "name");
	if (!name)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'name'");
		return;
	}
	if (!(target = find_user(name, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "User not online");
		return;
	}
	if (!MyUser(target))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "User online but not on our server");
		return;
	}
	strlcpy(target->user->account, "0", sizeof(target->user->account));
	user_account_login(NULL, target);
	result = json_object();
	json_expand_client(result, "client", target, 1);
	rpc_response(client, request, result);
	json_decref(result);
}
