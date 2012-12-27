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
	CommandAdd(modinfo->handle, MSG_SQLINE, TOK_SQLINE, m_sqline, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
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
	return MOD_SUCCESS;
}

/*    m_sqline
**	parv[0] = sender
**	parv[1] = nickmask
**	parv[2] = reason
*/
DLLFUNC int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char    mo[1024];
	char *comment = (parc == 3) ? parv[2] : NULL;
	char *tkllayer[9] = {
                me.name,        /*0  server.name */
                "+",            /*1  +|- */
                "Q",            /*2  G   */
                "*" ,           /*3  user */
                parv[1],           /*4  host */
                sptr->name,           /*5  setby */
                "0",            /*6  expire_at */
                NULL,           /*7  set_at */
                "no reason"     /*8  reason */
        };

	if (!IsServer(cptr))
		return 0;

	if (parc < 2)
		return 0;

	ircsprintf(mo, "%li", TStime());
	tkllayer[7] = mo;
        tkllayer[8] = comment ? comment : "no reason";
        return m_tkl(&me, &me, 9, tkllayer);
}
