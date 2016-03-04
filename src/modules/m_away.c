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

#include "unrealircd.h"

CMD_FUNC(m_away);

#define MSG_AWAY 	"AWAY"	

DLLFUNC ModuleHeader MOD_HEADER(m_away)
  = {
	"m_away",
	"4.0",
	"command /away", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_away)
{
	CommandAdd(modinfo->handle, MSG_AWAY, m_away, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_away)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_away)
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
**      parv[1] = away message
*/
CMD_FUNC(m_away)
{
char *away, *awy2 = parv[1];
int n, wasaway = 0;

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
	                sendto_server(cptr, 0, 0, ":%s AWAY", sptr->name);
	                hash_check_watch(cptr, RPL_NOTAWAY);

			sendto_common_channels_local_butone(sptr, PROTO_AWAY_NOTIFY, ":%s AWAY", sptr->name);
                }
                /* hope this works XX */
                if (MyConnect(sptr))
                        sendto_one(sptr, rpl_str(RPL_UNAWAY), me.name, sptr->name);
				RunHook2(HOOKTYPE_AWAY, sptr, NULL);
                return 0;
        }

    n = dospamfilter(sptr, parv[1], SPAMF_AWAY, NULL, 0, NULL);
    if (n < 0)
        return n;

#ifdef NO_FLOOD_AWAY
	if (MyClient(sptr) && AWAY_PERIOD && !ValidatePermissionsForPath("immune:awayflood",sptr,NULL,NULL,NULL))
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
			sendto_one(sptr, err_str(ERR_TOOMANYAWAY), me.name, sptr->name);
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
	
        sendto_server(cptr, 0, 0, ":%s AWAY :%s", sptr->name, awy2);

	if (away)
	{
		MyFree(away);
		wasaway = 1;
        }
	
	away = sptr->user->away = strdup(awy2);

        if (MyConnect(sptr))
                sendto_one(sptr, rpl_str(RPL_NOWAWAY), me.name, sptr->name);

	hash_check_watch(cptr, wasaway ? RPL_REAWAY : RPL_GONEAWAY);
	sendto_common_channels_local_butone(sptr, PROTO_AWAY_NOTIFY, ":%s AWAY :%s", sptr->name, away);

	RunHook2(HOOKTYPE_AWAY, sptr, away);
        return 0;
}
