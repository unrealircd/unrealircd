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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_away(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_AWAY 	"AWAY"	
#define TOK_AWAY 	"6"	

DLLFUNC ModuleHeader MOD_HEADER(m_away)
  = {
	"m_away",
	"$Id$",
	"command /away", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_away)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_AWAY, TOK_AWAY, m_away, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_away)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_away)(int module_unload)
{
	return MOD_SUCCESS;
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
int  m_away(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
char *away, *awy2 = parv[1];
int n;

	if (IsServer(sptr))
		return 0;
        away = sptr->user->away;

        if (parc < 2 || !*awy2)
        {
                /* Marking as not away */
                if (away)
                {
                        MyFree(away);
                        sptr->user->away = NULL;
			/* Only send this if they were actually away -- codemastr */
	                sendto_serv_butone_token(cptr, parv[0], MSG_AWAY, TOK_AWAY, "");
	                hash_check_watch(cptr, RPL_NOTAWAY);
                }
                /* hope this works XX */
                if (MyConnect(sptr))
                        sendto_one(sptr, rpl_str(RPL_UNAWAY), me.name, parv[0]);
                return 0;
        }

    n = dospamfilter(sptr, parv[1], SPAMF_AWAY, NULL, 0, NULL);
    if (n < 0)
        return n;

#ifdef NO_FLOOD_AWAY
	if (MyClient(sptr) && AWAY_PERIOD && !IsAnOper(sptr))
	{
		if ((sptr->user->flood.away_t + AWAY_PERIOD) <= timeofday)
		{
			sptr->user->flood.away_c = 0;
			sptr->user->flood.away_t = timeofday;
		}
		if (sptr->user->flood.away_c <= AWAY_COUNT)
			sptr->user->flood.away_c++;
		if (sptr->user->flood.away_c > AWAY_COUNT)
		{
			sendto_one(sptr, err_str(ERR_TOOMANYAWAY), me.name, parv[0]);
			return 0;
		}
	}
#endif
        /* Marking as away */
        if (strlen(awy2) > (size_t)TOPICLEN)
                awy2[TOPICLEN] = '\0';

        if (away)
                if (strcmp(away, parv[1]) == 0)
                        return 0;

	sptr->user->lastaway = TStime();
	
        sendto_serv_butone_token(cptr, parv[0], MSG_AWAY, TOK_AWAY, ":%s", awy2);

	if (away)
		MyFree(away);
	
	away = sptr->user->away = strdup(awy2);

        if (MyConnect(sptr))
                sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, parv[0]);

	hash_check_watch(cptr, RPL_GONEAWAY);
	
        return 0;
}
