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

/* -- Jto -- 16 Jun 1990
 * Added Armin's PRIVMSG patches...
 */

#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)send.c	2.32 2/28/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";
#endif

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

void vsendto_one(aClient *to, char *pattern, va_list vl);
void sendbufto_one(aClient *to, char *msg, unsigned int quick);

#define ADD_CRLF(buf, len) { if (len > 510) len = 510; \
                             buf[len++] = '\r'; buf[len++] = '\n'; buf[len] = '\0'; } while(0)

#ifndef NO_FDLIST
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;
#endif

#define NEWLINE	"\r\n"

static char sendbuf[2048];
static char tcmd[2048];
static char ccmd[2048];
static char xcmd[2048];
static char wcmd[2048];

/* this array is used to ensure we send a msg only once to a remote 
** server.  like, when we are sending a message to all channel members
** send the message to those that are directly connected to us and once 
** to each server that has these members.  the servers then forward the
** message to other servers and to those channel members that are directly
** connected to them
*/
static int sentalong[MAXCONNECTIONS];

void vsendto_prefix_one(struct Client *to, struct Client *from,
    const char *pattern, va_list vl);

MODVAR int  sentalong_marker;
MODVAR int  sendanyways = 0;
/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	notice will be the quit message. notice will also be
**	sent to failops in case 'to' is a server.
*/
static int dead_link(aClient *to, char *notice)
{
	
	to->flags |= FLAGS_DEADSOCKET;
	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		(void)sendto_failops_whoare_opers("Closing link: %s - %s",
			notice, get_client_name(to, FALSE));
	Debug((DEBUG_ERROR, "dead_link: %s - %s", notice, get_client_name(to, FALSE)));
	to->error_str = strdup(notice);
	return -1;
}

/*
** flush_connections
**	Used to empty all output buffers for all connections. Should only
**	be called once per scan of connections. There should be a select in
**	here perhaps but that means either forcing a timeout or doing a poll.
**	When flushing, all we do is empty the obuffer array for each local
**	client and try to send it. if we cant send it, it goes into the sendQ
**	-avalon
*/
void flush_connections(aClient* cptr)
{
	int  i;
	aClient *acptr;

	if (&me == cptr)
	{
		for (i = LastSlot; i >= 0; i--)
			if ((acptr = local[i]) && !(acptr->flags & FLAGS_BLOCKED)
			    && DBufLength(&acptr->sendQ) > 0)
				send_queued(acptr);
	}
	else if (cptr->fd >= 0 && !(cptr->flags & FLAGS_BLOCKED)
	    && DBufLength(&cptr->sendQ) > 0)
		send_queued(cptr);

}
/* flush an fdlist intelligently */
#ifndef NO_FDLIST
void flush_fdlist_connections(fdlist * listp)
{
	int  i, fd;
	aClient *cptr;

	for (fd = listp->entry[i = 1]; i <= listp->last_entry;
	    fd = listp->entry[++i])
		if ((cptr = local[fd]) && !(cptr->flags & FLAGS_BLOCKED)
		    && DBufLength(&cptr->sendQ) > 0)
			send_queued(cptr);
}
#endif

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
#ifdef ZIP_LINKS
	int more = 0;
#endif
	if (IsBlocked(to))
		return -1;		/* Can't write to already blocked socket */

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
#ifdef ZIP_LINKS
	/*
	** Here, we must make sure than nothing will be left in to->zip->outbuf
	** This buffer needs to be compressed and sent if all the sendQ is sent
	*/
	if ((IsZipped(to)) && to->zip->outcount) {
		if (DBufLength(&to->sendQ) > 0) {
			more = 1;
		} else {
			msg = zip_buffer(to, NULL, &len, 1);
			if (len == -1)
				return dead_link(to, "fatal error in zip_buffer()");
			if (!dbuf_put(&to->sendQ, msg, len))
				return dead_link(to, "Buffer allocation error");
		}
	}
#endif
	while (DBufLength(&to->sendQ) > 0)
	{
		msg = dbuf_map(&to->sendQ, &len);
		/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
		{
			char buf[256];
			snprintf(buf, 256, "Write error: %s", STRERROR(ERRNO));
			return dead_link(to, buf);
		}
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ) / 1024;
		if (rlen < len)
		{
			/* If we can't write full message, mark the socket
			 * as "blocking" and stop trying. -Donwulff */
			SetBlocked(to);
			break;
		}
#ifdef ZIP_LINKS
		if (DBufLength(&to->sendQ) == 0 && more) {
			/*
			** The sendQ is now empty, compress what's left
			** uncompressed and try to send it too
			*/
			more = 0;
			msg = zip_buffer(to, NULL, &len, 1);
			if (len == -1)
				return dead_link(to, "fatal error in zip_buffer()");
			if (!dbuf_put(&to->sendQ, msg, len))
				return dead_link(to, "Buffer allocation error");
		}
#endif
	}

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

void vsendto_one(aClient *to, char *pattern, va_list vl)
{
	ircvsprintf(sendbuf, pattern, vl);
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
	if (DBufLength(&to->sendQ) > get_sendq(to))
	{
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: %u > %d",
			    get_client_name(to, FALSE), DBufLength(&to->sendQ),
			    get_sendq(to));
		dead_link(to, "Max SendQ exceeded");
		return;
	}

