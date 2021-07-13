/*
 * Check for modules that are required across the network, as well as modules
 * that *aren't* even allowed (deny/require module { } blocks)
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

#define MSG_SMOD "SMOD"
#define SMOD_FLAG_REQUIRED 'R'
#define SMOD_FLAG_GLOBAL 'G'
#define SMOD_FLAG_LOCAL 'L'

ModuleHeader MOD_HEADER = {
	"require-module",
	"5.0.1",
	"Require/deny modules across the network",
	"UnrealIRCd Team",
	"unrealircd-5",
};

typedef struct _denymod DenyMod;
struct _denymod {
	DenyMod *prev, *next;
	char *name;
	char *reason;
};

typedef struct _requiremod ReqMod;
struct _requiremod {
	ReqMod *prev, *next;
	char *name;
	char *minversion;
};

// Forward declarations
Module *find_modptr_byname(char *name, unsigned strict);
DenyMod *find_denymod_byname(char *name);
ReqMod *find_reqmod_byname(char *name);

int reqmods_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun(ConfigFile *cf, ConfigEntry *ce, int type);

int reqmods_configtest_deny(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun_deny(ConfigFile *cf, ConfigEntry *ce, int type);

int reqmods_configtest_require(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun_require(ConfigFile *cf, ConfigEntry *ce, int type);

CMD_FUNC(cmd_smod);
int reqmods_hook_serverconnect(Client *client);

// Globals
extern MODVAR Module *Modules;
DenyMod *DenyModList = NULL;
ReqMod *ReqModList = NULL;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, reqmods_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	MARK_AS_GLOBAL_MODULE(modinfo);
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGRUN, 0, reqmods_configrun);
	HookAdd(modinfo->handle, HOOKTYPE_SERVER_CONNECT, 0, reqmods_hook_serverconnect);
	CommandAdd(modinfo->handle, MSG_SMOD, cmd_smod, MAXPARA, CMD_SERVER);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("A critical error occurred when loading module %s: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	DenyMod *dmod, *dnext;
	ReqMod *rmod, *rnext;
	for (dmod = DenyModList; dmod; dmod = dnext)
	{
		dnext = dmod->next;
		safe_free(dmod->name);
		safe_free(dmod->reason);
		DelListItem(dmod, DenyModList);
		safe_free(dmod);
	}
	for (rmod = ReqModList; rmod; rmod = rnext)
	{
		rnext = rmod->next;
		safe_free(rmod->name);
		safe_free(rmod->minversion);
		DelListItem(rmod, ReqModList);
		safe_free(rmod);
	}
	DenyModList = NULL;
	ReqModList = NULL;
	return MOD_SUCCESS;
}

Module *find_modptr_byname(char *name, unsigned strict)
{
	Module *mod;
	for (mod = Modules; mod; mod = mod->next)
	{
		// Let's not be too strict with the name
		if (!strcasecmp(mod->header->name, name))
		{
			if (strict && !(mod->flags & MODFLAG_LOADED))
				mod = NULL;
			return mod;
		}
	}
	return NULL;
}

DenyMod *find_denymod_byname(char *name)
{
	DenyMod *dmod;
	for (dmod = DenyModList; dmod; dmod = dmod->next)
	{
		if (!strcasecmp(dmod->name, name))
			return dmod;
	}
	return NULL;
}

ReqMod *find_reqmod_byname(char *name)
{
	ReqMod *rmod;
	for (rmod = ReqModList; rmod; rmod = rmod->next)
	{
		if (!strcasecmp(rmod->name, name))
			return rmod;
	}
	return NULL;
}

int reqmods_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	if (type == CONFIG_DENY)
		return reqmods_configtest_deny(cf, ce, type, errs);

	if (type == CONFIG_REQUIRE)
		return reqmods_configtest_require(cf, ce, type, errs);

	return 0;
}

int reqmods_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type == CONFIG_DENY)
		return reqmods_configrun_deny(cf, ce, type);

	if (type == CONFIG_REQUIRE)
		return reqmods_configrun_require(cf, ce, type);

	return 0;
}

int reqmods_configtest_deny(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_name, has_reason;

	// We are only interested in deny module { }
	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	has_name = has_reason = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strlen(cep->ce_varname))
		{
			config_error("%s:%i: blank directive for deny module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			continue;
		}

		if (!cep->ce_vardata || !strlen(cep->ce_vardata))
		{
			config_error("%s:%i: blank %s without value for deny module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}

		if (!strcmp(cep->ce_varname, "name"))
		{
			if (has_name)
			{
				config_error("%s:%i: duplicate %s for deny module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				continue;
			}

			// We do a loose check here because a module might not be fully loaded yet
			if (find_modptr_byname(cep->ce_vardata, 0))
			{
				config_error("[require-module] Module '%s' was specified as denied but we've actually loaded it ourselves", cep->ce_vardata);
				errors++;
			}
			has_name = 1;
			continue;
		}

		if (!strcmp(cep->ce_varname, "reason")) // Optional
		{
			// Still check for duplicate directives though
			if (has_reason)
			{
				config_error("%s:%i: duplicate %s for deny module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
			has_reason = 1;
			continue;
		}

		config_error("%s:%i: unknown directive %s for deny module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	if (!has_name)
	{
		config_error("%s:%i: missing required 'name' directive for deny module { } block", ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int reqmods_configrun_deny(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	DenyMod *dmod;

	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	dmod = safe_alloc(sizeof(DenyMod));
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "name"))
		{
			safe_strdup(dmod->name, cep->ce_vardata);
			continue;
		}

		if (!strcmp(cep->ce_varname, "reason"))
		{
			safe_strdup(dmod->reason, cep->ce_vardata);
			continue;
		}
	}

	// Just use a default reason if none was specified (since it's optional)
	if (!dmod->reason || !strlen(dmod->reason))
		 safe_strdup(dmod->reason, "no reason");
	AddListItem(dmod, DenyModList);
	return 1;
}

int reqmods_configtest_require(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_name, has_minversion;

	// We are only interested in require module { }
	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	has_name = has_minversion = 0;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strlen(cep->ce_varname))
		{
			config_error("%s:%i: blank directive for require module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			continue;
		}

		if (!cep->ce_vardata || !strlen(cep->ce_vardata))
		{
			config_error("%s:%i: blank %s without value for require module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}

		if (!strcmp(cep->ce_varname, "name"))
		{
			if (has_name)
			{
				config_error("%s:%i: duplicate %s for require module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				continue;
			}

			if (!find_modptr_byname(cep->ce_vardata, 0))
			{
				config_error("[require-module] Module '%s' was specified as required but we didn't even load it ourselves (maybe double check the name?)", cep->ce_vardata);
				errors++;
			}

			// Let's be nice and let configrun handle adding this module to the list
			has_name = 1;
			continue;
		}

		if (!strcmp(cep->ce_varname, "min-version")) // Optional
		{
			// Still check for duplicate directives though
			if (has_minversion)
			{
				config_error("%s:%i: duplicate %s for require module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
			has_minversion = 1;
			continue;
		}

		// Reason directive is not used for require module { }, so error on that too
		config_error("%s:%i: unknown directive %s for require module { } block", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	if (!has_name)
	{
		config_error("%s:%i: missing required 'name' directive for require module { } block", ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int reqmods_configrun_require(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;
	Module *mod;
	ReqMod *rmod;
	char *name, *minversion;

	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	name = minversion = NULL;
	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "name"))
		{
			if (!(mod = find_modptr_byname(cep->ce_vardata, 0)))
			{
				// Something went very wrong :D
				config_warn("[require-module] [BUG?] Passed configtest_require() but not configrun_require() for module '%s' (seems to not be loaded after all)", cep->ce_vardata);
				continue;
			}

			name = cep->ce_vardata;
			continue;
		}

		if (!strcmp(cep->ce_varname, "min-version"))
		{
			minversion = cep->ce_vardata;
			continue;
		}
	}

	// While technically an error, let's not kill the entire server over it
	if (!name)
		return 1;

	rmod = safe_alloc(sizeof(ReqMod));
	safe_strdup(rmod->name, name);
	if (minversion)
		safe_strdup(rmod->minversion, minversion);
	AddListItem(rmod, ReqModList);
	return 1;
}

CMD_FUNC(cmd_smod)
{
	char modflag, name[64], *version;
	char buf[BUFSIZE];
	char *tmp, *p, *modbuf;
	Module *mod;
	DenyMod *dmod;
	int i;
	int abort;

	// A non-server client shouldn't really be possible here, but still :D
	if (!MyConnect(client) || !IsServer(client) || BadPtr(parv[1]))
		return;

	// Module strings are passed as 1 space-delimited parameter
	strlcpy(buf, parv[1], sizeof(buf));
	abort = 0;
	for (modbuf = strtoken(&tmp, buf, " "); modbuf; modbuf = strtoken(&tmp, NULL, " "))
	{
		/* The order of checks is:
		 * 1: deny module { } -- SQUIT always
		 * 2 (if module not loaded): require module { } -- SQUIT always
		 * 3 (if module not loaded): warn, but only if MOD_OPT_GLOBAL
		 * 4 (optional, if module loaded only): require module::min-version
		 */
		p = strchr(modbuf, ':');
		if (!p)
			continue; /* malformed request */
		modflag = *modbuf; // Get the module flag (FIXME: parses only first letter atm)
		modbuf = p+1;
		strlcpy(name, modbuf, sizeof(name)); // Let's work on a copy of the param

		version = strchr(name, ':');
		if (!version)
			continue; /* malformed request */
		*version++ = '\0';

		// Even if a denied module is only required locally, let's still prevent a server that uses it from linking in
		if ((dmod = find_denymod_byname(name)))
		{
			// Send this particular notice to local opers only
			sendto_umode_global(UMODE_OPER, "Server %s is using module '%s', which is specified in a deny module { } config block (reason: %s)", client->name, name, dmod->reason);
			abort = 1; // Always SQUIT because it was explicitly denied by admins
			continue;
		}

		// Doing a strict check for the module being fully loaded so we can emit an alert in that case too :>
		mod = find_modptr_byname(name, 1);
		if (!mod)
		{
			/* Since only the server missing the module will report it, we need to broadcast the warning network-wide ;]
			 * Obviously we won't take any real action if the module seems to be locally required only, except if it's marked as required
			 */
			if (modflag == 'R')
			{
				// We don't need to check the version yet because there's nothing to compare it to, so we'll treat it as if no require module::min-version was specified
				sendto_umode_global(UMODE_OPER, "Required module wasn't (fully) loaded or is missing entirely: %s", name);
				abort = 1; // Always SQUIT here too (explicitly required by admins)
			}

			else if (modflag == 'G')
				sendto_umode_global(UMODE_OPER, "[WARN] Module marked as global wasn't (fully) loaded or is missing entirely: %s", name);

			continue;
		}

		// Further checks are only necessary for explicitly required mods
		if (modflag != 'R')
			continue;

		// Module is loaded on both servers and the other end is require { }'ing a specific module version
		// An explicit version was specified in require module { } but our module version is less than that
		if (*version != '*' && strnatcasecmp(mod->header->version, version) < 0)
		{
			sendto_umode_global(UMODE_OPER, "Module version mismatch for required module '%s' (should be equal to or greater than %s but we're running %s)", name, version, mod->header->version);
			abort = 1;
		}
	}

	if (abort)
	{
		sendto_umode_global(UMODE_OPER, "ABORTING LINK: %s <=> %s", me.name, client->name);
		exit_client(client, NULL, "ABORTING LINK");
		return;
	}
}

