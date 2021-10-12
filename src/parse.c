/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/parse.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/** @file
 * @brief Main line parsing functions - for incoming lines from clients.
 */
#include "unrealircd.h"

/** Last (or current) command that we processed. Useful for post-mortem. */
char backupbuf[8192];

static char *para[MAXPARA + 2];

/* Forward declarations of functions that are local (static) */
static int do_numeric(int, Client *, MessageTag *, int, char **);
static void cancel_clients(Client *, Client *, char *);
static void remove_unknown(Client *, char *);
static void parse2(Client *client, Client **fromptr, MessageTag *mtags, char *ch);
static void parse_addlag(Client *client, int cmdbytes);
static int client_lagged_up(Client *client);
static void ban_handshake_data_flooder(Client *client);

/** Put a packet in the client receive queue and process the data (if
 * the 'fake lag' rules permit doing so).
 * @param client      The client
 * @param readbuf     The read buffer
 * @param length      The length of the data
 * @param killsafely  If 1 then we may call exit_client() if the client
 *                    is flooding. If 0 then we use dead_socket().
 * @returns 1 in normal circumstances, 0 if client was killed.
 * @note  If killsafely is 1 and the return value is 0 then
 *        the client was killed - IsDead() is true.
 *        If this is a problem, then set killsafely to 0 when calling.
 */
int process_packet(Client *client, char *readbuf, int length, int killsafely)
{
	dbuf_put(&client->local->recvQ, readbuf, length);

	/* parse some of what we have (inducing fakelag, etc) */
	parse_client_queued(client);

	/* We may be killed now, so check for it.. */
	if (IsDead(client))
		return 0;

	/* flood from unknown connection */
	if (IsUnknown(client) && (DBufLength(&client->local->recvQ) > iConf.handshake_data_flood_amount))
	{
		sendto_snomask(SNO_FLOOD, "Handshake data flood from %s detected", client->local->sockhost);
		if (!killsafely)
			ban_handshake_data_flooder(client);
		else
			dead_socket(client, "Handshake data flood detected");
		return 0;
	}

	/* excess flood check */
	if (IsUser(client) && DBufLength(&client->local->recvQ) > get_recvq(client))
	{
		sendto_snomask(SNO_FLOOD,
			"*** Flood -- %s!%s@%s (%d) exceeds %d recvQ",
			client->name[0] ? client->name : "*",
			client->user ? client->user->username : "*",
			client->user ? client->user->realhost : "*",
			DBufLength(&client->local->recvQ), get_recvq(client));
		if (!killsafely)
			exit_client(client, NULL, "Excess Flood");
		else
			dead_socket(client, "Excess Flood");
		return 0;
	}

	return 1;
}

/** Parse any queued data for 'client', if permitted.
 * @param client	The client.
 */
void parse_client_queued(Client *client)
{
	int dolen = 0;
	char buf[READBUFSIZE];

	if (IsDNSLookup(client))
		return; /* we delay processing of data until the host is resolved */

	if (IsIdentLookup(client))
		return; /* we delay processing of data until identd has replied */

	if (!IsUser(client) && !IsServer(client) && (iConf.handshake_delay > 0) &&
	    !IsNoHandshakeDelay(client) && (TStime() - client->local->firsttime < iConf.handshake_delay))
	{
		return; /* we delay processing of data until set::handshake-delay is reached */
	}

	while (DBufLength(&client->local->recvQ) && !client_lagged_up(client))
	{
		dolen = dbuf_getmsg(&client->local->recvQ, buf);

		if (dolen == 0)
			return;

		dopacket(client, buf, dolen);
		
		if (IsDead(client))
			return;
	}
}

