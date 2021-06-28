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
void _join_channel(Channel *channel, Client *client, MessageTag *mtags, int flags);
void _do_join(Client *client, int parc, char *parv[]);
int _can_join(Client *client, Channel *channel, char *key, char *parv[]);
void _userhost_save_current(Client *client);
void _userhost_changed(Client *client);
void _send_join_to_local_users(Client *client, Channel *channel, MessageTag *mtags);

/* Externs */
extern MODVAR int spamf_ugly_vchanoverride;
extern int find_invex(Channel *channel, Client *client);

/* Local vars */
static int bouncedtimes = 0;

/* Macros */
#define MAXBOUNCE   5 /** Most sensible */
#define MSG_JOIN 	"JOIN"	

ModuleHeader MOD_HEADER
  = {
	"join",
	"5.0",
	"command /join", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_JOIN_CHANNEL, _join_channel);
	EfunctionAddVoid(modinfo->handle, EFUNC_DO_JOIN, _do_join);
	EfunctionAdd(modinfo->handle, EFUNC_CAN_JOIN, _can_join);
	EfunctionAddVoid(modinfo->handle, EFUNC_USERHOST_SAVE_CURRENT, _userhost_save_current);
	EfunctionAddVoid(modinfo->handle, EFUNC_USERHOST_CHANGED, _userhost_changed);
	EfunctionAddVoid(modinfo->handle, EFUNC_SEND_JOIN_TO_LOCAL_USERS, _send_join_to_local_users);

	return MOD_SUCCESS;
}

MOD_INIT()
{
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
int _can_join(Client *client, Channel *channel, char *key, char *parv[])
{
	Link *lp;
	Ban *banned;
	Hook *h;
	int i=0, j=0;

	for (h = Hooks[HOOKTYPE_CAN_JOIN]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(client,channel,key,parv);
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
		return (ERR_BANNEDFROMCHAN);

	if (is_invited(client, channel))
		return 0; /* allowed */

        if (channel->users >= channel->mode.limit)
        {
                /* Hmmm.. don't really like this.. and not at this place */
                
                for (h = Hooks[HOOKTYPE_CAN_JOIN_LIMITEXCEEDED]; h; h = h->next) 
                {
                        i = (*(h->func.intfunc))(client,channel,key,parv);
                        if (i != 0)
                                return i;
                }

                /* We later check again for this limit (in case +L was not set) */
        }


        if (*channel->mode.key && (BadPtr(key) || strcmp(channel->mode.key, key)))
                return (ERR_BADCHANNELKEY);

        if ((channel->mode.mode & MODE_INVITEONLY) && !find_invex(channel, client))
                return (ERR_INVITEONLYCHAN);

        if ((channel->mode.limit && channel->users >= channel->mode.limit))
                return (ERR_CHANNELISFULL);

        if (banned)
                return (ERR_BANNEDFROMCHAN);

#ifndef NO_OPEROVERRIDE
#ifdef OPEROVERRIDE_VERIFY
        if (ValidatePermissionsForPath("channel:override:privsecret",client,NULL,channel,NULL) && (channel->mode.mode & MODE_SECRET ||
            channel->mode.mode & MODE_PRIVATE) && !is_autojoin_chan(channel->chname))
                return (ERR_OPERSPVERIFY);
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
		sendto_realops("join: bouncedtimes=%d??? [please report at https://bugs.unrealircd.org/]", bouncedtimes);

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
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");
	long CAP_AWAY_NOTIFY = ClientCapabilityBit("away-notify");

	ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN :%s",
		client->name, client->user->username, GetHost(client), channel->chname);

	ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
		client->name, client->user->username, GetHost(client), channel->chname,
		IsLoggedIn(client) ? client->user->svid : "*",
		client->info);

	for (lp = channel->members; lp; lp = lp->next)
	{
		acptr = lp->client;

		if (!MyConnect(acptr))
			continue; /* only locally connected clients */

		if (chanops_only && !(lp->flags & (CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANADMIN)) && (client != acptr))
			continue; /* skip non-ops if requested to (used for mode +D), but always send to 'client' */

		if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
			sendto_one(acptr, mtags, "%s", exjoinbuf);
		else
			sendto_one(acptr, mtags, "%s", joinbuf);

		if (client->user->away && HasCapabilityFast(acptr, CAP_AWAY_NOTIFY))
		{
			MessageTag *mtags_away = NULL;
			new_message(client, NULL, &mtags_away);
			sendto_one(acptr, mtags_away, ":%s!%s@%s AWAY :%s",
			           client->name, client->user->username, GetHost(client), client->user->away);
			free_message_tags(mtags_away);
		}
	}
}

