/* tkl.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/tkl",
	"1.0.0",
	"tkl.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
RPC_CALL_FUNC(rpc_tkl_list);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "tkl.list";
	r.call = rpc_tkl_list;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/tkl] Could not register RPC handler");
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
RPC_CALL_FUNC(rpc_tkl_list)
{
	json_t *result, *list, *item;
	int index, index2;
	TKL *tkl;

	result = json_object();
	list = json_array();
	json_object_set_new(result, "list", list);

	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				item = json_object();
				json_expand_tkl(item, NULL, tkl, 1);
				json_array_append_new(list, item);
			}
		}
	}
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			item = json_object();
			json_expand_tkl(item, NULL, tkl, 1);
			json_array_append_new(list, item);
		}
	}

	rpc_response(client, request, result);
	json_decref(result);
}
