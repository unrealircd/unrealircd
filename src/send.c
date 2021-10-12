/*
 *   Unreal Internet Relay Chat Daemon, src/send.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *		      University of Oulu, Computing Center
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

/* send.c 2.32 2/28/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen */

/** @file
 * @brief The sending functions to users, channels, servers.
 */

#include "unrealircd.h"

/* Some forward declarions are needed */
void vsendto_one(Client *to, MessageTag *mtags, const char *pattern, va_list vl);
void vsendto_prefix_one(Client *to, Client *from, MessageTag *mtags, const char *pattern, va_list vl);
static int vmakebuf_local_withprefix(char *buf, size_t buflen, Client *from, const char *pattern, va_list vl);

#define ADD_CRLF(buf, len) { if (len > 510) len = 510; \
                             buf[len++] = '\r'; buf[len++] = '\n'; buf[len] = '\0'; } while(0)

/* These are two local (static) buffers used by the various send functions */
static char sendbuf[2048];
static char sendbuf2[4096];

/** This is used to ensure no duplicate messages are sent
 * to the same server uplink/direction. In send functions
 * that deliver to multiple users or servers the value is
 * increased by 1 and then for each delivery in the loop
 * it is checked if to->direction->local->serial == current_serial
 * and if so, sending is skipped.
 */
MODVAR int  current_serial;

/** Mark the socket as "dead".
 * This is used when exit_client() cannot be used from the
 * current code because doing so would be (too) unexpected.
 * The socket is closed later in the main loop.
 * NOTE: this function is becoming less important, now that
 *       exit_client() will not actively free the client.
 *       Still, sometimes we need to use dead_socket()
 *       since we don't want to be doing IsDead() checks after
 *       each and every sendto...().
 * @param to		Client to mark as dead
 * @param notice	The quit reason to use
 */
int dead_socket(Client *to, char *notice)
{
	DBufClear(&to->local->recvQ);
	DBufClear(&to->local->sendQ);

	if (IsDeadSocket(to))
		return -1; /* already pending to be closed */

	SetDeadSocket(to);

	/* We may get here because of the 'CPR' in check_deadsockets().
	 * In which case, we return -1 as well.
	 */
	if (to->local->error_str)
		return -1; /* don't overwrite & don't send multiple times */
	
	if (!IsUser(to) && !IsUnknown(to) && !IsClosing(to))
		sendto_ops_and_log("Link to server %s (%s) closed: %s",
			to->name, to->ip?to->ip:"<no-ip>", notice);
	Debug((DEBUG_ERROR, "dead_socket: %s - %s", notice, get_client_name(to, FALSE)));
	safe_strdup(to->local->error_str, notice);
	return -1;
}

/** This is a callback function from the event loop.
 * All it does is call send_queued().
 */
void send_queued_cb(int fd, int revents, void *data)
{
	Client *to = data;

	if (IsDeadSocket(to))
		return;

	send_queued(to);
}

/** This function is called when queued data might be ready to be
 * sent to the client. It is called from the event loop and also
 * a couple of other places (such as when closing the connection).
 */
int send_queued(Client *to)
{
	int  len, rlen;
	dbufbuf *block;
	int want_read;

	/* We NEVER write to dead sockets. */
	if (IsDeadSocket(to))
		return -1;

	while (DBufLength(&to->local->sendQ) > 0)
	{
		block = container_of(to->local->sendQ.dbuf_list.next, dbufbuf, dbuf_node);
		len = block->size;

		/* Deliver it and check for fatal error.. */
		if ((rlen = deliver_it(to, block->data, len, &want_read)) < 0)
		{
			char buf[256];
			snprintf(buf, 256, "Write error: %s", STRERROR(ERRNO));
			return dead_socket(to, buf);
		}
		dbuf_delete(&to->local->sendQ, rlen);
		to->local->lastsq = DBufLength(&to->local->sendQ) / 1024;
		if (want_read)
		{
			/* SSL_write indicated that it cannot write data at this
			 * time and needs to READ data first. Let's stop talking
			 * to the user and ask to notify us when there's data
			 * to read.
			 */
			fd_setselect(to->local->fd, FD_SELECT_READ, send_queued_cb, to);
			fd_setselect(to->local->fd, FD_SELECT_WRITE, NULL, to);
			break;
		}
		/* Restore handling of reads towards read_packet(), since
		 * it may be overwritten in an earlier call to send_queued(),
		 * to handle reads by send_queued_cb(), see directly above.
		 */
		fd_setselect(to->local->fd, FD_SELECT_READ, read_packet, to);
		if (rlen < len)
		{
			/* incomplete write due to EWOULDBLOCK, reschedule */
			fd_setselect(to->local->fd, FD_SELECT_WRITE, send_queued_cb, to);
			break;
		}
	}
	
	/* Nothing left to write, stop asking for write-ready notification. */
	if ((DBufLength(&to->local->sendQ) == 0) && (to->local->fd >= 0))
		fd_setselect(to->local->fd, FD_SELECT_WRITE, NULL, to);

	return (IsDeadSocket(to)) ? -1 : 0;
}

