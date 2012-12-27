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

DLLFUNC int m_undccdeny(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_UNDCCDENY 	"UNDCCDENY"	
#define TOK_UNDCCDENY 	"BJ"	

ModuleHeader MOD_HEADER(m_undccdeny)
  = {
	"m_undccdeny",
	"$Id$",
	"command /undccdeny", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_undccdeny)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_UNDCCDENY, TOK_UNDCCDENY, m_undccdeny, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_undccdeny)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_undccdeny)(int module_unload)
{
	return MOD_SUCCESS;
}

/* Remove a temporary dccdeny line
 * parv[0] - sender
 * parv[1] - file/mask
 */
DLLFUNC CMD_FUNC(m_undccdeny)
{
	ConfigItem_deny_dcc *p;
	if (!MyClient(sptr))
		return 0;

	if (!IsAnOper(sptr) || !OPCanDCCDeny(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "UNDCCDENY");
		return 0;
	}

	if (BadPtr(parv[1]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "UNDCCDENY");
		return 0;
	}
/* If we find an exact match even if it is a wild card only remove the exact match -- codemastr */
	if ((p = Find_deny_dcc(parv[1])) && p->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
	{
		sendto_ops("%s removed a temp dccdeny for %s", parv[0],
		    parv[1]);
		DCCdeny_del(p);
		return 1;
	}
/* Next search using the wild card -- codemastr */
/* Uncommented by Stskeeps:
	else if (dcc_del_wild_match(parv[1]) == 1)
		sendto_ops
		    ("%s removed a temp dccdeny for all dccdenys matching %s",
		    parv[0], parv[1]);
*/
/* If still no match, give an error */
	else
		sendto_one(sptr,
		    "NOTICE %s :*** Unable to find a temp dccdeny matching %s",
		    parv[0], parv[1]);
	return 0;

}
