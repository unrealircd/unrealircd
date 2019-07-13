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

#include "unrealircd.h"

void vsendto_one(aClient *to, MessageTag *mtags, const char *pattern, va_list vl);
void sendbufto_one(aClient *to, char *msg, unsigned int quick);
static int vmakebuf_local_withprefix(char *buf, size_t buflen, struct Client *from, const char *pattern, va_list vl);

#define ADD_CRLF(buf, len) { if (len > 510) len = 510; \
                             buf[len++] = '\r'; buf[len++] = '\n'; buf[len] = '\0'; } while(0)

#define NEWLINE	"\r\n"

static char sendbuf[2048];
static char sendbuf2[4096];
static char tcmd[2048];
static char ccmd[2048];
static char xcmd[2048];

void vsendto_prefix_one(struct Client *to, struct Client *from, MessageTag *mtags,
    const char *pattern, va_list vl);

MODVAR int  current_serial;
/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	notice will be the quit message. notice will also be
**	sent to locally connected IRCOps in case 'to' is a server.
*/
int dead_link(aClient *to, char *notice)
{
	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->local->recvQ);
	DBufClear(&to->local->sendQ);

	if (to->flags & FLAGS_DEADSOCKET)
		return -1; /* already pending to be closed */

	to->flags |= FLAGS_DEADSOCKET;

	/* We may get here because of the 'CPR' in check_deadsockets().
	 * In which case, we return -1 as well.
	 */
	if (to->local->error_str)
		return -1; /* don't overwrite & don't send multiple times */
	
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		sendto_ops_and_log("Link to server %s (%s) closed: %s",
			to->name, to->ip?to->ip:"<no-ip>", notice);
	Debug((DEBUG_ERROR, "dead_link: %s - %s", notice, get_client_name(to, FALSE)));
	to->local->error_str = strdup(notice);
	return -1;
}

/** This is a callback function from the event loop.
 * All it does is call send_queued().
 */
static void send_queued_cb(int fd, int revents, void *data)
{
	aClient *to = data;

	if (IsDead(to))
		return;

	send_queued(to);
}

/** This function is called when queued data might be ready to be
 * sent to the client. It is called from the event loop and also
 * a couple of other places (such as when closing the connection).
 */
int send_queued(aClient *to)
{
	char *msg;
	int  len, rlen;
	dbufbuf *block;
	int want_read;

	/* We NEVER write to dead sockets. */
	if (IsDead(to))
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
			return dead_link(to, buf);
		}
		(void)dbuf_delete(&to->local->sendQ, rlen);
		to->local->lastsq = DBufLength(&to->local->sendQ) / 1024;
		if (want_read)
		{
			/* SSL_write indicated that it cannot write data at this
			 * time and needs to READ data first. Let's stop talking
			 * to the user and ask to notify us when there's data
			 * to read.
			 */
			fd_setselect(to->fd, FD_SELECT_READ, send_queued_cb, to);
			fd_setselect(to->fd, FD_SELECT_WRITE, NULL, to);
			break;
		}
		/* Restore handling of reads towards read_packet(), since
		 * it may be overwritten in an earlier call to send_queued(),
		 * to handle reads by send_queued_cb(), see directly above.
		 */
		fd_setselect(to->fd, FD_SELECT_READ, read_packet, to);
		if (rlen < block->size)
		{
			/* incomplete write due to EWOULDBLOCK, reschedule */
			fd_setselect(to->fd, FD_SELECT_WRITE, send_queued_cb, to);
			break;
		}
	}
	
	/* Nothing left to write, stop asking for write-ready notification. */
	if ((DBufLength(&to->local->sendQ) == 0) && (to->fd >= 0))
		fd_setselect(to->fd, FD_SELECT_NOWRITE, NULL, to);

	return (IsDead(to)) ? -1 : 0;
}

/*
 *  send message to single client
 */
void sendto_one(aClient *to, MessageTag *mtags, char *pattern, ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_one(to, mtags, pattern, vl);
	va_end(vl);
}

