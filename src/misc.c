/*
 *   Unreal Internet Relay Chat Daemon, src/misc.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
 *   Copyright (C) 1999-present UnrealIRCd team
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

/** @file
 * @brief Miscellaneous functions that don't fit in other files.
 * Generally these are either simple helper functions or larger
 * functions that don't fit in either user.c, channel.c.
 */

#include "unrealircd.h"

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
	{ SPAMF_CHANMSG,	'c',	"channel",		"PRIVMSG" },
	{ SPAMF_USERMSG,	'p',	"private",		"PRIVMSG" },
	{ SPAMF_USERNOTICE,	'n',	"private-notice",	"NOTICE" },
	{ SPAMF_CHANNOTICE,	'N',	"channel-notice",	"NOTICE" },
	{ SPAMF_PART,		'P',	"part",			"PART" },
	{ SPAMF_QUIT,		'q',	"quit",			"QUIT" },
	{ SPAMF_DCC,		'd',	"dcc",			"PRIVMSG" },
	{ SPAMF_USER,		'u',	"user",			"NICK" },
	{ SPAMF_AWAY,		'a',	"away",			"AWAY" },
	{ SPAMF_TOPIC,		't',	"topic",		"TOPIC" },
	{ SPAMF_MTAG,		'T',	"message-tag",		"message-tag" },
	{ 0, 0, 0, 0 }
};

/** IRC Statistics (quite useless?) */
struct IRCStatistics ircstats;

/** Main IRCd logging function.
 * @param flags		One of LOG_* (eg: LOG_ERROR)
 * @param format	Format string
 * @param ...		Arguments
 * @note This function is safe to call at all times. It provides
 *       protection against recursion.
 */
void ircd_log(int flags, FORMAT_STRING(const char *format), ...)
{
	static int last_log_file_warning = 0;
	static char recursion_trap=0;

	va_list ap;
	ConfigItem_log *logs;
	char buf[2048], timebuf[128];
	struct stat fstats;
	int written = 0;
	int n;

	/* Trap infinite recursions to avoid crash if log file is unavailable,
	 * this will also avoid calling ircd_log from anything else called
	 */
	if (recursion_trap == 1)
		return;

	recursion_trap = 1;

	/* NOTE: past this point you CANNOT just 'return'.
	 * You must set 'recursion_trap = 0;' before 'return'!
	 */

	va_start(ap, format);
	ircvsnprintf(buf, sizeof(buf), format, ap);
	va_end(ap);
	snprintf(timebuf, sizeof(timebuf), "[%s] - ", myctime(TStime()));

	RunHook3(HOOKTYPE_LOG, flags, timebuf, buf);
	strlcat(buf, "\n", sizeof(buf));

	if (!loop.ircd_forked && (flags & LOG_ERROR))
	{
#ifdef _WIN32
		win_log("* %s", buf);
#else
		fprintf(stderr, "%s", buf);
#endif
	}

	/* In case of './unrealircd configtest': don't write to log file, only to stderr */
	if (loop.config_test)
	{
		recursion_trap = 0;
		return;
	}

	for (logs = conf_log; logs; logs = logs->next)
	{
		if (!(logs->flags & flags))
			continue;

#ifdef HAVE_SYSLOG
		if (logs->file && !strcasecmp(logs->file, "syslog"))
		{
			syslog(LOG_INFO, "%s", buf);
			written++;
			continue;
		}
#endif

		/* This deals with dynamic log file names, such as ircd.%Y-%m-%d.log */
		if (logs->filefmt)
		{
			char *fname = unreal_strftime(logs->filefmt);
			if (logs->file && (logs->logfd != -1) && strcmp(logs->file, fname))
			{
				/* We are logging already and need to switch over */
				fd_close(logs->logfd);
				logs->logfd = -1;
			}
			safe_strdup(logs->file, fname);
		}

		/* log::maxsize code */
		if (logs->maxsize && (stat(logs->file, &fstats) != -1) && fstats.st_size >= logs->maxsize)
		{
			char oldlog[512];
			if (logs->logfd == -1)
			{
				/* Try to open, so we can write the 'Max file size reached' message. */
				logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
			}
			if (logs->logfd != -1)
			{
				if (write(logs->logfd, "Max file size reached, starting new log file\n", 45) < 0)
				{
					/* We already handle the unable to write to log file case for normal data.
					 * I think we can get away with not handling this one.
					 */
					;
				}
				fd_close(logs->logfd);
			}
			logs->logfd = -1;

			/* Rename log file to xxxxxx.old */
			snprintf(oldlog, sizeof(oldlog), "%s.old", logs->file);
			unlink(oldlog); /* windows rename cannot overwrite, so unlink here.. ;) */
			rename(logs->file, oldlog);
		}

		/* generic code for opening log if not open yet.. */
		if (logs->logfd == -1)
		{
			logs->logfd = fd_fileopen(logs->file, O_CREAT|O_APPEND|O_WRONLY);
			if (logs->logfd == -1)
			{
				if (!loop.ircd_booted)
				{
					config_status("WARNING: Unable to write to '%s': %s", logs->file, strerror(ERRNO));
				} else {
					if (last_log_file_warning + 300 < TStime())
					{
						config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", logs->file, strerror(ERRNO));
						last_log_file_warning = TStime();
					}
				}
				continue;
			}
		}

		/* Now actually WRITE to the log... */
		if (write(logs->logfd, timebuf, strlen(timebuf)) < 0)
		{
			/* Let's ignore any write errors for this one. Next write() will catch it... */
			;
		}
		n = write(logs->logfd, buf, strlen(buf));
		if (n == strlen(buf))
		{
			written++;
		}
		else
		{
			if (!loop.ircd_booted)
			{
				config_status("WARNING: Unable to write to '%s': %s", logs->file, strerror(ERRNO));
			} else {
				if (last_log_file_warning + 300 < TStime())
				{
					config_status("WARNING: Unable to write to '%s': %s. This warning will not re-appear for at least 5 minutes.", logs->file, strerror(ERRNO));
					last_log_file_warning = TStime();
				}
			}
		}
	}

	recursion_trap = 0;
}