/*
** dopacket
**	client - pointer to client structure for which the buffer data
**	       applies.
**	buffer - pointr to the buffer containing the newly read data
**	length - number of valid bytes of data in the buffer
**
** Note:
**	It is implicitly assumed that dopacket is called only
**	with client of "local" variation, which contains all the
**	necessary fields (buffer etc..)
**
** Rewritten for linebufs, 19th May 2013. --kaniini
*/
void dopacket(Client *client, char *buffer, int length)
{
	me.local->receiveB += length;	/* Update bytes received */
	client->local->receiveB += length;
	if (client->local->receiveB > 1023)
	{
		client->local->receiveK += (client->local->receiveB >> 10);
		client->local->receiveB &= 0x03ff;	/* 2^10 = 1024, 3ff = 1023 */
	}
	if (me.local->receiveB > 1023)
	{
		me.local->receiveK += (me.local->receiveB >> 10);
		me.local->receiveB &= 0x03ff;
	}

	me.local->receiveM += 1;	/* Update messages received */
	client->local->receiveM += 1;

	parse(client, buffer, length);
}


/** Parse an incoming line.
 * A line was received previously, buffered via dbuf, now popped from the dbuf stack,
 * and we should now process it.
 * @param cptr    The client from which the message was received
 * @param buffer  The buffer
 * @param length  The length of the buffer
 * @note parse() cannot not be called recusively by any other functions!
 */
void parse(Client *cptr, char *buffer, int length)
{
	Hook *h;
	Client *from = cptr;
	char *ch;
	int i, ret;
	MessageTag *mtags = NULL;

	/* Take extreme care in this function, as messages can be up to READBUFSIZE
	 * in size, which is 8192 at the time of writing.
	 * This, while all the rest of the IRCd code assumes a maximum length
	 * of BUFSIZE, which is 512 (including NUL byte).
	 */
	for (h = Hooks[HOOKTYPE_PACKET]; h; h = h->next)
	{
		(*(h->func.intfunc))(from, &me, NULL, &buffer, &length);
		if(!buffer)
			return;
	}

	Debug((DEBUG_ERROR, "Parsing: %s (from %s)", buffer, (*cptr->name ? cptr->name : "*")));

	if (IsDeadSocket(cptr))
		return;

	if ((cptr->local->receiveK >= iConf.handshake_data_flood_amount/1024) && IsUnknown(cptr))
	{
		sendto_snomask(SNO_FLOOD, "Handshake data flood from %s detected", cptr->local->sockhost);
		ban_handshake_data_flooder(cptr);
		return;
	}

	/* This stores the last executed command in 'backupbuf', useful for debugging crashes */
	strlcpy(backupbuf, buffer, sizeof(backupbuf));

#if defined(DEBUGMODE) && defined(RAWCMDLOGGING)
	ircd_log(LOG_ERROR, "<- %s: %s", cptr->name, backupbuf);
#endif

	/* This poisons unused para elements that code should never access */
	for (i = 0; i < MAXPARA+2; i++)
		para[i] = (char *)DEADBEEF_ADDR;

	/* First, skip any whitespace */
	for (ch = buffer; *ch == ' '; ch++)
		;

	/* Now, parse message tags, if any */
	if (*ch == '@')
	{
		parse_message_tags(cptr, &ch, &mtags);
		/* Skip whitespace again */
		for (; *ch == ' '; ch++)
			;
	}

	parse2(cptr, &from, mtags, ch);

	if (IsDead(cptr))
		RunHook3(HOOKTYPE_POST_COMMAND, NULL, mtags, ch);
	else
		RunHook3(HOOKTYPE_POST_COMMAND, from, mtags, ch);

	free_message_tags(mtags);
	return;
}

/** Parse the remaining line - helper function for parse().
 * @param cptr   The client from which the message was received
 * @param from   The sender, this may be changed by parse2() when
 *               the message has a sender, eg :xyz PRIVMSG ..
 * @param mtags  Message tags received for this message.
 * @param ch     The incoming line received (buffer), excluding message tags.
 */