#ifdef ZIP_LINKS
	/*
	** data is first stored in to->zip->outbuf until
	** it's big enough to be compressed and stored in the sendq.
	** send_queued is then responsible to never let the sendQ
	** be empty and to->zip->outbuf not empty.
	*/
	if (IsZipped(to))
		msg = zip_buffer(to, msg, &len, 0);
	
	if (len && !dbuf_put(&to->sendQ, msg, len))
#else
	if (!dbuf_put(&to->sendQ, msg, len))
#endif
	{
		dead_link(to, "Buffer allocation error");
		return;
	}
	/*
	 * Update statistics. The following is slightly incorrect
	 * because it counts messages even if queued, but bytes
	 * only really sent. Queued bytes get updated in SendQueued.
	 */
	to->sendM += 1;
	me.sendM += 1;
	if (to->listener != &me)
		to->listener->sendM += 1;
	/*
	 * This little bit is to stop the sendQ from growing too large when
	 * there is no need for it to. Thus we call send_queued() every time
	 * 2k has been added to the queue since the last non-fatal write.
	 * Also stops us from deliberately building a large sendQ and then
	 * trying to flood that link with data (possible during the net
	 * relinking done by servers with a large load).
	 */
	if (DBufLength(&to->sendQ) / 1024 > to->lastsq)
		send_queued(to);
}

void sendto_channel_butone(aClient *one, aClient *from, aChannel *chptr,
    char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);

	++sentalong_marker;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		/* skip the one and deaf clients (unless sendanyways is set) */
		if (acptr->from == one || (IsDeaf(acptr) && !(sendanyways == 1)))
			continue;
		if (MyConnect(acptr))	/* (It is always a client) */
			vsendto_prefix_one(acptr, from, pattern, vl);
		else if (sentalong[(i = acptr->from->slot)] != sentalong_marker)
		{
			sentalong[i] = sentalong_marker;
			/*
			 * Burst messages comes here..
			 */
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
	}
	va_end(vl);
}

void sendto_channelprefix_butone(aClient *one, aClient *from, aChannel *chptr,
	int	prefix,
    char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);

	++sentalong_marker;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		if (acptr->from == one)
			continue;	/* ...was the one I should skip
					   or user not not a channel op */
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
		i = acptr->from->slot;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
#ifdef SECURECHANMSGSONLYGOTOSECURE
			if (chptr->mode.mode & MODE_ONLYSECURE)
				if (!IsSecure(acptr))
					continue;
#endif
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
			sentalong[i] = sentalong_marker;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] != sentalong_marker)
			{
#ifdef SECURECHANMSGSONLYGOTOSECURE
				if (chptr->mode.mode & MODE_ONLYSECURE)
					if (!IsSecure(acptr->from))
						continue;
#endif
				va_start(vl, pattern);
				vsendto_prefix_one(acptr, from, pattern, vl);
				va_end(vl);
				sentalong[i] = sentalong_marker;
			}
		}
		va_end(vl);
	}
	va_end(vl);
	return;
}

void sendto_channelprefix_butone_tok(aClient *one, aClient *from, aChannel *chptr,
	int	prefix,
    char *cmd, char *tok, char *nick, char *text, char do_send_check)
{
	Member *lp;
	aClient *acptr;
	int  i;
	char is_ctcp = 0;
	unsigned int tlen, clen, xlen, wlen = 0;
	char *p;

	/* For servers with token capability */
	p = ircsprintf(tcmd, ":%s %s %s :%s", from->name, tok, nick, text);
	tlen = (int)(p - tcmd);
	ADD_CRLF(tcmd, tlen);

	/* For dumb servers without tokens */
	p = ircsprintf(ccmd, ":%s %s %s :%s", from->name, cmd, nick, text);
	clen = (int)(p - ccmd);
	ADD_CRLF(ccmd, clen);

	/* For our users... */
	if (IsPerson(from))
		p = ircsprintf(xcmd, ":%s!%s@%s %s %s :%s",
			from->name, from->user->username, GetHost(from), cmd, nick, text);
	else
		p = ircsprintf(xcmd, ":%s %s %s :%s", from->name, cmd, nick, text);
	xlen = (int)(p - xcmd);
	ADD_CRLF(xcmd, xlen);

	/* For our webtv friends... */
	if (!strcmp(cmd, "NOTICE"))
	{
		char *chan = strchr(nick, '#'); /* impossible to become NULL? */
		if (IsPerson(from))
			p = ircsprintf(wcmd, ":%s!%s@%s %s %s :%s",
				from->name, from->user->username, GetHost(from), MSG_PRIVATE, chan, text);
		else
			p = ircsprintf(wcmd, ":%s %s %s :%s", from->name, MSG_PRIVATE, chan, text);
		wlen = (int)(p - wcmd);
		ADD_CRLF(wcmd, wlen);
	}

	if (do_send_check && *text == 1 && myncmp(text+1,"ACTION ",7) && myncmp(text+1,"DCC ",4))
		is_ctcp = 1;


	++sentalong_marker;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;
		if (acptr->from == one)
			continue;	/* ...was the one I should skip
					   or user not not a channel op */
        if (prefix == PREFIX_ALL)
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
		i = acptr->from->slot;
		if (IsDeaf(acptr) && !sendanyways)
			continue;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			if (IsNoCTCP(acptr) && !IsOper(from) && is_ctcp)
				continue;

			if (IsWebTV(acptr) && wlen)
				sendbufto_one(acptr, wcmd, wlen);
			else
				sendbufto_one(acptr, xcmd, xlen);
			sentalong[i] = sentalong_marker;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] != sentalong_marker)
			{
				if (IsToken(acptr->from))
					sendbufto_one(acptr, tcmd, tlen);
				else
					sendbufto_one(acptr, ccmd, clen);
				sentalong[i] = sentalong_marker;
			}
		}
	}
	return;
}