void vsendto_one(aClient *to, MessageTag *mtags, const char *pattern, va_list vl)
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
 * @notes
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
void sendbufto_one(aClient *to, char *msg, unsigned int quick)
{
	int len;
	Hook *h;
	aClient *intended_to = to;
	
	Debug((DEBUG_ERROR, "Sending [%s] to %s", msg, to->name));

	if (to->from)
		to = to->from;
	if (IsDead(to))
		return;		/* This socket has already
				   been marked as dead */
	if (to->fd < 0)
	{
		/* This is normal when 'to' was being closed (via exit_client
		 *  and close_connection) --Run
		 * Print the debug message anyway...
		 */
		Debug((DEBUG_ERROR,
		    "Local socket %s with negative fd %d... AARGH!", to->name,
		    to->fd));
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
		dead_link(to, "Max SendQ exceeded");
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

	if (DBufLength(&to->local->sendQ) > 0)
		send_queued(to);
}

/** A single function to send data to a channel.
 * Previously there were 6, now there is 1. This means there
 * are likely some parameters that you will pass as NULL or 0
 * but at least we can all use one single function.
 * @param chptr       The channel to send to
 * @param from        The source of the message
 * @param skip        The client to skip (can be NULL)
 * @param prefix      Any combination of PREFIX_* (can be 0 for all)
 * @param clicap      Client capability the recipient should have
 *                    (this only works for local clients, we will
 *                     always send the message to remote clients and
 *                     assume the server there will handle it)
 * @param sendflags   Determines whether to send the message to local/remote users
 * @param senddeaf    Send message to 'deaf' clients
 * @param mtags       The message tags to attach to this message
 * @param pattern     The pattern (eg: ":%s PRIVMSG %s :%s")
 * @param ...         The parameters for the pattern.
 */
void sendto_channel(aChannel *chptr, aClient *from, aClient *skip,
                    int prefix, long clicap, int sendflags,
                    MessageTag *mtags,
                    char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int  i = 0,j = 0;
	Hook *h;

	++current_serial;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;

		/* Skip sending to 'skip' */
		if ((acptr == skip) || (acptr->from == skip))
			continue;
		/* Don't send to deaf clients (unless 'senddeaf' is set) */
		if (IsDeaf(acptr) && (sendflags & SKIP_DEAF))
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
		if (clicap && MyClient(acptr) && !HasCapabilityFast(acptr, clicap))
			continue;

		if (MyClient(acptr))
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
				if (acptr->from->local->serial != current_serial)
				{
					va_start(vl, pattern);
					vsendto_prefix_one(acptr, from, mtags, pattern, vl);
					va_end(vl);

					acptr->from->local->serial = current_serial;
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
		    ((iConf.broadcast_channel_messages == BROADCAST_CHANNEL_MESSAGES_AUTO) && has_channel_mode(chptr, 'H')))
		{
			list_for_each_entry(acptr, &server_list, special_node)
			{
				if (acptr->from->local->serial != current_serial)
				{
					va_start(vl, pattern);
					vsendto_prefix_one(acptr, from, mtags, pattern, vl);
					va_end(vl);

					acptr->from->local->serial = current_serial;
				}
			}
		}
	}
}

/*
 * sendto_server
 *
 * inputs       - pointer to client to NOT send to
 *              - caps or'd together which must ALL be present
 *              - caps or'd together which must ALL NOT be present
 *              - printf style format string
 *              - args to format string
 * output       - NONE
 * side effects - Send a message to all connected servers, except the
 *                client 'one' (if non-NULL), as long as the servers
 *                support ALL capabs in 'caps', and NO capabs in 'nocaps'.
 *
 * This function was written in an attempt to merge together the other
 * billion sendto_*serv*() functions, which sprung up with capabs, uids etc
 * -davidt
 *
 * Ported this function over from charybdis 3.5, as it is much cleaner than
 * what we had going on here.
 * - kaniini
 */
