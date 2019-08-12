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

ModuleHeader MOD_HEADER(close)
  = {
	"close",
	"5.0",
	"command /close", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(close)
{
	CommandAdd(modinfo->handle, MSG_CLOSE, m_close, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(close)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(close)
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
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	list_for_each_entry_safe(acptr, acptr2, &unknown_list, lclient_node)
	{
		sendnumeric(sptr, RPL_CLOSING,
		    get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, NULL, "Oper Closing");
		closed++;
	}

	sendnumeric(sptr, RPL_CLOSEEND, closed);
	sendto_realops("%s!%s@%s closed %d unknown connections", sptr->name,
	    sptr->user->username, GetHost(sptr), closed);
	IRCstats.unknown = 0;

	return 0;
}
