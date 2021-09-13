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
int away_join(Client *client, Channel *channel, MessageTag *mtags);

long CAP_AWAY_NOTIFY = 0L;

#define MSG_AWAY 	"AWAY"	

ModuleHeader MOD_HEADER
  = {
	"away",
	"5.0",
	"command /away", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	ClientCapabilityInfo c;
	memset(&c, 0, sizeof(c));
	c.name = "away-notify";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_AWAY_NOTIFY);
	CommandAdd(modinfo->handle, MSG_AWAY, cmd_away, 1, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_JOIN, 0, away_join);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_JOIN, 0, away_join);

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

int away_join(Client *client, Channel *channel, MessageTag *mtags)
{
	Member *lp;
	Client *acptr;
	int invisible = invisible_user_in_channel(client, channel);
	for (lp = channel->members; lp; lp = lp->next)
	{
		acptr = lp->client;

		if (!MyConnect(acptr))
			continue; /* only locally connected clients */

		if (invisible && !check_channel_access_member(lp, "hoaq") && (client != acptr))
			continue; /* skip non-ops if requested to (used for mode +D), but always send to 'client' */

		if (client->user->away && HasCapabilityFast(acptr, CAP_AWAY_NOTIFY))
		{
			MessageTag *mtags_away = NULL;
			new_message(client, NULL, &mtags_away);
			sendto_one(acptr, mtags_away, ":%s!%s@%s AWAY :%s",
			           client->name, client->user->username, GetHost(client), client->user->away);
			free_message_tags(mtags_away);
		}
	}
	return 0;
}

/** Mark client as AWAY or mark them as back (in case of empty reason) */
CMD_FUNC(cmd_away)
{
	char reason[512];
	int n, already_as_away = 0;
	MessageTag *mtags = NULL;

	if (IsServer(client))
		return;

	if (parc < 2 || BadPtr(parv[1]))
	{
		/* Marking as not away */
		if (client->user->away)
		{
			safe_free(client->user->away);

			new_message(client, recv_mtags, &mtags);
			sendto_server(client, 0, 0, mtags, ":%s AWAY", client->name);
			sendto_local_common_channels(client, client, CAP_AWAY_NOTIFY, mtags,
			                             ":%s AWAY", client->name);
			RunHook(HOOKTYPE_AWAY, client, mtags, NULL, 0);
			free_message_tags(mtags);
		}

		if (MyConnect(client))
			sendnumeric(client, RPL_UNAWAY);
		return;
	}

	/* Obey set::away-length */
	strlncpy(reason, parv[1], sizeof(reason), iConf.away_length);

	/* Check spamfilters */
	if (MyUser(client) && match_spamfilter(client, reason, SPAMF_AWAY, "AWAY", NULL, 0, NULL))
		return;

	/* Check away-flood */
	if (MyUser(client) &&
	    !ValidatePermissionsForPath("immune:away-flood",client,NULL,NULL,NULL) &&
	    flood_limit_exceeded(client, FLD_AWAY))
	{
		sendnumeric(client, ERR_TOOMANYAWAY);
		return;
	}

	/* Check if the new away reason is the same as the current reason - if so then return (no change) */
	if ((client->user->away) && !strcmp(client->user->away, reason))
		return;

	/* All tests passed. Now marking as away (or still away but changing the away reason) */

	client->user->away_since = TStime();
	
	new_message(client, recv_mtags, &mtags);

	sendto_server(client, 0, 0, mtags, ":%s AWAY :%s", client->id, reason);

	if (client->user->away)
	{
		safe_free(client->user->away);
		already_as_away = 1;
	}
	
	safe_strdup(client->user->away, reason);

	if (MyConnect(client))
		sendnumeric(client, RPL_NOWAWAY);

	sendto_local_common_channels(client, client,
	                             CAP_AWAY_NOTIFY, mtags,
	                             ":%s AWAY :%s", client->name, client->user->away);

	RunHook(HOOKTYPE_AWAY, client, mtags, client->user->away, already_as_away);

	free_message_tags(mtags);

	return;
}