/* Routine that actually makes a user join the channel
 * this does no actual checking (banned, etc.) it just adds the user
 */
void _join_channel(Channel *channel, Client *client, MessageTag *recv_mtags, int flags)
{
	MessageTag *mtags = NULL; /** Message tags to send to local users (sender is :user) */
	MessageTag *mtags_sjoin = NULL; /* Message tags to send to remote servers for SJOIN (sender is :me.id) */
	char *parv[] = { 0, 0 };

	/* Same way as in SJOIN */
	new_message_special(client, recv_mtags, &mtags, ":%s JOIN %s", client->name, channel->chname);

	new_message(&me, recv_mtags, &mtags_sjoin);

	add_user_to_channel(channel, client, flags);

	send_join_to_local_users(client, channel, mtags);

	sendto_server(client, 0, 0, mtags_sjoin, ":%s SJOIN %lld %s :%s%s ",
		me.id, (long long)channel->creationtime,
		channel->chname, chfl_to_sjoin_symbol(flags), client->id);

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
			    me.id, channel->chname, (long long)channel->creationtime);
		}
		del_invite(client, channel);

		if (channel->topic)
		{
			sendnumeric(client, RPL_TOPIC, channel->chname, channel->topic);
			sendnumeric(client, RPL_TOPICWHOTIME, channel->chname, channel->topic_nick,
			    channel->topic_time);
		}
		
		/* Set default channel modes (set::modes-on-join).
		 * Set only if it's the 1st user and only if no other modes have been set
		 * already (eg: +P, permanent).
		 */
		if ((channel->users == 1) && !channel->mode.mode && !channel->mode.extmode &&
		    (MODES_ON_JOIN || iConf.modes_on_join.extmodes))
		{
			int i;
			MessageTag *mtags_mode = NULL;

			channel->mode.extmode =  iConf.modes_on_join.extmodes;
			/* Param fun */
			for (i = 0; i <= Channelmode_highest; i++)
			{
				if (!Channelmode_Table[i].flag || !Channelmode_Table[i].paracount)
					continue;
				if (channel->mode.extmode & Channelmode_Table[i].mode)
				        cm_putparameter(channel, Channelmode_Table[i].flag, iConf.modes_on_join.extparams[i]);
			}

			channel->mode.mode = MODES_ON_JOIN;

			*modebuf = *parabuf = 0;
			channel_modes(client, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), channel, 0);
			/* This should probably be in the SJOIN stuff */
			new_message_special(&me, recv_mtags, &mtags_mode, ":%s MODE %s %s %s", me.name, channel->chname, modebuf, parabuf);
			sendto_server(NULL, 0, 0, mtags_mode, ":%s MODE %s %s %s %lld",
			    me.id, channel->chname, modebuf, parabuf, (long long)channel->creationtime);
			sendto_one(client, mtags_mode, ":%s MODE %s %s %s", me.name, channel->chname, modebuf, parabuf);
			free_message_tags(mtags_mode);
		}

		parv[0] = client->name;
		parv[1] = channel->chname;
		do_cmd(client, NULL, "NAMES", 2, parv);

		RunHook4(HOOKTYPE_LOCAL_JOIN, client, channel, mtags, parv);
	} else {
		RunHook4(HOOKTYPE_REMOTE_JOIN, client, channel, mtags, parv);
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
void _do_join(Client *client, int parc, char *parv[])
{
	char jbuf[BUFSIZE], jbuf2[BUFSIZE];
	char *orig_parv1;
	Membership *lp;
	Channel *channel;
	char *name, *key = NULL;
	int i, flags = 0, ishold;
	char *p = NULL, *p2 = NULL;
	TKL *tklban;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("JOIN");

#define RET() do { bouncedtimes--; parv[1] = orig_parv1; return; } while(0)

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "JOIN");
		return;
	}
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
	for (i = 0, name = strtoken(&p, parv[1], ",");
	     name;
	     i++, name = strtoken(&p, NULL, ","))
	{
		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, name, maxtargets, "JOIN");
			break;
		}
		if (*name == '0' && !atoi(name))
		{
			/* UnrealIRCd 5: we only support "JOIN 0",
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
			if (IsOper(client) && find_channel(name, NULL))
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
		key = strtoken(&p2, parv[2], ",");
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
				               client->name, channel->chname, "Left all channels");
				sendto_server(client, 0, 0, mtags, ":%s PART %s :Left all channels", client->name, channel->chname);

				if (MyConnect(client))
					RunHook4(HOOKTYPE_LOCAL_PART, client, channel, mtags, "Left all channels");

				remove_user_from_channel(client, channel);
				free_message_tags(mtags);
			}
			continue;
		}

		if (MyConnect(client))
		{
			/*
			   ** local client is first to enter previously nonexistant
			   ** channel so make them (rightfully) the Channel
			   ** Operator.
			 */
			/* Where did this come from? Potvin ? --Stskeeps
			   flags = (ChannelExists(name)) ? CHFL_DEOPPED :
			   CHFL_CHANOWNER;

			 */

			flags =
			    (ChannelExists(name)) ? CHFL_DEOPPED : LEVEL_ON_JOIN;

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
							sendto_snomask(SNO_EYES, "*** %s tried to join forbidden channel %s",
								get_client_name(client, 1), name);
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
				Channel *channel = find_channel(name, NULL);
				
				if (!channel || !is_invited(client, channel))
				{
					sendnotice(client, "*** Cannot join '%s' because it's the virus-help-channel "
					                   "which is reserved for infected users only", name);
					continue;
				}
			}
		}

		channel = get_channel(client, name, CREATE);
		if (channel && (lp = find_membership_link(client->user->channel, channel)))
			continue;

		if (!channel)
			continue;

		i = HOOK_CONTINUE;
		if (!MyConnect(client))
			flags = CHFL_DEOPPED;
		else
		{
			Hook *h;
			for (h = Hooks[HOOKTYPE_PRE_LOCAL_JOIN]; h; h = h->next) 
			{
				/* Note: this is just a hack not to break the ABI but still be
				 * able to fix https://bugs.unrealircd.org/view.php?id=5644
				 * In the future we should just drop the parv/parx argument
				 * and use key as an argument instead.
				 */
				char *parx[4];
				parx[0] = NULL;
				parx[1] = name;
				parx[2] = key;
				parx[3] = NULL;
				i = (*(h->func.intfunc))(client,channel,parx);
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
			   (i = can_join(client, channel, key, parv)))
			{
				if (i != -1)
				{
					sendnumeric(client, i, name);
				}
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
		join_channel(channel, client, mtags, flags);
		free_message_tags(mtags);
	}
	RET();
#undef RET
}

