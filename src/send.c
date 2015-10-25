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

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

#include "struct.h"
#include "numeric.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "msg.h"
#include <stdarg.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <string.h>

void vsendto_one(aClient *to, const char *pattern, va_list vl);
void sendbufto_one(aClient *to, char *msg, unsigned int quick);
static int vmakebuf_local_withprefix(char *buf, size_t buflen, struct Client *from, const char *pattern, va_list vl);

#define ADD_CRLF(buf, len) { if (len > 510) len = 510; \
                             buf[len++] = '\r'; buf[len++] = '\n'; buf[len] = '\0'; } while(0)

#define NEWLINE	"\r\n"

static char sendbuf[2048];
static char tcmd[2048];
static char ccmd[2048];
static char xcmd[2048];

void vsendto_prefix_one(struct Client *to, struct Client *from,
    const char *pattern, va_list vl);

MODVAR int  current_serial;
MODVAR int  sendanyways = 0;
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

	if ((to->flags & FLAGS_DEADSOCKET) && to->local->error_str)
		return -1; /* already pending to be closed */

	to->flags |= FLAGS_DEADSOCKET;
	
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		sendto_umode(UMODE_OPER, "Closing link: %s - %s",
			notice, get_client_name(to, FALSE));
	Debug((DEBUG_ERROR, "dead_link: %s - %s", notice, get_client_name(to, FALSE)));
	to->local->error_str = strdup(notice);
	return -1;
}

/*
** write_data_handler
**	This function is called as a callback when we want to dump
**	data to a buffer as a function of the eventloop.
*/
static void send_queued_write(int fd, int revents, void *data)
{
	aClient *to = data;

	if (IsDead(to))
		return;

	send_queued(to);
}

