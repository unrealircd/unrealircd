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

DLLFUNC int m_svspart(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SVSPART       "SVSPART"
#define TOK_SVSPART       "BT"

#ifndef DYNAMIC_LINKING
ModuleHeader m_svspart_Header
#else
#define m_svspart_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"svspart",	/* Name of module */
	"$Id$", /* Version */
	"command /svspart", /* Short description of module */
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
int    m_svspart_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SVSPART, TOK_SVSPART, m_svspart, MAXPARA);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_svspart_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_svspart_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SVSPART, TOK_SVSPART, m_svspart) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svspart_Header.name);
	}
	return MOD_SUCCESS;	
}

/* m_svspart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
  Modified for PART by Stskeeps
	parv[0] - sender
	parv[1] - nick to make part
	parv[2] - channel(s) to part
*/
CMD_FUNC(m_svspart)
{
	aClient *acptr;
	if (!IsULine(sptr))
		return 0;

	if (parc != 3 || !(acptr = find_person(parv[1], NULL))) return 0;

	if (MyClient(acptr))
	{
		parv[0] = parv[1];
		parv[1] = parv[2];
		(void)m_part(acptr, acptr, 2, parv);
	}
	else
		sendto_one(acptr, ":%s SVSPART %s %s", parv[0],
		    parv[1], parv[2]);

	return 0;
}