/* weird channelmode +mu crap:
 * - local: deliver msgs to chanops (and higher) like <IRC> SrcNick: hi all
 * - remote: deliver msgs to every server once (if needed) with real sourcenick.
 * The problem is we can't send to remote servers with sourcenick (prefix) 'IRC'
 * because that's a virtual user... Fun... -- Syzop.
 */
void sendto_chmodemucrap(aClient *from, aChannel *chptr, char *text)
{
	Member *lp;
	aClient *acptr;
	int  i;
	int remote = MyClient(from) ? 0 : 1;

	sprintf(tcmd, ":%s %s %s :%s", from->name, TOK_PRIVATE, chptr->chname, text); /* token */
	sprintf(ccmd, ":%s %s %s :%s", from->name, MSG_PRIVATE, chptr->chname, text); /* msg */
	sprintf(xcmd, ":IRC!IRC@%s PRIVMSG %s :%s: %s", me.name, chptr->chname, from->name, text); /* local */

	++sentalong_marker;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->cptr;

		if (IsDeaf(acptr) && !sendanyways)
			continue;
		if (!(lp->flags & (CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANPROT)))
			continue;
		if (remote && (acptr->from == from->from)) /* don't send it back to where it came from */
			continue;
		i = acptr->from->slot;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			sendto_one(acptr, "%s", xcmd);
			sentalong[i] = sentalong_marker;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] != sentalong_marker)
			{
				if (IsToken(acptr->from))
					sendto_one(acptr, "%s", tcmd);
				else
					sendto_one(acptr, "%s", ccmd);
				sentalong[i] = sentalong_marker;
			}
		}
	}
	return;
}


/*
   sendto_chanops_butone -Stskeeps
*/

void sendto_chanops_butone(aClient *one, aChannel *chptr, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;

	va_start(vl, pattern);
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
	va_end(vl);
}

/*
 * sendto_server_butone
 *
 * Send a message to all connected servers except the client 'one'.
 */
void sendto_serv_butone(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif

	va_start(vl, pattern);
#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

/*
 * sendto_server_butone_token
 *
 * Send a message to all connected servers except the client 'one'.
 * with capab to tokenize
 */

void sendto_serv_butone_token(aClient *one, char *prefix, char *command,
    char *token, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
	aClient *acptr;
#ifndef NO_FDLIST
	int  j;
#endif
	static char buff[2048];
	static char pref[100];
	va_start(vl, pattern);

	pref[0] = '\0';
	if (strchr(prefix, '.'))
	{
		acptr = (aClient *) find_server_quick(prefix);
		if (acptr->serv->numeric)
		{
			strcpy(pref, base64enc(acptr->serv->numeric));
		}
	}
	strcpy(tcmd, token);
	strcpy(ccmd, command);
	strcat(tcmd, " ");
	strcat(ccmd, " ");
	ircvsprintf(buff, pattern, vl);
	strcat(tcmd, buff);
	strcat(ccmd, buff);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
			if (IsToken(cptr))
			{
				if (SupportNS(cptr) && pref[0])
				{
					sendto_one(cptr, "@%s %s",
						pref, tcmd);
				}
					else
				{
					sendto_one(cptr, ":%s %s",
						prefix, tcmd);
				}
			}
			else
			{
				if (SupportNS(cptr) && pref[0])
				{
					sendto_one(cptr, "@%s %s",
						pref, ccmd);
				}
				else
				{
					sendto_one(cptr, ":%s %s", prefix,
					    ccmd);
				}
			}
	}
	va_end(vl);
	return;
}

/*
 * sendto_server_butone_token_opt
 *
 * Send a message to all connected servers except the client 'one'.
 * with capab to tokenize, opt
 */

