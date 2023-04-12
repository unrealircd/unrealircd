/* stats.* RPC calls
 * (C) Copyright 2022-.. Bram Matthys (Syzop) and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
= {
	"rpc/stats",
	"1.0.2",
	"stats.* RPC calls",
	"UnrealIRCd Team",
	"unrealircd-6",
};

/* Forward declarations */
void rpc_stats_get(Client *client, json_t *request, json_t *params);

MOD_INIT()
{
	RPCHandlerInfo r;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&r, 0, sizeof(r));
	r.method = "stats.get";
	r.loglevel = ULOG_DEBUG;
	r.call = rpc_stats_get;
	if (!RPCHandlerAdd(modinfo->handle, &r))
	{
		config_error("[rpc/stats] Could not register RPC handler");
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

void json_expand_countries(json_t *main, const char *name, NameValuePrioList *geo)
{
	json_t *list = json_array();
	json_t *item;

	json_object_set_new(main, name, list);

	for (; geo; geo = geo->next)
	{
		item = json_object();
		json_object_set_new(item, "country", json_string_unreal(geo->name));
		json_object_set_new(item, "count", json_integer(0 - geo->priority));
		json_array_append_new(list, item);
	}
}

void rpc_stats_user(json_t *main, int detail)
{
	Client *client;
	int total = 0, ulined = 0, oper = 0;
	json_t *child;
	GeoIPResult *geo;
	NameValuePrioList *countries = NULL;

	child = json_object();
	json_object_set_new(main, "user", child);

	list_for_each_entry(client, &client_list, client_node)
	{
		if (IsUser(client))
		{
			total++;
			if (IsULine(client))
				ulined++;
			else if (IsOper(client))
				oper++;
			if (detail >= 1)
			{
				geo = geoip_client(client);
				if (geo && geo->country_code)
				{
					NameValuePrioList *e = find_nvplist(countries, geo->country_code);
					if (e)
					{
						DelListItem(e, countries);
						e->priority--;
						AddListItemPrio(e, countries, e->priority);
					} else {
						add_nvplist(&countries, -1, geo->country_code, NULL);
					}
				}
			}
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "ulined", json_integer(ulined));
	json_object_set_new(child, "oper", json_integer(oper));
	json_object_set_new(child, "record", json_integer(irccounts.global_max));
	if (detail >= 1)
		json_expand_countries(child, "countries", countries);
}

void rpc_stats_channel(json_t *main)
{
	json_t *child = json_object();
	json_object_set_new(main, "channel", child);
	json_object_set_new(child, "total", json_integer(irccounts.channels));
}

void rpc_stats_server(json_t *main)
{
	Client *client;
	int total = 0, ulined = 0, oper = 0;
	json_t *child = json_object();
	json_object_set_new(main, "server", child);

	total++; /* ourselves */
	list_for_each_entry(client, &global_server_list, client_node)
	{
		if (IsServer(client))
		{
			total++;
			if (IsULine(client))
				ulined++;
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "ulined", json_integer(ulined));
}

void rpc_stats_server_ban(json_t *main)
{
	Client *client;
	int index, index2;
	TKL *tkl;
	int total = 0;
	int server_ban = 0;
	int server_ban_exception = 0;
	int spamfilter = 0;
	int name_ban = 0;
	json_t *child = json_object();
	json_object_set_new(main, "server_ban", child);

	/* First, hashed entries.. */
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				total++;
				if (TKLIsServerBan(tkl))
					server_ban++;
				else if (TKLIsBanException(tkl))
					server_ban_exception++;
				else if (TKLIsNameBan(tkl))
					name_ban++;
				else if (TKLIsSpamfilter(tkl))
					spamfilter++;
			}
		}
	}

	/* Now normal entries.. */
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			total++;
			if (TKLIsServerBan(tkl))
				server_ban++;
			else if (TKLIsBanException(tkl))
				server_ban_exception++;
			else if (TKLIsNameBan(tkl))
				name_ban++;
			else if (TKLIsSpamfilter(tkl))
				spamfilter++;
		}
	}

	json_object_set_new(child, "total", json_integer(total));
	json_object_set_new(child, "server_ban", json_integer(server_ban));
	json_object_set_new(child, "spamfilter", json_integer(spamfilter));
	json_object_set_new(child, "name_ban", json_integer(name_ban));
	json_object_set_new(child, "server_ban_exception", json_integer(server_ban_exception));
}

void rpc_stats_get(Client *client, json_t *request, json_t *params)
{
	json_t *result, *item;
	const char *statsname;
	Channel *stats;
	int details;

	OPTIONAL_PARAM_INTEGER("object_detail_level", details, 1);

	result = json_object();
	rpc_stats_server(result);
	rpc_stats_user(result, details);
	rpc_stats_channel(result);
	rpc_stats_server_ban(result);
	rpc_response(client, request, result);
	json_decref(result);
}
