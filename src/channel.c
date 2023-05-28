/* Unreal Internet Relay Chat Daemon, src/channel.c
 * (C) Copyright 1990 Jarkko Oikarinen and
 *                    University of Oulu, Co Center
 * (C) Copyright 1999-present The UnrealIRCd team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/** @file
 * @brief Various important (common) channel functions.
 */

#include "unrealircd.h"

/** Lazy way to signal an OperOverride MODE */
long opermode = 0;
/** Lazy way to signal an SAJOIN MODE */
long sajoinmode = 0;
/** List of all channels on the server.
 * @ingroup ListFunctions
 * @section channels_example Example
 * This code will list all channels on the network.
 * @code
 * sendnotice(client, "List of all channels:");
 * for (channel = channels; channel; channel=channel->nextch)
 *     sendnotice(client, "Channel %s", channel->name);
 * @endcode
 */
Channel *channels = NULL;

static mp_pool_t *channel_pool = NULL;

/** This describes the letters, modes and options for core channel modes.
 * These are +ntmispklr and also the list modes +vhoaq and +beI.
 */
CoreChannelModeTable corechannelmodetable[] = {
	{MODE_BAN, 'b', 1, 1},
	{MODE_EXCEPT, 'e', 1, 1},	/* exception ban */
	{MODE_INVEX, 'I', 1, 1},	/* invite-only exception */
	{0x0, 0x0, 0x0, 0x0}
};

/** The advertised supported channel modes in the 004 numeric */
char cmodestring[512];

/** Returns 1 if the IRCOp can override or is a remote connection */
inline int op_can_override(const char *acl, Client *client, Channel *channel, void* extra)
{
#ifndef NO_OPEROVERRIDE
	if (MyUser(client) && !(ValidatePermissionsForPath(acl,client,NULL,channel,extra)))
		return 0;
	return 1;
#else
	return 0;
#endif
}

/** Returns 1 if a half-op can set this channel mode */
int Halfop_mode(long mode)
{
	CoreChannelModeTable *tab = &corechannelmodetable[0];

	while (tab->mode != 0x0)
	{
		if (tab->mode == mode)
			return (tab->halfop == 1 ? TRUE : FALSE);
		tab++;
	}
	return TRUE;
}

/** Find client in a Member linked list (eg: channel->members) */
Member *find_member_link(Member *lp, Client *ptr)
{
	if (ptr)
	{
		while (lp)
		{
			if (lp->client == ptr)
				return (lp);
			lp = lp->next;
		}
	}
	return NULL;
}

/** Find channel in a Membership linked list (eg: client->user->channel) */
Membership *find_membership_link(Membership *lp, Channel *ptr)
{
	if (ptr)
		while (lp)
		{
			if (lp->channel == ptr)
				return (lp);
			lp = lp->next;
		}
	return NULL;
}

/** Allocate and return an empty Member struct */
static Member *make_member(void)
{
	Member *lp;
	unsigned int	i;

	if (freemember == NULL)
	{
		for (i = 1; i <= (4072/sizeof(Member)); ++i)
		{
			lp = safe_alloc(sizeof(Member));
			lp->next = freemember;
			freemember = lp;
		}
	}
	lp = freemember;
	freemember = freemember->next;
	lp->next = NULL;
	return lp;
}

/** Free a Member struct */
static void free_member(Member *lp)
{
	if (!lp)
		return;
	moddata_free_member(lp);
	memset(lp, 0, sizeof(Member));
	lp->next = freemember;
	freemember = lp;
}

/** Allocate and return an empty Membership struct */
static Membership *make_membership(void)
{
	Membership *m = NULL;
	unsigned int	i;

	if (freemembership == NULL)
	{
		for (i = 1; i <= (4072/sizeof(Membership)); i++)
		{
			m = safe_alloc(sizeof(Membership));
			m->next = freemembership;
			freemembership = m;
		}
		m = freemembership;
		freemembership = m->next;
	}
	else
	{
		m = freemembership;
		freemembership = freemembership->next;
	}
	memset(m, 0, sizeof(Membership));
	return m;
}

/** Free a Membership struct */
static void free_membership(Membership *m)
{
	if (m)
	{
		moddata_free_membership(m);
		memset(m, 0, sizeof(Membership));
		m->next = freemembership;
		freemembership = m;
	}
}

/** Find a client by nickname, hunt for older nick names if not found.
 * This can be handy, for example for /KILL nick, if 'nick' keeps
 * nick-changing and you are slow with typing.
 * @param client	The requestor
 * @param user		The nick name (or server name)
 * @param chasing	This will be set to 1 if the client was found
 *			only after searching through the nick history.
 * @returns The client (if found) or NULL (if not found).
 */
Client *find_chasing(Client *client, const char *user, int *chasing)
{
	Client *who = find_client(user, NULL);

	if (chasing)
		*chasing = 0;
	if (who)
	{
		if (!IsServer(who))
			return who;
		else
			return NULL;
	}
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, user);
		return NULL;
	}
	if (chasing)
		*chasing = 1;
	if (!IsServer(who))
		return who;
	else return NULL;
}

