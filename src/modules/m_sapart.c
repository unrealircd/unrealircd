/*
 *   IRC - Internet Relay Chat, src/modules/m_sapart.c
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

DLLFUNC int m_sapart(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SAPART 	"SAPART"	
#define TOK_SAPART 	"AY"	

ModuleHeader MOD_HEADER(m_sapart)
  = {
	"m_sapart",
	"$Id$",
	"command /sapart", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_sapart)(ModuleInfo *modinfo)
{
	add_Command(MSG_SAPART, TOK_SAPART, m_sapart, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_sapart)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_sapart)(int module_unload)
{
	if (del_Command(MSG_SAPART, TOK_SAPART, m_sapart) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_sapart).name);
	}
	return MOD_SUCCESS;
}

/* m_sapart() - Lamego - Wed Jul 21 20:04:48 1999
   Copied off PTlink IRCd (C) PTlink coders team.
   Coded for Sadmin by Stskeeps
   also Modified by NiQuiL (niquil@programmer.net)
	parv[0] - sender
	parv[1] - nick to make part
	parv[2] - channel(s) to part
*/
DLLFUNC CMD_FUNC(m_sapart)
{
	aClient *acptr;
	if (!IsSAdmin(sptr) && !IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SAPART");
		return 0;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, parv[0], parv[1]);
		return 0;
	}

	sendto_realops("%s used SAPART to make %s part %s", sptr->name, parv[1],
	    parv[2]);

 
	/* Logging function added by XeRXeS */
	ircd_log(LOG_SACMDS,"SAPART: %s used SAPART to make %s part %s", 
		sptr->name, parv[1], parv[2]);

	if (MyClient(acptr))
	{
		parv[0] = parv[1];
		parv[1] = parv[2];
		parv[2] = NULL;
		sendto_one(acptr,
		    ":%s %s %s :*** You were forced to part %s", me.name,
		    IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name, parv[1]);
		(void)m_part(acptr, acptr, 2, parv);
	}
	else
		sendto_one(acptr, ":%s SAPART %s %s", parv[0],
		    parv[1], parv[2]);

	return 0;
}
