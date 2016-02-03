
/*
 *   Unreal Internet Relay Chat Daemon, src/s_misc.c
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

#ifndef _WIN32
#include <sys/time.h>
#endif
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#if !defined(ULTRIX) && !defined(SGI) && \
    !defined(__convex__) && !defined(_WIN32)
# include <sys/param.h>
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
# include <time.h>
#endif
#ifdef HPUX
#include <unistd.h>
#endif
#ifdef _WIN32
# include <io.h>
#endif
#include "h.h"
#include "proto.h"
#include "channel.h"
#include <string.h>

extern ircstats IRCstats;
extern char	*me_hash;

static void exit_one_client(aClient *, const char *);

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
	{ BAN_ACT_TEMPSHUN,	'S',	"tempshun" },
	{ BAN_ACT_SHUN,		's',	"shun" },
	{ BAN_ACT_KLINE,	'k',	"kline" },
	{ BAN_ACT_ZLINE,	'z',	"zline" },
	{ BAN_ACT_GLINE,	'g',	"gline" },
	{ BAN_ACT_GZLINE,	'Z',	"gzline" },
	{ BAN_ACT_BLOCK,	'b',	"block" },
	{ BAN_ACT_DCCBLOCK,	'd',	"dccblock" },
	{ BAN_ACT_VIRUSCHAN,'v',	"viruschan" },
	{ BAN_ACT_WARN,		'w',	"warn" },
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
	{ 0, 0, 0 }
};


/*
 * stats stuff
 */
struct stats ircst, *ircstp = &ircst;

