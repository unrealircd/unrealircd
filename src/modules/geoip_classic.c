/* GEOIP Classic module
 * (C) Copyright 2021 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
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
/* for config reading only */
	int have_config;
	int have_ipv4_database;
	int have_ipv6_database;
};

/* Variables */

struct geoip_classic_config_s geoip_classic_config;
GeoIP *gi4 = NULL;
GeoIP *gi6 = NULL;

/* Forward declarations */
int geoip_classic_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int geoip_classic_configposttest(int *errs);
int geoip_classic_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void geoip_classic_free(void);
GeoIPResult *geoip_lookup_classic(char *ip);

int geoip_classic_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int i;
	
	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-classic"))
		return 0;

	geoip_classic_config.have_config = 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ipv4-database"))
		{
			if (geoip_classic_config.have_ipv4_database)
			{
				config_error("%s:%i: duplicate item set::geoip-classic::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-classic::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_classic_config.have_ipv4_database = 1;
			continue;
		}
		if (!strcmp(cep->name, "ipv6-database"))
		{
			if (geoip_classic_config.have_ipv6_database)
			{
				config_error("%s:%i: duplicate item set::geoip-classic::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-classic::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_classic_config.have_ipv6_database = 1;
			continue;
		}
		config_warn("%s:%i: unknown item set::geoip-classic::%s", cep->file->filename, cep->line_number, cep->name);
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_classic_configposttest(int *errs)
{
	int errors = 0;
	if (geoip_classic_config.have_config)
	{
		if (!geoip_classic_config.have_ipv4_database && !geoip_classic_config.have_ipv6_database)
		{
			config_error("geoip_classic: no database files specified! Remove set::geoip-classic to use defaults");
			errors++;
		}
	} else
	{
		safe_strdup(geoip_classic_config.v4_db_file, "GeoIP.dat");
		safe_strdup(geoip_classic_config.v6_db_file, "GeoIPv6.dat");

		if (is_file_readable(geoip_classic_config.v4_db_file, PERMDATADIR))
		{
			geoip_classic_config.have_ipv4_database = 1;
		} else
		{
			config_warn("[geoip_classic] cannot open IPv4 database file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_classic_config.v4_db_file, strerror(errno));
			safe_free(geoip_classic_config.v4_db_file);
		}
		if (is_file_readable(geoip_classic_config.v6_db_file, PERMDATADIR))
		{
			geoip_classic_config.have_ipv6_database = 1;
		} else
		{
			config_warn("[geoip_classic] cannot open IPv6 database file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_classic_config.v6_db_file, strerror(errno));
			safe_free(geoip_classic_config.v6_db_file);
		}
		if (!geoip_classic_config.have_ipv4_database && !geoip_classic_config.have_ipv6_database)
		{
			config_error("[geoip_classic] couldn't read any database! Either put these in %s location "
					"or specify another in set::geoip-classic config block", PERMDATADIR);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_classic_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-classic"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "ipv4-database") && geoip_classic_config.have_ipv4_database)
			safe_strdup(geoip_classic_config.v4_db_file, cep->value);
		if (!strcmp(cep->name, "ipv6-database") && geoip_classic_config.have_ipv6_database)
			safe_strdup(geoip_classic_config.v6_db_file, cep->value);
	}
	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPVoid(modinfo->handle, CALLBACKTYPE_GEOIP_LOOKUP, TO_PVOIDFUNC(geoip_lookup_classic)))
	{
		unreal_log(ULOG_ERROR, "geoip_classic", "GEOIP_ADD_CALLBACK_FAILED", NULL,
		           "geoip_classic: Could not install GEOIP_LOOKUP callback. "
		           "Most likely another geoip module is already loaded. "
		           "You can only load one!");
		return MOD_FAILED;
	}

	geoip_classic_config.have_config = 0;
	geoip_classic_config.have_ipv4_database = 0;
	geoip_classic_config.have_ipv6_database = 0;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, geoip_classic_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, geoip_classic_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	geoip_classic_free();
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, geoip_classic_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	int found_good_file = 0;

	if (geoip_classic_config.v4_db_file)
	{
		convert_to_absolute_path(&geoip_classic_config.v4_db_file, PERMDATADIR);
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
	}
	if (geoip_classic_config.v6_db_file)
	{
		convert_to_absolute_path(&geoip_classic_config.v6_db_file, PERMDATADIR);
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
		convert_to_absolute_path(&geoip_classic_config.v6_db_file, PERMDATADIR);
	}

	if (!found_good_file)
	{
		unreal_log(ULOG_ERROR, "geoip_classic", "GEOIP_CANNOT_OPEN_DB", NULL,
					"could not open any database!");
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	geoip_classic_free();
	return MOD_SUCCESS;
}

void geoip_classic_free(void)
{
	if (gi4)
		GeoIP_delete(gi4);
	if (gi6)
		GeoIP_delete(gi6);
	gi4 = NULL;
	gi6 = NULL;
	safe_free(geoip_classic_config.v4_db_file);
	safe_free(geoip_classic_config.v6_db_file);
}

GeoIPResult *geoip_lookup_classic(char *ip)
{
	static char buf[256];
	const char *country_code, *country_name;
	GeoIPLookup gl;
	GeoIP *gi;
	int geoid;
	GeoIPResult *r;

	if (!ip)
		return NULL;

	if (strchr(ip, ':'))
	{
		if (!gi6)
			return NULL;
		geoid = GeoIP_id_by_addr_v6_gl(gi6, ip, &gl);
		gi = gi6;
	} else
	{
		if (!gi4 || !strcmp(ip, "255.255.255.255"))
			return NULL;
		geoid = GeoIP_id_by_addr_gl(gi4, ip, &gl);
		gi = gi4;
	}

	if (geoid == 0)
		return NULL;

	country_code = GeoIP_code_by_id(geoid);
	country_name = GeoIP_country_name_by_id(gi, geoid);

	if (!country_code || !country_name)
		return NULL;

	r = safe_alloc(sizeof(GeoIPResult));
	safe_strdup(r->country_code, country_code);
	safe_strdup(r->country_name, country_name);
	return r;
}