/** Returns the date in rather long string */
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

/** Return a string with the "pretty date" - yeah, another variant */
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

/** Helper function for make_user_host() and friends.
 * Fixes a string so that the first white space found becomes an end of
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

/** Create a user@host based on the provided name and host */
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

/** Create a nick!user@host string based on the provided variables.
 * If any of the variables are NULL, it becomes * (asterisk)
 * This is the reentrant safe version.
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

/** Create a nick!user@host string based on the provided variables.
 * If any of the variables are NULL, it becomes * (asterisk)
 * This version uses static storage.
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
			ircsnprintf(nbuf, sizeof(nbuf), "%s[%s@%s.%u]",
			    client->name,
			    IsIdentSuccess(client) ? client->ident : "",
			    client->ip ? client->ip : "???",
			    (unsigned int)client->local->port);
		else
		{
			if (mycmp(client->name, client->local->sockhost))
				ircsnprintf(nbuf, sizeof(nbuf), "%s[%s]",
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
	ircsnprintf(nbuf, sizeof(nbuf), "%s[%-.*s@%-.*s]",
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

/** Returns 1 if 'from' is on the allow list of 'to' */
int on_dccallow_list(Client *to, Client *from)
{
	Link *lp;

	for(lp = to->user->dccallow; lp; lp = lp->next)
		if(lp->flags == DCC_LINK_ME && lp->value.client == from)
			return 1;
	return 0;
}

/** Delete all DCCALLOW references.
 * Ultimately, this should be moved to modules/dccallow.c
 */
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
	{
		if (acptr != from && !(acptr->direction && (acptr->direction == from)))
			sendto_one(acptr, mtags, "SQUIT %s :%s", client->name, comment);
	}

	recurse_remove_clients(client, mtags, splitstr);
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
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
	exit_client_ex(client, client->direction, recv_mtags, comment);
}

/** Exit this IRC client, and all the dependents (users, servers) if this is a server.
 * @param client        The client to exit.
 * @param recv_mtags  Message tags to use as a base (if any).
 * @param comment     The (s)quit message
 */