static void parse2(Client *cptr, Client **fromptr, MessageTag *mtags, char *ch)
{
	Client *from = cptr;
	char *s;
	int len, i, numeric = 0, paramcount;
#ifdef DEBUGMODE
	time_t then, ticks;
	int retval;
#endif
	RealCommand *cmptr = NULL;
	int bytes;

	*fromptr = cptr; /* The default, unless a source is specified (and permitted) */

	/* The remaining part should never be more than 510 bytes
	 * (that is 512 minus CR LF, as specified in RFC1459 section 2.3).
	 * If it is too long, then we cut it off here.
	 */
	if (strlen(ch) > 510)
	{
		ch[510] = '\0';
	}

	para[0] = (char *)DEADBEEF_ADDR; /* helps us catch bugs :) */

	if (*ch == ':' || *ch == '@')
	{
		char sender[HOSTLEN + 1];
		s = sender;
		*s = '\0';

		/* Deal with :sender ... */
		for (++ch, i = 0; *ch && *ch != ' '; ++ch)
		{
			if (s < sender + sizeof(sender) - 1)
				*s++ = *ch;
		}
		*s = '\0';

		/* For servers we lookup the sender and change 'from' accordingly.
		 * For other clients we ignore the sender.
		 */
		if (*sender && IsServer(cptr))
		{
			from = find_client(sender, NULL);

			if (!from && strchr(sender, '@'))
				from = hash_find_nickatserver(sender, NULL);

			/* Sender not found. Possibly a ghost, so kill it.
			 * This can happen in normal circumstances. For example
			 * in case of A-B-C where we are B. If a KILL came from C
			 * for a client on A and we processed it at B, then until
			 * A has processed it we may still receive messages from A
			 * about it's soon-to-be-killed-client (all due to lag).
			 */
			if (!from)
			{
				ircstats.is_unpf++;
				remove_unknown(cptr, sender);
				return;
			}
			/* This is more severe. The server gave a source of a client
			 * that cannot exist from that direction.
			 * Eg in case of a topology of A-B-C-D and we are B,
			 * we got a message from A with ":D MODE...".
			 * In that case we send a SQUIT to that direction telling to
			 * unlink D from that side. This will likely lead to a
			 * problematic situation, though.
			 * This is, by the way, also why we try to prevent this situation
			 * in the first place by using PROTOCTL SERVERS=...
			 * in which case we reject such a flawed link very early
			 * in the server handshake process. -- Syzop
			 */
			if (from->direction != cptr)
			{
				ircstats.is_wrdi++;
				cancel_clients(cptr, from, ch);
				return;
			}
			*fromptr = from; /* Update source client */
		}
		while (*ch == ' ')
			ch++;
	}

	RunHook3(HOOKTYPE_PRE_COMMAND, from, mtags, ch);

	if (*ch == '\0')
	{
		if (!IsServer(cptr))
			cptr->local->since++; /* 1s fake lag */
		return;
	}

	/* Recalculate string length, now that we have skipped the sender */
	bytes = strlen(ch);

	/* Now let's figure out the command (or numeric)... */
	s = strchr(ch, ' ');	/* s -> End of the command code */
	len = (s) ? (s - ch) : 0;

	if (len == 3 && isdigit(*ch) && isdigit(*(ch + 1)) && isdigit(*(ch + 2)))
	{
		/* Numeric (eg: 311) */
		cmptr = NULL;
		numeric = (*ch - '0') * 100 + (*(ch + 1) - '0') * 10 + (*(ch + 2) - '0');
		paramcount = MAXPARA;
		ircstats.is_num++;
		parse_addlag(cptr, bytes);
	}
	else
	{
		/* Command (eg: PRIVMSG) */
		int flags = 0;
		if (s)
			*s++ = '\0';

		/* Set the appropriate flags for the command lookup */
		if (!IsRegistered(from))
			flags |= CMD_UNREGISTERED;
		if (IsUser(from))
			flags |= CMD_USER;
		if (IsServer(from))
			flags |= CMD_SERVER;
		if (IsShunned(from))
			flags |= CMD_SHUN;
		if (IsVirus(from))
			flags |= CMD_VIRUS;
		if (IsOper(from))
			flags |= CMD_OPER;
		cmptr = find_command(ch, flags);
		if (!cmptr || !(cmptr->flags & CMD_NOLAG))
		{
			/* Add fake lag (doing this early in the code, so we don't forget) */
			parse_addlag(cptr, bytes);
		}
		if (!cmptr)
		{
			/* Don't send error messages in response to NOTICEs
			 * in pre-connection state.
			 */
			if (!IsRegistered(cptr) && strcasecmp(ch, "NOTICE"))
			{
				sendnumericfmt(from, ERR_NOTREGISTERED, ":You have not registered");
				return;
			}
			/* If the user is shunned then don't send anything back in case
			 * of an unknown command, since we want to save data.
			 */
			if (IsShunned(cptr))
				return;
				
			if (ch[0] != '\0')
			{
				if (IsUser(from))
				{
					sendto_one(from, NULL, ":%s %d %s %s :Unknown command",
					                       me.name, ERR_UNKNOWNCOMMAND,
					                       from->name, ch);
					Debug((DEBUG_ERROR, "Unknown (%s) from %s",
					    ch, get_client_name(cptr, TRUE)));
				}
			}
			ircstats.is_unco++;
			return;
		}
		if (cmptr->flags != 0) { /* temporary until all commands are updated */

		/* Logic in comparisons below is a bit complicated, see notes */

		/* If you're a user, and this command does not permit users or opers, deny */
		if ((flags & CMD_USER) && !(cmptr->flags & CMD_USER) && !(cmptr->flags & CMD_OPER))
		{
			if (cmptr->flags & CMD_UNREGISTERED)
				sendnumeric(cptr, ERR_ALREADYREGISTRED); /* only for unregistered phase */
			else
				sendnumeric(cptr, ERR_NOTFORUSERS, cmptr->cmd); /* really never for users */
			return;
		}

		/* If you're a server, but command doesn't want servers, deny */
		if ((flags & CMD_SERVER) && !(cmptr->flags & CMD_SERVER))
			return;
		}

		/* If you're a user, but not an operator, and this requires operators, deny */
		if ((cmptr->flags & CMD_OPER) && (flags & CMD_USER) && !(flags & CMD_OPER))
		{
			sendnumeric(cptr, ERR_NOPRIVILEGES);
			return;
		}
		paramcount = cmptr->parameters;
		cmptr->bytes += bytes;
	}
	/*
	   ** Must the following loop really be so devious? On
	   ** surface it splits the message to parameters from
	   ** blank spaces. But, if paramcount has been reached,
	   ** the rest of the message goes into this last parameter
	   ** (about same effect as ":" has...) --msa
	 */

	/* Note initially true: s==NULL || *(s-1) == '\0' !! */

	i = 0;
	if (s)
	{
		/*
		if (paramcount > MAXPARA)
			paramcount = MAXPARA;
		We now use functions to create commands, so we can just check this 
		once when the command is created rather than each time the command
		is used -- codemastr
		*/
		for (;;)
		{
			/*
			   ** Never "FRANCE " again!! ;-) Clean
			   ** out *all* blanks.. --msa
			 */
			while (*s == ' ')
				*s++ = '\0';

			if (*s == '\0')
				break;
			if (*s == ':')
			{
				/*
				   ** The rest is single parameter--can
				   ** include blanks also.
				 */
				para[++i] = s + 1;
				break;
			}
			para[++i] = s;
			if (i >= paramcount)
				break;
			for (; *s != ' ' && *s; s++)
				;
		}
	}
	para[++i] = NULL;

	/* Check if one of the message tags are rejected by spamfilter */
	if (MyConnect(from) && !IsServer(from) && match_spamfilter_mtags(from, mtags, cmptr ? cmptr->cmd : NULL))
		return;

	if (cmptr == NULL)
	{
		do_numeric(numeric, from, mtags, i, para);
		return;
	}
	cmptr->count++;
	if (IsUser(cptr) && (cmptr->flags & CMD_RESETIDLE))
		cptr->local->last = TStime();

	/* Now ready to execute the command */
#ifndef DEBUGMODE
	if (cmptr->flags & CMD_ALIAS)
	{
		(*cmptr->aliasfunc) (from, mtags, i, para, cmptr->cmd);
	} else {
		if (!cmptr->overriders)
			(*cmptr->func) (from, mtags, i, para);
		else
			(*cmptr->overriders->func) (cmptr->overriders, from, mtags, i, para);
	}
#else
	then = clock();
	if (cmptr->flags & CMD_ALIAS)
	{
		(*cmptr->aliasfunc) (from, mtags, i, para, cmptr->cmd);
	} else {
		if (!cmptr->overriders)
			(*cmptr->func) (from, mtags, i, para);
		else
			(*cmptr->overriders->func) (cmptr->overriders, from, mtags, i, para);
	}
	if (!IsDead(cptr))
	{
		ticks = (clock() - then);
		if (IsServer(cptr))
			cmptr->rticks += ticks;
		else
			cmptr->lticks += ticks;
		cptr->local->cputime += ticks;
	}
#endif
}