/** Mark "to" with "there is data to be send" */
void mark_data_to_send(Client *to)
{
	if (!IsDeadSocket(to) && (to->local->fd >= 0) && (DBufLength(&to->local->sendQ) > 0))
	{
		fd_setselect(to->local->fd, FD_SELECT_WRITE, send_queued_cb, to);
	}
}

/** Send data to clients, servers, channels, IRCOps, etc.
 * There are a lot of send functions. The most commonly functions
 * are: sendto_one() to send to an individual user,
 * sendnumeric() to send a numeric to an individual user
 * and sendto_channel() to send a message to a channel.
 * @defgroup SendFunctions Send functions
 * @{
 */

/** Send a message to a single client.
 * This function is used quite a lot, after sendnumeric() it is the most-used send function.
 * @param to		The client to send to
 * @param mtags		Any message tags associated with this message (can be NULL)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 * @section sendto_one_examples Examples
 * @subsection sendto_one_mode_r Send "MODE -r"
 * This will send the `:serv.er.name MODE yournick -r` message.
 * Note that it will send only this message to illustrate the sendto_one() function.
 * It does *not* set anyone actually -r.
 * @code
 * sendto_one(client, NULL, ":%s MODE %s :-r", me.name, client->name);
 * @endcode
 */
void sendto_one(Client *to, MessageTag *mtags, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_one(to, mtags, pattern, vl);
	va_end(vl);
}

/** Send a message to a single client - va_list variant.
 * This function is similar to sendto_one() except that it
 * doesn't use varargs but uses a va_list instead.
 * Generally this function is NOT used outside send.c, so not by modules.
 * @param to		The client to send to
 * @param mtags		Any message tags associated with this message (can be NULL)
 * @param pattern	The format string / pattern to use.
 * @param vl		Format string parameters.
 */
void vsendto_one(Client *to, MessageTag *mtags, const char *pattern, va_list vl)
{
	char *mtags_str = mtags ? mtags_to_string(mtags, to) : NULL;

	ircvsnprintf(sendbuf, sizeof(sendbuf), pattern, vl);

	if (BadPtr(mtags_str))
	{
		/* Simple message without message tags */
		sendbufto_one(to, sendbuf, 0);
	} else {
		/* Message tags need to be prepended */
		snprintf(sendbuf2, sizeof(sendbuf2), "@%s %s", mtags_str, sendbuf);
		sendbufto_one(to, sendbuf2, 0);
	}
}


/** Send a line buffer to the client.
 * This function is used (usually indirectly) for pretty much all
 * cases where a line needs to be sent to a client.
 * @param to    The client to which the buffer should be send.
 * @param msg   The message.
 * @param quick Normally set to 0, see the notes.
 * @note
 * - Neither 'to' or 'msg' may be NULL.
 * - If quick is set to 0 then the length is calculated,
 *   the string is cut off at 510 bytes if needed, and
 *   CR+LF is added if needed.
 *   If quick is >0 then it is assumed the message already
 *   is within boundaries and passed all safety checks and
 *   contains CR+LF at the end. This if, for example, used in
 *   channel broadcasts to save some CPU cycles. It is NOT
 *   recommended as normal usage since you need to be very
 *   careful to take everything into account, including side-
 *   effects not mentioned here.
 */