void exit_client_ex(Client *client, Client *origin, MessageTag *recv_mtags, char *comment)
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
				ircd_log(LOG_CLIENT, "Disconnect - (%lld:%lld:%lld) %s!%s@%s [%s] [vhost: %s] (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					client->name, client->user->username,
					client->user->realhost, GetIP(client), client->user->virthost, comment);
			else
				ircd_log(LOG_CLIENT, "Disconnect - (%lld:%lld:%lld) %s!%s@%s [%s] (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					client->name, client->user->username, client->user->realhost, GetIP(client), comment);
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

		remove_dependents(client, origin, recv_mtags, comment, splitstr);

		RunHook2(HOOKTYPE_SERVER_QUIT, client, recv_mtags);
	}
	else if (IsUser(client) && !IsKilled(client))
	{
		sendto_server(client, 0, 0, recv_mtags, ":%s QUIT :%s", client->id, comment);
	}

	/* Finally, the client/server itself exits.. */
	exit_one_client(client, recv_mtags, comment);

	free_message_tags(mtags_generated);
}

/** Initialize the (quite useless) IRC statistics */
void initstats(void)
{
	memset(&ircstats, 0, sizeof(ircstats));
}

/** Verify operator count, to catch bugs introduced by flawed services */
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

/** Replace underscores back to the space character.
 * This is used for the spamfilter reason.
 */
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

/** Replace spaces to underscore characters.
 * This is used for the spamfilter reason.
 */
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

/** Returns 1 if this is a channel from set::auto-join or set::oper-auto-join */
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

// FIXME: should detect <U5 ;)
int mixed_network(void)
{
	Client *client;

	list_for_each_entry(client, &server_list, special_node)
	{
		if (!IsServer(client) || IsULine(client))
			continue; /* skip u-lined servers (=non-unreal, unless you configure your ulines badly, that is) */
		// uh.. right.. bit hard to detect u4 this way now :D
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

/** Check if a client matches any of the masks in the mask list.
 * The following rules apply:
 * - If you have only negating entries, like '!abc' and '!def', then
 *   we assume an implicit * rule first, since that is clearly what
 *   the user wants.
 * - If you have a mix, like '*.com', '!irc1*', '!irc2*' then the
 *   implicit * is dropped and we assume you only want to match *.com,
 *   with the exception of irc1*.com and irc2*.com.
 * - If you only have normal entries without ! then things are
 *   as they always are.
 * @param client	The client to run the mask match against
 * @param mask		The mask entry from the config file
 * @returns 1 on match, 0 on non-match.
 */
int unreal_mask_match(Client *client, ConfigItem_mask *mask)
{
	int retval = 1;
	ConfigItem_mask *m;

	if (!mask)
		return 0; /* Empty mask block is no match */

	/* First check normal matches (without ! prefix) */
	for (m = mask; m; m = m->next)
	{
		if (m->mask[0] != '!')
		{
			retval = 0; /* no implicit * */
			if (match_user(m->mask, client, MATCH_CHECK_REAL|MATCH_CHECK_EXTENDED))
			{
				retval = 1;
				break;
			}
		}
	}

	if (retval)
	{
		/* We matched. Check for exceptions (with ! prefix) */
		for (m = mask; m; m = m->next)
		{
			if ((m->mask[0] == '!') && match_user(m->mask+1, client, MATCH_CHECK_REAL|MATCH_CHECK_EXTENDED))
				return 0;
		}
	}

	return retval;
}

/** Check if a string matches any of the masks in the mask list.
 * The following rules apply:
 * - If you have only negating entries, like '!abc' and '!def', then
 *   we assume an implicit * rule first, since that is clearly what
 *   the user wants.
 * - If you have a mix, like '*.com', '!irc1*', '!irc2*' then the
 *   implicit * is dropped and we assume you only want to match *.com,
 *   with the exception of irc1*.com and irc2*.com.
 * - If you only have normal entries without ! then things are
 *   as they always are.
 * @param name	The name to run the mask matching on
 * @param mask	The mask entry from the config file
 * @returns 1 on match, 0 on non-match.
 */
int unreal_mask_match_string(const char *name, ConfigItem_mask *mask)
{
	int retval = 1;
	ConfigItem_mask *m;

	if (!mask)
		return 0; /* Empty mask block is no match */

	/* First check normal matches (without ! prefix) */
	for (m = mask; m; m = m->next)
	{
		if (m->mask[0] != '!')
		{
			retval = 0; /* no implicit * */
			if (match_simple(m->mask, name))
			{
				retval = 1;
				break;
			}
		}
	}

	if (retval)
	{
		/* We matched. Check for exceptions (with ! prefix) */
		for (m = mask; m; m = m->next)
		{
			if ((m->mask[0] == '!') && match_simple(m->mask+1, name))
				return 0;
		}
	}

	return retval;
}

/** Our own strcasestr implementation because strcasestr is
 * often not available or is not working correctly.
 */
char *our_strcasestr(char *haystack, char *needle)
{
	int i;
	int nlength = strlen(needle);
	int hlength = strlen(haystack);

	if (nlength > hlength)
		return NULL;

	if (hlength <= 0)
		return NULL;

	if (nlength <= 0)
		return haystack;

	for (i = 0; i <= (hlength - nlength); i++)
	{
		if (strncasecmp (haystack + i, needle, nlength) == 0)
			return haystack + i;
	}

	return NULL; /* not found */
}

/** Add a title to the users' WHOIS ("special whois"). Broadcast change to servers.
 * @param client	The client
 * @param tag		A tag used internally and for server-to-server traffic,
 *			not visible to end-users.
 * @param priority	Priority - for ordering multiple swhois entries
 *                      (lower number = further up in the swhoises list in WHOIS)
 * @param swhois	The actual special whois title (string) you want to add to the user
 * @param from		Who added this entry
 * @param skip		Which server(-side) to skip broadcasting this entry to.
 */
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
		from->id, client->id, swhois);

	sendto_server(skip, PROTO_EXTSWHOIS, 0, NULL, ":%s SWHOIS %s + %s %d :%s",
		from->id, client->id, tag, priority, swhois);

	return 0;
}

