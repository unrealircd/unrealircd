/*
 *   IRC - Internet Relay Chat, src/modules/nick.c
 *   (C) 1999-2005 The UnrealIRCd Team
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
	"nick",
	"5.0",
	"command /nick",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Defines */

#define NICKCOL_EQUAL         0
#define NICKCOL_NEW_WON       1
#define NICKCOL_EXISTING_WON  2

/* Assume that on collision a NICK is in flight and the other server will take
 * the exact same decision we would do, and thus we don't send a KILL to cptr?
 * This works great with this code, seems to kill the correct person and not
 * cause desyncs even without UID/SID. HOWEVER.. who knows what code the other servers run?
 * Should use UID/SID anyway, then this whole problem doesn't exist.
 */
#define ASSUME_NICK_IN_FLIGHT

/* Variables */
static char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64];

/* Forward declarations */
CMD_FUNC(cmd_nick);
CMD_FUNC(cmd_nick_local);
CMD_FUNC(cmd_nick_remote);
CMD_FUNC(cmd_uid);
int _register_user(Client *client);
void nick_collision(Client *cptr, const char *newnick, const char *newid, Client *new, Client *existing, int type);
int AllowClient(Client *client);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_REGISTER_USER, _register_user);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, "NICK", cmd_nick, MAXPARA, CMD_USER|CMD_SERVER|CMD_UNREGISTERED);
	CommandAdd(modinfo->handle, "UID", cmd_uid, MAXPARA, CMD_SERVER);
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

/** Hmm.. don't we already have such a function? */
void set_user_modes_dont_spread(Client *client, const char *umode)
{
	const char *args[4];

	args[0] = client->name;
	args[1] = client->name;
	args[2] = umode;
	args[3] = NULL;

	dontspread = 1;
	do_cmd(client, NULL, "MODE", 3, args);
	dontspread = 0;
}

/** Remote client (already fully registered) changing their nick */
CMD_FUNC(cmd_nick_remote)
{
	TKL *tklban;
	int ishold;
	Client *acptr;
	char nick[NICKLEN + 2];
	char oldnick[NICKLEN + 1];
	time_t lastnick = 0;
	int differ = 1;
	unsigned char removemoder = (client->umodes & UMODE_REGNICK) ? 1 : 0;
	MessageTag *mtags = NULL;

	/* 'client' is always the fully registered user doing the nick change */

	strlcpy(nick, parv[1], NICKLEN + 1);
	strlcpy(oldnick, client->name, sizeof(oldnick));

	if (parc > 2)
		lastnick = atol(parv[2]);

	if (!do_remote_nick_name(nick) || !strcasecmp("ircd", nick) || !strcasecmp("irc", nick))
	{
		ircstats.is_kill++;
		unreal_log(ULOG_ERROR, "nick", "BAD_NICK_REMOTE", client,
		           "Server link $server tried to change '$client' to bad nick '$nick' -- rejected.",
		           log_data_string("nick", parv[1]),
		           log_data_client("server", client->uplink));
		mtags = NULL;
		new_message(client, NULL, &mtags);
		sendto_one(client, mtags, ":%s KILL %s :Illegal nick name", me.id, client->id);
		SetKilled(client);
		exit_client(client, mtags, "Illegal nick name");
		free_message_tags(mtags);
		mtags = NULL;
		return;
	}

	/* Check Q-lines / ban nick */
	if (!IsULine(client) && (tklban = find_qline(client, nick, &ishold)) && !ishold)
	{
		unreal_log(ULOG_INFO, "nick", "QLINE_NICK_REMOTE", client,
			   "Banned nick $nick [$ip] from server $server ($reason)",
			   log_data_string("nick", parv[1]),
			   log_data_string("ip", GetIP(client)),
			   log_data_client("server", client->uplink),
			   log_data_string("reason", tklban->ptr.nameban->reason));
		/* Let it through */
	}

	if ((acptr = find_client(nick, NULL)))
	{
		/* If existing nick is still in handshake, kill it */
		if (IsUnknown(acptr) && MyConnect(acptr))
		{
			SetKilled(acptr);
			exit_client(acptr, NULL, "Overridden");
		} else
		if (acptr == client)
		{
			/* 100% identical? Must be a bug, but ok */
			if (!strcmp(acptr->name, nick))
				return;
			/* Allows change of case in their nick */
			removemoder = 0; /* don't set the user -r */
		} else
		{
			/*
			   ** A NICK change has collided (e.g. message type ":old NICK new").
			 */
			differ = (mycmp(acptr->user->username, client->user->username) ||
			          mycmp(acptr->user->realhost, client->user->realhost));

			if (!(parc > 2) || lastnick == acptr->lastnick)
			{
				nick_collision(client, parv[1], client->id, client, acptr, NICKCOL_EQUAL);
				return; /* Now that I killed them both, ignore the NICK */
			} else
			if ((differ && (acptr->lastnick > lastnick)) ||
			    (!differ && (acptr->lastnick < lastnick)))
			{
				nick_collision(client, parv[1], client->id, client, acptr, NICKCOL_NEW_WON);
				/* fallthrough: their user won, continue and proceed with the nick change */
			} else
			if ((differ && (acptr->lastnick < lastnick)) ||
			    (!differ && (acptr->lastnick > lastnick)))
			{
				nick_collision(client, parv[1], client->id, client, acptr, NICKCOL_EXISTING_WON);
				return; /* their user lost, ignore the NICK */
			} else
			{
				return;		/* just in case */
			}
		}
	}

	mtags = NULL;

	if (!IsULine(client))
	{
		unreal_log(ULOG_INFO, "nick", "REMOTE_NICK_CHANGE", client,
		           "Client $client.details has changed their nickname to $new_nick",
		           log_data_string("new_nick", nick));
	}

	new_message(client, recv_mtags, &mtags);
	RunHook(HOOKTYPE_REMOTE_NICKCHANGE, client, mtags, nick);
	client->lastnick = lastnick ? lastnick : TStime();
	add_history(client, 1);
	sendto_server(client, 0, 0, mtags, ":%s NICK %s %lld",
	    client->id, nick, (long long)client->lastnick);
	sendto_local_common_channels(client, client, 0, mtags, ":%s NICK :%s", client->name, nick);
	if (removemoder)
		client->umodes &= ~UMODE_REGNICK;

	/* Finally set new nick name. */
	del_from_client_hash_table(client->name, client);
	strlcpy(client->name, nick, sizeof(client->name));
	add_to_client_hash_table(nick, client);

	RunHook(HOOKTYPE_POST_REMOTE_NICKCHANGE, client, mtags, oldnick);
	free_message_tags(mtags);
}