void sendbufto_one(Client *to, char *msg, unsigned int quick)
{
	int len;
	Hook *h;
	Client *intended_to = to;
	
	Debug((DEBUG_ERROR, "Sending [%s] to %s", msg, to->name));

	if (to->direction)
		to = to->direction;
	if (IsDeadSocket(to))
		return;		/* This socket has already
				   been marked as dead */
	if (to->local->fd < 0)
	{
		/* This is normal when 'to' was being closed (via exit_client
		 *  and close_connection) --Run
		 * Print the debug message anyway...
		 */
		Debug((DEBUG_ERROR,
		    "Local socket %s with negative fd %d... AARGH!", to->name,
		    to->local->fd));
		return;
	}

	/* Unless 'quick' is set, we do some safety checks,
	 * cut the string off at the appropriate place and add
	 * CR+LF if needed (nearly always).
	 */
	if (!quick)
	{
		char *p = msg;
		if (*msg == '@')
		{
			/* The message includes one or more message tags:
			 * Spec-wise the rules allow about 8K for message tags
			 * and then 512 bytes for the remainder of the message.
			 * Since we do not allow user tags and only permit a
			 * limited set of tags we can have our own limits for
			 * the outgoing messages that we generate: a maximum of
			 * 500 bytes for message tags and 512 for the remainder.
			 * These limits will never be hit unless there is a bug
			 * somewhere.
			 */
			p = strchr(msg+1, ' ');
			if (!p)
			{
				ircd_log(LOG_ERROR, "[BUG] sendbufto_one(): Malformed message: %s",
					msg);
				return;
			}
			if (p - msg > 500)
			{
				ircd_log(LOG_ERROR, "[BUG] sendbufto_one(): Spec-wise legal, but massively oversized message-tag (len %d)",
				         (int)(p - msg));
				return;
			}
			p++; /* skip space character */
		}
		len = strlen(p);
		if (!len || (p[len - 1] != '\n'))
		{
			if (len > 510)
				len = 510;
			p[len++] = '\r';
			p[len++] = '\n';
			p[len] = '\0';
		}
		len = strlen(msg); /* (note: we could use pointer jugling to avoid a strlen here) */
	} else {
		len = quick;
	}

	if (len >= 1024)
	{
		ircd_log(LOG_ERROR, "sendbufto_one: len=%d, quick=%u", len, quick);
		abort();
	}

	if (IsMe(to))
	{
		char tmp_msg[500], *p;

		p = strchr(msg, '\r');
		if (p) *p = '\0';
		snprintf(tmp_msg, 500, "Trying to send data to myself! '%s'", msg);
		ircd_log(LOG_ERROR, "%s", tmp_msg);
		sendto_ops("%s", tmp_msg); /* recursion? */
		return;
	}

	for (h = Hooks[HOOKTYPE_PACKET]; h; h = h->next)
	{
		(*(h->func.intfunc))(&me, to, intended_to, &msg, &len);
		if (!msg)
			return;
	}

#if defined(DEBUGMODE) && defined(RAWCMDLOGGING)
	{
		char copy[512], *p;
		strlcpy(copy, msg, len > sizeof(copy) ? sizeof(copy) : len);
		p = strchr(copy, '\n');
		if (p) *p = '\0';
		p = strchr(copy, '\r');
		if (p) *p = '\0';
		ircd_log(LOG_ERROR, "-> %s: %s", to->name, copy);
	}
#endif

	if (DBufLength(&to->local->sendQ) > get_sendq(to))
	{
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: %u > %d",
			    get_client_name(to, FALSE), DBufLength(&to->local->sendQ),
			    get_sendq(to));
		dead_socket(to, "Max SendQ exceeded");
		return;
	}

	dbuf_put(&to->local->sendQ, msg, len);

	/*
	 * Update statistics. The following is slightly incorrect
	 * because it counts messages even if queued, but bytes
	 * only really sent. Queued bytes get updated in SendQueued.
	 */
	// FIXME: something is wrong here, I think we do double counts, either in message or in traffic, I forgot.. CHECK !!!!
	to->local->sendM += 1;
	me.local->sendM += 1;

	/* Previously we ran send_queued() here directly, but that is
	 * a bad idea, CPU-wise. So now we just mark the client indicating
	 * that there is data to send.
	 */
	mark_data_to_send(to);
}

/** A single function to send data to a channel.
 * Previously there were 6 different functions to send channel data,
 * now there is 1 single function. This also means that you most
 * likely will pass NULL or 0 as some parameters.
 * @param channel       The channel to send to
 * @param from        The source of the message
 * @param skip        The client to skip (can be NULL).
 *                    Note that if you specify a remote link then
 *                    you usually mean xyz->direction and not xyz.
 * @param prefix      Any combination of PREFIX_* (can be 0 for all)
 * @param clicap      Client capability the recipient should have
 *                    (this only works for local clients, we will
 *                     always send the message to remote clients and
 *                     assume the server there will handle it)
 * @param sendflags   Determines whether to send the message to local/remote users
 * @param mtags       The message tags to attach to this message
 * @param pattern     The pattern (eg: ":%s PRIVMSG %s :%s")
 * @param ...         The parameters for the pattern.
 * @note For all channel messages, it is important to attach the correct
 *       message tags (mtags) via a new_message() call, as can be seen
 *       in the example.
 * @section sendto_channel_examples Examples
 * @subsection sendto_channel_privmsg Send a PRIVMSG to a channel
 * This command will send the message "Hello everyone!!!" to the channel when executed.
 * @code
 * CMD_FUNC(cmd_sayhello)
 * {
 *     MessageTag *mtags = NULL;
 *     Channel *channel = NULL;
 *     if ((parc < 2) || BadPtr(parv[1]))
 *     {
 *         sendnumeric(client, ERR_NEEDMOREPARAMS, "SAYHELLO");
 *         return;
 *     }
 *     channel = find_channel(parv[1], NULL);
 *     if (!channel)
 *     {
 *         sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
 *         return;
 *     }
 *     new_message(client, recv_mtags, &mtags);
 *     sendto_channel(channel, client, client->direction, 0, 0,
 *                    SEND_LOCAL|SEND_REMOTE, mtags,
 *                    ":%s PRIVMSG %s :Hello everyone!!!",
 *                    client->name, channel->name);
 *     free_message_tags(mtags);
 * }
 * @endcode
 */