/** Delete swhois title(s).
 * Delete swhois by tag and swhois. Then broadcast this change to all other servers.
 * @param client	The client
 * @param tag		A tag used internally and for server-to-server traffic,
 *			not visible to end-users.
 * @param swhois	The actual special whois title (string) you are removing
 * @param from		Who added this entry earlier on
 * @param skip		Which server(-side) to skip broadcasting this entry to.
 * @note If you use swhois "*" then it will remove all swhois titles for that tag
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
				from->id, client->id);

			sendto_server(skip, PROTO_EXTSWHOIS, 0, NULL, ":%s SWHOIS %s - %s %d :%s",
				from->id, client->id, tag, 0, swhois);

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
 * @note This function will call exit_client() appropriately.
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

/** Our stpcpy implementation - discouraged due to lack of bounds checking */
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
 * @section ex1 Example
 * @code
 * char buf[512];
 * concat_params(buf, sizeof(buf), parc, parv);
 * sendto_server(client, 0, 0, recv_mtags, ":%s SOMECOMMAND %s", client->name, buf);
 * @endcode
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

		if (strchr(param, ' ') || (*param == ':'))
		{
			/* Last parameter, with : */
			strlcat(buf, ":", len);
			strlcat(buf, parv[i], len);
			break;
		}
		strlcat(buf, parv[i], len);
	}
}

/** Find a particular message-tag in the 'mtags' list */
MessageTag *find_mtag(MessageTag *mtags, const char *token)
{
	for (; mtags; mtags = mtags->next)
		if (!strcmp(mtags->name, token))
			return mtags;
	return NULL;
}

/** Free all message tags in the list 'm' */
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
 * @note  This duplicate a single MessageTag.
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

/** Default handler for add_silence().
 * This is only used if the 'silence' module is NOT loaded,
 * which would be unusual, but possible.
 */
int add_silence_default_handler(Client *client, const char *mask, int senderr)
{
	return 0;
}

/** Default handler for del_silence().
 * This is only used if the 'silence' module is NOT loaded,
 * which would be unusual, but possible.
 */
int del_silence_default_handler(Client *client, const char *mask)
{
	return 0;
}

/** Default handler for is_silenced().
 * This is only used if the 'silence' module is NOT loaded,
 * which would be unusual, but possible.
 */
int is_silenced_default_handler(Client *client, Client *acptr)
{
	return 0;
}

/** Generate a BATCH id.
 * This can be used in a :serv BATCH +%s ... message
 */
void generate_batch_id(char *str)
{
	gen_random_alnum(str, BATCHLEN);
}

/** A default handler if labeled-response module is not loaded.
 * Normally a NOOP, but since caller will safe_free it
 * later we do actually allocate something.
 */
void *labeled_response_save_context_default_handler(void)
{
	return safe_alloc(8);
}

/** A default handler for if labeled-response module is not loaded */
void labeled_response_set_context_default_handler(void *ctx)
{
}

