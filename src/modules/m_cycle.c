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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <sys/timeb.h>
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_cycle(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_CYCLE       "CYCLE"
#define TOK_CYCLE       "BP"

ModuleHeader MOD_HEADER(m_cycle)
  = {
	"cycle",	/* Name of module */
	"$Id$", /* Version */
	"command /cycle", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_cycle)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CYCLE, TOK_CYCLE, m_cycle, MAXPARA);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_cycle)(int module_load)
{
	return MOD_SUCCESS;
	
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_cycle)(int module_unload)
{
	if (del_Command(MSG_CYCLE, TOK_CYCLE, m_cycle) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_cycle).name);
	}
	return MOD_SUCCESS;	
}

/*
 * m_cycle() - Stskeeps
 * parv[0] = prefix
 * parv[1] = channels
*/

CMD_FUNC(m_cycle)
{
	char	channels[1024];
	
	if (IsServer(sptr))
		return 0;

        if (parc < 2)
                return 0;
        parv[2] = "cycling";
	strncpyzt(channels, parv[1], 1020);
        (void)m_part(cptr, sptr, 3, parv);
	parv[1] = channels;
        parv[2] = NULL;
	return m_join(cptr, sptr, 2, parv);
}
