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

DLLFUNC int m_eos(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_EOS 	"EOS"	
#define TOK_EOS 	"ES"	

ModuleHeader MOD_HEADER(m_eos)
  = {
	"m_eos",
	"$Id$",
	"command /eos", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_eos)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_EOS, TOK_EOS, m_eos, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_eos)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_eos)(int module_unload)
{
	if (del_Command(MSG_EOS, TOK_EOS, m_eos) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_eos).name);
	}
	return MOD_SUCCESS;
}

/*
 * EOS (End Of Sync) command.
 * Type: Broadcast
 * Purpose: Broadcasted over a network if a server is synced (after the users, channels,
 *          etc are introduced). Makes us able to know if a server is linked.
 * History: Added in beta18 (in cvs since 2003-08-11) by Syzop
 */
DLLFUNC CMD_FUNC(m_eos)
{
	if (!IsServer(sptr))
		return 0;
	sptr->serv->flags.synced = 1;
	/* pass it on ^_- */
#ifdef DEBUGMODE
	ircd_log(LOG_ERROR, "[EOSDBG] m_eos: got sync from %s (path:%s)", sptr->name, cptr->name);
	ircd_log(LOG_ERROR, "[EOSDBG] m_eos: broadcasting it back to everyone except route from %s", cptr->name);
#endif
	sendto_serv_butone_token(cptr,
		parv[0], MSG_EOS, TOK_EOS, "", NULL);
	return 0;
}
