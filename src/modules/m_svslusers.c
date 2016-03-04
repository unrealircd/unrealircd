/*
 *   IRC - Internet Relay Chat, src/modules/m_svslusers.c
 *   (C) 2002 codemastr [Dominick Meglio] (codemastr@unrealircd.com)
 *
 *   SVSLUSERS command, allows remote setting of local and global max user count
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

CMD_FUNC(m_svslusers);

#define MSG_SVSLUSERS 	"SVSLUSERS"	

ModuleHeader MOD_HEADER(m_svslusers)
  = {
	"m_svslusers",
	"4.0",
	"command /svslusers", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_svslusers)
{
	CommandAdd(modinfo->handle, MSG_SVSLUSERS, m_svslusers, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svslusers)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svslusers)
{
	return MOD_SUCCESS;
}
/*
** m_svslusers
**      parv[1] = server to update
**      parv[2] = max global users
**      parv[3] = max local users
**      If -1 is specified for either number, it is ignored and the current count
**      is kept.
*/
CMD_FUNC(m_svslusers)
{
        if (!IsULine(sptr) || parc < 4)
		return -1;  
        if (hunt_server(cptr, sptr, ":%s SVSLUSERS %s %s :%s", 1, parc, parv) == HUNTED_ISME)
        {
		int temp;
		temp = atoi(parv[2]);
		if (temp >= 0)
			IRCstats.global_max = temp;
		temp = atoi(parv[3]);
		if (temp >= 0) 
			IRCstats.me_max = temp;
        }
        return 0;
}
