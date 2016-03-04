/*
 *   IRC - Internet Relay Chat, src/modules/m_umode2.c
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

CMD_FUNC(m_umode2);

#define MSG_UMODE2 	"UMODE2"	

ModuleHeader MOD_HEADER(m_umode2)
  = {
	"m_umode2",
	"4.0",
	"command /umode2", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_umode2)
{
	CommandAdd(modinfo->handle, MSG_UMODE2, m_umode2, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_umode2)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_umode2)
{
	return MOD_SUCCESS;
}

/*
    m_umode2 added by Stskeeps
    parv[1] - modes to change
    Small wrapper to bandwidth save
*/

CMD_FUNC(m_umode2)
{
	char *xparv[5] = {
		sptr->name,
		sptr->name,
		parv[1],
		(parc > 3) ? parv[3] : NULL,
		NULL
	};

	if (!parv[1])
		return 0;
	return m_umode(cptr, sptr, (parc > 3) ? 4 : 3, xparv);
}