void sendto_channel(Channel *channel, Client *from, Client *skip,
                    int prefix, long clicap, int sendflags,
                    MessageTag *mtags,
                    FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Member *lp;
	Client *acptr;

	++current_serial;
	for (lp = channel->members; lp; lp = lp->next)
	{
		acptr = lp->client;

		/* Skip sending to 'skip' */
		if ((acptr == skip) || (acptr->direction == skip))
			continue;
		/* Don't send to deaf clients (unless 'senddeaf' is set) */
		if (IsDeaf(acptr) && (sendflags & SKIP_DEAF))
			continue;
		/* Don't send to NOCTCP clients */
		if (has_user_mode(acptr, 'T') && (sendflags & SKIP_CTCP))
			continue;
		/* Now deal with 'prefix' (if non-zero) */
		if (!prefix)
			goto good;
		if ((prefix & PREFIX_HALFOP) && (lp->flags & CHFL_HALFOP))
			goto good;
		if ((prefix & PREFIX_VOICE) && (lp->flags & CHFL_VOICE))
			goto good;
		if ((prefix & PREFIX_OP) && (lp->flags & CHFL_CHANOP))
			goto good;
#ifdef PREFIX_AQ
		if ((prefix & PREFIX_ADMIN) && (lp->flags & CHFL_CHANADMIN))
			goto good;
		if ((prefix & PREFIX_OWNER) && (lp->flags & CHFL_CHANOWNER))
			goto good;
#endif
		continue;
good:
		/* Now deal with 'clicap' (if non-zero) */
		if (clicap && MyUser(acptr) && ((clicap & CAP_INVERT) ? HasCapabilityFast(acptr, clicap) : !HasCapabilityFast(acptr, clicap)))
			continue;

		if (MyUser(acptr))
		{
			/* Local client */
			if (sendflags & SEND_LOCAL)
			{
				va_start(vl, pattern);
				vsendto_prefix_one(acptr, from, mtags, pattern, vl);
				va_end(vl);
			}
		}
		else
		{
			/* Remote client */
			if (sendflags & SEND_REMOTE)
			{
				/* Message already sent to remote link? */
				if (acptr->direction->local->serial != current_serial)
				{
					va_start(vl, pattern);
					vsendto_prefix_one(acptr, from, mtags, pattern, vl);
					va_end(vl);

					acptr->direction->local->serial = current_serial;
				}
			}
		}
	}

	if (sendflags & SEND_REMOTE)
	{
		/* For the remaining uplinks that we have not sent a message to yet...
		 * broadcast-channel-messages=never: don't send it to them
		 * broadcast-channel-messages=always: always send it to them
		 * broadcast-channel-messages=auto: send it to them if the channel is set +H (history)
		 */

		if ((iConf.broadcast_channel_messages == BROADCAST_CHANNEL_MESSAGES_ALWAYS) ||
		    ((iConf.broadcast_channel_messages == BROADCAST_CHANNEL_MESSAGES_AUTO) && has_channel_mode(channel, 'H')))
		{
			list_for_each_entry(acptr, &server_list, special_node)
			{
				if ((acptr == skip) || (acptr->direction == skip))
					continue; /* still obey this rule.. */
				if (acptr->direction->local->serial != current_serial)
				{
					va_start(vl, pattern);
					vsendto_prefix_one(acptr, from, mtags, pattern, vl);
					va_end(vl);

					acptr->direction->local->serial = current_serial;
				}
			}
		}
	}
}

/** Send a message to a server, taking into account server options if needed.
 * @param one		The client to skip (can be NULL)
 * @param servercaps	Server capabilities which must be present (OR'd together, if multiple)
 * @param noservercaps	Server capabilities which must NOT be present (OR'd together, if multiple)
 * @param mtags		The message tags to attach to this message.
 * @param format	The format string / pattern, such as ":%s NICK %s".
 * @param ...		The parameters for the format string
 */
void sendto_server(Client *one, unsigned long servercaps, unsigned long noservercaps, MessageTag *mtags, FORMAT_STRING(const char *format), ...)
{
	Client *acptr;

	/* noone to send to.. */
	if (list_empty(&server_list))
		return;

	list_for_each_entry(acptr, &server_list, special_node)
	{
		va_list vl;

		if (one && acptr == one->direction)
			continue;

		if (servercaps && !CHECKSERVERPROTO(acptr, servercaps))
			continue;

		if (noservercaps && CHECKSERVERPROTO(acptr, noservercaps))
			continue;

		va_start(vl, format);
		vsendto_one(acptr, mtags, format, vl);
		va_end(vl);
	}
}

/** Send a message to all local users on all channels where
 * the user 'user' is on.
 * This is used for events such as a nick change and quit.
 * @param user        The user and source of the message.
 * @param skip        The client to skip (can be NULL)
 * @param clicap      Client capability the recipient should have
 *                    (this only works for local clients, we will
 *                     always send the message to remote clients and
 *                     assume the server there will handle it)
 * @param mtags       The message tags to attach to this message.
 * @param pattern     The pattern (eg: ":%s NICK %s").
 * @param ...         The parameters for the pattern.
 */
