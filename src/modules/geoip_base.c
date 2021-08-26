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
int geoip_base_whois(Client *client, Client *target);
int geoip_connect_extinfo(Client *client, NameValuePrioList **list);
int geoip_whois(Client *client, Client *target);
ModDataInfo *geoip_md; /* Module Data structure which we acquire */

/* We can use GEOIPDATA() and GEOIPDATARAW() for fast access.
 * People wanting to get this information from outside this module
 * should use geoip_client(client) !
 */

#define GEOIPDATARAW(x)	(moddata_client((x), geoip_md).ptr)
#define GEOIPDATA(x)	((GeoIPResult *)moddata_client((x), geoip_md).ptr)

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
	geoip_md = ModDataAdd(modinfo->handle, mreq);
	if (!geoip_md)
		abort();

	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_CONNECT_EXTINFO, 1, geoip_connect_extinfo); /* (prio: near-first) */
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0,geoip_base_handshake); /* in case the IP changed in registration phase (WEBIRC, HTTP Forwarded) */
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, geoip_base_handshake); /* remote user */
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
	if (!client->ip)
		return 0;
	GeoIPResult *res = geoip_lookup(client->ip);

	if (!res)
		return 0;

	if (GEOIPDATA(client))
	{
		/* Can this even happen? Ah well.. */
		free_geoip_result(GEOIPDATA(client));
		GEOIPDATARAW(client) = NULL;
	}
	GEOIPDATARAW(client) = res;
	return 0;
}

void geoip_base_free(ModData *m)
{
	if (m->ptr)
	{
		free_geoip_result((GeoIPResult *)m->ptr);
		m->ptr = NULL;
	}
}

char *geoip_base_serialize(ModData *m)
{
	static char buf[512];
	GeoIPResult *geo;

	if (!m->ptr)
		return NULL;

	geo = m->ptr;
	snprintf(buf, sizeof(buf), "cc=%s|cd=%s",
	         geo->country_code,
	         geo->country_name);

	return buf;
}

void geoip_base_unserialize(char *str, ModData *m)
{
	char buf[512], *p=NULL, *varname, *value;
	char *country_name = NULL;
	char *country_code = NULL;
	GeoIPResult *res;

	if (m->ptr == NULL)
	{
		free_geoip_result((GeoIPResult *)m->ptr);
		m->ptr = NULL;
	}
	if (str == NULL)
		return;

	strlcpy(buf, str, sizeof(buf));
	for (varname = strtoken(&p, buf, "|"); varname; varname = strtoken(&p, NULL, "|"))
	{
		value = strchr(varname, '=');
		if (!value)
			continue;
		*value++ = '\0';
		if (!strcmp(varname, "cc"))
			country_code = value;
		else if (!strcmp(varname, "cd"))
			country_name = value;
	}

	if (!country_code || !country_name)
		return; /* does not meet minimum criteria */

	res = safe_alloc(sizeof(GeoIPResult));
	safe_strdup(res->country_name, country_name);
	safe_strdup(res->country_code, country_code);
	m->ptr = res;
}

int geoip_connect_extinfo(Client *client, NameValuePrioList **list)
{
	GeoIPResult *geo = GEOIPDATA(client);
	if (geo)
		add_nvplist(list, 0, "country", geo->country_code);
	return 0;
}

int geoip_whois(Client *client, Client *target)
{
	GeoIPResult *geo;

	if (!IsOper(client))
		return 0;

	geo = GEOIPDATA(target);
	if (!geo)
		return 0;

	sendnumeric(client, RPL_WHOISCOUNTRY, target->name, geo->country_code, geo->country_name);
	return 0;
}
