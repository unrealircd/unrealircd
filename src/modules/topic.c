/*
 *   IRC - Internet Relay Chat, src/modules/topic.c
 *   (C) 2004-present The UnrealIRCd Team
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

CMD_FUNC(cmd_topic);

#define MSG_TOPIC 	"TOPIC"

ModuleHeader MOD_HEADER
  = {
	"topic",
	"5.0",
	"command /topic", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
void _set_channel_topic(Client *client, Channel *channel, MessageTag *recv_mtags, const char *topic, const char *set_by, time_t set_at);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_SET_CHANNEL_TOPIC, _set_channel_topic);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_TOPIC, cmd_topic, 4, CMD_USER|CMD_SERVER);
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

void topic_operoverride_msg(Client *client, Channel *channel, const char *topic)
{
	unreal_log(ULOG_INFO, "operoverride", "OPEROVERRIDE_TOPIC", client,
		   "OperOverride: $client.details changed the topic of $channel to '$topic'",
		   log_data_string("override_type", "topic"),
		   log_data_string("topic", topic),
		   log_data_channel("channel", channel));
}

/** Query or change the channel topic.
 *
 * Syntax for clients:
 * parv[1] = channel
 * parv[2] = new topic
 *
 * Syntax for server to server traffic:
 * parv[1] = channel name
 * parv[2] = topic nickname
 * parv[3] = topic time
 * parv[4] = topic text
 */
CMD_FUNC(cmd_topic)
{
	Channel *channel = NULL;
	const char *topic = NULL;
	const char *name, *tnick = client->name;
	const char *errmsg = NULL;
	char topicbuf[MAXTOPICLEN+1];
	time_t ttime = 0;
	int i = 0;
	Hook *h;
	MessageTag *mtags = NULL;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "TOPIC");
		return;
	}

	name = parv[1];

	channel = find_channel(parv[1]);
	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, name);
		return;
	}

	if (parc > 2 || SecretChannel(channel))
	{
		if (!IsMember(client, channel) && !IsServer(client)
		    && !ValidatePermissionsForPath("channel:see:list:secret",client,NULL,channel,NULL) && !IsULine(client))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, name);
			return;
		}
		if (parc > 2)
			topic = parv[2];
	}

	if (parc > 4)
	{
		if (MyUser(client))
		{
			sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC", "Invalid parameters. Usage is TOPIC #channel :topic here");
			return;
		}
		tnick = parv[2];
		ttime = atol(parv[3]);
		topic = parv[4];
	}

	/* Only asking for the topic */
	if (!topic)
	{
		if (IsServer(client))
			return; /* Servers must maintain state, not ask */

		for (h = Hooks[HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(client,channel);
			if (i != HOOK_CONTINUE)
				break;
		}

		/* If you're not a member, and you can't view outside channel, deny */
		if ((!IsMember(client, channel) && i == HOOK_DENY) ||
		    (is_banned(client,channel,BANCHK_JOIN,NULL,NULL) &&
		     !ValidatePermissionsForPath("channel:see:topic",client,NULL,channel,NULL)))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, name);
			return;
		}

		if (!channel->topic)
			sendnumeric(client, RPL_NOTOPIC, channel->name);
		else
		{
			sendnumeric(client, RPL_TOPIC, channel->name, channel->topic);
			sendnumeric(client, RPL_TOPICWHOTIME, channel->name,
			            channel->topic_nick, (long long)channel->topic_time);
		}
		return;
	}

	/* Cut topic here, because we are in BIGLINES code */
	strlcpy(topicbuf, topic, sizeof(topicbuf));
	topic = topicbuf;

	if (ttime && topic && (IsServer(client) || IsULine(client)))
	{
		if (!channel->topic_time || ttime > channel->topic_time || IsULine(client))
		/* The IsUline is to allow services to use an old TS. Apparently
		 * some services do this in their topic enforcement -- codemastr 
		 */
		{
			/* Set the topic */
			safe_strldup(channel->topic, topic, iConf.topic_length+1);
			safe_strldup(channel->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);
			channel->topic_time = ttime;

			new_message(client, recv_mtags, &mtags);
			RunHook(HOOKTYPE_TOPIC, client, channel, mtags, channel->topic);
			sendto_server(client, 0, 0, mtags, ":%s TOPIC %s %s %lld :%s",
			    client->id, channel->name, channel->topic_nick,
			    (long long)channel->topic_time, channel->topic);
			sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				       ":%s TOPIC %s :%s",
				       client->name, channel->name, channel->topic);
			free_message_tags(mtags);
		}
		return;
	}

	/* Topic change. Either locally (check permissions!) or remote, check permissions: */
	if (IsUser(client))
	{
		const char *newtopic = NULL;
		const char *errmsg = NULL;
		int ret = EX_ALLOW;
		int operoverride = 0;

		for (h = Hooks[HOOKTYPE_CAN_SET_TOPIC]; h; h = h->next)
		{
			int n = (*(h->func.intfunc))(client, channel, topic, &errmsg);

			if (n == EX_DENY)
			{
				ret = n;
			} else
			if (n == EX_ALWAYS_DENY)
			{
				ret = n;
				break;
			}
		}

		if (ret == EX_ALWAYS_DENY)
		{
			if (MyUser(client) && errmsg)
				sendto_one(client, NULL, "%s", errmsg); /* send error, if any */

			if (MyUser(client))
				return; /* reject the topic set (note: we never block remote sets) */
		}

		if (ret == EX_DENY)
		{
			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, channel, NULL))
			{
				if (errmsg)
					sendto_one(client, NULL, "%s", errmsg);
				return; /* reject */
			} else {
				operoverride = 1; /* allow */
			}
		}

		/* banned? */
		newtopic = topic;
		if (!check_channel_access(client, channel, "hoaq") && is_banned(client, channel, BANCHK_MSG, &newtopic, &errmsg))
		{
			char buf[512];

			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, channel, NULL))
			{
				ircsnprintf(buf, sizeof(buf), "You cannot change the topic on %s while being banned", channel->name);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return;
			}
			operoverride = 1;
		}

		if (MyUser(client) && newtopic)
			topic = newtopic; /* process is_banned() changes of topic (eg: text replacement), but only for local clients */

		if (operoverride)
			topic_operoverride_msg(client, channel, topic);

		/* For local users, run spamfilters and hooks.. */
		if (MyUser(client))
		{
			Hook *tmphook;
			int n;

			if (match_spamfilter(client, topic, SPAMF_TOPIC, "TOPIC", channel->name, 0, NULL))
				return;

			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_TOPIC]; tmphook; tmphook = tmphook->next) {
				topic = (*(tmphook->func.stringfunc))(client, channel, topic);
				if (!topic)
					return;
			}
		}

		/* At this point 'tnick' is set to client->name.
		 * If set::topic-setter nick-user-host; is set
		 * then we update it here to nick!user@host.
		 */
		if (iConf.topic_setter == SETTER_NICK_USER_HOST)
			tnick = make_nick_user_host(client->name, client->user->username, GetHost(client));
	}

	_set_channel_topic(client, channel, recv_mtags, topic, tnick, ttime);
}

