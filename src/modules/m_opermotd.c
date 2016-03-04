/*
 *   IRC - Internet Relay Chat, src/modules/m_opermotd.c
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

CMD_FUNC(m_opermotd);

#define MSG_OPERMOTD 	"OPERMOTD"	

ModuleHeader MOD_HEADER(m_opermotd)
  = {
	"m_opermotd",
	"4.0",
	"command /opermotd", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_opermotd)
{
	CommandAdd(modinfo->handle, MSG_OPERMOTD, m_opermotd, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_opermotd)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_opermotd)
{
	return MOD_SUCCESS;
}

/*
 * Modified from comstud by codemastr
 */
CMD_FUNC(m_opermotd)
{
	aMotdLine *motdline;
	ConfigItem_tld *tld;

	if (!ValidatePermissionsForPath("server:opermotd",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	tld = Find_tld(sptr);

	motdline = NULL;
	if (tld)
		motdline = tld->opermotd.lines;
	if (!motdline)
		motdline = opermotd.lines;

	if (!motdline)
	{
		sendto_one(sptr, err_str(ERR_NOOPERMOTD), me.name, sptr->name);
		return 0;
	}
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, sptr->name, me.name);
	sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
	    "IRC Operator Message of the Day");

	while (motdline)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
			   motdline->line);
		motdline = motdline->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, sptr->name);
	return 0;
}