void sendto_local_common_channels(Client *user, Client *skip, long clicap, MessageTag *mtags, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Membership *channels;
	Member *users;
	Client *acptr;

	/* We now create the buffer _before_ we send it to the clients. -- Syzop */
	*sendbuf = '\0';
	va_start(vl, pattern);
	vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, user, pattern, vl);
	va_end(vl);

	++current_serial;

	if (user->user)
	{
		for (channels = user->user->channel; channels; channels = channels->next)
		{
			for (users = channels->channel->members; users; users = users->next)
			{
				acptr = users->client;

				if (!MyConnect(acptr))
					continue; /* only process local clients */

				if (acptr->local->serial == current_serial)
					continue; /* message already sent to this client */

				if (clicap && ((clicap & CAP_INVERT) ? HasCapabilityFast(acptr, clicap) : !HasCapabilityFast(acptr, clicap)))
					continue; /* client does not have the specified capability */

				if (acptr == skip)
					continue; /* the one to skip */

				if (!user_can_see_member(acptr, user, channels->channel))
					continue; /* the sending user (quit'ing or nick changing) is 'invisible' -- skip */

				acptr->local->serial = current_serial;
				sendto_one(acptr, mtags, "%s", sendbuf);
			}
		}
	}
}

/*
** send a msg to all ppl on servers/hosts that match a specified mask
** (used for enhanced PRIVMSGs)
**
** addition -- Armin, 8jun90 (gruner@informatik.tu-muenchen.de)
*/

static int match_it(Client *one, char *mask, int what)
{
	switch (what)
	{
		case MATCH_HOST:
			return match_simple(mask, one->user->realhost);
		case MATCH_SERVER:
		default:
			return match_simple(mask, one->user->server);
	}
}

/** Send to all clients which match the mask.
 * This function is rarely used.
 * @param one		The client to skip
 * @param from		The sender
 * @param mask		The mask
 * @param what		One of MATCH_HOST or MATCH_SERVER
 * @param mtags		Message tags associated with the message
 * @param pattern	Format string
 * @param ...		Parameters to the format string
 */
void sendto_match_butone(Client *one, Client *from, char *mask, int what,
                         MessageTag *mtags, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char cansendlocal, cansendglobal;

	if (MyConnect(from))
	{
		cansendlocal = (ValidatePermissionsForPath("chat:notice:local",from,NULL,NULL,NULL)) ? 1 : 0;
		cansendglobal = (ValidatePermissionsForPath("chat:notice:global",from,NULL,NULL,NULL)) ? 1 : 0;
	}
	else
		cansendlocal = cansendglobal = 1;

	/* To servers... */
	if (cansendglobal)
	{
		char buf[512];

		va_start(vl, pattern);
		ircvsnprintf(buf, sizeof(buf), pattern, vl);
		va_end(vl);

		sendto_server(one, 0, 0, mtags, "%s", buf);
	}

	/* To local clients... */
	if (cansendlocal)
	{
		list_for_each_entry(acptr, &lclient_list, lclient_node)
		{
			if (!IsMe(acptr) && (acptr != one) && IsUser(acptr) && match_it(acptr, mask, what))
			{
				va_start(vl, pattern);
				vsendto_prefix_one(acptr, from, mtags, pattern, vl);
				va_end(vl);
			}
		}
	}
}

/** Send a message to all locally connected IRCOps
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_ops(FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char nbuf[1024];

	list_for_each_entry(acptr, &lclient_list, lclient_node)
		if (!IsServer(acptr) && !IsMe(acptr) && SendServNotice(acptr))
		{
			ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :*** ", me.name, acptr->name);
			strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(acptr, NULL, nbuf, vl);
			va_end(vl);
		}
}

/* Hmm.. so local sending is called sendto_ops() and local+remote is sendto_ops_butone(),
 * that is weird naming... (TODO fix some day in a new major series)
 */

/** Send a message to all IRCOps (local and remote), except one.
 * @param one		Skip sending the message to this client/direction
 * @param from		The sender (can not be NULL)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_ops_butone(Client *one, Client *from, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;

	++current_serial;
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!SendWallops(acptr))
			continue;
		if (acptr->direction->local->serial == current_serial)	/* sent message along it already ? */
			continue;
		if (acptr->direction == one)
			continue;	/* ...was the one I should skip */
		acptr->direction->local->serial = current_serial;

		va_start(vl, pattern);
		vsendto_prefix_one(acptr->direction, from, NULL, pattern, vl);
		va_end(vl);
	}
}

/** This function does exactly the same as sendto_ops() in practice in 5.x.
 * There used to be a difference between sendto_ops() and sendto_realops()
 * with regards to user-settable snomasks, but this is no longer the case.
 * TODO: remove this function in some future cleanup
 */
void sendto_realops(FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char nbuf[1024];

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :*** ", me.name, acptr->name);
		strlcat(nbuf, pattern, sizeof nbuf);

		va_start(vl, pattern);
		vsendto_one(acptr, NULL, nbuf, vl);
		va_end(vl);
	}
}