void sendto_server(aClient *one, unsigned long caps, unsigned long nocaps, MessageTag *mtags, const char *format, ...)
{
	aClient *cptr;

	/* noone to send to.. */
	if (list_empty(&server_list))
		return;

	list_for_each_entry(cptr, &server_list, special_node)
	{
		va_list vl;

		if (one && cptr == one->from)
			continue;

		if (caps && !CHECKPROTO(cptr, caps))
			continue;

		if (nocaps && CHECKPROTO(cptr, nocaps))
			continue;

		va_start(vl, format);
		vsendto_one(cptr, mtags, format, vl);
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
void sendto_local_common_channels(aClient *user, aClient *skip, long clicap, MessageTag *mtags, char *pattern, ...)
{
	va_list vl;
	Membership *channels;
	Member *users;
	aClient *cptr;

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
			for (users = channels->chptr->members; users; users = users->next)
			{
				cptr = users->cptr;

				if (!MyConnect(cptr))
					continue; /* only process local clients */

				if (cptr->local->serial == current_serial)
					continue; /* message already sent to this client */

				if (clicap && !HasCapabilityFast(cptr, clicap))
					continue; /* client does not have the specified capability */

				if (cptr == skip)
					continue; /* the one to skip */

				if (!user_can_see_member(cptr, user, channels->chptr))
					continue; /* the sending user (quit'ing or nick changing) is 'invisible' -- skip */

				cptr->local->serial = current_serial;
				sendto_one(cptr, mtags, "%s", sendbuf);
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

static int match_it(one, mask, what)
	aClient *one;
	char *mask;
	int  what;
{
	switch (what)
	{
	  case MATCH_HOST:
		  return (match(mask, one->user->realhost) == 0);
	  case MATCH_SERVER:
	  default:
		  return (match(mask, one->user->server) == 0);
	}
}

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
void sendto_match_butone(aClient *one, aClient *from, char *mask, int what,
    MessageTag *mtags, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr, *acptr;
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
		list_for_each_entry(cptr, &lclient_list, lclient_node)
		{
			if (!IsMe(cptr) && (cptr != one) && IsRegisteredUser(cptr) && match_it(cptr, mask, what))
			{
				va_start(vl, pattern);
				vsendto_prefix_one(cptr, from, mtags, pattern, vl);
				va_end(vl);
			}
		}
	}
}

/*
 * sendto_ops
 *
 *	Send to *local* ops only.
 */
void sendto_ops(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	char nbuf[1024];

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (!IsServer(cptr) && !IsMe(cptr) && SendServNotice(cptr))
		{
			(void)ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :*** ", me.name, cptr->name);
			(void)strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(cptr, NULL, nbuf, vl);
			va_end(vl);
		}
}

/*
 * sendto_umode
 *
 *  Send to specified umode
 */
void sendto_umode(int umodes, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	char nbuf[1024];

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (IsPerson(cptr) && (cptr->umodes & umodes) == umodes)
		{
			(void)ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :",
			    me.name, cptr->name);
			(void)strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(cptr, NULL, nbuf, vl);
			va_end(vl);
		}
}

/*
 * sendto_umode_global
 *
 * Send to specified umode *GLOBALLY* (on all servers)
 */
void sendto_umode_global(int umodes, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
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

	list_for_each_entry(cptr, &lclient_list, lclient_node)
	{
		if (IsPerson(cptr) && (cptr->umodes & umodes) == umodes)
		{
			(void)ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :",
			    me.name, cptr->name);
			(void)strlcat(nbuf, pattern, sizeof nbuf);

			va_start(vl, pattern);
			vsendto_one(cptr, NULL, nbuf, vl);
			va_end(vl);
		} else
		if (IsServer(cptr) && *modestr)
		{
			snprintf(nbuf, sizeof(nbuf), ":%s SENDUMODE %s :%s", me.name, modestr, pattern);
			va_start(vl, pattern);
			vsendto_one(cptr, NULL, nbuf, vl);
			va_end(vl);
		}
	}
}

/** Send to specified snomask - local / operonly.
 * @param snomask Snomask to send to (can be a bitmask [AND])
 * @param pattern printf-style pattern, followed by parameters.
 * This function does not send snomasks to non-opers.
 */
void sendto_snomask(int snomask, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	char nbuf[2048];

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		if (cptr->user->snomask & snomask)
			sendnotice(cptr, "%s", nbuf);
	}
}

