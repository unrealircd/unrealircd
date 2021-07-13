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
	"unrealircd-5",
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
	Channel *channel;
	Membership *lp;
	char *p = NULL, *name;
	char *commentx = (parc > 2 && parv[2]) ? parv[2] : NULL;
	char *comment;
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
		if (IsShunned(client))
			commentx = NULL;
		if (STATIC_PART)
		{
			if (!strcasecmp(STATIC_PART, "yes") || !strcmp(STATIC_PART, "1"))
				commentx = NULL;
			else if (!strcasecmp(STATIC_PART, "no") || !strcmp(STATIC_PART, "0"))
				; /* keep original reason */
			else
				commentx = STATIC_PART;
		}
		if (commentx)
		{
			if (match_spamfilter(client, commentx, SPAMF_PART, "PART", parv[1], 0, NULL))
				commentx = NULL;
			if (IsDead(client))
				return;
		}
	}

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		MessageTag *mtags = NULL;

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "PART");
			break;
		}

		channel = get_channel(client, name, 0);
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

		if (!ValidatePermissionsForPath("channel:override:banpartmsg",client,NULL,channel,NULL) && !is_chan_op(client, channel)) {
			/* Banned? No comment allowed ;) */
			if (comment && is_banned(client, channel, BANCHK_MSG, &comment, NULL))
				comment = NULL;
			if (comment && is_banned(client, channel, BANCHK_LEAVE_MSG, &comment, NULL))
				comment = NULL;
			/* Same for +m */
			if ((channel->mode.mode & MODE_MODERATED) && comment &&
				 !has_voice(client, channel) && !is_half_op(client, channel))
			{
				comment = NULL;
			}
		}

		if (MyConnect(client))
		{
			Hook *tmphook;
			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_PART]; tmphook; tmphook = tmphook->next) {
				comment = (*(tmphook->func.pcharfunc))(client, channel, comment);
				if (!comment)
					break;
			}
		}

		/* Create a new message, this one is actually used by 8 calls (though at most 4 max) */
		new_message_special(client, recv_mtags, &mtags, ":%s PART %s", client->name, channel->chname);

		/* Send to other servers... */
		sendto_server(client, 0, 0, mtags, ":%s PART %s :%s",
			client->id, channel->chname, comment ? comment : "");

		if (invisible_user_in_channel(client, channel))
		{
			/* Show PART only to chanops and self */
			if (!comment)
			{
				sendto_channel(channel, client, client,
					       PREFIX_HALFOP|PREFIX_OP|PREFIX_OWNER|PREFIX_ADMIN, 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s",
					       client->name, channel->chname);
				if (MyUser(client))
				{
					sendto_one(client, mtags, ":%s!%s@%s PART %s",
						client->name, client->user->username, GetHost(client), channel->chname);
				}
			}
			else
			{
				sendto_channel(channel, client, client,
					       PREFIX_HALFOP|PREFIX_OP|PREFIX_OWNER|PREFIX_ADMIN, 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s %s",
					       client->name, channel->chname, comment);
				if (MyUser(client))
				{
					sendto_one(client, mtags,
						":%s!%s@%s PART %s %s",
						client->name, client->user->username, GetHost(client),
						channel->chname, comment);
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
				               client->name, channel->chname);
			} else {
				sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s :%s",
				               client->name, channel->chname, comment);
			}
		}

		if (MyUser(client))
			RunHook4(HOOKTYPE_LOCAL_PART, client, channel, mtags, comment);
		else
			RunHook4(HOOKTYPE_REMOTE_PART, client, channel, mtags, comment);

		free_message_tags(mtags);

		remove_user_from_channel(client, channel);
	}
}
