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

#ifndef lint
static char sccsid[] =
    "@(#)send.c	2.32 2/28/94 (C) 1988 University of Oulu, Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "h.h"
#include "msg.h"
#include <stdarg.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#endif

ID_CVS("$Id$");

void vsendto_one(aClient *to, char *pattern, va_list vl);
void sendbufto_one(aClient *to);
extern int sendanyways;
#ifndef NO_FDLIST
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;
#endif

#define NEWLINE	"\r\n"

static char sendbuf[2048];
static int send_message PROTO((aClient *, char *, int));

static int sentalong[MAXCONNECTIONS];

void vsendto_prefix_one(struct Client *to, struct Client *from,
    const char *pattern, va_list vl);

int  sentalong_marker;

/*
** dead_link
**	An error has been detected. The link *must* be closed,
**	but *cannot* call ExitClient (m_bye) from here.
**	Instead, mark it with FLAGS_DEADSOCKET. This should
**	generate ExitClient from the main loop.
**
**	If 'notice' is not NULL, it is assumed to be a format
**	for a message to local opers. I can contain only one
**	'%s', which will be replaced by the sockhost field of
**	the failing link.
**
**	Also, the notice is skipped for "uninteresting" cases,
**	like Persons and yet unknown connections...
*/
static int dead_link(to, notice)
	aClient *to;
	char *notice;
{
	to->flags |= FLAGS_DEADSOCKET;
	/*
	 * If because of BUFFERPOOL problem then clean dbuf's now so that
	 * notices don't hurt operators below.
	 */
	DBufClear(&to->recvQ);
	DBufClear(&to->sendQ);
	if (!IsPerson(to) && !IsUnknown(to) && !(to->flags & FLAGS_CLOSING))
		(void)sendto_failops_whoare_opers(notice, get_client_name(to, FALSE));
	Debug((DEBUG_ERROR, notice, get_client_name(to, FALSE)));
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
void flush_connections(fd)
	int  fd;
{
	int  i;
	aClient *cptr;

	if (fd == me.fd)
	{
		for (i = highest_fd; i >= 0; i--)
			if ((cptr = local[i]) && !(cptr->flags & FLAGS_BLOCKED)
			    && DBufLength(&cptr->sendQ) > 0)
				send_queued(cptr);
	}
	else if (fd >= 0 && (cptr = local[fd]) && !(cptr->flags & FLAGS_BLOCKED)
	    && DBufLength(&cptr->sendQ) > 0)
		send_queued(cptr);

}
/* flush an fdlist intelligently */
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


/*
** send_queued
**	This function is called from the main select-loop (or whatever)
**	when there is a chance the some output would be possible. This
**	attempts to empty the send queue as far as possible...
*/
int  send_queued(to)
	aClient *to;
{
	char *msg;
	int  len, rlen;

	if (IsBlocked(to))
		return;		/* Can't write to already blocked socket */

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
#ifndef SENDQ_ALWAYS
		return dead_link(to, "send_queued called for a DEADSOCKET:%s");
#else
		return -1;
#endif
	}
	while (DBufLength(&to->sendQ) > 0)
	{
		msg = dbuf_map(&to->sendQ, &len);
		/* Returns always len > 0 */
		if ((rlen = deliver_it(to, msg, len)) < 0)
			return dead_link(to, "Write error to %s, closing link");
		(void)dbuf_delete(&to->sendQ, rlen);
		to->lastsq = DBufLength(&to->sendQ) / 1024;
		if (rlen < len)
		{
			/* If we can't write full message, mark the socket
			 * as "blocking" and stop trying. -Donwulff */
			SetBlocked(to);
			break;
		}
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
	sendbufto_one(to);
}

void sendbufto_one(aClient *to)
{
	int  len;

	Debug((DEBUG_ERROR, "Sending [%s] to %s", sendbuf, to->name));

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

	len = strlen(sendbuf);
	if (sendbuf[len - 1] != '\n')
	{
		if (len > 510)
			len = 510;
		sendbuf[len++] = '\r';
		sendbuf[len++] = '\n';
		sendbuf[len] = '\0';
	}

	if (IsMe(to))
	{
		char tmp_sendbuf[sizeof(sendbuf)];

		strcpy(tmp_sendbuf, sendbuf);
		sendto_ops("Trying to send [%s] to myself!", tmp_sendbuf);
		return;
	}

	if (DBufLength(&to->sendQ) > get_sendq(to))
	{
		if (IsServer(to))
			sendto_ops("Max SendQ limit exceeded for %s: "
			    "%lu > %lu",
			    get_client_name(to, FALSE), DBufLength(&to->sendQ),
			    get_sendq(to));
		dead_link(to, "Max SendQ exceeded");
		return;
	}

	else if (!dbuf_put(&to->sendQ, sendbuf, len))
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
	if (to->acpt != &me)
		to->acpt->sendM += 1;
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
	Link *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);

	++sentalong_marker;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		/* ...was the one I should skip */
		if (acptr->from == one || (IsDeaf(acptr)
		    && !(sendanyways == 1)))
			continue;
		if (MyConnect(acptr))	/* (It is always a client) */
			vsendto_prefix_one(acptr, from, pattern, vl);
		else if (sentalong[(i = acptr->from->fd)] != sentalong_marker)
		{
			sentalong[i] = sentalong_marker;
			/*
			 * Burst messages comes here..
			 */
			vsendto_prefix_one(acptr, from, pattern, vl);
		}
	}
	va_end(vl);
}