/** Ban user that is "flooding from an unknown connection".
 * This is basically a client sending lots of data but not registering.
 * Note that "lots" in terms of IRC is a few KB's, since more is rather unusual.
 * @param client The client.
 */
static void ban_handshake_data_flooder(Client *client)
{
	if (find_tkl_exception(TKL_HANDSHAKE_DATA_FLOOD, client))
	{
		/* If the user is exempt we will still KILL the client, since it is
		 * clearly misbehaving. We just won't ZLINE the host, so it won't
		 * affect any other connections from the same IP address.
		 */
		exit_client(client, NULL, "Handshake data flood detected");
	}
	else
	{
		/* place_host_ban also takes care of removing any other clients with same host/ip */
		place_host_ban(client, iConf.handshake_data_flood_ban_action, "Handshake data flood detected", iConf.handshake_data_flood_ban_time);
	}
}

/** Add "fake lag" if needed.
 * The main purpose of fake lag is to create artificial lag when
 * processing incoming data from the client. So, if a client sends
 * a lot of commands, then next command will be processed at a rate
 * of 1 per second, or even slower. The exact algorithm is defined in this function.
 *
 * Servers are exempt from fake lag, so are IRCOps and clients tagged as
 * 'no fake lag' by services (rarely used). Finally, there is also an
 * option called class::options::nofakelag which exempts fakelag.
 * Exemptions should be granted with extreme care, since a client will
 * be able to flood at full speed causing potentially many Mbits or even
 * GBits of data to be sent out to other clients.
 *
 * @param client    The client.
 * @param cmdbytes  Number of bytes in the command.
 */
