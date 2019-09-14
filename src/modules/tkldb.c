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
	"unrealircd-5",
};

#define TKL_DB_MAGIC 0x10101010
#define TKL_DB_VERSION 4999
#define TKL_DB_SAVE_EVERY 299

#ifdef DEBUGMODE
 #define BENCHMARK
/* Benchmark results (2GHz Xeon Skylake, compiled with -O2, Linux):
 * 100,000 zlines:
 * - load db: 510 ms
 * - save db:  72 ms
 * Thus, saving does not take much time and can be done by a timer
 * which executes every 5 minutes.
 * Of course, exact figures will depend on the machine.
 */
#endif

#define FreeTKLRead() \
 	do { \
		/* Some of these might be NULL */ \
		if (tkl) \
			free_tkl(tkl); \
	} while(0)

#define R_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[tkldb] Read error from database file '%s' (possible corruption): %s", cfg.database, strerror(errno)); \
			fclose(fd); \
			FreeTKLRead(); \
			return 0; \
		} \
	} while(0)

#define W_SAFE(x) \
	do { \
		if (!(x)) { \
			config_warn("[tkldb] Error writing to temporary database file '%s': %s. DATABASE NOT SAVED!", tmpfname, strerror(errno)); \
			fclose(fd); \
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

/* Forward declarations */
void tkldb_moddata_free(ModData *md);
void setcfg(void);
void freecfg(void);
int tkldb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int tkldb_configrun(ConfigFile *cf, ConfigEntry *ce, int type);
EVENT(write_tkldb_evt);
int write_tkldb(void);
int write_tkline(FILE *fd, const char *tmpfname, TKL *tkl);
int read_tkldb(void);

/* Globals variables */
const uint32_t tkl_db_version = TKL_DB_VERSION;
struct cfgstruct {
	char *database;
};
static struct cfgstruct cfg;

static int tkls_loaded = 0;

MOD_TEST()
{
	memset(&cfg, 0, sizeof(cfg));
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, tkldb_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	LoadPersistentInt(modinfo, tkls_loaded);

	setcfg();

	if (!tkls_loaded)
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
		tkls_loaded = 1;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, tkldb_configrun);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	EventAdd(modinfo->handle, "tkldb_write_tkldb", TKL_DB_SAVE_EVERY, 0, write_tkldb_evt, NULL);
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	write_tkldb();
	freecfg();
	SavePersistentInt(modinfo, tkls_loaded);
	return MOD_SUCCESS;
}

void tkldb_moddata_free(ModData *md)
{
	if (md->i)
		md->i = 0;
}

void setcfg(void)
{
	// Default: data/tkl.db
	safe_strdup(cfg.database, "tkl.db");
	convert_to_absolute_path(&cfg.database, PERMDATADIR);
}

void freecfg(void)
{
	safe_free(cfg.database);
}

int tkldb_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "tkldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!cep->ce_vardata) {
			config_error("%s:%i: blank set::tkldb::%s without value", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}
		if (!strcmp(cep->ce_varname, "database")) {
			convert_to_absolute_path(&cep->ce_vardata, PERMDATADIR);
			continue;
		}
		config_error("%s:%i: unknown directive set::tkldb::%s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int tkldb_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::tkldb::database
	if (type != CONFIG_SET)
		return 0;

	if (!ce || strcmp(ce->ce_varname, "tkldb"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "database"))
			safe_strdup(cfg.database, cep->ce_vardata);
	}
	return 1;
}

EVENT(write_tkldb_evt)
{
	write_tkldb();
}