/* Local user: either setting their nick for the first time (registration)
 * or changing their nick (fully registered already, or not)
 */
CMD_FUNC(cmd_nick_local)
{
	TKL *tklban;
	int ishold;
	Client *acptr;
	char nick[NICKLEN + 2];
	char oldnick[NICKLEN + 1];
	char descbuf[BUFSIZE];
	Membership *mp;
	int newuser = 0;
	unsigned char removemoder = (client->umodes & UMODE_REGNICK) ? 1 : 0;
	Hook *h;
	int ret;

	strlcpy(oldnick, client->name, sizeof(oldnick));

	/* Enforce minimum nick length */
	if (iConf.min_nick_length && !IsOper(client) && !IsULine(client) && strlen(parv[1]) < iConf.min_nick_length)
	{
		snprintf(descbuf, sizeof descbuf, "A minimum length of %d chars is required", iConf.min_nick_length);
		sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], descbuf);
		return;
	}

	/* Enforce maximum nick length */
	strlcpy(nick, parv[1], iConf.nick_length + 1);

	/* Check if this is a valid nick name */
	if (!do_nick_name(nick))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], "Illegal characters");
		return;
	}

	/* Check for collisions / in use */
	if (!strcasecmp("ircd", nick) || !strcasecmp("irc", nick))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, "Reserved for internal IRCd purposes");
		return;
	}

	if (MyUser(client))
	{
		/* Local client changing nick: check spamfilter */
		spamfilter_build_user_string(spamfilter_user, nick, client);
		if (match_spamfilter(client, spamfilter_user, SPAMF_USER, "NICK", NULL, 0, NULL))
			return;
	}

	/* Check Q-lines / ban nick */
	if (!IsULine(client) && (tklban = find_qline(client, nick, &ishold)))
	{
		if (ishold)
		{
			sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, tklban->ptr.nameban->reason);
			return;
		}
		if (!ValidatePermissionsForPath("immune:server-ban:ban-nick",client,NULL,NULL,nick))
		{
			add_fake_lag(client, 4000); /* lag them up */
			sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, tklban->ptr.nameban->reason);
			unreal_log(ULOG_INFO, "nick", "QLINE_NICK_LOCAL_ATTEMPT", client,
				   "Attempt to use banned nick $nick [$ip] blocked ($reason)",
				   log_data_string("nick", parv[1]),
				   log_data_string("ip", GetIP(client)),
				   log_data_client("server", client->uplink),
				   log_data_string("reason", tklban->ptr.nameban->reason));
			return;	/* NICK message ignored */
		}
		/* fallthrough for ircops that have sufficient privileges */
	}

	if (!ValidatePermissionsForPath("immune:nick-flood",client,NULL,NULL,NULL))
		add_fake_lag(client, 3000);

	if ((acptr = find_client(nick, NULL)))
	{
		/* Shouldn't be possible since dot is disallowed: */
		if (IsServer(acptr))
		{
			sendnumeric(client, ERR_NICKNAMEINUSE, nick);
			return;
		}
		if (acptr == client)
		{
			/* New nick is exactly the same as the old nick? */
			if (!strcmp(acptr->name, nick))
				return;
			/* Changing cAsE */
			removemoder = 0;
		} else
		/* Collision with a nick of a session that is still in handshake */
		if (IsUnknown(acptr) && MyConnect(acptr))
		{
			/* Kill the other connection that is still in progress */
			SetKilled(acptr);
			exit_client(acptr, NULL, "Overridden");
		} else
		{
			sendnumeric(client, ERR_NICKNAMEINUSE, nick);
			return;	/* NICK message ignored */
		}
	}

	/* set::anti-flood::nick-flood */
	if (client->user &&
	    !ValidatePermissionsForPath("immune:nick-flood",client,NULL,NULL,NULL) &&
	    flood_limit_exceeded(client, FLD_NICK))
	{
		/* Throttle... */
		sendnumeric(client, ERR_NCHANGETOOFAST, nick);
		return;
	}

	/* New local client? */
	if (!client->name[0])
	{
		newuser = 1;

		if (iConf.ping_cookie)
		{
			/*
			 * Client setting NICK the first time.
			 *
			 * Generate a random string for them to pong with.
			 */
			client->local->nospoof = getrandom32();

			sendto_one(client, NULL, "PING :%X", client->local->nospoof);
		}

		/* Copy password to the passwd field if it's given after NICK */
		if ((parc > 2) && (strlen(parv[2]) <= PASSWDLEN))
			safe_strdup(client->local->passwd, parv[2]);

		/* This had to be copied here to avoid problems.. */
		strlcpy(client->name, nick, sizeof(client->name));

		/* Let's see if we can get online now... */
		if (is_handshake_finished(client))
		{
			/* Send a CTCP VERSION */
			if (!iConf.ping_cookie && USE_BAN_VERSION && MyConnect(client))
				sendto_one(client, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1", me.name, nick);

			client->lastnick = TStime();
			if (!register_user(client))
			{
				if (IsDead(client))
					return;
				/* ..otherwise.. fallthrough so we run the same code
				 * as in case of !is_handshake_finished()
				 */
			} else {
				/* New user! */
				strlcpy(nick, client->name, sizeof(nick)); /* don't ask, but I need this. do not remove! -- Syzop */
			}
		}
	} else
	if (MyUser(client))
	{
		MessageTag *mtags = NULL;
		int ret;

		/* Existing client nick-changing */

		/*
		   ** If the client belongs to me, then check to see
		   ** if client is currently on any channels where it
		   ** is currently banned.  If so, do not allow the nick
		   ** change to occur.
		   ** Also set 'lastnick' to current time, if changed.
		 */
		for (mp = client->user->channel; mp; mp = mp->next)
		{
			int ret = HOOK_CONTINUE;
			Hook *h;
			if (!check_channel_access(client, mp->channel, "hoaq") && is_banned(client, mp->channel, BANCHK_NICK, NULL, NULL))
			{
				sendnumeric(client, ERR_BANNICKCHANGE,
				    mp->channel->name);
				return;
			}
			if (CHECK_TARGET_NICK_BANS && !check_channel_access(client, mp->channel, "hoaq") && is_banned_with_nick(client, mp->channel, BANCHK_NICK, nick, NULL, NULL))
			{
				sendnumeric(client, ERR_BANNICKCHANGE, mp->channel->name);
				return;
			}

			for (h = Hooks[HOOKTYPE_CHAN_PERMIT_NICK_CHANGE]; h; h = h->next)
			{
				ret = (*(h->func.intfunc))(client,mp->channel);
				if (ret != HOOK_CONTINUE)
					break;
			}

			if (ret == HOOK_DENY)
			{
				sendnumeric(client, ERR_NONICKCHANGE, mp->channel->name);
				return;
			}
		}

		unreal_log(ULOG_INFO, "nick", "LOCAL_NICK_CHANGE", client,
		           "Client $client.details has changed their nickname to $new_nick",
		           log_data_string("new_nick", nick));

		new_message(client, recv_mtags, &mtags);
		RunHook(HOOKTYPE_LOCAL_NICKCHANGE, client, mtags, nick);
		client->lastnick = TStime();
		add_history(client, 1);
		sendto_server(client, 0, 0, mtags, ":%s NICK %s %lld",
		    client->id, nick, (long long)client->lastnick);
		sendto_local_common_channels(client, client, 0, mtags, ":%s NICK :%s", client->name, nick);
		sendto_one(client, mtags, ":%s NICK :%s", client->name, nick);
		free_message_tags(mtags);
		if (removemoder)
			client->umodes &= ~UMODE_REGNICK;
	} else
	{
		/* Someone changing nicks in the pre-registered phase */
	}

	del_from_client_hash_table(client->name, client);

	strlcpy(client->name, nick, sizeof(client->name));
	add_to_client_hash_table(nick, client);

	/* update fdlist --nenolod */
	snprintf(descbuf, sizeof(descbuf), "Client: %s", nick);
	fd_desc(client->local->fd, descbuf);

	if (removemoder && MyUser(client))
		sendto_one(client, NULL, ":%s MODE %s :-r", me.name, client->name);

	if (MyUser(client) && !newuser)
		RunHook(HOOKTYPE_POST_LOCAL_NICKCHANGE, client, recv_mtags, oldnick);
}

