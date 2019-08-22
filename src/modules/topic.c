/*
 *   IRC - Internet Relay Chat, src/modules/m_topic.c
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

CMD_FUNC(m_topic);

#define MSG_TOPIC 	"TOPIC"

ModuleHeader MOD_HEADER(topic)
  = {
	"topic",
	"5.0",
	"command /topic", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(topic)
{
	CommandAdd(modinfo->handle, MSG_TOPIC, m_topic, 4, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(topic)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(topic)
{
	return MOD_SUCCESS;
}

void topicoverride(aClient *sptr, aChannel *chptr, char *topic)
{
	sendto_snomask(SNO_EYES,
	    "*** OperOverride -- %s (%s@%s) TOPIC %s \'%s\'",
	    sptr->name, sptr->user->username, sptr->user->realhost,
	    chptr->chname, topic);

	/* Logging implementation added by XeRXeS */
	ircd_log(LOG_OVERRIDE, "OVERRIDE: %s (%s@%s) TOPIC %s \'%s\'",
		sptr->name, sptr->user->username, sptr->user->realhost,
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
CMD_FUNC(m_topic)
{
	aChannel *chptr = NULL;
	char *topic = NULL, *name, *tnick = sptr->name;
	char *errmsg = NULL;
	time_t ttime = 0;
	int i = 0;
	Hook *h;
	int ismember; /* cache: IsMember() */
	long flags = 0; /* cache: membership flags */
	MessageTag *mtags = NULL;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "TOPIC");
		return 0;
	}

	name = parv[1];

	chptr = find_channel(parv[1], NULL);
	if (!chptr)
	{
		sendnumeric(sptr, ERR_NOSUCHCHANNEL, name);
		return 0;
	}

	ismember = IsMember(sptr, chptr); /* CACHE */
	if (ismember)
		flags = get_access(sptr, chptr); /* CACHE */

	if (parc > 2 || SecretChannel(chptr))
	{
		if (!ismember && !IsServer(sptr)
		    && !ValidatePermissionsForPath("channel:see:list:secret",sptr,NULL,chptr,NULL) && !IsULine(sptr))
		{
			sendnumeric(sptr, ERR_NOTONCHANNEL, name);
			return 0;
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
		if (IsServer(sptr))
			return 0; /* Servers must maintain state, not ask */

		for (h = Hooks[HOOKTYPE_VIEW_TOPIC_OUTSIDE_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(sptr,chptr);
			if (i != HOOK_CONTINUE)
				break;
		}

		/* If you're not a member, and you can't view outside channel, deny */
		if ((!ismember && i == HOOK_DENY) ||
		    (is_banned(sptr,chptr,BANCHK_JOIN,NULL,NULL) &&
		     !ValidatePermissionsForPath("channel:see:topic",sptr,NULL,chptr,NULL)))
		{
			sendnumeric(sptr, ERR_NOTONCHANNEL, name);
			return 0;
		}

		if (!chptr->topic)
			sendnumeric(sptr, RPL_NOTOPIC, chptr->chname);
		else
		{
			sendnumeric(sptr, RPL_TOPIC,
			    chptr->chname, chptr->topic);
			sendnumeric(sptr, RPL_TOPICWHOTIME, chptr->chname,
			    chptr->topic_nick, chptr->topic_time);
		}
		return 0;
	}

	if (ttime && topic && (IsServer(sptr) || IsULine(sptr)))
	{
		if (!chptr->topic_time || ttime > chptr->topic_time || IsULine(sptr))
		/* The IsUline is to allow services to use an old TS. Apparently
		 * some services do this in their topic enforcement -- codemastr 
		 */
		{
			/* Set the topic */
			safestrldup(chptr->topic, topic, iConf.topic_length+1);
			safestrldup(chptr->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);
			chptr->topic_time = ttime;

			RunHook4(HOOKTYPE_TOPIC, cptr, sptr, chptr, topic);
			new_message(sptr, recv_mtags, &mtags);
			sendto_server(cptr, PROTO_SID, 0, mtags, ":%s TOPIC %s %s %lu :%s",
			    ID(sptr), chptr->chname, chptr->topic_nick,
			    chptr->topic_time, chptr->topic);
			sendto_server(cptr, 0, PROTO_SID, mtags, ":%s TOPIC %s %s %lu :%s",
			    sptr->name, chptr->chname, chptr->topic_nick,
			    chptr->topic_time, chptr->topic);
			sendto_channel(chptr, sptr, NULL, 0, 0, SEND_LOCAL, mtags,
				       ":%s TOPIC %s :%s",
				       sptr->name, chptr->chname, chptr->topic);
			free_message_tags(mtags);
		}
		return 0;
	}

	/* Topic change. Either locally (check permissions!) or remote, check permissions: */
	if (IsPerson(sptr))
	{
		char *newtopic = NULL;

		/* +t and not +hoaq ? */
		if ((chptr->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(sptr, chptr) && !IsULine(sptr) && !IsServer(sptr))
		{
			if (MyClient(sptr) && !ValidatePermissionsForPath("channel:override:topic", sptr, NULL, chptr, NULL))
			{
				sendnumeric(sptr, ERR_CHANOPRIVSNEEDED, chptr->chname);
				return 0;
			}
			topicoverride(sptr, chptr, topic);
		}

		/* -t and banned? */
		newtopic = topic;
		if (!(chptr->mode.mode & MODE_TOPICLIMIT) &&
		    !is_skochanop(sptr, chptr) && is_banned(sptr, chptr, BANCHK_MSG, &newtopic, &errmsg))
		{
			char buf[512];

			if (MyClient(sptr) && !ValidatePermissionsForPath("channel:override:topic", sptr, NULL, chptr, NULL))
			{
				ircsnprintf(buf, sizeof(buf), "You cannot change the topic on %s while being banned", chptr->chname);
				sendnumeric(sptr, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return -1;
			}
			topicoverride(sptr, chptr, topic);
		}
		if (MyClient(sptr) && newtopic)
			topic = newtopic; /* process is_banned() changes of topic (eg: text replacement), but only for local clients */

		/* -t, +m, and not +vhoaq */
		if (((flags&CHFL_OVERLAP) == 0) && (chptr->mode.mode & MODE_MODERATED))
		{
			char buf[512];

			if (MyClient(sptr) && ValidatePermissionsForPath("channel:override:topic", sptr, NULL, chptr, NULL))
			{
				topicoverride(sptr, chptr, topic);
			} else {
				/* With +m and -t, only voice and higher may change the topic */
				ircsnprintf(buf, sizeof(buf), "Voice (+v) or higher is required in order to change the topic on %s (channel is +m)", chptr->chname);
				sendnumeric(sptr, ERR_CANNOTDOCOMMAND, "TOPIC",  buf);
				return -1;
			}
		}

		/* For local users, run spamfilters and hooks.. */
		if (MyClient(sptr))
		{
			Hook *tmphook;
			int n;

			if ((n = run_spamfilter(sptr, topic, SPAMF_TOPIC, chptr->chname, 0, NULL)) < 0)
				return n;

			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_TOPIC]; tmphook; tmphook = tmphook->next) {
				topic = (*(tmphook->func.pcharfunc))(sptr, chptr, topic);
				if (!topic)
					return 0;
			}
			RunHook4(HOOKTYPE_LOCAL_TOPIC, cptr, sptr, chptr, topic);
		}

		/* At this point 'tnick' is set to sptr->name.
		 * If set::topic-setter nick-user-host; is set
		 * then we update it here to nick!user@host.
		 */
		if (iConf.topic_setter == SETTER_NICK_USER_HOST)
			tnick = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));
	}

	/* Set the topic */
	safestrldup(chptr->topic, topic, iConf.topic_length+1);
	safestrldup(chptr->topic_nick, tnick, NICKLEN+USERLEN+HOSTLEN+5);

	RunHook4(HOOKTYPE_TOPIC, cptr, sptr, chptr, topic);
	if (ttime && IsServer(cptr))
		chptr->topic_time = ttime;
	else
		chptr->topic_time = TStime();

	new_message(sptr, recv_mtags, &mtags);
	sendto_server(cptr, 0, 0, mtags, ":%s TOPIC %s %s %lu :%s",
	    sptr->name, chptr->chname, chptr->topic_nick,
	    chptr->topic_time, chptr->topic);
	sendto_channel(chptr, sptr, NULL, 0, 0, SEND_LOCAL, mtags,
		       ":%s TOPIC %s :%s",
		       sptr->name, chptr->chname, chptr->topic);
	free_message_tags(mtags);

	return 0;
}