int write_tkldb(void)
{
	char tmpfname[512];
	FILE *fd;
	uint64_t tklcount;
	int index, index2;
	TKL *tkl;
#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	// Write to a tempfile first, then rename it if everything succeeded
	snprintf(tmpfname, sizeof(tmpfname), "%s.tmp", cfg.database);
	fd = fopen(tmpfname, "wb");
	if (!fd)
	{
		config_warn("[tkldb] Unable to open temporary database file '%s' for writing: %s. DATABASE NOT SAVED!", tmpfname, strerror(errno));
		return 0;
	}

	W_SAFE(write_int32(fd, TKL_DB_MAGIC));
	W_SAFE(write_data(fd, &tkl_db_version, sizeof(tkl_db_version)));

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
	W_SAFE(write_data(fd, &tklcount, sizeof(tklcount)));

	// Now write the actual *-Lines, first the ones in the hash table
	for (index = 0; index < TKLIPHASHLEN1; index++)
	{
		for (index2 = 0; index2 < TKLIPHASHLEN2; index2++)
		{
			for (tkl = tklines_ip_hash[index][index2]; tkl; tkl = tkl->next)
			{
				if (tkl->flags & TKL_FLAG_CONFIG)
					continue; /* config entry */
				if (!write_tkline(fd, tmpfname, tkl)) // write_tkline() closes the fd on errors itself
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
			if (!write_tkline(fd, tmpfname, tkl))
				return 0;
		}
	}

	// Everything seems to have gone well, attempt to close and rename the tempfile
	if (fclose(fd) != 0)
	{
		config_warn("[tkldb] Got an error when trying to close database file '%s' (possible corruption occurred, DATABASE NOT SAVED): %s", cfg.database, strerror(errno));
		return 0;
	}
	if (rename(tmpfname, cfg.database) < 0)
	{
		config_warn("[tkldb] Error renaming '%s' to '%s': %s (DATABASE NOT SAVED)", tmpfname, cfg.database, strerror(errno));
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
int write_tkline(FILE *fd, const char *tmpfname, TKL *tkl)
{
	char tkltype;
	char buf[256];

	/* First, write the common attributes */
	tkltype = tkl_typetochar(tkl->type);
	W_SAFE(write_data(fd, &tkltype, sizeof(tkltype))); // TKL char

	W_SAFE(write_str(fd, tkl->set_by));
	W_SAFE(write_int64(fd, tkl->set_at));
	W_SAFE(write_int64(fd, tkl->expire_at));

	if (TKLIsServerBan(tkl))
	{
		char *usermask = tkl->ptr.serverban->usermask;
		if (tkl->ptr.serverban->subtype & TKL_SUBTYPE_SOFT)
		{
			snprintf(buf, sizeof(buf), "%%%s", tkl->ptr.serverban->usermask);
			usermask = buf;
		}
		W_SAFE(write_str(fd, usermask));
		W_SAFE(write_str(fd, tkl->ptr.serverban->hostmask));
		W_SAFE(write_str(fd, tkl->ptr.serverban->reason));
	} else
	if (TKLIsNameBan(tkl))
	{
		char *hold = tkl->ptr.nameban->hold ? "H" : "*";
		W_SAFE(write_str(fd, hold));
		W_SAFE(write_str(fd, tkl->ptr.nameban->name));
		W_SAFE(write_str(fd, tkl->ptr.nameban->reason));
	} else
	if (TKLIsSpamfilter(tkl))
	{
		char *match_type = unreal_match_method_valtostr(tkl->ptr.spamfilter->match->type);
		char *target = spamfilter_target_inttostring(tkl->ptr.spamfilter->target);
		char action = banact_valtochar(tkl->ptr.spamfilter->action);

		W_SAFE(write_str(fd, match_type));
		W_SAFE(write_str(fd, tkl->ptr.spamfilter->match->str));
		W_SAFE(write_str(fd, target));
		W_SAFE(write_data(fd, &action, sizeof(action)));
		W_SAFE(write_str(fd, tkl->ptr.spamfilter->tkl_reason));
		W_SAFE(write_int64(fd, tkl->ptr.spamfilter->tkl_duration));
	}

	return 1;
}

/** Read all entries from the TKL db */
int read_tkldb(void)
{
	FILE *fd;
	TKL *tkl = NULL;
	uint32_t magic = 0;
	uint64_t cnt;
	uint64_t tklcount = 0;
	uint32_t version;
	int added_cnt = 0;
	char c;
	char *str;

#ifdef BENCHMARK
	struct timeval tv_alpha, tv_beta;

	gettimeofday(&tv_alpha, NULL);
#endif

	fd = fopen(cfg.database, "rb");
	if (!fd)
	{
		if (errno == ENOENT)
		{
			/* Database does not exist. Could be first boot */
			config_warn("[tkldb] No database present at '%s', will start a new one", cfg.database);
			return 1;
		} else {
			config_warn("[tkldb] Unable to open database file '%s' for reading: %s", cfg.database, strerror(errno));
			return 0;
		}
	}

	/* The database starts with a "magic value" - unless it's some old version or corrupt */
	R_SAFE(read_data(fd, &magic, sizeof(magic)));
	if (magic != TKL_DB_MAGIC)
	{
		config_warn("[tkldb] Database '%s' uses an old and unsupported format OR is corrupt", cfg.database);
		config_status("If you are upgrading from UnrealIRCd 4 (or 5.0.0-alpha1) then we suggest you to "
		              "delete the existing database. Just keep at least 1 server linked during the upgrade "
		              "process to preserve your global *LINES and Spamfilters.");
		fclose(fd);
		return 0;
	}

	/* Now do a version check */
	R_SAFE(read_data(fd, &version, sizeof(version)));
	if (version < 4999)
	{
		config_warn("[tkldb] Database '%s' uses an unsupport - possibly old - format (%ld).", cfg.database, (long)version);
		fclose(fd);
		return 0;
	}
	if (version > tkl_db_version)
	{
		config_warn("[tkldb] Database '%s' has version %lu while we only support %lu. Did you just downgrade UnrealIRCd? Sorry this is not suported",
			cfg.database, (unsigned long)tkl_db_version, (unsigned long)version);
		fclose(fd);
		return 0;
	}

	R_SAFE(read_data(fd, &tklcount, sizeof(tklcount)));

	for (cnt = 0; cnt < tklcount; cnt++)
	{
		int do_not_add = 0;

		tkl = safe_alloc(sizeof(TKL));

		/* First, fetch the TKL type.. */
		R_SAFE(read_data(fd, &c, sizeof(c)));
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
		R_SAFE(read_str(fd, &tkl->set_by));
		R_SAFE(read_int64(fd, &tkl->set_at));
		R_SAFE(read_int64(fd, &tkl->expire_at));

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
			R_SAFE(read_str(fd, &str));
			if (*str == '%')
			{
				softban = 1;
				safe_strdup(tkl->ptr.serverban->usermask, str+1);
			} else {
				safe_strdup(tkl->ptr.serverban->usermask, str);
			}
			safe_free(str);

			/* And the other 2 fields.. */
			R_SAFE(read_str(fd, &tkl->ptr.serverban->hostmask));
			R_SAFE(read_str(fd, &tkl->ptr.serverban->reason));

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
		if (TKLIsNameBan(tkl))
		{
			tkl->ptr.nameban = safe_alloc(sizeof(NameBan));

			R_SAFE(read_str(fd, &str));
			if (*str == 'H')
				tkl->ptr.nameban->hold = 1;
			safe_free(str);
			R_SAFE(read_str(fd, &tkl->ptr.nameban->name));
			R_SAFE(read_str(fd, &tkl->ptr.nameban->reason));

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
			R_SAFE(read_str(fd, &str));
			match_method = unreal_match_method_strtoval(str);
			if (!match_method)
			{
				config_warn("[tkldb] Unhandled spamfilter match method '%s' -- spamfilter entry not added", str);
				do_not_add = 1;
			}
			safe_free(str);

			/* Match string (eg: regex) */
			R_SAFE(read_str(fd, &str));
			tkl->ptr.spamfilter->match = unreal_create_match(match_method, str, &err);
			if (!tkl->ptr.spamfilter->match)
			{
				config_warn("[tkldb] Spamfilter '%s' does not compile: %s -- spamfilter entry not added", str, err);
				do_not_add = 1;
			}
			safe_free(str);

			/* Target (eg: cpn) */
			R_SAFE(read_str(fd, &str));
			tkl->ptr.spamfilter->target = spamfilter_gettargets(str, NULL);
			if (!tkl->ptr.spamfilter->target)
			{
				config_warn("[tkldb] Spamfilter '%s' without any valid targets (%s) -- spamfilter entry not added",
					tkl->ptr.spamfilter->match->str, str);
				do_not_add = 1;
			}
			safe_free(str);

			/* Action */
			R_SAFE(read_data(fd, &c, sizeof(c)));
			tkl->ptr.spamfilter->action = banact_chartoval(c);
			if (!tkl->ptr.spamfilter->action)
			{
				config_warn("[tkldb] Spamfilter '%s' without valid action (%c) -- spamfilter entry not added",
					tkl->ptr.spamfilter->match->str, c);
				do_not_add = 1;
			}

			R_SAFE(read_str(fd, &tkl->ptr.spamfilter->tkl_reason));
			R_SAFE(read_int64(fd, &tkl->ptr.spamfilter->tkl_duration));

			if (find_tkl_spamfilter(tkl->type, tkl->ptr.spamfilter->match->str,
			                        tkl->ptr.spamfilter->action,
			                        tkl->ptr.spamfilter->target))
			{
				do_not_add = 1;
			}

			if (!do_not_add)
			{
				tkl_add_spamfilter(tkl->type, tkl->ptr.spamfilter->target,
				                   tkl->ptr.spamfilter->action,
				                   tkl->ptr.spamfilter->match,
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

	/* If everything went fine, then reading a single byte should cause an EOF error */
	if (fread(&c, 1, 1, fd) == 1)
		config_warn("[tkldb] Database invalid. Extra data found at end of DB file.");
	fclose(fd);

	if (added_cnt)
		config_status("[tkldb] Re-added %d *-Lines", added_cnt);

#ifdef BENCHMARK
	gettimeofday(&tv_beta, NULL);
	ircd_log(LOG_ERROR, "[tkldb] Benchmark: LOAD DB: %lld microseconds",
		(long long)(((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec)));
#endif
	return 1;
}

