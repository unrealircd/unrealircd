/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_sqline.c
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

DLLFUNC int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SQLINE      "SQLINE"        /* SQLINE */
#define TOK_SQLINE      "c"     /* 98 */


#ifndef DYNAMIC_LINKING
ModuleHeader m_sqline_Header
#else
#define m_sqline_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"sqline",	/* Name of module */
	"$Id$", /* Version */
	"command /sqline", /* Short description of module */
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
int    m_sqline_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SQLINE, TOK_SQLINE, m_sqline, MAXPARA);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_sqline_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_sqline_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SQLINE, TOK_SQLINE, m_sqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_sqline_Header.name);
	}
	return MOD_SUCCESS;
	
}

/*    m_sqline
**	parv[0] = sender
**	parv[1] = nickmask
**	parv[2] = reason
*/
DLLFUNC int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban	*bconf;
	/* So we do not make double entries */
	int		addit = 0;

	if (!(IsServer(sptr) || IsUline(sptr)) || parc < 2)
		return 0;

	if (parv[2])
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s :%s", parv[1], parv[2]);
	else
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s", parv[1]);

	/* Only replaces AKILL (global ban nick)'s */
	if ((bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL)))
	{
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		bconf->mask = NULL;
		bconf->reason = NULL;
		addit = 0;
	}
	else
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		addit = 1;
	}
	if (parv[2])
		DupString(bconf->reason, parv[2]);
	if (parv[1])
		DupString(bconf->mask, parv[1]);
		
	/* CONF_BAN_NICK && CONF_BAN_TYPE_AKILL == SQLINE */
	bconf->flag.type = CONF_BAN_NICK;
	bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
	if (addit == 1)
		AddListItem(bconf, conf_ban);
	return 0;
}
