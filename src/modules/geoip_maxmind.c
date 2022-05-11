/* GEOIP maxmind module
 * (C) Copyright 2021 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"
#include <maxminddb.h>

ModuleHeader MOD_HEADER
  = {
	"geoip_maxmind",
	"5.0",
	"GEOIP using maxmind databases", 
	"UnrealIRCd Team",
	"unrealircd-6",
	};

struct geoip_maxmind_config_s {
	char *db_file;
/* for config reading only */
	int have_config;
	int have_database;
};

/* Variables */

struct geoip_maxmind_config_s geoip_maxmind_config;
MMDB_s mmdb;

/* Forward declarations */
int geoip_maxmind_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int geoip_maxmind_configposttest(int *errs);
int geoip_maxmind_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void geoip_maxmind_free(void);
GeoIPResult *geoip_lookup_maxmind(char *ip);

int geoip_maxmind_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;
	int i;
	
	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-maxmind"))
		return 0;

	geoip_maxmind_config.have_config = 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database"))
		{
			if (geoip_maxmind_config.have_database)
			{
				config_error("%s:%i: duplicate item set::geoip-maxmind::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-maxmind::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_maxmind_config.have_database = 1;
			continue;
		}
		config_warn("%s:%i: unknown item set::geoip-maxmind::%s", cep->file->filename, cep->line_number, cep->name);
	}
	
	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_maxmind_configposttest(int *errs)
{
	int errors = 0;
	if (geoip_maxmind_config.have_config)
	{
		if (!geoip_maxmind_config.have_database)
		{
			config_error("geoip_maxmind: no database file specified! Remove set::geoip-maxmind to use defaults");
			errors++;
		}
	} else
	{
		safe_strdup(geoip_maxmind_config.db_file, "GeoLite2-Country.mmdb");

		if (is_file_readable(geoip_maxmind_config.db_file, PERMDATADIR))
		{
			geoip_maxmind_config.have_database = 1;
		} else
		{
			config_error("[geoip_maxmind] cannot open database file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_maxmind_config.db_file, strerror(errno));
			safe_free(geoip_maxmind_config.db_file);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_maxmind_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-maxmind"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database") && geoip_maxmind_config.have_database)
			safe_strdup(geoip_maxmind_config.db_file, cep->value);
	}
	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPVoid(modinfo->handle, CALLBACKTYPE_GEOIP_LOOKUP, TO_PVOIDFUNC(geoip_lookup_maxmind)))
	{
		unreal_log(ULOG_ERROR, "geoip_maxmind", "GEOIP_ADD_CALLBACK_FAILED", NULL,
				   "geoip_maxmind: Could not install GEOIP_LOOKUP callback. "
				   "Most likely another geoip module is already loaded. "
				   "You can only load one!");
		return MOD_FAILED;
	}

	geoip_maxmind_config.have_config = 0;
	geoip_maxmind_config.have_database = 0;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, geoip_maxmind_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, geoip_maxmind_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, geoip_maxmind_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	geoip_maxmind_free();
	convert_to_absolute_path(&geoip_maxmind_config.db_file, PERMDATADIR);
	
	int status = MMDB_open(geoip_maxmind_config.db_file, MMDB_MODE_MMAP, &mmdb);

	if (status != MMDB_SUCCESS) {
		int save_err = errno;
		unreal_log(ULOG_WARNING, "geoip_maxmind", "GEOIP_CANNOT_OPEN_DB", NULL,
				   "Could not open '$filename' - $maxmind_error; IO error: $io_error",
				   log_data_string("filename", geoip_maxmind_config.db_file),
				   log_data_string("maxmind_error", MMDB_strerror(status)),
				   log_data_string("io_error", (status == MMDB_IO_ERROR)?strerror(save_err):"none"));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	geoip_maxmind_free();
	return MOD_SUCCESS;
}

void geoip_maxmind_free(void)
{
	MMDB_close(&mmdb);
}

GeoIPResult *geoip_lookup_maxmind(char *ip)
{
	int gai_error, mmdb_error, status;
	MMDB_lookup_result_s result;
	MMDB_entry_data_s country_code;
	MMDB_entry_data_s country_name;
	char *country_code_str, *country_name_str;
	GeoIPResult *r;

	if (!ip)
		return NULL;

	result = MMDB_lookup_string(&mmdb, ip, &gai_error, &mmdb_error);
	if (gai_error)
	{
		unreal_log(ULOG_DEBUG, "geoip_maxmind", "GEOIP_DB_ERROR", NULL,
				"libmaxminddb: getaddrinfo error for $ip: $error",
				log_data_string("ip", ip),
				log_data_string("error", gai_strerror(gai_error)));
		return NULL;
	}
	
	if (mmdb_error != MMDB_SUCCESS)
	{
		unreal_log(ULOG_DEBUG, "geoip_maxmind", "GEOIP_DB_ERROR", NULL,
				"libmaxminddb: library error for $ip: $error",
				log_data_string("ip", ip),
				log_data_string("error", MMDB_strerror(mmdb_error)));
		return NULL;
	}

	if (!result.found_entry) /* no result */
		return NULL;

	status = MMDB_get_value(&result.entry, &country_code, "country", "iso_code", NULL);
	if (status != MMDB_SUCCESS || !country_code.has_data || country_code.type != MMDB_DATA_TYPE_UTF8_STRING)
		return NULL;
	status = MMDB_get_value(&result.entry, &country_name, "country", "names", "en", NULL);
	if (status != MMDB_SUCCESS || !country_name.has_data || country_name.type != MMDB_DATA_TYPE_UTF8_STRING)
		return NULL;

	/* these results are not null-terminated */
	country_code_str = safe_alloc(country_code.data_size + 1);
	country_name_str = safe_alloc(country_name.data_size + 1);
	memcpy(country_code_str, country_code.utf8_string, country_code.data_size);
	country_code_str[country_code.data_size] = '\0';
	memcpy(country_name_str, country_name.utf8_string, country_name.data_size);
	country_name_str[country_name.data_size] = '\0';

	r = safe_alloc(sizeof(GeoIPResult));
	r->country_code = country_code_str;
	r->country_name = country_name_str;
	return r;
}

