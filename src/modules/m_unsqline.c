/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_unsqline.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_unsqline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_UNSQLINE    "UNSQLINE"      /* UNSQLINE */
#define TOK_UNSQLINE    "d"     /* 99 */  


#ifndef DYNAMIC_LINKING
ModuleHeader m_unsqline_Header
#else
#define m_unsqline_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"unsqline",	/* Name of module */
	"$Id$", /* Version */
	"command /unsqline", /* Short description of module */
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
int    m_unsqline_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline, MAXPARA);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_unsqline_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_unsqline_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_unsqline_Header.name);
	}
	return MOD_SUCCESS;
}

/*    m_unsqline
**	parv[0] = sender
**	parv[1] = nickmask
*/
DLLFUNC int m_unsqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban *bconf;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	sendto_serv_butone_token(cptr, parv[0], MSG_UNSQLINE, TOK_UNSQLINE,
	    "%s", parv[1]);

	if ((bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL)))
	{
		DelListItem(bconf, conf_ban);
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		MyFree(bconf);
	}
	return 0;
}
