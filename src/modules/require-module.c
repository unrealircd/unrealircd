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

ModuleHeader MOD_HEADER = {
	"require-module",
	"5.0",
	"Check for required modules across the network",
	"UnrealIRCd Team",
	"unrealircd-5",
};

typedef struct _denymod DenyMod;
struct _denymod {
	DenyMod *prev, *next;
	char *name;
	char *reason;
};

// Forward declarations
Module *find_modptr_byname(char *name, unsigned strict);
DenyMod *find_denymod_byname(char *name);

int reqmods_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun(ConfigFile *cf, ConfigEntry *ce, int type);

int reqmods_configtest_deny(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun_deny(ConfigFile *cf, ConfigEntry *ce, int type);

int reqmods_configtest_require(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun_require(ConfigFile *cf, ConfigEntry *ce, int type);

int reqmods_configtest_set(ConfigFile *cf, ConfigEntry *ce, int type, int *errs);
int reqmods_configrun_set(ConfigFile *cf, ConfigEntry *ce, int type);

CMD_FUNC(cmd_smod);
int reqmods_hook_serverconnect(Client *client);

// Globals
extern Module *Modules;
DenyMod *DenyModList = NULL;

struct cfgstruct {
	int squit_on_deny;
	int squit_on_missing;
	int squit_on_mismatch;
};
static struct cfgstruct cfg;

MOD_TEST()
{
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, reqmods_configtest);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	MARK_AS_GLOBAL_MODULE(modinfo);
	memset(&cfg, 0, sizeof(cfg));
	cfg.squit_on_deny = 1;
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
	DenyMod *dmod, *next;
	for (dmod = DenyModList; dmod; dmod = next)
	{
		next = dmod->next;
		safe_free(dmod->name);
		safe_free(dmod->reason);
		DelListItem(dmod, DenyModList);
		safe_free(dmod);
	}
	DenyModList = NULL;
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

int reqmods_configtest(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	if (type == CONFIG_DENY)
		return reqmods_configtest_deny(cf, ce, type, errs);

	if (type == CONFIG_REQUIRE)
		return reqmods_configtest_require(cf, ce, type, errs);

	if (type == CONFIG_SET)
		return reqmods_configtest_set(cf, ce, type, errs);

	return 0;
}

int reqmods_configrun(ConfigFile *cf, ConfigEntry *ce, int type)
{
	if (type == CONFIG_DENY)
		return reqmods_configrun_deny(cf, ce, type);

	if (type == CONFIG_REQUIRE)
		return reqmods_configrun_require(cf, ce, type);

	if (type == CONFIG_SET)
		return reqmods_configrun_set(cf, ce, type);

	return 0;
}

int reqmods_configtest_deny(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;
	int has_name;

	// We are only interested in deny module { }
	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	has_name = 0;
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
			continue;

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
	int has_name;

	// We are only interested in require module { }
	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	has_name = 0;
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
			if (!find_modptr_byname(cep->ce_vardata, 0))
			{
				config_error("[require-module] Module '%s' was specified as required but we didn't even load it ourselves (maybe double check the name?)", cep->ce_vardata);
				errors++;
			}

			// Let's be nice and let configrun handle the module flags
			has_name = 1;
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

	if (strcmp(ce->ce_vardata, "module"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "name"))
		{
			if (!(mod = find_modptr_byname(cep->ce_vardata, 0)))
			{
				// Something went very wrong :D
				config_error("[require-module] [BUG?] Passed configtest_require() but not configrun_require() for module '%s' (seems to not be loaded after all)", cep->ce_vardata);
				continue;
			}

			// Just add the global flag so we don't have to keep a separate list for required modules too =]
			if (!(mod->options & MOD_OPT_GLOBAL))
				mod->options |= MOD_OPT_GLOBAL;
			continue;
		}
	}

	return 1;
}

int reqmods_configtest_set(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;
	ConfigEntry *cep;

	// We are only interested in set::require-module
	if (strcmp(ce->ce_varname, "require-module"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strlen(cep->ce_varname))
		{
			config_error("%s:%i: blank set::require-module directive", cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
			errors++;
			continue;
		}

		if (!cep->ce_vardata || !strlen(cep->ce_vardata))
		{
			config_error("%s:%i: blank set::require-module::%s without value", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
			errors++;
			continue;
		}

		if (!strcmp(cep->ce_varname, "squit-on-deny") || !strcmp(cep->ce_varname, "squit-on-missing") || !strcmp(cep->ce_varname, "squit-on-mismatch"))
			continue;

		config_error("%s:%i: unknown directive set::require-module::%s", cep->ce_fileptr->cf_filename, cep->ce_varlinenum, cep->ce_varname);
		errors++;
	}

	*errs = errors;
	return errors ? -1 : 1;
}

int reqmods_configrun_set(ConfigFile *cf, ConfigEntry *ce, int type)
{
	ConfigEntry *cep;

	// We are only interested in set::require-module
	if (strcmp(ce->ce_varname, "require-module"))
		return 0;

	for (cep = ce->ce_entries; cep; cep = cep->ce_next)
	{
		if (!strcmp(cep->ce_varname, "squit-on-deny"))
		{
			cfg.squit_on_deny = config_checkval(cep->ce_vardata, CFG_YESNO);
			continue;
		}

		if (!strcmp(cep->ce_varname, "squit-on-missing"))
		{
			cfg.squit_on_missing = config_checkval(cep->ce_vardata, CFG_YESNO);
			continue;
		}

		if (!strcmp(cep->ce_varname, "squit-on-mismatch"))
		{
			cfg.squit_on_mismatch = config_checkval(cep->ce_vardata, CFG_YESNO);
			continue;
		}
	}

	return 1;
}

CMD_FUNC(cmd_smod)
{
	char flag, name[64], *version;
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
		p = strchr(modbuf, ':');
		if (!p)
			continue; /* malformed request */
		flag = *modbuf; // Get the local/global flag (FIXME: parses only first letter atm)
		modbuf = p+1;
		strlcpy(name, modbuf, sizeof(name)); // Let's work on a copy of the param

		version = strchr(name, ':');
		if (!version)
			continue; /* malformed request */
		*version++ = '\0';

		// Even if a denied module is only required locally, maybe still prevent a server that uses it from linking in
		if ((dmod = find_denymod_byname(name)))
		{
			// Send this particular notice to local opers only
			sendto_umode_global(UMODE_OPER, "Server %s is using module '%s' which is specified in a deny module { } config block (reason: %s)", client->name, name, dmod->reason);
			if (cfg.squit_on_deny)
				abort = 1;
			continue;
		}

		// Doing a strict check for the module being fully loaded so we can emit an alert in that case too :>
		if (!(mod = find_modptr_byname(name, 1)))
		{
			/* Since only the server missing the module will report it, we need to broadcast the warning network-wide ;]
			 * Obviously we won't take any action if the module seems to be locally required only
			 */
			if (flag == 'G')
			{
				sendto_umode_global(UMODE_OPER, "Globally required module '%s' wasn't (fully) loaded or is missing entirely", name);
				if (cfg.squit_on_missing)
					abort = 1;
			}
			continue;
		}

		/* A strcasecmp() suffices because the version string only has to *start* with a digit, it can have e.g. "-alpha" at the end
		 * We also check the module version for locally required modules (for completeness)
		 */
		if (!version || strcasecmp(mod->header->version, version))
		{
			// Version mismatches can be (and are) reported on both ends separately, so a local server notice is enough
			sendto_umode(UMODE_OPER, "Version mismatch for module '%s' (ours: %s, theirs: %s)", name, mod->header->version, version);
			if (cfg.squit_on_mismatch)
				abort = 1;
			continue;
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
	char modbuf[64];
	/* Try to use a large buffer, but take into account the hostname, command, spaces, etc */
	char sendbuf[BUFSIZE - HOSTLEN - 16];
	Module *mod;
	size_t len, modlen;

	/* Let's not have leaves directly connected to the hub send their module list to other *leaves* as well =]
	 * Since the hub will introduce all servers currently linked to it, this hook is actually called for every separate node
	 */
	if (!MyConnect(client))
		return HOOK_CONTINUE;

	sendbuf[0] = '\0';
	len = 0;

	for (mod = Modules; mod; mod = mod->next)
	{
		/* At this stage we don't care if the module isn't global (or not fully loaded), we'll dump all modules
		 * so we can properly deny certain ones across the network
		 */
		ircsnprintf(modbuf, sizeof(modbuf), "%c:%s:%s", ((mod->options & MOD_OPT_GLOBAL) ? 'G' : 'L'), mod->header->name, mod->header->version);
		modlen = strlen(modbuf);
		if (len + modlen + 2 > sizeof(sendbuf)) // Account for space and nullbyte, otherwise the last module string might be cut off
		{
			// "Flush" current list =]
			sendto_one(client, NULL, ":%s %s :%s", me.id, MSG_SMOD, sendbuf);
			sendbuf[0] = '\0';
			len = 0;
		}

		ircsnprintf(sendbuf + len, sizeof(sendbuf) - len, "%s%s", (len > 0 ? " " : ""), modbuf);

		/* Maybe account for the space between modules, can't do this earlier because otherwise the ircsnprintf() would skip past the nullbyte
		 * of the previous module (which in turn terminates the string prematurely)
		 */
		if (len)
			len++;
		len += modlen;
	}

	// May have something left
	if (sendbuf[0])
		sendto_one(client, NULL, ":%s %s :%s", me.id, MSG_SMOD, sendbuf);
	return HOOK_CONTINUE;
}
