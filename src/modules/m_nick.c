/*
 *   IRC - Internet Relay Chat, src/modules/m_nick.c
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

CMD_FUNC(m_nick);
CMD_FUNC(m_uid);
DLLFUNC int _register_user(aClient *cptr, aClient *sptr, char *nick, char *username, char *umode, char *virthost, char *ip);

#define MSG_NICK 	"NICK"	

ModuleHeader MOD_HEADER(m_nick)
  = {
	"m_nick",
	"4.0",
	"command /nick", 
	"3.2-b8-1",
	NULL 
    };

MOD_TEST(m_nick)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_REGISTER_USER, _register_user);
	return MOD_SUCCESS;
}

MOD_INIT(m_nick)
{
	CommandAdd(modinfo->handle, MSG_NICK, m_nick, MAXPARA, M_USER|M_SERVER|M_UNREGISTERED);
	CommandAdd(modinfo->handle, "UID", m_uid, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_nick)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_nick)
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
void nick_collision(aClient *cptr, char *newnick, char *newid, aClient *new, aClient *existing, int type)
{
	char comment[512];
	char *new_server, *existing_server;
	char reintroduce_existing_user = 0;
	
	ircd_log(LOG_ERROR, "Nick collision: %s[%s]@%s (new) vs %s[%s]@%s (existing). Winner: %s. Type: %s",
		newnick, newid ? newid : "", cptr->name,
		existing->name, existing->id, existing->srvptr->name,
		(type == NICKCOL_EQUAL) ? "None (equal)" : ((type == NICKCOL_NEW_WON) ? "New won" : "Existing won"),
		new ? "nick-change" : "new user connecting");

	new_server = cptr->name; // not correct, should be sptr
	existing_server = (existing == existing->from) ? me.name : existing->from->name;
	if (type == NICKCOL_EXISTING_WON)
		snprintf(comment, sizeof(comment), "Nick collision: %s <- %s", new_server, existing_server);
	else if (type == NICKCOL_NEW_WON)
		snprintf(comment, sizeof(comment), "Nick collision: %s <- %s", existing_server, new_server);
	else
		snprintf(comment, sizeof(comment), "Nick collision: %s <-> %s", existing_server, new_server);

	if ((type == NICKCOL_EQUAL) || (type == NICKCOL_EXISTING_WON))
	{
		/* Kill 'new':
		 * - 'new' is known by the cptr-side as 'newnick' already
		 * - if not nick-changing then the other servers don't know this user
		 * - if nick-changing, then the the other servers know the user as new->name
		 */

		/* cptr case first... this side knows the user by newnick/newid */
		if (CHECKPROTO(cptr, PROTO_SID) && !BadPtr(newid))
		{
			/* SID server can kill 'new' by ID */
			sendto_one(cptr, ":%s KILL %s :%s (%s)",
				me.name, newid, me.name, comment);
		} else {
#ifndef ASSUME_NICK_IN_FLIGHT
			/* cptr is not SID-capable or user has no UID */
			sendto_one(cptr, ":%s KILL %s :%s (%s)",
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
			/* non-cptr side knows this user by their old nick name */
			sendto_server(cptr, PROTO_SID, 0,
				":%s KILL %s :%s (%s)",
				me.name, ID(new), me.name, comment);

			sendto_server(cptr, 0, PROTO_SID,
				":%s KILL %s :%s (%s)",
				me.name, new->name, me.name, comment);
			
			/* Exit the client */
			ircstp->is_kill++;
			new->flags |= FLAGS_KILLED;
			(void)exit_client(NULL, new, &me, comment);
		}
	}
	
	if ((type == NICKCOL_EQUAL) || (type == NICKCOL_NEW_WON))
	{
		/* Now let's kill 'existing' */
		sendto_server(NULL, PROTO_SID, 0,
			":%s KILL %s :%s (%s)",
			me.name, ID(existing), me.name, comment);
		
#ifndef ASSUME_NICK_IN_FLIGHT
		/* This is not ideal on non-SID servers, may kill the wrong person. */
		sendto_server(NULL, 0, PROTO_SID,
			":%s KILL %s :%s (%s)",
			 me.name, existing->name, me.name, comment);
#else
		sendto_server(cptr, 0, PROTO_SID,
			":%s KILL %s :%s (%s)",
			 me.name, existing->name, me.name, comment);
#endif
		
		/* NOTE: we may have sent two KILLs on the same nick in some cases.
		 * Should be acceptable and only happens in a non-100% UID network.
		 */

		/* Exit the client */
		ircstp->is_kill++;
		existing->flags |= FLAGS_KILLED;
		(void)exit_client(NULL, existing, &me, comment);
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
				if (lp->flags & MODE_CHANPROT)
					*p++ = '~';
				*p = '\0';
				
				sendto_one(cptr, ":%s SJOIN %ld %s + :%s%s",
					me.name, lp->chptr->creationtime, lp->chptr->chname,
					flags, existing->name);
			}
		}
		/* Could synch channel-member-data here. But this is apparently an old server anyway.. */
	}
#endif
}