/** Return 1 if the bans are identical, taking into account special handling for extbans */
int identical_ban(const char *one, const char *two)
{
#if 0
	if (is_extended_ban(one) && is_extended_ban(two))
	{
		/* compare the first 3 characters case-sensitive and if identical then compare
		 * the remainder of the string case-insensitive.
		 */
		if (!strncmp(one, two, 3) && !strcasecmp(one+3, two+3))
			return 1;
	} else {
		if (!mycmp(one, two))
			return 1;
	}
#else
	/* Actually I think we can live with this nowadays.
	 * We are pushing towards named extbans, and all the
	 * letter extbans that could clash no longer exist.
	 */
	if (!mycmp(one, two))
		return 1;
#endif
	return 0;
}

/** Add a listmode (+beI) with the specified banid to
 *  the specified channel. (Extended version with
 *  set by nick and set on timestamp)
 * @retval	1	Added
 * @retval	0	Updated
 * @retval	-1	Ban list full
 */
int add_listmode_ex(Ban **list, Client *client, Channel *channel, const char *banid, const char *setby, time_t seton)
{
	Ban *ban;
	int cnt = 0;
	int do_not_add = 0;
	char isnew = 0;

	//if (MyUser(client))
	//	collapse(banid);

	if (!*list && (MAXBANS < 1))
	{
		if (MyUser(client))
		{
			/* Only send the error to local clients */
			sendnumeric(client, ERR_BANLISTFULL, channel->name, banid);
		}
		do_not_add = 1;
	}
	for (ban = *list; ban; ban = ban->next)
	{
		/* Check MAXBANS only for local clients and 'me' (for +b's set during +f).
		 */
		if ((MyUser(client) || IsMe(client)) && (++cnt >= MAXBANS))
			do_not_add = 1;
		if (identical_ban(ban->banstr, banid))
			break; /* update existing ban (potentially) */
	}

	/* Create a new ban if needed */
	if (!ban)
	{
		if (do_not_add)
		{
			/* The banlist is full and trying to add a new ban.
			 * This is not permitted.
			 */
			if (MyUser(client))
			{
				/* Only send the error to local clients */
				sendnumeric(client, ERR_BANLISTFULL, channel->name, banid);
			}
			return -1;
		}
		ban = make_ban();
		ban->next = *list;
		*list = ban;
		isnew = 1;
	}

	if ((ban->when > 0) && (seton >= ban->when))
	{
		/* Trying to add the same ban while an older version
		 * or identical version of the ban already exists.
		 */
		return -1;
	}

	/* Update/set if this ban is new or older than existing one */
	safe_strdup(ban->banstr, banid); /* cAsE may differ, use oldest version of it */
	safe_strdup(ban->who, setby);
	ban->when = seton;
	return isnew ? 1 : 0;
}

/** Add a listmode (+beI) with the specified banid to
 *  the specified channel. (Simplified version)
 */
int add_listmode(Ban **list, Client *client, Channel *channel, const char *banid)
{
	char *setby = client->name;
	char nuhbuf[NICKLEN+USERLEN+HOSTLEN+4];

	if (IsUser(client) && (iConf.ban_setter == SETTER_NICK_USER_HOST))
		setby = make_nick_user_host_r(nuhbuf, sizeof(nuhbuf), client->name, client->user->username, GetHost(client));

	return add_listmode_ex(list, client, channel, banid, setby, TStime());
}

/** Delete a listmode (+beI) from a channel that matches the specified banid.
 */
int del_listmode(Ban **list, Channel *channel, const char *banid)
{
	Ban **ban;
	Ban *tmp;

	if (!banid)
		return -1;
	for (ban = list; *ban; ban = &((*ban)->next))
	{
		if (identical_ban(banid, (*ban)->banstr))
		{
			tmp = *ban;
			*ban = tmp->next;
			safe_free(tmp->banstr);
			safe_free(tmp->who);
			free_ban(tmp);
			return 0;
		}
	}
	return -1;
}

/** is_banned - Check if a user is banned on a channel.
 * @param client   Client to check (can be remote client)
 * @param channel  Channel to check
 * @param type   Type of ban to check for (BANCHK_*)
 * @param msg    Message, only for some BANCHK_* types, otherwise NULL
 * @param errmsg Error message returned, could be NULL (which does not
 *               indicate absence of an error).
 * @returns      A pointer to the ban struct if banned, otherwise NULL.
 * @comments     Simple wrapper for is_banned_with_nick()
 */
inline Ban *is_banned(Client *client, Channel *channel, int type, const char **msg, const char **errmsg)
{
	return is_banned_with_nick(client, channel, type, NULL, msg, errmsg);
}

/** ban_check_mask - Checks if the user matches the specified n!u@h mask -or- run an extended ban.
 * This is basically extracting the mask and extban check from is_banned_with_nick,
 * but with being a bit more strict in what an extban is.
 * Strange things could happen if this is called outside standard ban checking.
 * @param b	Ban context, see BanContext
 * @returns	Nonzero if the mask/extban succeeds. Zero if it doesn't.
 */
