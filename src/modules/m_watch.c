/*
 *   IRC - Internet Relay Chat, src/modules/m_watch.c
 *   (C) 2005 The UnrealIRCd Team
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC CMD_FUNC(m_watch);

#define MSG_WATCH 	"WATCH"	
#define TOK_WATCH 	"`"	

ModuleHeader MOD_HEADER(m_watch)
  = {
	"m_watch",
	"$Id$",
	"command /watch", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_watch)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_WATCH, TOK_WATCH, m_watch, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_watch)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_watch)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * RPL_NOWON	- Online at the moment (Succesfully added to WATCH-list)
 * RPL_NOWOFF	- Offline at the moement (Succesfully added to WATCH-list)
 * RPL_WATCHOFF	- Succesfully removed from WATCH-list.
 * ERR_TOOMANYWATCH - Take a guess :>  Too many WATCH entries.
 */
static void show_watch(aClient *cptr, char *name, int rpl1, int rpl2, int awaynotify)
{
	aClient *acptr;


	if ((acptr = find_person(name, NULL)))
	{
		if (awaynotify && acptr->user->away)
		{
			if (IsWebTV(cptr))
				sendto_one(cptr, ":IRC!IRC@%s PRIVMSG %s :%s (%s@%s) is on IRC, but away", 
				    me.name, cptr->name, acptr->name, acptr->user->username,
				    IsHidden(acptr) ? acptr->user->virthost : acptr->user->
				    realhost);
			else
				sendto_one(cptr, rpl_str(RPL_NOWISAWAY), me.name, cptr->name,
				    acptr->name, acptr->user->username,
				    IsHidden(acptr) ? acptr->user->virthost : acptr->user->
				    realhost, acptr->user->lastaway);
			return;
		}
		
		if (IsWebTV(cptr))
			sendto_one(cptr, ":IRC!IRC@%s PRIVMSG %s :%s (%s@%s) is on IRC", 
			    me.name, cptr->name, acptr->name, acptr->user->username,
			    IsHidden(acptr) ? acptr->user->virthost : acptr->user->
			    realhost);
		else
			sendto_one(cptr, rpl_str(rpl1), me.name, cptr->name,
			    acptr->name, acptr->user->username,
			    IsHidden(acptr) ? acptr->user->virthost : acptr->user->
			    realhost, acptr->lastnick);
	}
	else
	{
		if (IsWebTV(cptr))
			sendto_one(cptr, ":IRC!IRC@%s PRIVMSG %s :%s is not on IRC", me.name, 
				cptr->name, name);
		else	
			sendto_one(cptr, rpl_str(rpl2), me.name, cptr->name,
			    name, "*", "*", 0);
	}
}

static char buf[BUFSIZE];

/*
 * m_watch
 */
DLLFUNC CMD_FUNC(m_watch)
{
	aClient *acptr;
	char *s, **pav = parv, *user;
	char *p = NULL, *def = "l";
	int awaynotify = 0;


	if (parc < 2)
	{
		/*
		 * Default to 'l' - list who's currently online
		 */
		parc = 2;
		parv[1] = def;
	}

	for (s = (char *)strtoken(&p, *++pav, " "); s;
	    s = (char *)strtoken(&p, NULL, " "))
	{
		if ((user = (char *)index(s, '!')))
			*user++ = '\0';	/* Not used */
			
		if (!strcmp(s, "A"))
			awaynotify = 1;

		/*
		 * Prefix of "+", they want to add a name to their WATCH
		 * list.
		 */
		if (*s == '+')
		{
			if (!*(s+1))
				continue;
			if (do_nick_name(s + 1))
			{
				if (sptr->watches >= MAXWATCH)
				{
					sendto_one(sptr,
					    err_str(ERR_TOOMANYWATCH), me.name,
					    cptr->name, s + 1);

					continue;
				}

				add_to_watch_hash_table(s + 1, sptr, awaynotify);
			}

			show_watch(sptr, s + 1, RPL_NOWON, RPL_NOWOFF, awaynotify);
			continue;
		}

		/*
		 * Prefix of "-", coward wants to remove somebody from their
		 * WATCH list.  So do it. :-)
		 */
		if (*s == '-')
		{
			if (!*(s+1))
				continue;
			del_from_watch_hash_table(s + 1, sptr);
			show_watch(sptr, s + 1, RPL_WATCHOFF, RPL_WATCHOFF, 0);

			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			hash_del_watch_list(sptr);

			continue;
		}

		/*
		 * Now comes the fun stuff, "S" or "s" returns a status report of
		 * their WATCH list.  I imagine this could be CPU intensive if its
		 * done alot, perhaps an auto-lag on this?
		 */
		if (*s == 'S' || *s == 's')
		{
			Link *lp;
			aWatch *anptr;
			int  count = 0;

			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on.
			 */
			anptr = hash_get_watch(sptr->name);
			if (anptr)
				for (lp = anptr->watch, count = 1;
				    (lp = lp->next); count++)
					;
			sendto_one(sptr, rpl_str(RPL_WATCHSTAT), me.name,
			    parv[0], sptr->watches, count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			if ((lp = sptr->watch) == NULL)
			{
				sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST),
				    me.name, parv[0], *s);
				continue;
			}
			*buf = '\0';
			strlcpy(buf, lp->value.wptr->nick, sizeof buf);
			count =
			    strlen(parv[0]) + strlen(me.name) + 10 +
			    strlen(buf);
			while ((lp = lp->next))
			{
				if (count + strlen(lp->value.wptr->nick) + 1 >
				    BUFSIZE - 2)
				{
					sendto_one(sptr, rpl_str(RPL_WATCHLIST),
					    me.name, parv[0], buf);
					*buf = '\0';
					count =
					    strlen(parv[0]) + strlen(me.name) +
					    10;
				}
				strcat(buf, " ");
				strcat(buf, lp->value.wptr->nick);
				count += (strlen(lp->value.wptr->nick) + 1);
			}
			sendto_one(sptr, rpl_str(RPL_WATCHLIST), me.name,
			    parv[0], buf);

			sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name,
			    parv[0], *s);
			continue;
		}

		/*
		 * Well that was fun, NOT.  Now they want a list of everybody in
		 * their WATCH list AND if they are online or offline? Sheesh,
		 * greedy arn't we?
		 */
		if (*s == 'L' || *s == 'l')
		{
			Link *lp = sptr->watch;

			while (lp)
			{
				if ((acptr =
				    find_person(lp->value.wptr->nick, NULL)))
				{
					sendto_one(sptr, rpl_str(RPL_NOWON),
					    me.name, parv[0], acptr->name,
					    acptr->user->username,
					    IsHidden(acptr) ? acptr->user->
					    virthost : acptr->user->realhost,
					    acptr->lastnick);
				}
				/*
				 * But actually, only show them offline if its a capital
				 * 'L' (full list wanted).
				 */
				else if (isupper(*s))
					sendto_one(sptr, rpl_str(RPL_NOWOFF),
					    me.name, parv[0],
					    lp->value.wptr->nick, "*", "*",
					    lp->value.wptr->lasttime);
				lp = lp->next;
			}

			sendto_one(sptr, rpl_str(RPL_ENDOFWATCHLIST), me.name,
			    parv[0], *s);

			continue;
		}

		/*
		 * Hmm.. unknown prefix character.. Ignore it. :-)
		 */
	}

	return 0;
}
