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

#define MSG_WATCH 	"WATCH"

CMD_FUNC(cmd_watch);
int watch_user_quit(Client *client, MessageTag *mtags, const char *comment);
int watch_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away);
int watch_nickchange(Client *client, MessageTag *mtags, const char *newnick);
int watch_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick);
int watch_user_connect(Client *client);
int watch_notification(Client *client, Watch *watch, Link *lp, int event);

ModuleHeader MOD_HEADER
  = {
	"watch",
	"5.0",
	"command /watch", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	
	CommandAdd(modinfo->handle, MSG_WATCH, cmd_watch, 1, CMD_USER);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_QUIT, 0, watch_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_QUIT, 0, watch_user_quit);
	HookAdd(modinfo->handle, HOOKTYPE_AWAY, 0, watch_away);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_NICKCHANGE, 0, watch_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_NICKCHANGE, 0, watch_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_LOCAL_NICKCHANGE, 0, watch_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_POST_REMOTE_NICKCHANGE, 0, watch_post_nickchange);
	HookAdd(modinfo->handle, HOOKTYPE_LOCAL_CONNECT, 0, watch_user_connect);
	HookAdd(modinfo->handle, HOOKTYPE_REMOTE_CONNECT, 0, watch_user_connect);

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
 */
static void show_watch(Client *client, char *name, int awaynotify)
{
	Client *target;

	if ((target = find_user(name, NULL)))
	{
		if (awaynotify && target->user->away)
		{
			sendnumeric(client, RPL_NOWISAWAY,
			    target->name, target->user->username,
			    IsHidden(target) ? target->user->virthost : target->user->realhost,
			    (long long)target->user->away_since);
			return;
		}
		
		sendnumeric(client, RPL_NOWON,
		    target->name, target->user->username,
		    IsHidden(target) ? target->user->virthost : target->user->realhost,
		    (long long)target->lastnick);
	}
	else
	{
		sendnumeric(client, RPL_NOWOFF, name, "*", "*", 0LL);
	}
}

/*
 * RPL_WATCHOFF	- Successfully removed from WATCH-list.
 */
static void show_watch_removed(Client *client, char *name)
{
	Client *target;

	if ((target = find_user(name, NULL)))
	{
		sendnumeric(client, RPL_WATCHOFF,
		    target->name, target->user->username,
		    IsHidden(target) ? target->user->virthost : target->user->realhost,
		    (long long)target->lastnick);
	}
	else
	{
		sendnumeric(client, RPL_WATCHOFF, name, "*", "*", 0LL);
	}
}

#define WATCHES(client) (moddata_local_client(client, watchCounterMD).i)
#define WATCH(client) (moddata_local_client(client, watchListMD).ptr)

/*
 * cmd_watch
 */
