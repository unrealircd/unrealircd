/* whowas.* RPC calls
 * (C) Copyright 2023-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/whowas",
	"1.0.1",
	"whowas.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Externals */
extern WhoWas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];

/* Forward declarations */
RPC_CALL_FUNC(rpc_whowas_get);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "whowas.get";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_whowas_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/whowas] Could not register RPC handler");
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

const char *whowas_event_to_string(WhoWasEvent event)
{
	if (event == WHOWAS_EVENT_QUIT)
		return "quit";
	if (event == WHOWAS_EVENT_NICK_CHANGE)
		return "nick-change";
	if (event == WHOWAS_EVENT_SERVER_TERMINATING)
		return "server-terminating";
	return "unknown";
}

void json_expand_whowas(json_t *j, const char *key, WhoWas *e, int detail)
{
	json_t *child;
	json_t *user = NULL;
	char buf[BUFSIZE+1];

	if (key)
	{
		child = json_object();
		json_object_set_new(j, key, child);
	} else {
		child = j;
	}

	json_object_set_new(child, "name", json_string_unreal(e->name));
	json_object_set_new(child, "event", json_string_unreal(whowas_event_to_string(e->event)));
	json_object_set_new(child, "logon_time", json_timestamp(e->logon));
	json_object_set_new(child, "logoff_time", json_timestamp(e->logoff));

	if (detail == 0)
		return;

	json_object_set_new(child, "hostname", json_string_unreal(e->hostname));
	json_object_set_new(child, "ip", json_string_unreal(e->ip));

	snprintf(buf, sizeof(buf), "%s!%s@%s", e->name, e->username, e->hostname);
	json_object_set_new(child, "details", json_string_unreal(buf));

	if (detail < 2)
		return;

	if (e->connected_since)
		json_object_set_new(child, "connected_since", json_timestamp(e->connected_since));

	/* client.user */
	user = json_object();
	json_object_set_new(child, "user", user);

	json_object_set_new(user, "username", json_string_unreal(e->username));
	if (!BadPtr(e->realname))
		json_object_set_new(user, "realname", json_string_unreal(e->realname));
	if (!BadPtr(e->virthost))
		json_object_set_new(user, "vhost", json_string_unreal(e->virthost));
	json_object_set_new(user, "servername", json_string_unreal(e->servername));
	if (!BadPtr(e->account))
		json_object_set_new(user, "account", json_string_unreal(e->account));

	if (e->ip)
	{
		GeoIPResult *geo = geoip_lookup(e->ip);
		if (geo)
		{
			json_t *geoip = json_object();
			json_object_set_new(child, "geoip", geoip);
			if (geo->country_code)
				json_object_set_new(geoip, "country_code", json_string_unreal(geo->country_code));
			if (geo->asn)
				json_object_set_new(geoip, "asn", json_integer(geo->asn));
			if (geo->asname)
				json_object_set_new(geoip, "asname", json_string_unreal(geo->asname));
			free_geoip_result(geo);
		}
	}
}

RPC_CALL_FUNC(rpc_whowas_get)
{
	json_t *result, *list, *item;
	int details;
	int i;
	const char *nick;
	const char *ip;

	OPTIONAL_PARAM_STRING("nick", nick);
	OPTIONAL_PARAM_STRING("ip", ip);
	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 2);
	if (details == 3)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Using an 'object_detail_level' of 3 is not allowed in user.* calls, use 0, 1, 2 or 4.");
		return;
	}

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (i=0; i < NICKNAMEHISTORYLENGTH; i++)
	{
		WhoWas *e = &WHOWAS[i];
		if (!e->name)
			continue;
		if (nick && !match_simple(nick, e->name))
			continue;
		if (ip && !match_simple(ip, e->ip))
			continue;
		item = json_object();
		json_expand_whowas(item, NULL, e, details);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}
