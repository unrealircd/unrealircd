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

DLLFUNC int m_close(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_CLOSE 	"CLOSE"	
#define TOK_CLOSE 	"Q"	

ModuleHeader MOD_HEADER(m_close)
  = {
	"m_close",
	"$Id$",
	"command /close", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_close)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_CLOSE, TOK_CLOSE, m_close, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_close)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_close)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_close - added by Darren Reed Jul 13 1992.
*/
DLLFUNC CMD_FUNC(m_close)
{
	aClient *acptr;
	int  i;
	int  closed = 0;


	if (!MyOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	for (i = LastSlot; i >= 0; --i)
	{
		if (!(acptr = local[i]))
			continue;
		if (!IsUnknown(acptr) && !IsConnecting(acptr) &&
		    !IsHandshake(acptr))
			continue;
		sendto_one(sptr, rpl_str(RPL_CLOSING), me.name, parv[0],
		    get_client_name(acptr, TRUE), acptr->status);
		(void)exit_client(acptr, acptr, acptr, "Oper Closing");
		closed++;
	}
	sendto_one(sptr, rpl_str(RPL_CLOSEEND), me.name, parv[0], closed);
	sendto_realops("%s!%s@%s closed %d unknown connections", sptr->name,
	    sptr->user->username, GetHost(sptr), closed);
	IRCstats.unknown = 0;
	return 0;
}