/** Send to specified snomask - global / operonly.
 * @param snomask Snomask to send to (can be a bitmask [AND])
 * @param pattern printf-style pattern, followed by parameters
 * This function does not send snomasks to non-opers.
 */
void sendto_snomask_global(int snomask, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[2048], snobuf[32], *p;

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		if (cptr->user->snomask & snomask)
			sendnotice(cptr, "%s", nbuf);
	}

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_server(&me, 0, 0, NULL, ":%s SENDSNO %s :%s", me.name, snobuf, nbuf);
}

/*
 * send_cap_notify
 *
 * Send CAP DEL or CAP NEW to clients supporting this.
 */
void send_cap_notify(int add, char *token)
{
	va_list vl;
	aClient *cptr;
	char nbuf[1024];
	ClientCapability *clicap = ClientCapabilityFindReal(token);
	long CAP_NOTIFY = ClientCapabilityBit("cap-notify");

	list_for_each_entry(cptr, &lclient_list, lclient_node)
	{
		if (HasCapabilityFast(cptr, CAP_NOTIFY))
		{
			if (add)
			{
				char *args = NULL;
				if (clicap)
				{
					if (clicap->visible && !clicap->visible(cptr))
						continue; /* invisible CAP, so don't announce it */
					if (clicap->parameter && (cptr->local->cap_protocol >= 302))
						args = clicap->parameter(cptr);
				}
				if (!args)
				{
					sendto_one(cptr, NULL, ":%s CAP %s NEW :%s",
						me.name, (*cptr->name ? cptr->name : "*"), token);
				} else {
					sendto_one(cptr, NULL, ":%s CAP %s NEW :%s=%s",
						me.name, (*cptr->name ? cptr->name : "*"), token, args);
				}
			} else {
				sendto_one(cptr, NULL, ":%s CAP %s DEL :%s",
					me.name, (*cptr->name ? cptr->name : "*"), token);
			}
		}
	}
}

/* ** sendto_ops_butone
**	Send message to all operators.
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
void sendto_ops_butone(aClient *one, aClient *from, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;

	++current_serial;
	list_for_each_entry(cptr, &client_list, client_node)
	{
		if (!SendWallops(cptr))
			continue;
		if (cptr->from->local->serial == current_serial)	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		cptr->from->local->serial = current_serial;

		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, NULL, pattern, vl);
		va_end(vl);
	}
}

/* Prepare buffer based on format string and 'from' for LOCAL delivery.
 * The prefix (:<something>) will be expanded to :nick!user@host if 'from'
 * is a person, taking into account the rules for hidden/cloaked host.
 * NOTE: Do not send this prepared buffer to remote clients or servers,
 *       they do not want or need the expanded prefix. In that case, simply
 *       use ircvsnprintf() directly.
 */
static int vmakebuf_local_withprefix(char *buf, size_t buflen, struct Client *from, const char *pattern, va_list vl)
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

		if (IsPerson(from))
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

void vsendto_prefix_one(struct Client *to, struct Client *from, MessageTag *mtags,
                        const char *pattern, va_list vl)
{
	char *mtags_str = mtags ? mtags_to_string(mtags, to) : NULL;

	if (to && from && MyClient(to) && from->user)
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

/*
 * sendto_prefix_one
 *
 * to - destination client
 * from - client which message is from
 *
 * NOTE: NEITHER OF THESE SHOULD *EVER* BE NULL!!
 * -avalon
 */

void sendto_prefix_one(aClient *to, aClient *from, MessageTag *mtags, const char *pattern, ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_prefix_one(to, from, mtags, pattern, vl);
	va_end(vl);
}

/*
 * sendto_realops
 *
 *	Send to *local* ops only but NOT +s nonopers.
 */
void sendto_realops(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	char nbuf[1024];

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		(void)ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :*** ",
		    me.name, cptr->name);
		(void)strlcat(nbuf, pattern, sizeof nbuf);

		va_start(vl, pattern);
		vsendto_one(cptr, NULL, nbuf, vl);
		va_end(vl);
	}
}

