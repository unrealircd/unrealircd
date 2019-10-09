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

long opermode = 0;
long sajoinmode = 0;
Channel *channels = NULL;

/* some buffers for rebuilding channel/nick lists with comma's */
static char buf[BUFSIZE];
MODVAR char modebuf[BUFSIZE], parabuf[BUFSIZE];

/** This describes the letters, modes and options for core channel modes.
 * These are +ntmispklr and also the list modes +vhoaq and +beI.
 */
CoreChannelModeTable corechannelmodetable[] = {
	{MODE_LIMIT, 'l', 1, 1},
	{MODE_VOICE, 'v', 1, 1},
	{MODE_HALFOP, 'h', 0, 1},
	{MODE_CHANOP, 'o', 0, 1},
	{MODE_PRIVATE, 'p', 0, 0},
	{MODE_SECRET, 's', 0, 0},
	{MODE_MODERATED, 'm', 1, 0},
	{MODE_NOPRIVMSGS, 'n', 1, 0},
	{MODE_TOPICLIMIT, 't', 1, 0},
	{MODE_INVITEONLY, 'i', 1, 0},
	{MODE_KEY, 'k', 1, 1},
	{MODE_RGSTR, 'r', 0, 0},
	{MODE_CHANADMIN, 'a', 0, 1},
	{MODE_CHANOWNER, 'q', 0, 1},
	{MODE_BAN, 'b', 1, 1},
	{MODE_EXCEPT, 'e', 1, 1},	/* exception ban */
	{MODE_INVEX, 'I', 1, 1},	/* invite-only exception */
	{0x0, 0x0, 0x0, 0x0}
};

char cmodestring[512];

/* Some forward declarations */
char *clean_ban_mask(char *, int, Client *);
void channel_modes(Client *client, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, Channel *channel);
int sub1_from_channel(Channel *);
void del_invite(Client *, Channel *);

inline int op_can_override(char* acl, Client *client,Channel *channel,void* extra)
{
#ifndef NO_OPEROVERRIDE
	if (MyUser(client) && !(ValidatePermissionsForPath(acl,client,NULL,channel,extra)))
		return 0;
	return 1;
#else
	return 0;
#endif
}

int  Halfop_mode(long mode)
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


/*
 * return the length (>=0) of a chain of links.
 */
static int list_length(Link *lp)
{
	int  count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

Member	*find_member_link(Member *lp, Client *ptr)
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

static Member *make_member(void)
{
	Member *lp;
	unsigned int	i;

	if (freemember == NULL)
	{
		for (i = 1; i <= (4072/sizeof(Member)); ++i)
		{
			lp = safe_alloc(sizeof(Member));
			lp->client = NULL;
			lp->flags = 0;
			lp->next = freemember;
			freemember = lp;
		}
	}
	lp = freemember;
	freemember = freemember->next;
	lp->next = NULL;
	return lp;
}

static void free_member(Member *lp)
{
	if (!lp)
		return;
	moddata_free_member(lp);
	memset(lp, 0, sizeof(Member));
	lp->next = freemember;
	lp->client = NULL;
	lp->flags = 0;
	freemember = lp;
}

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

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
Client *find_chasing(Client *client, char *user, int *chasing)
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
int identical_ban(char *one, char *two)
{
	if ((*one == '~') && (strlen(one) > 3))
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
	return 0;
}

/** Add a listmode (+beI) with the specified banid to
 *  the specified channel. (Extended version with
 *  set by nick and set on timestamp)
 */
int add_listmode_ex(Ban **list, Client *client, Channel *channel, char *banid, char *setby, time_t seton)
{
	Ban *ban;
	int cnt = 0, len;
	int do_not_add = 0;

	if (MyUser(client))
		(void)collapse(banid);

	len = strlen(banid);
	if (!*list && ((len > MAXBANLENGTH) || (MAXBANS < 1)))
	{
		if (MyUser(client))
		{
			/* Only send the error to local clients */
			sendnumeric(client, ERR_BANLISTFULL, channel->chname, banid);
		}
		do_not_add = 1;
	}
	for (ban = *list; ban; ban = ban->next)
	{
		len += strlen(ban->banstr);
		/* Check MAXBANLENGTH / MAXBANS only for local clients
		 * and 'me' (for +b's set during +f).
		 */
		if ((MyUser(client) || IsMe(client)) && ((len > MAXBANLENGTH) || (++cnt >= MAXBANS)))
		{
			do_not_add = 1;
		}
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
				sendnumeric(client, ERR_BANLISTFULL, channel->chname, banid);
			}
			return -1;
		}
		ban = make_ban();
		ban->next = *list;
		*list = ban;
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
	return 0;
}

