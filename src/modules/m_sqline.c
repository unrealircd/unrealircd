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



ModuleHeader MOD_HEADER(m_sqline)
  = {
	"sqline",	/* Name of module */
	"$Id$", /* Version */
	"command /sqline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_sqline)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SQLINE, TOK_SQLINE, m_sqline, MAXPARA);
	ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_sqline)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_sqline)(int module_unload)
{
	if (del_Command(MSG_SQLINE, TOK_SQLINE, m_sqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_sqline).name);
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

	if (!(IsServer(sptr) || IsULine(sptr)) || parc < 2 || BadPtr(parv[1]))
		return 0;

	if (parv[2])
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s :%s", parv[1], parv[2]);
	else
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s", parv[1]);

	/* Only replaces AKILL (global ban nick)'s */
	for (bconf = conf_ban; bconf; bconf = (ConfigItem_ban *)bconf->next)
	{
		if (bconf->flag.type != CONF_BAN_NICK)
			continue;
		if (bconf->flag.type2 != CONF_BAN_TYPE_AKILL)
			continue;
/*** Temporarely for tracing a bconf->mask being NULL issue. -- Syzop */
		if (!bconf->mask) {
			sendto_realops("bconf->mask is null! %p/%d/%d/%d/'%s'",
				bconf, bconf->flag.temporary, bconf->flag.type, bconf->flag.type2,
				bconf->reason ? bconf->reason : "<NULL>");
			continue; /* let's be nice and not make it crash too */
		}
		if (!stricmp(bconf->mask, parv[1]))
			break;
	}
	if (bconf)
	{
		if (bconf->reason)
			MyFree(bconf->reason);
		bconf->reason = NULL;
		addit = 0;
	}
	else
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		DupString(bconf->mask, parv[1]);
		addit = 1;
	}
	if (parv[2])
		DupString(bconf->reason, parv[2]);
		
	/* CONF_BAN_NICK && CONF_BAN_TYPE_AKILL == SQLINE */
	bconf->flag.type = CONF_BAN_NICK;
	bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
	if (addit == 1)
		AddListItem(bconf, conf_ban);
	return 0;
}