/*
** cmd_uid
**	parv[1] = nickname
**      parv[2] = hopcount
**      parv[3] = timestamp
**      parv[4] = username
**      parv[5] = hostname
**      parv[6] = UID
**	parv[7] = account name (SVID)
**      parv[8] = umodes
**	parv[9] = virthost, * if none
**	parv[10] = cloaked host, * if none
**	parv[11] = ip
**	parv[12] = info
**
** Technical documentation is available at:
** https://www.unrealircd.org/docs/Server_protocol:UID_command
*/
CMD_FUNC(cmd_uid)
{
	TKL *tklban;
	int ishold;
	Client *acptr, *serv = NULL;
	Client *acptrs;
	char nick[NICKLEN + 1];
	char buf[BUFSIZE];
	long lastnick = 0;
	int differ = 1;
	const char *hostname, *username, *sstamp, *umodes, *virthost, *ip_raw, *realname;
	const char *ip = NULL;

	if (parc < 13)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "UID");
		return;
	}

	/* It's not just the directly attached client which must be a
	 * server. The source itself needs to be a server.
	 */
	if (!IsServer(client))
	{
		sendnumeric(client, ERR_NOTFORUSERS, "UID");
		return;
	}

	strlcpy(nick, parv[1], sizeof(nick));
	hostname = parv[5];
	sstamp = parv[7];
	username = parv[4];
	umodes = parv[8];
	virthost = parv[9];
	ip_raw = parv[11];
	realname = parv[12];

	/* Do some *MINIMAL* nick name checking for remote nicknames.
	 * This will only catch things that severely break things. -- Syzop
	 */
	if (!do_remote_nick_name(nick) || !strcasecmp("ircd", nick) || !strcasecmp("irc", nick))
	{
		unreal_log(ULOG_ERROR, "nick", "BAD_NICK_REMOTE", client->uplink,
		           "Server link $client tried to introduce bad nick '$nick' -- rejected.",
		           log_data_string("nick", parv[1]));
		sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], "Illegal nick name");

		ircstats.is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :Bad nick", me.id, parv[1]);
		return;
	}

	if (!valid_uid(parv[6]) || strncmp(parv[6], client->id, 3))
	{
		ircstats.is_kill++;
		unreal_log(ULOG_ERROR, "link", "BAD_UID", client,
		           "Server link $client ($sid) used bad UID $uid in UID command.",
		           log_data_string("sid", client->id),
		           log_data_string("uid", parv[6]));
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :Bad UID", me.id, parv[6]);
		return;
	}

	if (!valid_host(hostname, 0))
	{
		ircstats.is_kill++;
		unreal_log(ULOG_ERROR, "link", "BAD_HOSTNAME", client,
		           "Server link $client ($client.id) introduced user $nick with bad host name: $bad_hostname.",
		           log_data_string("nick", nick),
		           log_data_string("bad_hostname", hostname));
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :Bad hostname", me.id, parv[6]);
		return;
	}

	if (strcmp(virthost, "*") && !valid_host(virthost, 0))
	{
		ircstats.is_kill++;
		unreal_log(ULOG_ERROR, "link", "BAD_HOSTNAME", client,
		           "Server link $client ($client.id) introduced user $nick with bad virtual hostname: $bad_hostname.",
		           log_data_string("nick", nick),
		           log_data_string("bad_hostname", virthost));
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :Bad virtual host", me.id, parv[6]);
		return;
	}

	if (strcmp(ip_raw, "*"))
	{
		if (!(ip = decode_ip(ip_raw)))
		{
			ircstats.is_kill++;
			unreal_log(ULOG_ERROR, "link", "BAD_IP", client,
				   "Server link $client ($client.id) introduced user $nick with bad IP: $bad_ip.",
				   log_data_string("nick", nick),
				   log_data_string("bad_ip", ip_raw));
			/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
			sendto_one(client, NULL, ":%s KILL %s :Bad IP in UID command", me.id, parv[6]);
			return;
		}
	}

	/* Kill quarantined opers early... */
	if (IsQuarantined(client->direction) && strchr(parv[8], 'o'))
	{
		ircstats.is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		unreal_log(ULOG_INFO, "link", "OPER_KILLED_QUARANTINE", NULL,
		           "QUARANTINE: Oper $nick on server $server killed, due to quarantine",
		           log_data_string("nick", parv[1]),
		           log_data_client("server", client));
		sendto_one(client, NULL, ":%s KILL %s :Quarantined: no oper privileges allowed", me.id, parv[6]);
		return;
	}

	if (!IsULine(client) && (tklban = find_qline(client, nick, &ishold)))
	{
		unreal_log(ULOG_INFO, "nick", "QLINE_NICK_REMOTE", client,
			   "Banned nick $nick [$nick.ip] from server $server ($reason)",
			   log_data_string("nick", parv[1]),
			   log_data_string("ip", ip),
			   log_data_client("server", client->uplink),
			   log_data_string("reason", tklban->ptr.nameban->reason));
		/* Let it through */
	}

	/* Now check if 'nick' already exists - collision with a user (or still in handshake, unknown) */
	if ((acptr = find_client(nick, NULL)) != NULL)
	{
		/* If there's a collision with a user that is still in handshake, on our side,
		 * then we can just kill our client and continue.
		 */
		if (MyConnect(acptr) && IsUnknown(acptr))
		{
			SetKilled(acptr);
			exit_client(acptr, NULL, "Overridden");
			goto nickkill2done;
		}

		lastnick = atol(parv[3]);
		differ = (mycmp(acptr->user->username, parv[4]) || mycmp(acptr->user->realhost, parv[5]));

		if (acptr->lastnick == lastnick)
		{
			nick_collision(client, parv[1], parv[6], NULL, acptr, NICKCOL_EQUAL);
			return;	/* We killed both users, now stop the process. */
		}

		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)) || acptr->direction == client->direction)	/* we missed a QUIT somewhere ? */
		{
			nick_collision(client, parv[1], parv[6], NULL, acptr, NICKCOL_NEW_WON);
			/* We got rid of the "wrong" user. Introduce the correct one. */
			/* ^^ hmm.. why was this absent in nenolod's code, resulting in a 'return 0'? seems wrong. */
			goto nickkill2done;
		}

		if ((differ && (acptr->lastnick < lastnick)) || (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(client, parv[1], parv[6], NULL, acptr, NICKCOL_EXISTING_WON);
			return;	/* Ignore the NICK */
		}
		return; /* just in case */
	}