inline int ban_check_mask(BanContext *b)
{
	if (!b->no_extbans && is_extended_ban(b->banstr))
	{
		/* Is an extended ban. */
		const char *nextbanstr;
		Extban *extban = findmod_by_bantype(b->banstr, &nextbanstr);
		if (!extban || !(extban->is_banned_events & b->ban_check_types))
		{
			return 0;
		} else {
			b->banstr = nextbanstr;
			return extban->is_banned(b);
		}
	}
	else
	{
		/* Is a n!u@h mask. */
		return match_user(b->banstr, b->client, MATCH_CHECK_ALL);
	}
}

/** is_banned_with_nick - Check if a user is banned on a channel.
 * @param client   Client to check (can be remote client)
 * @param channel  Channel to check
 * @param type   Type of ban to check for (BANCHK_*)
 * @param nick   Nick of the user (or NULL, to default to client->name)
 * @param msg    Message, only for some BANCHK_* types, otherwise NULL
 * @returns      A pointer to the ban struct if banned, otherwise NULL.
 */
Ban *is_banned_with_nick(Client *client, Channel *channel, int type, const char *nick, const char **msg, const char **errmsg)
{
	Ban *ban, *ex;
	char savednick[NICKLEN+1];
	BanContext *b = safe_alloc(sizeof(BanContext));

	/* It's not really doable to pass 'nick' to all the ban layers,
	 * including extbans (with stacking) and so on. Or at least not
	 * without breaking several module API's.
	 * So, instead, we temporarily set 'client->name' to 'nick' and
	 * restore it to the orginal value at the end of this function.
	 * This is possible because all these layers never send a message
	 * to 'client' and only indicate success/failure.
	 * Note that all this ONLY happens if is_banned_with_nick() is called
	 * with a non-NULL nick. That doesn't happen much. In UnrealIRCd
	 * only in case of '/NICK newnick'. This fixes #5165.
	 */
	if (nick)
	{
		strlcpy(savednick, client->name, sizeof(savednick));
		strlcpy(client->name, nick, sizeof(client->name));
	}

	b->client = client;
	b->channel = channel;
	b->ban_check_types = type;
	if (msg)
		b->msg = *msg;

	/* We check +b first, if a +b is found we then see if there is a +e.
	 * If a +e was found we return NULL, if not, we return the ban.
	 */

	for (ban = channel->banlist; ban; ban = ban->next)
	{
		b->banstr = ban->banstr;
		if (ban_check_mask(b))
			break;
	}

	if (ban)
	{
		/* Ban found, now check for +e */
		for (ex = channel->exlist; ex; ex = ex->next)
		{
			b->banstr = ex->banstr;
			if (ban_check_mask(b))
			{
				/* except matched */
				ban = NULL;
				break;
			}
		}
		/* user is not on except, 'ban' stays non-NULL. */
	}

	if (nick)
	{
		/* Restore the nick */
		strlcpy(client->name, savednick, sizeof(client->name));
	}

	/* OUT: */
	if (msg)
		*msg = b->msg;
	if (errmsg)
		*errmsg = b->error_msg;

	safe_free(b);
	return ban;
}

/** Checks if a ban already exists */
int ban_exists(Ban *lst, const char *str)
{
	for (; lst; lst = lst->next)
		if (!mycmp(lst->banstr, str))
			return 1;
	return 0;
}

/** Checks if a ban already exists - special version.
 * This ignores the "~time:xx:" suffixes in the banlist.
 * So it will return 1 if a ban is there for ~time:5:blah!*@*
 * and you call ban_exists_ignore_time(channel->banlist, "blah!*@*")
 */
int ban_exists_ignore_time(Ban *lst, const char *str)
{
	const char *p;

	for (; lst; lst = lst->next)
	{
		if (!strncmp(lst->banstr, "~time:", 6))
		{
			/* Special treatment for ~time:xx: */
			p = strchr(lst->banstr+6, ':');
			if (p)
			{
				p++;
				if (!mycmp(p, str))
					return 1;
			}
		} else
		{
			/* The simple version */
			if (!mycmp(lst->banstr, str))
				return 1;
		}
	}
	return 0;
}

/** Add user to the channel.
 * This adds both the Member struct to the channel->members linked list
 * and also the Membership struct to the client->user->channel linked list.
 * @note This does NOT send the JOIN, it only does the linked list stuff.
 */
void add_user_to_channel(Channel *channel, Client *client, const char *modes)
{
	Member *m;
	Membership *mb;
	const char *p;

	if (!client->user)
		return;

	m = make_member();
	m->client = client;
	m->next = channel->members;
	channel->members = m;
	channel->users++;

	mb = make_membership();
	mb->channel = channel;
	mb->next = client->user->channel;
	client->user->channel = mb;
	client->user->joined++;

	for (p = modes; *p; p++)
		add_member_mode_fast(m, mb, *p);

	RunHook(HOOKTYPE_JOIN_DATA, client, channel);
}

/** Remove the user from the channel.
 * This frees the memberships, decreases the user counts,
 * destroys the channel if needed, etc.
 * This does not send any PART/KICK/..!
 * @param client	The client that is removed from the channel
 * @param channel	The channel
 * @param dont_log	Set to 1 if it should not be logged as a part,
 *                      for example if you are already logging it as a kick.
 */