/** Add a listmode (+beI) with the specified banid to
 *  the specified channel. (Simplified version)
 */
int add_listmode(Ban **list, Client *client, Channel *channel, char *banid)
{
	char *setby = client->name;
	char nuhbuf[NICKLEN+USERLEN+HOSTLEN+4];

	if (IsUser(client) && (iConf.ban_setter == SETTER_NICK_USER_HOST))
		setby = make_nick_user_host_r(nuhbuf, client->name, client->user->username, GetHost(client));

	return add_listmode_ex(list, client, channel, banid, setby, TStime());
}

/*
 * del_listmode - delete a listmode (+beI) from a channel
 *                that matches the specified banid.
 */
int del_listmode(Ban **list, Channel *channel, char *banid)
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

/*
 * IsMember - returns 1 if a person is joined
 * Moved to struct.h
 */

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
inline Ban *is_banned(Client *client, Channel *channel, int type, char **msg, char **errmsg)
{
	return is_banned_with_nick(client, channel, type, NULL, msg, errmsg);
}

/** ban_check_mask - Checks if the user matches the specified n!u@h mask -or- run an extended ban.
 * @param client         Client to check (can be remote client)
 * @param channel        Channel to check
 * @param banstr       Mask string to check user
 * @param type         Type of ban to check for (BANCHK_*)
 * @param msg          Message, only for some BANCHK_* types, otherwise NULL.
 * @param errmsg       Error message, could be NULL
 * @param no_extbans   0 to check extbans, nonzero to disable extban checking.
 * @returns            Nonzero if the mask/extban succeeds. Zero if it doesn't.
 * @comments           This is basically extracting the mask and extban check from is_banned_with_nick, but with being a bit more strict in what an extban is.
 *                     Strange things could happen if this is called outside standard ban checking.
 */
