/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_cycle.c
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

CMD_FUNC(m_cycle);

/* Place includes here */
#define MSG_CYCLE       "CYCLE"

ModuleHeader MOD_HEADER(m_cycle)
  = {
	"cycle",	/* Name of module */
	"4.0", /* Version */
	"command /cycle", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_cycle)
{
	CommandAdd(modinfo->handle, MSG_CYCLE, m_cycle, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_cycle)
{
	return MOD_SUCCESS;
	
}

/* Called when module is unloaded */
MOD_UNLOAD(m_cycle)
{
	return MOD_SUCCESS;	
}

/*
 * m_cycle() - Stskeeps
 * parv[1] = channels
*/

CMD_FUNC(m_cycle)
{
	char channels[BUFSIZE];
	int n;
	
	if (parc < 2)
		return 0;

	parv[2] = "cycling";
	strlcpy(channels, parv[1], sizeof(channels));
	n = do_cmd(cptr, sptr, "PART", 3, parv);
	if (n == FLUSH_BUFFER)
		return n;
	parv[1] = channels;
	parv[2] = NULL;
	return do_cmd(cptr, sptr, "JOIN", 2, parv);
}
