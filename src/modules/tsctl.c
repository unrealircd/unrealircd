/*
 *   Unreal Internet Relay Chat Daemon, src/modules/tsctl.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

ModuleHeader MOD_HEADER
  = {
	"tsctl",	/* Name of module */
	"5.0", /* Version */
	"command /tsctl", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

CMD_FUNC(cmd_tsctl);

MOD_INIT()
{
	CommandAdd(modinfo->handle, "TSCTL", cmd_tsctl, MAXPARA, M_USER|M_SERVER);
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

CMD_FUNC(cmd_tsctl)
{
	if (!ValidatePermissionsForPath("server:tsctl:view",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (MyUser(sptr) && (!parv[1] || strcasecmp(parv[1], "alltime")))
	{
		sendnotice(sptr, "/TSCTL now shows the time on all servers. You can now longer MODIFY the time.");
		parv[1] = "alltime";
	}

	if (parv[1] && !strcasecmp(parv[1], "alltime"))
	{
		sendnotice(sptr, "*** Server=%s TStime=%lld",
			me.name, (long long)TStime());
		sendto_server(cptr, 0, 0, NULL, ":%s TSCTL alltime", sptr->name);
		return 0;
	}

	return 0;
}
