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
	"unrealircd-5",
    };

#define MSG_NICK 	"NICK"

/* Forward declarations */
CMD_FUNC(cmd_nick);
CMD_FUNC(cmd_uid);
int _register_user(Client *client, char *nick, char *username, char *umode, char *virthost, char *ip);
int AllowClient(Client *client, char *username);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_REGISTER_USER, _register_user);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_NICK, cmd_nick, MAXPARA, CMD_USER|CMD_SERVER|CMD_UNREGISTERED);
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

static char buf[BUFSIZE];
static char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64];

#define NICKCOL_EQUAL         0
#define NICKCOL_NEW_WON       1
#define NICKCOL_EXISTING_WON  2

/* Assume that on collision a NICK is in flight and the other server will take
 * the exact same decision we would do, and thus we don't send a KILL to cptr?
 * This works great with this code, seems to kill the correct person and not
 * cause desynchs even without UID/SID. HOWEVER.. who knows what code the other servers run?
 * Should use UID/SID anyway, then this whole problem doesn't exist.
 */
#define ASSUME_NICK_IN_FLIGHT

/** Nick collission detected. A winner has been decided upstream. Deal with killing.
 * I moved this all to a single routine here rather than having all code duplicated
 * due to SID vs NICK and some code quadruplicated.
 */
void nick_collision(Client *cptr, char *newnick, char *newid, Client *new, Client *existing, int type)
{
	char comment[512];
	char *new_server, *existing_server;

	ircd_log(LOG_ERROR, "Nick collision: %s[%s]@%s (new) vs %s[%s]@%s (existing). Winner: %s. Type: %s",
		newnick, newid ? newid : "", cptr->name,
		existing->name, existing->id, existing->srvptr->name,
		(type == NICKCOL_EQUAL) ? "None (equal)" : ((type == NICKCOL_NEW_WON) ? "New won" : "Existing won"),
		new ? "nick-change" : "new user connecting");

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
		if (!BadPtr(newid))
		{
			/* SID server can kill 'new' by ID */
			sendto_one(cptr, NULL, ":%s KILL %s :%s (%s)",
				me.name, newid, me.name, comment);
		} else {
			// FIXME: this never happens, right? after the old proto ripout...
#ifndef ASSUME_NICK_IN_FLIGHT
			/* cptr is not SID-capable or user has no UID */
			sendto_one(cptr, NULL, ":%s KILL %s :%s (%s)",
				me.name, newnick, me.name, comment);

			if (type == NICKCOL_EXISTING_WON)
				reintroduce_existing_user = 1; /* we may have killed 'existing' by the above, so.. */
#else
			/* Don't send a kill here since most likely this is a crossing
			 * NICK over the wire (in flight) so the other end will kill
			 * the nick already.
			 * We avoid sending the KILL because it may/will cause our
			 * own client to be killed, which then needs to be re-introduced.
			 */
#endif
		}

		/* non-cptr case... only necessary if nick-changing. */
		if (new)
		{
			MessageTag *mtags = NULL;

			new_message(new, NULL, &mtags);

			/* non-cptr side knows this user by their old nick name */
			sendto_server(cptr, 0, 0, mtags,
				":%s KILL %s :%s (%s)",
				me.name, ID(new), me.name, comment);

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
		sendto_server(NULL, 0, 0, mtags,
			":%s KILL %s :%s (%s)",
			me.name, ID(existing), me.name, comment);

		/* NOTE: we may have sent two KILLs on the same nick in some cases.
		 * Should be acceptable and only happens in a non-100% UID network.
		 */

		/* Exit the client */
		ircstats.is_kill++;
		SetKilled(existing);
		exit_client(existing, mtags, comment);

		free_message_tags(mtags);
	}

#ifndef ASSUME_NICK_IN_FLIGHT
	if (reintroduce_existing_user)
	{
		/* Due to non-SID capable or no UID for the user we were forced
		 * to send a KILL that could possibly match our own user.
		 * So we re-introduce the user here.
		 * This is not ideal but if we don't do this then 'cptr'-side would be desynced.
		 */
		introduce_user(cptr, existing);
		if (!SupportSJ3(cptr))
		{
			/* Wow. Old. */
			send_user_joins(cptr, existing);
			/* Didn't bother to do the +vhoaq stuff for this. Shouldn't we rip out SJOIN/SJOIN2 anyway? */
		} else
		{
			/* Hmmm duplicate code... hmmmmmm. taken from send_channel_modes_sjoin3. */
			Membership *lp;
			char flags[16], *p;

			for (lp = existing->user->channel; lp; lp = lp->next)
			{
				p = flags;
				if (lp->flags & MODE_CHANOP)
					*p++ = '@';
				if (lp->flags & MODE_VOICE)
					*p++ = '+';
				if (lp->flags & MODE_HALFOP)
					*p++ = '%';
				if (lp->flags & MODE_CHANOWNER)
					*p++ = '*';
				if (lp->flags & MODE_CHANADMIN)
					*p++ = '~';
				*p = '\0';

				sendto_one(cptr, NULL, ":%s SJOIN %ld %s + :%s%s",
					me.name, lp->channel->creationtime, lp->channel->chname,
					flags, existing->name);
			}
		}
		/* Could synch channel-member-data here. But this is apparently an old server anyway.. */
	}
#endif
}

