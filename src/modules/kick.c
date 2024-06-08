/*
 *   IRC - Internet Relay Chat, src/modules/kick.c
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

ModuleHeader MOD_HEADER
  = {
	"kick",
	"5.0",
	"command /kick",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
CMD_FUNC(cmd_kick);
void _kick_user(MessageTag *mtags, Channel *channel, Client *client, Client *victim, char *comment);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_KICK_USER, _kick_user);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, "KICK", cmd_kick, 3, CMD_USER|CMD_SERVER);
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

void kick_operoverride_msg(Client *client, Channel *channel, Client *target, char *reason)
{
	unreal_log(ULOG_INFO, "operoverride", "OPEROVERRIDE_KICK", client,
		   "OperOverride: $client.details kicked $target from $channel ($reason)",
		   log_data_string("override_type", "kick"),
		   log_data_string("reason", reason),
		   log_data_client("target", target),
		   log_data_channel("channel", channel));
}

/** Kick a user from a channel.
 * @param initial_mtags	Message tags associated with this KICK (can be NULL)
 * @param channel	The channel where the KICK should happen
 * @param client	The evil user doing the kick, can be &me
 * @param victim	The target user that will be kicked
 * @param comment	The KICK comment (cannot be NULL)
 * @notes The msgid in initial_mtags is actually used as a prefix.
 *        The actual mtag will be "initial_mtags_msgid-suffix_msgid"
 *        All this is done in order for message tags to be
 *        consistent accross servers.
 *        The suffix is necessary to handle multi-target-kicks.
 *        If initial_mtags is NULL then we will autogenerate one.
 */
void _kick_user(MessageTag *initial_mtags, Channel *channel, Client *client, Client *victim, char *comment)
{
	MessageTag *mtags = NULL;
	int initial_mtags_generated = 0;

	if (!initial_mtags)
	{
		/* Yeah, we allow callers to be lazy.. */
		initial_mtags_generated = 1;
		new_message(client, NULL, &initial_mtags);
	}

	new_message_special(client, initial_mtags, &mtags, ":%s KICK %s %s", client->name, channel->name, victim->name);
	/* The same message is actually sent at 5 places below (though max 4 at most) */

	if (MyUser(client))
		RunHook(HOOKTYPE_LOCAL_KICK, client, victim, channel, mtags, comment);
	else
		RunHook(HOOKTYPE_REMOTE_KICK, client, victim, channel, mtags, comment);

	if (invisible_user_in_channel(victim, channel))
	{
		/* Send it only to chanops & victim */
		sendto_channel(channel, client, victim,
			       "h", 0,
			       SEND_LOCAL, mtags,
			       ":%s KICK %s %s :%s",
			       client->name, channel->name, victim->name, comment);

		if (MyUser(victim))
		{
			sendto_prefix_one(victim, client, mtags, ":%s KICK %s %s :%s",
				client->name, channel->name, victim->name, comment);
		}
	} else {
		/* NORMAL */
		sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
			       ":%s KICK %s %s :%s",
			       client->name, channel->name, victim->name, comment);
	}

	sendto_server(client, 0, 0, mtags, ":%s KICK %s %s :%s",
	    client->id, channel->name, victim->id, comment);

	free_message_tags(mtags);
	if (initial_mtags_generated)
	{
		free_message_tags(initial_mtags);
		initial_mtags = NULL;
	}

	if (MyUser(victim))
	{
		unreal_log(ULOG_INFO, "kick", "LOCAL_CLIENT_KICK", victim,
		           "User $client kicked from $channel",
		           log_data_channel("channel", channel));
	} else {
		unreal_log(ULOG_INFO, "kick", "REMOTE_CLIENT_KICK", victim,
		           "User $client kicked from $channel",
		           log_data_channel("channel", channel));
	}

	remove_user_from_channel(victim, channel, 1);
}

/*
** cmd_kick
**	parv[1] = channel (single channel)
**	parv[2] = client to kick (comma separated)
**	parv[3] = kick comment
*/

