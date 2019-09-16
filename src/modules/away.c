/*
 *   IRC - Internet Relay Chat, src/modules/away.c
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

CMD_FUNC(cmd_away);

#define MSG_AWAY 	"AWAY"	

ModuleHeader MOD_HEADER
  = {
	"away",
	"5.0",
	"command /away", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_AWAY, cmd_away, 1, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}
/***********************************************************************
 * cmd_away() - Added 14 Dec 1988 by jto.
 *            Not currently really working, I don't like this
 *            call at all...
 *
 *            ...trying to make it work. I don't like it either,
 *            but perhaps it's worth the load it causes to net.
 *            This requires flooding of the whole net like NICK,
 *            USER, MODE, etc messages...  --msa
 ***********************************************************************/

/*
** cmd_away
**      parv[1] = away message
*/
CMD_FUNC(cmd_away)
{
	char *new_reason = parv[1];
	int n, already_as_away = 0;
	MessageTag *mtags = NULL;

	if (IsServer(sptr))
		return 0;

	if (parc < 2 || !*new_reason)
	{
		/* Marking as not away */
		if (sptr->user->away)
		{
			safe_free(sptr->user->away);

			new_message(sptr, recv_mtags, &mtags);
			sendto_server(cptr, 0, 0, mtags, ":%s AWAY", sptr->name);
			hash_check_watch(cptr, RPL_NOTAWAY);
			sendto_local_common_channels(sptr, sptr, ClientCapabilityBit("away-notify"), mtags,
			                             ":%s AWAY", sptr->name);
			RunHook3(HOOKTYPE_AWAY, sptr, mtags, NULL);
			free_message_tags(mtags);
		}

		if (MyConnect(sptr))
			sendnumeric(sptr, RPL_UNAWAY);
		return 0;
	}

	/* Check spamfilters */
	if (MyUser(sptr))
	{
		n = run_spamfilter(sptr, new_reason, SPAMF_AWAY, NULL, 0, NULL);
		if (n < 0)
			return n;
	}

	/* Check set::anti-flood::away-flood */
	if (MyUser(sptr) && AWAY_PERIOD && !ValidatePermissionsForPath("immune:away-flood",sptr,NULL,NULL,NULL))
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
			sendnumeric(sptr, ERR_TOOMANYAWAY);
			return 0;
		}
	}

	/* Obey set::away-length */
	if (strlen(new_reason) > iConf.away_length)
		new_reason[iConf.away_length] = '\0';

	/* Check if the new away reason is the same as the current reason - if so then return (no change) */
	if ((sptr->user->away) && !strcmp(sptr->user->away, new_reason))
		return 0;

	/* All tests passed. Now marking as away (or still away but changing the away reason) */

	sptr->user->lastaway = TStime();
	
	new_message(sptr, recv_mtags, &mtags);

	sendto_server(cptr, 0, 0, mtags, ":%s AWAY :%s", sptr->name, new_reason);

	if (sptr->user->away)
	{
		safe_free(sptr->user->away);
		already_as_away = 1;
	}
	
	safe_strdup(sptr->user->away, new_reason);

	if (MyConnect(sptr))
		sendnumeric(sptr, RPL_NOWAWAY);

	hash_check_watch(cptr, already_as_away ? RPL_REAWAY : RPL_GONEAWAY);

	sendto_local_common_channels(sptr, sptr,
	                             ClientCapabilityBit("away-notify"), mtags,
	                             ":%s AWAY :%s", sptr->name, sptr->user->away);

	RunHook3(HOOKTYPE_AWAY, sptr, mtags, sptr->user->away);

	free_message_tags(mtags);

	return 0;
}
