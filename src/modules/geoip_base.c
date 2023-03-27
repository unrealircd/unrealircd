/*
 * GEOIP Base module, needed for all geoip functions
 * as this stores the geo information in ModData.
 * (C) Copyright 2021-.. Syzop and The UnrealIRCd Team
 * License: GPLv2 or later
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

struct geoip_base_config_s {
	int check_on_load;
};

/* Forward declarations */
void geoip_base_free(ModData *m);
const char *geoip_base_serialize(ModData *m);
void geoip_base_unserialize(const char *str, ModData *m);
int geoip_base_handshake(Client *client);
int geoip_base_ip_change(Client *client, const char *oldip);
int geoip_base_whois(Client *client, Client *target, NameValuePrioList **list);
int geoip_connect_extinfo(Client *client, NameValuePrioList **list);
int geoip_json_expand_client(Client *client, int detail, json_t *j);
int geoip_base_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int geoip_base_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(geoip_base_set_existing_users_evt);
CMD_FUNC(cmd_geoip);

ModDataInfo *geoip_md; /* Module Data structure which we acquire */
struct geoip_base_config_s geoip_base_config;

/* We can use GEOIPDATA() and GEOIPDATARAW() for fast access.
 * People wanting to get this information from outside this module
 * should use geoip_client(client) !
 */

#define GEOIPDATARAW(x)	(moddata_client((x), geoip_md).ptr)
#define GEOIPDATA(x)	((GeoIPResult *)moddata_client((x), geoip_md).ptr)

int geoip_base_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int i;
	
	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "check-on-load"))
		{
			CheckNull(cep);
			continue;
		}
		config_warn("%s:%i: unknown item geoip::%s", cep->file->filename, cep->line_number, cep->name);
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_base_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "check-on-load"))
			geoip_base_config.check_on_load = config_checkval(cep->value, CFG_YESNO);
	}
	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, geoip_base_configtest);
	return MOD_SUCCESS;
}

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

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, geoip_base_configrun);
	HookAdd(modinfo->handle, HOOKTYPE_HANDSHAKE, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_IP_CHANGE, 0, geoip_base_ip_change);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_HANDSHAKE_OUT, 0, geoip_base_handshake);
	HookAdd(modinfo->handle, HOOKTYPE_CONNECT_EXTINFO, 1, geoip_connect_extinfo); /* (prio: near-first) */
	HookAdd(modinfo->handle, HOOKTYPE_PRE_LOCAL_CONNECT, 0,geoip_base_handshake); /* in case the IP changed in registration phase (WEBIRC, HTTP Forwarded) */
	HookAdd(modinfo->handle, HOOKTYPE_WHOIS, 0, geoip_base_whois);
	HookAdd(modinfo->handle, HOOKTYPE_JSON_EXPAND_CLIENT, 0, geoip_json_expand_client);

	CommandAdd(modinfo->handle, "GEOIP", cmd_geoip, MAXPARA, CMD_USER);

	/* set defaults */
	geoip_base_config.check_on_load = 1;

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	/* add info for all users upon module loading if enabled, but delay it a bit for data provider module to load */
	if (geoip_base_config.check_on_load)
	{
		EventAdd(modinfo->handle, "geoip_base_set_existing_users", geoip_base_set_existing_users_evt, NULL, 1000, 1);
	}
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
		free_geoip_result(GEOIPDATA(client));
		GEOIPDATARAW(client) = NULL;
	}
	GEOIPDATARAW(client) = res;
	return 0;
}

int geoip_base_ip_change(Client *client, const char *oldip)
{
	geoip_base_handshake(client);
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

const char *geoip_base_serialize(ModData *m)
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

void geoip_base_unserialize(const char *str, ModData *m)
{
	char buf[512], *p=NULL, *varname, *value;
	char *country_name = NULL;
	char *country_code = NULL;
	GeoIPResult *res;

	if (m->ptr)
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

EVENT(geoip_base_set_existing_users_evt)
{
	Client *client;
	list_for_each_entry(client, &client_list, client_node)
	{
		if (MyUser(client))
			geoip_base_handshake(client);
	}
}

int geoip_connect_extinfo(Client *client, NameValuePrioList **list)
{
	GeoIPResult *geo = GEOIPDATA(client);
	if (geo)
		add_nvplist(list, 0, "country", geo->country_code);
	return 0;
}

int geoip_json_expand_client(Client *client, int detail, json_t *j)
{
	GeoIPResult *geo = GEOIPDATA(client);
	json_t *geoip;

	if (!geo)
		return 0;

	geoip = json_object();
	json_object_set_new(j, "geoip", geoip);
	json_object_set_new(geoip, "country_code", json_string_unreal(geo->country_code));

	return 0;
}

int geoip_base_whois(Client *client, Client *target, NameValuePrioList **list)
{
	GeoIPResult *geo;
	char buf[512];
	int policy = whois_get_policy(client, target, "geo");

	if (policy == WHOIS_CONFIG_DETAILS_NONE)
		return 0;

	geo = GEOIPDATA(target);
	if (!geo)
		return 0;

	// we only have country atm, but if we add city then city goes in 'full' and
	// country goes in 'limited'
	// if policy == WHOIS_CONFIG_DETAILS_LIMITED ...
	add_nvplist_numeric_fmt(list, 0, "geo", client, RPL_WHOISCOUNTRY,
	                        "%s %s :is connecting from %s",
	                        target->name,
	                        geo->country_code,
	                        geo->country_name);
	return 0;
}

CMD_FUNC(cmd_geoip)
{
	const char *ip = NULL;
	Client *target;
	GeoIPResult *res;

	if (!IsOper(client))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 2) || BadPtr(parv[1]))
	{
		/* Maybe some report */
		return;
	}

	if (strchr(parv[1], '.') || strchr(parv[1], ':'))
	{
		ip = parv[1];
	} else {
		target = find_user(parv[1], NULL);
		if (!target)
		{
			sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
			return;
		}
		ip = target->ip;
		if (!ip)
		{
			sendnotice(client, "User %s has no known IP address", client->name); // (eg: services bot)
			return;
		}
	}

	res = geoip_lookup(ip);

	sendnotice(client, "*** GEOIP information for IP %s ***", ip);
	if (!res)
	{
		sendnotice(client, "- No information available");
		return;
	} else {
		if (res->country_code)
			sendnotice(client, "- Country code: %s", res->country_code);
		if (res->country_name)
			sendnotice(client, "- Country name: %s", res->country_name);
	}

	free_geoip_result(res);

	sendnotice(client, "*** End of information ***");
}