/*
** m_uid
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
*/
CMD_FUNC(m_uid)
{
	aTKline *tklban;
	int ishold;
	aClient *acptr, *serv = NULL;
	aClient *acptrs;
	char nick[NICKLEN + 2], *s, descbuf[BUFSIZE];
	Membership *mp;
	long lastnick = 0l;
	int  differ = 1, update_watch = 1;
	unsigned char removemoder = 1;

	if (!IsServer(cptr))
		strlcpy(nick, parv[1], iConf.nicklen + 1);
	else
		strlcpy(nick, parv[1], NICKLEN + 1);

	/* For a local clients, do proper nickname checking via do_nick_name()
	 * and reject the nick if it returns false.
	 * For remote clients, do a quick check by using do_remote_nick_name(),
	 * if this returned false then reject and kill it. -- Syzop
	 */
	if (IsServer(cptr) && !do_remote_nick_name(nick))
	{
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
		    me.name, sptr->name, parv[1], "Illegal characters");

		if (IsServer(cptr))
		{
			ircstp->is_kill++;
			sendto_umode(UMODE_OPER, "Bad Nick: %s From: %s %s",
			    parv[1], sptr->name, get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
			    me.name, parv[1], me.name, parv[1],
			    nick, cptr->name);
			if (sptr != cptr)
			{	/* bad nick change */
				sendto_server(cptr, 0, 0,
				    ":%s KILL %s :%s (%s <- %s!%s@%s)",
				    me.name, sptr->name, me.name,
				    get_client_name(cptr, FALSE),
				    sptr->name,
				    sptr->user ? sptr->username : "",
				    sptr->user ? sptr->user->server :
				    cptr->name);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr, sptr, &me, "BadNick");
			}
		}
		return 0;
	}

	/* Kill quarantined opers early... */
	if (IsServer(cptr) && (sptr->from->flags & FLAGS_QUARANTINE) &&
	    (parc >= 11) && strchr(parv[8], 'o'))
	{
		ircstp->is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(cptr, ":%s KILL %s :%s (Quarantined: no oper privileges allowed)",
			me.name, parv[1], me.name);
		sendto_realops("QUARANTINE: Oper %s on server %s killed, due to quarantine",
			parv[1], sptr->name);
		/* (nothing to exit_client or to free, since user was never added) */
		return 0;
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
	    && (!(serv = (aClient *)find_server(sptr->name, NULL))
	    || serv->from != cptr->from)))
	{
		sendto_realops("Cannot find SID for %s (%s)", sptr->name,
		    backupbuf);
		return 0;
	}

	/*
	   ** Check for a Q-lined nickname. If we find it, and it's our
	   ** client, just reject it. -Lefler
	   ** Allow opers to use Q-lined nicknames. -Russell
	 */
	if (!stricmp("ircd", nick) || !stricmp("irc", nick))
	{
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME), me.name,
		    sptr->name, nick,
		    "Reserved for internal IRCd purposes");
		return 0;
	}
	if (!IsULine(sptr) && (tklban = find_qline(sptr, nick, &ishold)))
	{
		if (IsServer(sptr) && !ishold) /* server introducing new client */
		{
			acptrs =
			    (aClient *)find_server(sptr->user ==
			    NULL ? (char *)parv[6] : (char *)sptr->user->
			    server, NULL);
			/* (NEW: no unregistered q:line msgs anymore during linking) */
			if (!acptrs || (acptrs->serv && acptrs->serv->flags.synced))
				sendto_snomask(SNO_QLINE, "Q:lined nick %s from %s on %s", nick,
				    (*sptr->name != 0
				    && !IsServer(sptr) ? sptr->name : "<unregistered>"),
				    acptrs ? acptrs->name : "unknown server");
		}
		
		if (IsServer(cptr) && IsPerson(sptr) && !ishold) /* remote user changing nick */
		{
			sendto_snomask(SNO_QLINE, "Q:lined nick %s from %s on %s", nick,
				sptr->name, sptr->srvptr ? sptr->srvptr->name : "<unknown>");
		}
	}

	/*
	   ** acptr already has result from previous find_server()
	 */
	if ((acptr = find_server(nick, NULL)) != NULL)
	{
		/*
		   ** We have a nickname trying to use the same name as
		   ** a server. Send out a nick collision KILL to remove
		   ** the nickname. As long as only a KILL is sent out,
		   ** there is no danger of the server being disconnected.
		   ** Ultimate way to jupiter a nick ? >;-). -avalon
		 */
		sendto_umode(UMODE_OPER, "Nick collision on %s(%s <- %s)",
		    sptr->name, acptr->from->name,
		    get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
		    me.name, sptr->name, me.name, acptr->from->name,
		    /* NOTE: Cannot use get_client_name
		       ** twice here, it returns static
		       ** string pointer--the other info
		       ** would be lost
		     */
		    get_client_name(cptr, FALSE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	}

	if ((acptr = find_client(nick, NULL)) != NULL)
	{
		/*
		   ** A new NICK being introduced by a neighbouring
		   ** server (e.g. message type "NICK new" received)
		 */

		if (MyConnect(acptr) && IsUnknown(acptr))
		{
			acptr->flags |= FLAGS_KILLED;
			exit_client(NULL, acptr, &me, "Overridden");
			goto nickkill2done;
		}

		if (parc > 3)
		{
			lastnick = atol(parv[3]);
			if (parc > 5)
				differ = (mycmp(acptr->user->username, parv[4])
				    || mycmp(acptr->user->realhost, parv[5]));
		}
		sendto_umode(UMODE_OPER, "Nick collision on %s (%s %ld <- %s %ld)",
		    acptr->name, acptr->from->name, acptr->lastnick,
		    cptr->name, lastnick);
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
			nick_collision(cptr, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_EQUAL);
			return 0;	/* We killed both users, now stop the process. */
		}

		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)) || acptr->from == cptr)	/* we missed a QUIT somewhere ? */
		{
			nick_collision(cptr, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_NEW_WON);
			/* We got rid of the "wrong" user. Introduce the correct one. */
			/* ^^ hmm.. why was this absent in nenolod's code, resulting in a 'return 0'? seems wrong. */
			goto nickkill2done;
		}

		if ((differ && (acptr->lastnick < lastnick)) ||
		    (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(cptr, parv[1], ((parc > 6) ? parv[6] : NULL), NULL, acptr, NICKCOL_EXISTING_WON);
			return 0;	/* Ignore the NICK */
		}
		return 0; /* just in case */
	}