int reqmods_hook_serverconnect(Client *client)
{
	/* This function simply dumps a list of modules and their version to the other server,
	 * which will then run through the received list and check the names/versions
	 */
	char modflag;
	char modbuf[64];
	char *modversion;
	/* Try to use a large buffer, but take into account the hostname, command, spaces, etc */
	char sendbuf[BUFSIZE - HOSTLEN - 16];
	Module *mod;
	ReqMod *rmod;
	size_t len, modlen;

	/* Let's not have leaves directly connected to the hub send their module list to other *leaves* as well =]
	 * Since the hub will introduce all servers currently linked to it, this hook is actually called for every separate node
	 */
	if (!MyConnect(client))
		return HOOK_CONTINUE;

	sendbuf[0] = '\0';
	len = 0;

	/* At this stage we don't care if a module isn't global (or not fully loaded), we'll dump all modules so we can properly deny
	 * certain ones across the network
	 * Also, the G flag is only used for modules that tag themselves as global, since we're keeping separate lists for require (R flag) and deny
	 */
	for (mod = Modules; mod; mod = mod->next)
	{
		modflag = SMOD_FLAG_LOCAL;
		modversion = mod->header->version;

		// require { }'d modules should be loaded on this server anyways, meaning we don't have to use a separate loop for those =]
		if ((rmod = find_reqmod_byname(mod->header->name)))
		{
			// require module::min-version overrides the version found in the module's header
			modflag = SMOD_FLAG_REQUIRED;
			modversion = (rmod->minversion ? rmod->minversion : "*");
		}

		else if ((mod->options & MOD_OPT_GLOBAL))
			modflag = SMOD_FLAG_GLOBAL;

		ircsnprintf(modbuf, sizeof(modbuf), "%c:%s:%s", modflag, mod->header->name, modversion);
		modlen = strlen(modbuf);
		if (len + modlen + 2 > sizeof(sendbuf)) // Account for space and nullbyte, otherwise the last module entry might be cut off
		{
			// "Flush" current list =]
			sendto_one(client, NULL, ":%s %s :%s", me.id, MSG_SMOD, sendbuf);
			sendbuf[0] = '\0';
			len = 0;
		}

		/* Maybe account for the space between modules, can't do this earlier because otherwise the ircsnprintf() would skip past the nullbyte
		 * of the previous module (which in turn terminates the string prematurely)
		 */
		ircsnprintf(sendbuf + len, sizeof(sendbuf) - len, "%s%s", (len > 0 ? " " : ""), modbuf);
		if (len)
			len++;
		len += modlen;
	}

	// May have something left
	if (sendbuf[0])
		sendto_one(client, NULL, ":%s %s :%s", me.id, MSG_SMOD, sendbuf);
	return HOOK_CONTINUE;
}
