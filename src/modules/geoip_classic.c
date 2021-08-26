/* GEOIP Classic module
 * (C) Copyright 2021 Bram Matthys and the UnrealIRCd team
 * License: GPLv2
 */

#include "unrealircd.h"
#include <GeoIP.h>

ModuleHeader MOD_HEADER
  = {
	"geoip_classic",
	"5.0",
	"GEOIP using classic databases", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

struct geoip_classic_config_s {
	char *v4_db_file;
	char *v6_db_file;
};

/* Variables */

struct geoip_classic_config_s geoip_classic_config;
GeoIP *gi4 = NULL;
GeoIP *gi6 = NULL;

/* Forward declarations */
GeoIPResult *geoip_lookup_classic(char *ip);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPVoidEx(modinfo->handle, CALLBACKTYPE_GEOIP_LOOKUP, TO_PVOIDFUNC(geoip_lookup_classic)))
	{
		unreal_log(ULOG_ERROR, "geoip_classic", "GEOIP_ADD_CALLBACK_FAILED", NULL,
		           "geoip_classic: Could not install GEOIP_LOOKUP callback. "
		           "Most likely another geoip module is already loaded. "
		           "You can only load one!");
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_INIT()
{
	int found_good_file = 0;

	MARK_AS_OFFICIAL_MODULE(modinfo);
	safe_strdup(geoip_classic_config.v4_db_file, "GeoIP.dat");
	safe_strdup(geoip_classic_config.v6_db_file, "GeoIPv6.dat");

	convert_to_absolute_path(&geoip_classic_config.v4_db_file, PERMDATADIR);
	convert_to_absolute_path(&geoip_classic_config.v6_db_file, PERMDATADIR);

	gi4 = GeoIP_open(geoip_classic_config.v4_db_file, GEOIP_STANDARD | GEOIP_CHECK_CACHE | GEOIP_SILENCE);
	if (gi4)
	{
		found_good_file = 1;
	} else
	{
		int save_err = errno;
		unreal_log(ULOG_WARNING, "geoip_classic", "GEOIP_CANNOT_OPEN_DB", NULL,
		           "[IPv4] Could not open '$filename': $system_error",
		           log_data_string("filename", geoip_classic_config.v4_db_file),
		           log_data_string("system_error", strerror(save_err)));
	}

	gi6 = GeoIP_open(geoip_classic_config.v6_db_file, GEOIP_STANDARD | GEOIP_CHECK_CACHE | GEOIP_SILENCE);
	if (gi6)
	{
		found_good_file = 1;
	} else
	{
		int save_err = errno;
		unreal_log(ULOG_WARNING, "geoip_classic", "GEOIP_CANNOT_OPEN_DB", NULL,
		           "[IPv6] Could not open '$filename': $system_error",
		           log_data_string("filename", geoip_classic_config.v6_db_file),
		           log_data_string("system_error", strerror(save_err)));
	}

	if (!found_good_file)
	{
		unreal_log(ULOG_ERROR, "geoip_classic", "GEOIP_CANNOT_OPEN_DB", NULL,
					"could not open any database!");
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
	if (gi4)
		GeoIP_delete(gi4);
	if (gi6)
		GeoIP_delete(gi6);
	safe_free(geoip_classic_config.v4_db_file);
	safe_free(geoip_classic_config.v6_db_file);
	return MOD_SUCCESS;
}

GeoIPResult *geoip_lookup_classic(char *ip)
{
	static char buf[256];
	const char *country_code, *country_name;
	GeoIPResult *r;

	if (!ip)
		return NULL;

	if (strchr(ip, ':'))
	{
		if (!gi6)
			return NULL;

		country_code = GeoIP_country_code_by_name_v6(gi6, ip);
		country_name = GeoIP_country_name_by_name_v6(gi6, ip);
	} else
	{
		if (!gi4 || !strcmp(ip, "255.255.255.255"))
			return NULL;

		country_code = GeoIP_country_code_by_name(gi4, ip);
		country_name = GeoIP_country_name_by_name(gi4, ip);
	}

	if (!country_code || !country_name)
			return NULL;

	r = safe_alloc(sizeof(GeoIPResult));
	safe_strdup(r->country_code, country_code);
	safe_strdup(r->country_name, country_name);
	return r;
}

