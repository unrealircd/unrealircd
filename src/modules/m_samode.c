/*
 *   IRC - Internet Relay Chat, src/modules/m_samode.c
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

CMD_FUNC(m_samode);

#define MSG_SAMODE 	"SAMODE"	

ModuleHeader MOD_HEADER(m_samode)
  = {
	"m_samode",
	"4.0",
	"command /samode", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_samode)
{
	CommandAdd(modinfo->handle, MSG_SAMODE, m_samode, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_samode)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_samode)
{
	return MOD_SUCCESS;
}

/*
 * m_samode
 * parv[1] = channel
 * parv[2] = modes
 * -t
 */
CMD_FUNC(m_samode)
{
	aChannel *chptr;

	if (parc > 2)
        {
                chptr = find_channel(parv[1], NullChn);
                if (chptr == NullChn)
                        return 0;
        }
	else
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                    me.name, sptr->name, "SAMODE");
                return 0;
        }

	if (!ValidatePermissionsForPath("samode",sptr,NULL,chptr,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	opermode = 0;
	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, 0, 1);

	return 0;
}
