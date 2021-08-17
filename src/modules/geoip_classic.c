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

/* Variables */
char *db_file = NULL;
GeoIP *gi = NULL;

/* Forward declarations */
char *geo_lookup_classic(char *ip);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPCharEx(modinfo->handle, CALLBACKTYPE_GEO_LOOKUP, geo_lookup_classic))
	{
		config_error("geoip_classic: Could not install GEO_LOOKUP callback. "
		             "Maybe another geoip module is already loaded? You can only load one!");
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	safe_strdup(db_file, "GeoIP.dat");
	convert_to_absolute_path(&db_file, PERMDATADIR);
	gi = GeoIP_open(db_file, GEOIP_STANDARD | GEOIP_CHECK_CACHE | GEOIP_SILENCE);
	if (!gi)
	{
		int save_err = errno;
		unreal_log(ULOG_ERROR, "geoip_classic", "GEOIP_CANNOT_OPEN_DB", NULL,
		           "Could not open '$filename': $system_error",
		           log_data_string("filename", db_file),
		           log_data_string("system_error", strerror(save_err)));
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
	if (gi)
		GeoIP_delete(gi);
	safe_free(db_file);
	return MOD_SUCCESS;
}

char *geo_lookup_classic(char *ip)
{
	static char buf[256];
	const char *r;

	if (!gi || !ip || !strcmp(ip, "255.255.255.255"))
		return NULL;

	r = GeoIP_country_code_by_name(gi, ip);
	if (!r)
		return NULL;

	/* Return a copy, since we cannot guarantee const char * */
	strlcpy(buf, r, sizeof(buf));
	return buf;
}