/** Send a message to all locally connected IRCOps and also log the error.
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_ops_and_log(FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	char buf[1024];

	va_start(vl, pattern);
	ircvsnprintf(buf, sizeof(buf), pattern, vl);
	va_end(vl);

	ircd_log(LOG_ERROR, "%s", buf);
	sendto_umode(UMODE_OPER, "%s", buf);
}

/** This function does exactly the same as sendto_ops_and_log()
 * TODO: remove this function in some future cleanup
 */
void sendto_realops_and_log(FORMAT_STRING(const char *fmt), ...)
{
	va_list vl;
	static char buf[2048];

	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);

	sendto_realops("%s", buf);
	ircd_log(LOG_ERROR, "%s", buf);
}


/** Send a message to all locally connected users with specified user mode.
 * @param umodes	The umode that the recipient should have set (one of UMODE_)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_umode(int umodes, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char nbuf[1024];

	list_for_each_entry(acptr, &lclient_list, lclient_node)
		if (IsUser(acptr) && (acptr->umodes & umodes) == umodes)
		{
			ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :", me.name, acptr->name);
			strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(acptr, NULL, nbuf, vl);
			va_end(vl);
		}
}

/** Send a message to all users with specified user mode (local & remote users).
 * @param umodes	The umode that the recipient should have set (one of UMODE_*)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_umode_global(int umodes, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char nbuf[1024];
	int i;
	char modestr[128];
	char *p;

	/* Convert 'umodes' (int) to 'modestr' (string) */
	*modestr = '\0';
	p = modestr;
	for(i = 0; i <= Usermode_highest; i++)
	{
		if (!Usermode_Table[i].flag)
			continue;
		if (umodes & Usermode_Table[i].mode)
			*p++ = Usermode_Table[i].flag;
	}
	*p = '\0';

	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		if (IsUser(acptr) && (acptr->umodes & umodes) == umodes)
		{
			ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :", me.name, acptr->name);
			strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(acptr, NULL, nbuf, vl);
			va_end(vl);
		} else
		if (IsServer(acptr) && *modestr)
		{
			snprintf(nbuf, sizeof(nbuf), ":%s SENDUMODE %s :%s", me.id, modestr, pattern);
			va_start(vl, pattern);
			vsendto_one(acptr, NULL, nbuf, vl);
			va_end(vl);
		}
	}
}

/** Send a message to all locally connected users with specified snomask.
 * @param snomask	The snomask that the recipient should have set (one of SNO_*)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_snomask(int snomask, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	char nbuf[2048];

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->user->snomask & snomask)
			sendnotice(acptr, "%s", nbuf);
	}
}

/** Send a message to all users with specified snomask (local and remote users).
 * @param snomask	The snomask that the recipient should have set (one of SNO_*)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_snomask_global(int snomask, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;
	int  i;
	char nbuf[2048], snobuf[32], *p;

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->user->snomask & snomask)
			sendnotice(acptr, "%s", nbuf);
	}

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_server(NULL, 0, 0, NULL, ":%s SENDSNO %s :%s", me.id, snobuf, nbuf);
}

/** Send CAP DEL and CAP NEW notification to clients supporting it.
 * This function is mostly meant to be used by the CAP and SASL modules.
 * @param add		Whether the CAP token is added (1) or removed (0)
 * @param token		The CAP token
 */
void send_cap_notify(int add, char *token)
{
	Client *client;
	ClientCapability *clicap = ClientCapabilityFindReal(token);
	long CAP_NOTIFY = ClientCapabilityBit("cap-notify");

	list_for_each_entry(client, &lclient_list, lclient_node)
	{
		if (HasCapabilityFast(client, CAP_NOTIFY))
		{
			if (add)
			{
				char *args = NULL;
				if (clicap)
				{
					if (clicap->visible && !clicap->visible(client))
						continue; /* invisible CAP, so don't announce it */
					if (clicap->parameter && (client->local->cap_protocol >= 302))
						args = clicap->parameter(client);
				}
				if (!args)
				{
					sendto_one(client, NULL, ":%s CAP %s NEW :%s",
						me.name, (*client->name ? client->name : "*"), token);
				} else {
					sendto_one(client, NULL, ":%s CAP %s NEW :%s=%s",
						me.name, (*client->name ? client->name : "*"), token, args);
				}
			} else {
				sendto_one(client, NULL, ":%s CAP %s DEL :%s",
					me.name, (*client->name ? client->name : "*"), token);
			}
		}
	}
}

/* Prepare buffer based on format string and 'from' for LOCAL delivery.
 * The prefix (:<something>) will be expanded to :nick!user@host if 'from'
 * is a person, taking into account the rules for hidden/cloaked host.
 * NOTE: Do not send this prepared buffer to remote clients or servers,
 *       they do not want or need the expanded prefix. In that case, simply
 *       use ircvsnprintf() directly.
 */
