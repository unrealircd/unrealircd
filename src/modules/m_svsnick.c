/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnick.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnick command
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

DLLFUNC int m_svsnick(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSNICK 	"SVSNICK"	
#define TOK_SVSNICK 	"e"	


#ifndef DYNAMIC_LINKING
ModuleHeader m_svsnick_Header
#else
#define m_svsnick_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"test",
	"$Id$",
	"command /svsnick", 
	"3.2-b5",
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_svsnick_Init(int module_load)
#endif
{
	add_Command(MSG_SVSNICK, TOK_SVSNICK, m_svsnick, MAXPARA);
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_svsnick_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_svsnick_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SVSNICK, TOK_SVSNICK, m_svsnick) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_svsnick_Header.name);
	}
	return MOD_SUCCESS;
}
/*
** m_svsnick
**      parv[0] = sender
**      parv[1] = old nickname
**      parv[2] = new nickname
**      parv[3] = timestamp
*/
int  m_svsnick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient *acptr;

        if (!IsULine(sptr) || parc < 4 || (strlen(parv[2]) > NICKLEN))
		return -1;        /* This looks like an error anyway -Studded */
        if (!hunt_server(cptr, sptr, ":%s SVSNICK %s %s :%s", 1, parc, parv) != HUNTED_ISME)
        {
                if ((acptr = find_person(parv[1], NULL)))
                {
                        if (find_client(parv[2], NULL)) /* Collision */
                                return exit_client(cptr, acptr, sptr,
                                    "Nickname collision due to Services enforced "
                                    "nickname change, your nick was overruled");
                        if (do_nick_name(parv[2]) == 0)
                                return 0;
                        acptr->umodes &= ~UMODE_REGNICK;
                        acptr->lastnick = TS2ts(parv[3]);
                        sendto_common_channels(acptr, ":%s NICK :%s", parv[1],
                            parv[2]);
                        if (IsPerson(acptr))
                                add_history(acptr, 1);
                        sendto_serv_butone_token(NULL, parv[1], MSG_NICK,
                            TOK_NICK, "%s :%i", parv[2], TS2ts(parv[3]));
                        if (acptr->name[0])
                        {
				(void)del_from_client_hash_table(acptr->name, acptr);
                                if (IsPerson(acptr))
                                        hash_check_watch(acptr, RPL_LOGOFF);
                        }
                        if (MyClient(acptr))
                        {
                                RunHook2(HOOKTYPE_LOCAL_NICKCHANGE, acptr, parv[2]);
                        }
                        (void)strcpy(acptr->name, parv[2]);
                        (void)add_to_client_hash_table(parv[2], acptr);
                        if (IsPerson(acptr))
                                hash_check_watch(acptr, RPL_LOGON);

                }
        }
        return 0;
}
