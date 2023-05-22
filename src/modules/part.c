/*
 *   IRC - Internet Relay Chat, src/modules/part.c
 *   (C) 2005 The UnrealIRCd Team
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

CMD_FUNC(cmd_part);

#define MSG_PART 	"PART"	

ModuleHeader MOD_HEADER
  = {
	"part",
	"5.0",
	"command /part", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PART, cmd_part, 2, CMD_USER);
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
** cmd_part
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
CMD_FUNC(cmd_part)
{
	char request[BUFSIZE];
	Channel *channel;
	Membership *lp;
	char *p = NULL, *name;
	const char *commentx = (parc > 2 && parv[2]) ? parv[2] : NULL;
	const char *comment;
	int n;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("PART");
	
	if (parc < 2 || parv[1][0] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "PART");
		return;
	}

	if (MyUser(client))
	{
		const char *str;
		if (IsShunned(client))
			commentx = NULL;
		if ((str = get_setting_for_user_string(client, SET_STATIC_PART)))
		{
			if (!strcasecmp(str, "yes") || !strcmp(str, "1"))
				commentx = NULL;
			else if (!strcasecmp(str, "no") || !strcmp(str, "0"))
				; /* keep original reason */
			else
				commentx = str;
		}
		if (commentx)
		{
			if (match_spamfilter(client, commentx, SPAMF_PART, "PART", parv[1], 0, NULL))
				commentx = NULL;
			if (IsDead(client))
				return;
		}
	}

	strlcpy(request, parv[1], sizeof(request));
	for (name = strtoken(&p, request, ","); name; name = strtoken(&p, NULL, ","))
	{
		MessageTag *mtags = NULL;

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "PART");
			break;
		}

		channel = find_channel(name);
		if (!channel)
		{
			sendnumeric(client, ERR_NOSUCHCHANNEL, name);
			continue;
		}

		/* 'commentx' is the general part msg, but it can be changed
		 * per-channel (eg some chans block badwords, strip colors, etc)
		 * so we copy it to 'comment' and use that in this for loop :)
		 */
		comment = commentx;

		if (!(lp = find_membership_link(client->user->channel, channel)))
		{
			/* Normal to get get when our client did a kick
			   ** for a remote client (who sends back a PART),
			   ** so check for remote client or not --Run
			 */
			if (MyUser(client))
				sendnumeric(client, ERR_NOTONCHANNEL, name);
			continue;
		}

		if (!ValidatePermissionsForPath("channel:override:banpartmsg",client,NULL,channel,NULL) && !check_channel_access(client, channel, "oaq")) {
			/* Banned? No comment allowed ;) */
			if (comment && is_banned(client, channel, BANCHK_MSG, &comment, NULL))
				comment = NULL;
			if (comment && is_banned(client, channel, BANCHK_LEAVE_MSG, &comment, NULL))
				comment = NULL;
		}

		if (MyConnect(client))
		{
			Hook *tmphook;
			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_PART]; tmphook; tmphook = tmphook->next) {
				comment = (*(tmphook->func.stringfunc))(client, channel, comment);
				if (!comment)
					break;
			}
		}

		/* Create a new message, this one is actually used by 8 calls (though at most 4 max) */
		new_message_special(client, recv_mtags, &mtags, ":%s PART %s", client->name, channel->name);

		/* Send to other servers... */
		sendto_server(client, 0, 0, mtags, ":%s PART %s :%s",
			client->id, channel->name, comment ? comment : "");

		if (invisible_user_in_channel(client, channel))
		{
			/* Show PART only to chanops and self */
			if (!comment)
			{
				sendto_channel(channel, client, client,
					       "ho", 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s",
					       client->name, channel->name);
				if (MyUser(client))
				{
					sendto_one(client, mtags, ":%s!%s@%s PART %s",
						client->name, client->user->username, GetHost(client), channel->name);
				}
			}
			else
			{
				sendto_channel(channel, client, client,
					       "ho", 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s %s",
					       client->name, channel->name, comment);
				if (MyUser(client))
				{
					sendto_one(client, mtags,
						":%s!%s@%s PART %s %s",
						client->name, client->user->username, GetHost(client),
						channel->name, comment);
				}
			}
		}
		else
		{
			/* Show PART to all users in channel */
			if (!comment)
			{
				sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s",
				               client->name, channel->name);
			} else {
				sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s :%s",
				               client->name, channel->name, comment);
			}
		}

		if (MyUser(client))
			RunHook(HOOKTYPE_LOCAL_PART, client, channel, mtags, comment);
		else
			RunHook(HOOKTYPE_REMOTE_PART, client, channel, mtags, comment);

		free_message_tags(mtags);

		remove_user_from_channel(client, channel, 0);
	}
}
