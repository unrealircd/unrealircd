/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/webtv.c
 *   (C) Carsten V. Munk (Stskeeps <stskeeps@tspre.org>) 2000
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include "userload.h"
#include "version.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"

ID_Copyright("(C) Carsten Munk 2000");

extern ircstats IRCstats;

typedef struct zMessage aMessage;
struct zMessage {
	char *command;
	int  (*func) ();
	int  maxpara;
};


int	w_whois(aClient *cptr, aClient *sptr, int parc, char *parv[]);

aMessage	webtv_cmds[] = 
{
	{"WHOIS", w_whois, 15},
	{NULL, 0, 15}
};


int	webtv_parse(aClient *sptr, char *string)
{
	char *cmd = NULL, *s = NULL;
	int i;
	aMessage *message = webtv_cmds;
	static char *para[16];
	
	if (!string || !*string)
	{
		sendto_one(sptr, ":IRC %s %s :No command given", MSG_PRIVATE, sptr->name);
		return;
	}
	
	cmd = strtok(string, " ");
	if (!cmd)
		return -2;	
		
	for (message = webtv_cmds; message->command; message++)
		if (strcasecmp(message->command, cmd) == 0)
			break;

	if (!message->command || !message->func)
 	{
/*		sendto_one(sptr, ":IRC %s %s :Sorry, \"%s\" is an unknown command to me",
			MSG_PRIVATE, sptr->name, cmd); */
		/* restore the string*/
		cmd[strlen(cmd)]= ' ';
		return -2;
	}

	i = 0;
	s = strtok(NULL, "");
	if (s)
	{
		if (message->maxpara > 15)
			message->maxpara = 15;
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
			if (i >= message->maxpara)
				break;
			for (; *s != ' ' && *s; s++)
				;
		}
	}
	para[++i] = NULL;

	(*message->func) (sptr->from, sptr, i, para);
	return 0;
}

