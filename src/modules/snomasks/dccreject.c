/*
 * Show DCC SEND rejection notices (Snomask +D)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

/* Module header */
ModuleHeader MOD_HEADER(dccreject)
  = {
	"snomasks/dccreject",
	"$Id$",
	"Snomask +D",
	"3.2-b8-1",
	NULL 
    };

/* Global variables */
long SNO_DCCREJECT = 0L;

/* Forward declarations */
int dccreject_dcc_denied(aClient *sptr, aClient *target, char *realfile, char *displayfile, ConfigItem_deny_dcc *dccdeny);

DLLFUNC int MOD_TEST(dccreject)(ModuleInfo *modinfo)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(dccreject)(ModuleInfo *modinfo)
{
	SnomaskAdd(modinfo->handle, 'D', 1, umode_allow_opers, &SNO_DCCREJECT);
	
	HookAdd(modinfo->handle, HOOKTYPE_DCC_DENIED, 0, dccreject_dcc_denied);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(dccreject)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(dccreject)(int module_unload)
{
	return MOD_SUCCESS;
}

int dccreject_dcc_denied(aClient *sptr, aClient *target, char *realfile, char *displayfile, ConfigItem_deny_dcc *dccdeny)
{
	sendto_snomask_global(SNO_DCCREJECT, 
		"%s tried to send forbidden file %s (%s) to %s (is blocked now)",
		sptr->name, displayfile, dccdeny->reason, target->name);

	return 0;
}