static int vmakebuf_local_withprefix(char *buf, size_t buflen, Client *from, const char *pattern, va_list vl)
{
	int len;

	/* This expands the ":%s " part of the pattern
	 * into ":nick!user@host ".
	 * In case of a non-person (server) it doesn't do
	 * anything since no expansion is needed.
	 */
	if (from && from->user && !strncmp(pattern, ":%s ", 4))
	{
		va_arg(vl, char *); /* eat first parameter */

		*buf = ':';
		strcpy(buf+1, from->name);

		if (IsUser(from))
		{
			char *username = from->user->username;
			char *host = GetHost(from);

			if (*username)
			{
				strcat(buf, "!");
				strcat(buf, username);
			}
			if (*host)
			{
				strcat(buf, "@");
				strcat(buf, host);
			}
		}
		/* Now build the remaining string */
		ircvsnprintf(buf + strlen(buf), buflen - strlen(buf), &pattern[3], vl);
	}
	else
	{
		ircvsnprintf(buf, buflen, pattern, vl);
	}

	len = strlen(buf);
	ADD_CRLF(buf, len);
	return len;
}

/** Send a message to a client, expand the sender prefix.
 * This is similar to sendto_one() except that it will expand the source part :%s
 * to :nick!user@host if needed, while with sendto_one() it will be :nick.
 * @param to		The client to send to
 * @param mtags		Any message tags associated with this message (can be NULL)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_prefix_one(Client *to, Client *from, MessageTag *mtags, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_prefix_one(to, from, mtags, pattern, vl);
	va_end(vl);
}

/** Send a message to a single client, expand the sender prefix - va_list variant.
 * This is similar to vsendto_one() except that it will expand the source part :%s
 * to :nick!user@host if needed, while with sendto_one() it will be :nick.
 * This function is also similar to sendto_prefix_one(), but this is the va_list
 * variant.
 * @param to		The client to send to
 * @param mtags		Any message tags associated with this message (can be NULL)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void vsendto_prefix_one(Client *to, Client *from, MessageTag *mtags, const char *pattern, va_list vl)
{
	char *mtags_str = mtags ? mtags_to_string(mtags, to) : NULL;

	if (to && from && MyUser(to) && from->user)
		vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, from, pattern, vl);
	else
		ircvsnprintf(sendbuf, sizeof(sendbuf), pattern, vl);

	if (BadPtr(mtags_str))
	{
		/* Simple message without message tags */
		sendbufto_one(to, sendbuf, 0);
	} else {
		/* Message tags need to be prepended */
		snprintf(sendbuf2, sizeof(sendbuf2), "@%s %s", mtags_str, sendbuf);
		sendbufto_one(to, sendbuf2, 0);
	}
}

void sendto_connectnotice(Client *newuser, int disconnect, char *comment)
{
	Client *acptr;
	char connect[512];

	if (!disconnect)
	{
		RunHook(HOOKTYPE_LOCAL_CONNECT, newuser);

		ircsnprintf(connect, sizeof(connect),
		    "*** Client connecting: %s (%s@%s) [%s] %s", newuser->name,
		    newuser->user->username, newuser->user->realhost, newuser->ip,
		    get_connect_extinfo(newuser));
	}
	else
	{
		ircsnprintf(connect, sizeof(connect), "*** Client exiting: %s (%s@%s) [%s] (%s)",
			newuser->name, newuser->user->username, newuser->user->realhost, newuser->ip, comment);
	}

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->user->snomask & SNO_CLIENT)
			sendnotice(acptr, "%s", connect);
	}
}

void sendto_fconnectnotice(Client *newuser, int disconnect, char *comment)
{
	Client *acptr;
	char connect[512];

	if (!disconnect)
	{
		ircsnprintf(connect, sizeof(connect),
		    "*** Client connecting: %s (%s@%s) [%s] %s", newuser->name,
		    newuser->user->username, newuser->user->realhost, newuser->ip ? newuser->ip : "0",
		    get_connect_extinfo(newuser));
	}
	else
	{
		ircsnprintf(connect, sizeof(connect), "*** Client exiting: %s (%s@%s) [%s] (%s)",
			newuser->name, newuser->user->username, newuser->user->realhost,
			newuser->ip ? newuser->ip : "0", comment);
	}

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->user->snomask & SNO_FCLIENT)
			sendto_one(acptr, NULL, ":%s NOTICE %s :%s", newuser->user->server, acptr->name, connect);
	}
}

/** Introduce user to all other servers, except the one to skip.
 * @param one    Server to skip (can be NULL)
 * @param client Client to introduce
 * @param umodes User modes of client
 */
void sendto_serv_butone_nickcmd(Client *one, Client *client, char *umodes)
{
	Client *acptr;

	list_for_each_entry(acptr, &server_list, special_node)
	{
		if (one && acptr == one->direction)
			continue;
		
		sendto_one_nickcmd(acptr, client, umodes);
	}
}

/** Introduce user to a server.
 * @param server  Server to send to (locally connected!)
 * @param client  Client to introduce
 * @param umodes  User modes of client
 */
