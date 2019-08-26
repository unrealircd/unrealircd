/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

CMD_FUNC(m_dccdeny);

#define MSG_DCCDENY 	"DCCDENY"	

ModuleHeader MOD_HEADER(dccdeny)
  = {
	"dccdeny",
	"5.0",
	"command /dccdeny", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(dccdeny)
{
	CommandAdd(modinfo->handle, MSG_DCCDENY, m_dccdeny, 2, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(dccdeny)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(dccdeny)
{
	return MOD_SUCCESS;
}

/* Add a temporary dccdeny line
 *
 * parv[1] - file
 * parv[2] - reason
 */
CMD_FUNC(m_dccdeny)
{
	if (!MyClient(sptr))
		return 0;

	if (!ValidatePermissionsForPath("server-ban:dccdeny",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	/* fixup --Stskeeps */
	if (parc < 2)
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS,
		    "DCCDENY");
		return 0;
	}
	
	if (BadPtr(parv[2]))
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS,
		    "DCCDENY");
		return 0;
	}
	if (!Find_deny_dcc(parv[1]))
	{
		sendto_ops("%s added a temp dccdeny for %s (%s)", sptr->name,
		    parv[1], parv[2]);
		DCCdeny_add(parv[1], parv[2], DCCDENY_HARD, CONF_BAN_TYPE_TEMPORARY);
		return 0;
	}
	else
		sendnotice(sptr, "*** %s already has a dccdeny", parv[1]);
	return 0;
}
