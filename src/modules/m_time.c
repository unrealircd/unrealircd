/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_time.c
 *   (C) 2004 The UnrealIRCd Team
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

DLLFUNC int m_time(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_TIME	"TIME"
#define TOK_TIME	">"

ModuleHeader MOD_HEADER(m_time)
  = {
	"time",	/* Name of module */
	"$Id$", /* Version */
	"command /time", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_time)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_TIME, TOK_TIME, m_time, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_time)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_time)(int module_unload)
{
	if (del_Command(MSG_TIME, TOK_TIME, m_time) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_time).name);
	}
	return MOD_SUCCESS;
}

/*
** m_time
**	parv[0] = sender prefix
**	parv[1] = servername
*/
CMD_FUNC(m_time)
{
	if (hunt_server_token(cptr, sptr, MSG_TIME, TOK_TIME, ":%s", 1, parc,
	    parv) == HUNTED_ISME)
		sendto_one(sptr, rpl_str(RPL_TIME), me.name, parv[0], me.name,
		    date((long)0));
	return 0;
}
