/*
 * Stores active *-Lines (G-Lines etc) inside a .db file for persistency
 * (C) Copyright 2019 Gottem and the UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

ModuleHeader MOD_HEADER = {
	"tkldb",
	"1.10",
	"Stores active TKL entries (*-Lines) persistently/across IRCd restarts",
	"UnrealIRCd Team",
	"unrealircd-6",
};

#define TKLDB_MAGIC 0x10101010
/* Database version */
#define TKLDB_VERSION 4999
/* Save tkls to file every <this> seconds */
#define TKLDB_SAVE_EVERY 300
/* The very first save after boot, apply this delta, this
 * so we don't coincide with other (potentially) expensive
 * I/O events like saving channeldb.
 */
#define TKLDB_SAVE_EVERY_DELTA +15

// #undef BENCHMARK
/* Benchmark results (2GHz Xeon Skylake, compiled with -O2, Linux):
 * 100,000 zlines:
 * - load db: 510 ms
 * - save db:  72 ms
 * Thus, saving does not take much time and can be done by a timer
 * which executes every 5 minutes.
 * Of course, exact figures will depend on the machine.
 */

#define FreeTKLRead() \
 	do { \
		/* Some of these might be NULL */ \
		if (tkl) \
			free_tkl(tkl); \
	} while(0)

#define WARN_WRITE_ERROR(fname) \
	do { \
		unreal_log(ULOG_ERROR, "tkldb", "TKLDB_FILE_WRITE_ERROR", NULL, \
			   "[tkldb] Error writing to temporary database file $filename: $system_error", \
			   log_data_string("filename", fname), \
			   log_data_string("system_error", unrealdb_get_error_string())); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[tkldb] Read error from database file '%s' (possible corruption): %s", cfg.database, unrealdb_get_error_string()); \
			unrealdb_close(db); \
			FreeTKLRead(); \
			return 0; \
		} \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
			WARN_WRITE_ERROR(tmpfname); \
			unrealdb_close(db); \
			return 0; \
		} \
	} while(0)

#define IsMDErr(x, y, z) \
	do { \
		if (!(x)) { \
			config_error("A critical error occurred when registering ModData for %s: %s", MOD_HEADER.name, ModuleGetErrorStr((z)->handle)); \
			return MOD_FAILED; \
		} \
	} while(0)

/* Structs */
struct cfgstruct {
	char *database;
	char *db_secret;
};

/* Forward declarations */
void tkldb_moddata_free(ModData *md);
void setcfg(struct cfgstruct *cfg);
void freecfg(struct cfgstruct *cfg);
int tkldb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int tkldb_config_posttest(int *errs);
int tkldb_config_run(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(write_tkldb_evt);
int write_tkldb(void);
int write_tkline(UnrealDB *db, const char *tmpfname, TKL *tkl);
int read_tkldb(void);

/* Globals variables */
const uint32_t tkldb_version = TKLDB_VERSION;
static struct cfgstruct cfg;
static struct cfgstruct test;

static long tkldb_next_event = 0;

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	memset(&test, 0, sizeof(test));
	setcfg(&test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkldb_config_test);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGPOSTTEST, 0, tkldb_config_posttest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModuleSetOptions(modinfo->handle, MOD_OPT_PRIORITY, -9999);

	LoadPersistentLong(modinfo, tkldb_next_event);

	setcfg(&cfg);

	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkldb_config_run);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!tkldb_next_event)
	{
		/* If this is the first time that our module is loaded, then
		 * read the TKL DB and add all *-Lines.
		 */
		if (!read_tkldb())
		{
			char fname[512];
			snprintf(fname, sizeof(fname), "%s.corrupt", cfg.database);
			if (rename(cfg.database, fname) == 0)
				config_warn("[tkldb] Existing database renamed to %s and starting a new one...", fname);
			else
				config_warn("[tkldb] Failed to rename database from %s to %s: %s", cfg.database, fname, strerror(errno));
		}
		tkldb_next_event = TStime() + TKLDB_SAVE_EVERY + TKLDB_SAVE_EVERY_DELTA;
	}
	EventAdd(modinfo->handle, "tkldb_write_tkldb", write_tkldb_evt, NULL, 1000, 0);
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	if (loop.terminating)
		write_tkldb();
	freecfg(&test);
	freecfg(&cfg);
	SavePersistentLong(modinfo, tkldb_next_event);
	return MOD_SUCCESS;
}

