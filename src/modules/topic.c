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

void topicoverride(Client *client, Channel *chptr, char *topic)
{
	sendto_snomask(SNO_EYES,
	    "*** OperOverride -- %s (%s@%s) TOPIC %s \'%s\'",
	    client->name, client->user->username, client->user->realhost,
	    chptr->chname, topic);

	/* Logging implementation added by XeRXeS */
	ircd_log(LOG_OVERRIDE, "OVERRIDE: %s (%s@%s) TOPIC %s \'%s\'",
		client->name, client->user->username, client->user->realhost,
		chptr->chname, topic);
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
	Channel *chptr = NULL;
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

	chptr = find_channel(parv[1], NULL);
	if (!chptr)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, name);
		return;
	}

	ismember = IsMember(client, chptr); /* CACHE */
	if (ismember)
		flags = get_access(client, chptr); /* CACHE */

	if (parc > 2 || SecretChannel(chptr))
	{
		if (!ismember && !IsServer(client)
		    && !ValidatePermissionsForPath("channel:see:list:secret",client,NULL,chptr,NULL) && !IsULine(client))
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
			i = (*(h->func.intfunc))(client,chptr);
			if (i != HOOK_CONTINUE)
				break;
		}

		/* If you're not a member, and you can't view outside channel, deny */
		if ((!ismember && i == HOOK_DENY) ||
		    (is_banned(client,chptr,BANCHK_JOIN,NULL,NULL) &&
		     !ValidatePermissionsForPath("channel:see:topic",client,NULL,chptr,NULL)))
		{
			sendnumeric(client, ERR_NOTONCHANNEL, name);
			return;
		}

		if (!chptr->topic)
			sendnumeric(client, RPL_NOTOPIC, chptr->chname);
		else
		{
			sendnumeric(client, RPL_TOPIC,
			    chptr->chname, chptr->topic);
			sendnumeric(client, RPL_TOPICWHOTIME, chptr->chname,
			    chptr->topic_nick, chptr->topic_time);
		}
		return;
	}

	if (ttime && topic && (IsServer(client) || IsULine(client)))
	{
		if (!chptr->topic_time || ttime > chptr->topic_time || IsULine(client))
		/* The IsUline is to allow services to use an old TS. Apparently
		 * some services do this in their topic enforcement -- codemastr 
		 */
		{
			/* Set the topic */
			safe_strldup(chptr->topic, topic, iConf.topic_length+1);
			safe_strldup(chptr->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);
			chptr->topic_time = ttime;

			new_message(client, recv_mtags, &mtags);
			RunHook4(HOOKTYPE_TOPIC, client, chptr, mtags, topic);
			sendto_server(client, PROTO_SID, 0, mtags, ":%s TOPIC %s %s %lld :%s",
			    ID(client), chptr->chname, chptr->topic_nick,
			    (long long)chptr->topic_time, chptr->topic);
			sendto_server(client, 0, PROTO_SID, mtags, ":%s TOPIC %s %s %lld :%s",
			    client->name, chptr->chname, chptr->topic_nick,
			    (long long)chptr->topic_time, chptr->topic);
			sendto_channel(chptr, client, NULL, 0, 0, SEND_LOCAL, mtags,
				       ":%s TOPIC %s :%s",
				       client->name, chptr->chname, chptr->topic);
			free_message_tags(mtags);
		}
		return;
	}

	/* Topic change. Either locally (check permissions!) or remote, check permissions: */
	if (IsUser(client))
	{
		char *newtopic = NULL;

		/* +t and not +hoaq ? */
		if ((chptr->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(client, chptr) && !IsULine(client) && !IsServer(client))
		{
			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, chptr, NULL))
			{
				sendnumeric(client, ERR_CHANOPRIVSNEEDED, chptr->chname);
				return;
			}
			topicoverride(client, chptr, topic);
		}

		/* -t and banned? */
		newtopic = topic;
		if (!(chptr->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(client, chptr) && is_banned(client, chptr, BANCHK_MSG, &newtopic, &errmsg))
		{
			char buf[512];

			if (MyUser(client) && !ValidatePermissionsForPath("channel:override:topic", client, NULL, chptr, NULL))
			{
				ircsnprintf(buf, sizeof(buf), "You cannot change the topic on %s while being banned", chptr->chname);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return;
			}
			topicoverride(client, chptr, topic);
		}
		if (MyUser(client) && newtopic)
			topic = newtopic; /* process is_banned() changes of topic (eg: text replacement), but only for local clients */

		/* -t, +m, and not +vhoaq */
		if (((flags&CHFL_OVERLAP) == 0) && (chptr->mode.mode & MODE_MODERATED))
		{
			char buf[512];

			if (MyUser(client) && ValidatePermissionsForPath("channel:override:topic", client, NULL, chptr, NULL))
			{
				topicoverride(client, chptr, topic);
			} else {
				/* With +m and -t, only voice and higher may change the topic */
				ircsnprintf(buf, sizeof(buf), "Voice (+v) or higher is required in order to change the topic on %s (channel is +m)", chptr->chname);
				sendnumeric(client, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return;
			}
		}

		/* For local users, run spamfilters and hooks.. */
		if (MyUser(client))
		{
			Hook *tmphook;
			int n;

			if (match_spamfilter(client, topic, SPAMF_TOPIC, chptr->chname, 0, NULL))
				return;

			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_TOPIC]; tmphook; tmphook = tmphook->next) {
				topic = (*(tmphook->func.pcharfunc))(client, chptr, topic);
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
	safe_strldup(chptr->topic, topic, iConf.topic_length+1);
	safe_strldup(chptr->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);

	if (ttime && !MyUser(client))
		chptr->topic_time = ttime;
	else
		chptr->topic_time = TStime();

	new_message(client, recv_mtags, &mtags);
	RunHook4(HOOKTYPE_TOPIC, client, chptr, mtags, topic);
	sendto_server(client, 0, 0, mtags, ":%s TOPIC %s %s %lld :%s",
	    client->name, chptr->chname, chptr->topic_nick,
	    (long long)chptr->topic_time, chptr->topic);
	sendto_channel(chptr, client, NULL, 0, 0, SEND_LOCAL, mtags,
		       ":%s TOPIC %s :%s",
		       client->name, chptr->chname, chptr->topic);
	free_message_tags(mtags);
}
