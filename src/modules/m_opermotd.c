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
	"4.2",
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
		sendnumeric(sptr, ERR_NOPRIVILEGES, me.name, sptr->name);
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
		sendnumeric(sptr, ERR_NOOPERMOTD, me.name, sptr->name);
		return 0;
	}
	sendnumeric(sptr, RPL_MOTDSTART, me.name, sptr->name, me.name);
	sendnumeric(sptr, RPL_MOTD, me.name, sptr->name,
	    "IRC Operator Message of the Day");

	while (motdline)
	{
		sendnumeric(sptr, RPL_MOTD, me.name, sptr->name,
			   motdline->line);
		motdline = motdline->next;
	}
	sendnumeric(sptr, RPL_ENDOFMOTD, me.name, sptr->name);
	return 0;
}