int remove_user_from_channel(Client *client, Channel *channel, int dont_log)
{
	Member **m;
	Member *m2;
	Membership **mb;
	Membership *mb2;

	/* Update channel->members list */
	for (m = &channel->members; (m2 = *m); m = &m2->next)
	{
		if (m2->client == client)
		{
			*m = m2->next;
			free_member(m2);
			break;
		}
	}

	/* Update client->user->channel list */
	for (mb = &client->user->channel; (mb2 = *mb); mb = &mb2->next)
	{
		if (mb2->channel == channel)
		{
			*mb = mb2->next;
			free_membership(mb2);
			break;
		}
	}

	/* Update user record to reflect 1 less joined */
	client->user->joined--;

	if (!dont_log)
	{
		if (MyUser(client))
		{
			unreal_log(ULOG_INFO, "part", "LOCAL_CLIENT_PART", client,
				   "User $client left $channel",
				   log_data_channel("channel", channel));
		} else {
			unreal_log(ULOG_INFO, "part", "REMOTE_CLIENT_PART", client,
				   "User $client left $channel",
				   log_data_channel("channel", channel));
		}
	}

	/* Now sub1_from_channel() will deal with the channel record
	 * and destroy the channel if needed.
	 */
	return sub1_from_channel(channel);
}

/** Returns 1 if channel has this channel mode set and 0 if not */
int has_channel_mode(Channel *channel, char mode)
{
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->letter == mode) && (channel->mode.mode & cm->mode))
			return 1;

	return 0; /* Not found */
}

/** Returns 1 if channel has this mode is set and 0 if not */
int has_channel_mode_raw(Cmode_t m, char mode)
{
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
		if ((cm->letter == mode) && (m & cm->mode))
			return 1;

	return 0; /* Not found */
}

/** Get the extended channel mode 'bit' value (eg: 0x20) by character (eg: 'Z') */
Cmode_t get_extmode_bitbychar(char m)
{
	Cmode *cm;

	for (cm=channelmodes; cm; cm = cm->next)
                if (cm->letter == m)
                        return cm->mode;

        return 0;
}

/** Write the "simple" list of channel modes for channel channel onto buffer mbuf with the parameters in pbuf.
 * @param client		The client requesting the mode list (can be NULL)
 * @param mbuf			Modes will be stored here
 * @param pbuf			Mode parameters will be stored here
 * @param mbuf_size		Length of the mbuf buffer
 * @param pbuf_size		Length of the pbuf buffer
 * @param channel		The channel to fetch modes from
 * @param hide_local_modes	If set to 1 then we will hide local channel modes like Z and d
 *				(eg: if you intend to send the buffer to a remote server)
 */
void channel_modes(Client *client, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, Channel *channel, int hide_local_modes)
{
	int show_mode_parameters = 0;
	Cmode *cm;

	if (!mbuf_size || !pbuf_size)
		return;

	if (!client || IsMember(client, channel) || IsServer(client) || IsMe(client) || IsULine(client) ||
	    ValidatePermissionsForPath("channel:see:mode:remote",client,NULL,channel,NULL))
	{
		show_mode_parameters = 1;
	}

	*pbuf = '\0';
	strlcpy(mbuf, "+", mbuf_size);

	for (cm=channelmodes; cm; cm = cm->next)
	{
		if (cm->letter &&
		    !(hide_local_modes && cm->local) &&
		    (channel->mode.mode & cm->mode))
		{
			char flag = cm->letter;

			if (mbuf_size)
				strlcat_letter(mbuf, flag, mbuf_size);

			if (cm->paracount && show_mode_parameters)
			{
				strlcat(pbuf, cm_getparameter(channel, flag), pbuf_size);
				strlcat(pbuf, " ", pbuf_size);
			}
		}
	}

	/* Remove the trailing space from the parameters -- codemastr */
	if (*pbuf)
		pbuf[strlen(pbuf)-1]='\0';
}

/** Make a pretty mask from the input string - only used by SILENCE
 */
char *pretty_mask(const char *mask_in)
{
	char mask[512];
	char *cp, *user, *host;

	strlcpy(mask, mask_in, sizeof(mask));

	if ((user = strchr((cp = mask), '!')))
		*user++ = '\0';

	if ((host = strrchr(user ? user : cp, '@')))
	{
		*host++ = '\0';
		if (!user)
			return make_nick_user_host(NULL, cp, host);
	}
	else if (!user && strchr(cp, '.'))
	{
		return make_nick_user_host(NULL, NULL, cp);
	}
	return make_nick_user_host(cp, user, host);
}

/** Trim a string - rather than cutting it off sharply, this adds a * at the end.
 * So "toolong" becomes "toolon*"
 */
char *trim_str(char *str, int len)
{
	int l;
	if (!str)
		return NULL;
	if ((l = strlen(str)) > len)
	{
		str += l - len;
		*str = '*';
	}
	return str;
}

