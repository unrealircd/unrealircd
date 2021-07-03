/*
 *   IRC - Internet Relay Chat, src/modules/invite.c
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

CMD_FUNC(cmd_invite);

#define MSG_INVITE 	"INVITE"	

ModuleHeader MOD_HEADER
  = {
	"invite",
	"5.0",
	"command /invite", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_INVITE, cmd_invite, MAXPARA, CMD_USER);
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

/* Send the user their list of active invites */
void send_invite_list(Client *client)
{
	Link *inv;

	for (inv = client->user->invited; inv; inv = inv->next)
	{
		sendnumeric(client, RPL_INVITELIST,
			   inv->value.channel->chname);	
	}
	sendnumeric(client, RPL_ENDOFINVITELIST);
}

/*
** cmd_invite
**	parv[1] - user to invite
**	parv[2] - channel number
*/
CMD_FUNC(cmd_invite)
{
	Client *target;
	Channel *channel;
	int override = 0;
	int i = 0;
	Hook *h;

	if (parc == 1)
	{
		send_invite_list(client);
		return;
	}
	
	if (parc < 3 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "INVITE");
		return;
	}

	if (!(target = find_person(parv[1], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	if (MyConnect(client) && !valid_channelname(parv[2]))
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[2]);
		return;
	}

	if (!(channel = find_channel(parv[2], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[2]);
		return;
	}

	for (h = Hooks[HOOKTYPE_PRE_INVITE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client,target,channel,&override);
		if (i == HOOK_DENY)
			return;
		if (i == HOOK_ALLOW)
			break;
	}

	if (!IsMember(client, channel) && !IsULine(client))
	{
		if (ValidatePermissionsForPath("channel:override:invite:notinchannel",client,NULL,channel,NULL) && client == target)
		{
			override = 1;
		} else {
			sendnumeric(client, ERR_NOTONCHANNEL, parv[2]);
			return;
		}
	}

	if (IsMember(target, channel))
	{
		sendnumeric(client, ERR_USERONCHANNEL, parv[1], parv[2]);
		return;
	}

	if (channel->mode.mode & MODE_INVITEONLY)
	{
		if (!is_chan_op(client, channel) && !IsULine(client))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",client,NULL,channel,NULL) && client == target)
			{
				override = 1;
			} else {
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->chname);
				return;
			}
		}
		else if (!IsMember(client, channel) && !IsULine(client))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",client,NULL,channel,NULL) && client == target)
			{
				override = 1;
			} else {
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->chname);
				return;
			}
		}
	}

	if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
	    !strcasecmp(channel->chname, SPAMFILTER_VIRUSCHAN) &&
	    !is_chan_op(client, channel) && !ValidatePermissionsForPath("immune:server-ban:viruschan",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->chname);
		return;
	}

	if (MyUser(client))
	{
		if (target_limit_exceeded(client, target, target->name))
			return;

		if (!ValidatePermissionsForPath("immune:invite-flood",client,NULL,NULL,NULL) &&
		    flood_limit_exceeded(client, FLD_INVITE))
		{
			sendnumeric(client, RPL_TRYAGAIN, "INVITE");
			return;
		}

		if (!override)
		{
			sendnumeric(client, RPL_INVITING, target->name, channel->chname);
			if (target->user->away)
			{
				sendnumeric(client, RPL_AWAY, target->name, target->user->away);
			}
		}
	}

	/* Send OperOverride messages */
	if (override && MyConnect(target))
	{
		if (is_banned(client, channel, BANCHK_JOIN, NULL, NULL))
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +b).",
			  client->name, client->user->username, client->user->realhost, channel->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Ban).",
				client->name, client->user->username, client->user->realhost, channel->chname);

		}
		else if (channel->mode.mode & MODE_INVITEONLY)
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +i).",
			  client->name, client->user->username, client->user->realhost, channel->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Invite Only)",
				client->name, client->user->username, client->user->realhost, channel->chname);

		}
		else if (channel->mode.limit)
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +l).",
			  client->name, client->user->username, client->user->realhost, channel->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Limit)",
				client->name, client->user->username, client->user->realhost, channel->chname);

		}

		else if (*channel->mode.key)
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +k).",
			  client->name, client->user->username, client->user->realhost, channel->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Key)",
				client->name, client->user->username, client->user->realhost, channel->chname);

		}
		else if (has_channel_mode(channel, 'z'))
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +z).",
			  client->name, client->user->username, client->user->realhost, channel->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding SSL/TLS-Only)",
				client->name, client->user->username, client->user->realhost, channel->chname);
		}
#ifdef OPEROVERRIDE_VERIFY
		else if (channel->mode.mode & MODE_SECRET || channel->mode.mode & MODE_PRIVATE)
		       override = -1;
#endif
		else
			return;
	}

	if (MyConnect(target))
	{
		if (IsUser(client) 
		    && (is_chan_op(client, channel)
		    || IsULine(client)
		    || ValidatePermissionsForPath("channel:override:invite:self",client,NULL,channel,NULL)
		    ))
		{
			MessageTag *mtags = NULL;

			new_message(&me, NULL, &mtags);
			if (override == 1)
			{
				sendto_channel(channel, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				               0, SEND_ALL, mtags,
				               ":%s NOTICE @%s :OperOverride -- %s invited him/herself into the channel.",
				               me.name, channel->chname, client->name);
			} else
			if (override == 0)
			{
				sendto_channel(channel, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				               0, SEND_ALL, mtags,
				               ":%s NOTICE @%s :%s invited %s into the channel.",
				               me.name, channel->chname, client->name, target->name);
			}
			add_invite(client, target, channel, mtags);
			free_message_tags(mtags);
		}
	}

	/* Notify the person who got invited */
	if (!is_silenced(client, target))
	{
		MessageTag *mtags = NULL;

		new_message(client, NULL, &mtags);
		sendto_prefix_one(target, client, mtags, ":%s INVITE %s :%s", client->name,
			target->name, channel->chname);

		free_message_tags(mtags);
	}
}
