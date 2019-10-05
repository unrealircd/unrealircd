/*
 *   Unreal Internet Relay Chat Daemon, src/misc.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

extern char	*me_hash;

static void exit_one_client(Client *, MessageTag *mtags_i, const char *);

static char *months[] = {
	"January", "February", "March", "April",
	"May", "June", "July", "August",
	"September", "October", "November", "December"
};

static char *weekdays[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};

typedef struct {
	int value;			/** Unique integer value of item */
	char character;		/** Unique character assigned to item */
	char *name;			/** Name of item */
} BanActTable;

static BanActTable banacttable[] = {
	{ BAN_ACT_KILL,		'K',	"kill" },
	{ BAN_ACT_SOFT_KILL,	'i',	"soft-kill" },
	{ BAN_ACT_TEMPSHUN,	'S',	"tempshun" },
	{ BAN_ACT_SOFT_TEMPSHUN,'T',	"soft-tempshun" },
	{ BAN_ACT_SHUN,		's',	"shun" },
	{ BAN_ACT_SOFT_SHUN,	'H',	"soft-shun" },
	{ BAN_ACT_KLINE,	'k',	"kline" },
	{ BAN_ACT_SOFT_KLINE,	'I',	"soft-kline" },
	{ BAN_ACT_ZLINE,	'z',	"zline" },
	{ BAN_ACT_GLINE,	'g',	"gline" },
	{ BAN_ACT_SOFT_GLINE,	'G',	"soft-gline" },
	{ BAN_ACT_GZLINE,	'Z',	"gzline" },
	{ BAN_ACT_BLOCK,	'b',	"block" },
	{ BAN_ACT_SOFT_BLOCK,	'B',	"soft-block" },
	{ BAN_ACT_DCCBLOCK,	'd',	"dccblock" },
	{ BAN_ACT_SOFT_DCCBLOCK,'D',	"soft-dccblock" },
	{ BAN_ACT_VIRUSCHAN,	'v',	"viruschan" },
	{ BAN_ACT_SOFT_VIRUSCHAN,'V',	"soft-viruschan" },
	{ BAN_ACT_WARN,		'w',	"warn" },
	{ BAN_ACT_SOFT_WARN,	'W',	"soft-warn" },
	{ 0, 0, 0 }
};

typedef struct {
	int value;			/** Unique integer value of item */
	char character;		/** Unique character assigned to item */
	char *name;			/** Name of item */
	char *irccommand;	/** Raw IRC command of item (not unique!) */
} SpamfilterTargetTable;

SpamfilterTargetTable spamfiltertargettable[] = {
	{ SPAMF_CHANMSG,	'c',	"channel",			"PRIVMSG" },
	{ SPAMF_USERMSG,	'p',	"private",			"PRIVMSG" },
	{ SPAMF_USERNOTICE,	'n',	"private-notice",	"NOTICE" },
	{ SPAMF_CHANNOTICE,	'N',	"channel-notice",	"NOTICE" },
	{ SPAMF_PART,		'P',	"part",				"PART" },
	{ SPAMF_QUIT,		'q',	"quit",				"QUIT" },
	{ SPAMF_DCC,		'd',	"dcc",				"PRIVMSG" },
	{ SPAMF_USER,		'u',	"user",				"NICK" },
	{ SPAMF_AWAY,		'a',	"away",				"AWAY" },
	{ SPAMF_TOPIC,		't',	"topic",			"TOPIC" },
	{ 0, 0, 0, 0 }
};


/*
 * stats stuff
 */
struct IRCStatistics ircstats;

char *long_date(time_t clock)
{
	static char buf[80], plus;
	struct tm *lt, *gm;
	struct tm gmbuf;
	int  minswest;

	if (!clock)
		time(&clock);
	gm = gmtime(&clock);
	memcpy(&gmbuf, gm, sizeof(gmbuf));
	gm = &gmbuf;
	lt = localtime(&clock);
#ifndef _WIN32
	if (lt->tm_yday == gm->tm_yday)
		minswest = (gm->tm_hour - lt->tm_hour) * 60 +
		    (gm->tm_min - lt->tm_min);
	else if (lt->tm_yday > gm->tm_yday)
		minswest = (gm->tm_hour - (lt->tm_hour + 24)) * 60;
	else
		minswest = ((gm->tm_hour + 24) - lt->tm_hour) * 60;
#else
	minswest = (_timezone / 60);
#endif
	plus = (minswest > 0) ? '-' : '+';
	if (minswest < 0)
		minswest = -minswest;
	ircsnprintf(buf, sizeof(buf), "%s %s %d %d -- %02d:%02d %c%02d:%02d",
	    weekdays[lt->tm_wday], months[lt->tm_mon], lt->tm_mday,
	    1900 + lt->tm_year,
	    lt->tm_hour, lt->tm_min, plus, minswest / 60, minswest % 60);

	return buf;
}

/** Convert timestamp to a short date, a la: Wed Jun 30 21:49:08 1993
 * @returns A short date string, or NULL if the timestamp is invalid
 * (out of range)
 * @param ts   The timestamp
 * @param buf  The buffer to store the string (minimum size: 128 bytes),
 *             or NULL to use temporary static storage.
 */
char *short_date(time_t ts, char *buf)
{
	struct tm *t = gmtime(&ts);
	char *timestr;
	static char retbuf[128];

	if (!buf)
		buf = retbuf;

	*buf = '\0';
	if (!t)
		return NULL;

	timestr = asctime(t);
	if (!timestr)
		return NULL;

	strlcpy(buf, timestr, 128);
	stripcrlf(buf);
	return buf;
}

