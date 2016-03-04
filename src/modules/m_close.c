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

CMD_FUNC(m_close);

#define MSG_CLOSE 	"CLOSE"	

ModuleHeader MOD_HEADER(m_close)
  = {
	"m_close",
	"4.0",
	"command /close", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_close)
{
	CommandAdd(modinfo->handle, MSG_CLOSE, m_close, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_close)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_close)
{
	return MOD_SUCCESS;
}

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
CMD_FUNC(m_close)
{
	aClient *acptr, *acptr2;
	int  i;
	int  closed = 0;

	if (!ValidatePermissionsForPath("server:close",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	list_for_each_entry_safe(acptr, acptr2, &unknown_list, lclient_node)
	{
		sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, sptr->name,
		    get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, "Oper Closing");
		closed++;
	}

	sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, sptr->name, closed);
	sendto_realops("%s!%s@%s closed %d unknown connections", sptr->name,
	    sptr->user->username, GetHost(sptr), closed);
	IRCstats.unknown = 0;

	return 0;
}