nickkill2done:
	/* Proceed with introducing the new client, change source (replace client) */

	serv = client;
	client = make_client(serv->direction, serv);
	strlcpy(client->id, parv[6], IDLEN);
	add_client_to_list(client);
	add_to_id_hash_table(client->id, client);
	client->lastnick = atol(parv[3]);
	strlcpy(client->name, nick, NICKLEN+1);
	add_to_client_hash_table(nick, client);

	make_user(client);

	/* Note that cloaked host aka parv[10] is unused */

	client->user->server = find_or_add(client->uplink->name);
	strlcpy(client->user->realhost, hostname, sizeof(client->user->realhost));
	if (ip)
		safe_strdup(client->ip, ip);

	if (*sstamp != '*')
		strlcpy(client->user->account, sstamp, sizeof(client->user->account));

	strlcpy(client->info, realname, sizeof(client->info));
	strlcpy(client->user->username, username, USERLEN + 1);
	SetUser(client);

	make_cloakedhost(client, client->user->realhost, client->user->cloakedhost, sizeof(client->user->cloakedhost));
	safe_strdup(client->user->virthost, client->user->cloakedhost);

	/* Inherit flags from server, makes it easy in the send routines
	 * and this also makes clients inherit ulines.
	 */
	client->flags |= client->uplink->flags;

	/* Update counts */
	irccounts.clients++;
	if (client->uplink->server)
		client->uplink->server->users++;
	if (client->umodes & UMODE_INVISIBLE)
		irccounts.invisible++;

	/* Set user modes */
	set_user_modes_dont_spread(client, umodes);

	/* Set the vhost */
	if (*virthost != '*')
		safe_strdup(client->user->virthost, virthost);

	build_umode_string(client, 0, SEND_UMODES|UMODE_SERVNOTICE, buf);

	sendto_serv_butone_nickcmd(client->direction, recv_mtags, client, (*buf == '\0' ? "+" : buf));

	moddata_extract_s2s_mtags(client, recv_mtags);

	if (IsLoggedIn(client))
	{
		user_account_login(recv_mtags, client);
		/* no need to check for kill upon user_account_login() here
		 * since that can only happen for local users.
		 */
	}

	RunHook(HOOKTYPE_REMOTE_CONNECT, client);

	if (!IsULine(serv) && IsSynched(serv))
	{
		unreal_log(ULOG_INFO, "connect", "REMOTE_CLIENT_CONNECT", client,
			   "Client connecting: $client ($client.user.username@$client.hostname) [$client.ip] $extended_client_info",
			   log_data_string("extended_client_info", get_connect_extinfo(client)),
		           log_data_string("from_server_name", client->user->server));
	}
}

