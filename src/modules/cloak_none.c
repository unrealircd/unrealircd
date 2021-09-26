/*
 *   IRC - Internet Relay Chat, src/modules/cloak_none.c
 *   (C) 2021 Bram Matthys and The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "unrealircd.h"

char *cloakcsum();
int cloak_config_test(ConfigFile *, ConfigEntry *, int, int *);

ModuleHeader MOD_HEADER = {
	"cloak_none",
	"1.0",
	"Cloaking module that does nothing",
	"UnrealIRCd Team",
	"unrealircd-6",
};

MOD_TEST()
{
	if (!CallbackAddString(modinfo->handle, CALLBACKTYPE_CLOAK_KEY_CHECKSUM, cloakcsum))
	{
		unreal_log(ULOG_ERROR, "config", "CLOAK_MODULE_DUPLICATE", NULL,
		           "cloak_none: Error while trying to install callback.\n"
		           "Maybe you have multiple cloaking modules loaded? You can only load one!");
		return MOD_FAILED;
	}
	HookAdd(modinfo->handle, HOOKTYPE_CONFIGTEST, 0, cloak_config_test);
	return MOD_SUCCESS;
}

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

int cloak_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs)
{
	int errors = 0;

	if (type != CONFIG_CLOAKKEYS)
		return 0;

	if (ce->items)
	{
		config_error("%s:%i: The cloaking module 'cloak_none' is loaded (no cloaking) but "
		             "you also have set::cloak-keys set. Either delete your cloak keys, "
		             "or switch to a real cloaking module.",
		             ce->file->filename, ce->line_number);
		errors++;
	}
	*errs = errors;
	return errors ? -1 : 1;
}

char *cloakcsum()
{
	return "NONE";
}