void parse_addlag(Client *client, int cmdbytes)
{
	if (!IsServer(client) && !IsNoFakeLag(client) &&
#ifdef FAKELAG_CONFIGURABLE
	    !(client->local->class && (client->local->class->options & CLASS_OPT_NOFAKELAG)) &&
#endif
	    !ValidatePermissionsForPath("immune:lag",client,NULL,NULL,NULL))
	{
		client->local->since += (1 + cmdbytes/90);
	}
}

/** Returns 1 if the client is lagged up and data should NOT be parsed.
 * See also parse_addlag() for more information on "fake lag".
 * @param client	The client to check
 * @returns 1 if client is lagged up and data should not be parsed, 0 otherwise.
 */
static int client_lagged_up(Client *client)
{
	if (client->status < CLIENT_STATUS_UNKNOWN)
		return 0;
	if (IsServer(client))
		return 0;
	if (ValidatePermissionsForPath("immune:lag",client,NULL,NULL,NULL))
		return 0;
	if (client->local->since - TStime() < 10)
		return 0;
	return 1;
}


/** Numeric received from a connection.
 * @param numeric     The numeric code (range 000-999)
 * @param cptr        The client
 * @param recv_mtags  Received message tags
 * @param parc        Parameter count
 * @param parv        Parameters
 * @note  In general you should NOT send anything back if you receive
 *        a numeric, this to prevent creating loops.
 */