void tkldb_moddata_free(ModData *md)
{
	if (md->i)
		md->i = 0;
}

void setcfg(struct cfgstruct *cfg)
{
	// Default: data/tkl.db
	safe_strdup(cfg->database, "tkl.db");
	convert_to_absolute_path(&cfg->database, PERMDATADIR);
}

void freecfg(struct cfgstruct *cfg)
{
	safe_free(cfg->database);
	safe_free(cfg->db_secret);
}

int tkldb_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "tkldb"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!cep->value)
		{
			config_error("%s:%i: blank set::tkldb::%s without value", cep->file->filename, cep->line_number, cep->name);
			errors++;
		} else
		if (!strcmp(cep->name, "database"))
		{
			convert_to_absolute_path(&cep->value, PERMDATADIR);
			safe_strdup(test.database, cep->value);
		} else
		if (!strcmp(cep->name, "db-secret"))
		{
			const char *err;
			if ((err = unrealdb_test_secret(cep->value)))
			{
				config_error("%s:%i: set::tkldb::db-secret: %s", cep->file->filename, cep->line_number, err);
				errors++;
				continue;
			}
			safe_strdup(test.db_secret, cep->value);
		} else
		{
			config_error("%s:%i: unknown directive set::tkldb::%s", cep->file->filename, cep->line_number, cep->name);
			errors++;
		}
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int tkldb_config_posttest(int *errs)
{
	int errors = 0;
	char *errstr;

	if (test.database && ((errstr = unrealdb_test_db(test.database, test.db_secret))))
	{
		config_error("[tkldb] %s", errstr);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int tkldb_config_run(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->name, "tkldb"))
		return 0;

	for (cep = ce->items; cep; cep = cep->next)
	{
		if (!strcmp(cep->name, "database"))
			safe_strdup(cfg.database, cep->value);
		else if (!strcmp(cep->name, "db-secret"))
			safe_strdup(cfg.db_secret, cep->value);
	}
	return 1;
}

EVENT(write_tkldb_evt)
{
	if (tkldb_next_event > TStime())
		return;
	tkldb_next_event = TStime() + TKLDB_SAVE_EVERY;
	write_tkldb();
}

int write_tkldb(void)
{
	char tmpfname[512];
	UnrealDB *db;
	uint64_t tklcount;
	int index, index2;
	TKL *tkl;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	// Write to a tempfile first, then rename it if everything succeeded
	snprintf(tmpfname, sizeof(tmpfname), "%s.%x.tmp", cfg.database, getrandom32());
	db = unrealdb_open(tmpfname, UNREALDB_MODE_WRITE, cfg.db_secret);
	if (!db)
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}

	W_SAFE(unrealdb_write_int32(db, TKLDB_MAGIC));
	W_SAFE(unrealdb_write_int32(db, tkldb_version));

	// Count the *-Lines
	tklcount = 0;

	// First the ones in the hash table
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				if (tkl->flags & TKL_FLAG_CONFIG)
					continue; /* config entry */
				tklcount++;
			}
		}
	}
	// Then the regular *-Lines
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			if (tkl->flags & TKL_FLAG_CONFIG)
				continue; /* config entry */
			tklcount++;
		}
	}
	W_SAFE(unrealdb_write_int64(db, tklcount));

	// Now write the actual *-Lines, first the ones in the hash table
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				if (tkl->flags & TKL_FLAG_CONFIG)
					continue; /* config entry */
				if (!write_tkline(db, tmpfname, tkl)) // write_tkline() closes the db on errors itself
					return 0;
			}
		}
	}
	// Then the regular *-Lines
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (tkl = tklines[index]; tkl; tkl = tkl->next)
		{
			if (tkl->flags & TKL_FLAG_CONFIG)
				continue; /* config entry */
			if (!write_tkline(db, tmpfname, tkl))
				return 0;
		}
	}

	// Everything seems to have gone well, attempt to close and rename the tempfile
	if (!unrealdb_close(db))
	{
		WARN_WRITE_ERROR(tmpfname);
		return 0;
	}
#ifdef _WIN32
	/* The rename operation cannot be atomic on Windows as it will cause a "file exists" error */
	unlink(cfg.database);
#endif
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_error("[tkldb] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
		return 0;
	}