void sendto_serv_butone_token_opt(aClient *one, int opt, char *prefix, char *command,
    char *token, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
	aClient *acptr;
#ifndef NO_FDLIST
	int  j;
#endif
	static char tcmd[2048];
	static char ccmd[2048];
	static char buff[2048];
	static char pref[100];

	va_start(vl, pattern);

	pref[0] = '\0';
	if (strchr(prefix, '.'))
	{
		acptr = (aClient *) find_server_quick(prefix);
		if (acptr && acptr->serv)
			if (acptr->serv->numeric)
			{
				strcpy(pref, base64enc(acptr->serv->numeric));
			}
	}

	strcpy(tcmd, token);
	strcpy(ccmd, command);
	strcat(tcmd, " ");
	strcat(ccmd, " ");
	ircvsprintf(buff, pattern, vl);
	strcat(tcmd, buff);
	strcat(ccmd, buff);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif

		if ((opt & OPT_NOT_SJOIN) && SupportSJOIN(cptr))
			continue;
		if ((opt & OPT_NOT_NICKv2) && SupportNICKv2(cptr))
			continue;
		if ((opt & OPT_NOT_SJOIN2) && SupportSJOIN2(cptr))
			continue;
		if ((opt & OPT_NOT_UMODE2) && SupportUMODE2(cptr))
			continue;
		if ((opt & OPT_NOT_SJ3) && SupportSJ3(cptr))
			continue;
		if ((opt & OPT_NICKv2) && !SupportNICKv2(cptr))
			continue;
		if ((opt & OPT_SJOIN) && !SupportSJOIN(cptr))
			continue;
		if ((opt & OPT_SJOIN2) && !SupportSJOIN2(cptr))
			continue;
		if ((opt & OPT_UMODE2) && !SupportUMODE2(cptr))
			continue;
		if ((opt & OPT_SJ3) && !SupportSJ3(cptr))
			continue;
		if ((opt & OPT_SJB64) && !(cptr->proto & PROTO_SJB64))
			continue;
		if ((opt & OPT_NOT_SJB64) && (cptr->proto & PROTO_SJB64))
			continue;
		if ((opt & OPT_VHP) && !(cptr->proto & PROTO_VHP))
			continue;
		if ((opt & OPT_NOT_VHP) && (cptr->proto & PROTO_VHP))
			continue;
		if ((opt & OPT_TKLEXT) && !(cptr->proto & PROTO_TKLEXT))
			continue;
		if ((opt & OPT_NOT_TKLEXT) && (cptr->proto & PROTO_TKLEXT))
			continue;
		if ((opt & OPT_NICKIP) && !(cptr->proto & PROTO_TKLEXT))
			continue;
		if ((opt & OPT_NOT_NICKIP) && (cptr->proto & PROTO_NICKIP))
			continue;

		if (IsToken(cptr))
		{
			if (SupportNS(cptr) && pref[0])
			{
				sendto_one(cptr, "@%s %s",
					pref, tcmd);
			}
				else
			{
				sendto_one(cptr, ":%s %s",
					prefix, tcmd);
			}
		}
		else
		{
			if (SupportNS(cptr) && pref[0])
			{
				sendto_one(cptr, "@%s %s",
					pref, ccmd);
			}
			else
			{
				sendto_one(cptr, ":%s %s", prefix,
				    ccmd);
			}
		}
	}
	va_end(vl);
	return;
}

/*
 * sendto_serv_butone_quit
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT, don't send to NOQUIT servers.
 */