/*
 * sendto_channelops_butone Added 1 Sep 1996 by Cabal95.
 *   Send a message to all OPs in channel chptr that
 *   are directly on this server and sends the message
 *   on to the next server if it has any OPs.
 *
 *   All servers must have this functional ability
 *    or one without will send back an error message. -- Cabal95
 */
void sendto_channelops_butone(aClient *one, aClient *from, aChannel *chptr,
    char *pattern, ...)
{
	va_list vl;
	Link *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);
	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->from == one || !(lp->flags & CHFL_CHANOP))
			continue;	/* ...was the one I should skip
					   or user not not a channel op */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			vsendto_prefix_one(acptr, from, pattern, vl);
			sentalong[i] = 1;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] == 0)
			{
				vsendto_prefix_one(acptr, from, pattern, vl);
				sentalong[i] = 1;
			}
		}
	}
	va_end(vl);
	return;
}

/*
 * sendto_channelvoice_butone
 * direct port of Cabal95's sendto_channelops_butone
 * to allow for /notice @+#channel messages
 * not exactly the most adventurous coding (made heavy use of copy-paste) <G>
 * but it's needed to avoid mass-msg trigger in script vnotices
 * -DuffJ
 */

void sendto_channelvoice_butone(aClient *one, aClient *from, aChannel *chptr,
    char *pattern, ...)
{
	va_list vl;
	Link *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);
	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->from == one || !(lp->flags & CHFL_VOICE))
			continue;	/* ...was the one I should skip
					   or user not (a channel voice or op) */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			vsendto_prefix_one(acptr, from, pattern, vl);
			sentalong[i] = 1;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] == 0)
			{
				vsendto_prefix_one(acptr, from, pattern, vl);
				sentalong[i] = 1;
			}
		}
	}
	va_end(vl);
	return;
}

/*
 * sendto_channelhalfop_butone
 * direct port of Cabal95's sendto_channelops_butone
 * to allow for /notice @+#channel messages
 * not exactly the most adventurous coding (made heavy use of copy-paste) <G>
 * but it's needed to avoid mass-msg trigger in script hnotices
 * -Stskeeps
 */

void sendto_channelhalfop_butone(aClient *one, aClient *from, aChannel *chptr,
    char *pattern, ...)
{
	va_list vl;
	Link *lp;
	aClient *acptr;
	int  i;

	va_start(vl, pattern);
	for (i = 0; i < MAXCONNECTIONS; i++)
		sentalong[i] = 0;
	for (lp = chptr->members; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->from == one || !(lp->flags & CHFL_HALFOP))
			continue;	/* ...was the one I should skip
					   or user not (a channel halfop or op) */
		i = acptr->from->fd;
		if (MyConnect(acptr) && IsRegisteredUser(acptr))
		{
			vsendto_prefix_one(acptr, from, pattern, vl);
			sentalong[i] = 1;
		}
		else
		{
			/* Now check whether a message has been sent to this
			 * remote link already */
			if (sentalong[i] == 0)
			{
				vsendto_prefix_one(acptr, from, pattern, vl);
				sentalong[i] = 1;
			}
		}
	}
	va_end(vl);
	return;
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
#ifndef NO_FDLIST
	int  j;
#endif
	static char tcmd[1024];
	static char ccmd[1024];
	static char buff[1024];
	static char pref[100];
	va_start(vl, pattern);

	pref[0] = '\0';
	if (strchr(prefix, '.'))
		ircsprintf(pref, "@%s", find_server_aln(prefix));

	strcpy(tcmd, token);
	strcpy(ccmd, command);
	strcat(tcmd, " ");
	strcat(ccmd, " ");
	ircvsprintf(buff, pattern, vl);
	strcat(tcmd, buff);
	strcat(ccmd, buff);

