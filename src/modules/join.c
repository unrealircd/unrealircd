/*
 *   IRC - Internet Relay Chat, src/modules/join.c
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

/* Forward declarations */
CMD_FUNC(cmd_join);
void _join_channel(Channel *channel, Client *client, MessageTag *mtags, const char *member_modes);
void _do_join(Client *client, int parc, const char *parv[]);
int _can_join(Client *client, Channel *channel, const char *key, char **errmsg);
void _send_join_to_local_users(Client *client, Channel *channel, MessageTag *mtags);
char *_get_chmodes_for_user(Client *client, const char *flags);
void send_cannot_join_error(Client *client, int numeric, char *fmtstr, char *channel_name);

/* Externs */
extern MODVAR int spamf_ugly_vchanoverride;
extern int find_invex(Channel *channel, Client *client);

/* Local vars */
static int bouncedtimes = 0;
long CAP_EXTENDED_JOIN = 0L;

/* Macros */
#define MAXBOUNCE   5 /** Most sensible */
#define MSG_JOIN 	"JOIN"	

ModuleHeader MOD_HEADER
  = {
	"join",
	"5.0",
	"command /join", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_JOIN_CHANNEL, _join_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_JOIN, _do_join);
	EfunctionAdd(modinfo->handle, EFUNC_CAN_JOIN, _can_join);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_JOIN_TO_LOCAL_USERS, _send_join_to_local_users);
	EfunctionAddPVoid(modinfo->handle, EFUNC_GET_CHMODES_FOR_USER, TO_PVOIDFUNC(_get_chmodes_for_user));

	return MOD_SUCCESS;
}

MOD_INIT()
{
	ClientCapabilityInfo c;
	memset(&c, 0, sizeof(c));
	c.name = "extended-join";
	ClientCapabilityAdd(modinfo->handle, &c, &CAP_EXTENDED_JOIN);

	CommandAdd(modinfo->handle, MSG_JOIN, cmd_join, MAXPARA, CMD_USER);
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

/* This function checks if a locally connected user may join the channel.
 * It also provides an number of hooks where modules can plug in to.
 * Note that the order of checking has been carefully thought of
 * (eg: bans at the end), so don't change it unless you have a good reason
 * to do so -- Syzop.
 */
int _can_join(Client *client, Channel *channel, const char *key, char **errmsg)
{
	Link *lp;
	Ban *banned;
	Hook *h;
	int i=0, j=0;

	for (h = Hooks[HOOKTYPE_CAN_JOIN]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client,channel,key, errmsg);
		if (i != 0)
			return i;
	}

	for (h = Hooks[HOOKTYPE_OPER_INVITE_BAN]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(client,channel);
		if (j != 0)
			break;
	}

	/* See if we can evade this ban */
	banned = is_banned(client, channel, BANCHK_JOIN, NULL, NULL);
	if (banned && j == HOOK_DENY)
	{
		*errmsg = STR_ERR_BANNEDFROMCHAN;
		return ERR_BANNEDFROMCHAN;
	}

	if (is_invited(client, channel))
		return 0; /* allowed to walk through all the other modes */

	if (banned)
	{
		*errmsg = STR_ERR_BANNEDFROMCHAN;
		return ERR_BANNEDFROMCHAN;
	}

#ifndef NO_OPEROVERRIDE
#ifdef OPEROVERRIDE_VERIFY
	if (ValidatePermissionsForPath("channel:override:privsecret",client,NULL,channel,NULL) && (channel->mode.mode & MODE_SECRET ||
	    channel->mode.mode & MODE_PRIVATE) && !is_autojoin_chan(channel->name))
	{
		*errmsg = STR_ERR_OPERSPVERIFY;
		return (ERR_OPERSPVERIFY);
	}
#endif
#endif

	return 0;
}

