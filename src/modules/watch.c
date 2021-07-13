/*
 *   IRC - Internet Relay Chat, src/modules/watch.c
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

#include "unrealircd.h"

CMD_FUNC(cmd_watch);

#define MSG_WATCH 	"WATCH"	

ModuleHeader MOD_HEADER
  = {
	"watch",
	"5.0",
	"command /watch", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_WATCH, cmd_watch, 1, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
 * RPL_NOWON	- Online at the moment (Successfully added to WATCH-list)
 * RPL_NOWOFF	- Offline at the moement (Successfully added to WATCH-list)
 * RPL_WATCHOFF	- Successfully removed from WATCH-list.
 * ERR_TOOMANYWATCH - Take a guess :>  Too many WATCH entries.
 */
static void show_watch(Client *client, char *name, int rpl1, int rpl2, int awaynotify)
{
	Client *target;

	if ((target = find_person(name, NULL)))
	{
		if (awaynotify && target->user->away)
		{
			sendnumeric(client, RPL_NOWISAWAY,
			    target->name, target->user->username,
			    IsHidden(target) ? target->user->virthost : target->user->
			    realhost, target->user->lastaway);
			return;
		}
		
		sendnumeric(client, rpl1,
		    target->name, target->user->username,
		    IsHidden(target) ? target->user->virthost : target->user->
		    realhost, target->lastnick);
	}
	else
	{
		sendnumeric(client, rpl2, name, "*", "*", 0L);
	}
}

static char buf[BUFSIZE];

/*
 * cmd_watch
 */
CMD_FUNC(cmd_watch)
{
	Client *target;
	char *s, **pav = parv, *user;
	char *p = NULL, *def = "l";
	int awaynotify = 0;
	int did_l=0, did_s=0;

	if (!MyUser(client))
		return;

	if (parc < 2)
	{
		/*
		 * Default to 'l' - list who's currently online
		 */
		parc = 2;
		parv[1] = def;
	}

	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
	{
		if ((user = strchr(s, '!')))
			*user++ = '\0';	/* Not used */
			
		if (!strcmp(s, "A") && WATCH_AWAY_NOTIFICATION)
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
				if (client->local->watches >= MAXWATCH)
				{
					sendnumeric(client, ERR_TOOMANYWATCH, s + 1);
					continue;
				}

				add_to_watch_hash_table(s + 1, client, awaynotify);
			}

			show_watch(client, s + 1, RPL_NOWON, RPL_NOWOFF, awaynotify);
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
			del_from_watch_hash_table(s + 1, client);
			show_watch(client, s + 1, RPL_WATCHOFF, RPL_WATCHOFF, 0);

			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			hash_del_watch_list(client);

			continue;
		}

		/*
		 * Now comes the fun stuff, "S" or "s" returns a status report of
		 * their WATCH list.  I imagine this could be CPU intensive if its
		 * done alot, perhaps an auto-lag on this?
		 */
		if ((*s == 'S' || *s == 's') && !did_s)
		{
			Link *lp;
			Watch *anptr;
			int  count = 0;
			
			did_s = 1;
			
			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on.
			 */
			anptr = hash_get_watch(client->name);
			if (anptr)
				for (lp = anptr->watch, count = 1;
				    (lp = lp->next); count++)
					;
			sendnumeric(client, RPL_WATCHSTAT, client->local->watches, count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			if ((lp = client->local->watch) == NULL)
			{
				sendnumeric(client, RPL_ENDOFWATCHLIST, *s);
				continue;
			}
			*buf = '\0';
			strlcpy(buf, lp->value.wptr->nick, sizeof buf);
			count =
			    strlen(client->name) + strlen(me.name) + 10 +
			    strlen(buf);
			while ((lp = lp->next))
			{
				if (count + strlen(lp->value.wptr->nick) + 1 >
				    BUFSIZE - 2)
				{
					sendnumeric(client, RPL_WATCHLIST, buf);
					*buf = '\0';
					count = strlen(client->name) + strlen(me.name) + 10;
				}
				strcat(buf, " ");
				strcat(buf, lp->value.wptr->nick);
				count += (strlen(lp->value.wptr->nick) + 1);
			}
			sendnumeric(client, RPL_WATCHLIST, buf);

			sendnumeric(client, RPL_ENDOFWATCHLIST, *s);
			continue;
		}

		/*
		 * Well that was fun, NOT.  Now they want a list of everybody in
		 * their WATCH list AND if they are online or offline? Sheesh,
		 * greedy arn't we?
		 */
		if ((*s == 'L' || *s == 'l') && !did_l)
		{
			Link *lp = client->local->watch;

			did_l = 1;

			while (lp)
			{
				if ((target = find_person(lp->value.wptr->nick, NULL)))
				{
					sendnumeric(client, RPL_NOWON, target->name,
					    target->user->username,
					    IsHidden(target) ? target->user->
					    virthost : target->user->realhost,
					    target->lastnick);
				}
				/*
				 * But actually, only show them offline if its a capital
				 * 'L' (full list wanted).
				 */
				else if (isupper(*s))
					sendnumeric(client, RPL_NOWOFF,
					    lp->value.wptr->nick, "*", "*",
					    lp->value.wptr->lasttime);
				lp = lp->next;
			}

			sendnumeric(client, RPL_ENDOFWATCHLIST, *s);

			continue;
		}

		/*
		 * Hmm.. unknown prefix character.. Ignore it. :-)
		 */
	}
}