nickkill2done:
	if (IsServer(sptr))
	{
		/* A server introducing a new client, change source */

		sptr = make_client(cptr, serv);
		strlcpy(sptr->id, parv[6], IDLEN);
		add_client_to_list(sptr);
		add_to_id_hash_table(sptr->id, sptr);
		if (parc > 2)
			sptr->hopcount = atol(parv[2]);
		if (parc > 3)
			sptr->lastnick = atol(parv[3]);
		else		/* Little bit better, as long as not all upgraded */
			sptr->lastnick = TStime();
		if (sptr->lastnick < 0)
		{
			sendto_realops
			    ("Negative timestamp recieved from %s, resetting to TStime (%s)",
			    cptr->name, backupbuf);
			sptr->lastnick = TStime();
		}
	}

	(void)strlcpy(sptr->name, nick, NICKLEN);
	(void)add_to_client_hash_table(nick, sptr);

	if (IsServer(cptr) && parc > 7)
	{
		/* XXX: we need to split this out into register_remote_user() or something. */
		parv[3] = nick;
		parv[6] = sptr->name;
		do_cmd(cptr, sptr, "USER", parc - 3, &parv[3]);
		if (GotNetInfo(cptr) && !IsULine(sptr))
			sendto_fconnectnotice(sptr, 0, NULL);
	}

	RunHook(HOOKTYPE_REMOTE_CONNECT, sptr);

	return 0;
}