#ifdef NO_FDLIST
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
			if (IsToken(cptr))
			{
				if ((pref[0] != '\0') && SupportALN(cptr))
					sendto_one(cptr, "%s %s", pref, tcmd);
				else
					sendto_one(cptr, ":%s %s", prefix,
					    tcmd);
			}
			else
			{
				if ((pref[0] != '\0') && SupportALN(cptr))
					sendto_one(cptr, "%s %s", pref, ccmd);
				else
					sendto_one(cptr, ":%s %s", prefix,
					    ccmd);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr) && !DontSendQuit(cptr))
#else
		if (!DontSendQuit(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr) && !SupportSJOIN(cptr))
#else
		if (!SupportSJOIN(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportSJOIN(cptr))
#else
		if (SupportSJOIN(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr) && !SupportNICKv2(cptr))
#else
		if (!SupportNICKv2(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr) && SupportNICKv2(cptr))
#else
		if (SupportNICKv2(cptr))
#endif
			vsendto_one(cptr, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
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
	}
	va_end(vl);
	return;
}

/*
 * sendto_common_channels()
 * 
 * Sends a message to all people (inclusing user) on local server who are
 * in same channel with user.
 */
void sendto_common_channels(aClient *user, char *pattern, ...)
{
	va_list vl;

	Link *channels;
	Link *users;
	aClient *cptr;

	va_start(vl, pattern);
	memset((char *)sentalong, '\0', sizeof(sentalong));
	if (user->fd >= 0)
		sentalong[user->fd] = 1;
	if (user->user)
		for (channels = user->user->channel; channels;
		    channels = channels->next)
			for (users = channels->value.chptr->members; users;
			    users = users->next)
			{
				cptr = users->value.cptr;
				if (!MyConnect(cptr) || sentalong[cptr->fd])
					continue;
				sentalong[cptr->fd]++;
				vsendto_prefix_one(cptr, user, pattern, vl);
			}
	if (MyConnect(user))
		vsendto_prefix_one(user, user, pattern, vl);
	va_end(vl);
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
	Link *lp;
	aClient *acptr;

	for (va_start(vl, pattern), lp = chptr->members; lp; lp = lp->next)
		if (MyConnect(acptr = lp->value.cptr))
			vsendto_prefix_one(acptr, from, pattern, vl);
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
		if (mask = (char *)rindex(chptr->chname, ':'))
			mask++;
	}
	else
		mask = (char *)NULL;

	for (i = 0; i <= highest_fd; i++)
	{
		if (!(cptr = local[i]))
			continue;
		if ((cptr == from) || !IsServer(cptr))
			continue;
		if (!BadPtr(mask) && IsServer(cptr) && match(mask, cptr->name))
			continue;
		vsendto_one(cptr, format, vl);
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

	for (i = 0; i <= highest_fd; i++)
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
		vsendto_prefix_one(cptr, from, pattern, vl);
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

	for (va_start(vl, pattern), i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsMe(cptr) && one != cptr)
			vsendto_prefix_one(cptr, from, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendServNotice(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Notice -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
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
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
		}
	va_end(vl);
	return;
}

/*
 * sendto_chatops
 *
 *      Send to *local* mode +b ops only.
 */
/*
sendto_umode does just as good a job -- codemastr
  void    sendto_chatops(char *pattern, ...)
{
        va_list vl;
        aClient *cptr;
        int     i;
        char    nbuf[1024];

        va_start(vl,pattern);
        for (i = 0; i <= highest_fd; i++)
                if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
                    SendChatops(cptr))
                    {
                        (void)ircsprintf(nbuf, ":%s NOTICE %s :*** ChatOps -- ", 
                                        me.name, cptr->name);
                        (void)strncat(nbuf, pattern,
                                        sizeof(nbuf) - strlen(nbuf));
                        vsendto_one(cptr, nbuf, vl);
                    }
	va_end(vl);
        return;
} */

/*
 * sendto_helpops
 *
 *	Send to mode +h people
 */
void sendto_helpops(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsHelpOp(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** HelpOp -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
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
	int  w;
	va_start(vl, pattern);
	w = (umodes == UMODE_OPER | UMODE_CLIENT ? 1 : 0);
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (cptr->umodes & umodes) == umodes && ((w == 1)
		    && !IsHybNotice(cptr)))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
		}
	va_end(vl);
	return;
}

/*
 * sendto_conn_hcn
 *
 *  Send to umode +c && IsHybNotice(cptr)
 */