/* Convert regular ban (non-extban) if needed.
 * This does things like:
 * nick!user@host -> nick!user@host (usually no change)
 * nickkkkkkkkkkkkkkkkkkkkkkkkkk!user@host -> nickkkkkkk*!user@host (dealing with NICKLEN restrictions and such).
 * user@host -> *!user@host
 * 1.2.3.4 -> *!*@1.2.3.4 (converting IP to a proper mask)
 * @param mask		Incoming mask (this will be touched/fragged!)
 * @param buf		Output buffer
 * @param buflen	Length of the output buffer, eg sizeof(buf)
 * @retval The sanitized mask, or NULL if it should be rejected fully.
 * @note Since 'mask' will be fragged, you most likely wish to pass a copy of it rather than the original.
 */
const char *convert_regular_ban(char *mask, char *buf, size_t buflen)
{
	static char namebuf[USERLEN + HOSTLEN + 6];
	char *user, *host;

	if (!*mask)
		return NULL; /* empty extban */

	if (!buf)
	{
		buf = namebuf;
		buflen = sizeof(namebuf);
	}

	if ((*mask == '~') && !strchr(mask, '@'))
	{
		/* has a '~', which makes it look like an extban,
		 * but is not a user@host ban, too confusing.
		 */
		return NULL;
	}

	if ((user = strchr(mask, '!')))
		*user++ = '\0';

	if ((host = strrchr(user ? user : mask, '@')))
	{
		*host++ = '\0';
		if (!user)
			return make_nick_user_host_r(buf, buflen, NULL, trim_str(mask,USERLEN), trim_str(host,HOSTLEN));
	}
	else if (!user && (strchr(mask, '.') || strchr(mask, ':')))
	{
		/* 1.2.3.4 -> *!*@1.2.3.4 (and the same for IPv6) */
		return make_nick_user_host_r(buf, buflen, NULL, NULL, trim_str(mask,HOSTLEN));
	}

	/* regular nick!user@host with the auto-trimming feature */
	return make_nick_user_host_r(buf, buflen, trim_str(mask,NICKLEN), trim_str(user,USERLEN), trim_str(host,HOSTLEN));
}

/** Make a proper ban mask.
 * This takes user input (eg: "nick") and converts it to a mask suitable
 * in the +beI lists (eg: "nick!*@*"). It also deals with extended bans,
 * in which case it will call the extban->conv_param() function.
 * @param mask		The ban mask
 * @param what		MODE_DEL or MODE_ADD
 * @param client	The client adding/removing this ban mask
 * @param conv_options	Options for BanContext.conv_options (eg BCTX_CONV_OPTION_WRITE_LETTER_BANS)
 * @returns pointer to correct banmask or NULL in case of error
 * @note A pointer is returned to a static buffer, which is overwritten
 *       on next clean_ban_mask or make_nick_user_host call.
 */
