/*
 *   IRC - Internet Relay Chat, src/modules/m_sdesc.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

CMD_FUNC(m_sdesc);

#define MSG_SDESC 	"SDESC"	/* sdesc */

ModuleHeader MOD_HEADER(m_sdesc)
  = {
	"sdesc",	/* Name of module */
	"4.0", /* Version */
	"command /sdesc", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_sdesc)
{
	CommandAdd(modinfo->handle, MSG_SDESC, m_sdesc, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_sdesc)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_sdesc)
{
	return MOD_SUCCESS;
}

/* m_sdesc - 15/05/1999 - Stskeeps
 *  :prefix SDESC
 *  parv[1] - description
 *  D: Sets server info if you are Server Admin (ONLINE)
*/

CMD_FUNC(m_sdesc)
{
	if (!ValidatePermissionsForPath("server:description",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}
	
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "SDESC");
		return 0;
	}

	if (strlen(parv[1]) > REALLEN)
	{
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SDESC Error: \"Server info\" may maximum be %i characters of length",
			    me.name, sptr->name, REALLEN);
			return 0;
		}
		parv[1][REALLEN] = '\0';
	}

	ircsnprintf(sptr->srvptr->info, sizeof(sptr->srvptr->info), "%s", parv[1]);

	sendto_server(cptr, 0, 0, ":%s SDESC :%s", sptr->name, parv[1]);

	if (MyConnect(sptr))
		sendto_one(sptr,
			":%s NOTICE %s :Your \"server description\" is now set to be %s - you have to set it manually to undo it",
			me.name, sptr->name, parv[1]);

	sendto_ops("Server description for %s is now '%s' changed by %s",
		sptr->srvptr->name, sptr->srvptr->info, sptr->name);
	return 0;
}