void sendto_one_nickcmd(Client *server, Client *client, char *umodes)
{
	char *vhost;

	if (!*umodes)
		umodes = "+";

	if (SupportVHP(server))
	{
		if (IsHidden(client))
			vhost = client->user->virthost;
		else
			vhost = client->user->realhost;
	}
	else
	{
		if (IsHidden(client) && client->umodes & UMODE_SETHOST)
			vhost = client->user->virthost;
		else
			vhost = "*";
	}

	sendto_one(server, NULL,
		":%s UID %s %d %lld %s %s %s %s %s %s %s %s :%s",
		client->srvptr->id, client->name, client->hopcount,
		(long long)client->lastnick,
		client->user->username, client->user->realhost, client->id,
		client->user->svid, umodes, vhost, getcloak(client),
		encode_ip(client->ip), client->info);
}

/* sidenote: sendnotice() and sendtxtnumeric() assume no client or server
 * has a % in their nick, which is a safe assumption since % is illegal.
 */
 
/** Send a server notice to a client.
 * @param to		The client to send to
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendnotice(Client *to, FORMAT_STRING(const char *pattern), ...)
{
	static char realpattern[1024];
	va_list vl;
	char *name = *to->name ? to->name : "*";

	ircsnprintf(realpattern, sizeof(realpattern), ":%s NOTICE %s :%s", me.name, name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

/** Send MultiLine list as a notice, one for each line.
 * @param client	The client to send to
 * @param m		The MultiLine list.
 */
void sendnotice_multiline(Client *client, MultiLine *m)
{
	for (; m; m = m->next)
		sendnotice(client, "%s", m->line);
}


/** Send numeric message to a client.
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see src/numeric.c
 * @param ...		The parameters for the numeric
 * @note Be sure to provide the correct number and type of parameters that belong to the numeric. Check src/numeric.c when in doubt!
 * @section sendnumeric_examples Examples
 * @subsection sendnumeric_permission_denied Send "Permission Denied" numeric
 * This numeric has no parameter, so is simple:
 * @code
 * sendnumeric(client, ERR_NOPRIVILEGES);
 * @endcode
 * @subsection sendnumeric_notenoughparameters Send "Not enough parameters" numeric
 * This numeric requires 1 parameter: the name of the command.
 * @code
 * sendnumeric(client, ERR_NEEDMOREPARAMS, "SOMECOMMAND");
 * @endcode
 */
void sendnumeric(Client *to, int numeric, ...)
{
	va_list vl;
	char pattern[512];

	snprintf(pattern, sizeof(pattern), ":%s %.3d %s %s", me.name, numeric, to->name[0] ? to->name : "*", rpl_str(numeric));

	va_start(vl, numeric);
	vsendto_one(to, NULL, pattern, vl);
	va_end(vl);
}

/** Send numeric message to a client - format to user specific needs.
 * This will ignore the numeric definition of src/numeric.c and always send ":me.name numeric clientname "
 * followed by the pattern and format string you choose.
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see src/numeric.c
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 * @note Don't forget to add a colon if you need it (eg `:%%s`), this is a common mistake.
 */
void sendnumericfmt(Client *to, int numeric, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	char realpattern[512];

	snprintf(realpattern, sizeof(realpattern), ":%s %.3d %s %s", me.name, numeric, to->name[0] ? to->name : "*", pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

/** Send text numeric message to a client (RPL_TEXT).
 * Because this generic output numeric is commonly used it got a special function for it.
 * @param to		The recipient
 * @param numeric	The numeric, one of RPL_* or ERR_*, see src/numeric.c
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 * @note Don't forget to add a colon if you need it (eg `:%%s`), this is a common mistake.
 */
void sendtxtnumeric(Client *to, FORMAT_STRING(const char *pattern), ...)
{
	static char realpattern[1024];
	va_list vl;

	ircsnprintf(realpattern, sizeof(realpattern), ":%s %d %s :%s", me.name, RPL_TEXT, to->name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

/* Send raw data directly to socket, bypassing everything.
 * Looks like an interesting function to call? NO! STOP!
 * Don't use this function. It may only be used by the initial
 * Z-Line check via the codepath to banned_client().
 * YOU SHOULD NEVER USE THIS FUNCTION.
 * If you want to send raw data (without formatting) to a client
 * then have a look at sendbufto_one() instead.
 *
 * Side-effects:
 * Too many to list here. Only in the early accept code the
 * "if's" and side-effects are under control.
 *
 * By the way, did I already mention that you SHOULD NOT USE THIS
 * FUNCTION? ;)
 */
void send_raw_direct(Client *user, FORMAT_STRING(FORMAT_STRING(const char *pattern)), ...)
{
	va_list vl;
	int sendlen;

	*sendbuf = '\0';
	va_start(vl, pattern);
	sendlen = vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, user, pattern, vl);
	va_end(vl);
	(void)send(user->local->fd, sendbuf, sendlen, 0);
}

/** @} */
