/*
 *   Unreal Internet Relay Chat Daemon, src/modules/time.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_time);

/* Place includes here */
#define MSG_TIME	"TIME"

ModuleHeader MOD_HEADER
  = {
	"time",	/* Name of module */
	"5.0", /* Version */
	"command /time", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };


/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_TIME, cmd_time, MAXPARA, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
** cmd_time
**	parv[1] = servername
*/
CMD_FUNC(cmd_time)
{
	if (hunt_server(client, recv_mtags, "TIME", 1, parc, parv) == HUNTED_ISME)
		sendnumeric(client, RPL_TIME, me.name, long_date(0));
}
