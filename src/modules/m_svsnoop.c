/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnoop.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnoop command
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

DLLFUNC int m_svsnoop(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSNOOP 	"SVSNOOP"	
#define TOK_SVSNOOP 	"f"


extern ircstats IRCstats;
extern int SVSNOOP;


#ifndef DYNAMIC_LINKING
ModuleHeader m_svsnoop_Header
#else
#define m_svsnoop_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"m_svsnoop",
	"$Id$",
	"command /svsnoop", 
	"3.2-b5",
	NULL
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_svsnoop_Init(int module_load)
#endif
{
	add_Command(MSG_SVSNOOP, TOK_SVSNOOP, m_svsnoop, MAXPARA);
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_svsnoop_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_svsnoop_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SVSNOOP, TOK_SVSNOOP, m_svsnoop) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svsnoop_Header.name);
	}
	return MOD_SUCCESS;
}
int m_svsnoop(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient *acptr;

        if (!(check_registered(sptr) && IsULine(sptr) && parc > 2))
                return 0;
        /* svsnoop bugfix --binary */
        if (hunt_server(cptr, sptr, ":%s SVSNOOP %s :%s", 1, parc,
            parv) == HUNTED_ISME)
        {
                if (parv[2][0] == '+')
                {
                        SVSNOOP = 1;
                        sendto_ops("This server has been placed in NOOP mode");
                        for (acptr = &me; acptr; acptr = acptr->prev)
                        {
                                if (MyClient(acptr) && IsAnOper(acptr))
                                {
                                        if (IsAnOper(acptr))
                                                IRCstats.operators--;
                                        acptr->umodes &=
                                            ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP | UMODE_SERVICES |
                                            UMODE_SADMIN | UMODE_ADMIN);
                                        acptr->umodes &=
                                                ~(UMODE_NETADMIN | UMODE_WHOIS);
                                        acptr->umodes &=
                                            ~(UMODE_KIX | UMODE_HIDING | UMODE_DEAF | UMODE_HIDEOPER);
                                        acptr->user->oflag = 0;
                                       acptr->user->snomask &= ~(SNO_CLIENT|SNO_FLOOD|SNO_FCLIENT|
                                                SNO_EYES|SNO_VHOST);

                                }
                        }

                }
                else
                {
                        SVSNOOP = 0;
                        sendto_ops("This server is no longer in NOOP mode");
                }
        }
}
