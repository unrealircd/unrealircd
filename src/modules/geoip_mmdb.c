/* GEOIP mmdb module
 * (C) Copyright 2021 Bram Matthys and the UnrealIRCd team
 * License: GPLv2 or later
 */

#include "unrealircd.h"
#include "mmdb.h"

ModuleHeader MOD_HEADER = {
    "geoip_mmdb",
    "5.2",
    "GEOIP using mmdb databases",
    "UnrealIRCd Team",
    "unrealircd-6",
};

struct geoip_mmdb_config_s {
	char *db_file;
	char *asn_db_file;
/* for config reading only */
	int have_config;
	int have_database;
	int have_asn_database;
};

/* Variables */

struct geoip_mmdb_config_s geoip_mmdb_config;
MMDB_DB mmdb, asn_mmdb;

/* Forward declarations */
int geoip_mmdb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int geoip_mmdb_configposttest(int *errs);
int geoip_mmdb_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
void geoip_mmdb_free(void);
GeoIPResult *geoip_lookup_mmdb(char *ip);

int geoip_mmdb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-mmdb") && strcmp(ce->name, "geoip-maxmind"))
		return 0;

	geoip_mmdb_config.have_config = 1;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database"))
		{
			if (geoip_mmdb_config.have_database)
			{
				config_error("%s:%i: duplicate item set::geoip-mmdb::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-mmdb::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_mmdb_config.have_database = 1;
			continue;
		}
		if (!strcmp(cep->name, "asn-database"))
		{
			if (geoip_mmdb_config.have_asn_database)
			{
				config_error("%s:%i: duplicate item set::geoip-mmdb::%s", cep->file->filename, cep->line_number, cep->name);
				continue;
			}
			if (!is_file_readable(cep->value, PERMDATADIR))
			{
				config_error("%s:%i: set::geoip-mmdb::%s: cannot open file \"%s/%s\" for reading (%s)", cep->file->filename, cep->line_number, cep->name, PERMDATADIR, cep->value, strerror(errno));
				errors++;
				continue;
			}
			geoip_mmdb_config.have_asn_database = 1;
			continue;
		}
		config_warn("%s:%i: unknown item set::geoip-mmdb::%s", cep->file->filename, cep->line_number, cep->name);
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_mmdb_configposttest(int *errs)
{
	int errors = 0;
	if (geoip_mmdb_config.have_config)
	{
		if (!geoip_mmdb_config.have_database)
		{
			config_error("geoip_mmdb: no working database file specified! Remove set::geoip-mmdb to use defaults");
			errors++;
		}
		if (!geoip_mmdb_config.have_asn_database)
			safe_free(geoip_mmdb_config.asn_db_file); /* at this point we aren't going to use ASN at all */

	} else
	{
		safe_strdup(geoip_mmdb_config.db_file, "GeoLite2-Country.mmdb");
		safe_strdup(geoip_mmdb_config.asn_db_file, "GeoLite2-ASN.mmdb");

		if (is_file_readable(geoip_mmdb_config.db_file, PERMDATADIR))
		{
			geoip_mmdb_config.have_database = 1;
		} else
		{
			config_warn("[geoip_mmdb] cannot open database file \"%s/%s\" for reading (%s)", PERMDATADIR, geoip_mmdb_config.db_file, strerror(errno));
			safe_free(geoip_mmdb_config.db_file);
		}

		if (is_file_readable(geoip_mmdb_config.asn_db_file, PERMDATADIR))
			geoip_mmdb_config.have_asn_database = 1;
		else
			safe_free(geoip_mmdb_config.asn_db_file);
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int geoip_mmdb_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	if (type != CONFIG_SET)
		return 0;

	if (!ce || !ce->name)
		return 0;

	if (strcmp(ce->name, "geoip-mmdb") && strcmp(ce->name, "geoip-maxmind"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database") && geoip_mmdb_config.have_database)
			safe_strdup(geoip_mmdb_config.db_file, cep->value);
		if (!strcmp(cep->name, "asn-database") && geoip_mmdb_config.have_asn_database)
			safe_strdup(geoip_mmdb_config.asn_db_file, cep->value);
	}
	return 1;
}

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (!CallbackAddPVoid(modinfo->handle, CALLBACKTYPE_GEOIP_LOOKUP, TO_PVOIDFUNC(geoip_lookup_mmdb)))
	{
		unreal_log(ULOG_ERROR, "geoip_mmdb", "GEOIP_ADD_CALLBACK_FAILED", NULL,
		           "geoip_mmdb: Could not install GEOIP_LOOKUP callback. "
		           "Most likely another geoip module is already loaded. "
		           "You can only load one!");
		return MOD_FAILED;
	}

	geoip_mmdb_config.have_config = 0;
	geoip_mmdb_config.have_database = 0;
	geoip_mmdb_config.have_asn_database = 0;
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, geoip_mmdb_configtest);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, geoip_mmdb_configposttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, geoip_mmdb_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	int status;

	geoip_mmdb_free();

	if (geoip_mmdb_config.db_file)
	{
		convert_to_absolute_path(&geoip_mmdb_config.db_file, PERMDATADIR);
		status = mmdb_open(&mmdb, geoip_mmdb_config.db_file);
		if (status != MMDB_OK)
		{
			unreal_log(ULOG_WARNING, "geoip_mmdb", "GEOIP_CANNOT_OPEN_DB", NULL,
			           "Could not open '$filename' - $mmdb_error",
			           log_data_string("filename", geoip_mmdb_config.db_file),
			           log_data_string("mmdb_error", mmdb_strerror(status)));
			geoip_mmdb_config.have_database = 0;
		}
	}

	if (geoip_mmdb_config.asn_db_file)
	{
		convert_to_absolute_path(&geoip_mmdb_config.asn_db_file, PERMDATADIR);
		status = mmdb_open(&asn_mmdb, geoip_mmdb_config.asn_db_file);
		if (status != MMDB_OK)
		{
			unreal_log(ULOG_WARNING, "geoip_mmdb", "GEOIP_CANNOT_OPEN_ASN_DB", NULL,
			           "Could not open '$filename' - $mmdb_error",
			           log_data_string("filename", geoip_mmdb_config.asn_db_file),
			           log_data_string("mmdb_error", mmdb_strerror(status)));
			geoip_mmdb_config.have_asn_database = 0;
		}
	}

	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	geoip_mmdb_free();
	safe_free(geoip_mmdb_config.db_file);
	safe_free(geoip_mmdb_config.asn_db_file);
	return MOD_SUCCESS;
}

void geoip_mmdb_free(void)
{
	mmdb_close(&mmdb);
	mmdb_close(&asn_mmdb);
}

GeoIPResult *geoip_lookup_mmdb(char *ip)
{
	MMDB_Status status;
	MMDB_Result result;
	GeoIPResult *r;

	if (!ip)
		return NULL;

	if (!geoip_mmdb_config.have_database)
		return NULL;

	/* Country database */
	status = mmdb_lookup(&mmdb, ip, &result);
	if (status != MMDB_OK)
	{
		unreal_log(ULOG_DEBUG, "geoip_mmdb", "GEOIP_DB_ERROR", NULL,
		           "mmdb: lookup error for $ip: $error",
		           log_data_string("ip", ip),
		           log_data_string("error", mmdb_strerror(status)));
		return NULL;
	}

	if (!result.has_data) /* no result */
		return NULL;

	r = safe_alloc(sizeof(GeoIPResult));

	if (mmdb_get_str(&result, &r->country_code, "country", "iso_code") != MMDB_OK)
	{
		free_geoip_result(r);
		return NULL;
	}

	if (mmdb_get_str(&result, &r->country_name, "country", "names", "en") != MMDB_OK)
	{
		free_geoip_result(r);
		return NULL;
	}

	/* No ASN database? Then we are done. */
	if (!geoip_mmdb_config.have_asn_database)
		return r;

	status = mmdb_lookup(&asn_mmdb, ip, &result);

	if (status != MMDB_OK)
	{
		unreal_log(ULOG_DEBUG, "geoip_mmdb", "GEOIP_ASN_DB_ERROR", NULL,
		           "mmdb: lookup error for $ip: $error",
		           log_data_string("ip", ip),
		           log_data_string("error", mmdb_strerror(status)));
		return r;
	}

	if (!result.has_data)
		return r; /* no ASN result, we are done. */

	if (mmdb_get_uint32(&result, &r->asn, "autonomous_system_number") != MMDB_OK)
		return r;

	if (mmdb_get_str(&result, &r->asname, "autonomous_system_organization") != MMDB_OK)
		return r;

	return r;
}
