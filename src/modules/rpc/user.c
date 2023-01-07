/* user.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/user",
	"1.0.2",
	"user.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_user_list);
RPC_CALL_FUNC(rpc_user_get);
RPC_CALL_FUNC(rpc_user_set_nick);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "user.list";
	r.call = rpc_user_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.get";
	r.call = rpc_user_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_nick";
	r.call = rpc_user_set_nick;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
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
RPC_CALL_FUNC(rpc_user_list)
{
	json_t *result, *list, *item;
	Client *acptr;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsUser(acptr))
			continue;

		item = json_object();
		json_expand_client(item, NULL, acptr, 1);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_get)
{
	json_t *result, *list, *item;
	const char *nick;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	result = json_object();
	json_expand_client(result, "client", acptr, 1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_set_nick)
{
	json_t *result, *list, *item;
	const char *args[5];
	const char *nick, *newnick_requested, *str;
	int force = 0;
	char newnick[NICKLEN+1];
	char tsbuf[32];
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	str = json_object_get_string(params, "force");
	if (str)
		force = config_checkval(str, CFG_YESNO);

	newnick_requested = json_object_get_string(params, "newnick");
	if (!newnick_requested)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'newnick'");
		return;
	}
	strlcpy(newnick, newnick_requested, iConf.nick_length + 1);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if (!do_nick_name(newnick) || strcmp(newnick, newnick_requested) ||
	    !strcasecmp(newnick, "IRC") || !strcasecmp(newnick, "IRCd"))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME, "New nickname contains forbidden character(s) or is too long");
		return;
	}

	if (!strcmp(nick, newnick))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Old nickname and new nickname are identical");
		return;
	}

	if (!force)
	{
		/* Check other restrictions */
		Client *check = find_user(newnick, NULL);
		int ishold = 0;

		/* Check if in use by someone else (do allow case-changing) */
		if (check && (acptr != check))
		{
			rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "New nickname is already taken by another user");
			return;
		}

		// Can't really check for spamfilter here, since it assumes user is local

		// But we can check q-lines...
		if (find_qline(acptr, newnick, &ishold))
		{
			rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME, "New nickname is forbidden by q-line");
			return;
		}
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = newnick;
	snprintf(tsbuf, sizeof(tsbuf), "%lld", (long long)TStime());
	args[3] = tsbuf;
	args[4] = NULL;
	do_cmd(&me, NULL, "SVSNICK", 4, args);

	/* Simply return success */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