#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	config_status("[tkldb] Benchmark: SAVE DB: %lld microseconds",
		(long long)(((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}

/** Write a TKL entry */
int write_tkline(UnrealDB *db, const char *tmpfname, TKL *tkl)
{
	char tkltype;
	char buf[256];

	/* First, write the common attributes */
	tkltype = tkl_typetochar(tkl->type);
	W_SAFE(unrealdb_write_char(db, tkltype)); // TKL char

	W_SAFE(unrealdb_write_str(db, tkl->set_by));
	W_SAFE(unrealdb_write_int64(db, tkl->set_at));
	W_SAFE(unrealdb_write_int64(db, tkl->expire_at));

	if (TKLIsServerBan(tkl))
	{
		char *usermask = tkl->ptr.serverban->usermask;
		if (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT)
		{
			snprintf(buf, sizeof(buf), "%%%s", tkl->ptr.serverban->usermask);
			usermask = buf;
		}
		W_SAFE(unrealdb_write_str(db, usermask));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.serverban->hostmask));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.serverban->reason));
	} else
	if (TKLIsBanException(tkl))
	{
		char *usermask = tkl->ptr.banexception->usermask;
		if (tkl->ptr.banexception->subtype & TKL_SUBTYPE_SOFT)
		{
			snprintf(buf, sizeof(buf), "%%%s", tkl->ptr.banexception->usermask);
			usermask = buf;
		}
		W_SAFE(unrealdb_write_str(db, usermask));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.banexception->hostmask));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.banexception->bantypes));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.banexception->reason));
	} else
	if (TKLIsNameBan(tkl))
	{
		char *hold = tkl->ptr.nameban->hold ? "H" : "*";
		W_SAFE(unrealdb_write_str(db, hold));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.nameban->name));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.nameban->reason));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		char *match_type = unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type);
		char *target = spamfilter_target_inttostring(tkl->ptr.spamfilter->target);
		char action = banact_valtochar(tkl->ptr.spamfilter->action->action);

		W_SAFE(unrealdb_write_str(db, match_type));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.spamfilter->match->str));
		W_SAFE(unrealdb_write_str(db, target));
		W_SAFE(unrealdb_write_char(db, action));
		W_SAFE(unrealdb_write_str(db, tkl->ptr.spamfilter->tkl_reason));
		W_SAFE(unrealdb_write_int64(db, tkl->ptr.spamfilter->tkl_duration));
	}

	return 1;
}

