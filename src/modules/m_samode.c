/*
 *   IRC - Internet Relay Chat, src/modules/m_samode.c
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

DLLFUNC int m_samode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SAMODE 	"SAMODE"	
#define TOK_SAMODE 	"o"	

ModuleHeader MOD_HEADER(m_samode)
  = {
	"m_samode",
	"$Id$",
	"command /samode", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_samode)(ModuleInfo *modinfo)
{
	add_Command(MSG_SAMODE, TOK_SAMODE, m_samode, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_samode)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_samode)(int module_unload)
{
	if (del_Command(MSG_SAMODE, TOK_SAMODE, m_samode) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_samode).name);
	}
	return MOD_SUCCESS;
}

/*
 * m_samode
 * parv[0] = sender
 * parv[1] = channel
 * parv[2] = modes
 * -t
 */
DLLFUNC CMD_FUNC(m_samode)
{
	aChannel *chptr;

	if (!IsPrivileged(cptr) || !IsSAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc > 2)
	{
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "SAMODE");
		return 0;
	}
	opermode = 0;
	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, 0, 1);

	return 0;
}
