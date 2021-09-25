/*
 *   IRC - Internet Relay Chat, src/modules/knock.c
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

#include "unrealircd.h"

CMD_FUNC(cmd_knock);

#define MSG_KNOCK 	"KNOCK"	

ModuleHeader MOD_HEADER
  = {
	"knock",
	"5.0",
	"command /knock", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_KNOCK, cmd_knock, 2, CMD_USER);
	ISupportAdd(modinfo->handle, "KNOCK", NULL);
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

/*
** cmd_knock
**	parv[1] - channel
**	parv[2] - reason
**
** Coded by Stskeeps
** Additional bugfixes/ideas by codemastr
** (C) codemastr & Stskeeps
** 
** 2019-11-27: Behavior change. We now send the KNOCK
** across servers and only deliver the channel notice
** to local channel members. The reason for this is that
** otherwise we cannot count KNOCKs network-wide which
** caused knock-floods per-channel to be per-server
** rather than global, which undesirable.
** Unfortunately, this means that if you have a mixed
** U4 and U5 network you will see KNOCK notices twice
** for every attempt.
*/
CMD_FUNC(cmd_knock)
{
	Channel *channel;
	Hook *h;
	int i = 0;
	MessageTag *mtags = NULL;
	const char *reason;

	if (IsServer(client))
		return;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KNOCK");
		return;
	}

	reason = parv[2] ? parv[2] : "no reason specified";

	if (MyConnect(client) && !valid_channelname(parv[1]))
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
		return;
	}

	if (!(channel = find_channel(parv[1])))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, parv[1], "Channel does not exist!");
		return;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(client, channel) == 1)
	{
		sendnumeric(client, ERR_CANNOTKNOCK, channel->name, "You're already there!");
		return;
	}

	if (!has_channel_mode(channel, 'i'))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, channel->name, "Channel is not invite only!");
		return;
	}

	if (is_banned(client, channel, BANCHK_JOIN, NULL, NULL))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, channel->name, "You're banned!");
		return;
	}

	for (h = Hooks[HOOKTYPE_PRE_KNOCK]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client, channel, &reason);
		if (i == HOOK_DENY || i == HOOK_ALLOW)
			break;
	}

	if (i == HOOK_DENY)
		return;

	if (MyUser(client) &&
	    !ValidatePermissionsForPath("immune:knock-flood",client,NULL,NULL,NULL) &&
	    flood_limit_exceeded(client, FLD_KNOCK))
	{
		sendnumeric(client, ERR_CANNOTKNOCK, parv[1], "You are KNOCK flooding");
		return;
	}

	new_message(&me, NULL, &mtags);

	sendto_channel(channel, &me, NULL, "o",
	               0, SEND_LOCAL, mtags,
	               ":%s NOTICE @%s :[Knock] by %s!%s@%s (%s)",
	               me.name, channel->name,
	               client->name, client->user->username, GetHost(client),
	               reason);

	sendto_server(client, 0, 0, mtags, ":%s KNOCK %s :%s", client->id, channel->name, reason);

	if (MyUser(client))
		sendnotice(client, "Knocked on %s", channel->name);

        RunHook(HOOKTYPE_KNOCK, client, channel, mtags, parv[2]);

	free_message_tags(mtags);
}
