/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svskill.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(m_svskill);

#define MSG_SVSKILL	"SVSKILL"

ModuleHeader MOD_HEADER(m_svskill)
  = {
	"svskill",	/* Name of module */
	"4.0", /* Version */
	"command /svskill", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* This is called on module init, before Server Ready */
MOD_INIT(m_svskill)
{
	CommandAdd(modinfo->handle, MSG_SVSKILL, m_svskill, MAXPARA, M_SERVER|M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_svskill)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD(m_svskill)
{
	return MOD_SUCCESS;
}

/*
** m_svskill
**	parv[1] = client
**	parv[2] = kill message
*/
CMD_FUNC(m_svskill)
{
	aClient *acptr;
	/* this is very wierd ? */
	char *comment = "SVS Killed";

	if (parc < 2)
		return -1;
	if (parc > 3)
		return -1;
	if (parc == 3)
		comment = parv[2];

	if (!IsULine(sptr))
		return -1;

	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	sendto_server(cptr, 0, 0, ":%s SVSKILL %s :%s", sptr->name, parv[1], comment);

	acptr->flags |= FLAGS_KILLED;

	return exit_client(cptr, acptr, sptr, comment);
}

