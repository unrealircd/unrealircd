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

ModuleHeader MOD_HEADER(m_unsqline)
  = {
	"unsqline",	/* Name of module */
	"$Id$", /* Version */
	"command /unsqline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_unsqline)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline, MAXPARA);
	ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_unsqline)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_unsqline)(int module_unload)
{
	if (del_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_unsqline).name);
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

	if (!(IsServer(sptr) || IsULine(sptr)) || parc < 2)
		return 0;

	sendto_serv_butone_token(cptr, parv[0], MSG_UNSQLINE, TOK_UNSQLINE,
	    "%s", parv[1]);

	for (bconf = conf_ban; bconf; bconf = (ConfigItem_ban *)bconf->next)
	{
		if (bconf->flag.type != CONF_BAN_NICK)
			continue;
		if (bconf->flag.type2 != CONF_BAN_TYPE_AKILL)
			continue;
		if (!stricmp(parv[1], bconf->mask))
			break;
	}
	if (bconf)
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