/*
** cmd_uid
**	parv[1] = nickname
**      parv[2] = hopcount
**      parv[3] = timestamp
**      parv[4] = username
**      parv[5] = hostname
**      parv[6] = UID
**	parv[7] = servicestamp
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
	char nick[NICKLEN + 2]; // FIXME: why +2 and not +1?
	long lastnick = 0l;
	int differ = 1;

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

	strlcpy(nick, parv[1], NICKLEN + 1);

	/* Do some *MINIMAL* nick name checking for remote nicknames.
	 * This will only catch things that severely break things. -- Syzop
	 */
	if (!do_remote_nick_name(nick))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], "Illegal characters");

		ircstats.is_kill++;
		sendto_umode(UMODE_OPER, "Bad Nick: %s From: %s %s",
		    parv[1], client->name, get_client_name(client, FALSE));
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :%s (%s <- %s[%s])",
		    me.name, parv[1], me.name, parv[1],
		    nick, client->name);
		return;
	}

	/* Kill quarantined opers early... */
	if (IsQuarantined(client->direction) && strchr(parv[8], 'o'))
	{
		ircstats.is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(client, NULL, ":%s KILL %s :%s (Quarantined: no oper privileges allowed)",
			me.name, parv[1], me.name);
		sendto_realops("QUARANTINE: Oper %s on server %s killed, due to quarantine",
			parv[1], client->name);
		return;
	}

	/* This one is never allowed, even from remotes */
	if (!strcasecmp("ircd", nick) || !strcasecmp("irc", nick))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, "Reserved for internal IRCd purposes");
		sendto_one(client, NULL, ":%s KILL %s :%s (%s <- %s[%s])",
		    me.name, parv[1], me.name, parv[1],
		    nick, client->name);
		return;
	}

	if (!IsULine(client) && (tklban = find_qline(client, nick, &ishold)))
	{
		if (IsServer(client) && !ishold) /* server introducing new client */
		{
			acptrs = find_server(client->user == NULL ? parv[6] : client->user->server, NULL);
			/* (NEW: no unregistered Q-Line msgs anymore during linking) */
			if (!acptrs || (acptrs->serv && acptrs->serv->flags.synced))
				sendto_snomask(SNO_QLINE, "Q-Lined nick %s from %s on %s", nick,
				    (*client->name != 0
				    && !IsServer(client) ? client->name : "<unregistered>"),
				    acptrs ? acptrs->name : "unknown server");
		}
	}

	/* Now check if 'nick' already exists - first, collisions with server names/ids (extremely rare) */
	if ((acptr = find_server(nick, NULL)) != NULL)
	{
		sendto_umode(UMODE_OPER, "Nick collision on %s(%s <- %s)",
		    client->name, acptr->direction->name,
		    get_client_name(client, FALSE));
		ircstats.is_kill++;
		sendto_one(client, NULL, ":%s KILL %s :%s (%s <- %s)",
		    me.name, parv[1], me.name, acptr->direction->name,
		    get_client_name(client, FALSE));
		return;
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
		sendto_umode(UMODE_OPER, "Nick collision on %s (%s %lld <- %s %lld)",
		    acptr->name, acptr->direction->name, (long long)acptr->lastnick,
		    client->direction->name, (long long)lastnick);
		/*
		   **    I'm putting the KILL handling here just to make it easier
		   ** to read, it's hard to follow it the way it used to be.
		   ** Basically, this is what it will do.  It will kill both
		   ** users if no timestamp is given, or they are equal.  It will
		   ** kill the user on our side if the other server is "correct"
		   ** (user@host differ and their user is older, or user@host are
		   ** the same and their user is younger), otherwise just kill the
		   ** user an reintroduce our correct user.
		   **    The old code just sat there and "hoped" the other server
		   ** would kill their user.  Not anymore.
		   **                                               -- binary
		 */
		if (acptr->lastnick == lastnick)
		{
			nick_collision(client, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_EQUAL);
			return;	/* We killed both users, now stop the process. */
		}

		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)) || acptr->direction == client->direction)	/* we missed a QUIT somewhere ? */
		{
			nick_collision(client, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_NEW_WON);
			/* We got rid of the "wrong" user. Introduce the correct one. */
			/* ^^ hmm.. why was this absent in nenolod's code, resulting in a 'return 0'? seems wrong. */
			goto nickkill2done;
		}

		if ((differ && (acptr->lastnick < lastnick)) || (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(client, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_EXISTING_WON);
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

	/* FIXME: we need to split this out into register_remote_user() or something. */
	parv[3] = nick;
	parv[6] = client->name;
	do_cmd(client, recv_mtags, "USER", parc - 3, &parv[3]);
	if (IsDead(client))
		return;
	if (!IsULine(serv) && IsSynched(serv))
		sendto_fconnectnotice(client, 0, NULL);

	RunHook(HOOKTYPE_REMOTE_CONNECT, client);
}

/*
** cmd_nick
**	parv[1] = nickname
**  if from new client  -taz
**	parv[2] = nick password
**  if from server:
**      parv[2] = hopcount
**      parv[3] = timestamp
**      parv[4] = username
**      parv[5] = hostname
**      parv[6] = servername
**  if NICK version 1:
**      parv[7] = servicestamp
**	parv[8] = info
**  if NICK version 2:
**	parv[7] = servicestamp
**      parv[8] = umodes
**	parv[9] = virthost, * if none
**	parv[10] = info
**  if NICKIP:
**      parv[10] = ip
**      parv[11] = info
*/
/* FIXME: update above docs and this function since NICKv2 and NICKIP are now always true ;) */
CMD_FUNC(cmd_nick)
{
	TKL *tklban;
	int ishold;
	Client *acptr, *serv = NULL;
	Client *acptrs;
	char nick[NICKLEN + 2], descbuf[BUFSIZE];
	Membership *mp;
	long lastnick = 0l;
	int  differ = 1, update_watch = 1;
	unsigned char newusr = 0;
	unsigned char removemoder = (client->umodes & UMODE_REGNICK) ? 1 : 0;
	Hook *h;
	int i = 0;
	char *nickid = (IsUser(client) && *client->id) ? client->id : NULL;
	Client *cptr = client->direction; /* Pending a complete overhaul... (TODO) */
	/*
	 * If the user didn't specify a nickname, complain
	 */
	if (parc < 2)
	{
		sendnumeric(client, ERR_NONICKNAMEGIVEN);
		return;
	}

	if (!IsServer(cptr))
	{
		if (MyConnect(client) && iConf.min_nick_length && !IsOper(client) && !IsULine(client) && strlen(parv[1]) < iConf.min_nick_length)
		{
			snprintf(descbuf, sizeof descbuf, "A minimum length of %d chars is required", iConf.min_nick_length);
			sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], descbuf);
			return;
		}
		strlcpy(nick, parv[1], iConf.nick_length + 1);
	}
	else
		strlcpy(nick, parv[1], NICKLEN + 1);

	if (MyConnect(client) && client->user && !ValidatePermissionsForPath("immune:nick-flood",client,NULL,NULL,NULL))
	{
		if ((client->user->flood.nick_c >= NICK_COUNT) &&
		    (TStime() - client->user->flood.nick_t < NICK_PERIOD))
		{
			/* Throttle... */
			sendnumeric(client, ERR_NCHANGETOOFAST, nick,
				(int)(NICK_PERIOD - (TStime() - client->user->flood.nick_t)));
			return;
		}
	}

	/* For a local clients, do proper nickname checking via do_nick_name()
	 * and reject the nick if it returns false.
	 * For remote clients, do a quick check by using do_remote_nick_name(),
	 * if this returned false then reject and kill it. -- Syzop
	 */
	if ((IsServer(cptr) && !do_remote_nick_name(nick)) ||
	    (!IsServer(cptr) && !do_nick_name(nick)))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, parv[1], "Illegal characters");

		if (IsServer(cptr))
		{
			ircstats.is_kill++;
			sendto_umode(UMODE_OPER, "Bad Nick: %s From: %s %s",
			    parv[1], client->name, get_client_name(cptr, FALSE));
			sendto_one(cptr, NULL, ":%s KILL %s :%s (%s <- %s[%s])",
			    me.name, parv[1], me.name, parv[1],
			    nick, cptr->name);
			if (client != cptr)
			{
				/* bad nick change */
				MessageTag *mtags = NULL;
				int n;

				new_message(client, NULL, &mtags);

				sendto_server(client, 0, 0, mtags,
				    ":%s KILL %s :%s (%s <- %s!%s@%s)",
				    me.name, client->name, me.name,
				    get_client_name(cptr, FALSE),
				    client->name,
				    client->user ? client->ident : "",
				    client->user ? client->user->server :
				    cptr->name);
				SetKilled(client);
				exit_client(client, mtags, "BadNick");

				free_message_tags(mtags);

				return;
			}
		}
		return;
	}

	/* Kill quarantined opers early... */
	if (IsServer(cptr) && IsQuarantined(client->direction) &&
	    (parc >= 11) && strchr(parv[8], 'o'))
	{
		ircstats.is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(cptr, NULL, ":%s KILL %s :%s (Quarantined: no oper privileges allowed)",
			me.name, parv[1], me.name);
		sendto_realops("QUARANTINE: Oper %s on server %s killed, due to quarantine",
			parv[1], client->name);
		/* (nothing to exit_client or to free, since user was never added) */
		return;
	}

	/*
	   ** Protocol 4 doesn't send the server as prefix, so it is possible
	   ** the server doesn't exist (a lagged net.burst), in which case
	   ** we simply need to ignore the NICK. Also when we got that server
	   ** name (again) but from another direction. --Run
	 */
	/*
	   ** We should really only deal with this for msgs from servers.
	   ** -- Aeto
	 */
	if (IsServer(cptr) &&
	    (parc > 7
	    && (!(serv = find_server(parv[6], NULL))
	    || serv->direction != cptr->direction)))
	{
		sendto_realops("Cannot find server %s (%s)", parv[6], backupbuf);
		return;
	}
	/*
	   ** Check against nick name collisions.
	   **
	   ** Put this 'if' here so that the nesting goes nicely on the screen :)
	   ** We check against server name list before determining if the nickname
	   ** is present in the nicklist (due to the way the below for loop is
	   ** constructed). -avalon
	 */
	if ((acptr = find_server(nick, NULL)))
	{
		if (MyConnect(client))
		{
			sendnumeric(client, ERR_NICKNAMEINUSE, nick);
			return;	/* NICK message ignored */
		}
	}

	/*
	   ** Check for a Q-lined nickname. If we find it, and it's our
	   ** client, just reject it. -Lefler
	   ** Allow opers to use Q-lined nicknames. -Russell
	 */
	if (!strcasecmp("ircd", nick) || !strcasecmp("irc", nick))
	{
		sendnumeric(client, ERR_ERRONEUSNICKNAME, nick,
		    "Reserved for internal IRCd purposes");
		return;
	}
	if (MyUser(client)) /* local client changin nick afterwards.. */
	{
		int xx;
		spamfilter_build_user_string(spamfilter_user, nick, client);
		if (match_spamfilter(client, spamfilter_user, SPAMF_USER, NULL, 0, NULL))
			return;
	}
	if (!IsULine(client) && (tklban = find_qline(client, nick, &ishold)))
	{
		if (IsServer(client) && !ishold) /* server introducing new client */
		{
			acptrs = find_server(client->user == NULL ? parv[6] : client->user->server, NULL);
			/* (NEW: no unregistered Q-Line msgs anymore during linking) */
			if (!acptrs || (acptrs->serv && acptrs->serv->flags.synced))
				sendto_snomask(SNO_QLINE, "Q-Lined nick %s from %s on %s", nick,
				    (*client->name != 0
				    && !IsServer(client) ? client->name : "<unregistered>"),
				    acptrs ? acptrs->name : "unknown server");
		}

		if (IsServer(cptr) && IsUser(client) && !ishold) /* remote user changing nick */
		{
			sendto_snomask(SNO_QLINE, "Q-Lined nick %s from %s on %s", nick,
				client->name, client->srvptr ? client->srvptr->name : "<unknown>");
		}

		if (!IsServer(cptr)) /* local */
		{
			if (ishold)
			{
				sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, tklban->ptr.nameban->reason);
				return;
			}
			if (!ValidatePermissionsForPath("immune:server-ban:ban-nick",client,NULL,NULL,nick))
			{
				client->local->since += 4; /* lag them up */
				sendnumeric(client, ERR_ERRONEUSNICKNAME, nick, tklban->ptr.nameban->reason);
				sendto_snomask(SNO_QLINE, "Forbidding Q-lined nick %s from %s.",
				    nick, get_client_name(cptr, FALSE));
				return;	/* NICK message ignored */
			}
		}
	}
	/*
	   ** acptr already has result from previous find_server()
	 */
	if (acptr)
	{
		/*
		   ** We have a nickname trying to use the same name as
		   ** a server. Send out a nick collision KILL to remove
		   ** the nickname. As long as only a KILL is sent out,
		   ** there is no danger of the server being disconnected.
		   ** Ultimate way to jupiter a nick ? >;-). -avalon
		 */
		sendto_umode(UMODE_OPER, "Nick collision on %s(%s <- %s)",
		    client->name, acptr->direction->name,
		    get_client_name(cptr, FALSE));
		ircstats.is_kill++;
		sendto_one(cptr, NULL, ":%s KILL %s :%s (%s <- %s)",
		    me.name, parv[1], me.name, acptr->direction->name,
		    /* NOTE: Cannot use get_client_name
		       ** twice here, it returns static
		       ** string pointer--the other info
		       ** would be lost
		     */
		    get_client_name(cptr, FALSE));
		return;
	}

	if (MyUser(cptr) && !ValidatePermissionsForPath("immune:nick-flood",client,NULL,NULL,NULL))
		cptr->local->since += 3;	/* Nick-flood prot. -Donwulff */

	if (!(acptr = find_client(nick, NULL)))
		goto nickkilldone;	/* No collisions, all clear... */
	/*
	   ** If the older one is "non-person", the new entry is just
	   ** allowed to overwrite it. Just silently drop non-person,
	   ** and proceed with the nick. This should take care of the
	   ** "dormant nick" way of generating collisions...
	 */
	/* Moved before Lost User Field to fix some bugs... -- Barubary */
	if (IsUnknown(acptr) && MyConnect(acptr))
	{
		/* This may help - copying code below */
		if (acptr == cptr)
			return;
		SetKilled(acptr);
		exit_client(acptr, NULL, "Overridden");
		goto nickkilldone;
	}
	/* A sanity check in the user field... */
	if (acptr->user == NULL)
	{
		/* This is a Bad Thing */
		sendto_umode(UMODE_OPER, "Lost user field for %s in change from %s",
		    acptr->name, get_client_name(cptr, FALSE));
		ircstats.is_kill++;
		sendto_one(acptr, NULL, ":%s KILL %s :%s (Lost user field!)",
		    me.name, acptr->name, me.name);
		SetKilled(acptr);
		/* Here's the previous versions' desynch.  If the old one is
		   messed up, trash the old one and accept the new one.
		   Remember - at this point there is a new nick coming in!
		   Handle appropriately. -- Barubary */
		exit_client(acptr, NULL, "Lost user field");
		goto nickkilldone;
	}
	/*
	   ** If acptr == client, then we have a client doing a nick
	   ** change between *equivalent* nicknames as far as server
	   ** is concerned (user is changing the case of their
	   ** nickname or somesuch)
	 */
	if (acptr == client) {
		if (strcmp(acptr->name, nick) != 0)
		{
			/* Allows change of case in their nick */
			removemoder = 0; /* don't set the user -r */
			goto nickkilldone;	/* -- go and process change */
		} else
			/*
			 ** This is just ':old NICK old' type thing.
			 ** Just forget the whole thing here. There is
			 ** no point forwarding it to anywhere,
			 ** especially since servers prior to this
			 ** version would treat it as nick collision.
			 */
			return;	/* NICK Message ignored */
	}
	/*
	   ** Note: From this point forward it can be assumed that
	   ** acptr != client (point to different client structures).
	 */
	/*
	   ** Decide, we really have a nick collision and deal with it
	 */
	if (!IsServer(cptr))
	{
		/*
		   ** NICK is coming from local client connection. Just
		   ** send error reply and ignore the command.
		 */
		sendnumeric(client, ERR_NICKNAMEINUSE, nick);
		return;	/* NICK message ignored */
	}
	/*
	   ** NICK was coming from a server connection.
	   ** This means we have a race condition (two users signing on
	   ** at the same time), or two net fragments reconnecting with
	   ** the same nick.
	   ** The latter can happen because two different users connected
	   ** or because one and the same user switched server during a
	   ** net break.
	   ** If we have the old protocol (no TimeStamp and no user@host)
	   ** or if the TimeStamps are equal, we kill both (or only 'new'
	   ** if it was a "NICK new"). Otherwise we kill the youngest
	   ** when user@host differ, or the oldest when they are the same.
	   ** --Run
	   **
	 */
	if (IsServer(client))
	{
		/*
		   ** A new NICK being introduced by a neighbouring
		   ** server (e.g. message type "NICK new" received)
		 */
		if (parc > 3)
		{
			lastnick = atol(parv[3]);
			if (parc > 5)
				differ = (mycmp(acptr->user->username, parv[4])
				    || mycmp(acptr->user->realhost, parv[5]));
		}
		sendto_umode(UMODE_OPER, "Nick collision on %s (%s %lld <- %s %lld)",
		    acptr->name, acptr->direction->name, (long long)acptr->lastnick,
		    cptr->name, (long long)lastnick);
		/*
		   **    I'm putting the KILL handling here just to make it easier
		   ** to read, it's hard to follow it the way it used to be.
		   ** Basically, this is what it will do.  It will kill both
		   ** users if no timestamp is given, or they are equal.  It will
		   ** kill the user on our side if the other server is "correct"
		   ** (user@host differ and their user is older, or user@host are
		   ** the same and their user is younger), otherwise just kill the
		   ** user an reintroduce our correct user.
		   **    The old code just sat there and "hoped" the other server
		   ** would kill their user.  Not anymore.
		   **                                               -- binary
		 */
		if (!(parc > 3) || (acptr->lastnick == lastnick))
		{
			nick_collision(client, parv[1], nickid, NULL, acptr, NICKCOL_EQUAL);
			return; /* We killed both users, now stop the process. */
		}

		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)) || acptr->direction == cptr)	/* we missed a QUIT somewhere ? */
		{
			nick_collision(client, parv[1], nickid, NULL, acptr, NICKCOL_NEW_WON);
			/* OK, we got rid of the "wrong" user, now we're going to add the
			 * user the other server introduced.
			 */
			goto nickkilldone;
		}

		if ((differ && (acptr->lastnick < lastnick)) ||
		    (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(client, parv[1], nickid, NULL, acptr, NICKCOL_EXISTING_WON);
			return; /* Ignore the NICK. */
		}
		return;
	}
	else
	{
		/*
		   ** A NICK change has collided (e.g. message type ":old NICK new").
		 */
		if (parc > 2)
			lastnick = atol(parv[2]);
		differ = (mycmp(acptr->user->username, client->user->username) ||
		    mycmp(acptr->user->realhost, client->user->realhost));
		sendto_umode(UMODE_OPER, "Nick change collision from %s to %s (%s %lld <- %s %lld)",
		    client->name, acptr->name, acptr->direction->name,
		    (long long)acptr->lastnick,
		    client->direction->name, (long long)lastnick);
		if (!(parc > 2) || lastnick == acptr->lastnick)
		{
			nick_collision(client, parv[1], nickid, client, acptr, NICKCOL_EQUAL);
			return; /* Now that I killed them both, ignore the NICK */
		}
		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)))
		{
			nick_collision(client, parv[1], nickid, client, acptr, NICKCOL_NEW_WON);
			goto nickkilldone;	/* their user won, introduce new nick */
		}
		if ((differ && (acptr->lastnick < lastnick)) ||
		    (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(client, parv[1], nickid, client, acptr, NICKCOL_EXISTING_WON);
			return; /* their user lost, ignore the NICK */
		}

	}
	return;		/* just in case */
      nickkilldone:
	if (IsServer(client))
	{
		/* A server introducing a new client, change source */

		if (serv == NULL)
			serv = client;
		client = make_client(cptr, serv);
		add_client_to_list(client);
		if (parc > 2)
			client->hopcount = atol(parv[2]);
		if (parc > 3)
			client->lastnick = atol(parv[3]);
		else		/* Little bit better, as long as not all upgraded */
			client->lastnick = TStime();
		if (client->lastnick < 0)
		{
			sendto_realops
			    ("Negative timestamp recieved from %s, resetting to TStime (%s)",
			    cptr->name, backupbuf);
			client->lastnick = TStime();
		}
		newusr = 1;
	}
	else if (client->name[0] && IsUser(client))
	{
		MessageTag *mtags = NULL;

		/*
		   ** If the client belongs to me, then check to see
		   ** if client is currently on any channels where it
		   ** is currently banned.  If so, do not allow the nick
		   ** change to occur.
		   ** Also set 'lastnick' to current time, if changed.
		 */
		if (MyUser(client))
		{
			for (mp = client->user->channel; mp; mp = mp->next)
			{
				if (!is_skochanop(client, mp->channel) && is_banned(client, mp->channel, BANCHK_NICK, NULL, NULL))
				{
					sendnumeric(client, ERR_BANNICKCHANGE,
					    mp->channel->chname);
					return;
				}
				if (CHECK_TARGET_NICK_BANS && !is_skochanop(client, mp->channel) && is_banned_with_nick(client, mp->channel, BANCHK_NICK, nick, NULL, NULL))
				{
					sendnumeric(client, ERR_BANNICKCHANGE, mp->channel->chname);
					return;
				}

				for (h = Hooks[HOOKTYPE_CHAN_PERMIT_NICK_CHANGE]; h; h = h->next)
				{
					i = (*(h->func.intfunc))(client,mp->channel);
					if (i != HOOK_CONTINUE)
						break;
				}

				if (i == HOOK_DENY)
				{
					sendnumeric(client, ERR_NONICKCHANGE,
					    mp->channel->chname);
					return;
				}
			}

			if (TStime() - client->user->flood.nick_t >= NICK_PERIOD)
			{
				client->user->flood.nick_t = TStime();
				client->user->flood.nick_c = 1;
			} else
				client->user->flood.nick_c++;

			sendto_snomask(SNO_NICKCHANGE, "*** %s (%s@%s) has changed their nickname to %s",
				client->name, client->user->username, client->user->realhost, nick);

			RunHook2(HOOKTYPE_LOCAL_NICKCHANGE, client, nick);
		} else {
			if (!IsULine(client))
				sendto_snomask(SNO_FNICKCHANGE, "*** %s (%s@%s) has changed their nickname to %s",
					client->name, client->user->username, client->user->realhost, nick);

			RunHook2(HOOKTYPE_REMOTE_NICKCHANGE, client, nick);
		}
		/*
		 * Client just changing their nick. If he/she is
		 * on a channel, send note of change to all clients
		 * on that channel. Propagate notice to other servers.
		 */
		if (mycmp(client->name, nick) ||
		    /* Next line can be removed when all upgraded  --Run */
		    (!MyUser(client) && parc > 2
		    && atol(parv[2]) < client->lastnick))
			client->lastnick = (MyUser(client)
			    || parc < 3) ? TStime() : atol(parv[2]);
		if (client->lastnick < 0)
		{
			sendto_realops("Negative timestamp (%s)", backupbuf);
			client->lastnick = TStime();
		}
		add_history(client, 1);
		new_message(client, recv_mtags, &mtags);
		sendto_server(client, 0, 0, mtags, ":%s NICK %s %lld",
		    ID(client), nick, (long long)client->lastnick);
		sendto_local_common_channels(client, client, 0, mtags, ":%s NICK :%s", client->name, nick);
		sendto_one(client, mtags, ":%s NICK :%s", client->name, nick);
		free_message_tags(mtags);
		if (removemoder)
			client->umodes &= ~UMODE_REGNICK;
	}
	else if (!client->name[0])
	{
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
				sendto_one(client, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
					me.name, nick);

			client->lastnick = TStime();
			if (!register_user(client, nick, client->user->username, NULL, NULL, NULL))
				return;
			strlcpy(nick, client->name, sizeof(nick)); /* don't ask, but I need this. do not remove! -- Syzop */
			update_watch = 0;
			newusr = 1;
		}
	}
	/*
	 *  Finally set new nick name.
	 */
	if (update_watch && client->name[0])
	{
		(void)del_from_client_hash_table(client->name, client);
		if (IsUser(client))
			hash_check_watch(client, RPL_LOGOFF);
	}

	/* update fdlist --nenolod */
	if (MyConnect(client))
	{
		snprintf(descbuf, sizeof descbuf, "Client: %s", nick);
		fd_desc(client->local->fd, descbuf);
	}

	(void)strcpy(client->name, nick);
	(void)add_to_client_hash_table(nick, client);
	if (IsServer(cptr) && parc > 7)
	{
		parv[3] = nick;
		do_cmd(client, recv_mtags, "USER", parc - 3, &parv[3]);
		if (IsDead(client))
			return;
		if (IsNetInfo(cptr) && !IsULine(client)) // FIXME: use IsSynced here too to match cmd_uid
			sendto_fconnectnotice(client, 0, NULL);
	}
	else if (IsUser(client) && update_watch)
		hash_check_watch(client, RPL_LOGON);

	if (newusr && !MyUser(client) && IsUser(client))
	{
		RunHook(HOOKTYPE_REMOTE_CONNECT, client);
	}

	if (removemoder && MyUser(client))
	{
		sendto_one(client, NULL, ":%s MODE %s :-r", me.name, client->name);
	}
}