/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int  send_queued(aClient *to)
{
	char *msg;
	int  len, rlen;
	dbufbuf *block;

	/*
	   ** Once socket is marked dead, we cannot start writing to it,
	   ** even if the error is removed...
	 */
	if (IsDead(to))
	{
		/*
		   ** Actually, we should *NEVER* get here--something is
		   ** not working correct if send_queued is called for a
		   ** dead socket... --msa
		 */
		return -1;
	}

	while (DBufLength(&to->local->sendQ) > 0)
	{
		block = container_of(to->local->sendQ.dbuf_list.next, dbufbuf, dbuf_node);
		len = block->size;

		/* Returns always len > 0 */
		if ((rlen = deliver_it(to, block->data, len)) < 0)
		{
			char buf[256];
			snprintf(buf, 256, "Write error: %s", STRERROR(ERRNO));
			return dead_link(to, buf);
		}
		(void)dbuf_delete(&to->local->sendQ, rlen);
		to->local->lastsq = DBufLength(&to->local->sendQ) / 1024;
		if (rlen < block->size)
		{
			/* incomplete write due to EWOULDBLOCK, reschedule */
			fd_setselect(to->fd, FD_SELECT_WRITE, send_queued_write, to);
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
void sendto_one(aClient *to, char *pattern, ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_one(to, pattern, vl);
	va_end(vl);
}

void vsendto_one(aClient *to, const char *pattern, va_list vl)
{
	ircvsnprintf(sendbuf, sizeof(sendbuf), pattern, vl);
	sendbufto_one(to, sendbuf, 0);
}


/* sendbufto_one:
 * to: the client to which the buffer should be send
 * msg: the message
 * quick: normally set to 0, see later.
 * NOTES:
 * - neither to or msg can be NULL
 * - if quick is set to 0, the length is calculated, the string is cutoff
 *   at 510 bytes if needed, and \r\n is added if needed.
 *   if quick is >0 it is assumed the message has \r\n and 'quick' is used
 *   as length. Of course you should be very careful with that.
 */
void sendbufto_one(aClient *to, char *msg, unsigned int quick)
{
	int  len;
	Hook *h;
	
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

	if (!quick)
	{
		len = strlen(msg);
		if (!len || (msg[len - 1] != '\n'))
		{
			if (len > 510)
				len = 510;
			msg[len++] = '\r';
			msg[len++] = '\n';
			msg[len] = '\0';
		}
	} else
		len = quick;

	if (len > 512)
	{
		ircd_log(LOG_ERROR, "sendbufto_one: len=%u, quick=%u", len, quick);
		abort();
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
        for(h = Hooks[HOOKTYPE_PACKET]; h; h = h->next) {
		(*(h->func.intfunc))(&me, to, &msg, &len);
		if(!msg) return;
	}
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
	to->local->sendM += 1;
	me.local->sendM += 1;

	if (DBufLength(&to->local->sendQ) > 0)
		send_queued(to);
}

void sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
    char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int  i;

	++current_serial;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		/* skip the one and deaf clients (unless sendanyways is set) */
		if (acptr->from == one || (IsDeaf(acptr) && !(sendanyways == 1)))
			continue;
		if (MyConnect(acptr))	/* (It is always a client) */
			vsendto_prefix_one(acptr, from, pattern, vl);
		else if (acptr->from->local->serial != current_serial)
		{
			acptr->from->local->serial = current_serial;
			/*
			 * Burst messages comes here..
			 */
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
	}
}

void sendto_channel_butone_with_capability(aClient *one, unsigned int cap,
	aClient *from, aChannel *chptr, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int  i;

	++current_serial;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		/* skip the one and deaf clients (unless sendanyways is set) */
		if (acptr->from == one || (IsDeaf(acptr) && !(sendanyways == 1)))
			continue;
		if (!CHECKPROTO(acptr, cap))
			continue;
		if (MyConnect(acptr))	/* (It is always a client) */
		{
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
		else if (acptr->from->local->serial != current_serial)
		{
			acptr->from->local->serial = current_serial;
			/*
			 * Burst messages comes here..
			 */
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
	}
}

void sendto_channelprefix_butone(aClient *one, aClient *from, aChannel *chptr,
	int	prefix,
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
		if (acptr->from == one)
			continue;	/* ...was the one I should skip
					   or user not not a channel op */
		if (!prefix)
			goto good;
	        if ((prefix & PREFIX_HALFOP) && (lp->flags & CHFL_HALFOP))
			goto good;
		if ((prefix & PREFIX_VOICE) && (lp->flags & CHFL_VOICE))
			goto good;
		if ((prefix & PREFIX_OP) && (lp->flags & CHFL_CHANOP))
			goto good;
#ifdef PREFIX_AQ
		if ((prefix & PREFIX_ADMIN) && (lp->flags & CHFL_CHANPROT))
			goto good;
		if ((prefix & PREFIX_OWNER) && (lp->flags & CHFL_CHANOWNER))
			goto good;
#endif
		continue;
good:
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
#ifdef SECURECHANMSGSONLYGOTOSECURE
			for (h = Hooks[HOOKTYPE_CAN_SEND_SECURE]; h; h = h->next)
			{
				j = (*(h->func.intfunc))(acptr,chptr);
				if (j != HOOK_CONTINUE)
					break;
			}

			if (j != HOOK_CONTINUE)
				continue;
#endif
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (acptr->from->local->serial != current_serial)
			{
#ifdef SECURECHANMSGSONLYGOTOSECURE
				for (h = Hooks[HOOKTYPE_CAN_SEND_SECURE]; h; h = h->next)
				{
					j = (*(h->func.intfunc))(acptr,chptr);
					if (j != HOOK_CONTINUE)
						break;
				}

				if (j != HOOK_CONTINUE)
					continue;
#endif
				va_start(vl, pattern);
				vsendto_prefix_one(acptr, from, pattern, vl);
				va_end(vl);

				acptr->from->local->serial = current_serial;
			}
		}
	}
}

/*
   sendto_chanops_butone -Stskeeps
*/

void sendto_chanops_butone(aClient *one, aChannel *chptr, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;

	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		if (acptr == one || !(lp->flags & (CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
			continue;	/* ...was the one I should skip
					   or user not not a channel op */
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			va_start(vl, pattern);
			vsendto_one(acptr, pattern, vl);
			va_end(vl);
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
void
sendto_server(aClient *one, unsigned long caps,
	unsigned long nocaps, const char *format, ...)
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
		vsendto_one(cptr, format, vl);
		va_end(vl);
	}
}

/*
 * sendto_common_channels()
 *
 * Sends a message to all people (including user) on local server who are
 * in same channel with user.
 */
void sendto_common_channels(aClient *user, char *pattern, ...)
{
	va_list vl;

	Membership *channels;
	Member *users;
	aClient *cptr;
	int sendlen;

	/* We now create the buffer _before_ we send it to the clients. -- Syzop */
	*sendbuf = '\0';
	va_start(vl, pattern);
	sendlen = vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, user, pattern, vl);
	va_end(vl);

	++current_serial;
	if (MyConnect(user))
		user->local->serial = current_serial;
	if (user->user)
		for (channels = user->user->channel; channels; channels = channels->next)
			for (users = channels->chptr->members; users; users = users->next)
			{
				cptr = users->cptr;
				if (!MyConnect(cptr) || (cptr->local->serial == current_serial))
					continue;
				cptr->local->serial = current_serial;
				sendbufto_one(cptr, sendbuf, sendlen);
			}

	if (MyConnect(user))
		sendbufto_one(user, sendbuf, sendlen);

	return;
}

/*
 * sendto_common_channels_local_butone()
 *
 * Sends a message to all people on local server who are
 * in same channel with user and have the specified capability.
 */
void sendto_common_channels_local_butone(aClient *user, int cap, char *pattern, ...)
{
	va_list vl;

	Membership *channels;
	Member *users;
	aClient *cptr;
	int sendlen;

	/* We now create the buffer _before_ we send it to the clients. -- Syzop */
	*sendbuf = '\0';
	va_start(vl, pattern);
	sendlen = vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, user, pattern, vl);
	va_end(vl);

	++current_serial;
	if (MyConnect(user))
		user->local->serial = current_serial;
	if (user->user)
	{
		for (channels = user->user->channel; channels; channels = channels->next)
			for (users = channels->chptr->members; users; users = users->next)
			{
				cptr = users->cptr;
				if (!MyConnect(cptr) || (cptr->local->serial == current_serial) ||
				    !CHECKPROTO(cptr, cap))
					continue;
				cptr->local->serial = current_serial;
				sendbufto_one(cptr, sendbuf, sendlen);
			}
	}

	return;
}

/*
 * sendto_channel_butserv
 *
 * Send a message to all members of a channel that are connected to this
 * server.
 */

void sendto_channel_butserv(aChannel *chptr, aClient *from, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int sendlen;

	/* We now create the buffer _before_ we send it to the clients. Rather than
	 * rebuilding the buffer 1000 times for a 1000 local-users channel. -- Syzop
	 */
	*sendbuf = '\0';
	va_start(vl, pattern);
	sendlen = vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, from, pattern, vl);
	va_end(vl);

	for (lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->cptr))
			sendbufto_one(acptr, sendbuf, sendlen);

	return;
}