CMD_FUNC(cmd_kick)
{
	Client *target;
	Channel *channel;
	int  chasing = 0;
	char *p = NULL, *user, *p2 = NULL, *badkick;
	char comment[MAXKICKLEN+1];
	Membership *lp;
	Hook *h;
	int ret;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("KICK");
	MessageTag *mtags;
	char request[BUFSIZE];
	char request_chans[BUFSIZE];
	const char *client_member_modes = NULL;
	const char *target_member_modes;

	if (parc < 3 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KICK");
		return;
	}

	if (BadPtr(parv[3]))
		strlcpy(comment, client->name, sizeof(comment));
	else
		strlncpy(comment, parv[3], sizeof(comment), iConf.kick_length);

	strlcpy(request_chans, parv[1], sizeof(request_chans));
	p = strchr(request_chans, ',');
	if (p)
		*p = '\0';
	channel = find_channel(request_chans);
	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, request_chans);
		return;
	}

	/* Store "client" access flags */
	if (IsUser(client))
		client_member_modes = get_channel_access(client, channel);
	if (MyUser(client) && !IsULine(client) &&
	    !op_can_override("channel:override:kick:no-ops",client,channel,NULL) &&
	    !check_channel_access(client, channel, "hoaq"))
	{
		sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->name);
		return;
	}

	strlcpy(request, parv[2], sizeof(request));
	for (user = strtoken(&p2, request, ","); user; user = strtoken(&p2, NULL, ","))
	{
		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, user, maxtargets, "KICK");
			break;
		}

		if (!(target = find_chasing(client, user, &chasing)))
			continue;	/* No such user left! */

		if (!target->user)
			continue; /* non-user */

		lp = find_membership_link(target->user->channel, channel);
		if (!lp)
		{
			if (MyUser(client))
				sendnumeric(client, ERR_USERNOTINCHANNEL, user, request_chans);
			continue;
		}

		if (IsULine(client) || IsServer(client) || IsMe(client))
			goto attack;

		/* Note for coders regarding oper override:
		 * always let a remote kick (=from a user on another server) through or
		 * else we will get desynced. In short this means all the denying should
		 * always contain a && MyUser(client) and at the end
		 * a remote kick should always be allowed (pass through). -- Syzop
		 */

		/* Store "target" access flags */
		target_member_modes = get_channel_access(target, channel);

		badkick = NULL;
		ret = EX_ALLOW;
		for (h = Hooks[HOOKTYPE_CAN_KICK]; h; h = h->next) {
			int n = (*(h->func.intfunc))(client, target, channel, comment, client_member_modes, target_member_modes, &badkick);

			if (n == EX_DENY)
				ret = n;
			else if (n == EX_ALWAYS_DENY)
			{
				ret = n;
				break;
			}
		}

		if (ret == EX_ALWAYS_DENY)
		{
			if (MyUser(client) && badkick)
				sendto_one(client, NULL, "%s", badkick); /* send error, if any */

			if (MyUser(client))
				continue; /* reject the kick (note: we never block remote kicks) */
		}

		if (ret == EX_DENY)
		{
			/* If set it means 'not allowed to kick'.. now check if (s)he can override that.. */
			if (op_can_override("channel:override:kick:no-ops",client,channel,NULL))
			{
				kick_operoverride_msg(client, channel, target, comment);
				goto attack; /* all other checks don't matter anymore (and could cause double msgs) */
			} else {
				/* Not an oper overriding */
				if (MyUser(client) && badkick)
					sendto_one(client, NULL, "%s", badkick); /* send error, if any */

				continue; /* reject the kick */
			}
		}

		// FIXME: Most, maybe even all, of these must be moved to HOOKTYPE_CAN_KICK checks in the corresponding halfop/chanop/chanadmin/chanowner modules :)
		// !!!! FIXME

		/* we are neither +o nor +h, OR..
		 * we are +h but target is +o, OR...
		 * we are +h and target is +h
		 */
		if (op_can_override("channel:override:kick:no-ops",client,channel,NULL))
		{
			if ((!check_channel_access_string(client_member_modes, "o") && !check_channel_access_string(client_member_modes, "h")) ||
			    (check_channel_access_string(client_member_modes, "h") && check_channel_access_string(target_member_modes, "h")) ||
			    (check_channel_access_string(client_member_modes, "h") && check_channel_access_string(target_member_modes, "o")))
			{
				if (IsOper(client) && client != target)
					kick_operoverride_msg(client, channel, target, comment);
				goto attack;
			}	/* is_chan_op */

		}

		/* target is +a/+q, and we are not +q? */
		if (check_channel_access_string(target_member_modes, "qa") && !check_channel_access_string(client_member_modes, "q"))
		{
			if (client == target)
				goto attack; /* kicking self == ok */
			if (op_can_override("channel:override:kick:owner",client,channel,NULL)) /* (and f*ck local ops) */
			{
				/* IRCop kicking owner/prot */
				kick_operoverride_msg(client, channel, target, comment);
				goto attack;
			}
			else if (!IsULine(client) && (target != client) && MyUser(client))
			{
				char errbuf[NICKLEN+25];
				if (check_channel_access_string(target_member_modes, "q"))
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel owner", target->name);
				else
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel admin", target->name);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "KICK", errbuf);
				goto deny;
			}
		}

		/* target is +o, we are +h [operoverride is already taken care of 2 blocks above] */
		if (check_channel_access_string(target_member_modes, "h") && check_channel_access_string(client_member_modes, "h")
		    && !check_channel_access_string(client_member_modes, "o") && !IsULine(client) && MyUser(client))
		{
			char errbuf[NICKLEN+30];
			ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel operator", target->name);
			sendnumeric(client, ERR_CANNOTDOCOMMAND, "KICK",
				   errbuf);
			goto deny;
		}

		/* target is +h, we are +h [operoverride is already taken care of 3 blocks above] */
		if (check_channel_access_string(target_member_modes, "o") && check_channel_access_string(client_member_modes, "h")
		    && !check_channel_access_string(client_member_modes, "o") && MyUser(client))
		{
			char errbuf[NICKLEN+15];
			ircsnprintf(errbuf, sizeof(errbuf), "%s is a halfop", target->name);
			sendnumeric(client, ERR_CANNOTDOCOMMAND, "KICK",
				   errbuf);
			goto deny;
		}	/* halfop */

		/* allowed (either coz access granted or a remote kick), so attack! */
		goto attack;

	      deny:
		continue;

	      attack:
		if (MyConnect(client)) {
			int breakit = 0;
			Hook *h;
			for (h = Hooks[HOOKTYPE_PRE_LOCAL_KICK]; h; h = h->next) {
				if ((*(h->func.intfunc))(client,target,channel,comment) > 0) {
					breakit = 1;
					break;
				}
			}
			if (breakit)
				continue;
		}

		kick_user(recv_mtags, channel, client, target, comment);
	}
}
