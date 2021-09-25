/*
 *   IRC - Internet Relay Chat, src/modules/lusers.c
 *   (C) 2005 The UnrealIRCd Team
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

CMD_FUNC(cmd_lusers);

#define MSG_LUSERS 	"LUSERS"	

ModuleHeader MOD_HEADER
  = {
	"lusers",
	"5.0",
	"command /lusers", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_LUSERS, cmd_lusers, MAXPARA, CMD_USER|CMD_SERVER);
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
 * parv[1] = server to query
 */
CMD_FUNC(cmd_lusers)
{
char flatmap;

	if (hunt_server(client, recv_mtags, "LUSERS", 1, parc, parv) != HUNTED_ISME)
		return;

	flatmap = (FLAT_MAP && !ValidatePermissionsForPath("server:info:lusers",client,NULL,NULL,NULL)) ? 1 : 0;

	/* Just to correct results ---Stskeeps */
	if (irccounts.clients > irccounts.global_max)
		irccounts.global_max = irccounts.clients;
	if (irccounts.me_clients > irccounts.me_max)
		irccounts.me_max = irccounts.me_clients;

	sendnumeric(client, RPL_LUSERCLIENT,
	    irccounts.clients - irccounts.invisible, irccounts.invisible,
	    irccounts.servers);

	if (irccounts.operators)
		sendnumeric(client, RPL_LUSEROP, irccounts.operators);
	if (irccounts.unknown)
		sendnumeric(client, RPL_LUSERUNKNOWN, irccounts.unknown);
	if (irccounts.channels)
		sendnumeric(client, RPL_LUSERCHANNELS, irccounts.channels);
	sendnumeric(client, RPL_LUSERME, irccounts.me_clients, flatmap ? 0 : irccounts.me_servers);
	sendnumeric(client, RPL_LOCALUSERS, irccounts.me_clients, irccounts.me_max, irccounts.me_clients, irccounts.me_max);
	sendnumeric(client, RPL_GLOBALUSERS, irccounts.clients, irccounts.global_max, irccounts.clients, irccounts.global_max);
	if (irccounts.me_clients > max_connection_count)
	{
		max_connection_count = irccounts.me_clients;
		if (max_connection_count % 10 == 0)	/* only send on even tens */
		{
			unreal_log(ULOG_INFO, "client", "NEW_USER_RECORD", NULL,
			           "New record on this server: $num_users connections",
			           log_data_integer("num_users", max_connection_count));
		}
	}
}