inline int ban_check_mask(Client *client, Channel *channel, char *banstr, int type, char **msg, char **errmsg, int no_extbans)
{
	Extban *extban = NULL;
	if (!no_extbans && banstr[0] == '~' && banstr[1] != '\0' && banstr[2] == ':')
	{
		/* Is an extended ban. */
		extban = findmod_by_bantype(banstr[1]);
		if (!extban)
		{
			return 0;
		}
		else
		{
			return extban->is_banned(client, channel, banstr, type, msg, errmsg);
		}
	}
	else
	{
		/* Is a n!u@h mask. */
		return match_user(banstr, client, MATCH_CHECK_ALL);
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
Ban *is_banned_with_nick(Client *client, Channel *channel, int type, char *nick, char **msg, char **errmsg)
{
	Ban *ban, *ex;
	char savednick[NICKLEN+1];

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

	/* We check +b first, if a +b is found we then see if there is a +e.
	 * If a +e was found we return NULL, if not, we return the ban.
	 */

	for (ban = channel->banlist; ban; ban = ban->next)
	{
		if (ban_check_mask(client, channel, ban->banstr, type, msg, errmsg, 0))
			break;
	}

	if (ban)
	{
		/* Ban found, now check for +e */
		for (ex = channel->exlist; ex; ex = ex->next)
		{
			if (ban_check_mask(client, channel, ex->banstr, type, msg, errmsg, 0))
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

	return ban;
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
void add_user_to_channel(Channel *channel, Client *who, int flags)
{
	Member *m;
	Membership *mb;

	if (who->user)
	{
		m = make_member();
		m->client = who;
		m->flags = flags;
		m->next = channel->members;
		channel->members = m;
		channel->users++;

		mb = make_membership();
		mb->channel = channel;
		mb->next = who->user->channel;
		mb->flags = flags;
		who->user->channel = mb;
		who->user->joined++;
		RunHook2(HOOKTYPE_JOIN_DATA,who,channel);
	}
}

/** Remove the user from the channel.
 * This doesn't send any PART etc. It does the free'ing of
 * membership etc. It will also DESTROY the channel if the
 * user was the last user (and the channel is not +P),
 * via sub1_from_channel(), that is.
 */
int remove_user_from_channel(Client *client, Channel *channel)
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

	/* Now sub1_from_channel() will deal with the channel record
	 * and destroy the channel if needed.
	 */
	return sub1_from_channel(channel);
}

long get_access(Client *client, Channel *channel)
{
	Membership *lp;
	if (channel && IsUser(client))
		if ((lp = find_membership_link(client->user->channel, channel)))
			return lp->flags;
	return 0;
}

/** Returns 1 if channel has this channel mode set and 0 if not */
int has_channel_mode(Channel *channel, char mode)
{
	CoreChannelModeTable *tab = &corechannelmodetable[0];
	int i;

	/* Extended channel modes */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if ((Channelmode_Table[i].flag == mode) && (channel->mode.extmode & Channelmode_Table[i].mode))
			return 1;
	}

	/* Built-in channel modes */
	while (tab->mode != 0x0)
	{
		if ((channel->mode.mode & tab->mode) && (tab->flag == mode))
			return 1;
		tab++;
	}

	/* Special handling for +l (needed??) */
	if (channel->mode.limit && (mode == 'l'))
		return 1;

	/* Special handling for +k (needed??) */
	if (channel->mode.key[0] && (mode == 'k'))
		return 1;

	return 0; /* Not found */
}

Cmode_t get_extmode_bitbychar(char m)
{
        int extm;
        for (extm=0; extm <= Channelmode_highest; extm++)
        {
                if (Channelmode_Table[extm].flag == m)
                        return Channelmode_Table[extm].mode;
        }
        return 0;
}

long get_mode_bitbychar(char m)
{
	CoreChannelModeTable *tab = &corechannelmodetable[0];

	while(tab->mode != 0x0)
	{
		if (tab->flag == m)
			return tab->mode;
		tab++;;
	}
	return 0;
}

/*
 * write the "simple" list of channel modes for channel channel onto buffer mbuf
 * with the parameters in pbuf.
 */
/* TODO: this function has many security issues and needs an audit, maybe even a recode */
void channel_modes(Client *client, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, Channel *channel)
{
	CoreChannelModeTable *tab = &corechannelmodetable[0];
	int ismember;
	int i;

	if (!(mbuf_size && pbuf_size)) return;

	ismember = (IsMember(client, channel) || IsServer(client) || IsMe(client) || IsULine(client)) ? 1 : 0;

	*pbuf = '\0';

	*mbuf++ = '+';
	mbuf_size--;
	/* Paramless first */
	while (mbuf_size && tab->mode != 0x0)
	{
		if ((channel->mode.mode & tab->mode))
			if (!tab->parameters) {
				*mbuf++ = tab->flag;
				mbuf_size--;
			}
		tab++;
	}
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!mbuf_size) break;
		if (Channelmode_Table[i].flag && !Channelmode_Table[i].paracount &&
		    (channel->mode.extmode & Channelmode_Table[i].mode)) {
			*mbuf++ = Channelmode_Table[i].flag;
			mbuf_size--;
		}
	}
	if (channel->mode.limit)
	{
		if (mbuf_size) {
			*mbuf++ = 'l';
			mbuf_size--;
		}
		if (ismember) {
			ircsnprintf(pbuf, pbuf_size, "%d ", channel->mode.limit);
			pbuf_size-=strlen(pbuf);
			pbuf+=strlen(pbuf);
		}
	}
	if (*channel->mode.key)
	{
		if (mbuf_size) {
			*mbuf++ = 'k';
			mbuf_size--;
		}
		if (ismember && pbuf_size) {
			ircsnprintf(pbuf, pbuf_size, "%s ", channel->mode.key);
			pbuf_size-=strlen(pbuf);
			pbuf+=strlen(pbuf);
		}
	}

	for (i=0; i <= Channelmode_highest; i++)
	{
		if (Channelmode_Table[i].flag && Channelmode_Table[i].paracount &&
		    (channel->mode.extmode & Channelmode_Table[i].mode)) {
		        char flag = Channelmode_Table[i].flag;
			if (mbuf_size) {
				*mbuf++ = flag;
				mbuf_size--;
			}
			if (ismember)
			{
				ircsnprintf(pbuf, pbuf_size, "%s ", cm_getparameter(channel, flag));
				pbuf_size-=strlen(pbuf);
				pbuf+=strlen(pbuf);
			}
		}
	}

	/* Remove the trailing space from the parameters -- codemastr */
	if (*pbuf) pbuf[strlen(pbuf)-1]=0;

	if (!mbuf_size) mbuf--;
	*mbuf++ = '\0';
	return;
}


int  DoesOp(char *modebuf)
{
	modebuf--;		/* Is it possible that a mode starts with o and not +o ? */
	while (*++modebuf)
		if (*modebuf == 'h' || *modebuf == 'o'
		    || *modebuf == 'v' || *modebuf == 'q')
			return (1);
	return 0;
}

/* This function is only used for non-SJOIN servers. So don't bother with mtags support.. */
int  sendmodeto_one(Client *client, char *from, char *name, char *mode, char *param, time_t creationtime)
{
	if ((IsServer(client) && DoesOp(mode) && creationtime) || IsULine(client))
		sendto_one(client, NULL, ":%s MODE %s %s %s %lld", from,
		    name, mode, param, (long long)creationtime);
	else
		sendto_one(client, NULL, ":%s MODE %s %s %s", from, name, mode, param);

	return 0;
}

char *pretty_mask(char *mask)
{
	char *cp;
	char *user;
	char *host;

	if ((user = strchr((cp = mask), '!')))
		*user++ = '\0';
	if ((host = strrchr(user ? user : cp, '@')))
	{
		*host++ = '\0';
		if (!user)
			return make_nick_user_host(NULL, cp, host);
	}
	else if (!user && strchr(cp, '.'))
		return make_nick_user_host(NULL, NULL, cp);
	return make_nick_user_host(cp, user, host);
}

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

/* clean_ban_mask:	makes a proper banmask
 * RETURNS: pointer to correct banmask or NULL in case of error
 * NOTES:
 * - A pointer is returned to a static buffer, which is overwritten
 *   on next clean_ban_mask or make_nick_user_host call.
 */
char *clean_ban_mask(char *mask, int what, Client *client)
{
	char *cp, *x;
	char *user;
	char *host;
	Extban *p;
	static char maskbuf[512];

	/* Work on a copy */
	strlcpy(maskbuf, mask, sizeof(maskbuf));
	mask = maskbuf;

	cp = strchr(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Strip any ':' at beginning since that would cause a desynch */
	for (; (*mask && (*mask == ':')); mask++);
	if (!*mask)
		return NULL;

	/* Forbid ASCII <= 32 in all bans */
	for (x = mask; *x; x++)
		if (*x <= ' ')
			return NULL;

	/* Extended ban? */
	if ((*mask == '~') && mask[1] && (mask[2] == ':'))
	{
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
		p = findmod_by_bantype(mask[1]);
		if (!p)
		{
			/* extended bantype not supported, what to do?
			 * Here are the rules:
			 * - if from a remote client/server: allow it (easy upgrading,
			 *   no desynch)
			 * - if from a local client trying to REMOVE the extban,
			 *   allow it too (so you don't get "unremovable" extbans).
			 */
			if (!MyUser(client) || (what == MODE_DEL))
				return mask; /* allow it */
			return NULL; /* reject */
		}
		if (p->conv_param)
			return p->conv_param(mask);
		/* else, do some basic sanity checks and cut it off at 80 bytes */
		if ((mask[1] != ':') || (mask[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not an extended ban and not a ~user@host ban either. */

	if ((user = strchr((cp = mask), '!')))
		*user++ = '\0';
	if ((host = strrchr(user ? user : cp, '@')))
	{
		*host++ = '\0';

		if (!user)
			return make_nick_user_host(NULL, trim_str(cp,USERLEN), 
				trim_str(host,HOSTLEN));
	}
	else if (!user && strchr(cp, '.'))
		return make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	return make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), 
		trim_str(host,HOSTLEN));
}

int find_invex(Channel *channel, Client *client)
{
	Ban *inv;

	for (inv = channel->invexlist; inv; inv = inv->next)
		if (ban_check_mask(client, channel, inv->banstr, BANCHK_JOIN, NULL, NULL, 0))
			return 1;

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

	if (iConf.allowed_channelchars == ALLOWED_CHANNELCHARS_ANY)
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

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
Channel *get_channel(Client *client, char *chname, int flag)
{
	Channel *channel;
	int  len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyUser(client) && len > CHANNELLEN)
	{
		len = CHANNELLEN;
		*(chname + CHANNELLEN) = '\0';
	}
	if ((channel = find_channel(chname, NULL)))
		return (channel);
	if (flag == CREATE)
	{
		channel = safe_alloc(sizeof(Channel) + len);
		strlcpy(channel->chname, chname, len + 1);
		if (channels)
			channels->prevch = channel;
		channel->topic = NULL;
		channel->topic_nick = NULL;
		channel->prevch = NULL;
		channel->nextch = channels;
		channel->creationtime = MyUser(client) ? TStime() : 0;
		channels = channel;
		(void)add_to_channel_hash_table(chname, channel);
		irccounts.channels++;
		RunHook2(HOOKTYPE_CHANNEL_CREATE, client, channel);
	}
	return channel;
}

/*
 * Slight changes in routine, now working somewhat symmetrical:
 *   First try to remove the client & channel pair to avoid duplicates
 *   Second check client & channel invite-list lengths and remove tail
 *   Finally add new invite-links to both client and channel
 * Should U-lined clients have higher limits?   -Donwulff
 */

void add_invite(Client *from, Client *to, Channel *channel, MessageTag *mtags)
{
	Link *inv, *tmp;

	del_invite(to, channel);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(to->user->invited) >= MAXCHANNELSPERUSER)
	{
		for (tmp = to->user->invited; tmp->next; tmp = tmp->next)
			;
		del_invite(to, tmp->value.channel);

	}
	/* We get pissy over too many invites per channel as well now,
	 * since otherwise mass-inviters could take up some major
	 * resources -Donwulff
	 */
	if (list_length(channel->invites) >= MAXCHANNELSPERUSER)
	{
		for (tmp = channel->invites; tmp->next; tmp = tmp->next)
			;
		del_invite(tmp->value.client, channel);
	}
	/*
	 * add client to the beginning of the channel invite list
	 */
	inv = make_link();
	inv->value.client = to;
	inv->next = channel->invites;
	channel->invites = inv;
	/*
	 * add channel to the beginning of the client invite list
	 */
	inv = make_link();
	inv->value.channel = channel;
	inv->next = to->user->invited;
	to->user->invited = inv;

	RunHook4(HOOKTYPE_INVITE, from, to, channel, mtags);
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void del_invite(Client *client, Channel *channel)
{
	Link **inv, *tmp;

	for (inv = &(channel->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.client == client)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}

	for (inv = &(client->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.channel == channel)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}
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
	RunHook2(HOOKTYPE_CHANNEL_DESTROY, channel, &should_destroy);
	if (!should_destroy)
		return 0;

	/* We are now going to destroy the channel.
	 * But first we will destroy all kinds of references and lists...
	 */

	while ((lp = channel->invites))
		del_invite(lp->value.client, channel);

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
	extcmode_free_paramlist(channel->mode.extmodeparams);

	safe_free(channel->mode_lock);
	safe_free(channel->topic);
	safe_free(channel->topic_nick);

	if (channel->prevch)
		channel->prevch->nextch = channel->nextch;
	else
		channels = channel->nextch;

	if (channel->nextch)
		channel->nextch->prevch = channel->prevch;
	(void)del_from_channel_hash_table(channel->chname, channel);

	irccounts.channels--;
	safe_free(channel);
	return 1;
}

/* This function is only used for non-SJOIN servers. So don't bother with mtags support.. */
// FIXME: remove this function in the cleanup
void send_user_joins(Client *client, Client *user)
{
	Membership *lp;
	Channel *channel;
	int  cnt = 0, len = 0, clen;
	char *mask;

	snprintf(buf, sizeof buf, ":%s JOIN ", user->name);
	len = strlen(buf);

	for (lp = user->user->channel; lp; lp = lp->next)
	{
		channel = lp->channel;
		if ((mask = strchr(channel->chname, ':')))
			if (!match_simple(++mask, client->name))
				continue;
		if (*channel->chname == '&')
			continue;
		clen = strlen(channel->chname);
		if (clen + 1 + len > BUFSIZE - 3)
		{
			if (cnt)
			{
				buf[len - 1] = '\0';
				sendto_one(client, NULL, "%s", buf);
			}
			snprintf(buf, sizeof buf, ":%s JOIN ", user->name);
			len = strlen(buf);
			cnt = 0;
		}
		(void)strlcpy(buf + len, channel->chname, sizeof buf-len);
		cnt++;
		len += clen;
		if (lp->next)
		{
			len++;
			(void)strlcat(buf, ",", sizeof buf);
		}
	}
	if (*buf && cnt)
		sendto_one(client, NULL, "%s", buf);

	return;
}

/* set_channel_mlock()
 *
 * inputs	- client, source, channel, params
 * output	- 
 * side effects - channel mlock is changed / MLOCK is propagated
 */
void set_channel_mlock(Client *client, Channel *channel, const char *newmlock, int propagate)
{
	safe_strdup(channel->mode_lock, newmlock);

	if (propagate)
	{
		sendto_server(client, 0, 0, NULL, ":%s MLOCK %lld %s :%s",
			      client->name, (long long)channel->creationtime, channel->chname,
			      BadPtr(channel->mode_lock) ? "" : channel->mode_lock);
	}
}

/** Parse a channelmode line.
 * @in pm A ParseMode struct, used to return values and to maintain internal state.
 * @in modebuf_in Buffer pointing to mode characters (eg: +snk-l)
 * @in parabuf_in Buffer pointing to all parameters (eg: key 123)
 * @retval Returns 1 if we have valid data to return, 0 if at end of mode line.
 * @example
 *
 * ParseMode pm;
 * int ret;
 * for (ret = parse_chanmode(&pm, modebuf, parabuf); ret; ret = parse_chanmode(&pm, NULL, NULL))
 * {
 *         ircd_log(LOG_ERROR, "Got %c%c %s",
 *                  pm.what == MODE_ADD ? '+' : '-',
 *                  pm.modechar,
 *                  pm.param ? pm.param : "");
 * }
 */
int parse_chanmode(ParseMode *pm, char *modebuf_in, char *parabuf_in)
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
			int i;
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
					if ((pm->what == MODE_DEL) && (tab->flag == 'l'))
						eatparam = 0; /* -l is special: no parameter required */
					else
						eatparam = 1; /* all other internal parameter modes do require a parameter on unset */
				}
			} else {
				/* EXTENDED CHANNEL MODE */
				int found = 0;
				for (i=0; i <= Channelmode_highest; i++)
					if (Channelmode_Table[i].flag == *pm->modebuf)
					{
						found = 1;
						break;
					}
				if (!found)
				{
					/* Not found. Will be ignored, just move on.. */
					pm->modebuf++;
					continue;
				}
				pm->extm = &Channelmode_Table[i];
				if (Channelmode_Table[i].paracount == 1)
				{
					if (pm->what == MODE_ADD)
						eatparam = 1;
					else if (Channelmode_Table[i].unset_with_param)
						eatparam = 1;
					/* else 0 (if MODE_DEL && !unset_with_param) */
				}
			}

			if (eatparam)
			{
				/* Hungry.. */
				if (pm->parabuf && *pm->parabuf)
				{
					char *start, *end;
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

/** Returns 1 if user 'user' can see channel member 'target'.
 * This may return 0 if the user is 'invisible' due to mode +D rules.
 * NOTE: Membership is unchecked, assumed membership of both.
 */
int user_can_see_member(Client *user, Client *target, Channel *channel)
{
	Hook *h;
	int j = 0;

	if (user == target)
		return 1;

	for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(target,channel);
		if (j != 0)
			break;
	}

	/* We must ensure that user is allowed to "see" target */
	if (j != 0 && !(is_skochanop(target, channel) || has_voice(target,channel)) && !is_skochanop(user, channel))
		return 0;

	return 1;
}

/** Returns 1 if user 'target' is invisible in channel 'channel'.
 * This may return 0 if the user is 'invisible' due to mode +D rules.
 */
int invisible_user_in_channel(Client *target, Channel *channel)
{
	Hook *h;
	int j = 0;

	for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(target,channel);
		if (j != 0)
			break;
	}

	/* We must ensure that user is allowed to "see" target */
	if (j != 0 && !(is_skochanop(target, channel) || has_voice(target,channel)))
		return 1;

	return 0;
}

/** Send a message to the user that (s)he is using an invalid channel name.
 * This is usually called after an if (MyUser(client) && !valid_channelname(name)).
 * @param client      The client to send the message to.
 * @param channelname The (invalid) channel that the user tried to join.
 */
void send_invalid_channelname(Client *client, char *channelname)
{
	char *reason;

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
