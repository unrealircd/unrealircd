/*
 *   IRC - Internet Relay Chat, src/modules/out.c
 *   (C) 2004 The UnrealIRCd Team
 *
 *   See file AUTHORS in IRC package for additional names of
 *   the programmers.
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
#include "proto.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_wallops(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_WALLOPS 	"WALLOPS"	
#define TOK_WALLOPS 	"="	

ModuleHeader MOD_HEADER(m_wallops)
  = {
	"m_wallops",
	"$Id$",
	"command /wallops", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_wallops)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_WALLOPS, TOK_WALLOPS, m_wallops, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_wallops)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_wallops)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_wallops (write to *all* opers currently online)
**	parv[0] = sender prefix
**	parv[1] = message text
*/
DLLFUNC CMD_FUNC(m_wallops)
{
	char *message;
	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "WALLOPS");
		return 0;
	}
	if (MyClient(sptr) && !OPCanWallOps(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
	    ":%s WALLOPS :%s", parv[0], message);
	return 0;
}
