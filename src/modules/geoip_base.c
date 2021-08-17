/*
 * GEOIP Base module, needed for all geoip functions
 * as this stores the geo information in ModData.
 * (C) Copyright 2021-.. Syzop and The UnrealIRCd Team
 * License: GPLv2
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER
  = {
	"geoip_base",
	"5.0",
	"Base module for geoip",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void geoip_base_free(ModData *m);
char *geoip_base_serialize(ModData *m);
void geoip_base_unserialize(char *str, ModData *m);
int geoip_base_handshake(Client *client);
int geoip_base_connect(Client *client);
int geoip_base_whois(Client *client, Client *target);
int geoip_connect_extinfo(Client *client, NameValuePrioList **list);
int geoip_whois(Client *client, Client *target);
ModDataInfo *geoip_base_md; /* Module Data structure which we acquire */

MOD_INIT()
{
ModDataInfo mreq;

	MARK_AS_OFFICIAL_MODULE(modinfo);

	memset(&mreq, 0, sizeof(mreq));
	mreq.name = "geoip";
	mreq.free = geoip_base_free;
	mreq.serialize = geoip_base_serialize;
	mreq.unserialize = geoip_base_unserialize;
	mreq.sync = MODDATA_SYNC_EARLY;
	mreq.type = MODDATATYPE_CLIENT;
	geoip_base_md = ModDataAdd(modinfo->handle, mreq);
	if (!geoip_base_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_CONNECT_EXTINFO, 1, geoip_connect_extinfo); /* (prio: near-first) */
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, geoip_whois);

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

int geoip_base_handshake(Client *client)
{
	if (client->ip)
	{
		char *country = geoip_lookup_country(client->ip);

		if (!country)
			return 0;

		moddata_client_set(client, "geoip", country);
	}
	return 0;
}

void geoip_base_free(ModData *m)
{
	safe_free(m->str);
}

char *geoip_base_serialize(ModData *m)
{
	if (!m->str)
		return NULL;
	return m->str;
}

void geoip_base_unserialize(char *str, ModData *m)
{
	safe_strdup(m->str, str);
}

int geoip_connect_extinfo(Client *client, NameValuePrioList **list)
{
	char *country = moddata_client_get(client, "geoip");
	if (country)
		add_nvplist(list, 0, "country", country);
	return 0;
}

int geoip_whois(Client *client, Client *target)
{
	char *country;

	if (!IsOper(client))
		return 0;

	country = moddata_client_get(target, "geoip");
	if (!country)
		return 0;
	sendnumeric(client, RPL_WHOISCOUNTRY, target->name, country, country);

	return 0;
}