/** A default handler for if labeled-response module is not loaded */
void labeled_response_force_end_default_handler(void)
{
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

/** Write a 64 bit integer to a file.
 * @param fd   File descriptor
 * @param t    The value to write
 * @returns 1 on success, 0 on failure.
 */
int write_int64(FILE *fd, uint64_t t)
{
	if (fwrite(&t, 1, sizeof(t), fd) < sizeof(t))
		return 0;
	return 1;
}

/** Write a 32 bit integer to a file.
 * @param fd   File descriptor
 * @param t    The value to write
 * @returns 1 on success, 0 on failure.
 */
int write_int32(FILE *fd, uint32_t t)
{
	if (fwrite(&t, 1, sizeof(t), fd) < sizeof(t))
		return 0;
	return 1;
}

/** Read a 64 bit integer from a file.
 * @param fd   File descriptor
 * @param t    The value to write
 * @returns 1 on success, 0 on failure.
 */
int read_int64(FILE *fd, uint64_t *t)
{
	if (fread(t, 1, sizeof(uint64_t), fd) < sizeof(uint64_t))
		return 0;
	return 1;
}

/** Read a 32 bit integer from a file.
 * @param fd   File descriptor
 * @param t    The value to write
 * @returns 1 on success, 0 on failure.
 */
int read_int32(FILE *fd, uint32_t *t)
{
	if (fread(t, 1, sizeof(uint32_t), fd) < sizeof(uint32_t))
		return 0;
	return 1;
}

/** Read binary data from a file.
 * @param fd   File descriptor
 * @param buf  Pointer to buffer
 * @param len  Size of buffer
 * @note  This function is not used much, in most cases
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

/** Write binary data to a file.
 * @param fd   File descriptor
 * @param buf  Pointer to buffer
 * @param len  Size of buffer
 * @note  This function is not used much, in most cases
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

/** Write a string to a file.
 * @param fd   File descriptor
 * @param x    Pointer to string
 * @note  This function can write a string up to 65534
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

/** Read a string from a file.
 * @param fd   File descriptor
 * @param x    Pointer to string pointer
 * @note  This function will allocate memory for the data
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

/** Convert binary 'data' of size 'len' to a hexadecimal string 'str'.
 * The caller is responsible to ensure that 'str' is sufficiently large.
 */
void binarytohex(void *data, size_t len, char *str)
{
	const char hexchars[16] = "0123456789abcdef";
	char *datastr = (char *)data;
	int i, n = 0;

	for (i=0; i<len; i++)
	{
		str[n++] = hexchars[(datastr[i] >> 4) & 0xF];
		str[n++] = hexchars[datastr[i] & 0xF];
	}
	str[n] = '\0';
}

/** Generates an MD5 checksum - binary version.
 * @param mdout[out] Buffer to store result in, the result will be 16 bytes in binary
 *                   (not ascii printable!).
 * @param src[in]    The input data used to generate the checksum.
 * @param n[in]      Length of data.
 * @deprecated       The MD5 algorithm is deprecated and insecure,
 *                   so only use this if absolutely needed.
 */
void DoMD5(char *mdout, const char *src, unsigned long n)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	unsigned int md_len;
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (EVP_DigestInit_ex(mdctx, md5_function, NULL) != 1)
		abort();
	EVP_DigestUpdate(mdctx, src, n);
	EVP_DigestFinal_ex(mdctx, mdout, &md_len);
	EVP_MD_CTX_free(mdctx);
#else
	MD5_CTX hash;

	MD5_Init(&hash);
	MD5_Update(&hash, src, n);
	MD5_Final(mdout, &hash);
#endif
}

/** Generates an MD5 checksum - ASCII printable string (0011223344..etc..).
 * @param dst[out]  Buffer to store result in, this will be the result will be
 *                  32 characters + nul terminator, so needs to be at least 33 characters.
 * @param src[in]   The input data used to generate the checksum.
 * @param n[in]     Length of data.
 * @deprecated      The MD5 algorithm is deprecated and insecure,
 *                  so only use this if absolutely needed.
 */
char *md5hash(char *dst, const char *src, unsigned long n)
{
	char tmp[16];

	DoMD5(tmp, src, n);
	binarytohex(tmp, sizeof(tmp), dst);
	return dst;
}

