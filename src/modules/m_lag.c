/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_lag.c
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

DLLFUNC int m_lag(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_LAG         "LAG"   /* Lag detect */
#define TOK_LAG         "AF"    /* a or ? */

ModuleHeader MOD_HEADER(m_lag)
  = {
	"lag",	/* Name of module */
	"$Id$", /* Version */
	"command /lag", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_lag)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_LAG, TOK_LAG, m_lag, MAXPARA);
	ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_lag)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_lag)(int module_unload)
{
	if (del_Command(MSG_LAG, TOK_LAG, m_lag) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_lag).name);
	}
	return MOD_SUCCESS;
	
}

/* m_lag (lag measure) - Stskeeps
 * parv[0] = prefix
 * parv[1] = server to query
*/

DLLFUNC int m_lag(aClient *cptr, aClient *sptr, int parc, char *parv[])
{

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (*parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (hunt_server_token(cptr, sptr, MSG_LAG, TOK_LAG, ":%s", 1, parc,
	    parv) == HUNTED_NOSUCH)
	{
		return 0;
	}

	sendto_one(sptr, ":%s NOTICE %s :Lag reply -- %s %s %li",
	    me.name, sptr->name, me.name, parv[1], TStime());

	return 0;
}