int	w_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static anUser UnknownUser = {
		NULL,		/* nextu */
		NULL,		/* channel */
		NULL,		/* invited */
		NULL,		/* silence */
		NULL,		/* away */
		0,		/* last */
		0,		/* servicestamp */
		1,		/* refcount */
		0,		/* joined */
		"<Unknown>",	/* username */
		"<Unknown>",	/* host */
		"<Unknown>"	/* server */
	};
	Link *lp;
	anUser *user;
	aClient *acptr, *a2cptr;
	aChannel *chptr;
	char *nick, *tmp, *name;
	char *p = NULL;
	char buf[512];
	int  found, len, mlen;

	if (parc < 2)
	{
		sendto_one(sptr, ":IRC %s %s :Syntax error, correct is WHOIS <nick>", 
			MSG_PRIVATE, sptr->name);
		return 0;
	}

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		int  invis, showsecret, showperson, member, wilds;

		found = 0;
		(void)collapse(nick);
		wilds = (index(nick, '?') || index(nick, '*'));
		if (wilds)
			continue;

		for (acptr = client; (acptr = next_client(acptr, nick));
		    acptr = acptr->next)
		{
			if (IsServer(acptr))
				continue;
			/*
			 * I'm always last :-) and acptr->next == NULL!!
			 */
			if (IsMe(acptr))
				break;
			/*
			 * 'Rules' established for sending a WHOIS reply:
			 *
			 * - only allow a remote client to get replies for
			 *   local clients if wildcards are being used;
			 *
			 * - if wildcards are being used dont send a reply if
			 *   the querier isnt any common channels and the
			 *   client in question is invisible and wildcards are
			 *   in use (allow exact matches only);
			 *
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */
			if (!MyConnect(sptr) && !MyConnect(acptr) && wilds)
				continue;
			user = acptr->user ? acptr->user : &UnknownUser;
			name = (!*acptr->name) ? "?" : acptr->name;

			invis = acptr != sptr && IsInvisible(acptr);
			member = (user->channel) ? 1 : 0;
			showperson = (wilds && !invis && !member) || !wilds;

			for (lp = user->channel; lp; lp = lp->next)
			{
				chptr = lp->value.chptr;
				member = IsMember(sptr, chptr);
				if (invis && !member)
					continue;
				if (member || (!invis && PubChannel(chptr)))
				{
					showperson = 1;
					break;
				}
				if (!invis && HiddenChannel(chptr) &&
				    !SecretChannel(chptr))
					showperson = 1;
				else if (IsAnOper(sptr) && SecretChannel(chptr)) {
					showperson = 1;
					showsecret = 1;
				}
			}
			if (!showperson)
				continue;
			a2cptr = find_server_quick(user->server);

			if (!IsPerson(acptr))
				continue;
			sendto_one(sptr, ":IRC PRIVMSG %s :WHOIS information for %s", sptr->name, acptr->name);
			if (IsWhois(acptr))
			{
				sendto_one(acptr,
				    ":%s NOTICE %s :*** %s (%s@%s) did a /whois on you.",
				    me.name, acptr->name, sptr->name,
				    sptr->user->username,
				    IsHidden(acptr) ? sptr->user->
				    virthost : sptr->user->realhost);
			}

			sendto_one(sptr, ":IRC PRIVMSG %s :%s is %s@%s * %s", sptr->name, 
				name, user->username, 
				    IsHidden(acptr) ? user->virthost : user->realhost,
				    acptr->info);

			if (IsEyes(sptr))
			{
				/* send the target user's modes */
				sendto_one(sptr, ":IRC PRIVMSG %s :%s uses modes %s",
					sptr->name, acptr->name, get_mode_str(acptr));
			}
			if (IsAnOper(sptr) && IsHidden(acptr) ||
			    acptr == sptr && IsHidden(sptr))
			{
				sendto_one(sptr, ":IRC PRIVMSG %s :%s is connecting from %s",
				    sptr->name, acptr->name,
				    acptr->user->realhost);
			}
			if (IsARegNick(acptr))
				sendto_one(sptr, ":IRC PRIVMSG %s :%s is a registered nick",
					sptr->name, name);
			found = 1;
			mlen = strlen(me.name) + strlen(sptr->name) + 6 +
			    strlen(name);
			for (len = 0, *buf = '\0', lp = user->channel; lp;
			    lp = lp->next)
			{
				chptr = lp->value.chptr;
				if (IsAnOper(sptr) || ShowChannel(sptr, chptr) || (acptr == sptr))
				{
					if (len + strlen(chptr->chname)
					    > (size_t)BUFSIZE - 4 - mlen)
					{
						sendto_one(sptr,
						    ":IRC PRIVMSG %s :%s is on %s",
						    sptr->name, name, buf);
						*buf = '\0';
						len = 0;
					}
					if (!(acptr == sptr) && IsAnOper(sptr)
					&& (showsecret == 1) && SecretChannel(chptr))
						*(buf + len++) = '~';
					if (is_chanowner(acptr, chptr))
						*(buf + len++) = '*';
					else if (is_chanprot(acptr, chptr))
						*(buf + len++) = '^';
					else if (is_chan_op(acptr, chptr))
						*(buf + len++) = '@';
					else if (is_half_op(acptr, chptr))
						*(buf + len++) = '%';
					else if (has_voice(acptr, chptr))
						*(buf + len++) = '+';
					if (len)
						*(buf + len) = '\0';
					(void)strcpy(buf + len, chptr->chname);
					len += strlen(chptr->chname);
					(void)strcat(buf + len, " ");
					len++;
				}
			}

			if (buf[0] != '\0' && !IsULine(acptr) && (!IsHiding(acptr) ||
				IsNetAdmin(sptr) || sptr == acptr))
				sendto_one(sptr, 
					":IRC PRIVMSG %s :%s is on %s",
						sptr->name, name, buf);

			sendto_one(sptr, ":IRC PRIVMSG %s :%s is on irc via %s %s",
				sptr->name, name, user->server,
			    a2cptr ? a2cptr->info : "*Not On This Net*");

			if (user->away)
				sendto_one(sptr, ":IRC PRIVMSG %s :%s is away: %s", 
					sptr->name, name, user->away);
			/* makesure they aren't +H (we'll also check 
			   before we display a helpop or IRCD Coder msg) 
			   -- codemastr */
			if ((IsAnOper(acptr) || IsServices(acptr))
			    && (!IsHideOper(acptr) || sptr == acptr || IsAnOper(sptr)))
			{
				buf[0] = '\0';
				if (IsNetAdmin(acptr))
					strcat(buf, "a Network Administrator");
				else if (IsSAdmin(acptr))
					strcat(buf, "a Services Operator");
				else if (IsAdmin(acptr) && !IsCoAdmin(acptr))
					strcat(buf, "a Server Administrator");
				else if (IsCoAdmin(acptr))
					strcat(buf, "a Co Administrator");
				else if (IsServices(acptr))
					strcat(buf, "a Network Service");
				else if (IsOper(acptr))
					strcat(buf, "an IRC Operator");

				else
					strcat(buf, "a Local IRC Operator");
				if (buf[0])
					sendto_one(sptr,
						":IRC PRIVMSG %s :%s is an %s on %s",
						sptr->name, name, buf, ircnetwork);
			}

			if (IsHelpOp(acptr) && (!IsHideOper(acptr) || sptr == acptr || IsAnOper(sptr)))
				if (!acptr->user->away)
					sendto_one(sptr, ":IRC PRIVMSG %s :%s is available for help.", 
						sptr->name, acptr->name);

			if (acptr->umodes & UMODE_BOT)
			{
				sendto_one(sptr, ":IRC PRIVMSG %s :%s is an Bot on %s",
					sptr->name, name, ircnetwork);
			}
			if (acptr->umodes & UMODE_SECURE)
			{
				sendto_one(sptr, ":IRC PRIVMSG %s :%s is a Secure Connection",
					sptr->name, acptr->name);
			}
			if (acptr->user->swhois)
			{
				if (*acptr->user->swhois != '\0')
					sendto_one(sptr, ":IRC PRIVMSG %s :%s %s",
						sptr->name, acptr->name, 
						acptr->user->swhois);
			}

			if (acptr->user && MyConnect(acptr))
				sendto_one(sptr, ":IRC PRIVMSG %s :%s has been idle for %s signed on at %s",
					sptr->name, acptr->name,
					(char *)convert_time(TStime() - user->last),
					date(acptr->firsttime));
		}
		if (!found)
		{
			sendto_one(sptr, ":IRC PRIVMSG %s :%s - No such nick",
				sptr->name, nick);
		}
		if (p)
			p[-1] = ',';
	}
	sendto_one(sptr, ":IRC PRIVMSG %s :End of whois information for %s",
		sptr->name, parv[1]);

	return 0;
}