/** Generates a SHA256 checksum - binary version.
 * Most people will want to use sha256hash() instead which outputs hex.
 * @param dst[out]  Buffer to store result in, which needs to be 32 bytes in length
 *                  (SHA256_DIGEST_LENGTH).
 * @param src[in]   The input data used to generate the checksum.
 * @param n[in]     Length of data.
 */
void sha256hash_binary(char *dst, const char *src, unsigned long n)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	unsigned int md_len;
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (EVP_DigestInit_ex(mdctx, sha256_function, NULL) != 1)
		abort();
	EVP_DigestUpdate(mdctx, src, n);
	EVP_DigestFinal_ex(mdctx, dst, &md_len);
	EVP_MD_CTX_free(mdctx);
#else
	SHA256_CTX hash;

	SHA256_Init(&hash);
	SHA256_Update(&hash, src, n);
	SHA256_Final(dst, &hash);
#endif
}

/** Generates a SHA256 checksum - ASCII printable string (0011223344..etc..).
 * @param dst[out]  Buffer to store result in, which needs to be 65 bytes minimum.
 * @param src[in]   The input data used to generate the checksum.
 * @param n[in]     Length of data.
 */
char *sha256hash(char *dst, const char *src, unsigned long n)
{
	char binaryhash[SHA256_DIGEST_LENGTH];

	sha256hash_binary(binaryhash, src, n);
	binarytohex(binaryhash, sizeof(binaryhash), dst);
	return dst;
}

/** Calculate the SHA256 checksum of a file */
char *sha256sum_file(const char *fname)
{
	FILE *fd;
	char buf[2048];
	SHA256_CTX hash;
	char binaryhash[SHA256_DIGEST_LENGTH];
	static char hexhash[SHA256_DIGEST_LENGTH*2+1];
	int n;
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	unsigned int md_len;
	EVP_MD_CTX *mdctx;

	mdctx = EVP_MD_CTX_new();
	if (EVP_DigestInit_ex(mdctx, sha256_function, NULL) != 1)
		abort();
#else
	SHA256_Init(&hash);
#endif

	fd = fopen(fname, "rb");
	if (!fd)
		return NULL;

	while ((n = fread(buf, 1, sizeof(buf), fd)) > 0)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
		EVP_DigestUpdate(mdctx, buf, n);
#else
		SHA256_Update(&hash, buf, n);
#endif
	}
	fclose(fd);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	EVP_DigestFinal_ex(mdctx, binaryhash, &md_len);
	EVP_MD_CTX_free(mdctx);
#else
	SHA256_Final(binaryhash, &hash);
#endif
	binarytohex(binaryhash, sizeof(binaryhash), hexhash);
	return hexhash;
}

/** Generates a SHA1 checksum - binary version.
 * @param dst[out]  Buffer to store result in, which needs to be 32 bytes in length
 *                  (SHA1_DIGEST_LENGTH).
 * @param src[in]   The input data used to generate the checksum.
 * @param n[in]     Length of data.
 * @deprecated      The SHA1 algorithm is deprecated and insecure,
 *                  so only use this if absolutely needed.
 */
void sha1hash_binary(char *dst, const char *src, unsigned long n)
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	unsigned int md_len;
	EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
	if (EVP_DigestInit_ex(mdctx, sha1_function, NULL) != 1)
		abort();
	EVP_DigestUpdate(mdctx, src, n);
	EVP_DigestFinal_ex(mdctx, dst, &md_len);
	EVP_MD_CTX_free(mdctx);
#else
	SHA_CTX hash;

	SHA1_Init(&hash);
	SHA1_Update(&hash, src, n);
	SHA1_Final(dst, &hash);
#endif
}

/** Remove a suffix from a filename, eg ".c" (if it is present) */
char *filename_strip_suffix(const char *fname, const char *suffix)
{
	static char buf[512];

	strlcpy(buf, fname, sizeof(buf));

	if (suffix)
	{
		int buf_len = strlen(buf);
		int suffix_len = strlen(suffix);
		if (buf_len >= suffix_len)
		{
			if (!strncmp(buf+buf_len-suffix_len, suffix, suffix_len))
				buf[buf_len-suffix_len] = '\0';
		}
	} else {
		char *p = strrchr(buf, '.');
		if (p)
			*p = '\0';
	}
	return buf;
}