void sendto_serv_butone_quit(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, pattern);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && !DontSendQuit(cptr))
#else
		if (!DontSendQuit(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

/*
 * sendto_serv_butone_sjoin
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT, don't send to SJOIN servers.
 */
void sendto_serv_butone_sjoin(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, pattern);
#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && !SupportSJOIN(cptr))
#else
		if (!SupportSJOIN(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

/*
 * sendto_serv_sjoin
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT only send to SJOIN servers.
 */
void sendto_serv_sjoin(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, pattern);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportSJOIN(cptr))
#else
		if (SupportSJOIN(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

/*
 * sendto_serv_butone_nickv2
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT, don't send to NICKv2 servers.
 */
void sendto_serv_butone_nickv2(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, pattern);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && !SupportNICKv2(cptr))
#else
		if (!SupportNICKv2(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

/*
 * sendto_serv_nickv2
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT only send to NICKv2 servers.
 */
void sendto_serv_nickv2(aClient *one, char *pattern, ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, pattern);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, pattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportNICKv2(cptr))
#else
		if (SupportNICKv2(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}


/*
 * sendto_serv_nickv2_token
 *
 * Send a message to all connected servers except the client 'one'.
 * BUT only send to NICKv2 servers. As of Unreal3.1 it uses two patterns now
 * one for non token and one for tokens */
void sendto_serv_nickv2_token(aClient *one, char *pattern, char *tokpattern,
    ...)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif
	va_start(vl, tokpattern);

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
		va_start(vl, tokpattern);

#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportNICKv2(cptr) && !IsToken(cptr))
#else
		if (SupportNICKv2(cptr) && !IsToken(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
		else
#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportNICKv2(cptr) && IsToken(cptr))
#else
		if (SupportNICKv2(cptr) && IsToken(cptr))
#endif
			vsendto_one(cptr, tokpattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
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

	va_start(vl, pattern);

	++sentalong_marker;
	if (user->fd >= 0)
		sentalong[user->slot] = sentalong_marker;
	if (user->user)
		for (channels = user->user->channel; channels; channels = channels->next)
			for (users = channels->chptr->members; users; users = users->next)
			{
				cptr = users->cptr;
				if (!MyConnect(cptr) || (cptr->slot < 0) || (sentalong[cptr->slot] == sentalong_marker))
					continue;
				if ((channels->chptr->mode.mode & MODE_AUDITORIUM) &&
				    !(is_chanownprotop(user, channels->chptr) || is_chanownprotop(cptr, channels->chptr)))
					continue;
				sentalong[cptr->slot] = sentalong_marker;
				va_start(vl, pattern);
				vsendto_prefix_one(cptr, user, pattern, vl);
				va_end(vl);
			}
	if (MyConnect(user))
	{
		va_start(vl, pattern);
		vsendto_prefix_one(user, user, pattern, vl);
	}
	va_end(vl);
	return;
}
/*
 * sendto_channel_butserv
 *
 * Send a message to all members of a channel that are connected to this
 * server.
 */

//STOPPED HERE
void sendto_channel_butserv(aChannel *chptr, aClient *from, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;

	for (va_start(vl, pattern), lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->cptr))
		{
			va_start(vl, pattern);
			vsendto_prefix_one(acptr, from, pattern, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
}

void sendto_channel_butserv_butone(aChannel *chptr, aClient *from, aClient *one, char *pattern, ...)
{
	va_list vl;
	Member *lp;
	aClient *acptr;

	for (va_start(vl, pattern), lp = chptr->members; lp; lp = lp->next)
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
	va_end(vl);
	return;
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
	int  i;
	aClient *cptr;
	char *mask;

	va_start(vl, format);

	if (chptr)
	{
		if (*chptr->chname == '&')
			return;
		if ((mask = (char *)rindex(chptr->chname, ':')))
			mask++;
	}
	else
		mask = (char *)NULL;

	for (i = 0; i <= LastSlot; i++)
	{
		if (!(cptr = local[i]))
			continue;
		if ((cptr == from) || !IsServer(cptr))
			continue;
		if (!BadPtr(mask) && IsServer(cptr) && match(mask, cptr->name))
			continue;
		va_start(vl, format);
		vsendto_one(cptr, format, vl);
		va_end(vl);
	}
	va_end(vl);
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

	va_start(vl, pattern);
	if (MyConnect(from))
	{
		cansendlocal = (OPCanLNotice(from)) ? 1 : 0;
		cansendglobal = (OPCanGNotice(from)) ? 1 : 0;
	}
	else
		cansendlocal = cansendglobal = 1;

	for (i = 0; i <= LastSlot; i++)
	{
		if (!(cptr = local[i]))
			continue;	/* that clients are not mine */
		if (cptr == one)	/* must skip the origin !! */
			continue;
		if (IsServer(cptr))
		{
			if (!cansendglobal)
				continue;
			for (acptr = client; acptr; acptr = acptr->next)
				if (IsRegisteredUser(acptr)
				    && match_it(acptr, mask, what)
				    && acptr->from == cptr)
					break;
			/* a person on that server matches the mask, so we
			   ** send *one* msg to that server ...
			 */
			if (acptr == NULL)
				continue;
			/* ... but only if there *IS* a matching person */
		}
		/* my client, does he match ? */
		else if (!cansendlocal || (!(IsRegisteredUser(cptr) &&
		    match_it(cptr, mask, what))))
			continue;
		va_start(vl, pattern);
		vsendto_prefix_one(cptr, from, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
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
	int  i;
	aClient *cptr;

	for (va_start(vl, pattern), i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
		{
			va_start(vl, pattern);
			vsendto_prefix_one(cptr, from, pattern, vl);
			va_end(vl);
		}
	va_end(vl);
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
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) && SendServNotice(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Notice -- ", me.name, cptr->name);
			(void)strncat(nbuf, pattern, sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
}

/*
 * sendto_failops
 *
 *      Send to *local* mode +g ops only.
 */
void sendto_failops(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
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
	int  i;
	char nbuf[1024];
	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && IsPerson(cptr) && (cptr->umodes & umodes) == umodes)
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
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
	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && IsPerson(cptr) && (cptr->umodes & umodes) == umodes)
		{
			va_start(vl, pattern);
			vsendto_one(cptr, pattern, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
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
	int  i, j;
	char nbuf[2048];

	va_start(vl, pattern);
	ircvsprintf(nbuf, pattern, vl);
	va_end(vl);

	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
		if (((cptr = local[i])) && (cptr->user->snomask & snomask))
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);
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
	int  i, j;
	char nbuf[2048], snobuf[32], *p;

	va_start(vl, pattern);
	ircvsprintf(nbuf, pattern, vl);
	va_end(vl);

	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
		if (((cptr = local[i])) && (cptr->user->snomask & snomask))
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_serv_butone_token(NULL, me.name, MSG_SENDSNO, TOK_SENDSNO,
		"%s :%s", snobuf, nbuf);
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
	ircvsprintf(nbuf, pattern, vl);
	va_end(vl);

	for (i = LastSlot; i >= 0; i--)
		if ((cptr = local[i]) && IsPerson(cptr) && (cptr->user->snomask & snomask))
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
	ircvsprintf(nbuf, pattern, vl);
	va_end(vl);

	for (i = LastSlot; i >= 0; i--)
		if ((cptr = local[i]) && IsPerson(cptr) && (cptr->user->snomask & snomask))
			sendto_one(cptr, ":%s NOTICE %s :%s", me.name, cptr->name, nbuf);

	/* Build snomasks-to-send-to buffer */
	snobuf[0] = '\0';
	for (i = 0, p=snobuf; i<= Snomask_highest; i++)
		if (snomask & Snomask_Table[i].mode)
			*p++ = Snomask_Table[i].flag;
	*p = '\0';

	sendto_serv_butone_token(NULL, me.name, MSG_SENDSNO, TOK_SENDSNO,
		"%s :%s", snobuf, nbuf);
}


/*
 * sendto_failops_whoare_opers
 *
 *      Send to *local* mode +g ops only who are also +o.
 */
void sendto_failops_whoare_opers(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr) && IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
}
/*
 * sendto_locfailops
 *
 *      Send to *local* mode +g ops only who are also +o.
 */
void sendto_locfailops(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr) && IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** LocOps -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
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

	va_start(vl, pattern);
	for (i = 0; i <= LastSlot; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Oper -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
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

	va_start(vl, pattern);

	++sentalong_marker;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->slot;	/* find connection oper is on */
		if (sentalong[i] == sentalong_marker)	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = sentalong_marker;
		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
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

	va_start(vl, pattern);

	++sentalong_marker;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!IsAnOper(cptr))
			continue;
		i = cptr->from->slot;	/* find connection oper is on */
		if (sentalong[i] == sentalong_marker)	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = sentalong_marker;
		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
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

	va_start(vl, pattern);

	++sentalong_marker;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->slot;	/* find connection oper is on */
		if (sentalong[i] == sentalong_marker)	/* sent message along it already ? */
			continue;
		if (!strcmp(cptr->user->server, me.name))	/* a locop */
			continue;
		sentalong[i] = sentalong_marker;
		va_start(vl, pattern);
		vsendto_prefix_one(cptr->from, from, pattern, vl);
		va_end(vl);
	}
	va_end(vl);
	return;
}

void vsendto_prefix_one(struct Client *to, struct Client *from,
    const char *pattern, va_list vl)
{
	if (to && from && MyClient(to) && from->user)
	{
		static char sender[HOSTLEN + NICKLEN + USERLEN + 5];
		char *par;
		int  flag = 0;
		struct User *user = from->user;

		par = va_arg(vl, char *);
		strcpy(sender, from->name);
		if (user)
		{
			if (*user->username)
			{
				strcat(sender, "!");
				strcat(sender, user->username);
			}
			if ((IsHidden(from) ? *user->virthost : *user->realhost)
			    && !MyConnect(from))
			{
				strcat(sender, "@");
				(void)strcat(sender, GetHost(from));
				flag = 1;
			}
		}
		/*
		 * Flag is used instead of strchr(sender, '@') for speed and
		 * also since username/nick may have had a '@' in them. -avalon
		 */
		if (!flag && MyConnect(from)
		    && (IsHidden(from) ? *user->virthost : *user->realhost))
		{
			strcat(sender, "@");
			strcat(sender, GetHost(from));
		}
		*sendbuf = ':';
		strcpy(&sendbuf[1], sender);
		/* Assuming 'pattern' always starts with ":%s ..." */
		if (!strcmp(&pattern[3], "%s"))
			strcpy(sendbuf + strlen(sendbuf), va_arg(vl, char *)); /* This can speed things up by 30% -- Syzop */
		else
			ircvsprintf(sendbuf + strlen(sendbuf), &pattern[3], vl);
	}
	else
		ircvsprintf(sendbuf, pattern, vl);
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
	int  i;
#ifndef NO_FDLIST
	int  j;
#endif
	char nbuf[1024];

	va_start(vl, pattern);
#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
#endif
#ifdef NO_FDLIST
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsOper(cptr))
#else
		if ((cptr = local[i]))
#endif
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			va_start(vl, pattern);
			vsendto_one(cptr, nbuf, vl);
			va_end(vl);
		}
	va_end(vl);
	return;
}

void sendto_connectnotice(char *nick, anUser *user, aClient *sptr, int disconnect, char *comment)
{
	aClient *cptr;
	int  i, j;
	char connectd[1024];
	char connecth[1024];

	if (!disconnect)
	{
		RunHook(HOOKTYPE_LOCAL_CONNECT, sptr);
		ircsprintf(connectd,
		    "*** Notice -- Client connecting on port %d: %s (%s@%s) [%s] %s%s%s",
		    sptr->listener->port, nick, user->username, user->realhost,
		    sptr->class ? sptr->class->name : "",
#ifdef USE_SSL
		IsSecure(sptr) ? "[secure " : "",
		IsSecure(sptr) ? SSL_get_cipher((SSL *)sptr->ssl) : "",
		IsSecure(sptr) ? "]" : "");
#else
		"", "", "");
#endif
		ircsprintf(connecth,
		    "*** Notice -- Client connecting: %s (%s@%s) [%s] {%s}", nick,
		    user->username, user->realhost, Inet_ia2p(&sptr->ip),
		    sptr->class ? sptr->class->name : "0");
	}
	else 
	{
		ircsprintf(connectd, "*** Notice -- Client exiting: %s (%s@%s) [%s]",
			nick, user->username, user->realhost, comment);
		ircsprintf(connecth, "*** Notice -- Client exiting: %s (%s@%s) [%s] [%s]",
			nick, user->username, user->realhost, comment, Inet_ia2p(&sptr->ip));
	}

	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
		if (((cptr = local[i])) && (cptr->user->snomask & SNO_CLIENT))
		{
			if (IsHybNotice(cptr))
				sendto_one(cptr, ":%s NOTICE %s :%s", me.name,
				    cptr->name, connecth);
			else
				sendto_one(cptr, ":%s NOTICE %s :%s", me.name,
				    cptr->name, connectd);

		}
}

void sendto_fconnectnotice(char *nick, anUser *user, aClient *sptr, int disconnect, char *comment)
{
	aClient *cptr;
	int  i, j;
	char connectd[1024];
	char connecth[1024];

	if (!disconnect)
	{
		ircsprintf(connectd, "*** Notice -- Client connecting at %s: %s (%s@%s)",
			    user->server, nick, user->username, user->realhost);
		ircsprintf(connecth,
		    "*** Notice -- Client connecting at %s: %s (%s@%s) [%s] {0}", user->server, nick,
		    user->username, user->realhost, user->ip_str ? user->ip_str : "0");
	}
	else 
	{
		ircsprintf(connectd, "*** Notice -- Client exiting at %s: %s!%s@%s (%s)",
			   user->server, nick, user->username, user->realhost, comment);
		ircsprintf(connecth, "*** Notice -- Client exiting at %s: %s (%s@%s) [%s] [%s]",
			user->server, nick, user->username, user->realhost, comment,
			user->ip_str ? user->ip_str : "0");
	}

	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
		if (((cptr = local[i])) && (cptr->user->snomask & SNO_FCLIENT))
		{
			if (IsHybNotice(cptr))
				sendto_one(cptr, ":%s NOTICE %s :%s", me.name,
				    cptr->name, connecth);
			else
				sendto_one(cptr, ":%s NOTICE %s :%s", me.name,
				    cptr->name, connectd);

		}
}

/*
 * sendto_server_butone_nickcmd
 *
 * Send a message to all connected servers except the client 'one'.
 */
void sendto_serv_butone_nickcmd(aClient *one, aClient *sptr,
			char *nick, int hopcount,
    long lastnick, char *username, char *realhost, char *server,
    long servicestamp, char *info, char *umodes, char *virthost)
{
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif

#ifdef NO_FDLIST
	for (i = 0; i <= LastSlot; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry; i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
		{
			char *vhost;
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
				
			if (SupportNICKv2(cptr))
			{
				if (sptr->srvptr->serv->numeric && SupportNS(cptr))
					sendto_one(cptr,
						(cptr->proto & PROTO_SJB64) ?
					    /* Ugly double %s to prevent excessive spaces */
					    "%s %s %d %B %s %s %b %lu %s %s %s%s%s%s:%s"
					    :
					    "%s %s %d %lu %s %s %b %lu %s %s %s%s%s%s:%s"
					    ,
					    (IsToken(cptr) ? TOK_NICK : MSG_NICK), nick,
					    hopcount, (long)lastnick, username, realhost,
					    (long)(sptr->srvptr->serv->numeric),
					    servicestamp, umodes, vhost,
					    SupportCLK(cptr) ? getcloak(sptr) : "",
					    SupportCLK(cptr) ? " " : "",
					    SupportNICKIP(cptr) ? encode_ip(sptr->user->ip_str) : "",
					    SupportNICKIP(cptr) ? " " : "",
					    info);
				else
					sendto_one(cptr,
					    "%s %s %d %d %s %s %s %lu %s %s %s%s%s%s:%s",
					    (IsToken(cptr) ? TOK_NICK : MSG_NICK), nick,
					    hopcount, lastnick, username, realhost,
					    SupportNS(cptr) && sptr->srvptr->serv->numeric ? base64enc(sptr->srvptr->serv->numeric) : server,
					    servicestamp, umodes, vhost,
					    SupportCLK(cptr) ? getcloak(sptr) : "",
					    SupportCLK(cptr) ? " " : "",
					    SupportNICKIP(cptr) ? encode_ip(sptr->user->ip_str) : "",
					    SupportNICKIP(cptr) ? " " : "",
					    info);

			}
			else
			{
				sendto_one(cptr, "%s %s %d %d %s %s %s %lu :%s",
				    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
				    nick, hopcount, lastnick, username,
				    realhost,
				    server, servicestamp, info);
				if (strcmp(umodes, "+"))
				{
					sendto_one(cptr, ":%s %s %s :%s",
					    nick,
					    (IsToken(cptr) ? TOK_MODE :
					    MSG_MODE), nick, umodes);
				}
				if (IsHidden(sptr) && (sptr->umodes & UMODE_SETHOST))
				{
					sendto_one(cptr, ":%s %s %s",
					    nick,
					    (IsToken(cptr) ? TOK_SETHOST :
					    MSG_SETHOST), virthost);
				}
				else if (SupportVHP(cptr))
				{
					sendto_one(cptr, ":%s %s %s",
					    nick,
					    (IsToken(cptr) ? TOK_SETHOST :
					     MSG_SETHOST), (IsHidden(sptr) ? virthost :
					     realhost));
				}
			}
		}
	}
	return;
}

/*
 * sendto_one_nickcmd
 *
 */
void sendto_one_nickcmd(aClient *cptr, aClient *sptr, char *umodes)
{
	if (SupportNICKv2(cptr))
	{
		char *vhost;
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
		if (sptr->srvptr->serv->numeric && SupportNS(cptr))
			sendto_one(cptr,
				(cptr->proto & PROTO_SJB64) ?
			    /* Ugly double %s to prevent excessive spaces */
			    "%s %s %d %B %s %s %b %lu %s %s %s%s:%s"
			    :
			    "%s %s %d %lu %s %s %b %lu %s %s %s%s:%s"
			    ,
			    (IsToken(cptr) ? TOK_NICK : MSG_NICK), sptr->name,
			    sptr->hopcount+1, (long)sptr->lastnick, sptr->user->username, 
			    sptr->user->realhost, (long)(sptr->srvptr->serv->numeric),
			    sptr->user->servicestamp, umodes, vhost,
			    SupportNICKIP(cptr) ? encode_ip(sptr->user->ip_str) : "",
			    SupportNICKIP(cptr) ? " " : "", sptr->info);
		else
			sendto_one(cptr,
			    "%s %s %d %d %s %s %s %lu %s %s %s%s:%s",
			    (IsToken(cptr) ? TOK_NICK : MSG_NICK), sptr->name,
			    sptr->hopcount+1, sptr->lastnick, sptr->user->username, 
			    sptr->user->realhost, SupportNS(cptr) && 
			    sptr->srvptr->serv->numeric ? base64enc(sptr->srvptr->serv->numeric)
			    : sptr->user->server, sptr->user->servicestamp, umodes, vhost,
			    SupportNICKIP(cptr) ? encode_ip(sptr->user->ip_str) : "",
			    SupportNICKIP(cptr) ? " " : "", sptr->info);
	}
	else
	{
		sendto_one(cptr, "%s %s %d %d %s %s %s %lu :%s",
		    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
		    sptr->name, sptr->hopcount+1, sptr->lastnick, sptr->user->username,
		    sptr->user->realhost, sptr->user->server, sptr->user->servicestamp, 
		    sptr->info);
		if (strcmp(umodes, "+"))
		{
			sendto_one(cptr, ":%s %s %s :%s",
			    sptr->name, (IsToken(cptr) ? TOK_MODE :
			    MSG_MODE), sptr->name, umodes);
		}
		if (IsHidden(sptr) && (sptr->umodes & UMODE_SETHOST))
		{
			sendto_one(cptr, ":%s %s %s",
			    sptr->name, (IsToken(cptr) ? TOK_SETHOST :
			    MSG_SETHOST), sptr->user->virthost);
		}
		else if (SupportVHP(cptr))
		{
			sendto_one(cptr, ":%s %s %s", sptr->name, 
			    (IsToken(cptr) ? TOK_SETHOST : MSG_SETHOST),
			    (IsHidden(sptr) ? sptr->user->virthost :
			    sptr->user->realhost));
		}
	}
	return;
}

void	sendto_message_one(aClient *to, aClient *from, char *sender,
			char *cmd, char *nick, char *msg)
{
        if(IsServer(to->from) && IsToken(to->from)) {
          if(*cmd == 'P') cmd = TOK_PRIVATE;
          if(*cmd == 'N') cmd = TOK_NOTICE;
        }
        sendto_prefix_one(to, from, ":%s %s %s :%s",
                         sender, cmd, nick, msg);
}

/* sidenote: sendnotice() and sendtxtnumeric() assume no client or server
 * has a % in their nick, which is a safe assumption since % is illegal.
 */
 
void sendnotice(aClient *to, char *pattern, ...)
{
static char realpattern[1024];
va_list vl;
char *name = *to->name ? to->name : "*";

	if (!IsWebTV(to))
		ircsprintf(realpattern, ":%s NOTICE %s :%s", me.name, name, pattern);
	else
		ircsprintf(realpattern, ":%s PRIVMSG %s :%s", me.name, name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, realpattern, vl);
	va_end(vl);
}

void sendtxtnumeric(aClient *to, char *pattern, ...)
{
static char realpattern[1024];
va_list vl;

	if (!IsWebTV(to))
		ircsprintf(realpattern, ":%s %d %s :%s", me.name, RPL_TEXT, to->name, pattern);
	else
		ircsprintf(realpattern, ":%s PRIVMSG %s :%s", me.name, to->name, pattern);

	va_start(vl, pattern);
	vsendto_one(to, realpattern, vl);
	va_end(vl);
}
