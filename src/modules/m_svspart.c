/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svspart.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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

CMD_FUNC(m_svspart);

#define MSG_SVSPART       "SVSPART"

ModuleHeader MOD_HEADER(m_svspart)
  = {
	"svspart",	/* Name of module */
	"4.0", /* Version */
	"command /svspart", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_svspart)
{
	CommandAdd(modinfo->handle, MSG_SVSPART, m_svspart, 3, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_svspart)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_svspart)
{
	return MOD_SUCCESS;	
}

/* m_svspart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
  Modified for PART by Stskeeps
	parv[1] - nick to make part
	parv[2] - channel(s) to part
	parv[3] - comment
*/
CMD_FUNC(m_svspart)
{
	aClient *acptr;
	char *comment = (parc > 3 && parv[3] ? parv[3] : NULL);
	if (!IsULine(sptr))
		return 0;

	if (parc < 3 || !(acptr = find_person(parv[1], NULL))) 
		return 0;

	if (MyClient(acptr))
	{
		parv[0] = acptr->name;
		parv[1] = parv[2];
		parv[2] = comment;
		parv[3] = NULL;
		(void)do_cmd(acptr, acptr, "PART", comment ? 3 : 2, parv);
		/* NOTE: acptr may be killed now by spamfilter due to the part reason */
	}
	else
	{
		if (comment)
			sendto_one(acptr, ":%s SVSPART %s %s :%s", sptr->name,
			    parv[1], parv[2], parv[3]);
		else
			sendto_one(acptr, ":%s SVSPART %s %s", sptr->name,
			    parv[1], parv[2]);
	}

	return 0;
}
