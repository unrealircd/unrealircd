/*
 *   IRC - Internet Relay Chat, src/modules/m_away.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   away command
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

DLLFUNC int m_away(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_AWAY 	"AWAY"	
#define TOK_AWAY 	"6"	


#ifndef DYNAMIC_LINKING
ModuleHeader m_away_Header
#else
#define m_away_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"test",
	"$Id$",
	"command /away", 
	"3.2-b5",
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_away_Init(int module_load)
#endif
{
	add_Command(MSG_AWAY, TOK_AWAY, m_away, MAXPARA);
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_away_Load(int module_load)
#endif
{
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_away_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_AWAY, TOK_AWAY, m_away) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_away_Header.name);
	}
}
/***********************************************************************
 * m_away() - Added 14 Dec 1988 by jto.
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *            but perhaps it's worth the load it causes to net.
 *            This requires flooding of the whole net like NICK,
 *            USER, MODE, etc messages...  --msa
 ***********************************************************************/

/*
** m_away
**      parv[0] = sender prefix
**      parv[1] = away message
*/
int  m_away(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
        char *away, *awy2 = parv[1];


        away = sptr->user->away;
        if (parc < 2 || !*awy2)
        {
                /* Marking as not away */
                if (away)
                {
                        MyFree(away);
                        sptr->user->away = NULL;
                }
                /* hope this works XX */
                sendto_serv_butone_token(cptr, parv[0], MSG_AWAY, TOK_AWAY, "");
                if (MyConnect(sptr))
                        sendto_one(sptr, rpl_str(RPL_UNAWAY), me.name, parv[0]);
                return 0;
        }

        /* Marking as away */
        if (strlen(awy2) > (size_t)TOPICLEN)
                awy2[TOPICLEN] = '\0';

        if (away)
                if (strcmp(away, parv[1]) == 0)
                {
                        return 0;
                }
        sendto_serv_butone_token(cptr, parv[0], MSG_AWAY, TOK_AWAY, ":%s",
            awy2);

        if (away)
                away = (char *)MyRealloc(away, strlen(awy2) + 1);
        else
                away = (char *)MyMalloc(strlen(awy2) + 1);

        sptr->user->away = away;
        (void)strcpy(away, awy2);
        if (MyConnect(sptr))
                sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);
        return 0;
}
