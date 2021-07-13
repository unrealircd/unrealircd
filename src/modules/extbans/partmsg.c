/*
 * Hide Part/Quit message extended ban (+b ~p:nick!user@host)
 * (C) Copyright i <info@servx.org> and the UnrealIRCd team
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

ModuleHeader MOD_HEADER
= {
	"extbans/partmsg",
	"4.2",
	"ExtBan ~p - Ban/exempt Part/Quit message",
	"UnrealIRCd Team",
	"unrealircd-5",
};

int extban_partmsg_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg);

MOD_INIT()
{
	ExtbanInfo req;

	req.flag = 'p';
	req.is_ok = extban_is_ok_nuh_extban;
	req.conv_param = extban_conv_param_nuh_or_extban;
	req.options = EXTBOPT_ACTMODIFIER;
	req.is_banned = extban_partmsg_is_banned;
	if (!ExtbanAdd(modinfo->handle, req))
	{
		config_error("could not register extended ban type");
		return MOD_FAILED;
	}

	MARK_AS_OFFICIAL_MODULE(modinfo);

	return MOD_SUCCESS;
}

/** Called upon module load */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/** Called upon unload */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

int extban_partmsg_is_banned(Client *client, Channel *channel, char *banin, int type, char **msg, char **errmsg)
{
	if (type == BANCHK_LEAVE_MSG)
		*msg = NULL;

	return 0;
}