/* Sends a message to all (local) opers AND logs to the ircdlog (as LOG_ERROR) */
void sendto_realops_and_log(char *fmt, ...)
{
va_list vl;
static char buf[2048];

	va_start(vl, fmt);
	vsnprintf(buf, sizeof(buf), fmt, vl);
	va_end(vl);

	sendto_realops("%s", buf);
	ircd_log(LOG_ERROR, "%s", buf);
}

void sendto_connectnotice(aClient *acptr, int disconnect, char *comment)
{
	aClient *cptr;
	int  i, j;
	char connect[512], secure[256];

	if (!disconnect)
	{
		RunHook(HOOKTYPE_LOCAL_CONNECT, acptr);

		*secure = '\0';
		if (IsSecure(acptr))
			snprintf(secure, sizeof(secure), " [secure %s]", SSL_get_cipher(acptr->local->ssl));

		ircsnprintf(connect, sizeof(connect),
		    "*** Client connecting: %s (%s@%s) [%s] {%s}%s", acptr->name,
		    acptr->user->username, acptr->user->realhost, acptr->ip,
		    acptr->local->class ? acptr->local->class->name : "0",
		    secure);
	}
	else
	{
		ircsnprintf(connect, sizeof(connect), "*** Client exiting: %s (%s@%s) [%s] (%s)",
			acptr->name, acptr->user->username, acptr->user->realhost, acptr->ip, comment);
	}

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		if (cptr->user->snomask & SNO_CLIENT)
			sendnotice(cptr, "%s", connect);
	}
}

void sendto_fconnectnotice(aClient *acptr, int disconnect, char *comment)
{
	aClient *cptr;
	int  i, j;
	char connect[512], secure[256];

	if (!disconnect)
	{
		*secure = '\0';
		if (IsSecureConnect(acptr))
			strcpy(secure, " [secure]"); /* will we ever expand this? */

		ircsnprintf(connect, sizeof(connect),
		    "*** Client connecting: %s (%s@%s) [%s] {0}%s", acptr->name,
		    acptr->user->username, acptr->user->realhost, acptr->ip ? acptr->ip : "0",
		    secure);
	}
	else
	{
		ircsnprintf(connect, sizeof(connect), "*** Client exiting: %s (%s@%s) [%s] (%s)",
			acptr->name, acptr->user->username, acptr->user->realhost,
			acptr->ip ? acptr->ip : "0", comment);
	}

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		if (cptr->user->snomask & SNO_FCLIENT)
			sendnotice(cptr, ":%s NOTICE %s :%s", acptr->user->server, cptr->name, connect);
	}
}

/** Introduce user to NICKv2-capable and SID-capable servers.
 * @param cptr Server to skip
 * @param sptr Client to introduce
 * @param umodes User modes of client
 */
void sendto_serv_butone_nickcmd(aClient *one, aClient *sptr, char *umodes)
{
	aClient *cptr;

	if (BadPtr(umodes))
		umodes = "+";
	
	list_for_each_entry(cptr, &server_list, special_node)
	{
		va_list vl;

		if (one && cptr == one->from)
			continue;
		
		if (!CHECKPROTO(cptr, PROTO_SID) && !CHECKPROTO(cptr, PROTO_NICKv2))
			continue;
		
		sendto_one_nickcmd(cptr, sptr, umodes);
	}
}

/** Introduce user to NICKv2-capable and SID-capable servers.
 * @param cptr Server to send to (locally connected!)
 * @param sptr Client to introduce
 * @param umodes User modes of client
 */
