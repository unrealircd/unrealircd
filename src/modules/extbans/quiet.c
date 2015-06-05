/*
 * Extended ban that affects messages/notices only (+b ~q)
 * (C) Copyright 2003-.. Bram Matthys (Syzop) and the UnrealIRCd team
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

ModuleHeader MOD_HEADER(quiet)
= {
	"chanmodes/extbans/quiet",
	"$Id$",
	"ExtBan ~q - prevent messages only (quiet)",
	"3.2-b8-1",
	NULL 
};

/* Forward declarations */
int extban_quiet_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type);

/** Called upon module init */
DLLFUNC int MOD_INIT(quiet)(ModuleInfo *modinfo)
{
	ExtbanInfo req;
	
	req.flag = 'q';
	req.is_ok = extban_is_ok_nuh_extban;
	req.conv_param = extban_conv_param_nuh_or_extban;
	req.is_banned = extban_quiet_is_banned;
	req.options = EXTBOPT_ACTMODIFIER;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	return MOD_SUCCESS;
}

/** Called upon module load */
DLLFUNC int MOD_LOAD(quiet)(int module_load)
{
	return MOD_SUCCESS;
}

/** Called upon unload */
DLLFUNC int MOD_UNLOAD(quiet)(int module_unload)
{
	return MOD_SUCCESS;
}

/** This ban that affects messages/notices only */
int extban_quiet_is_banned(aClient *sptr, aChannel *chptr, char *banin, int type)
{
	char *sub_ban;

	if (type != BANCHK_MSG)
		return 0;

	sub_ban = banin + 3;

	return ban_check_mask(sptr, chptr, sub_ban, type, 0);
}