/** Register the connection as a User.
 * This is called after NICK + USER (in no particular order)
 * and possibly other protocol messages as well (eg CAP).
 * @param client		Client to be made a user.
 * @param nick		Nick name
 * @param username	Username
 * @param umode		User modes
 * @param virthost	Virtual host (can be NULL)
 * @param ip		IP address string (can be NULL)
 * @returns 1 if successfully registered, 0 if not (client might be killed).
 */
int _register_user(Client *client, char *nick, char *username, char *umode, char *virthost, char *ip)
{
	ConfigItem_ban *bconf;
	char *tmpstr;
	char stripuser[USERLEN + 1], *u1 = stripuser, *u2, olduser[USERLEN + 1],
	    userbad[USERLEN * 2 + 1], *ubad = userbad, noident = 0;
	int i, xx;
	Hook *h;
	ClientUser *user = client->user;
	Client *nclient;
	char *tkllayer[9] = {
		me.name,	/*0  server.name */
		"+",		/*1  +|- */
		"z",		/*2  G   */
		"*",		/*3  user */
		NULL,		/*4  host */
		NULL,
		NULL,		/*6  expire_at */
		NULL,		/*7  set_at */
		NULL		/*8  reason */
	};
	TKL *savetkl = NULL;
	ConfigItem_tld *tlds;

	nick = client->name; /* <- The data is always the same, but the pointer is sometimes not,
	                    *    I need this for one of my modules, so do not remove! ;) -- Syzop */

	if (MyConnect(client))
	{
	        char temp[USERLEN + 1];

		if (!AllowClient(client, username))
		{
			ircstats.is_ref++;
			/* For safety, we have an extra kill here */
			if (!IsDead(client))
				exit_client(client, NULL, "Rejected");
			return 0;
		}

		if (client->local->hostp)
		{
			/* reject ASCII < 32 and ASCII >= 127 (note: upper resolver might be even more strict). */
			for (tmpstr = client->local->sockhost; *tmpstr > ' ' && *tmpstr < 127; tmpstr++);

			/* if host contained invalid ASCII _OR_ the DNS reply is an IP-like reply
			 * (like: 1.2.3.4 or ::ffff:1.2.3.4), then reject it and use IP instead.
			 */
			if (*tmpstr || !*user->realhost || (isdigit(*client->local->sockhost) && (client->local->sockhost > tmpstr && isdigit(*(tmpstr - 1))) )
			    || (client->local->sockhost[0] == ':'))
				strlcpy(client->local->sockhost, client->ip, sizeof(client->local->sockhost));
		}
		if (client->local->sockhost[0])
		{
			strlcpy(user->realhost, client->local->sockhost, sizeof(client->local->sockhost)); /* SET HOSTNAME */
		} else {
			sendto_realops("[HOSTNAME BUG] client->local->sockhost is empty for user %s (%s, %s)",
				client->name, client->ip ? client->ip : "<null>", user->realhost);
			ircd_log(LOG_ERROR, "[HOSTNAME BUG] client->local->sockhost is empty for user %s (%s, %s)",
				client->name, client->ip ? client->ip : "<null>", user->realhost);
		}

		/*
		 * I do not consider *, ~ or ! 'hostile' in usernames,
		 * as it is easy to differentiate them (Use \*, \? and \\)
		 * with the possible?
		 * exception of !. With mIRC etc. ident is easy to fake
		 * to contain @ though, so if that is found use non-ident
		 * username. -Donwulff
		 *
		 * I do, We only allow a-z A-Z 0-9 _ - and . now so the
		 * !strchr(client->ident, '@') check is out of date. -Cabal95
		 *
		 * Moved the noident stuff here. -OnyxDragon
		 */

		/* because username may point to user->username */
		strlcpy(temp, username, USERLEN + 1);

		if (!IsUseIdent(client))
			strlcpy(user->username, temp, USERLEN + 1);
		else if (IsIdentSuccess(client))
			strlcpy(user->username, client->ident, USERLEN+1);
		else
		{
			if (IDENT_CHECK == 0) {
				strlcpy(user->username, temp, USERLEN+1);
			}
			else {
				*user->username = '~';
				strlcpy((user->username + 1), temp, sizeof(user->username)-1);
				noident = 1;
			}

		}
		/*
		 * Limit usernames to just 0-9 a-z A-Z _ - and .
		 * It strips the "bad" chars out, and if nothing is left
		 * changes the username to the first 8 characters of their
		 * nickname. After the MOTD is displayed it sends numeric
		 * 455 to the user telling them what(if anything) happened.
		 * -Cabal95
		 *
		 * Moved the noident thing to the right place - see above
		 * -OnyxDragon
		 *
		 * No longer use nickname if the entire ident is invalid,
                 * if thats the case, it is likely the user is trying to cause
		 * problems so just ban them. (Using the nick could introduce
		 * hostile chars) -- codemastr
		 */
		for (u2 = user->username + noident; *u2; u2++)
		{
			if (isallowed(*u2))
				*u1++ = *u2;
			else if (*u2 < 32)
			{
				/*
				 * Make sure they can read what control
				 * characters were in their username.
				 */
				*ubad++ = '^';
				*ubad++ = *u2 + '@';
			}
			else
				*ubad++ = *u2;
		}
		*u1 = '\0';
		*ubad = '\0';
		if (strlen(stripuser) != strlen(user->username + noident))
		{
			if (stripuser[0] == '\0')
			{
				exit_client(client, NULL, "Hostile username. Please use only 0-9 a-z A-Z _ - and . in your username.");
				return 0;
			}

			strlcpy(olduser, user->username + noident, USERLEN+1);
			strlcpy(user->username + 1, stripuser, sizeof(user->username)-1);
			user->username[0] = '~';
			user->username[USERLEN] = '\0';
		}
		else
			u1 = NULL;

		/* Check ban realname { } blocks */
		if ((bconf = Find_ban(NULL, client->info, CONF_BAN_REALNAME)))
		{
			ircstats.is_ref++;
			banned_client(client, "realname", bconf->reason?bconf->reason:"", 0, 0);
			return 0;
		}
		/* Check G/Z lines before shuns -- kill before quite -- codemastr */
		if (find_tkline_match(client, 0))
		{
			ircstats.is_ref++;
			return 0;
		}
		find_shun(client);

		spamfilter_build_user_string(spamfilter_user, client->name, client);
		if (match_spamfilter(client, spamfilter_user, SPAMF_USER, NULL, 0, &savetkl))
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
			i = (*(h->func.intfunc))(client);
			if (i == HOOK_DENY)
				return 0;
			if (i == HOOK_ALLOW)
				break;
		}
	}
	else
	{
		strlcpy(user->username, username, USERLEN+1);
	}
	SetUser(client);
	irccounts.clients++;
	if (client->srvptr && client->srvptr->serv)
		client->srvptr->serv->users++;

	make_virthost(client, user->realhost, user->cloakedhost, 0);
	safe_strdup(user->virthost, user->cloakedhost);

	if (MyConnect(client))
	{
		char descbuf[BUFSIZE];
		int i;

		snprintf(descbuf, sizeof descbuf, "Client: %s", nick);
		fd_desc(client->local->fd, descbuf);

		list_move(&client->lclient_node, &lclient_list);

		irccounts.unknown--;
		irccounts.me_clients++;

		if (IsSecure(client))
		{
			client->umodes |= UMODE_SECURE;
			RunHook(HOOKTYPE_SECURE_CONNECT, client);
		}

		if (IsHidden(client))
			ircd_log(LOG_CLIENT, "Connect - %s!%s@%s [VHOST %s]", nick,
				user->username, user->realhost, user->virthost);
		else
			ircd_log(LOG_CLIENT, "Connect - %s!%s@%s", nick, user->username,
				user->realhost);
		RunHook2(HOOKTYPE_WELCOME, client, 0);
		sendnumeric(client, RPL_WELCOME,
		    ircnetwork, nick, user->username, user->realhost);
		RunHook2(HOOKTYPE_WELCOME, client, 1);
		sendnumeric(client, RPL_YOURHOST,
		    me.name, version);
		RunHook2(HOOKTYPE_WELCOME, client, 2);
		sendnumeric(client, RPL_CREATED, creation);
		RunHook2(HOOKTYPE_WELCOME, client, 3);
		sendnumeric(client, RPL_MYINFO,
		    me.name, version, umodestring, cmodestring);
		RunHook2(HOOKTYPE_WELCOME, client, 4);

		for (i = 0; ISupportStrings[i]; i++)
			sendnumeric(client, RPL_ISUPPORT, ISupportStrings[i]);

		RunHook2(HOOKTYPE_WELCOME, client, 5);

		if (IsHidden(client))
		{
			sendnumeric(client, RPL_HOSTHIDDEN, user->virthost);
			RunHook2(HOOKTYPE_WELCOME, client, 396);
		}

		if (IsSecureConnect(client))
		{
			if (client->local->ssl && !iConf.no_connect_tls_info)
			{
				sendnotice(client, "*** You are connected to %s with %s",
					me.name, tls_get_cipher(client->local->ssl));
			}
		}

		{
			char *parv[2];
			parv[0] = client->name;
			parv[1] = NULL;
			do_cmd(client, NULL, "LUSERS", 1, parv);
			if (IsDead(client))
				return 0;
		}

		RunHook2(HOOKTYPE_WELCOME, client, 266);

		short_motd(client);

		RunHook2(HOOKTYPE_WELCOME, client, 376);

#ifdef EXPERIMENTAL
		sendnotice(client,
			"*** \2NOTE:\2 This server is running experimental IRC server software (UnrealIRCd %s). "
			"If you find any bugs or problems, please report them at https://bugs.unrealircd.org/",
			VERSIONONLY);
#endif
		/*
		 * Now send a numeric to the user telling them what, if
		 * anything, happened.
		 */
		if (u1)
			sendnumeric(client, ERR_HOSTILENAME, olduser, userbad, stripuser);
	}
	else
	{
		Client *acptr;

		/* Remote client */
		if (!(acptr = find_server_quick(user->server)))
		{
			sendto_ops("Bad USER [%s] :%s USER %s %s : No such server",
			           client->name, nick, user->username, user->server);
			sendto_one(client, NULL, ":%s KILL %s :%s (No such server: %s)",
			    me.name, client->name, me.name, user->server);
			SetKilled(client);
			exit_client(client, NULL, "USER without prefix(2.8) or wrong prefix");
			return 0;
		}
		else if (acptr->direction != client->direction)
		{
			sendto_ops("Bad User [%s] :%s USER %s %s, != %s[%s]",
			    client->name, nick, user->username, user->server,
			    acptr->name, acptr->direction->name);
			sendto_one(client, NULL, ":%s KILL %s :%s (%s != %s[%s])",
			    me.name, client->name, me.name, user->server,
			    acptr->direction->name, acptr->direction->local->sockhost);
			SetKilled(client);
			exit_client(client, NULL, "USER server wrong direction");
			return 0;
		} else
		{
			client->flags |= acptr->flags;
		}

		if (IsULine(client->srvptr))
			SetULine(client);
	}
	if (client->umodes & UMODE_INVISIBLE)
	{
		irccounts.invisible++;
	}

	if (virthost && umode)
	{
		/* Set the IP address first */
		if (ip && (*ip != '*'))
		{
			char *ipstring = decode_ip(ip);
			if (!ipstring)
			{
				sendto_ops("USER with invalid IP (%s) (%s) -- "
				           "IP must be base64 encoded binary representation of either IPv4 or IPv6",
				           client->name, ip);
				exit_client(client, NULL, "USER with invalid IP");
				return 0;
			}
			safe_strdup(client->ip, ipstring);
		}

		/* For remote clients we recalculate the cloakedhost here because
		 * it may depend on the IP address (bug #5064).
		 */
		make_virthost(client, user->realhost, user->cloakedhost, 0);
		safe_strdup(user->virthost, user->cloakedhost);

		/* Set the umodes */
		tkllayer[0] = nick;
		tkllayer[1] = nick;
		tkllayer[2] = umode;
		tkllayer[3] = NULL;
		dontspread = 1;
		do_cmd(client, NULL, "MODE", 3, tkllayer);
		dontspread = 0;

		/* Set the vhost */
		if (virthost && *virthost != '*')
			safe_strdup(client->user->virthost, virthost);
	}

	hash_check_watch(client, RPL_LOGON);	/* Uglier hack */
	build_umode_string(client, 0, SEND_UMODES|UMODE_SERVNOTICE, buf);

	sendto_serv_butone_nickcmd(client->direction, client, (*buf == '\0' ? "+" : buf));

	if (MyConnect(client))
	{
		broadcast_moddata_client(client);
		sendto_connectnotice(client, 0, NULL); /* moved down, for modules. */

		/* Send password from client->local->passwd to NickServ for identification,
		 * if passwd given and if NickServ is online.
		 * - by taz, modified by Wizzu
		 */
		if (client->local->passwd && !IsLoggedIn(client) && SERVICES_NAME && (nclient = find_person(NickServ, NULL)))
		{
			int do_identify = 1;
			Hook *h;
			for (h = Hooks[HOOKTYPE_LOCAL_NICKPASS]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(client,nclient);
				if (i == HOOK_DENY)
				{
					do_identify = 0;
					break;
				}
			}
			if (do_identify)
				sendto_one(nclient, NULL, ":%s PRIVMSG %s@%s :IDENTIFY %s",
				    client->name,
				    NickServ, SERVICES_NAME, client->local->passwd);
		}
		if (buf[0] != '\0' && buf[1] != '\0')
			sendto_one(client, NULL, ":%s MODE %s :%s", client->name,
			    client->name, buf);
		if (user->snomask)
			sendnumeric(client, RPL_SNOMASK, get_snostr(user->snomask));

		if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_user == POLICY_WARN))
			sendnotice(client, "%s", iConf.plaintext_policy_user_message);

		if (IsSecure(client) && (iConf.outdated_tls_policy_user == POLICY_WARN) && outdated_tls_client(client))
			sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_user_message, client));

		/* Make creation time the real 'online since' time, excluding registration time.
		 * Otherwise things like set::anti-spam-quit-messagetime 10s could mean
		 * 1 second in practice (#2174).
		 */
		client->local->firsttime = TStime();

		/* Give the user a fresh start as far as fake-lag is concerned.
		 * Otherwise the user could be lagged up already due to all the CAP stuff.
		 */
		client->local->since = TStime();

		RunHook2(HOOKTYPE_WELCOME, client, 999);

		/* NOTE: Code after this 'if (savetkl)' will not be executed for quarantined-
		 *       virus-users. So be carefull with the order. -- Syzop
		 */
		// FIXME: verify if this works, trace code path upstream!!!!
		if (savetkl)
			return join_viruschan(client, savetkl, SPAMF_USER); /* [RETURN!] */

		/* Force the user to join the given chans -- codemastr */
		tlds = Find_tld(client);

		if (tlds && !BadPtr(tlds->channel)) {
			char *chans[3] = {
				client->name,
				tlds->channel,
				NULL
			};
			do_cmd(client, NULL, "JOIN", 3, chans);
			if (IsDead(client))
				return 0;
		}
		else if (!BadPtr(AUTO_JOIN_CHANS) && strcmp(AUTO_JOIN_CHANS, "0"))
		{
			char *chans[3] = {
				client->name,
				AUTO_JOIN_CHANS,
				NULL
			};
			do_cmd(client, NULL, "JOIN", 3, chans);
			if (IsDead(client))
				return 0;
		}
		/* NOTE: If you add something here.. be sure to check the 'if (savetkl)' note above */
	}

	if (MyConnect(client) && !BadPtr(client->local->passwd))
	{
		safe_free(client->local->passwd);
		client->local->passwd = NULL;
	}

	/* User successfully registered */
	return 1;
}