CMD_FUNC(cmd_watch)
{
	char request[BUFSIZE];
	char buf[BUFSIZE];
	Client *target;
	char *s, *user;
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


	ModDataInfo *watchCounterMD = findmoddata_byname("watchCount", MODDATATYPE_LOCAL_CLIENT);
	ModDataInfo *watchListMD = findmoddata_byname("watchList", MODDATATYPE_LOCAL_CLIENT);
	
	if (!watchCounterMD || !watchListMD)
	{
		unreal_log(ULOG_ERROR, "watch", "WATCH_BACKEND_MISSING", NULL,
		           "[watch] moddata unavailable. Is the 'watch-backend' module loaded?");
		sendnotice(client, "WATCH command is not available at this moment. Please try again later.");
		return;
	}

	strlcpy(request, parv[1], sizeof(request));
	for (s = strtoken(&p, request, " "); s; s = strtoken(&p, NULL, " "))
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
				if (WATCHES(client) >= MAXWATCH)
				{
					sendnumeric(client, ERR_TOOMANYWATCH, s + 1);
					continue;
				}

				watch_add(s + 1, client,
					WATCH_FLAG_TYPE_WATCH | (awaynotify ? WATCH_FLAG_AWAYNOTIFY : 0)
					);
			}

			show_watch(client, s + 1, awaynotify);
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
			watch_del(s + 1, client, WATCH_FLAG_TYPE_WATCH);
			show_watch_removed(client, s + 1);
			continue;
		}

		/*
		 * Fancy "C" or "c", they want to nuke their WATCH list and start
		 * over, so be it.
		 */
		if (*s == 'C' || *s == 'c')
		{
			watch_del_list(client, WATCH_FLAG_TYPE_WATCH);
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
			Watch *watch;
			int  count = 0;
			
			did_s = 1;
			
			/*
			 * Send a list of how many users they have on their WATCH list
			 * and how many WATCH lists they are on. This will also include
			 * other WATCH types if present - we're not checking for
			 * WATCH_FLAG_TYPE_*.
			 */
			watch = watch_get(client->name);
			if (watch)
				for (lp = watch->watch, count = 1;
				    (lp = lp->next); count++)
					;
			sendnumeric(client, RPL_WATCHSTAT, WATCHES(client), count);

			/*
			 * Send a list of everybody in their WATCH list. Be careful
			 * not to buffer overflow.
			 */
			lp = WATCH(client);
			*buf = '\0';
			count = strlen(client->name) + strlen(me.name) + 10;
			while (lp)
			{
				if (!(lp->flags & WATCH_FLAG_TYPE_WATCH))
				{
					lp = lp->next;
					continue; /* this one is not ours */
				}
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
				
				lp = lp->next;
			}
			if (*buf)
				/* anything to send */
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
			Link *lp = WATCH(client);

			did_l = 1;

			while (lp)
			{
				if (!(lp->flags & WATCH_FLAG_TYPE_WATCH))
				{
					lp = lp->next;
					continue; /* this one is not ours */
				}
				if ((target = find_user(lp->value.wptr->nick, NULL)))
				{
					sendnumeric(client, RPL_NOWON, target->name,
					    target->user->username,
					    IsHidden(target) ? target->user->
					    virthost : target->user->realhost,
					    (long long)target->lastnick);
				}
				/*
				 * But actually, only show them offline if its a capital
				 * 'L' (full list wanted).
				 */
				else if (isupper(*s))
					sendnumeric(client, RPL_NOWOFF,
					    lp->value.wptr->nick, "*", "*",
					    (long long)lp->value.wptr->lasttime);
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

int watch_user_quit(Client *client, MessageTag *mtags, const char *comment)
{
	if (IsUser(client))
		watch_check(client, WATCH_EVENT_OFFLINE, watch_notification);
	return 0;
}

int watch_away(Client *client, MessageTag *mtags, const char *reason, int already_as_away)
{
	if (reason)
		watch_check(client, already_as_away ? WATCH_EVENT_REAWAY : WATCH_EVENT_AWAY, watch_notification);
	else
		watch_check(client, WATCH_EVENT_NOTAWAY, watch_notification);

	return 0;
}

int watch_nickchange(Client *client, MessageTag *mtags, const char *newnick)
{
	watch_check(client, WATCH_EVENT_OFFLINE, watch_notification);

	return 0;
}

int watch_post_nickchange(Client *client, MessageTag *mtags, const char *oldnick)
{
	watch_check(client, WATCH_EVENT_ONLINE, watch_notification);

	return 0;
}

int watch_user_connect(Client *client)
{
	watch_check(client, WATCH_EVENT_ONLINE, watch_notification);

	return 0;
}

int watch_notification(Client *client, Watch *watch, Link *lp, int event)
{
	int awaynotify = 0;
	
	if (!(lp->flags & WATCH_FLAG_TYPE_WATCH))
		return 0;
	
	if ((event == WATCH_EVENT_AWAY) || (event == WATCH_EVENT_NOTAWAY) || (event == WATCH_EVENT_REAWAY))
		awaynotify = 1;

	if (!awaynotify)
	{
		if (event == WATCH_EVENT_OFFLINE)
		{
			sendnumeric(lp->value.client, RPL_LOGOFF,
			            client->name,
			            (IsUser(client) ? client->user->username : "<N/A>"),
			            (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
			            (long long)watch->lasttime);
		} else {
			sendnumeric(lp->value.client, RPL_LOGON,
			            client->name,
			            (IsUser(client) ? client->user->username : "<N/A>"),
			            (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
			            (long long)watch->lasttime);
			/* For watch away notification, a user who is away could change their nick,
			 * and that nick could be on someones watch list. In such a case we
			 * should not only send RPL_LOGON but also a RPL_GONEAWAY.
			 */
			if ((lp->flags & WATCH_FLAG_AWAYNOTIFY) && IsUser(client) && client->user->away)
			{
				/* This is possible if the user is nick changing,
				 * they come online, and then we send RPL_GONEAWAY
				 */
				sendnumeric(lp->value.client, RPL_GONEAWAY,
					    client->name,
					    (IsUser(client) ? client->user->username : "<N/A>"),
					    (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
					    (long long)client->user->away_since,
					    client->user->away);
			}
		}
	}
	else
	{
		/* AWAY or UNAWAY */
		if (!(lp->flags & WATCH_FLAG_AWAYNOTIFY))
			return 0; /* skip away/unaway notification for users not interested in them */

		if (event == WATCH_EVENT_NOTAWAY)
		{
			sendnumeric(lp->value.client, RPL_NOTAWAY,
			    client->name,
			    (IsUser(client) ? client->user->username : "<N/A>"),
			    (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
			    (long long)client->user->away_since);
		} else
		if (event == WATCH_EVENT_AWAY)
		{
			sendnumeric(lp->value.client, RPL_GONEAWAY,
			            client->name,
			            (IsUser(client) ? client->user->username : "<N/A>"),
			            (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
			            (long long)client->user->away_since,
			            client->user->away);
		} else
		if (event == WATCH_EVENT_REAWAY)
		{
			sendnumeric(lp->value.client, RPL_REAWAY,
			            client->name,
			            (IsUser(client) ? client->user->username : "<N/A>"),
			            (IsUser(client) ? (IsHidden(client) ? client->user->virthost : client->user->realhost) : "<N/A>"),
			            (long long)client->user->away_since,
			            client->user->away);
		}
	}
	
	return 0;
}