/** The NICK command.
 * In UnrealIRCd 4 and later this should only happen for:
 * 1) A local user setting or changing the nick name ("NICK xyz")
 *    -> cmd_nick_local()
 * 2) A remote user changing their nick name (":<uid> NICK <newnick>")
 *    -> cmd_nick_remote()
 */
CMD_FUNC(cmd_nick)
{
	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NONICKNAMEGIVEN);
		return;
	}

	if (MyConnect(client) && !IsServer(client))
	{
		CALL_CMD_FUNC(cmd_nick_local);
	} else
	if (!IsUser(client))
	{
		unreal_log(ULOG_ERROR, "link", "LINK_OLD_PROTOCOL_NICK", client->direction,
		           "Server link $client tried to introduce $nick using NICK command. "
		           "Server is using an old and unsupported protocol from UnrealIRCd 3.2.x or earlier, should use the UID command. "
		           "See https://www.unrealircd.org/docs/FAQ#old-server-protocol",
		           log_data_string("nick", parv[1]));
		/* Split the entire uplink, as it should never have allowed this (and probably they are to blame too) */
		exit_client(client->direction, NULL, "Server used NICK command, bad, must use UID!");
		return;
	} else
	{
		CALL_CMD_FUNC(cmd_nick_remote);
	}
}

/** Welcome the user on IRC.
 * Send the RPL_WELCOME, LUSERS, MOTD, auto join channels, everything...
 */