void sendto_channel_butserv_butone(aChannel *chptr, aClient *from, aClient *one, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;

	for (lp = chptr->members; lp; lp = lp->next)
	{
		if (lp->cptr == one)
			continue;

		if (MyConnect(acptr = lp->cptr))
		{
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
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
 * sendto_match_servs
 *
 * send to all servers which match the mask at the end of a channel name
 * (if there is a mask present) or to all if no mask.
 */
void sendto_match_servs(aChannel *chptr, aClient *from, char *format, ...)
{
	va_list vl;
	aClient *cptr;
	char *mask;

	if (chptr)
	{
		if (*chptr->chname == '&')
			return;
		if ((mask = (char *)rindex(chptr->chname, ':')))
			mask++;
	}
	else
		mask = (char *)NULL;

	list_for_each_entry(cptr, &server_list, special_node)
	{
		if (cptr == from)
			continue;
		if (!BadPtr(mask) && IsServer(cptr) && match(mask, cptr->name))
			continue;

		va_start(vl, format);
		vsendto_one(cptr, format, vl);
		va_end(vl);
	}
}

/*
 * sendto_match_butone
 *
 * Send to all clients which match the mask in a way defined on 'what';
 * either by user hostname or user servername.
 */
void sendto_match_butone(aClient *one, aClient *from, char *mask, int what,
    char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr, *acptr;
	char cansendlocal, cansendglobal;

	if (MyConnect(from))
	{
		cansendlocal = (ValidatePermissionsForPath("notice:local",from,NULL,NULL,NULL)) ? 1 : 0;
		cansendglobal = (ValidatePermissionsForPath("notice:global",from,NULL,NULL,NULL)) ? 1 : 0;
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

		sendto_server(one, 0, 0, "%s", buf);
	}

	/* To local clients... */
	if (cansendlocal)
	{
		list_for_each_entry(cptr, &lclient_list, lclient_node)
		{
			if (!IsMe(cptr) && (cptr != one) && IsRegisteredUser(cptr) && match_it(cptr, mask, what))
			{
				va_start(vl, pattern);
				vsendto_prefix_one(cptr, from, pattern, vl);
				va_end(vl);
			}
		}
	}
}

/*
 * sendto_all_butone.
 *
 * Send a message to all connections except 'one'. The basic wall type
 * message generator.
 */

void sendto_all_butone(aClient *one, aClient *from, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (!IsMe(cptr) && one != cptr)
		{
			va_start(vl, pattern);
			vsendto_prefix_one(cptr, from, pattern, vl);
			va_end(vl);
		}

	return;
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
			vsendto_one(cptr, nbuf, vl);
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
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
}

/*
 * sendto_umode
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
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		} else
		if (IsServer(cptr) && *modestr)
		{
			snprintf(nbuf, sizeof(nbuf), ":%s SENDUMODE %s :%s", me.name, modestr, pattern);
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	}
}

/*
 * sendto_umode_raw
 *
 *  Send to specified umode , raw, not a notice
 */
void sendto_umode_raw(int umodes, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (IsPerson(cptr) && (cptr->umodes & umodes) == umodes)
		{
			va_start(vl, pattern);
			vsendto_one(cptr, pattern, vl);
			va_end(vl);
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
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);
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
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);
	}

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_server(&me, 0, 0, ":%s SENDSNO %s :%s", me.name, snobuf, nbuf);
}

/** Send to specified snomask - local.
 * @param snomask Snomask to send to (can be a bitmask [AND])
 * @param pattern printf-style pattern, followed by parameters.
 * This function also delivers to non-opers w/the snomask if needed.
 */
void sendto_snomask_normal(int snomask, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[2048];

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (IsPerson(cptr) && (cptr->user->snomask & snomask))
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);
}