void sendto_conn_hcn(char *pattern, ...)
{
	va_list vl;
	aClient *cptr;
	int  i;
	char nbuf[1024];

	va_start(vl, pattern);
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    (cptr->umodes & UMODE_CLIENT) && IsHybNotice(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
		}
	va_end(vl);
	return;
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
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr) && IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Global -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
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
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    SendFailops(cptr) && IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** LocOps -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
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
	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsAnOper(cptr))
		{
			(void)ircsprintf(nbuf, ":%s NOTICE %s :*** Oper -- ",
			    me.name, cptr->name);
			(void)strncat(nbuf, pattern,
			    sizeof(nbuf) - strlen(nbuf));
			vsendto_one(cptr, nbuf, vl);
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
	for (i = 0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
		vsendto_prefix_one(cptr->from, from, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!IsAnOper(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (cptr->from == one)
			continue;	/* ...was the one I should skip */
		sentalong[i] = 1;
		vsendto_prefix_one(cptr->from, from, pattern, vl);
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
	for (i = 0; i <= highest_fd; i++)
		sentalong[i] = 0;
	for (cptr = client; cptr; cptr = cptr->next)
	{
		if (!SendWallops(cptr))
			continue;
		i = cptr->from->fd;	/* find connection oper is on */
		if (sentalong[i])	/* sent message along it already ? */
			continue;
		if (!strcmp(cptr->user->server, me.name))	/* a locop */
			continue;
		sentalong[i] = 1;
		vsendto_prefix_one(cptr->from, from, pattern, vl);
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
				
				    (void)strcat(sender,
				    (!IsHidden(from) ? user->realhost : user->
				    virthost));
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
			strcat(sender,
			    (!IsHidden(from) ? user->realhost : user->
			    virthost));
		}
		*sendbuf = ':';
		strcpy(&sendbuf[1], sender);
		/* Assuming 'pattern' always starts with ":%s ..." */
		ircvsprintf(sendbuf + strlen(sendbuf), &pattern[3], vl);
	}
	else
		ircvsprintf(sendbuf, pattern, vl);
	sendbufto_one(to);
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
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry;
	    i = oper_fdlist.entry[++j])
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
			vsendto_one(cptr, nbuf, vl);
		}
	va_end(vl);
	return;
}

void sendto_connectnotice(nick, user, sptr)
	char *nick;
	anUser *user;
	aClient *sptr;
{
	aClient *cptr;
	int  i;
	char connectd[1024];
	char connecth[1024];
	ircsprintf(connectd,
	    "*** Notice -- Client connecting on port %d: %s (%s@%s)",
	    sptr->acpt->port, nick, user->username, user->realhost);
	ircsprintf(connecth,
	    "*** Notice -- Client connecting: %s (%s@%s) [%s] {%d}", nick,
	    user->username, user->realhost, sptr->sockhost,
	    get_client_class(sptr));

	for (i = 0; i <= highest_fd; i++)
		if ((cptr = local[i]) && !IsServer(cptr) && !IsMe(cptr) &&
		    IsOper(cptr) && (cptr->umodes & UMODE_CLIENT))
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
void sendto_serv_butone_nickcmd(aClient *one, char *nick, int hopcount,
    int lastnick, char *username, char *realhost, char *server,
    long servicestamp, char *info, char *umodes, char *virthost)
{
	va_list vl;
	int  i;
	aClient *cptr;
#ifndef NO_FDLIST
	int  j;
#endif

#ifdef NO_FDLIST
	for (i = 0; i <= highest_fd; i++)
#else
	for (i = serv_fdlist.entry[j = 1]; j <= serv_fdlist.last_entry;
	    i = serv_fdlist.entry[++j])
#endif
	{
		if (!(cptr = local[i]) || (one && cptr == one->from))
			continue;
#ifdef NO_FDLIST
		if (IsServer(cptr))
#endif
		{
			if (SupportNICKv2(cptr))
			{
				sendto_one(cptr,
				    "%s %s %d %d %s %s %s %lu %s %s :%s",
				    (IsToken(cptr) ? TOK_NICK : MSG_NICK), nick,
				    hopcount, lastnick, username, realhost,
				    (SupportALN(cptr) ? find_server_aln(server)
				    : server), servicestamp, umodes, virthost,
				    info);
			}
			else
			{
				sendto_one(cptr, "%s %s %d %d %s %s %s %lu :%s",
				    (IsToken(cptr) ? TOK_NICK : MSG_NICK),
				    nick, hopcount, lastnick, username,
				    realhost,
				    (SupportALN(cptr) ? find_server_aln(server)
				    : server), servicestamp, info);
				if (strcmp(umodes, "+"))
				{
					sendto_one(cptr, ":%s %s %s :%s",
					    nick,
					    (IsToken(cptr) ? TOK_MODE :
					    MSG_MODE), nick, umodes);
				}
				if (strcmp(virthost, "*"))
				{
					sendto_one(cptr, ":%s %s %s",
					    nick,
					    (IsToken(cptr) ? TOK_SETHOST :
					    MSG_SETHOST), virthost);
				}
			}
		}
	}
	va_end(vl);
	return;
}