static int do_numeric(int numeric, Client *client, MessageTag *recv_mtags, int parc, char *parv[])
{
	Client *acptr;
	Channel *channel;
	char *nick, *p;
	int i;
	char buffer[BUFSIZE];

	if ((numeric < 0) || (numeric > 999))
		return -1;

	if (MyConnect(client) && !IsServer(client) && !IsUser(client) && IsHandshake(client) && client->serv && !IsServerSent(client))
	{
		/* This is an outgoing server connect that is currently not yet IsServer() but in 'unknown' state.
		 * We need to handle a few responses here.
		 */

		/* STARTTLS: unknown command */
		if ((numeric == 451) && (parc > 2) && strstr(parv[1], "STARTTLS"))
		{
			if (client->serv->conf && (client->serv->conf->outgoing.options & CONNECT_INSECURE))
				start_server_handshake(client);
			else
				reject_insecure_server(client);
			return 0;
		}

		/* STARTTLS failed */
		if (numeric == 691)
		{
			sendto_umode(UMODE_OPER, "STARTTLS failed for link %s. Please check the other side of the link.", client->name);
			reject_insecure_server(client);
			return 0;
		}

		/* STARTTLS OK */
		if (numeric == 670)
		{
			int ret = client_starttls(client);
			if (ret < 0)
			{
				sendto_umode(UMODE_OPER, "STARTTLS handshake failed for link %s. Strange.", client->name);
				reject_insecure_server(client);
				return ret;
			}
			/* We don't call start_server_handshake() here. First the TLS handshake will
			 * be completed, then completed_connection() will be called for a second time,
			 * which will call completed_connection() from there.
			 */
			return 0;
		}
	}

	/* Other than the (strange) code from above, we actually
	 * don't process numerics from non-servers. So return here.
	 */
	if ((parc < 2) || BadPtr(parv[1]) || !IsServer(client))
		return 0;

	/* Remap low number numerics. */
	if (numeric < 100)
		numeric += 100;

	/* Convert parv[] back to a string 'buffer', since that is
	 * what we use in the sendto_* functions below.
	 */
	concat_params(buffer, sizeof(buffer), parc, parv);

	/* Now actually process the numeric, IOTW: send it on */
	for (; (nick = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		if ((acptr = find_client(nick, NULL)))
		{
			if (!IsMe(acptr) && IsUser(acptr))
			{
				if (MyConnect(acptr) && isdigit(*nick))
				{
					/* Hack to prevent leaking UID */
					char *skip = strchr(buffer, ' ');
					if (skip)
					{
						sendto_prefix_one(acptr, client, recv_mtags, ":%s %d %s %s",
						    client->name, numeric, acptr->name, skip+1);
					} /* else.. malformed (no content) */
				} else {
					sendto_prefix_one(acptr, client, recv_mtags, ":%s %d %s",
					    client->name, numeric, buffer);
				}
			}
			else if (IsServer(acptr) && acptr->direction != client->direction)
				sendto_prefix_one(acptr, client, recv_mtags, ":%s %d %s",
				    client->name, numeric, buffer);
		}
		else if ((acptr = find_server_quick(nick)))
		{
			if (!IsMe(acptr) && acptr->direction != client->direction)
				sendto_prefix_one(acptr, client, recv_mtags, ":%s %d %s",
				    client->name, numeric, buffer);
		}
		else if ((channel = find_channel(nick, NULL)))
		{
			sendto_channel(channel, client, client->direction,
			               0, 0, SEND_ALL, recv_mtags,
			               ":%s %d %s", client->name, numeric, buffer);
		}
	}

	return 0;
}

// FIXME: aren't we exiting the wrong client?
static void cancel_clients(Client *cptr, Client *client, char *cmd)
{
	if (IsServer(cptr) || IsServer(client) || IsMe(client))
		return;
	exit_client(cptr, NULL, "Fake prefix");
}

static void remove_unknown(Client *client, char *sender)
{
	if (!IsRegistered(client) || IsUser(client))
		return;
	/*
	 * Not from a server so don't need to worry about it.
	 */
	if (!IsServer(client))
		return;

#ifdef DEVELOP
	sendto_ops("Killing %s (%s)", sender, backupbuf);
	return;
#endif
	/*
	 * Do kill if it came from a server because it means there is a ghost
	 * user on the other server which needs to be removed. -avalon
	 */
	if ((isdigit(*sender) && strlen(sender) <= SIDLEN) || strchr(sender, '.'))
		sendto_one(client, NULL, ":%s SQUIT %s :Unknown prefix (%s) from %s",
		    me.id, sender, sender, client->name);
	else
		sendto_one(client, NULL, ":%s KILL %s :Ghost user", me.id, sender);
}