void sendto_one_nickcmd(aClient *cptr, aClient *sptr, char *umodes)
{
	char *vhost;

	if (!*umodes)
		umodes = "+";

	if (SupportVHP(cptr))
	{
		if (IsHidden(sptr))
			vhost = sptr->user->virthost;
		else
			vhost = sptr->user->realhost;
	}
	else
	{
		if (IsHidden(sptr) && sptr->umodes & UMODE_SETHOST)
			vhost = sptr->user->virthost;
		else
			vhost = "*";
	}

	if (CHECKPROTO(cptr, PROTO_SID) && *sptr->id)
	{
		sendto_one(cptr, NULL,
			":%s UID %s %d %ld %s %s %s %s %s %s %s %s :%s",
			sptr->srvptr->id, sptr->name, sptr->hopcount, sptr->lastnick,
			sptr->user->username, sptr->user->realhost, sptr->id,
			sptr->user->svid, umodes, vhost, getcloak(sptr),
			encode_ip(sptr->ip), sptr->info);
		return;
	}

	sendto_one(cptr, NULL,
		    "NICK %s %d %ld %s %s %s %s %s %s %s%s%s%s:%s",
		    sptr->name,
		    sptr->hopcount+1, sptr->lastnick, sptr->user->username, 
		    sptr->user->realhost, sptr->srvptr->name,
		    sptr->user->svid, umodes, vhost,
		    CHECKPROTO(cptr, PROTO_CLK) ? getcloak(sptr) : "",
		    CHECKPROTO(cptr, PROTO_CLK) ? " " : "",
		    CHECKPROTO(cptr, PROTO_NICKIP) ? encode_ip(sptr->ip) : "",
		    CHECKPROTO(cptr, PROTO_NICKIP) ? " " : "",
		    sptr->info);

	return;
}

/* sidenote: sendnotice() and sendtxtnumeric() assume no client or server
 * has a % in their nick, which is a safe assumption since % is illegal.
 */
 
void sendnotice(aClient *to, char *pattern, ...)
{
static char realpattern[1024];
va_list vl;
char *name = *to->name ? to->name : "*";

	ircsnprintf(realpattern, sizeof(realpattern), ":%s NOTICE %s :%s", me.name, name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

void sendtxtnumeric(aClient *to, char *pattern, ...)
{
static char realpattern[1024];
va_list vl;

	ircsnprintf(realpattern, sizeof(realpattern), ":%s %d %s :%s", me.name, RPL_TEXT, to->name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

/** Send numeric to IRC client */
void sendnumeric(aClient *to, int numeric, ...)
{
	va_list vl;
	char pattern[512];

	snprintf(pattern, sizeof(pattern), ":%s %.3d %s %s", me.name, numeric, to->name[0] ? to->name : "*", rpl_str(numeric));

	va_start(vl, numeric);
	vsendto_one(to, NULL, pattern, vl);
	va_end(vl);
}

/** Send numeric to IRC client */
void sendnumericfmt(aClient *to, int numeric, char *pattern, ...)
{
	va_list vl;
	char realpattern[512];

	snprintf(realpattern, sizeof(realpattern), ":%s %.3d %s %s", me.name, numeric, to->name[0] ? to->name : "*", pattern);

	va_start(vl, pattern);
	vsendto_one(to, NULL, realpattern, vl);
	va_end(vl);
}

/** Send raw data directly to socket, bypassing everything.
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
void send_raw_direct(aClient *user, char *pattern, ...)
{
	va_list vl;
	int sendlen;

	*sendbuf = '\0';
	va_start(vl, pattern);
	sendlen = vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, user, pattern, vl);
	va_end(vl);
	(void)send(user->fd, sendbuf, sendlen, 0);
}

/** Send a message to all locally connected IRCOps and log the error.
 */
void sendto_ops_and_log(char *pattern, ...)
{
	va_list vl;
	char buf[1024];

	va_start(vl, pattern);
	ircvsnprintf(buf, sizeof(buf), pattern, vl);
	va_end(vl);

	ircd_log(LOG_ERROR, "%s", buf);
	sendto_umode(UMODE_OPER, "%s", buf);
}