void welcome_user(Client *client, TKL *viruschan_tkl)
{
	int i;
	ConfigItem_tld *tld;
	char buf[BUFSIZE];

	/* Make creation time the real 'online since' time, excluding registration time.
	 * Otherwise things like set::anti-spam-quit-messagetime 10s could mean
	 * 1 second in practice (#2174).
	 */
	client->local->creationtime = TStime();
	client->local->idle_since = TStime();

	RunHook(HOOKTYPE_WELCOME, client, 0);
	sendnumeric(client, RPL_WELCOME, NETWORK_NAME, client->name, client->user->username, client->user->realhost);

	RunHook(HOOKTYPE_WELCOME, client, 1);
	sendnumeric(client, RPL_YOURHOST, me.name, version);

	RunHook(HOOKTYPE_WELCOME, client, 2);
	sendnumeric(client, RPL_CREATED, creation);

	RunHook(HOOKTYPE_WELCOME, client, 3);
	sendnumeric(client, RPL_MYINFO, me.name, version, umodestring, cmodestring);

	RunHook(HOOKTYPE_WELCOME, client, 4);
	for (i = 0; ISupportStrings[i]; i++)
		sendnumeric(client, RPL_ISUPPORT, ISupportStrings[i]);

	RunHook(HOOKTYPE_WELCOME, client, 5);

	if (IsHidden(client))
	{
		sendnumeric(client, RPL_HOSTHIDDEN, client->user->virthost);
		RunHook(HOOKTYPE_WELCOME, client, 396);
	}

	if (IsSecureConnect(client))
	{
		if (client->local->ssl && !iConf.no_connect_tls_info)
		{
			sendnotice(client, "*** You are connected to %s with %s",
				me.name, tls_get_cipher(client));
		}
	}

	{
		const char *parv[2];
		parv[0] = NULL;
		parv[1] = NULL;
		do_cmd(client, NULL, "LUSERS", 1, parv);
		if (IsDead(client))
			return;
	}

	RunHook(HOOKTYPE_WELCOME, client, 266);

	short_motd(client);

	RunHook(HOOKTYPE_WELCOME, client, 376);

#ifdef EXPERIMENTAL
	sendnotice(client,
		"*** \2NOTE:\2 This server is running experimental IRC server software (UnrealIRCd %s). "
		"If you find any bugs or problems, please report them at https://bugs.unrealircd.org/",
		VERSIONONLY);
#endif

	if (client->umodes & UMODE_INVISIBLE)
		irccounts.invisible++;

	build_umode_string(client, 0, SEND_UMODES|UMODE_SERVNOTICE, buf);

	sendto_serv_butone_nickcmd(client->direction, NULL, client, (*buf == '\0' ? "+" : buf));

	broadcast_moddata_client(client);

	if (buf[0] != '\0' && buf[1] != '\0')
		sendto_one(client, NULL, ":%s MODE %s :%s", client->name,
		    client->name, buf);

	if (client->user->snomask)
		sendnumeric(client, RPL_SNOMASK, client->user->snomask);

	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_user == POLICY_WARN))
		sendnotice_multiline(client, iConf.plaintext_policy_user_message);

	if (IsSecure(client) && (iConf.outdated_tls_policy_user == POLICY_WARN) && outdated_tls_client(client))
		sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_user_message, client));

	RunHook(HOOKTYPE_LOCAL_CONNECT, client);

	/* Give the user a fresh start as far as fake-lag is concerned.
	 * Otherwise the user could be lagged up already due to all the CAP stuff.
	 */
	client->local->fake_lag = TStime();

	RunHook(HOOKTYPE_WELCOME, client, 999);

	/* NOTE: Code after this 'if (viruschan_tkl)' will not be executed for quarantined-
	 *       virus-users. So be carefull with the order. -- Syzop
	 */
	// FIXME: verify if this works, trace code path upstream!!!!
	if (viruschan_tkl)
	{
		join_viruschan(client, viruschan_tkl, SPAMF_USER);
		return;
	}

	/* Force the user to join the given chans -- codemastr */
	tld = find_tld(client);

	if (tld && !BadPtr(tld->channel))
	{
		char *chans = strdup(tld->channel);
		const char *args[3] = {
			NULL,
			chans,
			NULL
		};
		do_cmd(client, NULL, "JOIN", 3, args);
		safe_free(chans);
		if (IsDead(client))
			return;
	}
	else if (!BadPtr(AUTO_JOIN_CHANS) && strcmp(AUTO_JOIN_CHANS, "0"))
	{
		char *chans = strdup(AUTO_JOIN_CHANS);
		const char *args[3] = {
			NULL,
			chans,
			NULL
		};
		do_cmd(client, NULL, "JOIN", 3, args);
		safe_free(chans);
		if (IsDead(client))
			return;
	}
}

/** Make a valid client->user->username, or try to anyway.
 * @param client	The client to check
 * @param noident	Whether we should ignore the first ~ or not
 * @returns 1 if the username is acceptable, 0 if not.
 * @note This function will modify client->user->username to make it valid.
 *       Only if there are zero valid characters it will return 0.
 * @note There is also valid_username() in src/misc.c
 */
int make_valid_username(Client *client, int noident)
{
	static char stripuser[USERLEN + 1];
	char *i;
	char *o = stripuser;
	char filtered = 0; /* any changes? */

	*stripuser = '\0';

	for (i = client->user->username + noident; *i; i++)
	{
		if (isallowed(*i))
			*o++ = *i;
		else
			filtered = 1;
	}
	*o = '\0';

	if (filtered == 0)
		return 1; /* No change needed, all good */

	if (*stripuser == '\0')
		return 0; /* Zero valid characters, reject it */

	strlcpy(client->user->username + 1, stripuser, sizeof(client->user->username)-1);
	client->user->username[0] = '~';
	client->user->username[USERLEN] = '\0';
	return 1; /* Filtered, but OK */
}

/** Register the connection as a User - only for local connections!
 * This is called after NICK + USER (in no particular order)
 * and possibly other protocol messages as well (eg CAP).
 * @param client	Client to be made a user.
 * @returns 1 if successfully registered, 0 if not (client might be killed).
 */