/*
 *  Fixes a string so that the first white space found becomes an end of
 * string marker (`\-`).  returns the 'fixed' string or "*" if the string
 * was NULL length or a NULL pointer.
 */
char *check_string(char *s)
{
	static char star[2] = "*";
	char *str = s;

	if (BadPtr(s))
		return star;

	for (; *s; s++)
		if (isspace(*s))
		{
			*s = '\0';
			break;
		}

	return (BadPtr(str)) ? star : str;
}

char *make_user_host(char *name, char *host)
{
	static char namebuf[USERLEN + HOSTLEN + 6];
	char *s = namebuf;

	memset(namebuf, 0, sizeof(namebuf));
	name = check_string(name);
	strlcpy(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strlcpy(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
}


/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
char *make_nick_user_host_r(char *namebuf, char *nick, char *name, char *host)
{
	char *s = namebuf;

	nick = check_string(nick);
	strlcpy(namebuf, nick, NICKLEN + 1);
	s += strlen(s);
	*s++ = '!';
	name = check_string(name);
	strlcpy(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strlcpy(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return namebuf;
}

/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
char *make_nick_user_host(char *nick, char *name, char *host)
{
	static char namebuf[NICKLEN + USERLEN + HOSTLEN + 24];

	return make_nick_user_host_r(namebuf, nick, name, host);
}


/** Similar to ctime() but without a potential newline and
 * also takes a time_t value rather than a pointer.
 */
char *myctime(time_t value)
{
	static char buf[28];
	char *p;

	strlcpy(buf, ctime(&value), sizeof buf);
	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

/*
** get_client_name
**      Return the name of the client for various tracking and
**      admin purposes. The main purpose of this function is to
**      return the "socket host" name of the client, if that
**	differs from the advertised name (other than case).
**	But, this can be used to any client structure.
**
**	Returns:
**	  "name[user@ip#.port]" if 'showip' is true;
**	  "name[sockethost]", if name and sockhost are different and
**	  showip is false; else
**	  "name".
**
** NOTE 1:
**	Watch out the allocation of "nbuf", if either client->name
**	or client->local->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (client) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char *get_client_name(Client *client, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (MyConnect(client))
	{
		if (showip)
			(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%s@%s.%u]",
			    client->name,
			    IsIdentSuccess(client) ? client->ident : "",
			    client->ip ? client->ip : "???",
			    (unsigned int)client->local->port);
		else
		{
			if (mycmp(client->name, client->local->sockhost))
				(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%s]",
				    client->name, client->local->sockhost);
			else
				return client->name;
		}
		return nbuf;
	}
	return client->name;
}

char *get_client_host(Client *client)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (!MyConnect(client))
		return client->name;
	if (!client->local->hostp)
		return get_client_name(client, FALSE);
	(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%-.*s@%-.*s]",
	    client->name, USERLEN,
  	    IsIdentSuccess(client) ? client->ident : "",
	    HOSTLEN, client->local->hostp->h_name);
	return nbuf;
}

/*
 * Set sockhost to 'host'. Skip the user@ part of 'host' if necessary.
 */
void set_sockhost(Client *client, char *host)
{
	char *s;
	if ((s = strchr(host, '@')))
		s++;
	else
		s = host;
	strlcpy(client->local->sockhost, s, sizeof(client->local->sockhost));
}

void remove_dcc_references(Client *client)
{
	Client *acptr;
	Link *lp, *nextlp;
	Link **lpp, *tmp;
	int found;

	lp = client->user->dccallow;
	while(lp)
	{
		nextlp = lp->next;
		acptr = lp->value.client;
		for(found = 0, lpp = &(acptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
		{
			if(lp->flags == (*lpp)->flags)
				continue; /* match only opposite types for sanity */
			if((*lpp)->value.client == client)
			{
				if((*lpp)->flags == DCC_LINK_ME)
				{
					sendto_one(acptr, NULL, ":%s %d %s :%s has been removed from "
						"your DCC allow list for signing off",
						me.name, RPL_DCCINFO, acptr->name, client->name);
				}
				tmp = *lpp;
				*lpp = tmp->next;
				free_link(tmp);
				found++;
				break;
			}
		}

		if(!found)
			sendto_realops("[BUG] remove_dcc_references:  %s was in dccallowme "
				"list[%d] of %s but not in dccallowrem list!",
				acptr->name, lp->flags, client->name);

		free_link(lp);
		lp = nextlp;
	}
}

/*
 * Recursively send QUITs and SQUITs for cptr and all of it's dependent
 * clients.  A server needs the client QUITs if it does not support NOQUIT.
 *    - kaniini
 */
static void recurse_send_quits(Client *cptr, Client *client, Client *from, Client *to,
                               MessageTag *mtags, const char *comment, const char *splitstr)
{
	Client *acptr, *next;

	if (!MyConnect(to))
		return; /* We shouldn't even be called for non-remotes */

	if (!CHECKPROTO(to, PROTO_NOQUIT))
	{
		list_for_each_entry_safe(acptr, next, &client_list, client_node)
		{
			if (acptr->srvptr != client)
				continue;

			sendto_one(to, NULL, ":%s QUIT :%s", acptr->name, splitstr);
		}
	}

	list_for_each_entry_safe(acptr, next, &global_server_list, client_node)
	{
		if (acptr->srvptr != client)
			continue;

		recurse_send_quits(cptr, acptr, from, to, mtags, comment, splitstr);
	}

	if ((cptr == client && to != from) || !CHECKPROTO(to, PROTO_NOQUIT))
		sendto_one(to, mtags, "SQUIT %s :%s", client->name, comment);
}

/*
 * Remove all clients that depend on source_p; assumes all (S)QUITs have
 * already been sent.  we make sure to exit a server's dependent clients
 * and servers before the server itself; exit_one_client takes care of
 * actually removing things off llists.   tweaked from +CSr31  -orabidoo
 */
static void recurse_remove_clients(Client *client, MessageTag *mtags, const char *comment)
{
	Client *acptr, *next;

	list_for_each_entry_safe(acptr, next, &client_list, client_node)
	{
		if (acptr->srvptr != client)
			continue;

		exit_one_client(acptr, mtags, comment);
	}

	list_for_each_entry_safe(acptr, next, &global_server_list, client_node)
	{
		if (acptr->srvptr != client)
			continue;

		recurse_remove_clients(acptr, mtags, comment);
		exit_one_client(acptr, mtags, comment);
	}
}

/*
** Remove *everything* that depends on source_p, from all lists, and sending
** all necessary QUITs and SQUITs.  source_p itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
*/
static void remove_dependents(Client *client, Client *from, MessageTag *mtags, const char *comment, const char *splitstr)
{
	Client *acptr;

	list_for_each_entry(acptr, &global_server_list, client_node)
		recurse_send_quits(client, client, from, acptr, mtags, comment, splitstr);

	recurse_remove_clients(client, mtags, splitstr);
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
/* DANGER: Ugly hack follows. */
/* Yeah :/ */
static void exit_one_client(Client *client, MessageTag *mtags_i, const char *comment)
{
	Link *lp;
	Membership *mp;

	assert(!IsMe(client));

	if (IsUser(client))
	{
		MessageTag *mtags_o = NULL;

		if (!MyUser(client))
			RunHook3(HOOKTYPE_REMOTE_QUIT, client, mtags_i, comment);

		new_message_special(client, mtags_i, &mtags_o, ":%s QUIT", client->name);
		sendto_local_common_channels(client, NULL, 0, mtags_o, ":%s QUIT :%s", client->name, comment);
		free_message_tags(mtags_o);

		while ((mp = client->user->channel))
			remove_user_from_channel(client, mp->channel);

		/* Clean up invitefield */
		while ((lp = client->user->invited))
			del_invite(client, lp->value.channel);
		/* again, this is all that is needed */

		/* Clean up dccallow list and (if needed) notify other clients
		 * that have this person on DCCALLOW that the user just left/got removed.
		 */
		remove_dcc_references(client);

		/* For remote clients, we need to check for any outstanding async
		 * connects attached to this 'client', and set those records to NULL.
		 * Why not for local? Well, we already do that in close_connection ;)
		 */
		if (!MyConnect(client))
			unrealdns_delreq_bycptr(client);
	}

	/* Free module related data for this client */
	moddata_free_client(client);
	if (MyConnect(client))
		moddata_free_local_client(client);

	/* Remove client from the client list */
	if (*client->id)
	{
		del_from_id_hash_table(client->id, client);
		*client->id = '\0';
	}
	if (*client->name)
		del_from_client_hash_table(client->name, client);
	if (IsUser(client))
		hash_check_watch(client, RPL_LOGOFF);
	if (remote_rehash_client == client)
		remote_rehash_client = NULL; /* client did a /REHASH and QUIT before rehash was complete */
	remove_client_from_list(client);
}

/** Exit this IRC client, and all the dependents (users, servers) if this is a server.
 * @param client        The client to exit.
 * @param recv_mtags  Message tags to use as a base (if any).
 * @param comment     The (s)quit message
 */
void exit_client(Client *client, MessageTag *recv_mtags, char *comment)
{
	long long on_for;
	ConfigItem_listen *listen_conf;
	MessageTag *mtags_generated = NULL;

	if (IsDead(client))
		return; /* Already marked as exited */

	/* We replace 'recv_mtags' here with a newly
	 * generated id if 'recv_mtags' is NULL or is
	 * non-NULL and contains no msgid etc.
	 * This saves us from doing a new_message()
	 * prior to the exit_client() call at around
	 * 100+ places elsewhere in the code.
	 */
	new_message(client, recv_mtags, &mtags_generated);
	recv_mtags = mtags_generated;

	if (MyConnect(client))
	{
		if (client->local->class)
		{
			client->local->class->clients--;
			if ((client->local->class->flag.temporary) && !client->local->class->clients && !client->local->class->xrefcount)
			{
				delete_classblock(client->local->class);
				client->local->class = NULL;
			}
		}
		if (IsUser(client))
			irccounts.me_clients--;
		if (client->serv && client->serv->conf)
		{
			client->serv->conf->refcount--;
			Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
				client->name, client->serv->conf->servername, client->serv->conf->refcount));
			if (!client->serv->conf->refcount
			  && client->serv->conf->flag.temporary)
			{
				Debug((DEBUG_ERROR, "deleting temporary block %s", client->serv->conf->servername));
				delete_linkblock(client->serv->conf);
				client->serv->conf = NULL;
			}
		}
		if (IsServer(client))
		{
			irccounts.me_servers--;
			ircd_log(LOG_SERVER, "SQUIT %s (%s)", client->name, comment);
		}
		free_pending_net(client);
		if (client->local->listener)
			if (client->local->listener && !IsOutgoing(client))
			{
				listen_conf = client->local->listener;
				listen_conf->clients--;
				if (listen_conf->flag.temporary && (listen_conf->clients == 0))
				{
					/* Call listen cleanup */
					listen_cleanup();
				}
			}
		SetClosing(client);
		if (IsUser(client))
		{
			RunHook3(HOOKTYPE_LOCAL_QUIT, client, recv_mtags, comment);
			sendto_connectnotice(client, 1, comment);
			/* Clean out list and watch structures -Donwulff */
			hash_del_watch_list(client);
			on_for = TStime() - client->local->firsttime;
			if (IsHidden(client))
				ircd_log(LOG_CLIENT, "Disconnect - (%lld:%lld:%lld) %s!%s@%s [VHOST %s] (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					client->name, client->user->username,
					client->user->realhost, client->user->virthost, comment);
			else
				ircd_log(LOG_CLIENT, "Disconnect - (%lld:%lld:%lld) %s!%s@%s (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					client->name, client->user->username, client->user->realhost, comment);
		} else
		if (IsUnknown(client))
		{
			RunHook3(HOOKTYPE_UNKUSER_QUIT, client, recv_mtags, comment);
		}

		if (client->local->fd >= 0 && !IsConnecting(client))
		{
			sendto_one(client, NULL, "ERROR :Closing Link: %s (%s)",
			    get_client_name(client, FALSE), comment);
		}
		close_connection(client);
	}
	else if (IsUser(client) && !IsULine(client))
	{
		if (client->srvptr != &me)
			sendto_fconnectnotice(client, 1, comment);
	}

	/*
	 * Recurse down the client list and get rid of clients who are no
	 * longer connected to the network (from my point of view)
	 * Only do this expensive stuff if exited==server -Donwulff
	 */
	if (IsServer(client))
	{
		char splitstr[HOSTLEN + HOSTLEN + 2];

		assert(client->serv != NULL && client->srvptr != NULL);

		if (FLAT_MAP)
			strlcpy(splitstr, "*.net *.split", sizeof splitstr);
		else
			ircsnprintf(splitstr, sizeof splitstr, "%s %s", client->srvptr->name, client->name);

		remove_dependents(client, client->direction, recv_mtags, comment, splitstr);

		RunHook2(HOOKTYPE_SERVER_QUIT, client, recv_mtags);
	}
	else if (IsUser(client) && !IsKilled(client))
	{
		sendto_server(client, PROTO_SID, 0, recv_mtags, ":%s QUIT :%s", ID(client), comment);
		sendto_server(client, 0, PROTO_SID, recv_mtags, ":%s QUIT :%s", client->name, comment);
	}

	/* Finally, the client/server itself exits.. */
	exit_one_client(client, recv_mtags, comment);

	free_message_tags(mtags_generated);
	
}

void initstats(void)
{
	memset(&ircstats, 0, sizeof(ircstats));
}

void verify_opercount(Client *orig, char *tag)
{
	int counted = 0;
	Client *client;
	char text[2048];

	list_for_each_entry(client, &client_list, client_node)
	{
		if (IsOper(client) && !IsHideOper(client))
			counted++;
	}
	if (counted == irccounts.operators)
		return;
	snprintf(text, sizeof(text), "[BUG] operator count bug! value in /lusers is '%d', we counted '%d', "
	               "user='%s', userserver='%s', tag=%s. Corrected. ",
	               irccounts.operators, counted, orig->name,
	               orig->srvptr ? orig->srvptr->name : "<null>", tag ? tag : "<null>");
#ifdef DEBUGMODE
	sendto_realops("%s", text);
#endif
	ircd_log(LOG_ERROR, "%s", text);
	irccounts.operators = counted;
}

/** Check if the specified hostname does not contain forbidden characters.
 * RETURNS:
 * 1 if ok, 0 if rejected.
 */
int valid_host(char *host)
{
	char *p;
	
	if (strlen(host) > HOSTLEN)
		return 0; /* too long hosts are invalid too */

	for (p=host; *p; p++)
		if (!isalnum(*p) && (*p != '_') && (*p != '-') && (*p != '.') && (*p != ':') && (*p != '/'))
			return 0;

	return 1;
}

/*|| BAN ACTION ROUTINES FOLLOW ||*/

/** Converts a banaction string (eg: "kill") to an integer value (eg: BAN_ACT_KILL) */
BanAction banact_stringtoval(char *s)
{
	BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (!strcasecmp(s, b->name))
			return b->value;
	return 0;
}

/** Converts a banaction character (eg: 'K') to an integer value (eg: BAN_ACT_KILL) */
BanAction banact_chartoval(char c)
{
	BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->character == c)
			return b->value;
	return 0;
}

/** Converts a banaction value (eg: BAN_ACT_KILL) to a character value (eg: 'k') */
char banact_valtochar(BanAction val)
{
	BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->value == val)
			return b->character;
	return '\0';
}

/** Converts a banaction value (eg: BAN_ACT_KLINE) to a string (eg: "kline") */
char *banact_valtostring(BanAction val)
{
	BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->value == val)
			return b->name;
	return "UNKNOWN";
}

/*|| BAN TARGET ROUTINES FOLLOW ||*/

/** Extract target flags from string 's'. */
int spamfilter_gettargets(char *s, Client *client)
{
SpamfilterTargetTable *e;
int flags = 0;

	for (; *s; s++)
	{
		for (e = &spamfiltertargettable[0]; e->value; e++)
			if (e->character == *s)
			{
				flags |= e->value;
				break;
			}
		if (!e->value && client)
		{
			sendnotice(client, "Unknown target type '%c'", *s);
			return 0;
		}
	}
	return flags;
}

/** Convert a string with a targetname to an integer value */
int spamfilter_getconftargets(char *s)
{
SpamfilterTargetTable *e;

	for (e = &spamfiltertargettable[0]; e->value; e++)
		if (!strcmp(s, e->name))
			return e->value;
	return 0;
}

/** Create a string with (multiple) targets from an integer mask */
char *spamfilter_target_inttostring(int v)
{
static char buf[128];
SpamfilterTargetTable *e;
char *p = buf;

	for (e = &spamfiltertargettable[0]; e->value; e++)
		if (v & e->value)
			*p++ = e->character;
	*p = '\0';
	return buf;
}

char *unreal_decodespace(char *s)
{
static char buf[512], *i, *o;
	for (i = s, o = buf; (*i) && (o < buf+510); i++)
		if (*i == '_')
		{
			if (i[1] != '_')
				*o++ = ' ';
			else {
				*o++ = '_';
				i++;
			}
		}
		else
			*o++ = *i;
	*o = '\0';
	return buf;
}

char *unreal_encodespace(char *s)
{
static char buf[512], *i, *o;

	if (!s)
		return NULL; /* NULL in = NULL out */

	for (i = s, o = buf; (*i) && (o < buf+509); i++)
	{
		if (*i == ' ')
			*o++ = '_';
		else if (*i == '_')
		{
			*o++ = '_';
			*o++ = '_';
		}
		else
			*o++ = *i;
	}
	*o = '\0';
	return buf;
}

/** This is basically only used internally by match_spamfilter()... */
char *cmdname_by_spamftarget(int target)
{
	SpamfilterTargetTable *e;

	for (e = &spamfiltertargettable[0]; e->value; e++)
		if (e->value == target)
			return e->irccommand;
	return "???";
}

int is_autojoin_chan(char *chname)
{
	char buf[512];
	char *p, *name;

	if (OPER_AUTO_JOIN_CHANS)
	{
		strlcpy(buf, OPER_AUTO_JOIN_CHANS, sizeof(buf));

		for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
			if (!strcasecmp(name, chname))
				return 1;
	}
	
	if (AUTO_JOIN_CHANS)
	{
		strlcpy(buf, AUTO_JOIN_CHANS, sizeof(buf));

		for (name = strtoken(&p, buf, ","); name; name = strtoken(&p, NULL, ","))
			if (!strcasecmp(name, chname))
				return 1;
	}

	return 0;
}

/** Convert a character like 'o' to the corresponding channel flag
 *  like CHFL_CHANOP.
 * @param c   The mode character. The only valid values are: vhoaq
 * @returns One of CHFL_* or 0 if an invalid mode character is specified.
 */
int char_to_channelflag(char c)
{
	if (c == 'v')
		return CHFL_VOICE;
	else if (c == 'h')
		return CHFL_HALFOP;
	else if (c == 'o')
		return CHFL_CHANOP;
	else if (c == 'a')
		return CHFL_CHANADMIN;
	else if (c == 'q')
		return CHFL_CHANOWNER;
	return 0;
}

char *getcloak(Client *client)
{
	if (!*client->user->cloakedhost)
	{
		/* need to calculate (first-time) */
		make_virthost(client, client->user->realhost, client->user->cloakedhost, 0);
	}

	return client->user->cloakedhost;
}

// FIXME: should detect <U5 ;)
int mixed_network(void)
{
	Client *client;
	
	list_for_each_entry(client, &server_list, special_node)
	{
		if (!IsServer(client) || IsULine(client))
			continue; /* skip u-lined servers (=non-unreal, unless you configure your ulines badly, that is) */
		if (SupportTKLEXT(client) && !SupportTKLEXT2(client))
			return 1; /* yup, something below 3.4-alpha3 is linked */
	}
	return 0;
}

/** Free all masks in the mask list */
void unreal_delete_masks(ConfigItem_mask *m)
{
	ConfigItem_mask *m_next;
	
	for (; m; m = m_next)
	{
		m_next = m->next;

		safe_free(m->mask);

		safe_free(m);
	}
}

/** Internal function to add one individual mask to the list */
static void unreal_add_mask(ConfigItem_mask **head, ConfigEntry *ce)
{
	ConfigItem_mask *m = safe_alloc(sizeof(ConfigItem_mask));

	/* Since we allow both mask "xyz"; and mask { abc; def; };... */
	if (ce->ce_vardata)
		safe_strdup(m->mask, ce->ce_vardata);
	else
		safe_strdup(m->mask, ce->ce_varname);
	
	add_ListItem((ListStruct *)m, (ListStruct **)head);
}

/** Add mask entries from config */
void unreal_add_masks(ConfigItem_mask **head, ConfigEntry *ce)
{
	if (ce->ce_entries)
	{
		ConfigEntry *cep;
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
			unreal_add_mask(head, cep);
	} else
	{
		unreal_add_mask(head, ce);
	}
}

/** Check if a client matches any of the masks in the mask list */
int unreal_mask_match(Client *client, ConfigItem_mask *m)
{
	for (; m; m = m->next)
	{
		/* With special support for '!' prefix (negative matching like "!192.168.*") */
		if (m->mask[0] == '!')
		{
			if (!match_user(m->mask+1, client, MATCH_CHECK_REAL))
				return 1;
		} else {
			if (match_user(m->mask, client, MATCH_CHECK_REAL))
				return 1;
		}
	}
	
	return 0;
}

/*
 * our own strcasestr implementation because strcasestr is often not
 * available or is not working correctly.
 */
char *our_strcasestr(char *haystack, char *needle) {
int i;
int nlength = strlen (needle);
int hlength = strlen (haystack);

	if (nlength > hlength) return NULL;
	if (hlength <= 0) return NULL;
	if (nlength <= 0) return haystack;
	for (i = 0; i <= (hlength - nlength); i++) {
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	}
  return NULL; /* not found */
}

int swhois_add(Client *client, char *tag, int priority, char *swhois, Client *from, Client *skip)
{
	SWhois *s;

	/* Make sure the line isn't added yet. If so, then bail out silently. */
	for (s = client->user->swhois; s; s = s->next)
		if (!strcmp(s->line, swhois))
			return -1; /* exists */

	s = safe_alloc(sizeof(SWhois));
	safe_strdup(s->line, swhois);
	safe_strdup(s->setby, tag);
	s->priority = priority;
	AddListItemPrio(s, client->user->swhois, s->priority);
	
	sendto_server(skip, 0, PROTO_EXTSWHOIS, NULL, ":%s SWHOIS %s :%s",
		from->name, client->name, swhois);

	sendto_server(skip, PROTO_EXTSWHOIS, 0, NULL, ":%s SWHOIS %s + %s %d :%s",
		from->name, client->name, tag, priority, swhois);

	return 0;
}

/** Delete swhois title(s)
 * Delete swhois by tag and swhois. Then broadcast this change to all other servers.
 * Remark: if you use swhois "*" then it will remove all swhois titles for that tag
 */
int swhois_delete(Client *client, char *tag, char *swhois, Client *from, Client *skip)
{
	SWhois *s, *s_next;
	int ret = -1; /* default to 'not found' */
	
	for (s = client->user->swhois; s; s = s_next)
	{
		s_next = s->next;
		
		/* If ( same swhois or "*" ) AND same tag */
		if ( ((!strcmp(s->line, swhois) || !strcmp(swhois, "*")) &&
		    !strcmp(s->setby, tag)))
		{
			DelListItem(s, client->user->swhois);
			safe_free(s->line);
			safe_free(s->setby);
			safe_free(s);

			sendto_server(skip, 0, PROTO_EXTSWHOIS, NULL, ":%s SWHOIS %s :",
				from->name, client->name);

			sendto_server(skip, PROTO_EXTSWHOIS, 0, NULL, ":%s SWHOIS %s - %s %d :%s",
				from->name, client->name, tag, 0, swhois);
			
			ret = 0;
		}
	}

	return ret;
}

/** Is this user using a websocket? (LOCAL USERS ONLY) */
int IsWebsocket(Client *client)
{
	ModDataInfo *md = findmoddata_byname("websocket", MODDATATYPE_CLIENT);
	if (!md)
		return 0; /* websocket module not loaded */
	return (MyConnect(client) && moddata_client(client, md).ptr) ? 1 : 0;
}

extern void send_raw_direct(Client *user, FORMAT_STRING(const char *pattern), ...);

/** Generic function to inform the user he/she has been banned.
 * @param client   The affected client.
 * @param bantype  The ban type, such as: "K-Lined", "G-Lined" or "realname".
 * @param reason   The specified reason.
 * @param global   Whether the ban is global (1) or for this server only (0)
 * @param noexit   Set this to NO_EXIT_CLIENT to make us not call exit_client().
 *                 This is really only needed from the accept code, do not
 *                 use it anywhere else. No really, never.
 *
 * @notes This function will call exit_client() appropriately.
 */
void banned_client(Client *client, char *bantype, char *reason, int global, int noexit)
{
	char buf[512];
	char *fmt = global ? iConf.reject_message_gline : iConf.reject_message_kline;
	const char *vars[6], *values[6];

	if (!MyConnect(client))
		abort(); /* hmm... or be more flexible? */

	/* This was: "You are not welcome on this %s. %s: %s. %s" but is now dynamic: */
	vars[0] = "bantype";
	values[0] = bantype;
	vars[1] = "banreason";
	values[1] = reason;
	vars[2] = "klineaddr";
	values[2] = KLINE_ADDRESS;
	vars[3] = "glineaddr";
	values[3] = GLINE_ADDRESS ? GLINE_ADDRESS : KLINE_ADDRESS; /* fallback to klineaddr */
	vars[4] = "ip";
	values[4] = GetIP(client);
	vars[5] = NULL;
	values[5] = NULL;
	buildvarstring(fmt, buf, sizeof(buf), vars, values);

	/* This is a bit extensive but we will send both a YOUAREBANNEDCREEP
	 * and a notice to the user.
	 * The YOUAREBANNEDCREEP will be helpful for the client since it makes
	 * clear the user should not quickly reconnect, as (s)he is banned.
	 * The notice still needs to be there because it stands out well
	 * at most IRC clients.
	 */
	if (noexit != NO_EXIT_CLIENT)
	{
		sendnumeric(client, ERR_YOUREBANNEDCREEP, buf);
		sendnotice(client, "%s", buf);
	} else {
		send_raw_direct(client, ":%s %d %s :%s",
		         me.name, ERR_YOUREBANNEDCREEP,
		         (*client->name ? client->name : "*"),
		         buf);
		send_raw_direct(client, ":%s NOTICE %s :%s",
		         me.name, (*client->name ? client->name : "*"), buf);
	}

	/* The final message in the ERROR is shorter. */
	if (HIDE_BAN_REASON && IsRegistered(client))
		snprintf(buf, sizeof(buf), "Banned (%s)", bantype);
	else
		snprintf(buf, sizeof(buf), "Banned (%s): %s", bantype, reason);

	if (noexit != NO_EXIT_CLIENT)
	{
		exit_client(client, NULL, buf);
	} else {
		/* Special handling for direct Z-line code */
		send_raw_direct(client, "ERROR :Closing Link: [%s] (%s)",
		           client->ip, buf);
	}
}

char *mystpcpy(char *dst, const char *src)
{
	for (; *src; src++)
		*dst++ = *src;
	*dst = '\0';
	return dst;
}

/** Helper function for send_channel_modes_sjoin3() and cmd_sjoin()
 * to build the SJSBY prefix which is <seton,setby> to
 * communicate when the ban was set and by whom.
 * @param buf   The buffer to write to
 * @param setby The setter of the "ban"
 * @param seton The time the "ban" was set
 * @retval The number of bytes written EXCLUDING the NUL byte,
 *         so similar to what strlen() would have returned.
 * @note Caller must ensure that the buffer 'buf' is of sufficient size.
 */
size_t add_sjsby(char *buf, char *setby, time_t seton)
{
	char tbuf[32];
	char *p = buf;

	snprintf(tbuf, sizeof(tbuf), "%ld", (long)seton);

	*p++ = '<';
	p = mystpcpy(p, tbuf);
	*p++ = ',';
	p = mystpcpy(p, setby);
	*p++ = '>';
	*p = '\0';

	return p - buf;
}

/** Concatenate the entire parameter string.
 * The function will take care of spaces in the final parameter (if any).
 * @param buf   The buffer to output in.
 * @param len   Length of the buffer.
 * @param parc  Parameter count, ircd style.
 * @param parv  Parameters, ircd style, so we will start at parv[1].
 * @example
 * char buf[512];
 * concat_params(buf, sizeof(buf), parc, parv);
 * sendto_server(client, 0, 0, recv_mtags, ":%s SOMECOMMAND %s", client->name, buf);
 */
void concat_params(char *buf, int len, int parc, char *parv[])
{
	int i;

	*buf = '\0';
	for (i = 1; i < parc; i++)
	{
		char *param = parv[i];

		if (!param)
			break;

		if (*buf)
			strlcat(buf, " ", len);

		if (strchr(param, ' '))
		{
			/* Last parameter, with : */
			strlcat(buf, ":", len);
			strlcat(buf, parv[i], len);
			break;
		}
		strlcat(buf, parv[i], len);
	}
}

char *pretty_date(time_t t)
{
	static char buf[128];
	struct tm *tm;

	if (!t)
		time(&t);
	tm = gmtime(&t);
	snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d GMT",
	         1900 + tm->tm_year,
	         tm->tm_mon + 1,
	         tm->tm_mday,
	         tm->tm_hour,
	         tm->tm_min,
	         tm->tm_sec);

	return buf;
}

/** Find a particular message-tag in the 'mtags' list */
MessageTag *find_mtag(MessageTag *mtags, const char *token)
{
	for (; mtags; mtags = mtags->next)
		if (!strcmp(mtags->name, token))
			return mtags;
	return NULL;
}

void free_message_tags(MessageTag *m)
{
	MessageTag *m_next;

	for (; m; m = m_next)
	{
		m_next = m->next;
		safe_free(m->name);
		safe_free(m->value);
		safe_free(m);
	}
}

/** Duplicate a MessageTag structure.
 * @notes This duplicate a single MessageTag.
 *        It does not duplicate an entire linked list.
 */
MessageTag *duplicate_mtag(MessageTag *mtag)
{
	MessageTag *m = safe_alloc(sizeof(MessageTag));
	safe_strdup(m->name, mtag->name);
	safe_strdup(m->value, mtag->value);
	return m;
}

/** New message. Either really brand new, or inherited from other servers.
 * This function calls modules so they can add tags, such as:
 * msgid, time and account.
 */
void new_message(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list)
{
	Hook *h;
	for (h = Hooks[HOOKTYPE_NEW_MESSAGE]; h; h = h->next)
		(*(h->func.voidfunc))(sender, recv_mtags, mtag_list, NULL);
}

/** New message - SPECIAL edition. Either really brand new, or inherited
 * from other servers.
 * This function calls modules so they can add tags, such as:
 * msgid, time and account.
 * This special version deals in a special way with msgid in particular.
 * TODO: document
 * The pattern and vararg create a 'signature', this is normally
 * identical to the message that is sent to clients (end-users).
 * For example ":xyz JOIN #chan".
 */
void new_message_special(Client *sender, MessageTag *recv_mtags, MessageTag **mtag_list, FORMAT_STRING(const char *pattern), ...)
{
	Hook *h;
	va_list vl;
	char buf[512];

	va_start(vl, pattern);
	ircvsnprintf(buf, sizeof(buf), pattern, vl);
	va_end(vl);

	for (h = Hooks[HOOKTYPE_NEW_MESSAGE]; h; h = h->next)
		(*(h->func.voidfunc))(sender, recv_mtags, mtag_list, buf);
}

/** Default handler for parse_message_tags().
 * This is only used if the 'mtags' module is NOT loaded,
 * which would be quite unusual, but possible.
 */
void parse_message_tags_default_handler(Client *client, char **str, MessageTag **mtag_list)
{
	/* Just skip everything until the space character */
	for (; **str && **str != ' '; *str = *str + 1);
}

/** Default handler for mtags_to_string().
 * This is only used if the 'mtags' module is NOT loaded,
 * which would be quite unusual, but possible.
 */
char *mtags_to_string_default_handler(MessageTag *m, Client *client)
{
	return NULL;
}

/** Generate a BATCH id.
 * This can be used in a :serv BATCH +%s ... message
 */
void generate_batch_id(char *str)
{
	gen_random_alnum(str, BATCHLEN);
}

/** my_timegm: mktime()-like function which will use GMT/UTC.
 * Strangely enough there is no standard function for this.
 * On some *NIX OS's timegm() may be available, sometimes only
 * with the help of certain #define's which we may or may
 * not do.
 * Windows provides _mkgmtime().
 * In the other cases the man pages and basically everyone
 * suggests to set TZ to empty prior to calling mktime and
 * restoring it after the call. Whut? How ridiculous is that?
 */
time_t my_timegm(struct tm *tm)
{
#if HAVE_TIMEGM
	return timegm(tm);
#elif defined(_WIN32)
	return _mkgmtime(tm);
#else
	time_t ret;
	char *tz = NULL;

	safe_strdup(tz, getenv("TZ"));
	setenv("TZ", "", 1);
	ret = mktime(tm);
	if (tz)
	{
		setenv("TZ", tz, 1);
		safe_free(tz);
	} else {
		unsetenv("TZ");
	}
	tzset();

	return ret;
#endif
}

/** Convert an ISO 8601 timestamp ('server-time') to UNIX time */
time_t server_time_to_unix_time(const char *tbuf)
{
	struct tm tm;
	int dontcare = 0;
	time_t ret;

	if (!tbuf)
	{
		ircd_log(LOG_ERROR, "[BUG] server_time_to_unix_time() failed for NULL item. Incorrect S2S traffic?");
		return 0;
	}

	if (strlen(tbuf) < 20)
	{
		ircd_log(LOG_ERROR, "[BUG] server_time_to_unix_time() failed for short item '%s'", tbuf);
		return 0;
	}

	memset(&tm, 0, sizeof(tm));
	ret = sscanf(tbuf, "%d-%d-%dT%d:%d:%d.%dZ",
		&tm.tm_year,
		&tm.tm_mon,
		&tm.tm_mday,
		&tm.tm_hour,
		&tm.tm_min,
		&tm.tm_sec,
		&dontcare);

	if (ret != 7)
	{
		ircd_log(LOG_ERROR, "[BUG] server_time_to_unix_time() failed for '%s'", tbuf);
		return 0;
	}

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	ret = my_timegm(&tm);
	return ret;
}

/** Write a 64 bit integer.
 * @param fd   File descriptor
 * @param t    The value to write
 * @example write_int64(fd, 1234);
 * @returns 1 on success, 0 on failure.
 */
int write_int64(FILE *fd, uint64_t t)
{
	if (fwrite(&t, 1, sizeof(t), fd) < sizeof(t))
		return 0;
	return 1;
}

/** Write a 32 bit integer.
 * @param fd   File descriptor
 * @param t    The value to write
 * @example write_int32(fd, 1234);
 * @returns 1 on success, 0 on failure.
 */
int write_int32(FILE *fd, uint32_t t)
{
	if (fwrite(&t, 1, sizeof(t), fd) < sizeof(t))
		return 0;
	return 1;
}

/** Read a 64 bit integer.
 * @param fd   File descriptor
 * @param t    The value to write
 * @example read_int64(fd, &var);
 * @returns 1 on success, 0 on failure.
 */
int read_int64(FILE *fd, uint64_t *t)
{
	if (fread(t, 1, sizeof(uint64_t), fd) < sizeof(uint64_t))
		return 0;
	return 1;
}

/** Read a 64 bit integer.
 * @param fd   File descriptor
 * @param t    The value to write
 * @example read_int64(fd, &var);
 * @returns 1 on success, 0 on failure.
 */
int read_int32(FILE *fd, uint32_t *t)
{
	if (fread(t, 1, sizeof(uint32_t), fd) < sizeof(uint32_t))
		return 0;
	return 1;
}

/** Read binary data.
 * @param fd   File descriptor
 * @param buf  Pointer to buffer
 * @param len  Size of buffer
 * @example read_data(fd, buf, sizeof(buf));
 * @notes This function is not used much, in most cases
 *        you should use read_str(), read_int32() or
 *        read_int64() instead.
 * @returns 1 on success, 0 on failure.
 */
int read_data(FILE *fd, void *buf, size_t len)
{
	if (fread(buf, 1, len, fd) < len)
		return 0;
	return 1;
}

/** Write binary data.
 * @param fd   File descriptor
 * @param buf  Pointer to buffer
 * @param len  Size of buffer
 * @example write_data(fd, buf, sizeof(buf));
 * @notes This function is not used much, in most cases
 *        you should use write_str(), write_int32() or
 *        write_int64() instead.
 * @returns 1 on success, 0 on failure.
 */
int write_data(FILE *fd, const void *buf, size_t len)
{
	if (fwrite(buf, 1, len, fd) < len)
		return 0;
	return 1;
}

/** Write a string.
 * @param fd   File descriptor
 * @param x    Pointer to string
 * @example write_str(fd, "hello there!");
 * @notes This function can write a string up to 65534
 *        characters, which should be plenty for usage
 *        in UnrealIRCd.
 *        Note that 'x' can safely be NULL.
 * @returns 1 on success, 0 on failure.
 */
int write_str(FILE *fd, char *x)
{
	uint16_t len;

	len = x ? strlen(x) : 0xffff;
	if (!write_data(fd, &len, sizeof(len)))
		return 0;
	if ((len > 0) && (len < 0xffff))
	{
		if (!write_data(fd, x, len))
			return 0;
	}
	return 1;
}

/** Read a string.
 * @param fd   File descriptor
 * @param x    Pointer to string pointer
 * @example write_str(fd, &str);
 * @notes This function will allocate memory for the data
 *        and set the string pointer to this value.
 *        If a NULL pointer was written via write_str()
 *        then read_str() may also return a NULL pointer.
 * @returns 1 on success, 0 on failure.
 */
int read_str(FILE *fd, char **x)
{
	uint16_t len;
	size_t size;

	*x = NULL;

	if (!read_data(fd, &len, sizeof(len)))
		return 0;

	if (len == 0xffff)
	{
		/* Magic value meaning NULL */
		*x = NULL;
		return 1;
	}

	if (len == 0)
	{
		/* 0 means empty string */
		safe_strdup(*x, "");
		return 1;
	}

	if (len > 10000)
		return 0;

	size = len;
	*x = safe_alloc(size + 1);
	if (!read_data(fd, *x, size))
	{
		safe_free(*x);
		return 0;
	}
	(*x)[len] = 0;
	return 1;
}
