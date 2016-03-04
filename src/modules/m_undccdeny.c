/*
 *   IRC - Internet Relay Chat, src/modules/m_undccdeny.c
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

CMD_FUNC(m_undccdeny);

#define MSG_UNDCCDENY 	"UNDCCDENY"	

ModuleHeader MOD_HEADER(m_undccdeny)
  = {
	"m_undccdeny",
	"4.0",
	"command /undccdeny", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_undccdeny)
{
	CommandAdd(modinfo->handle, MSG_UNDCCDENY, m_undccdeny, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_undccdeny)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_undccdeny)
{
	return MOD_SUCCESS;
}

/* Remove a temporary dccdeny line
 * parv[1] - file/mask
 */
CMD_FUNC(m_undccdeny)
{
	ConfigItem_deny_dcc *p;
	if (!MyClient(sptr))
		return 0;

	if (!ValidatePermissionsForPath("client:dcc",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name,
		    "UNDCCDENY");
		return 0;
	}

	if (BadPtr(parv[1]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name,
		    "UNDCCDENY");
		return 0;
	}
	if ((p = Find_deny_dcc(parv[1])) && p->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
	{
		sendto_ops("%s removed a temp dccdeny for %s", sptr->name,
		    parv[1]);
		DCCdeny_del(p);
		return 1;
	}
	else
		sendto_one(sptr,
		    "NOTICE %s :*** Unable to find a temp dccdeny matching %s",
		    sptr->name, parv[1]);
	return 0;

}
