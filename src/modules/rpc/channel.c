/* channel.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/channel",
	"1.0.0",
	"channel.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_channel_list(Client *client, json_t *request, json_t *params);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "channel.list";
	r.call = rpc_channel_list;
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

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (channel = channels; channel; channel=channel->nextch)
	{
		item = json_object();
		json_expand_channel(item, NULL, channel, 1);
		json_array_append_new(list, item);
	}

	rpc_response(client, request, result);
	json_decref(result);
}