/** Add a suffix to a filename, eg ".c" */
char *filename_add_suffix(const char *fname, const char *suffix)
{
	static char buf[512];
	snprintf(buf, sizeof(buf), "%s%s", fname, suffix);
	return buf;
}

/* Returns 1 if the filename has the suffix, eg ".c" */
int filename_has_suffix(const char *fname, const char *suffix)
{
	char buf[256];
	char *p;
	strlcpy(buf, fname, sizeof(buf));
	p = strrchr(buf, '.');
	if (!p)
		return 0;
	if (!strcmp(p, suffix))
		return 1;
	return 0;
}

/** Check if the specified file or directory exists */
int file_exists(char *file)
{
#ifdef _WIN32
	if (_access(file, 0) == 0)
#else
	if (access(file, 0) == 0)
#endif
		return 1;
	return 0;
}

/** Get the file creation time */
time_t get_file_time(char *fname)
{
	struct stat st;

	if (stat(fname, &st) != 0)
		return 0;

	return (time_t)st.st_ctime;
}

/** Get the size of a file */
long get_file_size(char *fname)
{
	struct stat st;

	if (stat(fname, &st) != 0)
		return -1;

	return (long)st.st_size;
}

/** Add a line to a MultiLine list */
void addmultiline(MultiLine **l, char *line)
{
	MultiLine *m = safe_alloc(sizeof(MultiLine));
	safe_strdup(m->line, line);
	append_ListItem((ListStruct *)m, (ListStruct **)l);
}

/** Free an entire MultiLine list */
void freemultiline(MultiLine *l)
{
	MultiLine *l_next;
	for (; l; l = l_next)
	{
		l_next = l->next;
		safe_free(l->line);
		safe_free(l);
	}
}

/** Convert a sendtype to a command string */
char *sendtype_to_cmd(SendType sendtype)
{
	if (sendtype == SEND_TYPE_PRIVMSG)
		return "PRIVMSG";
	if (sendtype == SEND_TYPE_NOTICE)
		return "NOTICE";
	if (sendtype == SEND_TYPE_TAGMSG)
		return "TAGMSG";
	return NULL;
}

/** Check password strength.
 * @param pass		The password to check
 * @param min_length	The minimum length of the password
 * @param strict	Whether to require UPPER+lower+digits
 * @returns 1 if good, 0 if not.
 */
int check_password_strength(char *pass, int min_length, int strict, char **err)
{
	char has_lowercase=0, has_uppercase=0, has_digit=0;
	char *p;
	static char buf[256];

	if (err)
		*err = NULL;

	if (strlen(pass) < min_length)
	{
		if (err)
		{
			snprintf(buf, sizeof(buf), "Password must be at least %d characters", min_length);
			*err = buf;
		}
		return 0;
	}

	for (p=pass; *p; p++)
	{
		if (islower(*p))
			has_lowercase = 1;
		else if (isupper(*p))
			has_uppercase = 1;
		else if (isdigit(*p))
			has_digit = 1;
	}

	if (strict)
	{
		if (!has_lowercase)
		{
			if (err)
				*err = "Password must contain at least 1 lowercase character";
			return 0;
		} else
		if (!has_uppercase)
		{
			if (err)
				*err = "Password must contain at least 1 UPPERcase character";
			return 0;
		} else
		if (!has_digit)
		{
			if (err)
				*err = "Password must contain at least 1 digit (number)";
			return 0;
		}
	}

	return 1;
}

int valid_secret_password(char *pass, char **err)
{
	return check_password_strength(pass, 10, 1, err);
}

int running_interactively(void)
{
#ifndef _WIN32
	char *s;

	if (!isatty(0))
		return 0;

	s = getenv("TERM");
	if (!s || !strcasecmp(s, "dumb") || !strcasecmp(s, "none"))
		return 0;

	return 1;
#else
	return IsService ? 0 : 1;
#endif
}

/** Skip whitespace (if any) */
void skip_whitespace(char **p)
{
	for (; **p == ' ' || **p == '\t'; *p = *p + 1);
}

/** Keep reading '*p' until we hit any of the 'stopchars'.
 * Actually behaves like strstr() but then hit the end
 * of the string (\0) i guess?
 */
void read_until(char **p, char *stopchars)
{
	for (; **p && !strchr(stopchars, **p); *p = *p + 1);
}
