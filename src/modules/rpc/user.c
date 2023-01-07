/* user.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/user",
	"1.0.4",
	"user.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_user_list);
RPC_CALL_FUNC(rpc_user_get);
RPC_CALL_FUNC(rpc_user_set_nick);
RPC_CALL_FUNC(rpc_user_set_username);
RPC_CALL_FUNC(rpc_user_set_realname);
RPC_CALL_FUNC(rpc_user_set_vhost);
RPC_CALL_FUNC(rpc_user_set_mode);
RPC_CALL_FUNC(rpc_user_set_snomask);
RPC_CALL_FUNC(rpc_user_set_oper);
RPC_CALL_FUNC(rpc_user_kill);

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
	memset(&r, 0, sizeof(r));
	r.method = "user.set_username";
	r.call = rpc_user_set_username;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_realname";
	r.call = rpc_user_set_realname;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_vhost";
	r.call = rpc_user_set_vhost;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_mode";
	r.call = rpc_user_set_mode;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_snomask";
	r.call = rpc_user_set_snomask;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.set_oper";
	r.call = rpc_user_set_oper;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.kill";
	r.call = rpc_user_kill;
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

	force = json_object_get_boolean(params, "force", 0);

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

RPC_CALL_FUNC(rpc_user_set_username)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *username, *str;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	username = json_object_get_string(params, "username");
	if (!username)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'username'");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if (!valid_username(username))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME, "New username contains forbidden character(s) or is too long");
		return;
	}

	if (!strcmp(acptr->user->username, username))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Old and new user name are identical");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = username;
	args[3] = NULL;
	do_cmd(&me, NULL, "CHGIDENT", 3, args);

	/* Return result */
	if (!strcmp(acptr->user->username, username))
		result = json_boolean(1);
	else
		result = json_boolean(0);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_set_realname)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *realname, *str;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	realname = json_object_get_string(params, "realname");
	if (!realname)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'realname'");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if (strlen(realname) > REALLEN)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME, "New real name is too long");
		return;
	}

	if (!strcmp(acptr->info, realname))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Old and new real name are identical");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = realname;
	args[3] = NULL;
	do_cmd(&me, NULL, "CHGNAME", 3, args);

	/* Return result */
	if (!strcmp(acptr->info, realname))
		result = json_boolean(1);
	else
		result = json_boolean(0);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_set_vhost)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *vhost, *str;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	vhost = json_object_get_string(params, "vhost");
	if (!vhost)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'vhost'");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if ((strlen(vhost) > HOSTLEN) || !valid_host(vhost, 0))
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_NAME, "New vhost contains forbidden character(s) or is too long");
		return;
	}

	if (!strcmp(GetHost(acptr), vhost))
	{
		rpc_error(client, request, JSON_RPC_ERROR_ALREADY_EXISTS, "Old and new vhost are identical");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = vhost;
	args[3] = NULL;
	do_cmd(&me, NULL, "CHGHOST", 3, args);

	/* Return result */
	if (!strcmp(GetHost(acptr), vhost))
		result = json_boolean(1);
	else
		result = json_boolean(0);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_set_mode)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *modes, *str;
	int hidden;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	modes = json_object_get_string(params, "modes");
	if (!modes)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'modes'");
		return;
	}

	hidden = json_object_get_boolean(params, "hidden", 0);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = modes;
	args[3] = NULL;
	do_cmd(&me, NULL, hidden ? "SVSMODE" : "SVS2MODE", 3, args);

	/* Return result (always true) */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_set_snomask)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *snomask, *str;
	int hidden;
	Client *acptr;

	nick = json_object_get_string(params, "nick");
	if (!nick)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'nick'");
		return;
	}

	snomask = json_object_get_string(params, "snomask");
	if (!snomask)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: 'snomask'");
		return;
	}

	hidden = json_object_get_boolean(params, "hidden", 0);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = snomask;
	args[3] = NULL;
	do_cmd(&me, NULL, hidden ? "SVSSNO" : "SVS2SNO", 3, args);

	/* Return result (always true) */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

#define REQUIRE_PARAM_STRING(name, varname)     do { \
                                                    varname = json_object_get_string(params, name); \
                                                    if (!varname) \
                                                    { \
                                                        rpc_error_fmt(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Missing parameter: '%s'", name); \
                                                        return; \
                                                    } \
                                                   } while(0)

RPC_CALL_FUNC(rpc_user_set_oper)
{
	json_t *result, *list, *item;
	const char *args[9];
	const char *nick, *oper_account, *oper_class;
	const char *class=NULL, *modes=NULL, *snomask=NULL, *vhost=NULL;
	Client *acptr;
	char default_modes[64];

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("oper_account", oper_account);
	REQUIRE_PARAM_STRING("oper_class", oper_class);
	class = json_object_get_string(params, "class");
	modes = json_object_get_string(params, "modes");
	snomask = json_object_get_string(params, "snomask");
	vhost = json_object_get_string(params, "vhost");

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	if (modes == NULL)
	{
		strlcpy(default_modes, get_usermode_string_raw(OPER_MODES), sizeof(default_modes));
		modes = default_modes;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = oper_account;
	args[3] = oper_class;
	args[4] = class ? class : "opers";
	args[5] = modes;
	args[6] = snomask ? snomask : iConf.oper_snomask;
	args[7] = vhost ? vhost : "-";
	args[8] = NULL;
	do_cmd(&me, NULL, "SVSO", 8, args);

	/* Return result (always true) */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_kill)
{
	json_t *result, *list, *item;
	const char *args[4];
	const char *nick, *reason;
	Client *acptr;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("reason", reason);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = reason;
	args[3] = NULL;
	do_cmd(&me, NULL, "KILL", 3, args);

	/* Return result */
	if ((acptr = find_user(nick, NULL)) && !IsDead(acptr))
		result = json_boolean(0);
	else
		result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
