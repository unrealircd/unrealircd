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

CMD_FUNC(m_links);

#define MSG_LINKS 	"LINKS"	

ModuleHeader MOD_HEADER(m_links)
  = {
	"m_links",
	"4.0",
	"command /links", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_links)
{
	CommandAdd(modinfo->handle, MSG_LINKS, m_links, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_links)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_links)
{
	return MOD_SUCCESS;
}

CMD_FUNC(m_links)
{
	aClient *acptr;
	int flat = (FLAT_MAP && !IsOper(sptr)) ? 1 : 0;

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		/* Some checks */
		if (HIDE_ULINES && IsULine(acptr) && !ValidatePermissionsForPath("map:ulines",cptr,NULL,NULL,NULL))
			continue;
		if (flat)
			sendto_one(sptr, rpl_str(RPL_LINKS),
			    me.name, sptr->name, acptr->name, me.name,
			    1, (acptr->info[0] ? acptr->info : "(Unknown Location)"));
		else
			sendto_one(sptr, rpl_str(RPL_LINKS),
			    me.name, sptr->name, acptr->name, acptr->serv->up,
			    acptr->hopcount, (acptr->info[0] ? acptr->info : "(Unknown Location)"));
	}

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, sptr->name, "*");
	return 0;
}
