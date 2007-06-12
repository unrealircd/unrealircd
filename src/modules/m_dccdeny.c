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

DLLFUNC int m_dccdeny(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_DCCDENY 	"DCCDENY"	
#define TOK_DCCDENY 	"BI"	

ModuleHeader MOD_HEADER(m_dccdeny)
  = {
	"m_dccdeny",
	"$Id$",
	"command /dccdeny", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_dccdeny)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_DCCDENY, TOK_DCCDENY, m_dccdeny, 2, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_dccdeny)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_dccdeny)(int module_unload)
{
	return MOD_SUCCESS;
}

/* Add a temporary dccdeny line
 *
 * parv[0] - sender
 * parv[1] - file
 * parv[2] - reason
 */
DLLFUNC CMD_FUNC(m_dccdeny)
{
	if (!MyClient(sptr))
		return 0;

	if (!IsAnOper(sptr) || !OPCanDCCDeny(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	/* fixup --Stskeeps */
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "DCCDENY");
		return 0;
	}
	
	if (BadPtr(parv[2]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "DCCDENY");
		return 0;
	}
	if (!Find_deny_dcc(parv[1]))
	{
		sendto_ops("%s added a temp dccdeny for %s (%s)", parv[0],
		    parv[1], parv[2]);
		DCCdeny_add(parv[1], parv[2], DCCDENY_HARD, CONF_BAN_TYPE_TEMPORARY);
		return 0;
	}
	else
		sendto_one(sptr, "NOTICE %s :*** %s already has a dccdeny", parv[0],
		    parv[1]);
	return 0;
}