/* Additional channel-related functions. I've put it here instead
 * of the core so it could be upgraded on the fly should it be necessary.
 */

char *get_chmodes_for_user(Client *client, int flags)
{
	static char modebuf[512]; /* returned */
	char flagbuf[8]; /* For holding "vhoaq" */
	char *p = flagbuf;
	char parabuf[512];
	int n, i;

	if (!flags)
		return "";

	if (flags & MODE_CHANOWNER)
		*p++ = 'q';
	if (flags & MODE_CHANADMIN)
		*p++ = 'a';
	if (flags & MODE_CHANOP)
		*p++ = 'o';
	if (flags & MODE_VOICE)
		*p++ = 'v';
	if (flags & MODE_HALFOP)
		*p++ = 'h';
	*p = '\0';

	parabuf[0] = '\0';

	n = strlen(flagbuf);
	if (n)
	{
		for (i=0; i < n; i++)
		{
			strlcat(parabuf, client->name, sizeof(parabuf));
			if (i < n - 1)
				strlcat(parabuf, " ", sizeof(parabuf));
		}
		/* And we have our mode line! */
		snprintf(modebuf, sizeof(modebuf), "+%s %s", flagbuf, parabuf);
		return modebuf;
	}

	return "";
}

static char remember_nick[NICKLEN+1];
static char remember_user[USERLEN+1];
static char remember_host[HOSTLEN+1];

