/*
 *   IRC - Internet Relay Chat, src/modules/m_swhois.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SWHOIS command
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

CMD_FUNC(m_swhois);

#define MSG_SWHOIS 	"SWHOIS"	

ModuleHeader MOD_HEADER(m_swhois)
  = {
	"m_swhois",
	"4.0",
	"command /swhois", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_swhois)
{
	CommandAdd(modinfo->handle, MSG_SWHOIS, m_swhois, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_swhois)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_swhois)
{
	return MOD_SUCCESS;
}
/*
 * m_swhois
 * Old syntax:
 * parv[1] = nickname
 * parv[2] = new swhois
 * New syntax:
 * parv[1] = nickname
 * parv[2] = + or -
 * parv[3] = added-by tag
 * parv[4] = priority
 * parv[5] = swhois
 */
CMD_FUNC(m_swhois)
{
        aClient *acptr;
        char tag[HOSTLEN+1];
        char swhois[SWHOISLEN+1];
        int add;
        int priority = 0;

        *tag = *swhois = '\0';

        if (parc < 3)
                return 0;

        acptr = find_person(parv[1], (aClient *)NULL);
        if (!acptr)
                return 0;

		if ((parc > 5) && !BadPtr(parv[5]))
		{
			/* New syntax */
			add = (*parv[2] == '+') ? 1 : 0;
			strlcpy(tag, parv[3], sizeof(tag));
			priority = atoi(parv[4]);
			strlcpy(swhois, parv[5], sizeof(swhois));
		} else {
			/* Old syntax */
			strlcpy(tag, sptr->name, sizeof(tag));
			if (BadPtr(parv[2]))
			{
				/* Delete. Hmmmm. Let's just delete anything with that tag. */
				strcpy(swhois, "*");
				add = 0;
			} else {
				/* Add */
				add = 1;
				strlcpy(swhois, parv[2], sizeof(swhois));
			}
		}
		
		if (add)
			swhois_add(acptr, tag, priority, swhois, sptr, sptr);
		else
			swhois_delete(acptr, tag, swhois, sptr, sptr);
		
		return 0;
}
