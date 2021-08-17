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

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
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

void test(void)
{
	GeoIP *gi;
	const char *country;

	gi = GeoIP_open("/data/GeoIP.dat", GEOIP_STANDARD | GEOIP_CHECK_CACHE/* | GEOIP_SILENCE*/);
	country = GeoIP_country_code_by_name(gi, "192.168.1.1");
}