int _register_user(Client *client)
{
	ConfigItem_ban *bconf;
	char *tmpstr;
	char noident = 0;
	int i;
	Hook *h;
	TKL *savetkl = NULL;
	char temp[USERLEN + 1];
	char descbuf[BUFSIZE];

	if (!MyConnect(client))
		abort();

	/* Set client->local->sockhost:
	 * First deal with the special 'localhost' case and
	 * then with generic setting based on DNS.
	 */
	if (!strcmp(GetIP(client), "127.0.0.1") ||
	    !strcmp(GetIP(client), "0:0:0:0:0:0:0:1") ||
	    !strcmp(GetIP(client), "0:0:0:0:0:ffff:127.0.0.1"))
	{
		set_sockhost(client, "localhost");
		if (client->local->hostp)
		{
			unreal_free_hostent(client->local->hostp);
			client->local->hostp = NULL;
		}
	} else
	{
		struct hostent *hp = client->local->hostp;
		if (hp && hp->h_name)
			set_sockhost(client, hp->h_name);
	}

	/* Set the hostname (client->user->realhost).
	 * This may later be overwritten by the AllowClient() call to
	 * revert to the IP again if allow::options::useip is set.
	 */
	strlcpy(client->user->realhost, client->local->sockhost, sizeof(client->local->sockhost));

	/* Check allow { } blocks... */
	if (!AllowClient(client))
	{
		ircstats.is_ref++;
		/* For safety, we have an extra kill here */
		if (!IsDead(client))
			exit_client(client, NULL, "Rejected");
		return 0;
	}

	if (IsUseIdent(client))
	{
		if (IsIdentSuccess(client))
		{
			/* ident succeeded: overwite client->user->username with the ident reply */
			strlcpy(client->user->username, client->ident, sizeof(client->user->username));
		} else
		if (IDENT_CHECK)
		{
			/* ident check is enabled and it failed: prefix the username with ~ */
			char temp[USERLEN+1];
			strlcpy(temp, client->user->username, sizeof(temp));
			snprintf(client->user->username, sizeof(client->user->username), "~%s", temp);
			noident = 1;
		}
	}

	/* Now validate the username. This may alter client->user->username
	 * or reject it completely.
	 */
	if (!make_valid_username(client, noident))
	{
		exit_client(client, NULL, "Hostile username. Please use only 0-9 a-z A-Z _ - and . in your username.");
		return 0;
	}

	/* Check ban realname { } blocks */
	if ((bconf = find_ban(NULL, client->info, CONF_BAN_REALNAME)))
	{
		ircstats.is_ref++;
		banned_client(client, "realname", bconf->reason?bconf->reason:"", 0, 0);
		return 0;
	}
	/* Check G/Z lines before shuns -- kill before quite -- codemastr */
	if (find_tkline_match(client, 0))
	{
		if (!IsDead(client) && client->local->class)
		{
			/* Fix client count bug, in case that it was a hold such as via authprompt */
			client->local->class->clients--;
			client->local->class = NULL;
		}
		ircstats.is_ref++;
		return 0;
	}
	find_shun(client);

	spamfilter_build_user_string(spamfilter_user, client->name, client);
	if (match_spamfilter(client, spamfilter_user, SPAMF_USER, NULL, NULL, 0, &savetkl))
	{
		if (savetkl && ((savetkl->ptr.spamfilter->action == BAN_ACT_VIRUSCHAN) ||
				(savetkl->ptr.spamfilter->action == BAN_ACT_SOFT_VIRUSCHAN)))
		{
			/* 'viruschan' action:
			 * Continue with registering the client, and at the end
			 * of this function we will do the actual joining to the
			 * virus channel.
			 */
		} else {
			/* Client is either dead or blocked (will hang, on purpose, and timeout) */
			return 0;
		}
	}

	for (h = Hooks[HOOKTYPE_PRE_LOCAL_CONNECT]; h; h = h->next)
	{
		int ret = (*(h->func.intfunc))(client);
		if (ret == HOOK_DENY)
		{
			if (!IsDead(client) && client->local->class)
			{
				/* Fix client count bug, in case that
				 * the HOOK_DENY was only meant temporarily.
				 */
				client->local->class->clients--;
				client->local->class = NULL;
			}
			return 0;
		}
		if (ret == HOOK_ALLOW)
			break;
	}

	SetUser(client);

	make_cloakedhost(client, client->user->realhost, client->user->cloakedhost, sizeof(client->user->cloakedhost));

	/* client->user->virthost should never be empty */
	if (!IsSetHost(client) || !client->user->virthost)
		safe_strdup(client->user->virthost, client->user->cloakedhost);

	snprintf(descbuf, sizeof descbuf, "Client: %s", client->name);
	fd_desc(client->local->fd, descbuf);

	/* Move user from unknown list to client list */
	list_move(&client->lclient_node, &lclient_list);

	/* Update counts */
	irccounts.unknown--;
	irccounts.clients++;
	irccounts.me_clients++;
	if (client->uplink && client->uplink->server)
		client->uplink->server->users++;

	if (IsSecure(client))
	{
		client->umodes |= UMODE_SECURE;
		RunHook(HOOKTYPE_SECURE_CONNECT, client);
	}

	safe_free(client->local->passwd);

	unreal_log(ULOG_INFO, "connect", "LOCAL_CLIENT_CONNECT", client,
		   "Client connecting: $client ($client.user.username@$client.hostname) [$client.ip] $extended_client_info",
		   log_data_string("extended_client_info", get_connect_extinfo(client)));

	/* Send the RPL_WELCOME, LUSERS, MOTD, auto join channels, everything... */
	welcome_user(client, savetkl);

	return IsDead(client) ? 0 : 1;
}

/** Nick collission detected. A winner has been decided upstream. Deal with killing.
 * I moved this all to a single routine here rather than having all code duplicated
 * due to SID vs NICK and some code quadruplicated.
 */