char *date(time_t clock)
{
	static char buf[80], plus;
	struct tm *lt, *gm;
	struct tm gmbuf;
	int  minswest;

	if (!clock)
		time(&clock);
	gm = gmtime(&clock);
	bcopy((char *)gm, (char *)&gmbuf, sizeof(gmbuf));
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


char *convert_time (time_t ltime)
{
	unsigned long days = 0,hours = 0,minutes = 0,seconds = 0;
	static char buffer[40];


	*buffer = '\0';
	seconds = ltime % 60;
	ltime = (ltime - seconds) / 60;
	minutes = ltime%60;
	ltime = (ltime - minutes) / 60;
	hours = ltime % 24;
	days = (ltime - hours) / 24;
	ircsnprintf(buffer, sizeof(buffer), "%ludays %luhours %luminutes %lusecs",
days, hours, minutes, seconds);
	return buffer;
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

	bzero(namebuf, sizeof(namebuf));
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


/**
 ** myctime()
 **   This is like standard ctime()-function, but it zaps away
 **   the newline from the end of that string. Also, it takes
 **   the time value as parameter, instead of pointer to it.
 **   Note that it is necessary to copy the string to alternate
 **   buffer (who knows how ctime() implements it, maybe it statically
 **   has newline there and never 'refreshes' it -- zapping that
 **   might break things in other places...)
 **
 **/

char *myctime(time_t value)
{
	static char buf[28];
	char *p;

	(void)strlcpy(buf, ctime(&value), sizeof buf);
	if ((p = (char *)index(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

/*
** check_registered_user is used to cancel message, if the
** originator is a server or not registered yet. In other
** words, passing this test, *MUST* guarantee that the
** sptr->user exists (not checked after this--let there
** be coredumps to catch bugs... this is intentional --msa ;)
**
** There is this nagging feeling... should this NOT_REGISTERED
** error really be sent to remote users? This happening means
** that remote servers have this user registered, althout this
** one has it not... Not really users fault... Perhaps this
** error message should be restricted to local clients and some
** other thing generated for remotes...
*/
int  check_registered_user(aClient *sptr)
{
	if (!IsRegisteredUser(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	}
	return 0;
}

/*
** check_registered user cancels message, if 'x' is not
** registered (e.g. we don't know yet whether a server
** or user)
*/
int  check_registered(aClient *sptr)
{
	if (!IsRegistered(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOTREGISTERED), me.name, "*");
		return -1;
	}
	return 0;
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
**	Watch out the allocation of "nbuf", if either sptr->name
**	or sptr->local->sockhost gets changed into pointers instead of
**	directly allocated within the structure...
**
** NOTE 2:
**	Function return either a pointer to the structure (sptr) or
**	to internal buffer (nbuf). *NEVER* use the returned pointer
**	to modify what it points!!!
*/
char *get_client_name(aClient *sptr, int showip)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (MyConnect(sptr))
	{
		if (showip)
			(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%s@%s.%u]",
			    sptr->name,
			    (!(sptr->flags & FLAGS_GOTID)) ? "" :
			    sptr->username,
			    sptr->ip ? sptr->ip : "???",
			    (unsigned int)sptr->local->port);
		else
		{
			if (mycmp(sptr->name, sptr->local->sockhost))
				(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%s]",
				    sptr->name, sptr->local->sockhost);
			else
				return sptr->name;
		}
		return nbuf;
	}
	return sptr->name;
}

char *get_client_host(aClient *cptr)
{
	static char nbuf[HOSTLEN * 2 + USERLEN + 5];

	if (!MyConnect(cptr))
		return cptr->name;
	if (!cptr->local->hostp)
		return get_client_name(cptr, FALSE);
	(void)ircsnprintf(nbuf, sizeof(nbuf), "%s[%-.*s@%-.*s]",
	    cptr->name, USERLEN,
  	    (!(cptr->flags & FLAGS_GOTID)) ? "" : cptr->username,
	    HOSTLEN, cptr->local->hostp->h_name);
	return nbuf;
}

/*
 * Set sockhost to 'host'. Skip the user@ part of 'host' if necessary.
 */
void set_sockhost(aClient *cptr, char *host)
{
	char *s;
	if ((s = (char *)index(host, '@')))
		s++;
	else
		s = host;
	strlcpy(cptr->local->sockhost, s, sizeof(cptr->local->sockhost));
}

void remove_dcc_references(aClient *sptr)
{
aClient *acptr;
Link *lp, *nextlp;
Link **lpp, *tmp;
int found;

	lp = sptr->user->dccallow;
	while(lp)
	{
		nextlp = lp->next;
		acptr = lp->value.cptr;
		for(found = 0, lpp = &(acptr->user->dccallow); *lpp; lpp=&((*lpp)->next))
		{
			if(lp->flags == (*lpp)->flags)
				continue; /* match only opposite types for sanity */
			if((*lpp)->value.cptr == sptr)
			{
				if((*lpp)->flags == DCC_LINK_ME)
				{
					sendto_one(acptr, ":%s %d %s :%s has been removed from "
						"your DCC allow list for signing off",
						me.name, RPL_DCCINFO, acptr->name, sptr->name);
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
				acptr->name, lp->flags, sptr->name);

		free_link(lp);
		lp = nextlp;
	}
}

/*
 * Recursively send QUITs and SQUITs for cptr and all of it's dependent
 * clients.  A server needs the client QUITs if it does not support NOQUIT.
 *    - kaniini
 */
static void
recurse_send_quits(aClient *cptr, aClient *sptr, aClient *from, aClient *to,
	const char *comment, const char *splitstr)
{
	aClient *acptr, *next;

	if (!MyConnect(to))
		return; /* We shouldn't even be called for non-remotes */

	if (!CHECKPROTO(to, PROTO_NOQUIT))
	{
		list_for_each_entry_safe(acptr, next, &client_list, client_node)
		{
			if (acptr->srvptr != sptr)
				continue;

			sendto_one(to, ":%s QUIT :%s", acptr->name, splitstr);
		}
	}

	list_for_each_entry_safe(acptr, next, &global_server_list, client_node)
	{
		if (acptr->srvptr != sptr)
			continue;

		recurse_send_quits(cptr, acptr, from, to, comment, splitstr);
	}

	if ((cptr == sptr && to != from) || !CHECKPROTO(to, PROTO_NOQUIT))
		sendto_one(to, "SQUIT %s :%s", sptr->name, comment);
}

/*
 * Remove all clients that depend on source_p; assumes all (S)QUITs have
 * already been sent.  we make sure to exit a server's dependent clients
 * and servers before the server itself; exit_one_client takes care of
 * actually removing things off llists.   tweaked from +CSr31  -orabidoo
 */
static void
recurse_remove_clients(aClient *sptr, const char *comment)
{
	aClient *acptr, *next;

	list_for_each_entry_safe(acptr, next, &client_list, client_node)
	{
		if (acptr->srvptr != sptr)
			continue;

		exit_one_client(acptr, comment);
	}

	list_for_each_entry_safe(acptr, next, &global_server_list, client_node)
	{
		if (acptr->srvptr != sptr)
			continue;

		recurse_remove_clients(acptr, comment);
		exit_one_client(acptr, comment);
	}
}

/*
** Remove *everything* that depends on source_p, from all lists, and sending
** all necessary QUITs and SQUITs.  source_p itself is still on the lists,
** and its SQUITs have been sent except for the upstream one  -orabidoo
*/
static void
remove_dependents(aClient *sptr, aClient *from, const char *comment, const char *splitstr)
{
	aClient *acptr;

	list_for_each_entry(acptr, &global_server_list, client_node)
		recurse_send_quits(sptr, sptr, from, acptr, comment, splitstr);

	recurse_remove_clients(sptr, splitstr);
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
/* DANGER: Ugly hack follows. */
/* Yeah :/ */
static void exit_one_client(aClient *sptr, const char *comment)
{
	Link *lp;
	Membership *mp;

	assert(!IsMe(sptr));

	if (IsClient(sptr))
	{
		sendto_common_channels(sptr, ":%s QUIT :%s",
		    sptr->name, comment);

		if (!MyClient(sptr))
		{
			RunHook2(HOOKTYPE_REMOTE_QUIT, sptr, comment);
		}

		/* This hook may or may not be redundant */
		RunHook(HOOKTYPE_EXIT_ONE_CLIENT, sptr);
		
		while ((mp = sptr->user->channel))
			remove_user_from_channel(sptr, mp->chptr);

		/* Clean up invitefield */
		while ((lp = sptr->user->invited))
			del_invite(sptr, lp->value.chptr);
		/* again, this is all that is needed */

		/* Clean up silencefield */
		while ((lp = sptr->user->silence))
			(void)del_silence(sptr, lp->value.cp);

		/* Clean up dccallow list and (if needed) notify other clients
		 * that have this person on DCCALLOW that the user just left/got removed.
		 */
		remove_dcc_references(sptr);

		/* For remote clients, we need to check for any outstanding async
		 * connects attached to this 'sptr', and set those records to NULL.
		 * Why not for local? Well, we already do that in close_connection ;)
		 */
		if (!MyConnect(sptr))
			unrealdns_delreq_bycptr(sptr);
	}

	/* Free module related data for this client */
	moddata_free_client(sptr);

	/* Remove sptr from the client list */
	if (*sptr->id)
		del_from_id_hash_table(sptr->id, sptr);
	if (*sptr->name)
		del_from_client_hash_table(sptr->name, sptr);
	if (IsRegisteredUser(sptr))
		hash_check_watch(sptr, RPL_LOGOFF);
	if (remote_rehash_client == sptr)
		remote_rehash_client = NULL; /* client did a /REHASH and QUIT before rehash was complete */
	remove_client_from_list(sptr);
}

/*
** exit_client
**	This is old "m_bye". Name  changed, because this is not a
**	protocol function, but a general server utility function.
**
**	This function exits a client of *any* type (user, server, etc)
**	from this server. Also, this generates all necessary prototol
**	messages that this exit may cause.
**
**   1) If the client is a local client, then this implicitly
**	exits all other clients depending on this connection (e.g.
**	remote clients having 'from'-field that points to this.
**
**   2) If the client is a remote client, then only this is exited.
**
** For convenience, this function returns a suitable value for
** m_funtion return value:
**
**	FLUSH_BUFFER	if (cptr == sptr)
**	0		if (cptr != sptr)
*/
int  exit_client(aClient *cptr, aClient *sptr, aClient *from, char *comment)
{
	time_t on_for;
	ConfigItem_listen *listen_conf;

	if (MyConnect(sptr))
	{
		if (sptr->local->class)
		{
			sptr->local->class->clients--;
			if ((sptr->local->class->flag.temporary) && !sptr->local->class->clients && !sptr->local->class->xrefcount)
			{
				delete_classblock(sptr->local->class);
				sptr->local->class = NULL;
			}
		}
		if (IsClient(sptr))
			IRCstats.me_clients--;
		if (sptr->serv && sptr->serv->conf)
		{
			sptr->serv->conf->refcount--;
			Debug((DEBUG_ERROR, "reference count for %s (%s) is now %d",
				sptr->name, sptr->serv->conf->servername, sptr->serv->conf->refcount));
			if (!sptr->serv->conf->refcount
			  && sptr->serv->conf->flag.temporary)
			{
				Debug((DEBUG_ERROR, "deleting temporary block %s", sptr->serv->conf->servername));
				delete_linkblock(sptr->serv->conf);
				sptr->serv->conf = NULL;
			}
		}
		if (IsServer(sptr))
		{
			IRCstats.me_servers--;
			ircd_log(LOG_SERVER, "SQUIT %s (%s)", sptr->name, comment);
		}
		free_pending_net(sptr);
		if (sptr->local->listener)
			if (sptr->local->listener && !IsOutgoing(sptr))
			{
				listen_conf = sptr->local->listener;
				listen_conf->clients--;
				if (listen_conf->flag.temporary && (listen_conf->clients == 0))
				{
					/* Call listen cleanup */
					listen_cleanup();
				}
			}
		sptr->flags |= FLAGS_CLOSING;
		if (IsPerson(sptr))
		{
			RunHook2(HOOKTYPE_LOCAL_QUIT, sptr, comment);
			sendto_connectnotice(sptr, 1, comment);
			/* Clean out list and watch structures -Donwulff */
			hash_del_watch_list(sptr);
			if (sptr->user && sptr->user->lopt)
			{
				free_str_list(sptr->user->lopt->yeslist);
				free_str_list(sptr->user->lopt->nolist);
				MyFree(sptr->user->lopt);
			}
			on_for = TStime() - sptr->local->firsttime;
			if (IsHidden(sptr))
				ircd_log(LOG_CLIENT, "Disconnect - (%ld:%ld:%ld) %s!%s@%s [VHOST %s] (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					sptr->name, sptr->user->username,
					sptr->user->realhost, sptr->user->virthost, comment);
			else
				ircd_log(LOG_CLIENT, "Disconnect - (%ld:%ld:%ld) %s!%s@%s (%s)",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					sptr->name, sptr->user->username, sptr->user->realhost, comment);
		} else
		if (IsUnknown(sptr))
		{
			RunHook2(HOOKTYPE_UNKUSER_QUIT, sptr, comment);
		}

		if (sptr->fd >= 0 && !IsConnecting(sptr))
		{
			if (cptr != NULL && sptr != cptr)
				sendto_one(sptr,
				    "ERROR :Closing Link: %s %s (%s)",
				    get_client_name(sptr, FALSE), cptr->name,
				    comment);
			else
				sendto_one(sptr, "ERROR :Closing Link: %s (%s)",
				    get_client_name(sptr, FALSE), comment);
		}
		/*
		   ** Currently only server connections can have
		   ** depending remote clients here, but it does no
		   ** harm to check for all local clients. In
		   ** future some other clients than servers might
		   ** have remotes too...
		   **
		   ** Close the Client connection first and mark it
		   ** so that no messages are attempted to send to it.
		   ** (The following *must* make MyConnect(sptr) == FALSE!).
		   ** It also makes sptr->from == NULL, thus it's unnecessary
		   ** to test whether "sptr != acptr" in the following loops.
		 */
		close_connection(sptr);
	}
	else if (IsPerson(sptr) && !IsULine(sptr))
	{
		if (sptr->srvptr != &me)
			sendto_fconnectnotice(sptr, 1, comment);
	}

	/*
	 * Recurse down the client list and get rid of clients who are no
	 * longer connected to the network (from my point of view)
	 * Only do this expensive stuff if exited==server -Donwulff
	 */
	if (IsServer(sptr))
	{
		char splitstr[HOSTLEN + HOSTLEN + 2];

		assert(sptr->serv != NULL && sptr->srvptr != NULL);

		if (FLAT_MAP)
			strlcpy(splitstr, "*.net *.split", sizeof splitstr);
		else
			ircsnprintf(splitstr, sizeof splitstr, "%s %s", sptr->srvptr->name, sptr->name);

		remove_dependents(sptr, cptr, comment, splitstr);

		RunHook(HOOKTYPE_SERVER_QUIT, sptr);
	}
	else if (IsClient(sptr) && !(sptr->flags & FLAGS_KILLED))
	{
		sendto_server(cptr, PROTO_SID, 0,
			":%s QUIT :%s", ID(sptr), comment);
		sendto_server(cptr, 0, PROTO_SID,
			":%s QUIT :%s", sptr->name, comment);
	}

	/*
	 * Finally, clear out the server we lost itself
	 */
	exit_one_client(sptr, comment);
	return cptr == sptr ? FLUSH_BUFFER : 0;
}

void checklist(void)
{
	if (!(bootopt & BOOT_AUTODIE))
		return;

	if (!list_empty(&lclient_list))
		exit (0);

	return;
}

void initstats(void)
{
	bzero((char *)&ircst, sizeof(ircst));
}

void verify_opercount(aClient *orig, char *tag)
{
int counted = 0;
aClient *acptr;
char text[2048];

	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (IsOper(acptr) && !IsHideOper(acptr))
			counted++;
	}
	if (counted == IRCstats.operators)
		return;
	snprintf(text, sizeof(text), "[BUG] operator count bug! value in /lusers is '%d', we counted '%d', "
	               "user='%s', userserver='%s', tag=%s. Corrected. ",
	               IRCstats.operators, counted, orig->name,
	               orig->srvptr ? orig->srvptr->name : "<null>", tag ? tag : "<null>");
#ifdef DEBUGMODE
	sendto_realops("%s", text);
#endif
	ircd_log(LOG_ERROR, "%s", text);
	IRCstats.operators = counted;
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
		if (!isalnum(*p) && (*p != '_') && (*p != '-') && (*p != '.') && (*p != ':'))
			return 0;

	return 1;
}

/** Checks if the specified regex (or fast badwords) is valid.
 * returns NULL in case of success [!],
 * pointer to buffer with error message otherwise
 * if check_broadness is 1, the function will attempt to determine
 * if the given regex string is too broad (i.e. matches everything)
 */
char *unreal_checkregex(char *s, int fastsupport, int check_broadness)
{
int errorcode, errorbufsize, regex=0;
char *errtmp, *tmp;
static char errorbuf[512];
regex_t expr;

	if (fastsupport)
	{
		for (tmp = s; *tmp; tmp++) {
			if (!isalnum(*tmp) && !(*tmp >= 128)) {
				if ((s == tmp) && (*tmp == '*'))
					continue;
				if ((*(tmp + 1) == '\0') && (*tmp == '*'))
					continue;
				regex = 1;
				break;
			}
		}
	}
	if (!fastsupport || regex)
	{
		errorcode = regcomp(&expr, s, REG_ICASE|REG_EXTENDED);
		if (errorcode > 0)
		{
			errorbufsize = regerror(errorcode, &expr, NULL, 0)+1;
			errtmp = MyMalloc(errorbufsize);
			regerror(errorcode, &expr, errtmp, errorbufsize);
			strlcpy(errorbuf, errtmp, sizeof(errorbuf));
			free(errtmp);
			regfree(&expr);
			return errorbuf;
		}
		if (check_broadness && !regexec(&expr, "", 0, NULL, 0))
		{
			strlcpy(errorbuf, "Regular expression is too broad", sizeof(errorbuf));
			regfree(&expr);
			return errorbuf;
		}
		regfree(&expr);
	}
	return NULL;
}

/*|| BAN ACTION ROUTINES FOLLOW ||*/

/** Converts a banaction string (eg: "kill") to an integer value (eg: BAN_ACT_KILL) */
int banact_stringtoval(char *s)
{
BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (!strcasecmp(s, b->name))
			return b->value;
	return 0;
}

/** Converts a banaction character (eg: 'K') to an integer value (eg: BAN_ACT_KILL) */
int banact_chartoval(char c)
{
BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->character == c)
			return b->value;
	return 0;
}

/** Converts a banaction value (eg: BAN_ACT_KILL) to a character value (eg: 'k') */
char banact_valtochar(int val)
{
BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->value == val)
			return b->character;
	return '\0';
}

/** Converts a banaction value (eg: BAN_ACT_KLINE) to a string (eg: "kline") */
char *banact_valtostring(int val)
{
BanActTable *b;

	for (b = &banacttable[0]; b->value; b++)
		if (b->value == val)
			return b->name;
	return "UNKNOWN";
}

/*|| BAN TARGET ROUTINES FOLLOW ||*/

/** Extract target flags from string 's'. */
int spamfilter_gettargets(char *s, aClient *sptr)
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
		if (!e->value && sptr)
		{
			sendnotice(sptr, "Unknown target type '%c'", *s);
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

/** This is basically only used internally by dospamfilter()... */
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

char *getcloak(aClient *sptr)
{
	if (!*sptr->user->cloakedhost)
	{
		/* need to calculate (first-time) */
		make_virthost(sptr, sptr->user->realhost, sptr->user->cloakedhost, 0);
	}

	return sptr->user->cloakedhost;
}

int mixed_network(void)
{
	aClient *acptr;
	
	list_for_each_entry(acptr, &server_list, special_node)
	{
		if (!IsServer(acptr) || IsULine(acptr))
			continue; /* skip u-lined servers (=non-unreal, unless you configure your ulines badly, that is) */
		if (SupportTKLEXT(acptr) && !SupportTKLEXT2(acptr))
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

		safefree(m->mask);

		MyFree(m);
	}
}

/** Internal function to add one individual mask to the list */
static void unreal_add_mask(ConfigItem_mask **head, ConfigEntry *ce)
{
	ConfigItem_mask *m = MyMallocEx(sizeof(ConfigItem_mask));

	/* Since we allow both mask "xyz"; and mask { abc; def; };... */
	if (!strcmp(ce->ce_varname, "mask") && ce->ce_vardata)
		safestrdup(m->mask, ce->ce_vardata);
	else
		safestrdup(m->mask, ce->ce_varname);
	
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
int unreal_mask_match(aClient *acptr, ConfigItem_mask *m)
{
	for (; m; m = m->next)
	{
		if (match_user(m->mask, acptr, MATCH_CHECK_REAL))
			return 1;
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

int swhois_add(aClient *acptr, char *tag, int priority, char *swhois, aClient *from, aClient *skip)
{
	SWhois *s;

	/* Make sure the line isn't added yet. If so, then bail out silently. */
	for (s = acptr->user->swhois; s; s = s->next)
		if (!strcmp(s->line, swhois))
			return -1; /* exists */

	s = MyMallocEx(sizeof(SWhois));
	s->line = strdup(swhois);
	s->setby = strdup(tag);
	s->priority = priority;
	AddListItemPrio(s, acptr->user->swhois, s->priority);
	
	sendto_server(skip, 0, PROTO_EXTSWHOIS, ":%s SWHOIS %s :%s",
		from->name, acptr->name, swhois);

	sendto_server(skip, PROTO_EXTSWHOIS, 0, ":%s SWHOIS %s + %s %d :%s",
		from->name, acptr->name, tag, priority, swhois);

	return 0;
}

/** Delete swhois title(s)
 * Delete swhois by tag and swhois. Then broadcast this change to all other servers.
 * Remark: if you use swhois "*" then it will remove all swhois titles for that tag
 */
int swhois_delete(aClient *acptr, char *tag, char *swhois, aClient *from, aClient *skip)
{
	SWhois *s, *s_next;
	int ret = -1; /* default to 'not found' */
	
	for (s = acptr->user->swhois; s; s = s_next)
	{
		s_next = s->next;
		
		/* If ( same swhois or "*" ) AND same tag */
		if ( ((!strcmp(s->line, swhois) || !strcmp(swhois, "*")) &&
		    !strcmp(s->setby, tag)))
		{
			DelListItem(s, acptr->user->swhois);
			MyFree(s->line);
			MyFree(s->setby);
			MyFree(s);

			sendto_server(skip, 0, PROTO_EXTSWHOIS, ":%s SWHOIS %s :",
				from->name, acptr->name);

			sendto_server(skip, PROTO_EXTSWHOIS, 0, ":%s SWHOIS %s - %s %d :%s",
				from->name, acptr->name, tag, 0, swhois);
			
			ret = 0;
		}
	}

	return ret;
}