/** Set topic on a channel.
 * @param client	The client setting the topic
 * @param channel	The channel
 * @param recv_mtags	Message tags
 * @param topic		The new topic (TODO: this function does not support unsetting yet)
 * @param set_by	Who set the topic (can be NULL, means client->name)
 * @param set_at	When the topic was set (can be 0, means now)
 */
void _set_channel_topic(Client *client, Channel *channel, MessageTag *recv_mtags, const char *topic, const char *set_by, time_t set_at)
{
	MessageTag *mtags = NULL;

	/* Set default values when needed */
	if (set_by == NULL)
		set_by = client->name;
	if (set_at == 0)
		set_at = TStime();

	/* Set the topic */
	safe_strldup(channel->topic, topic, iConf.topic_length+1);
	safe_strldup(channel->topic_nick, set_by, NICKLEN+USERLEN+HOSTLEN+5);
	channel->topic_time = set_at;

	/* And broadcast the change - locally and remote */
	new_message(client, recv_mtags, &mtags);
	RunHook(HOOKTYPE_TOPIC, client, channel, mtags, topic);
	sendto_server(client, 0, 0, mtags, ":%s TOPIC %s %s %lld :%s",
	    client->id, channel->name, channel->topic_nick,
	    (long long)channel->topic_time, channel->topic);
	sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
		       ":%s TOPIC %s :%s",
		       client->name, channel->name, channel->topic);
	free_message_tags(mtags);
}
