/*
 *   IRC - Internet Relay Chat, src/modules/m_wallops.c
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

CMD_FUNC(m_wallops);

#define MSG_WALLOPS 	"WALLOPS"	

ModuleHeader MOD_HEADER(m_wallops)
  = {
	"m_wallops",
	"4.0",
	"command /wallops", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_wallops)
{
	CommandAdd(modinfo->handle, MSG_WALLOPS, m_wallops, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_wallops)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_wallops)
{
	return MOD_SUCCESS;
}

/*
** m_wallops (write to *all* opers currently online)
**	parv[1] = message text
*/
CMD_FUNC(m_wallops)
{
	char *message;
	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "WALLOPS");
		return 0;
	}

	if (!ValidatePermissionsForPath("chat:wallops",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
	    ":%s WALLOPS :%s", sptr->name, message);

	return 0;
}
