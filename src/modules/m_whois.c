/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_whois.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

static char buf[BUFSIZE];

DLLFUNC int m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_WHOIS       "WHOIS" /* WHOI */
#define TOK_WHOIS       "#"     /* 35 */


#ifndef DYNAMIC_LINKING
ModuleHeader m_whois_Header
#else
#define m_whois_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"whois",	/* Name of module */
	"$Id$", /* Version */
	"command /whois", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_whois_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_WHOIS, TOK_WHOIS, m_whois, MAXPARA);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_whois_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_whois_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_WHOIS, TOK_WHOIS, m_whois) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_whois_Header.name);
	}
	return MOD_SUCCESS;
}


/*
** m_whois
**	parv[0] = sender prefix
**	parv[1] = nickname masklist
*/
DLLFUNC int  m_whois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	static anUser UnknownUser = {
		NULL,		/* channel */
		NULL,		/* invited */
		NULL,		/* silence */
		NULL,		/* away */
#ifdef NO_FLOOD_AWAY
		0,		/* last_away */
		0,		/* away_count */
#endif
		0,		/* servicestamp */
		1,		/* refcount */
		0,		/* joined */
		"<Unknown>",	/* username */
		"<Unknown>",	/* host */
		"<Unknown>"	/* server */
	};
	Membership *lp;
	anUser *user;
	aClient *acptr, *a2cptr;
	aChannel *chptr;
	char *nick, *tmp, *name;
	char *p = NULL;
	int  found, len, mlen;

	if (IsServer(sptr))	
		return 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		    me.name, parv[0]);
		return 0;
	}

	if (parc > 2)
	{
		if (hunt_server_token(cptr, sptr, MSG_WHOIS, TOK_WHOIS, "%s :%s", 1, parc,
		    parv) != HUNTED_ISME)
			return 0;
		parv[1] = parv[2];
	}

	for (tmp = parv[1]; (nick = strtoken(&p, tmp, ",")); tmp = NULL)
	{
		int  invis, showchannel, member, wilds;

		found = 0;
		/* We do not support "WHOIS *" */
		wilds = (index(nick, '?') || index(nick, '*'));
		if (wilds)
			continue;

		if ((acptr = find_client(nick, NULL)))
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
			 * - only send replies about common or public channels
			 *   the target user(s) are on;
			 */

			user = acptr->user ? acptr->user : &UnknownUser;
			name = (!*acptr->name) ? "?" : acptr->name;

			invis = acptr != sptr && IsInvisible(acptr);
			member = (user->channel) ? 1 : 0;

			a2cptr = find_server_quick(user->server);

			if (!IsPerson(acptr))
				continue;

			if (IsWhois(acptr) && (sptr != acptr))
			{
				sendto_one(acptr,
				    ":%s %s %s :*** %s (%s@%s) did a /whois on you.",
				    me.name, IsWebTV(acptr) ? "PRIVMSG" : "NOTICE", acptr->name, sptr->name,
				    sptr->user->username, sptr->user->realhost);
			}

			sendto_one(sptr, rpl_str(RPL_WHOISUSER), me.name,
			    parv[0], name,
			    user->username,
			    IsHidden(acptr) ? user->virthost : user->realhost,
			    acptr->info);

			if (IsEyes(sptr) && IsOper(sptr))
			{
				/* send the target user's modes */
				sendto_one(sptr, rpl_str(RPL_WHOISMODES),
				    me.name, parv[0], name,
				    get_mode_str(acptr));
			}
			if (IsHidden(acptr) && ((acptr == sptr) || IsAnOper(sptr))) 
			{
				sendto_one(sptr, rpl_str(RPL_WHOISHOST),
				    me.name, parv[0], acptr->name,
				    user->realhost);
			}

			if (IsARegNick(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISREGNICK), me.name, parv[0], name);
			
			found = 1;
			mlen = strlen(me.name) + strlen(parv[0]) + 10 + strlen(name);
			for (len = 0, *buf = '\0', lp = user->channel; lp; lp = lp->next)
			{
				chptr = lp->chptr;
				showchannel = 0;
				if (ShowChannel(sptr, chptr))
					showchannel = 1;
#ifndef SHOW_SECRET
				if (IsAnOper(sptr) && !SecretChannel(chptr))
#else
				if (IsAnOper(sptr))
#endif
					showchannel = 1;
				if ((acptr->umodes & UMODE_HIDEWHOIS) && !IsMember(sptr, chptr) && !IsAnOper(sptr))
					showchannel = 0;
				if (IsServices(acptr) && !IsNetAdmin(sptr))
					showchannel = 0;
				if (acptr == sptr)
					showchannel = 1;
				/* Hey, if you are editting here... don't forget to change the webtv w_whois ;p. */
				
				if (showchannel)
				{
					long access;
					if (len + strlen(chptr->chname) > (size_t)BUFSIZE - 4 - mlen)
					{
						sendto_one(sptr,
						    ":%s %d %s %s :%s",
						    me.name,
						    RPL_WHOISCHANNELS,
						    parv[0], name, buf);
						*buf = '\0';
						len = 0;
					}
#ifdef SHOW_SECRET
					if (IsAnOper(sptr)
#else
					if (IsNetAdmin(sptr)
#endif
					    && SecretChannel(chptr) && !IsMember(sptr, chptr))
						*(buf + len++) = '?';
					if (acptr->umodes & UMODE_HIDEWHOIS && !IsMember(sptr, chptr)
						&& IsAnOper(sptr))
						*(buf + len++) = '!';
					access = get_access(acptr, chptr);
#ifndef PREFIX_AQ
					if (access & CHFL_CHANOWNER)
						*(buf + len++) = '*';
					else if (access & CHFL_CHANPROT)
						*(buf + len++) = '^';
#else
					if (access & CHFL_CHANOWNER)
						*(buf + len++) = '~';
					else if (access & CHFL_CHANPROT)
						*(buf + len++) = '&';
#endif
					else if (access & CHFL_CHANOP)
						*(buf + len++) = '@';
					else if (access & CHFL_HALFOP)
						*(buf + len++) = '%';
					else if (access & CHFL_VOICE)
						*(buf + len++) = '+';
					if (len)
						*(buf + len) = '\0';
					(void)strcpy(buf + len, chptr->chname);
					len += strlen(chptr->chname);
					(void)strcat(buf + len, " ");
					len++;
				}
			}

			if (buf[0] != '\0')
				sendto_one(sptr, rpl_str(RPL_WHOISCHANNELS), me.name, parv[0], name, buf);

			sendto_one(sptr, rpl_str(RPL_WHOISSERVER),
			    me.name, parv[0], name, user->server,
			    a2cptr ? a2cptr->info : "*Not On This Net*");

			if (user->away)
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
				    parv[0], name, user->away);
			/* makesure they aren't +H (we'll also check
			   before we display a helpop or IRCD Coder msg)
			   -- codemastr */
			if ((IsAnOper(acptr) || IsServices(acptr))
			    && (!IsHideOper(acptr) || sptr == acptr
			    || IsAnOper(sptr)))
			{
				buf[0] = '\0';
				if (IsNetAdmin(acptr))
					strlcat(buf, "a Network Administrator", sizeof buf);
				else if (IsSAdmin(acptr))
					strlcat(buf, "a Services Operator", sizeof buf);
				else if (IsAdmin(acptr) && !IsCoAdmin(acptr))
					strlcat(buf, "a Server Administrator", sizeof buf);
				else if (IsCoAdmin(acptr))
					strlcat(buf, "a Co Administrator", sizeof buf);
				else if (IsServices(acptr))
					strlcat(buf, "a Network Service", sizeof buf);
				else if (IsOper(acptr))
					strlcat(buf, "an IRC Operator", sizeof buf);

				else
					strlcat(buf, "a Local IRC Operator", sizeof buf);
				if (buf[0])
					sendto_one(sptr,
					    rpl_str(RPL_WHOISOPERATOR), me.name,
					    parv[0], name, buf);
			}

			if (IsHelpOp(acptr) && (!IsHideOper(acptr)
			    || sptr == acptr || IsAnOper(sptr)))
				if (!user->away)
					sendto_one(sptr,
					    rpl_str(RPL_WHOISHELPOP), me.name,
					    parv[0], name);

			if (acptr->umodes & UMODE_BOT)
			{
				sendto_one(sptr, rpl_str(RPL_WHOISBOT),
				    me.name, parv[0], name, ircnetwork);
			}
			if (acptr->umodes & UMODE_SECURE)
			{
				sendto_one(sptr, ":%s %d %s %s :%s", me.name,
				    RPL_WHOISSPECIAL,
				    parv[0], name,
				    "is a Secure Connection");
			}
			if (user->swhois && !IsHideOper(acptr))
			{
				if (*user->swhois != '\0')
					sendto_one(sptr, ":%s %d %s %s :%s",
					    me.name, RPL_WHOISSPECIAL, parv[0],
					    name, acptr->user->swhois);
			}
			/*
			 * Fix /whois to not show idle times of
			 * global opers to anyone except another
			 * global oper or services.
			 * -CodeM/Barubary
			 */
			if (MyConnect(acptr))
				sendto_one(sptr, rpl_str(RPL_WHOISIDLE),
				    me.name, parv[0], name,
				    TStime() - acptr->last, acptr->firsttime);
		}
		if (!found)
			sendto_one(sptr, err_str(ERR_NOSUCHNICK),
			    me.name, parv[0], nick);
		if (p)
			p[-1] = ',';
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFWHOIS), me.name, parv[0], parv[1]);

	return 0;
}
