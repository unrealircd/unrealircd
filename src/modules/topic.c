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
	"unrealircd-5",
    };

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

void topicoverride(Client *client, Channel *channel, char *topic)
{
	sendto_snomask(SNO_EYES,
	    "*** OperOverride -- %s (%s@%s) TOPIC %s \'%s\'",
	    client->name, client->user->username, client->user->realhost,
	    channel->chname, topic);

	/* Logging implementation added by XeRXeS */
	ircd_log(LOG_OVERRIDE, "OVERRIDE: %s (%s@%s) TOPIC %s \'%s\'",
		client->name, client->user->username, client->user->realhost,
		channel->chname, topic);
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
	char *topic = NULL, *name, *tnick = client->name;
	char *errmsg = NULL;
	time_t ttime = 0;
	int i = 0;
	Hook *h;
	int ismember; /* cache: IsMember() */
	long flags = 0; /* cache: membership flags */
	MessageTag *mtags = NULL;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "TOPIC");
		return;
	}

	name = parv[1];

	channel = find_channel(parv[1], NULL);
	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, name);
		return;
	}

	ismember = IsMember(client, channel); /* CACHE */
	if (ismember)
		flags = get_access(client, channel); /* CACHE */

	if (parc > 2 || SecretChannel(channel))
	{
		if (!ismember && !IsServer(client)
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
		if ((!ismember && i == HOOK_DENY) ||
		    (is_banned(client,channel,BANCHK_JOIN,NULL,NULL) &&
		     !ValidatePermissionsForPath("channel:see:topic",client,NULL,channel,NULL)))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, name);
			return;
		}

		if (!channel->topic)
			sendnumeric(client, RPL_NOTOPIC, channel->chname);
		else
		{
			sendnumeric(client, RPL_TOPIC,
			    channel->chname, channel->topic);
			sendnumeric(client, RPL_TOPICWHOTIME, channel->chname,
			    channel->topic_nick, channel->topic_time);
		}
		return;
	}

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
			RunHook4(HOOKTYPE_TOPIC, client, channel, mtags, topic);
			sendto_server(client, 0, 0, mtags, ":%s TOPIC %s %s %lld :%s",
			    client->id, channel->chname, channel->topic_nick,
			    (long long)channel->topic_time, channel->topic);
			sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				       ":%s TOPIC %s :%s",
				       client->name, channel->chname, channel->topic);
			free_message_tags(mtags);
		}
		return;
	}

	/* Topic change. Either locally (check permissions!) or remote, check permissions: */
	if (IsUser(client))
	{
		char *newtopic = NULL;

		/* +t and not +hoaq ? */
		if ((channel->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(client, channel) && !IsULine(client) && !IsServer(client))
		{
			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, channel, NULL))
			{
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, channel->chname);
				return;
			}
			topicoverride(client, channel, topic);
		}

		/* -t and banned? */
		newtopic = topic;
		if (!(channel->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(client, channel) && is_banned(client, channel, BANCHK_MSG, &newtopic, &errmsg))
		{
			char buf[512];

			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, channel, NULL))
			{
				ircsnprintf(buf, sizeof(buf), "You cannot change the topic on %s while being banned", channel->chname);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return;
			}
			topicoverride(client, channel, topic);
		}
		if (MyUser(client) && newtopic)
			topic = newtopic; /* process is_banned() changes of topic (eg: text replacement), but only for local clients */

		/* -t, +m, and not +vhoaq */
		if (((flags&CHFL_OVERLAP) == 0) && (channel->mode.mode & MODE_MODERATED))
		{
			char buf[512];

			if (MyUser(client) && ValidatePermissionsForPath("channel:override:topic", client, NULL, channel, NULL))
			{
				topicoverride(client, channel, topic);
			} else {
				/* With +m and -t, only voice and higher may change the topic */
				ircsnprintf(buf, sizeof(buf), "Voice (+v) or higher is required in order to change the topic on %s (channel is +m)", channel->chname);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return;
			}
		}

		/* For local users, run spamfilters and hooks.. */
		if (MyUser(client))
		{
			Hook *tmphook;
			int n;

			if (match_spamfilter(client, topic, SPAMF_TOPIC, "TOPIC", channel->chname, 0, NULL))
				return;

			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_TOPIC]; tmphook; tmphook = tmphook->next) {
				topic = (*(tmphook->func.pcharfunc))(client, channel, topic);
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

	/* Set the topic */
	safe_strldup(channel->topic, topic, iConf.topic_length+1);
	safe_strldup(channel->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);

	if (ttime && !MyUser(client))
		channel->topic_time = ttime;
	else
		channel->topic_time = TStime();

	new_message(client, recv_mtags, &mtags);
	RunHook4(HOOKTYPE_TOPIC, client, channel, mtags, topic);
	sendto_server(client, 0, 0, mtags, ":%s TOPIC %s %s %lld :%s",
	    client->id, channel->chname, channel->topic_nick,
	    (long long)channel->topic_time, channel->topic);
	sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
		       ":%s TOPIC %s :%s",
		       client->name, channel->chname, channel->topic);
	free_message_tags(mtags);
}