/*
** cmd_join
**	parv[1] = channel
**	parv[2] = channel password (key)
**
** Due to message tags, remote servers should only send 1 channel
** per JOIN. Or even better, use SJOIN instead.
** Otherwise we cannot use unique msgid's and such.
** UnrealIRCd 4 and probably UnrealIRCd 3.2.something already do
** this, so this comment is mostly for services coders, I guess.
*/
CMD_FUNC(cmd_join)
{
	int r;

	if (bouncedtimes)
	{
		unreal_log(ULOG_ERROR, "join", "BUG_JOIN_BOUNCEDTIMES", NULL,
		           "[BUG] join: bouncedtimes is not initialized to zero ($bounced_times)!! "
		           "Please report at https://bugs.unrealircd.org/",
		           log_data_integer("bounced_times", bouncedtimes));
	}

	bouncedtimes = 0;
	if (IsServer(client))
		return;
	do_join(client, parc, parv);
	bouncedtimes = 0;
}

/** Send JOIN message for 'client' to all users in 'channel'.
 * Taking into account that not everyone in channel should see the JOIN (mode +D)
 * and taking into account the different types of JOIN (due to CAP extended-join).
 */
void _send_join_to_local_users(Client *client, Channel *channel, MessageTag *mtags)
{
	int chanops_only = invisible_user_in_channel(client, channel);
	Member *lp;
	Client *acptr;
	char joinbuf[512];
	char exjoinbuf[512];

	ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN :%s",
		client->name, client->user->username, GetHost(client), channel->name);

	ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
		client->name, client->user->username, GetHost(client), channel->name,
		IsLoggedIn(client) ? client->user->account : "*",
		client->info);

	for (lp = channel->members; lp; lp = lp->next)
	{
		acptr = lp->client;

		if (!MyConnect(acptr))
			continue; /* only locally connected clients */

		if (chanops_only && !check_channel_access_member(lp, "hoaq") && (client != acptr))
			continue; /* skip non-ops if requested to (used for mode +D), but always send to 'client' */

		if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
			sendto_one(acptr, mtags, "%s", exjoinbuf);
		else
			sendto_one(acptr, mtags, "%s", joinbuf);
	}
}

/* Routine that actually makes a user join the channel
 * this does no actual checking (banned, etc.) it just adds the user.
 * Note: this is called for local JOIN and remote JOIN, but not for SJOIN.
 */
void _join_channel(Channel *channel, Client *client, MessageTag *recv_mtags, const char *member_modes)
{
	MessageTag *mtags = NULL; /** Message tags to send to local users (sender is :user) */
	MessageTag *mtags_sjoin = NULL; /* Message tags to send to remote servers for SJOIN (sender is :me.id) */
	const char *parv[3];

	/* Same way as in SJOIN */
	new_message_special(client, recv_mtags, &mtags, ":%s JOIN %s", client->name, channel->name);

	new_message(&me, recv_mtags, &mtags_sjoin);

	add_user_to_channel(channel, client, member_modes);

	send_join_to_local_users(client, channel, mtags);

	sendto_server(client, 0, 0, mtags_sjoin, ":%s SJOIN %lld %s :%s%s ",
		me.id, (long long)channel->creationtime,
		channel->name, modes_to_sjoin_prefix(member_modes), client->id);

	if (MyUser(client))
	{
		/*
		   ** Make a (temporal) creationtime, if someone joins
		   ** during a net.reconnect : between remote join and
		   ** the mode with TS. --Run
		 */
		if (channel->creationtime == 0)
		{
			channel->creationtime = TStime();
			sendto_server(client, 0, 0, NULL, ":%s MODE %s + %lld",
			    me.id, channel->name, (long long)channel->creationtime);
		}

		if (channel->topic)
		{
			sendnumeric(client, RPL_TOPIC, channel->name, channel->topic);
			sendnumeric(client, RPL_TOPICWHOTIME, channel->name, channel->topic_nick, (long long)channel->topic_time);
		}
		
		/* Set default channel modes (set::modes-on-join).
		 * Set only if it's the 1st user and only if no other modes have been set
		 * already (eg: +P, permanent).
		 */
		if ((channel->users == 1) && !channel->mode.mode && MODES_ON_JOIN)
		{
			MessageTag *mtags_mode = NULL;
			Cmode *cm;
			char modebuf[BUFSIZE], parabuf[BUFSIZE];
			int should_destroy = 0;

			channel->mode.mode = MODES_ON_JOIN;

			/* Param fun */
			for (cm=channelmodes; cm; cm = cm->next)
			{
				if (!cm->letter || !cm->paracount)
					continue;
				if (channel->mode.mode & cm->mode)
				        cm_putparameter(channel, cm->letter, iConf.modes_on_join.extparams[cm->letter]);
			}

			*modebuf = *parabuf = 0;
			channel_modes(client, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 0);
			/* This should probably be in the SJOIN stuff */
			new_message_special(&me, recv_mtags, &mtags_mode, ":%s MODE %s %s %s", me.name, channel->name, modebuf, parabuf);
			sendto_server(NULL, 0, 0, mtags_mode, ":%s MODE %s %s %s %lld",
			    me.id, channel->name, modebuf, parabuf, (long long)channel->creationtime);
			sendto_one(client, mtags_mode, ":%s MODE %s %s %s", me.name, channel->name, modebuf, parabuf);
			RunHook(HOOKTYPE_LOCAL_CHANMODE, &me, channel, mtags_mode, modebuf, parabuf, 0, 0, &should_destroy);
			free_message_tags(mtags_mode);
		}

		parv[0] = NULL;
		parv[1] = channel->name;
		parv[2] = NULL;
		do_cmd(client, NULL, "NAMES", 2, parv);

		unreal_log(ULOG_INFO, "join", "LOCAL_CLIENT_JOIN", client,
			   "User $client joined $channel",
			   log_data_channel("channel", channel),
			   log_data_string("modes", member_modes));

		RunHook(HOOKTYPE_LOCAL_JOIN, client, channel, mtags);
	} else {
		unreal_log(ULOG_INFO, "join", "REMOTE_CLIENT_JOIN", client,
			   "User $client joined $channel",
			   log_data_channel("channel", channel),
			   log_data_string("modes", member_modes));
		RunHook(HOOKTYPE_REMOTE_JOIN, client, channel, mtags);
	}

	free_message_tags(mtags);
	free_message_tags(mtags_sjoin);
}