/** Send to specified snomask - global.
 * @param snomask Snomask to send to (can be a bitmask [AND])
 * @param pattern printf-style pattern, followed by parameters
 * This function also delivers to non-opers w/the snomask if needed.
 */
void sendto_snomask_normal_global(int snomask, char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[2048], snobuf[32], *p;

	va_start(vl, pattern);
	ircvsnprintf(nbuf, sizeof(nbuf), pattern, vl);
	va_end(vl);

	list_for_each_entry(cptr, &lclient_list, lclient_node)
		if (IsPerson(cptr) && (cptr->user->snomask & snomask))
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_server(&me, 0, 0, ":%s SENDSNO %s :%s", me.name, snobuf, nbuf);
}

/*
 * sendto_opers
 *
 *	Send to *local* ops only. (all +O or +o people)
 */
void sendto_opers(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	list_for_each_entry(cptr, &oper_list, special_node)
	{
		(void)ircsnprintf(nbuf, sizeof(nbuf), ":%s NOTICE %s :*** Oper -- ",
		    me.name, cptr->name);
		(void)strlcat(nbuf, pattern, sizeof nbuf);

		va_start(vl, pattern);
		vsendto_one(cptr, nbuf, vl);
		va_end(vl);
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
		vsendto_prefix_one(cptr->from, from, pattern, vl);
		va_end(vl);
	}
}

