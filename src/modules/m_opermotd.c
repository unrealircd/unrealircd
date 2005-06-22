/*
 *   IRC - Internet Relay Chat, src/modules/m_opermotd.c
 *   (C) 2005 The UnrealIRCd Team
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

DLLFUNC CMD_FUNC(m_opermotd);

#define MSG_OPERMOTD 	"OPERMOTD"	
#define TOK_OPERMOTD 	"AV"	

ModuleHeader MOD_HEADER(m_opermotd)
  = {
	"m_opermotd",
	"$Id$",
	"command /opermotd", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_opermotd)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_OPERMOTD, TOK_OPERMOTD, m_opermotd, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_opermotd)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_opermotd)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * Modified from comstud by codemastr
 */
DLLFUNC CMD_FUNC(m_opermotd)
{
	aMotd *temp;
	ConfigItem_tld *ptr;
	char userhost[HOSTLEN + USERLEN + 6];
	strlcpy(userhost,make_user_host(cptr->user->username, cptr->user->realhost), sizeof userhost);
	ptr = Find_tld(sptr, userhost);

	if (ptr)
	{
		if (ptr->opermotd)
			temp = ptr->opermotd;
		else
			temp = opermotd;
	}
	else
		temp = opermotd;

	if (!IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (!temp)
	{
		sendto_one(sptr, err_str(ERR_NOOPERMOTD), me.name, parv[0]);
		return 0;
	}
	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, parv[0], me.name);
	sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
	    "IRC Operator Message of the Day");

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv[0],
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, parv[0]);
	return 0;
}