/** Save current nick/user/host. Used later by userhost_changed(). */
void _userhost_save_current(Client *client)
{
	strlcpy(remember_nick, client->name, sizeof(remember_nick));
	strlcpy(remember_user, client->user->username, sizeof(remember_user));
	strlcpy(remember_host, GetHost(client), sizeof(remember_host));
}

/** User/Host changed for user.
 * Note that userhost_save_current() needs to be called before this
 * to save the old username/hostname.
 * This userhost_changed() function deals with notifying local clients
 * about the user/host change by sending PART+JOIN+MODE if
 * set::allow-userhost-change force-rejoin is in use,
 * and it wills end "CAP chghost" to such capable clients.
 * It will also deal with bumping fakelag for the user since a user/host
 * change is costly, doesn't matter if it was self-induced or not.
 *
 * Please call this function for any user/host change by doing:
 * userhost_save_current(client);
 * << change username or hostname here >>
 * userhost_changed(client);
 */
void _userhost_changed(Client *client)
{
	Membership *channels;
	Member *lp;
	Client *acptr;
	int impact = 0;
	char buf[512];
	long CAP_EXTENDED_JOIN = ClientCapabilityBit("extended-join");
	long CAP_CHGHOST = ClientCapabilityBit("chghost");

	if (strcmp(remember_nick, client->name))
	{
		ircd_log(LOG_ERROR, "[BUG] userhost_changed() was called but without calling userhost_save_current() first! Affected user: %s",
			client->name);
		ircd_log(LOG_ERROR, "Please report above bug on https://bugs.unrealircd.org/");
		sendto_realops("[BUG] userhost_changed() was called but without calling userhost_save_current() first! Affected user: %s",
			client->name);
		sendto_realops("Please report above bug on https://bugs.unrealircd.org/");
		return; /* We cannot safely process this request anymore */
	}

	/* It's perfectly acceptable to call us even if the userhost didn't change. */
	if (!strcmp(remember_user, client->user->username) && !strcmp(remember_host, GetHost(client)))
		return; /* Nothing to do */

	/* Most of the work is only necessary for set::allow-userhost-change force-rejoin */
	if (UHOST_ALLOWED == UHALLOW_REJOIN)
	{
		/* Walk through all channels of this user.. */
		for (channels = client->user->channel; channels; channels = channels->next)
		{
			Channel *channel = channels->channel;
			int flags = channels->flags;
			char *modes;
			char partbuf[512]; /* PART */
			char joinbuf[512]; /* JOIN */
			char exjoinbuf[512]; /* JOIN (for CAP extended-join) */
			char modebuf[512]; /* MODE (if any) */
			int chanops_only = invisible_user_in_channel(client, channel);

			modebuf[0] = '\0';

			/* If the user is banned, don't send any rejoins, it would only be annoying */
			if (is_banned(client, channel, BANCHK_JOIN, NULL, NULL))
				continue;

			/* Prepare buffers for PART, JOIN, MODE */
			ircsnprintf(partbuf, sizeof(partbuf), ":%s!%s@%s PART %s :%s",
						remember_nick, remember_user, remember_host,
						channel->chname,
						"Changing host");

			ircsnprintf(joinbuf, sizeof(joinbuf), ":%s!%s@%s JOIN %s",
						client->name, client->user->username, GetHost(client), channel->chname);

			ircsnprintf(exjoinbuf, sizeof(exjoinbuf), ":%s!%s@%s JOIN %s %s :%s",
				client->name, client->user->username, GetHost(client), channel->chname,
				IsLoggedIn(client) ? client->user->svid : "*",
				client->info);

			modes = get_chmodes_for_user(client, flags);
			if (!BadPtr(modes))
				ircsnprintf(modebuf, sizeof(modebuf), ":%s MODE %s %s", me.name, channel->chname, modes);

			for (lp = channel->members; lp; lp = lp->next)
			{
				acptr = lp->client;

				if (acptr == client)
					continue; /* skip self */

				if (!MyConnect(acptr))
					continue; /* only locally connected clients */

				if (chanops_only && !(lp->flags & (CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANADMIN)))
					continue; /* skip non-ops if requested to (used for mode +D) */

				if (HasCapabilityFast(acptr, CAP_CHGHOST))
					continue; /* we notify 'CAP chghost' users in a different way, so don't send it here. */

				impact++;

				/* FIXME: if a client does not have the "chghost" cap then
				 * here we will not generate a proper new message, probably
				 * needs to be fixed... I skipped doing it for now.
				 */
				sendto_one(acptr, NULL, "%s", partbuf);

				if (HasCapabilityFast(acptr, CAP_EXTENDED_JOIN))
					sendto_one(acptr, NULL, "%s", exjoinbuf);
				else
					sendto_one(acptr, NULL, "%s", joinbuf);

				if (*modebuf)
					sendto_one(acptr, NULL, "%s", modebuf);
			}
		}
	}

	/* Now deal with "CAP chghost" clients.
	 * This only needs to be sent one per "common channel".
	 * This would normally call sendto_common_channels_local_butone() but the user already
	 * has the new user/host.. so we do it here..
	 */
	ircsnprintf(buf, sizeof(buf), ":%s!%s@%s CHGHOST %s %s",
	            remember_nick, remember_user, remember_host,
	            client->user->username,
	            GetHost(client));
	current_serial++;
	for (channels = client->user->channel; channels; channels = channels->next)
	{
		for (lp = channels->channel->members; lp; lp = lp->next)
		{
			acptr = lp->client;
			if (MyUser(acptr) && HasCapabilityFast(acptr, CAP_CHGHOST) &&
			    (acptr->local->serial != current_serial) && (client != acptr))
			{
				/* FIXME: send mtag */
				sendto_one(acptr, NULL, "%s", buf);
				acptr->local->serial = current_serial;
			}
		}
	}

	if (MyUser(client))
	{
		/* We take the liberty of sending the CHGHOST to the impacted user as
		 * well. This makes things easy for client coders.
		 * (Note that this cannot be merged with the for loop from 15 lines up
		 *  since the user may not be in any channels)
		 */
		if (HasCapabilityFast(client, CAP_CHGHOST))
			sendto_one(client, NULL, "%s", buf);

		/* A userhost change always generates the following network traffic:
		 * server to server traffic, CAP "chghost" notifications, and
		 * possibly PART+JOIN+MODE if force-rejoin had work to do.
		 * We give the user a penalty so they don't flood...
		 */
		if (impact)
			client->local->since += 7; /* Resulted in rejoins and such. */
		else
			client->local->since += 4; /* No rejoins */
	}
}