void nick_collision(Client *cptr, const char *newnick, const char *newid, Client *new, Client *existing, int type)
{
	char comment[512];
	const char *new_server, *existing_server;
	const char *who_won;
	const char *nickcol_reason;

	if (type == NICKCOL_NEW_WON)
		who_won = "new";
	else if (type == NICKCOL_EXISTING_WON)
		who_won = "existing";
	else
		who_won = "none";

	nickcol_reason = new ? "nick change" : "new user connecting";

	unreal_log(ULOG_ERROR, "nick", "NICK_COLLISION", NULL,
	           "Nick collision: "
	           "$new_nick[$new_id]@$uplink (new) vs "
	           "$existing_client[$existing_client.id]@$existing_client.user.servername (existing). "
	           "Winner: $nick_collision_winner. "
	           "Cause: $nick_collision_reason",
	           log_data_string("new_nick", newnick),
	           log_data_string("new_id", newid),
	           log_data_client("uplink", cptr),
	           log_data_client("existing_client", existing),
	           log_data_string("nick_collision_winner", who_won),
	           log_data_string("nick_collision_reason", nickcol_reason));

	new_server = cptr->name;
	existing_server = (existing == existing->direction) ? me.name : existing->direction->name;
	if (type == NICKCOL_EXISTING_WON)
		snprintf(comment, sizeof(comment), "Nick collision: %s <- %s", new_server, existing_server);
	else if (type == NICKCOL_NEW_WON)
		snprintf(comment, sizeof(comment), "Nick collision: %s <- %s", existing_server, new_server);
	else
		snprintf(comment, sizeof(comment), "Nick collision: %s <-> %s", existing_server, new_server);

	/* We only care about the direction from this point, not about the originally sending server */
	cptr = cptr->direction;

	if ((type == NICKCOL_EQUAL) || (type == NICKCOL_EXISTING_WON))
	{
		/* Kill 'new':
		 * - 'new' is known by the cptr-side as 'newnick' already
		 * - if not nick-changing then the other servers don't know this user
		 * - if nick-changing, then the the other servers know the user as new->name
		 */

		/* cptr case first... this side knows the user by newnick/newid */
		/* SID server can kill 'new' by ID */
		sendto_one(cptr, NULL, ":%s KILL %s :%s", me.id, newid, comment);

		/* non-cptr case... only necessary if nick-changing. */
		if (new)
		{
			MessageTag *mtags = NULL;

			new_message(new, NULL, &mtags);

			/* non-cptr side knows this user by their old nick name */
			sendto_server(cptr, 0, 0, mtags, ":%s KILL %s :%s", me.id, new->id, comment);

			/* Exit the client */
			ircstats.is_kill++;
			SetKilled(new);
			exit_client(new, mtags, comment);

			free_message_tags(mtags);
		}
	}

	if ((type == NICKCOL_EQUAL) || (type == NICKCOL_NEW_WON))
	{
		MessageTag *mtags = NULL;

		new_message(existing, NULL, &mtags);

		/* Now let's kill 'existing' */
		sendto_server(NULL, 0, 0, mtags, ":%s KILL %s :%s", me.id, existing->id, comment);

		/* Exit the client */
		ircstats.is_kill++;
		SetKilled(existing);
		exit_client(existing, mtags, comment);

		free_message_tags(mtags);
	}
}

/** Returns 1 if allow::maxperip is exceeded by 'client' */
int exceeds_maxperip(Client *client, ConfigItem_allow *aconf)
{
	Client *acptr;
	int local_cnt = 1;
	int global_cnt = 1;

	if (find_tkl_exception(TKL_MAXPERIP, client))
		return 0; /* exempt */

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsUser(acptr) && !strcmp(GetIP(acptr), GetIP(client)))
		{
			if (MyUser(acptr))
			{
				local_cnt++;
				if (local_cnt > aconf->maxperip)
					return 1;
			}
			global_cnt++;
			if (global_cnt > aconf->global_maxperip)
				return 1;
		}
	}
	return 0;
}

/** Allow or reject the client based on allow { } blocks and all other restrictions.
 * @param client     Client to check (local)
 * @param username   Username, for some reason...
 * @returns 1 if OK, 0 if client is rejected (likely killed too)
 */
int AllowClient(Client *client)
{
	static char sockhost[HOSTLEN + 1];
	int i;
	ConfigItem_allow *aconf;
	char *hname;
	static char uhost[HOSTLEN + USERLEN + 3];
	static char fullname[HOSTLEN + 1];

	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_user == POLICY_DENY))
	{
		exit_client(client, NULL, iConf.plaintext_policy_user_message->line);
		return 0;
	}

	if (IsSecure(client) && (iConf.outdated_tls_policy_user == POLICY_DENY) && outdated_tls_client(client))
	{
		const char *msg = outdated_tls_client_build_string(iConf.outdated_tls_policy_user_message, client);
		exit_client(client, NULL, msg);
		return 0;
	}

	for (aconf = conf_allow; aconf; aconf = aconf->next)
	{
		if (aconf->flags.tls && !IsSecure(client))
			continue;

		if (!user_allowed_by_security_group(client, aconf->match))
			continue;

		/* Check authentication */
		if (aconf->auth && !Auth_Check(client, aconf->auth, client->local->passwd))
		{
			/* Incorrect password/authentication - but was is it required? */
			if (aconf->flags.reject_on_auth_failure)
			{
				exit_client(client, NULL, iConf.reject_message_unauthorized);
				return 0;
			} else {
				continue; /* Continue (this is the default behavior) */
			}
		}

		if (!aconf->flags.noident)
			SetUseIdent(client);

		if (aconf->flags.useip)
			set_sockhost(client, GetIP(client));

		if (exceeds_maxperip(client, aconf))
		{
			/* Already got too many with that ip# */
			exit_client(client, NULL, iConf.reject_message_too_many_connections);
			return 0;
		}

		if (!((aconf->class->clients + 1) > aconf->class->maxclients))
		{
			client->local->class = aconf->class;
			client->local->class->clients++;
		}
		else
		{
			/* Class is full */
			sendnumeric(client, RPL_REDIR, aconf->server ? aconf->server : DEFAULT_SERVER, aconf->port ? aconf->port : 6667);
			exit_client(client, NULL, iConf.reject_message_server_full);
			return 0;
		}
		return 1;
	}
	/* User did not match any allow { } blocks: */
	exit_client(client, NULL, iConf.reject_message_unauthorized);
	return 0;
}
