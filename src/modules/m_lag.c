/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_lag.c
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

CMD_FUNC(m_lag);

/* Place includes here */
#define MSG_LAG         "LAG"   /* Lag detect */

ModuleHeader MOD_HEADER(m_lag)
  = {
	"lag",	/* Name of module */
	"4.0", /* Version */
	"command /lag", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_lag)
{
	CommandAdd(modinfo->handle, MSG_LAG, m_lag, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_lag)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_lag)
{
	return MOD_SUCCESS;
}

/* m_lag (lag measure) - Stskeeps
 * parv[1] = server to query
*/

CMD_FUNC(m_lag)
{
	if (!ValidatePermissionsForPath("server:info:lag",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "LAG");
		return 0;
	}
	if (*parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "LAG");
		return 0;
	}
	if (hunt_server(cptr, sptr, ":%s LAG :%s", 1, parc, parv) == HUNTED_NOSUCH)
	{
		return 0;
	}

	sendto_one(sptr, ":%s NOTICE %s :Lag reply -- %s %s %li",
	    me.name, sptr->name, me.name, parv[1], TStime());

	return 0;
}
