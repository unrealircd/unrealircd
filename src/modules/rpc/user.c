/* user.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/user",
	"1.0.5",
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
RPC_CALL_FUNC(rpc_user_quit);
RPC_CALL_FUNC(rpc_user_join);
RPC_CALL_FUNC(rpc_user_part);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "user.list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_user_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.get";
	r.loglevel = ULOG_DEBUG;
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
	memset(&r, 0, sizeof(r));
	r.method = "user.quit";
	r.call = rpc_user_quit;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.join";
	r.call = rpc_user_join;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/user] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "user.part";
	r.call = rpc_user_part;
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

RPC_CALL_FUNC(rpc_user_list)
{
	json_t *result, *list, *item;
	Client *acptr;
	int details;

	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 2);
	if (details == 3)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Using an 'object_detail_level' of 3 is not allowed in user.* calls, use 0, 1, 2 or 4.");
		return;
	}

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!IsUser(acptr))
			continue;

		item = json_object();
		json_expand_client(item, NULL, acptr, details);
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
	int details;

	REQUIRE_PARAM_STRING("nick", nick);

	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 4);
	if (details == 3)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Using an 'object_detail_level' of 3 is not allowed in user.* calls, use 0, 1, 2 or 4.");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	result = json_object();
	json_expand_client(result, "client", acptr, details);
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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("newnick", newnick_requested);
	strlcpy(newnick, newnick_requested, iConf.nick_length + 1);
	OPTIONAL_PARAM_BOOLEAN("force", force, 0);

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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("username", username);

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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("realname", realname);

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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("vhost", vhost);

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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("modes", modes);
	OPTIONAL_PARAM_BOOLEAN("hidden", hidden, 0);

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

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("snomask", snomask);
	OPTIONAL_PARAM_BOOLEAN("hidden", hidden, 0);

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
	OPTIONAL_PARAM_STRING("class", class);
	OPTIONAL_PARAM_STRING("modes", modes);
	OPTIONAL_PARAM_STRING("snomask", snomask);
	OPTIONAL_PARAM_STRING("vhost", vhost);

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

RPC_CALL_FUNC(rpc_user_quit)
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
	do_cmd(&me, NULL, "SVSKILL", 3, args);

	/* Return result */
	if ((acptr = find_user(nick, NULL)) && !IsDead(acptr))
		result = json_boolean(0);
	else
		result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_join)
{
	json_t *result, *list, *item;
	const char *args[5];
	const char *nick, *channel, *key=NULL;
	Client *acptr;
	int force = 0;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("channel", channel);
	OPTIONAL_PARAM_STRING("key", key);
	OPTIONAL_PARAM_BOOLEAN("force", force, 0);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = channel;

	if (force == 0)
	{
		args[3] = key;
		args[4] = NULL;
		do_cmd(&me, NULL, "SVSJOIN", key ? 4 : 3, args);
	} else {
		args[3] = NULL;
		do_cmd(&me, NULL, "SAJOIN", 3, args);
	}

	/* Return result -- yeah this is always true, not so good.. :D
	 * It is that way because we (this server) may not actually
	 * do the SVSJOIN at all, we may be just relaying it to some
	 * other server.
	 */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

RPC_CALL_FUNC(rpc_user_part)
{
	json_t *result, *list, *item;
	const char *args[5];
	const char *nick, *channel, *reason=NULL;
	Client *acptr;
	int force = 0;

	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("channel", channel);
	OPTIONAL_PARAM_STRING("reason", reason);
	OPTIONAL_PARAM_BOOLEAN("force", force, 0);

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = acptr->name;
	args[2] = channel;
	args[3] = reason;
	args[4] = NULL;
	do_cmd(&me, NULL, force ? "SAPART" : "SVSPART", reason ? 4 : 3, args);

	/* Return result. Always 'true' at the moment.
	 * Technically we could check if the user is in all of these channels.
	 * But then again, do we really want to return failure if one specified
	 * channel does not exist out of X channels to be parted? Not worth it.
	 */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