/*
** sendto_ops_butone
**	Send message to all operators regardless of whether they are +w or
**	not..
** one - client not to send message to
** from- client which message is from *NEVER* NULL!!
*/
void sendto_opers_butone(aClient *one, aClient *from, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;

	++current_serial;
	list_for_each_entry(cptr, &client_list, client_node)
	{
		if (!IsOper(cptr))
			continue;
		if (cptr->from->local->serial == current_serial)	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		cptr->from->local->serial = current_serial;

		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, pattern, vl);
		va_end(vl);
	}
}

/*
** sendto_ops_butme
**	Send message to all operators except local ones
** from- client which message is from *NEVER* NULL!!
*/
void sendto_ops_butme(aClient *from, char *pattern, ...)
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
		if (!strcmp(cptr->user->server, me.name))	/* a locop */
			continue;
		cptr->from->local->serial = current_serial;

		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, pattern, vl);
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

	if (from && from->user)
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

		/* Assuming 'pattern' always starts with ":%s ..." */
		if (!strcmp(&pattern[3], "%s"))
			strcpy(buf + strlen(buf), va_arg(vl, char *)); /* This can speed things up by 30% -- Syzop */
		else
			ircvsnprintf(buf + strlen(buf), buflen - strlen(buf), &pattern[3], vl);
	}
	else
		ircvsnprintf(buf, buflen, pattern, vl);

	len = strlen(buf);
	ADD_CRLF(buf, len);
	return len;
}

void vsendto_prefix_one(struct Client *to, struct Client *from,
    const char *pattern, va_list vl)
{
	if (to && from && MyClient(to) && from->user)
		vmakebuf_local_withprefix(sendbuf, sizeof sendbuf, from, pattern, vl);
	else
		ircvsnprintf(sendbuf, sizeof(sendbuf), pattern, vl);

	sendbufto_one(to, sendbuf, 0);
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

void sendto_prefix_one(aClient *to, aClient *from, const char *pattern, ...)
{
	va_list vl;
	va_start(vl, pattern);
	vsendto_prefix_one(to, from, pattern, vl);
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
		vsendto_one(cptr, nbuf, vl);
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
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, connect);
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
			sendto_one(cptr, ":%s NOTICE %s :%s", acptr->user->server, cptr->name, connect);
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
		sendto_one(cptr,
			":%s UID %s %d %ld %s %s %s %s %s %s %s %s :%s",
			sptr->srvptr->id, sptr->name, sptr->hopcount, sptr->lastnick,
			sptr->user->username, sptr->user->realhost, sptr->id,
			sptr->user->svid, umodes, vhost, getcloak(sptr),
			encode_ip(sptr->ip), sptr->info);
		return;
	}

	sendto_one(cptr,
		    "NICK %s %d %d %s %s %s %s %s %s %s%s%s%s:%s",
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

void	sendto_message_one(aClient *to, aClient *from, char *sender,
			char *cmd, char *nick, char *msg)
{
        sendto_prefix_one(to, from, ":%s %s %s :%s",
                         CHECKPROTO(to->from, PROTO_SID) ? ID(from) : from->name, cmd, nick, msg);
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
	vsendto_one(to, realpattern, vl);
	va_end(vl);
}

void sendtxtnumeric(aClient *to, char *pattern, ...)
{
static char realpattern[1024];
va_list vl;

	ircsnprintf(realpattern, sizeof(realpattern), ":%s %d %s :%s", me.name, RPL_TEXT, to->name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, realpattern, vl);
	va_end(vl);
}
