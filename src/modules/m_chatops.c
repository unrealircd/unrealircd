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

DLLFUNC int m_chatops(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_CHATOPS 	"CHATOPS"	
#define TOK_CHATOPS 	"p"	

ModuleHeader MOD_HEADER(m_chatops)
  = {
	"m_chatops",
	"$Id$",
	"command /chatops", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_chatops)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_CHATOPS, TOK_CHATOPS, m_chatops, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_chatops)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_chatops)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_chatops (write to opers who are currently online)
**      parv[0] = sender prefix
**      parv[1] = message text
*/
DLLFUNC CMD_FUNC(m_chatops)
{
	char *message;

	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "CHATOPS");
		return 0;
	}

	if (MyClient(sptr) && !IsAnOper(cptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL,
	    parv[0], MSG_CHATOPS, TOK_CHATOPS, ":%s", message);
		sendto_umode(UMODE_OPER, "*** ChatOps -- from %s: %s",
		    parv[0], message);
		sendto_umode(UMODE_LOCOP, "*** ChatOps -- from %s: %s",
		    parv[0], message);
	return 0;
}
