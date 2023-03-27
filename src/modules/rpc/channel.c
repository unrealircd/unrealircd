/* channel.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/channel",
	"1.0.4",
	"channel.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_channel_list(Client *client, json_t *request, json_t *params);
void rpc_channel_get(Client *client, json_t *request, json_t *params);
void rpc_channel_set_mode(Client *client, json_t *request, json_t *params);
void rpc_channel_set_topic(Client *client, json_t *request, json_t *params);
void rpc_channel_kick(Client *client, json_t *request, json_t *params);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "channel.list";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_channel_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/channel] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "channel.get";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_channel_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/channel] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "channel.set_mode";
	r.call = rpc_channel_set_mode;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/channel] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "channel.set_topic";
	r.call = rpc_channel_set_topic;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/channel] Could not register RPC handler");
		return MOD_FAILED;
	}
	memset(&r, 0, sizeof(r));
	r.method = "channel.kick";
	r.call = rpc_channel_kick;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/channel] Could not register RPC handler");
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

void rpc_channel_list(Client *client, json_t *request, json_t *params)
{
	json_t *result, *list, *item;
	Channel *channel;
	int details;

	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 1);
	if (details >= 5)
	{
		rpc_error(client, request, JSON_RPC_ERROR_INVALID_PARAMS, "Using an 'object_detail_level' of >=5 is not allowed in this call");
		return;
	}

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (channel = channels; channel; channel=channel->nextch)
	{
		item = json_object();
		json_expand_channel(item, NULL, channel, details);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}

void rpc_channel_get(Client *client, json_t *request, json_t *params)
{
	json_t *result, *item;
	const char *channelname;
	Channel *channel;
	int details;

	REQUIRE_PARAM_STRING("channel", channelname);
	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 3);

	if (!(channel = find_channel(channelname)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Channel not found");
		return;
	}

	result = json_object();
	json_expand_channel(result, "channel", channel, details);
	rpc_response(client, request, result);
	json_decref(result);
}

void rpc_channel_set_mode(Client *client, json_t *request, json_t *params)
{
	json_t *result, *item;
	const char *channelname, *modes, *parameters;
	Channel *channel;

	REQUIRE_PARAM_STRING("channel", channelname);
	REQUIRE_PARAM_STRING("modes", modes);
	REQUIRE_PARAM_STRING("parameters", parameters);

	if (!(channel = find_channel(channelname)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Channel not found");
		return;
	}

	set_channel_mode(channel, modes, parameters);

	/* Simply return success */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

void rpc_channel_set_topic(Client *client, json_t *request, json_t *params)
{
	json_t *result, *item;
	const char *channelname, *topic, *set_by=NULL, *str;
	Channel *channel;
	time_t set_at = 0;

	REQUIRE_PARAM_STRING("channel", channelname);
	REQUIRE_PARAM_STRING("topic", topic);
	OPTIONAL_PARAM_STRING("set_by", set_by);
	OPTIONAL_PARAM_STRING("set_at", str);
	if (str)
		set_at = server_time_to_unix_time(str);

	if (!(channel = find_channel(channelname)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Channel not found");
		return;
	}

	set_channel_topic(&me, channel, NULL, topic, set_by, set_at);

	/* Simply return success */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}

void rpc_channel_kick(Client *client, json_t *request, json_t *params)
{
	json_t *result, *item;
	const char *args[5];
	const char *channelname, *nick, *reason;
	Channel *channel;
	Client *acptr;
	time_t set_at = 0;

	REQUIRE_PARAM_STRING("channel", channelname);
	REQUIRE_PARAM_STRING("nick", nick);
	REQUIRE_PARAM_STRING("reason", reason);

	if (!(channel = find_channel(channelname)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Channel not found");
		return;
	}

	if (!(acptr = find_user(nick, NULL)))
	{
		rpc_error(client, request, JSON_RPC_ERROR_NOT_FOUND, "Nickname not found");
		return;
	}

	args[0] = NULL;
	args[1] = channel->name;
	args[2] = acptr->name;
	args[3] = reason;
	args[4] = NULL;
	do_cmd(&me, NULL, "KICK", reason ? 4 : 3, args);

	/* Simply return success
	 * TODO: actually we can do a find_member() check and such to see if the user is kicked!
	 * that is, assuming single channel ;)
	 */
	result = json_boolean(1);
	rpc_response(client, request, result);
	json_decref(result);
}