/*
** m_nick
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
CMD_FUNC(m_nick)
{
	aTKline *tklban;
	int ishold;
	aClient *acptr, *serv = NULL;
	aClient *acptrs;
	char nick[NICKLEN + 2], *s, descbuf[BUFSIZE];
	Membership *mp;
	long lastnick = 0l;
	int  differ = 1, update_watch = 1;
	unsigned char newusr = 0, removemoder = 1;
	Hook *h;
	int i = 0;
	char *nickid = (IsPerson(sptr) && *sptr->id) ? sptr->id : NULL;
	/*
	 * If the user didn't specify a nickname, complain
	 */
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		    me.name, sptr->name);
		return 0;
	}

	if (!IsServer(cptr))
		strlcpy(nick, parv[1], iConf.nicklen + 1);
	else
		strlcpy(nick, parv[1], NICKLEN + 1);

	if (MyConnect(sptr) && sptr->user && !ValidatePermissionsForPath("immune:limits",sptr,NULL,NULL,NULL))
	{
		if ((sptr->user->flood.nick_c >= NICK_COUNT) && 
		    (TStime() - sptr->user->flood.nick_t < NICK_PERIOD))
		{
			/* Throttle... */
			sendto_one(sptr, err_str(ERR_NCHANGETOOFAST), me.name, sptr->name, nick,
				(int)(NICK_PERIOD - (TStime() - sptr->user->flood.nick_t)));
			return 0;
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
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
		    me.name, sptr->name, parv[1], "Illegal characters");

		if (IsServer(cptr))
		{
			ircstp->is_kill++;
			sendto_umode(UMODE_OPER, "Bad Nick: %s From: %s %s",
			    parv[1], sptr->name, get_client_name(cptr, FALSE));
			sendto_one(cptr, ":%s KILL %s :%s (%s <- %s[%s])",
			    me.name, parv[1], me.name, parv[1],
			    nick, cptr->name);
			if (sptr != cptr)
			{	/* bad nick change */
				sendto_server(cptr, 0, 0,
				    ":%s KILL %s :%s (%s <- %s!%s@%s)",
				    me.name, sptr->name, me.name,
				    get_client_name(cptr, FALSE),
				    sptr->name,
				    sptr->user ? sptr->username : "",
				    sptr->user ? sptr->user->server :
				    cptr->name);
				sptr->flags |= FLAGS_KILLED;
				return exit_client(cptr, sptr, &me, "BadNick");
			}
		}
		return 0;
	}

	/* Kill quarantined opers early... */
	if (IsServer(cptr) && (sptr->from->flags & FLAGS_QUARANTINE) &&
	    (parc >= 11) && strchr(parv[8], 'o'))
	{
		ircstp->is_kill++;
		/* Send kill to uplink only, hasn't been broadcasted to the rest, anyway */
		sendto_one(cptr, ":%s KILL %s :%s (Quarantined: no oper privileges allowed)",
			me.name, parv[1], me.name);
		sendto_realops("QUARANTINE: Oper %s on server %s killed, due to quarantine",
			parv[1], sptr->name);
		/* (nothing to exit_client or to free, since user was never added) */
		return 0;
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
	    && (!(serv = (aClient *)find_server(parv[6], NULL))
	    || serv->from != cptr->from)))
	{
		sendto_realops("Cannot find server %s (%s)", parv[6],
		    backupbuf);
		return 0;
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
		if (MyConnect(sptr))
		{
			sendto_one(sptr, err_str(ERR_NICKNAMEINUSE), me.name,
			    *sptr->name ? sptr->name : "*", nick);
			return 0;	/* NICK message ignored */
		}
	}

	/*
	   ** Check for a Q-lined nickname. If we find it, and it's our
	   ** client, just reject it. -Lefler
	   ** Allow opers to use Q-lined nicknames. -Russell
	 */
	if (!stricmp("ircd", nick) || !stricmp("irc", nick))
	{
		sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME), me.name,
		    *sptr->name ? sptr->name : "*", nick,
		    "Reserved for internal IRCd purposes");
		return 0;
	}
	if (MyClient(sptr)) /* local client changin nick afterwards.. */
	{
		int xx;
		spamfilter_build_user_string(spamfilter_user, nick, sptr);
		xx = dospamfilter(sptr, spamfilter_user, SPAMF_USER, NULL, 0, NULL);
		if (xx < 0)
			return xx;
	}
	if (!IsULine(sptr) && (tklban = find_qline(sptr, nick, &ishold)))
	{
		if (IsServer(sptr) && !ishold) /* server introducing new client */
		{
			acptrs =
			    (aClient *)find_server(sptr->user ==
			    NULL ? (char *)parv[6] : (char *)sptr->user->
			    server, NULL);
			/* (NEW: no unregistered q:line msgs anymore during linking) */
			if (!acptrs || (acptrs->serv && acptrs->serv->flags.synced))
				sendto_snomask(SNO_QLINE, "Q:lined nick %s from %s on %s", nick,
				    (*sptr->name != 0
				    && !IsServer(sptr) ? sptr->name : "<unregistered>"),
				    acptrs ? acptrs->name : "unknown server");
		}
		
		if (IsServer(cptr) && IsPerson(sptr) && !ishold) /* remote user changing nick */
		{
			sendto_snomask(SNO_QLINE, "Q:lined nick %s from %s on %s", nick,
				sptr->name, sptr->srvptr ? sptr->srvptr->name : "<unknown>");
		}

		if (!IsServer(cptr)) /* local */
		{
			if (ishold)
			{
				sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
				    me.name, *sptr->name ? sptr->name : "*",
				    nick, tklban->reason);
				return 0;
			}
			if (!ValidatePermissionsForPath("override:nick:qline",sptr,NULL,NULL,nick))
			{
				sptr->local->since += 4; /* lag them up */
				sendto_one(sptr, err_str(ERR_ERRONEUSNICKNAME),
				    me.name, *sptr->name ? sptr->name : "*",
				    nick, tklban->reason);
				sendto_snomask(SNO_QLINE, "Forbidding Q-lined nick %s from %s.",
				    nick, get_client_name(cptr, FALSE));
				return 0;	/* NICK message ignored */
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
		    sptr->name, acptr->from->name,
		    get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(cptr, ":%s KILL %s :%s (%s <- %s)",
		    me.name, sptr->name, me.name, acptr->from->name,
		    /* NOTE: Cannot use get_client_name
		       ** twice here, it returns static
		       ** string pointer--the other info
		       ** would be lost
		     */
		    get_client_name(cptr, FALSE));
		sptr->flags |= FLAGS_KILLED;
		return exit_client(cptr, sptr, &me, "Nick/Server collision");
	}

	if (MyClient(cptr) && ValidatePermissionsForPath("override:nick:flood",sptr,NULL,NULL,NULL))
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
			return 0;
		acptr->flags |= FLAGS_KILLED;
		exit_client(NULL, acptr, &me, "Overridden");
		goto nickkilldone;
	}
	/* A sanity check in the user field... */
	if (acptr->user == NULL)
	{
		/* This is a Bad Thing */
		sendto_umode(UMODE_OPER, "Lost user field for %s in change from %s",
		    acptr->name, get_client_name(cptr, FALSE));
		ircstp->is_kill++;
		sendto_one(acptr, ":%s KILL %s :%s (Lost user field!)",
		    me.name, acptr->name, me.name);
		acptr->flags |= FLAGS_KILLED;
		/* Here's the previous versions' desynch.  If the old one is
		   messed up, trash the old one and accept the new one.
		   Remember - at this point there is a new nick coming in!
		   Handle appropriately. -- Barubary */
		exit_client(NULL, acptr, &me, "Lost user field");
		goto nickkilldone;
	}
	/*
	   ** If acptr == sptr, then we have a client doing a nick
	   ** change between *equivalent* nicknames as far as server
	   ** is concerned (user is changing the case of their
	   ** nickname or somesuch)
	 */
	if (acptr == sptr) {
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
			return 0;	/* NICK Message ignored */
	}
	/*
	   ** Note: From this point forward it can be assumed that
	   ** acptr != sptr (point to different client structures).
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
		sendto_one(sptr, err_str(ERR_NICKNAMEINUSE),
		    me.name, *sptr->name ? sptr->name : "*", nick);
		return 0;	/* NICK message ignored */
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
	if (IsServer(sptr))
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
		sendto_umode(UMODE_OPER, "Nick collision on %s (%s %ld <- %s %ld)",
		    acptr->name, acptr->from->name, acptr->lastnick,
		    cptr->name, lastnick);
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
			nick_collision(cptr, parv[1], nickid, NULL, acptr, NICKCOL_EQUAL);
			return 0; /* We killed both users, now stop the process. */
		}

		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)) || acptr->from == cptr)	/* we missed a QUIT somewhere ? */
		{
			nick_collision(cptr, parv[1], nickid, NULL, acptr, NICKCOL_NEW_WON);
			/* OK, we got rid of the "wrong" user, now we're going to add the
			 * user the other server introduced.
			 */
			goto nickkilldone;
		}

		if ((differ && (acptr->lastnick < lastnick)) ||
		    (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(cptr, parv[1], nickid, NULL, acptr, NICKCOL_EXISTING_WON);
			return 0; /* Ignore the NICK. */
		}
		return 0;
	}
	else
	{
		/*
		   ** A NICK change has collided (e.g. message type ":old NICK new").
		 */
		if (parc > 2)
			lastnick = atol(parv[2]);
		differ = (mycmp(acptr->user->username, sptr->user->username) ||
		    mycmp(acptr->user->realhost, sptr->user->realhost));
		sendto_umode(UMODE_OPER, "Nick change collision from %s to %s (%s %ld <- %s %ld)",
		    sptr->name, acptr->name, acptr->from->name, acptr->lastnick,
		    sptr->from->name, lastnick);
		if (!(parc > 2) || lastnick == acptr->lastnick)
		{
			nick_collision(cptr, parv[1], nickid, sptr, acptr, NICKCOL_EQUAL);
			return 0; /* Now that I killed them both, ignore the NICK */
		}
		if ((differ && (acptr->lastnick > lastnick)) ||
		    (!differ && (acptr->lastnick < lastnick)))
		{
			nick_collision(cptr, parv[1], nickid, sptr, acptr, NICKCOL_NEW_WON);
			goto nickkilldone;	/* their user won, introduce new nick */
		}
		if ((differ && (acptr->lastnick < lastnick)) ||
		    (!differ && (acptr->lastnick > lastnick)))
		{
			nick_collision(cptr, parv[1], nickid, sptr, acptr, NICKCOL_EXISTING_WON);
			return 0; /* their user lost, ignore the NICK */
		}

	}
	return 0;		/* just in case */
      nickkilldone:
	if (IsServer(sptr))
	{
		/* A server introducing a new client, change source */

		sptr = make_client(cptr, serv);
		add_client_to_list(sptr);
		if (parc > 2)
			sptr->hopcount = atol(parv[2]);
		if (parc > 3)
			sptr->lastnick = atol(parv[3]);
		else		/* Little bit better, as long as not all upgraded */
			sptr->lastnick = TStime();
		if (sptr->lastnick < 0)
		{
			sendto_realops
			    ("Negative timestamp recieved from %s, resetting to TStime (%s)",
			    cptr->name, backupbuf);
			sptr->lastnick = TStime();
		}
		newusr = 1;
	}
	else if (sptr->name[0] && IsPerson(sptr))
	{
		/*
		   ** If the client belongs to me, then check to see
		   ** if client is currently on any channels where it
		   ** is currently banned.  If so, do not allow the nick
		   ** change to occur.
		   ** Also set 'lastnick' to current time, if changed.
		 */
		if (MyClient(sptr))
		{
			for (mp = sptr->user->channel; mp; mp = mp->next)
			{
				if (!is_skochanop(sptr, mp->chptr) && is_banned(sptr, mp->chptr, BANCHK_NICK))
				{
					sendto_one(sptr,
					    err_str(ERR_BANNICKCHANGE),
					    me.name, sptr->name,
					    mp->chptr->chname);
					return 0;
				}
				if (CHECK_TARGET_NICK_BANS && !is_skochanop(sptr, mp->chptr) && is_banned_with_nick(sptr, mp->chptr, BANCHK_NICK, nick))
				{
					sendto_one(sptr,
					    ":%s 437 %s %s :Cannot change to a nickname banned on channel",
					    me.name, sptr->name,
					    mp->chptr->chname);
					return 0;
				}

				for (h = Hooks[HOOKTYPE_CHAN_PERMIT_NICK_CHANGE]; h; h = h->next)
				{
					i = (*(h->func.intfunc))(sptr,mp->chptr);
					if (i != HOOK_CONTINUE)
						break;
				}

				if (i == HOOK_DENY)
				{
					sendto_one(sptr,
					    err_str(ERR_NONICKCHANGE),
					    me.name, sptr->name,
					    mp->chptr->chname);
					return 0;
				}
			}

			if (TStime() - sptr->user->flood.nick_t >= NICK_PERIOD)
			{
				sptr->user->flood.nick_t = TStime();
				sptr->user->flood.nick_c = 1;
			} else
				sptr->user->flood.nick_c++;

			sendto_snomask(SNO_NICKCHANGE, "*** %s (%s@%s) has changed their nickname to %s",
				sptr->name, sptr->user->username, sptr->user->realhost, nick);

			RunHook2(HOOKTYPE_LOCAL_NICKCHANGE, sptr, nick);
		} else {
			if (!IsULine(sptr))
				sendto_snomask(SNO_FNICKCHANGE, "*** %s (%s@%s) has changed their nickname to %s",
					sptr->name, sptr->user->username, sptr->user->realhost, nick);

			RunHook3(HOOKTYPE_REMOTE_NICKCHANGE, cptr, sptr, nick);
		}
		/*
		 * Client just changing their nick. If he/she is
		 * on a channel, send note of change to all clients
		 * on that channel. Propagate notice to other servers.
		 */
		if (mycmp(sptr->name, nick) ||
		    /* Next line can be removed when all upgraded  --Run */
		    (!MyClient(sptr) && parc > 2
		    && atol(parv[2]) < sptr->lastnick))
			sptr->lastnick = (MyClient(sptr)
			    || parc < 3) ? TStime() : atol(parv[2]);
		if (sptr->lastnick < 0)
		{
			sendto_realops("Negative timestamp (%s)", backupbuf);
			sptr->lastnick = TStime();
		}
		add_history(sptr, 1);
		sendto_server(cptr, PROTO_SID, 0, ":%s NICK %s %ld",
		    ID(sptr), nick, sptr->lastnick);
		sendto_server(cptr, 0, PROTO_SID, ":%s NICK %s %ld",
		    sptr->name, nick, sptr->lastnick);
		sendto_common_channels(sptr, ":%s NICK :%s", sptr->name, nick);
		if (removemoder)
			sptr->umodes &= ~UMODE_REGNICK;
	}
	else if (!sptr->name[0])
	{
		if (iConf.ping_cookie)
		{
			/*
			 * Client setting NICK the first time.
			 *
			 * Generate a random string for them to pong with.
			 */
			sptr->local->nospoof = getrandom32();

			sendto_one(sptr, "PING :%X", sptr->local->nospoof);
		}

#ifdef CONTACT_EMAIL
		sendto_one(sptr,
		    ":%s NOTICE %s :*** If you need assistance with a"
		    " connection problem, please email " CONTACT_EMAIL
		    " with the name and version of the client you are"
		    " using, and the server you tried to connect to: %s",
		    me.name, nick, me.name);
#endif /* CONTACT_EMAIL */
#ifdef CONTACT_URL
		sendto_one(sptr,
		    ":%s NOTICE %s :*** If you need assistance with"
		    " connecting to this server, %s, please refer to: "
		    CONTACT_URL, me.name, nick, me.name);
#endif /* CONTACT_URL */

		/* Copy password to the passwd field if it's given after NICK
		 * - originally by taz, modified by Wizzu
		 */
		if ((parc > 2) && (strlen(parv[2]) <= PASSWDLEN))
		{
			safefree(sptr->local->passwd);
			sptr->local->passwd = strdup(parv[2]);
		}
		/* This had to be copied here to avoid problems.. */
		strlcpy(sptr->name, nick, sizeof(sptr->name));
		if (sptr->user && *sptr->user->username && IsNotSpoof(sptr) && !CHECKPROTO(sptr, PROTO_CLICAP))
		{
			/*
			   ** USER already received, now we have NICK.
			   ** *NOTE* For servers "NICK" *must* precede the
			   ** user message (giving USER before NICK is possible
			   ** only for local client connection!). register_user
			   ** may reject the client and call exit_client for it
			   ** --must test this and exit m_nick too!!!
			 */
			if (!iConf.ping_cookie && USE_BAN_VERSION && MyConnect(sptr))
				sendto_one(sptr, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
					me.name, nick);

			sptr->lastnick = TStime();	/* Always local client */
			if (register_user(cptr, sptr, nick,
			    sptr->user->username, NULL, NULL, NULL) == FLUSH_BUFFER)
				return FLUSH_BUFFER;
			strlcpy(nick, sptr->name, sizeof(nick)); /* don't ask, but I need this. do not remove! -- Syzop */
			update_watch = 0;
			newusr = 1;
		}
	}
	/*
	 *  Finally set new nick name.
	 */
	if (update_watch && sptr->name[0])
	{
		(void)del_from_client_hash_table(sptr->name, sptr);
		if (IsPerson(sptr))
			hash_check_watch(sptr, RPL_LOGOFF);
	}

	/* update fdlist --nenolod */
	if (MyConnect(sptr))
	{
		snprintf(descbuf, sizeof descbuf, "Client: %s", nick);
		fd_desc(sptr->fd, descbuf);
	}

	(void)strcpy(sptr->name, nick);
	(void)add_to_client_hash_table(nick, sptr);
	if (IsServer(cptr) && parc > 7)
	{
		parv[3] = nick;
		do_cmd(cptr, sptr, "USER", parc - 3, &parv[3]);
		if (GotNetInfo(cptr) && !IsULine(sptr))
			sendto_fconnectnotice(sptr, 0, NULL);
	}
	else if (IsPerson(sptr) && update_watch)
		hash_check_watch(sptr, RPL_LOGON);

	if (newusr && !MyClient(sptr) && IsPerson(sptr))
	{
		RunHook(HOOKTYPE_REMOTE_CONNECT, sptr);
	}

	return 0;
}