/** User request to join a channel.
 * This routine is normally called from cmd_join but can also be called from
 * do_join->can_join->link module->do_join if the channel is 'linked' (chmode +L).
 * We therefore use a counter 'bouncedtimes' which is set to 0 in cmd_join,
 * increased every time we enter this loop and decreased anytime we leave the
 * loop. So be carefull not to use a simple 'return' after bouncedtimes++. -- Syzop
 */
void _do_join(Client *client, int parc, const char *parv[])
{
	char request[BUFSIZE];
	char request_key[BUFSIZE];
	char jbuf[BUFSIZE], jbuf2[BUFSIZE];
	const char *orig_parv1;
	Membership *lp;
	Channel *channel;
	char *name, *key = NULL;
	int i, ishold;
	char *p = NULL, *p2 = NULL;
	TKL *tklban;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("JOIN");
	const char *member_modes = "";

#define RET() do { bouncedtimes--; parv[1] = orig_parv1; return; } while(0)

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "JOIN");
		return;
	}

	/* For our tests we need super accurate time for JOINs or they mail fail. */
	gettimeofday(&timeofday_tv, NULL);
	timeofday = timeofday_tv.tv_sec;

	bouncedtimes++;
	orig_parv1 = parv[1];
	/* don't use 'return;' but 'RET();' from here ;p */

	if (bouncedtimes > MAXBOUNCE)
	{
		/* bounced too many times. yeah.. should be in the link module, I know.. then again, who cares.. */
		sendnotice(client, "*** Couldn't join %s ! - Link setting was too bouncy", parv[1]);
		RET();
	}

	*jbuf = '\0';
	/*
	   ** Rebuild list of channels joined to be the actual result of the
	   ** JOIN.  Note that "JOIN 0" is the destructive problem.
	 */
	strlcpy(request, parv[1], sizeof(request));
	for (i = 0, name = strtoken(&p, request, ","); name; i++, name = strtoken(&p, NULL, ","))
	{
		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "JOIN");
			break;
		}
		if (*name == '0' && !atoi(name))
		{
			/* UnrealIRCd 5+: we only support "JOIN 0",
			 * "JOIN 0,#somechan" etc... so only at the beginning.
			 * We do not support it half-way like "JOIN #a,0,#b"
			 * since that doesn't make sense, unless you are flooding...
			 * We still support it in remote joins for compatibility.
			 */
			if (MyUser(client) && (i != 0))
				continue;
			strlcpy(jbuf, "0", sizeof(jbuf));
			continue;
		} else
		if (MyConnect(client) && !valid_channelname(name))
		{
			send_invalid_channelname(client, name);
			if (IsOper(client) && find_channel(name))
			{
				/* Give IRCOps a bit more information */
				sendnotice(client, "Channel '%s' is unjoinable because it contains illegal characters. "
				                   "However, it does exist because another server in your "
				                   "network, which has a more loose restriction, created it. "
				                   "See https://www.unrealircd.org/docs/Set_block#set::allowed-channelchars",
				                   name);
			}
			continue;
		}
		else if (!IsChannelName(name))
		{
			if (MyUser(client))
				sendnumeric(client, ERR_NOSUCHCHANNEL, name);
			continue;
		}
		if (*jbuf)
			strlcat(jbuf, ",", sizeof jbuf);
		strlcat(jbuf, name, sizeof(jbuf));
	}

	/* We are going to overwrite 'jbuf' with the calls to strtoken()
	 * a few lines further down. Copy it to 'jbuf2' and make that
	 * the new parv[1].. or at least temporarily.
	 */
	strlcpy(jbuf2, jbuf, sizeof(jbuf2));
	parv[1] = jbuf2;

	p = NULL;
	if (parv[2])
	{
		strlcpy(request_key, parv[2], sizeof(request_key));
		key = strtoken(&p2, request_key, ",");
	}
	parv[2] = NULL;		/* for cmd_names call later, parv[parc] must == NULL */

	for (name = strtoken(&p, jbuf, ",");
	     name;
	     key = key ? strtoken(&p2, NULL, ",") : NULL, name = strtoken(&p, NULL, ","))
	{
		MessageTag *mtags = NULL;

		/*
		   ** JOIN 0 sends out a part for all channels a user
		   ** has joined.
		 */
		if (*name == '0' && !atoi(name))
		{
			/* Rewritten so to generate a PART for each channel to servers,
			 * so the same msgid is used for each part on all servers. -- Syzop
			 */
			while ((lp = client->user->channel))
			{
				MessageTag *mtags = NULL;
				channel = lp->channel;

				new_message(client, NULL, &mtags);

				sendto_channel(channel, client, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s :%s",
				               client->name, channel->name, "Left all channels");
				sendto_server(client, 0, 0, mtags, ":%s PART %s :Left all channels", client->name, channel->name);

				if (MyConnect(client))
					RunHook(HOOKTYPE_LOCAL_PART, client, channel, mtags, "Left all channels");

				remove_user_from_channel(client, channel, 0);
				free_message_tags(mtags);
			}
			continue;
		}

		if (MyConnect(client))
		{
			member_modes = (ChannelExists(name)) ? "" : LEVEL_ON_JOIN;

			if (!ValidatePermissionsForPath("immune:maxchannelsperuser",client,NULL,NULL,NULL))	/* opers can join unlimited chans */
				if (client->user->joined >= MAXCHANNELSPERUSER)
				{
					sendnumeric(client, ERR_TOOMANYCHANNELS, name);
					RET();
				}
/* RESTRICTCHAN */
			if (conf_deny_channel)
			{
				if (!ValidatePermissionsForPath("immune:server-ban:deny-channel",client,NULL,NULL,NULL))
				{
					ConfigItem_deny_channel *d;
					if ((d = find_channel_allowed(client, name)))
					{
						if (d->warn)
						{
							unreal_log(ULOG_INFO, "join", "JOIN_DENIED_FORBIDDEN_CHANNEL", client,
							           "Client $client.details tried to join forbidden channel $channel",
							           log_data_string("channel", name));
						}
						if (d->reason)
							sendnumeric(client, ERR_FORBIDDENCHANNEL, name, d->reason);
						if (d->redirect)
						{
							sendnotice(client, "*** Redirecting you to %s", d->redirect);
							parv[0] = NULL;
							parv[1] = d->redirect;
							do_join(client, 2, parv);
						}
						if (d->class)
							sendnotice(client, "*** Can not join %s: Your class is not allowed", name);
						continue;
					}
				}
			}
			if (ValidatePermissionsForPath("immune:server-ban:deny-channel",client,NULL,NULL,NULL) && (tklban = find_qline(client, name, &ishold)))
			{
				sendnumeric(client, ERR_FORBIDDENCHANNEL, name, tklban->ptr.nameban->reason);
				continue;
			}
			/* ugly set::spamfilter::virus-help-channel-deny hack.. */
			if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
			    !strcasecmp(name, SPAMFILTER_VIRUSCHAN) &&
			    !ValidatePermissionsForPath("immune:server-ban:viruschan",client,NULL,NULL,NULL) && !spamf_ugly_vchanoverride)
			{
				Channel *channel = find_channel(name);
				
				if (!channel || !is_invited(client, channel))
				{
					sendnotice(client, "*** Cannot join '%s' because it's the virus-help-channel "
					                   "which is reserved for infected users only", name);
					continue;
				}
			}
		}

		channel = make_channel(name);
		if (channel && (lp = find_membership_link(client->user->channel, channel)))
			continue;

		if (!channel)
			continue;

		i = HOOK_CONTINUE;
		if (!MyConnect(client))
			member_modes = "";
		else
		{
			Hook *h;
			char *errmsg = NULL;
			for (h = Hooks[HOOKTYPE_PRE_LOCAL_JOIN]; h; h = h->next) 
			{
				i = (*(h->func.intfunc))(client,channel,key);
				if (i == HOOK_DENY || i == HOOK_ALLOW)
					break;
			}
			/* Denied, get out now! */
			if (i == HOOK_DENY)
			{
				/* Rejected... if we just created a new chan we should destroy it too. -- Syzop */
				if (!channel->users)
					sub1_from_channel(channel);
				continue;
			}
			/* If they are allowed, don't check can_join */
			if (i != HOOK_ALLOW && 
			   (i = can_join(client, channel, key, &errmsg)))
			{
				if (i != -1)
					send_cannot_join_error(client, i, errmsg, name);
				continue;
			}
		}

		/* Generate a new message without inheritance.
		 * We can do this because remote joins don't follow this code path,
		 * or are highly discouraged to anyway.
		 * Remote servers use SJOIN and never reach this function.
		 * Locally we do follow this code path with JOIN and then generating
		 * a new_message() here is exactly what we want:
		 * Each "JOIN #a,#b,#c" gets processed individually in this loop
		 * and is sent by join_channel() as a SJOIN for #a, then SJOIN for #b,
		 * and so on, each with their own unique msgid and such.
		 */
		new_message(client, NULL, &mtags);
		join_channel(channel, client, mtags, member_modes);
		free_message_tags(mtags);
	}
	RET();
#undef RET
}

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
void send_cannot_join_error(Client *client, int numeric, char *fmtstr, char *channel_name)
{
	// TODO: add single %s validation !
	sendnumericfmt(client, numeric, fmtstr, channel_name);
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* Additional channel-related functions. I've put it here instead
 * of the core so it could be upgraded on the fly should it be necessary.
 */

char *_get_chmodes_for_user(Client *client, const char *member_flags)
{
	static char modebuf[512]; /* returned */
	char flagbuf[8]; /* For holding "vhoaq" */
	char parabuf[512];
	int n, i;

	if (BadPtr(member_flags))
		return "";

	parabuf[0] = '\0';
	n = strlen(member_flags);
	if (n)
	{
		for (i=0; i < n; i++)
		{
			strlcat(parabuf, client->name, sizeof(parabuf));
			if (i < n - 1)
				strlcat(parabuf, " ", sizeof(parabuf));
		}
		/* And we have our mode line! */
		snprintf(modebuf, sizeof(modebuf), "+%s %s", member_flags, parabuf);
		return modebuf;
	}

	return "";
}