/** Read all entries from the TKL db */
int read_tkldb(void)
{
	UnrealDB *db;
	TKL *tkl = NULL;
	uint32_t magic = 0;
	uint32_t version;
	uint64_t cnt;
	uint64_t tklcount = 0;
	uint64_t v;
	int added_cnt = 0;
	char c;
	char *str;

#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, cfg.db_secret);
	if (!db)
	{
		if (unrealdb_get_error_code() == UNREALDB_ERROR_FILENOTFOUND)
		{
			/* Database does not exist. Could be first boot */
			config_warn("[tkldb] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else
		if (unrealdb_get_error_code() == UNREALDB_ERROR_NOTCRYPTED)
		{
			/* Re-open as unencrypted */
			db = unrealdb_open(cfg.database, UNREALDB_MODE_READ, NULL);
			if (!db)
			{
				/* This should actually never happen, unless some weird I/O error */
				config_warn("[tkldb] Unable to open the database file '%s': %s", cfg.database, unrealdb_get_error_string());
				return 0;
			}
		} else
		{
			config_warn("[tkldb] Unable to open the database file '%s' for reading: %s", cfg.database, unrealdb_get_error_string());
			return 0;
		}
	}

	/* The database starts with a "magic value" - unless it's some old version or corrupt */
	R_SAFE(unrealdb_read_int32(db, &magic));
	if (magic != TKLDB_MAGIC)
	{
		config_warn("[tkldb] Database '%s' uses an old and unsupported format OR is corrupt", cfg.database);
		config_status("If you are upgrading from UnrealIRCd 4 (or 5.0.0-alpha1) then we suggest you to "
		              "delete the existing database. Just keep at least 1 server linked during the upgrade "
		              "process to preserve your global *LINES and Spamfilters.");
		unrealdb_close(db);
		return 0;
	}

	/* Now do a version check */
	R_SAFE(unrealdb_read_int32(db, &version));
	if (version < 4999)
	{
		config_warn("[tkldb] Database '%s' uses an unsupport - possibly old - format (%ld).", cfg.database, (long)version);
		unrealdb_close(db);
		return 0;
	}
	if (version > tkldb_version)
	{
		config_warn("[tkldb] Database '%s' has version %lu while we only support %lu. Did you just downgrade UnrealIRCd? Sorry this is not suported",
			cfg.database, (unsigned long)tkldb_version, (unsigned long)version);
		unrealdb_close(db);
		return 0;
	}

	R_SAFE(unrealdb_read_int64(db, &tklcount));

	for (cnt = 0; cnt < tklcount; cnt++)
	{
		int do_not_add = 0;

		tkl = safe_alloc(sizeof(TKL));

		/* First, fetch the TKL type.. */
		R_SAFE(unrealdb_read_char(db, &c));
		tkl->type = tkl_chartotype(c);
		if (!tkl->type)
		{
			/* We can't continue reading the DB if we don't know the TKL type,
			 * since we don't know how long the entry will be, we can't skip it.
			 * This is "impossible" anyway, unless we some day remove a TKL type
			 * in core UnrealIRCd. In which case we should add some skipping code
			 * here to gracefully handle that situation ;)
			 */
			config_warn("[tkldb] Invalid type '%c' encountered - STOPPED READING DATABASE!", tkl->type);
			FreeTKLRead();
			break; /* we MUST stop reading */
		}

		/* Read the common types (same for all TKLs) */
		R_SAFE(unrealdb_read_str(db, &tkl->set_by));
		R_SAFE(unrealdb_read_int64(db, &v));
		tkl->set_at = v;
		R_SAFE(unrealdb_read_int64(db, &v));
		tkl->expire_at = v;

		/* Save some CPU... if it's already expired then don't bother adding */
		if (tkl->expire_at != 0 && tkl->expire_at <= TStime())
			do_not_add = 1;

		/* Now handle all the specific types */
		if (TKLIsServerBan(tkl))
		{
			int softban = 0;

			tkl->ptr.serverban = safe_alloc(sizeof(ServerBan));

			/* Usermask - but taking into account that the
			 * %-prefix means a soft ban.
			 */
			R_SAFE(unrealdb_read_str(db, &str));
			if (*str == '%')
			{
				softban = 1;
				safe_strdup(tkl->ptr.serverban->usermask, str+1);
			} else {
				safe_strdup(tkl->ptr.serverban->usermask, str);
			}
			safe_free(str);

			/* And the other 2 fields.. */
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.serverban->hostmask));
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.serverban->reason));

			if (find_tkl_serverban(tkl->type, tkl->ptr.serverban->usermask,
			                       tkl->ptr.serverban->hostmask, softban))
			{
				do_not_add = 1;
			}

			if (!do_not_add)
			{
				tkl_add_serverban(tkl->type, tkl->ptr.serverban->usermask,
				                  tkl->ptr.serverban->hostmask,
				                  tkl->ptr.serverban->reason,
				                  tkl->set_by, tkl->expire_at,
				                  tkl->set_at, softban, 0);
			}
		} else
		if (TKLIsBanException(tkl))
		{
			int softban = 0;

			tkl->ptr.banexception = safe_alloc(sizeof(BanException));

			/* Usermask - but taking into account that the
			 * %-prefix means a soft ban.
			 */
			R_SAFE(unrealdb_read_str(db, &str));
			if (*str == '%')
			{
				softban = 1;
				safe_strdup(tkl->ptr.banexception->usermask, str+1);
			} else {
				safe_strdup(tkl->ptr.banexception->usermask, str);
			}
			safe_free(str);

			/* And the other 3 fields.. */
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.banexception->hostmask));
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.banexception->bantypes));
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.banexception->reason));

			if (find_tkl_banexception(tkl->type, tkl->ptr.banexception->usermask,
			                          tkl->ptr.banexception->hostmask, softban))
			{
				do_not_add = 1;
			}

			if (!do_not_add)
			{
				tkl_add_banexception(tkl->type, tkl->ptr.banexception->usermask,
				                     tkl->ptr.banexception->hostmask,
				                     NULL,
				                     tkl->ptr.banexception->reason,
				                     tkl->set_by, tkl->expire_at,
				                     tkl->set_at, softban,
				                     tkl->ptr.banexception->bantypes,
				                     0);
			}
		} else
		if (TKLIsNameBan(tkl))
		{
			tkl->ptr.nameban = safe_alloc(sizeof(NameBan));

			R_SAFE(unrealdb_read_str(db, &str));
			if (*str == 'H')
				tkl->ptr.nameban->hold = 1;
			safe_free(str);
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.nameban->name));
			R_SAFE(unrealdb_read_str(db, &tkl->ptr.nameban->reason));

			if (find_tkl_nameban(tkl->type, tkl->ptr.nameban->name,
			                     tkl->ptr.nameban->hold))
			{
				do_not_add = 1;
			}

			if (!do_not_add)
			{
				tkl_add_nameban(tkl->type, tkl->ptr.nameban->name,
				                tkl->ptr.nameban->hold,
				                tkl->ptr.nameban->reason,
				                tkl->set_by, tkl->expire_at,
				                tkl->set_at, 0);
			}
		} else
		if (TKLIsSpamfilter(tkl))
		{
			int match_method;
			char *err = NULL;

			tkl->ptr.spamfilter = safe_alloc(sizeof(Spamfilter));

			/* Match method */
			R_SAFE(unrealdb_read_str(db, &str));
			match_method = unreal_match_method_strtoval(str);
			if (!match_method)
			{
				config_warn("[tkldb] Unhandled spamfilter match method '%s' -- spamfilter entry not added", str);
				do_not_add = 1;
			}
			safe_free(str);

			/* Match string (eg: regex) */
			R_SAFE(unrealdb_read_str(db, &str));
			tkl->ptr.spamfilter->match = unreal_create_match(match_method, str, &err);
			if (!tkl->ptr.spamfilter->match)
			{
				config_warn("[tkldb] Spamfilter '%s' does not compile: %s -- spamfilter entry not added", str, err);
				do_not_add = 1;
			}
			safe_free(str);

			/* Target (eg: cpn) */
			R_SAFE(unrealdb_read_str(db, &str));
			tkl->ptr.spamfilter->target = spamfilter_gettargets(str, NULL);
			if (!tkl->ptr.spamfilter->target)
			{
				config_warn("[tkldb] Spamfilter '%s' without any valid targets (%s) -- spamfilter entry not added",
					tkl->ptr.spamfilter->match->str, str);
				do_not_add = 1;
			}
			safe_free(str);

			/* Action */
			R_SAFE(unrealdb_read_char(db, &c));
			tkl->ptr.spamfilter->action = banact_value_to_struct(banact_chartoval(c));
			if (!tkl->ptr.spamfilter->action)
			{
				config_warn("[tkldb] Spamfilter '%s' without valid action (%c) -- spamfilter entry not added",
					tkl->ptr.spamfilter->match->str, c);
				do_not_add = 1;
			}

			R_SAFE(unrealdb_read_str(db, &tkl->ptr.spamfilter->tkl_reason));
			R_SAFE(unrealdb_read_int64(db, &v));
			tkl->ptr.spamfilter->tkl_duration = v;

			if (find_tkl_spamfilter(tkl->type, tkl->ptr.spamfilter->match->str,
			                        tkl->ptr.spamfilter->action->action,
			                        tkl->ptr.spamfilter->target))
			{
				do_not_add = 1;
			}

			if (!do_not_add)
			{
				tkl_add_spamfilter(tkl->type, NULL, tkl->ptr.spamfilter->target,
				                   tkl->ptr.spamfilter->action,
				                   tkl->ptr.spamfilter->match,
				                   NULL,
				                   tkl->set_by, tkl->expire_at, tkl->set_at,
				                   tkl->ptr.spamfilter->tkl_duration,
				                   tkl->ptr.spamfilter->tkl_reason,
				                   0);
				/* tkl_add_spamfilter() does not copy the match but assign it.
				 * so set to NULL here to avoid a read-after-free later on.
				 */
				tkl->ptr.spamfilter->match = NULL;
			}
		} else
		{
			config_warn("[tkldb] Unhandled type!! TKLDB is missing support for type %ld -- STOPPED reading db entries!", (long)tkl->type);
			FreeTKLRead();
			break; /* we MUST stop reading */
		}

		if (!do_not_add)
			added_cnt++;

		FreeTKLRead();
	}

	unrealdb_close(db);

	if (added_cnt)
		config_status("[tkldb] Re-added %d *-Lines", added_cnt);

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	unreal_log(ULOG_DEBUG, "tkldb", "TKLDB_BENCHMARK", NULL,
	           "[tkldb] Benchmark: LOAD DB: $time_msec microseconds",
	           log_data_integer("time_msec", ((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}