const char *clean_ban_mask(const char *mask_in, int what, Client *client, int conv_options)
{
	char *cp, *x;
	static char mask[512];

	/* Strip any ':' at beginning since that would cause a desync */
	for (; (*mask_in && (*mask_in == ':')); mask_in++);
	if (!*mask_in)
		return NULL;

	/* Work on a copy */
	strlcpy(mask, mask_in, sizeof(mask));

	cp = strchr(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Forbid ASCII <= 32 in all bans */
	for (x = mask; *x; x++)
		if (*x <= ' ')
			return NULL;

	/* Extended ban? */
	if (is_extended_ban(mask))
	{
		const char *nextbanstr;
		Extban *extban;

		if (RESTRICT_EXTENDEDBANS && MyUser(client) && !ValidatePermissionsForPath("immune:restrict-extendedbans",client,NULL,NULL,NULL))
		{
			if (!strcmp(RESTRICT_EXTENDEDBANS, "*"))
			{
				sendnotice(client, "Setting/removing of extended bans has been disabled");
				return NULL;
			}
			if (strchr(RESTRICT_EXTENDEDBANS, mask[1]))
			{
				sendnotice(client, "Setting/removing of extended bantypes '%s' has been disabled",
					RESTRICT_EXTENDEDBANS);
				return NULL;
			}
		}

		extban = findmod_by_bantype(mask, &nextbanstr);
		if (!extban)
		{
			/* extended bantype not supported, what to do?
			 * Here are the rules:
			 * - if from a remote client/server: allow it (easy upgrading,
			 *   no desync)
			 * - if from a local client trying to REMOVE the extban,
			 *   allow it too (so you don't get "unremovable" extbans).
			 */
			if (!MyUser(client) || (what == MODE_DEL))
				return mask; /* allow it */
			return NULL; /* reject */
		}

		if (extban->conv_param)
		{
			const char *ret;
			static char retbuf[512];
			BanContext *b = safe_alloc(sizeof(BanContext));
			b->client = client;
			b->what = what;
			b->banstr = nextbanstr;
			b->conv_options = conv_options;
			ret = extban->conv_param(b, extban);
			ret = prefix_with_extban(ret, b, extban, retbuf, sizeof(retbuf));
			safe_free(b);
			return ret;
		}
		/* else, do some basic sanity checks and cut it off at 80 bytes */
		if ((mask[1] != ':') || (mask[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	return convert_regular_ban(mask, NULL, 0);
}

/** Check if 'client' matches an invite exception (+I) on 'channel' */
int find_invex(Channel *channel, Client *client)
{
	Ban *inv;
	BanContext *b = safe_alloc(sizeof(BanContext));

	b->client = client;
	b->channel = channel;
	b->ban_check_types = BANCHK_JOIN;

	for (inv = channel->invexlist; inv; inv = inv->next)
	{
		b->banstr = inv->banstr;
		if (ban_check_mask(b))
		{
			safe_free(b);
			return 1;
		}
	}

	safe_free(b);
	return 0;
}

/** Remove unwanted characters from channel name.
 * You must call this before creating a new channel,
 * eg in case of /JOIN.
 */
int valid_channelname(const char *cname)
{
	const char *p;

	/* Channel name must start with a dash */
	if (*cname != '#')
		return 0;

	if (strlen(cname) > CHANNELLEN)
		return 0;

	if ((iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ANY) || !iConf.allowed_channelchars)
	{
		/* The default up to and including UnrealIRCd 4 */
		for (p = cname; *p; p++)
		{
			if (*p < 33 || *p == ',' || *p == ':')
				return 0;
		}
	} else
	if (iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ASCII)
	{
		/* The strict setting: only allow ASCII 32-128, except some chars */
		for (p = cname; *p; p++)
		{
			if (*p < 33 || *p == ',' || *p == ':' || *p > 127)
				return 0;
		}
	} else
	if (iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_UTF8)
	{
		/* Only allow UTF8, and also disallow some chars */
		for (p = cname; *p; p++)
		{
			if (*p < 33 || *p == ',' || *p == ':')
				return 0;
		}
		/* And run it through the UTF8 validator */
		if (!unrl_utf8_validate(cname, (const char **)&p))
			return 0;
	} else
	{
		/* Impossible */
		abort();
	}
	return 1; /* Valid */
}

void initlist_channels(void)
{
	channel_pool = mp_pool_new(sizeof(Channel), 512 * 1024);
}

/** Create channel 'name' (or if it exists, return the existing one)
 * @param name		Channel name
 * @param flag		If set to 'CREATE' then the channel is
 *			created if it does not exist.
 * @returns Pointer to channel (new or existing).
 * @note Be sure to call valid_channelname() first before
 *       you blindly call this function!
 */
Channel *make_channel(const char *name)
{
	Channel *channel;
	int len;
	char *p;
	char namebuf[CHANNELLEN+1];

	if (BadPtr(name))
		return NULL;

	/* Copy and silently truncate */
	strlcpy(namebuf, name, sizeof(namebuf));

	/* Copied from valid_channelname(), the minimal requirements */
	for (p = namebuf; *p; p++)
	{
		if (*p < 33 || *p == ',' || *p == ':')
		{
			*p = '\0';
			break;
		}
	}

	/* Exists? Return it. */
	if ((channel = find_channel(name)))
		return channel;

	channel = mp_pool_get(channel_pool);
	memset(channel, 0, sizeof(Channel));

	strlcpy(channel->name, name, sizeof(channel->name));

	if (channels)
		channels->prevch = channel;

	channel->topic = NULL;
	channel->topic_nick = NULL;
	channel->prevch = NULL;
	channel->nextch = channels;
	channel->creationtime = TStime();
	channels = channel;
	add_to_channel_hash_table(channel->name, channel);
	irccounts.channels++;

	RunHook(HOOKTYPE_CHANNEL_CREATE, channel);

	return channel;
}

/** Is the user 'client' invited to channel 'channel' by a chanop?
 * @param client	The client who was invited
 * @param channel	The channel to which the person was invited
 */
int is_invited(Client *client, Channel *channel)
{
	int invited = 0;
	RunHook(HOOKTYPE_IS_INVITED, client, channel, &invited);
	return invited;
}

/** Subtract one user from channel i. Free the channel if it became empty.
 * @param channel The channel
 * @returns 1 if the channel was freed, 0 if the channel still exists.
 */
int sub1_from_channel(Channel *channel)
{
	Ban *ban;
	Link *lp;
	int should_destroy = 1;

	--channel->users;
	if (channel->users > 0)
		return 0;

	/* No users in the channel anymore */
	channel->users = 0; /* to be sure */

	/* If the channel is +P then this hook will actually stop destruction. */
	RunHook(HOOKTYPE_CHANNEL_DESTROY, channel, &should_destroy);
	if (!should_destroy)
		return 0;

	/* We are now going to destroy the channel.
	 * But first we will destroy all kinds of references and lists...
	 */

	moddata_free_channel(channel);

	while (channel->banlist)
	{
		ban = channel->banlist;
		channel->banlist = ban->next;
		safe_free(ban->banstr);
		safe_free(ban->who);
		free_ban(ban);
	}
	while (channel->exlist)
	{
		ban = channel->exlist;
		channel->exlist = ban->next;
		safe_free(ban->banstr);
		safe_free(ban->who);
		free_ban(ban);
	}
	while (channel->invexlist)
	{
		ban = channel->invexlist;
		channel->invexlist = ban->next;
		safe_free(ban->banstr);
		safe_free(ban->who);
		free_ban(ban);
	}

	/* free extcmode params */
	extcmode_free_paramlist(channel->mode.mode_params);

	safe_free(channel->mode_lock);
	safe_free(channel->topic);
	safe_free(channel->topic_nick);

	if (channel->prevch)
		channel->prevch->nextch = channel->nextch;
	else
		channels = channel->nextch;

	if (channel->nextch)
		channel->nextch->prevch = channel->prevch;
	del_from_channel_hash_table(channel->name, channel);

	irccounts.channels--;
	mp_pool_release(channel);
	return 1;
}

/** Set channel mode lock on the channel, these are modes that users cannot change.
 * @param client	The client or server issueing the MLOCK
 * @param channel	The channel that will be MLOCK'ed
 * @param newmlock	The MLOCK string: list of mode characters that are locked
 */
void set_channel_mlock(Client *client, Channel *channel, const char *newmlock, int propagate)
{
	safe_strdup(channel->mode_lock, newmlock);

	if (propagate)
	{
		sendto_server(client, 0, 0, NULL, ":%s MLOCK %lld %s :%s",
			      client->id, (long long)channel->creationtime, channel->name,
			      BadPtr(channel->mode_lock) ? "" : channel->mode_lock);
	}
}

/** Parse a channelmode line.
 * @in pm A ParseMode struct, used to return values and to maintain internal state.
 * @in modebuf_in Buffer pointing to mode characters (eg: +snk-l)
 * @in parabuf_in Buffer pointing to all parameters (eg: key 123)
 * @retval Returns 1 if we have valid data to return, 0 if at end of mode line.
 * @section parse_chanmode_example Example:
 * @code
 * ParseMode pm;
 * int ret;
 * for (ret = parse_chanmode(&pm, modebuf, parabuf); ret; ret = parse_chanmode(&pm, NULL, NULL))
 * {
 *         unreal_log(ULOG_INFO, "test", "TEST", "Got %c%c %s",
 *                    pm.what == MODE_ADD ? '+' : '-',
 *                    pm.modechar,
 *                    pm.param ? pm.param : "");
 * }
 * @endcode
 */
int parse_chanmode(ParseMode *pm, const char *modebuf_in, const char *parabuf_in)
{
	if (modebuf_in)
	{
		/* Initialize */
		memset(pm, 0, sizeof(ParseMode));
		pm->modebuf = modebuf_in;
		pm->parabuf = parabuf_in;
		pm->what = MODE_ADD;
	}

	while(1)
	{
		if (*pm->modebuf == '\0')
			return 0;
		else if (*pm->modebuf == '+')
		{
			pm->what = MODE_ADD;
			pm->modebuf++;
			continue;
		}
		else if (*pm->modebuf == '-')
		{
			pm->what = MODE_DEL;
			pm->modebuf++;
			continue;
		}
		else
		{
			CoreChannelModeTable *tab = &corechannelmodetable[0];
			Cmode *cm;
			int eatparam = 0;

			/* Set some defaults */
			pm->extm = NULL;
			pm->modechar = *pm->modebuf;
			pm->param = NULL;

			while (tab->mode != 0x0)
			{
				if (tab->flag == *pm->modebuf)
					break;
				tab++;
			}

			if (tab->mode)
			{
				/* INTERNAL MODE */
				if (tab->parameters)
				{
					eatparam = 1;
				}
			} else {
				/* EXTENDED CHANNEL MODE */
				int found = 0;
				for (cm=channelmodes; cm; cm = cm->next)
				{
					if (cm->letter == *pm->modebuf)
					{
						found = 1;
						break;
					}
				}
				if (!found)
				{
					/* Not found. Will be ignored, just move on.. */
					pm->modebuf++;
					continue;
				}
				pm->extm = cm;
				if (cm->paracount == 1)
				{
					if (pm->what == MODE_ADD)
						eatparam = 1;
					else if (cm->unset_with_param)
						eatparam = 1;
					/* else 0 (if MODE_DEL && !unset_with_param) */
				}
			}

			if (eatparam)
			{
				/* Hungry.. */
				if (pm->parabuf && *pm->parabuf)
				{
					const char *start, *end;
					for (; *pm->parabuf == ' '; pm->parabuf++); /* skip whitespace */
					start = pm->parabuf;
					if (*pm->parabuf == '\0')
					{
						pm->modebuf++;
						continue; /* invalid, got mode but no parameter available */
					}
					end = strchr(start, ' ');
					/* copy start .. end (where end may be null, then just copy all) */
					if (end)
					{
						pm->parabuf = end + 1; /* point to next param, or \0 */
						if (end - start + 1 > sizeof(pm->buf))
							end = start + sizeof(pm->buf); /* 'never' reached */
						strlcpy(pm->buf, start, end - start + 1);
					}
					else
					{
						strlcpy(pm->buf, start, sizeof(pm->buf));
						pm->parabuf = pm->parabuf + strlen(pm->parabuf); /* point to \0 at end */
					}
					stripcrlf(pm->buf); /* needed for unreal_server_compat.c */
					pm->param = pm->buf;
				} else {
					pm->modebuf++;
					continue; /* invalid, got mode but no parameter available */
				}
			}
		}
		pm->modebuf++; /* advance pointer */
		return 1;
	}
}

/** Returns 1 if both clients are at least in 1 same channel */
int has_common_channels(Client *c1, Client *c2)
{
	Membership *lp;

	for (lp = c1->user->channel; lp; lp = lp->next)
	{
		if (IsMember(c2, lp->channel) && user_can_see_member(c1, c2, lp->channel))
			return 1;
	}
	return 0;
}

/** Returns 1 if user 'user' can see channel member 'target' - fast version.
 * This may return 0 if the user is 'invisible' due to mode +D rules.
 * @param user			The user who is looking around
 * @param target		The target user who is being investigated
 * @param channel		The channel
 * @param target_member		The Member * struct of 'target'
 * @param user_member_modes	The member modes that 'user' has, eg "o". Can be NULL if not in channel.
 */
int user_can_see_member_fast(Client *user, Client *target, Channel *channel, Member *target_member, const char *user_member_modes)
{
	Hook *h;
	int j = 0;

	if (user == target)
		return 1;

	for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(target, channel, target_member);
		if (j != 0)
			break;
	}

	/* Requested to hide the person, but make sure neither one is +hoaq... */
	if ((j != 0) &&
	    !check_channel_access_member(target_member, "vhoaq") &&
	    !(user_member_modes && check_channel_access_string(user_member_modes, "hoaq")))
	{
		return 0;
	}

	return 1;
}

/** Returns 1 if user 'user' can see channel member 'target'.
 * This may return 0 if the user is 'invisible' due to mode +D rules.
 * NOTE: Membership is unchecked, assumed membership of both.
 */
int user_can_see_member(Client *user, Client *target, Channel *channel)
{
	Membership *user_member = NULL;
	Member *target_member = NULL;

	if (user == target)
		return 1;

	if (IsUser(user))
		user_member = find_membership_link(user->user->channel, channel);

	if (IsUser(target))
		target_member = find_member_link(channel->members, target); // SLOW!

	/* User is not in channel, yeah what shall we return? :D */
	if (!target_member)
		return 0;

	return user_can_see_member_fast(user, target, channel, target_member, user_member ? user_member->member_modes : NULL);
}

/** Returns 1 if user 'target' is invisible in channel 'channel'.
 * This may return 0 if the user is 'invisible' due to mode +D rules.
 */
int invisible_user_in_channel(Client *target, Channel *channel)
{
	Hook *h;
	Member *target_member;
	int j = 0;

	target_member = find_member_link(channel->members, target); // SLOW!
	if (!target_member)
		return 0; /* not in channel */

	for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(target,channel,target_member);
		if (j != 0)
			break;
	}

	/* We must ensure that user is allowed to "see" target */
	// SLOW !!!
	if (j != 0 && !(check_channel_access(target, channel, "hoaq") || check_channel_access(target,channel, "v")))
		return 1;

	return 0;
}

/** Send a message to the user that (s)he is using an invalid channel name.
 * This is usually called after an if (MyUser(client) && !valid_channelname(name)).
 * @param client      The client to send the message to.
 * @param channelname The (invalid) channel that the user tried to join.
 */
void send_invalid_channelname(Client *client, const char *channelname)
{
	const char *reason;

	if (*channelname != '#')
	{
		reason = "Channel name must start with a hash mark (#)";
	} else
	if (strlen(channelname) > CHANNELLEN)
	{
		reason = "Channel name is too long";
	} else {
		switch(iConf.allowed_channelchars)
		{
			case ALLOWED_CHANNELCHARS_ASCII:
				reason = "Channel name contains illegal characters (must be ASCII)";
				break;
			case ALLOWED_CHANNELCHARS_UTF8:
				reason = "Channel name contains illegal characters (must be valid UTF8)";
				break;
			case ALLOWED_CHANNELCHARS_ANY:
			default:
				reason = "Channel name contains illegal characters";
		}
	}

	sendnumeric(client, ERR_FORBIDDENCHANNEL, channelname, reason);
}

/** Is the provided string possibly an extended ban?
 * Note that it still may not exist, it just tests the first part.
 * @param str	The string to check (eg "~account:xyz")
 */
int is_extended_ban(const char *str)
{
	const char *p;

	if (*str != '~')
		return 0;
	for (p = str+1; *p; p++)
	{
		if (!isalnum(*p))
		{
			if (*p == ':')
				return 1;
		}
	}
	return 0;
}

/** Is the provided string possibly an extended server ban?
 * Actually this is only a very light check.
 * It may still not exist, it just tests the first part.
 * @param str	The string to check (eg "~account:xyz")
 * The only difference between this and is_extended_ban()
 * is that we allow a % at the beginning for soft-bans.
 * @see is_extended_ban()
 */
int is_extended_server_ban(const char *str)
{
	if (*str == '%')
		str++;
	return is_extended_ban(str);
}

/** Check if it is an empty (useless) mode, namely "", "+" or "-".
 * Typically called as: empty_mode(modebuf)
 */
int empty_mode(const char *m)
{
	if (!*m || (((m[0] == '+') || (m[0] == '-')) && m[1] == '\0'))
		return 1;
	return 0;
}

/** Free everything of/in a MultiLineMode */
void free_multilinemode(MultiLineMode *m)
{
	int i;
	if (m == NULL)
		return;
	for (i=0; i < m->numlines; i++)
	{
		safe_free(m->modeline[i]);
		safe_free(m->paramline[i]);
	}
	safe_free(m);
}
