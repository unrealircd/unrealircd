/*
 *   IRC - Internet Relay Chat, src/modules/close.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_close);

#define MSG_CLOSE 	"CLOSE"	

ModuleHeader MOD_HEADER
  = {
	"close",
	"5.0",
	"command /close", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_CLOSE, cmd_close, MAXPARA, CMD_USER);
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

/*
** cmd_close - added by Darren Reed Jul 13 1992.
*/
CMD_FUNC(cmd_close)
{
	Client *target, *next;
	int  closed = 0;

	if (!ValidatePermissionsForPath("server:close",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	list_for_each_entry_safe(target, next, &unknown_list, lclient_node)
	{
		sendnumeric(client, RPL_CLOSING,
		    get_client_name(target, TRUE), target->status);
		exit_client(target, NULL, "Oper Closing");
		closed++;
	}

	sendnumeric(client, RPL_CLOSEEND, closed);
	unreal_log(ULOG_INFO, "close", "CLOSED_CONNECTIONS", client,
	           "$client.details closed $num_closed unknown connections",
	           log_data_integer("num_closed", closed));
	irccounts.unknown = 0;
}