/*
** register_user
**	This function is called when both NICK and USER messages
**	have been accepted for the client, in whatever order. Only
**	after this the USER message is propagated.
**
**	NICK's must be propagated at once when received, although
**	it would be better to delay them too until full info is
**	available. Doing it is not so simple though, would have
**	to implement the following:
**
**	1) user telnets in and gives only "NICK foobar" and waits
**	2) another user far away logs in normally with the nick
**	   "foobar" (quite legal, as this server didn't propagate
**	   it).
**	3) now this server gets nick "foobar" from outside, but
**	   has already the same defined locally. Current server
**	   would just issue "KILL foobar" to clean out dups. But,
**	   this is not fair. It should actually request another
**	   nick from local user or kill him/her...
*/
extern MODVAR char cmodestring[512];
extern MODVAR char umodestring[UMODETABLESZ+1];

extern int short_motd(aClient *sptr);

int _register_user(aClient *cptr, aClient *sptr, char *nick, char *username, char *umode, char *virthost, char *ip)
{
	ConfigItem_ban *bconf;
	char *tmpstr;
	const char *id;
#ifdef HOSTILENAME
	char stripuser[USERLEN + 1], *u1 = stripuser, *u2, olduser[USERLEN + 1],
	    userbad[USERLEN * 2 + 1], *ubad = userbad, noident = 0;
#endif
	int  xx;
	anUser *user = sptr->user;
	aClient *nsptr;
	int  i;
	char mo[256];
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
	aTKline *savetkl = NULL;
	ConfigItem_tld *tlds;
	cptr->local->last = TStime();
	nick = sptr->name; /* <- The data is always the same, but the pointer is sometimes not,
	                    *    I need this for one of my modules, so do not remove! ;) -- Syzop */
	
	if (MyConnect(sptr))
	{
	        char temp[USERLEN + 1];
	        
		if ((i = check_client(sptr, username))) {
			/* This had return i; before -McSkaf */
			if (i == -5)
				return FLUSH_BUFFER;

			sendto_snomask(SNO_CLIENT,
			    "*** %s from %s.",
			    i == -3 ? "Too many connections" :
			    "Unauthorized connection", get_client_host(sptr));
			ircstp->is_ref++;
			ircsnprintf(mo, sizeof(mo), "This server is full.");
			return
			    exit_client(cptr, sptr, &me,
			    i ==
			    -3 ? mo :
			    "You are not authorized to connect to this server");
		}

		if (sptr->local->hostp)
		{
			/* reject ASCII < 32 and ASCII >= 127 (note: upper resolver might be even more strict). */
			for (tmpstr = sptr->local->sockhost; *tmpstr > ' ' && *tmpstr < 127; tmpstr++);
			
			/* if host contained invalid ASCII _OR_ the DNS reply is an IP-like reply
			 * (like: 1.2.3.4 or ::ffff:1.2.3.4), then reject it and use IP instead.
			 */
			if (*tmpstr || !*user->realhost || (isdigit(*sptr->local->sockhost) && (sptr->local->sockhost > tmpstr && isdigit(*(tmpstr - 1))) )
			    || (sptr->local->sockhost[0] == ':'))
				strlcpy(sptr->local->sockhost, sptr->ip, sizeof(sptr->local->sockhost));
		}
		if (sptr->local->sockhost[0])
		{
			strlcpy(user->realhost, sptr->local->sockhost, sizeof(sptr->local->sockhost)); /* SET HOSTNAME */
		} else {
			sendto_realops("[HOSTNAME BUG] sptr->local->sockhost is empty for user %s (%s, %s)",
				sptr->name, sptr->ip ? sptr->ip : "<null>", user->realhost);
			ircd_log(LOG_ERROR, "[HOSTNAME BUG] sptr->local->sockhost is empty for user %s (%s, %s)",
				sptr->name, sptr->ip ? sptr->ip : "<null>", user->realhost);
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
		 * !strchr(sptr->username, '@') check is out of date. -Cabal95
		 *
		 * Moved the noident stuff here. -OnyxDragon
		 */

		/* because username may point to user->username */
		strlcpy(temp, username, USERLEN + 1);

		if (!(sptr->flags & FLAGS_DOID)) 
			strlcpy(user->username, temp, USERLEN + 1);
		else if (sptr->flags & FLAGS_GOTID) 
			strlcpy(user->username, sptr->username, USERLEN+1);
		else
		{
			if (IDENT_CHECK == 0) {
				strlcpy(user->username, temp, USERLEN+1);
			}
			else {
				*user->username = '~';
				strlcpy((user->username + 1), temp, sizeof(user->username)-1);
#ifdef HOSTILENAME
				noident = 1;
#endif
			}

		}
#ifdef HOSTILENAME
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
				return exit_client(cptr, cptr, cptr, "Hostile username. Please use only 0-9 a-z A-Z _ - and . in your username.");
			}

			strlcpy(olduser, user->username + noident, USERLEN+1);
			strlcpy(user->username + 1, stripuser, sizeof(user->username)-1);
			user->username[0] = '~';
			user->username[USERLEN] = '\0';
		}
		else
			u1 = NULL;
#endif

		/*
		 * following block for the benefit of time-dependent K:-lines
		 */
		if ((bconf = Find_ban(sptr, NULL, CONF_BAN_USER)))
		{
			ircstp->is_ref++;
			sendto_one(cptr,
			    ":%s %d %s :*** You are not welcome on this server (%s)"
			    " Email %s for more information.",
			    me.name, ERR_YOUREBANNEDCREEP,
			    cptr->name, bconf->reason ? bconf->reason : "",
			    KLINE_ADDRESS);
			return exit_client(cptr, cptr, cptr, "You are banned");
		}
		if ((bconf = Find_ban(NULL, sptr->info, CONF_BAN_REALNAME)))
		{
			ircstp->is_ref++;
			sendto_one(cptr,
			    ":%s %d %s :*** Your GECOS (real name) is not allowed on this server (%s)"
			    " Please change it and reconnect",
			    me.name, ERR_YOUREBANNEDCREEP,
			    cptr->name, bconf->reason ? bconf->reason : "");

			return exit_client(cptr, sptr, &me,
			    "Your GECOS (real name) is banned from this server");
		}
		tkl_check_expire(NULL);
		/* Check G/Z lines before shuns -- kill before quite -- codemastr */
		if ((xx = find_tkline_match(sptr, 0)) < 0)
		{
			ircstp->is_ref++;
			return xx;
		}
		find_shun(sptr);

		/* Technical note regarding next few lines of code:
		 * If the spamfilter matches, depending on the action:
		 *  If it's block/dccblock/whatever the retval is -1 ===> we return, client stays "locked forever".
		 *  If it's kill/tklline the retval is -2 ==> we return with -2 (aka: FLUSH_BUFFER)
		 *  If it's action is viruschan the retval is -5 ==> we continue, and at the end of this return
		 *    take special actions. We cannot do that directly here since the user is not fully registered
		 *    yet (at all).
		 *  -- Syzop
		 */
		spamfilter_build_user_string(spamfilter_user, sptr->name, sptr);
		xx = dospamfilter(sptr, spamfilter_user, SPAMF_USER, NULL, 0, &savetkl);
		if ((xx < 0) && (xx != -5))
			return xx;

		RunHookReturnInt(HOOKTYPE_PRE_LOCAL_CONNECT, sptr, !=0);
	}
	else
	{
		strlcpy(user->username, username, USERLEN+1);
	}
	SetClient(sptr);
	IRCstats.clients++;
	if (sptr->srvptr && sptr->srvptr->serv)
		sptr->srvptr->serv->users++;

	make_virthost(sptr, user->realhost, user->cloakedhost, 0);
	user->virthost = strdup(user->cloakedhost);

	if (MyConnect(sptr))
	{
		char descbuf[BUFSIZE];
		int i;

		snprintf(descbuf, sizeof descbuf, "Client: %s", nick);
		fd_desc(sptr->fd, descbuf);

		list_move(&sptr->lclient_node, &lclient_list);

		while (hash_find_id((id = uid_get()), NULL) != NULL)
			;
		strlcpy(sptr->id, id, sizeof sptr->id);
		(void)add_to_id_hash_table(sptr->id, sptr);

		IRCstats.unknown--;
		IRCstats.me_clients++;
		if (IsHidden(sptr))
			ircd_log(LOG_CLIENT, "Connect - %s!%s@%s [VHOST %s]", nick,
				user->username, user->realhost, user->virthost);
		else
			ircd_log(LOG_CLIENT, "Connect - %s!%s@%s", nick, user->username,
				user->realhost);
		sendto_one(sptr, rpl_str(RPL_WELCOME), me.name, nick,
		    ircnetwork, nick, user->username, user->realhost);
		/* This is a duplicate of the NOTICE but see below... */
			sendto_one(sptr, rpl_str(RPL_YOURHOST), me.name, nick,
			    me.name, version);
		sendto_one(sptr, rpl_str(RPL_CREATED), me.name, nick, creation);
		sendto_one(sptr, rpl_str(RPL_MYINFO), me.name, sptr->name,
		    me.name, version, umodestring, cmodestring);

		for (i = 0; IsupportStrings[i]; i++)
			sendto_one(sptr, rpl_str(RPL_ISUPPORT), me.name, nick, IsupportStrings[i]);

		if (IsHidden(sptr))
			sendto_one(sptr, err_str(RPL_HOSTHIDDEN), me.name, sptr->name, user->virthost);

		if (sptr->flags & FLAGS_SSL)
		{
			if (sptr->local->ssl)
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** You are connected to %s with %s",
				    me.name, sptr->name, me.name,
				    ssl_get_cipher(sptr->local->ssl));
			}
		}
		
		{
			char *parv[2];
			parv[0] = sptr->name;
			parv[1] = NULL;
			do_cmd(sptr, sptr, "LUSERS", 1, parv);
		}
		
		short_motd(sptr);