/* This used to initialize the various name strings used to store hostnames.
 * But nowadays this takes place much earlier (in add_connection?).
 * It's mainly used for "localhost" and WEBIRC magic only now...
 */
int check_init(Client *client, char *sockn, size_t size)
{
	strlcpy(sockn, client->local->sockhost, HOSTLEN);

	RunHookReturnInt3(HOOKTYPE_CHECK_INIT, client, sockn, size, !=0);

	/* Some silly hack to convert 127.0.0.1 and such into 'localhost' */
	if (!strcmp(GetIP(client), "127.0.0.1") ||
	    !strcmp(GetIP(client), "0:0:0:0:0:0:0:1") ||
	    !strcmp(GetIP(client), "0:0:0:0:0:ffff:127.0.0.1"))
	{
		if (client->local->hostp)
		{
			unreal_free_hostent(client->local->hostp);
			client->local->hostp = NULL;
		}
		strlcpy(sockn, "localhost", HOSTLEN);
	}

	return 1;
}

/** Allow or reject the client based on allow { } blocks and all other restrictions.
 * @param client     Client to check (local)
 * @param username   Username, for some reason...
 * @returns 1 if OK, 0 if client is rejected (likely killed too)
 */
int AllowClient(Client *client, char *username)
{
	static char sockhost[HOSTLEN + 1];
	struct hostent *hp = NULL;
	int i;
	ConfigItem_allow *aconf;
	char *hname;
	static char uhost[HOSTLEN + USERLEN + 3];
	static char fullname[HOSTLEN + 1];

	Debug((DEBUG_DNS, "ch_cl: check access for %s[%s]", client->name, client->local->sockhost));

	if (!check_init(client, sockhost, sizeof(sockhost)))
		return 0;

	hp = client->local->hostp;

	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_user == POLICY_DENY))
	{
		exit_client(client, NULL, iConf.plaintext_policy_user_message);
		return 0;
	}

	if (IsSecure(client) && (iConf.outdated_tls_policy_user == POLICY_DENY) && outdated_tls_client(client))
	{
		char *msg = outdated_tls_client_build_string(iConf.outdated_tls_policy_user_message, client);
		exit_client(client, NULL, msg);
		return 0;
	}

	for (aconf = conf_allow; aconf; aconf = aconf->next)
	{
		if (!aconf->hostname || !aconf->ip)
			goto attach;
		if (aconf->auth && !client->local->passwd)
			continue;
		if (aconf->flags.tls && !IsSecure(client))
			continue;
		if (hp && hp->h_name)
		{
			hname = hp->h_name;
			strlcpy(fullname, hname, sizeof(fullname));
			Debug((DEBUG_DNS, "a_il: %s->%s", sockhost, fullname));
			if (strchr(aconf->hostname, '@'))
			{
				if (aconf->flags.noident)
					strlcpy(uhost, username, sizeof(uhost));
				else
					strlcpy(uhost, client->ident, sizeof(uhost));
				strlcat(uhost, "@", sizeof(uhost));
			}
			else
				*uhost = '\0';
			strlcat(uhost, fullname, sizeof(uhost));
			if (match_simple(aconf->hostname, uhost))
				goto attach;
		}

		if (strchr(aconf->ip, '@'))
		{
			if (aconf->flags.noident)
				strlcpy(uhost, username, sizeof(uhost));
			else
				strlcpy(uhost, client->ident, sizeof(uhost));
			(void)strlcat(uhost, "@", sizeof(uhost));
		}
		else
			*uhost = '\0';
		strlcat(uhost, sockhost, sizeof(uhost));
		/* Check the IP */
		if (match_user(aconf->ip, client, MATCH_CHECK_IP))
			goto attach;

		/* Hmm, localhost is a special case, hp == NULL and sockhost contains
		 * 'localhost' instead of an ip... -- Syzop. */
		if (!strcmp(sockhost, "localhost"))
		{
			if (strchr(aconf->hostname, '@'))
			{
				if (aconf->flags.noident)
					strlcpy(uhost, username, sizeof(uhost));
				else
					strlcpy(uhost, client->ident, sizeof(uhost));
				strlcat(uhost, "@localhost", sizeof(uhost));
			}
			else
				strcpy(uhost, "localhost");

			if (match_simple(aconf->hostname, uhost))
				goto attach;
		}

		continue;
	attach:
		if (!aconf->flags.noident)
			SetUseIdent(client);
		if (!aconf->flags.useip && hp)
			strlcpy(uhost, fullname, sizeof(uhost));
		else
			strlcpy(uhost, sockhost, sizeof(uhost));
		set_sockhost(client, uhost);

		if (aconf->maxperip)
		{
			Client *acptr, *acptr2;
			int cnt = 1;

			list_for_each_entry_safe(acptr, acptr2, &lclient_list, lclient_node)
			{
				if (!strcmp(GetIP(acptr), GetIP(client)))
				{
					cnt++;
					if (cnt > aconf->maxperip)
					{
						/* Already got too many with that ip# */
						exit_client(client, NULL, iConf.reject_message_too_many_connections);
						return 0;
					}
				}
			}
		}
		if (aconf->auth && !Auth_Check(client, aconf->auth, client->local->passwd))
		{
			/* Always continue if password was wrong. */
			continue;
		}
		/* Password (or other auth method) was correct */
		safe_free(client->local->passwd);

		if (!((aconf->class->clients + 1) > aconf->class->maxclients))
		{
			client->local->class = aconf->class;
			client->local->class->clients++;
		}
		else
		{
			/* Class is full */
			sendnumeric(client, RPL_REDIR, aconf->server ? aconf->server : defserv, aconf->port ? aconf->port : 6667);
			exit_client(client, NULL, iConf.reject_message_server_full);
			return 0;
		}
		return 1;
	}
	/* User did not match any allow { } blocks: */
	exit_client(client, NULL, iConf.reject_message_unauthorized);
	return 0;
}
