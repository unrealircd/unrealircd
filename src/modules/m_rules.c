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

DLLFUNC int m_rules(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_RULES 	"RULES"	
#define TOK_RULES 	"t"	

ModuleHeader MOD_HEADER(m_rules)
  = {
	"m_rules",
	"$Id$",
	"command /rules", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_rules)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_RULES, TOK_RULES, m_rules, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_rules)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_rules)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
DLLFUNC CMD_FUNC(m_rules)
{
	ConfigItem_tld *ptr;
	aMotd *temp;
	char userhost[USERLEN + HOSTLEN + 6];
	if (IsServer(sptr))
		return 0;
		
	if (hunt_server_token(cptr, sptr, MSG_RULES, TOK_RULES, ":%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;
#ifndef TLINE_Remote
	if (!MyConnect(sptr))
	{
		temp = rules;
		goto playrules;
	}
#endif
	strlcpy(userhost,make_user_host(cptr->user->username, cptr->user->realhost), sizeof userhost);
	ptr = Find_tld(sptr, userhost);

	if (ptr)
	{
		temp = ptr->rules;

	}
	else
		temp = rules;

      playrules:
	if (temp == NULL)
	{
		sendto_one(sptr, err_str(ERR_NORULES), me.name, parv[0]);
		return 0;

	}

	sendto_one(sptr, rpl_str(RPL_RULESSTART), me.name, parv[0], me.name);

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_RULES), me.name, parv[0],
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFRULES), me.name, parv[0]);
	return 0;
}