#ifdef EXPERIMENTAL
		sendto_one(sptr,
		    ":%s NOTICE %s :*** \2NOTE:\2 This server is running experimental IRC server software (UnrealIRCd %s). If you find any bugs or problems, please report them at http://bugs.unrealircd.org/",
		    me.name, sptr->name, VERSIONONLY);
#endif
#ifdef HOSTILENAME
		/*
		 * Now send a numeric to the user telling them what, if
		 * anything, happened.
		 */
		if (u1)
			sendto_one(sptr, err_str(ERR_HOSTILENAME), me.name,
			    sptr->name, olduser, userbad, stripuser);
#endif
		if (IsSecure(sptr))
			sptr->umodes |= UMODE_SECURE;
	}
	else if (IsServer(cptr))
	{
		aClient *acptr;

		if (!(acptr = (aClient *)find_server_quick(user->server)))
		{
			sendto_ops
			    ("Bad USER [%s] :%s USER %s %s : No such server",
			    cptr->name, nick, user->username, user->server);
			sendto_one(cptr, ":%s KILL %s :%s (No such server: %s)",
			    me.name, sptr->name, me.name, user->server);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(sptr, sptr, &me,
			    "USER without prefix(2.8) or wrong prefix");
		}
		else if (acptr->from != sptr->from)
		{
			sendto_ops("Bad User [%s] :%s USER %s %s, != %s[%s]",
			    cptr->name, nick, user->username, user->server,
			    acptr->name, acptr->from->name);
			sendto_one(cptr, ":%s KILL %s :%s (%s != %s[%s])",
			    me.name, sptr->name, me.name, user->server,
			    acptr->from->name, acptr->from->local->sockhost);
			sptr->flags |= FLAGS_KILLED;
			return exit_client(sptr, sptr, &me,
			    "USER server wrong direction");
		}
		else
			sptr->flags |= acptr->flags;
		/* *FINALL* this gets in ircd... -- Barubary */
		/* We change this a bit .. */
		if (IsULine(sptr->srvptr))
			sptr->flags |= FLAGS_ULINE;
	}
	if (sptr->umodes & UMODE_INVISIBLE)
	{
		IRCstats.invisible++;
	}

	if (virthost && umode)
	{
		tkllayer[0] = nick;
		tkllayer[1] = nick;
		tkllayer[2] = umode;
		dontspread = 1;
		do_cmd(cptr, sptr, "MODE", 3, tkllayer);
		dontspread = 0;
		if (virthost && *virthost != '*')
		{
			if (sptr->user->virthost)
			{
				MyFree(sptr->user->virthost);
				sptr->user->virthost = NULL;
			}
			/* Here pig.. yeah you .. -Stskeeps */
			sptr->user->virthost = strdup(virthost);
		}
		if (ip && (*ip != '*'))
		{
			char *ipstring = decode_ip(ip);
			if (!ipstring)
			{
				sendto_ops("USER with invalid IP (%s) (%s) -- "
				           "IP must be base64 encoded binary representation of either IPv4 or IPv6",
				           sptr->name, ip);
				return exit_client(sptr, sptr, &me, "USER with invalid IP");
			}
			sptr->ip = strdup(ipstring);
		}
	}

	hash_check_watch(sptr, RPL_LOGON);	/* Uglier hack */
	send_umode(NULL, sptr, 0, SEND_UMODES|UMODE_SERVNOTICE, buf);

	/* NICKv2 Servers ! */
	sendto_serv_butone_nickcmd(cptr, sptr, (*buf == '\0' ? "+" : buf));

	if (MyConnect(sptr))
	{
		broadcast_moddata_client(sptr);
		sendto_connectnotice(sptr, 0, NULL); /* moved down, for modules. */

		/* Send password from sptr->local->passwd to NickServ for identification,
		 * if passwd given and if NickServ is online.
		 * - by taz, modified by Wizzu
		 */
		if (sptr->local->passwd && (nsptr = find_person(NickServ, NULL)))
		{
			int do_identify = 1;
			Hook *h;
			for (h = Hooks[HOOKTYPE_LOCAL_NICKPASS]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(sptr,nsptr);
				if (i == HOOK_DENY)
				{
					do_identify = 0;
					break;
				}
			}
			if (do_identify)
				sendto_one(nsptr, ":%s PRIVMSG %s@%s :IDENTIFY %s",
				    sptr->name,
				    NickServ, SERVICES_NAME, sptr->local->passwd);
		}
		if (buf[0] != '\0' && buf[1] != '\0')
			sendto_one(cptr, ":%s MODE %s :%s", cptr->name,
			    cptr->name, buf);
		if (user->snomask)
			sendto_one(sptr, rpl_str(RPL_SNOMASK),
				me.name, sptr->name, get_snostr(user->snomask));
		
		/* Make creation time the real 'online since' time, excluding registration time.
		 * Otherwise things like set::anti-spam-quit-messagetime 10s could mean
		 * 1 second in practice (#2174).
		 */
		sptr->local->firsttime = TStime();
		
		/* Give the user a fresh start as far as fake-lag is concerned.
		 * Otherwise the user could be lagged up already due to all the CAP stuff.
		 */
		sptr->local->since = TStime();

		/* NOTE: Code after this 'if (savetkl)' will not be executed for quarantined-
		 *       virus-users. So be carefull with the order. -- Syzop
		 */
		if (savetkl)
			return dospamfilter_viruschan(sptr, savetkl, SPAMF_USER); /* [RETURN!] */

		/* Force the user to join the given chans -- codemastr */
		tlds = Find_tld(sptr);

		if (tlds && !BadPtr(tlds->channel)) {
			char *chans[3] = {
				sptr->name,
				tlds->channel,
				NULL
			};
			do_cmd(sptr, sptr, "JOIN", 3, chans);
		}
		else if (!BadPtr(AUTO_JOIN_CHANS) && strcmp(AUTO_JOIN_CHANS, "0"))
		{
			char *chans[3] = {
				sptr->name,
				AUTO_JOIN_CHANS,
				NULL
			};
			do_cmd(sptr, sptr, "JOIN", 3, chans);
		}
		/* NOTE: If you add something here.. be sure to check the 'if (savetkl)' note above */
	}

	if (MyConnect(sptr) && !BadPtr(sptr->local->passwd))
	{
		MyFree(sptr->local->passwd);
		sptr->local->passwd = NULL;
	}
	return 0;
}

