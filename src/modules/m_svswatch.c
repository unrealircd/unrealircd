/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svswatch.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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

DLLFUNC int m_svswatch(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SVSWATCH       "SVSWATCH"
#define TOK_SVSWATCH       "BW"

#ifndef DYNAMIC_LINKING
ModuleHeader m_svswatch_Header
#else
#define m_svswatch_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"svswatch",	/* Name of module */
	"$Id$", /* Version */
	"command /svswatch", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_svswatch_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SVSWATCH, TOK_SVSWATCH, m_svswatch, MAXPARA);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_svswatch_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_svswatch_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SVSWATCH, TOK_SVSWATCH, m_svswatch) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svswatch_Header.name);
	}
	return MOD_SUCCESS;	
}

/* m_svswatch() - written by Syzop, suggested by Griever.
 * parv[0] - sender
 * parv[1] - target nick
 * parv[2] - parameters
 */
CMD_FUNC(m_svswatch)
{
	aClient *acptr;
	if (!IsULine(sptr))
		return 0;

	if (parc < 3 || BadPtr(parv[2]) || !(acptr = find_person(parv[1], NULL)))
		return 0;

	if (MyClient(acptr))
	{
		parv[0] = parv[1];
		parv[1] = parv[2];
		(void)m_watch(acptr, acptr, 2, parv);
	}
	else
		sendto_one(acptr, ":%s SVSWATCH %s :%s", parv[0], parv[1], parv[2]);

	return 0;
}
