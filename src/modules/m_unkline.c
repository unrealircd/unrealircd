/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_unkline.c
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

DLLFUNC int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_UNKLINE     "UNKLINE"       /* UNKLINE */
#define TOK_UNKLINE     "X"     /* 88 */  

ModuleHeader MOD_HEADER(m_unkline)
  = {
	"unkline",	/* Name of module */
	"$Id$", /* Version */
	"command /unkline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_unkline)(ModuleInfo *modinfo)
{
	/*
	 * We call our CommandAdd crap here
	*/
	CommandAdd(modinfo->handle, MSG_UNKLINE, TOK_UNKLINE, m_unkline, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_unkline)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_unkline)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

DLLFUNC int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!MyClient(sptr) || !OPCanUnKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_one(sptr, ":%s NOTICE %s :Please use /kline -user@host", me.name, parv[0]);

	return 0;
}
