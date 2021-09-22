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
ModuleHeader MOD_HEADER
  = {
	"snomasks/dccreject",
	"4.2",
	"Snomask +D",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Global variables */
long SNO_DCCREJECT = 0L;

/* Forward declarations */
int dccreject_dcc_denied(Client *client, const char *target, const char *realfile, const char *displayfile, ConfigItem_deny_dcc *dccdeny);

MOD_TEST()
{
	return MOD_SUCCESS;
}

MOD_INIT()
{
	HookAdd(modinfo->handle, HOOKTYPE_DCC_DENIED, 0, dccreject_dcc_denied);
	
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

int dccreject_dcc_denied(Client *client, const char *target, const char *realfile, const char *displayfile, ConfigItem_deny_dcc *dccdeny)
{
	unreal_log(ULOG_INFO, "dcc", "DCC_REJECTED", client,
	           "$client.details tried to send forbidden file $filename ($ban_reason) to $target (is blocked now)",
	           log_data_string("filename", displayfile),
	           log_data_string("ban_reason", dccdeny->reason),
	           log_data_string("target", target));
	return 0;
}
