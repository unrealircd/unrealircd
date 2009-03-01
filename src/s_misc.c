
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

#ifndef CLEAN_COMPILE
static char sccsid[] =
    "@(#)s_misc.c	2.42 3/1/94 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#ifndef _WIN32
#include <sys/time.h>
#endif
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include <sys/stat.h>
#include <fcntl.h>
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

#ifndef NO_FDLIST
extern fdlist serv_fdlist;
extern fdlist oper_fdlist;
extern float currentrate;
extern float currentrate2;
#endif
extern ircstats IRCstats;
extern char	*me_hash;

static void exit_one_client(aClient *, aClient *, aClient *, char *, int);
extern void exit_one_client_in_split(aClient *, aClient *, aClient *,
    char *);

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
	(void)ircsprintf(buf, "%s %s %d %d -- %02d:%02d %c%02d:%02d",
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
	ircsprintf(buffer, "%ludays %luhours %luminutes %lusecs",
days, hours, minutes, seconds);
	return(*buffer ? buffer : "");
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
	strncpyzt(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strncpyzt(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
}


/*
 * create a string of form "foo!bar@fubar" given foo, bar and fubar
 * as the parameters.  If NULL, they become "*".
 */
inline char *make_nick_user_host_r(char *namebuf, char *nick, char *name, char *host)
{
	char *s = namebuf;

	bzero(namebuf, sizeof(namebuf));
	nick = check_string(nick);
	strncpyzt(namebuf, nick, NICKLEN + 1);
	s += strlen(s);
	*s++ = '!';
	name = check_string(name);
	strncpyzt(s, name, USERLEN + 1);
	s += strlen(s);
	*s++ = '@';
	host = check_string(host);
	strncpyzt(s, host, HOSTLEN + 1);
	s += strlen(s);
	*s = '\0';
	return (namebuf);
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
**	or sptr->sockhost gets changed into pointers instead of
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
			(void)ircsprintf(nbuf, "%s[%s@%s.%u]",
			    sptr->name,
			    (!(sptr->flags & FLAGS_GOTID)) ? "" :
			    sptr->username,
#ifdef INET6
			    inetntop(AF_INET6,
			    (char *)&sptr->ip, mydummy, MYDUMMY_SIZE),
#else
			    inetntoa((char *)&sptr->ip),
#endif
			    (unsigned int)sptr->port);
		else
		{
			if (mycmp(sptr->name, sptr->sockhost))
				(void)ircsprintf(nbuf, "%s[%s]",
				    sptr->name, sptr->sockhost);
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
	if (!cptr->hostp)
		return get_client_name(cptr, FALSE);
	(void)ircsprintf(nbuf, "%s[%-.*s@%-.*s]",
	    cptr->name, USERLEN,
  	    (!(cptr->flags & FLAGS_GOTID)) ? "" : cptr->username,
	    HOSTLEN, cptr->hostp->h_name);
	return nbuf;
}

/*
 * Form sockhost such that if the host is of form user@host, only the host
 * portion is copied.
 */
void get_sockhost(aClient *cptr, char *host)
{
	char *s;
	if ((s = (char *)index(host, '@')))
		s++;
	else
		s = host;
	strncpyzt(cptr->sockhost, s, sizeof(cptr->sockhost));
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
	aClient *acptr;
	aClient *next;
	time_t on_for;
	ConfigItem_listen *listen_conf;
	static char comment1[HOSTLEN + HOSTLEN + 2];
	static int recurse = 0;

	if (MyConnect(sptr))
	{
#ifndef NO_FDLIST
#define FDLIST_DEBUG
#ifdef FDLIST_DEBUG
		{
			int i;
			
			if (!IsAnOper(sptr))
			{
				for (i = oper_fdlist.last_entry; i; i--)
				{
					if (oper_fdlist.entry[i] == sptr->slot)
					{
						sendto_realops("[BUG] exit_client: oper_fdlist entry while not oper, fd=%d, user='%s'",
							sptr->slot, sptr->name);
						ircd_log(LOG_ERROR, "[BUG] exit_client: oper_fdlist entry while not oper, fd=%d, user='%s'",
							sptr->slot, sptr->name);
						delfrom_fdlist(sptr->slot, &oper_fdlist); /* be kind of enough to fix the problem.. */
						break; /* MUST break here */
					}
				}
			}
		}
#endif
		if (IsAnOper(sptr))
			delfrom_fdlist(sptr->slot, &oper_fdlist);
		if (IsServer(sptr))
			delfrom_fdlist(sptr->slot, &serv_fdlist);
#endif
		if (sptr->class)
		{
			sptr->class->clients--;
			if ((sptr->class->flag.temporary) && !sptr->class->clients && !sptr->class->xrefcount)
			{
				delete_classblock(sptr->class);
				sptr->class = NULL;
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

		if (sptr->listener)
			if (sptr->listener->class && !IsOutgoing(sptr))
			{
				listen_conf = (ConfigItem_listen *) sptr->listener->class;
				listen_conf->clients--;
				if (listen_conf->flag.temporary
				    && (listen_conf->clients == 0))
				{
					/* Call listen cleanup */
					listen_cleanup();
				}
			}
		sptr->flags |= FLAGS_CLOSING;
		if (IsPerson(sptr))
		{
			RunHook2(HOOKTYPE_LOCAL_QUIT, sptr, comment);
			sendto_connectnotice(sptr->name, sptr->user, sptr, 1, comment);
			/* Clean out list and watch structures -Donwulff */
			hash_del_watch_list(sptr);
			if (sptr->user && sptr->user->lopt)
			{
				free_str_list(sptr->user->lopt->yeslist);
				free_str_list(sptr->user->lopt->nolist);
				MyFree(sptr->user->lopt);
			}
			on_for = TStime() - sptr->firsttime;
			if (IsHidden(sptr))
				ircd_log(LOG_CLIENT, "Disconnect - (%ld:%ld:%ld) %s!%s@%s [VHOST %s]",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					sptr->name, sptr->user->username,
					sptr->user->realhost, sptr->user->virthost);
			else
				ircd_log(LOG_CLIENT, "Disconnect - (%ld:%ld:%ld) %s!%s@%s",
					on_for / 3600, (on_for % 3600) / 60, on_for % 60,
					sptr->name, sptr->user->username, sptr->user->realhost);
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

	/*
	 * Recurse down the client list and get rid of clients who are no
	 * longer connected to the network (from my point of view)
	 * Only do this expensive stuff if exited==server -Donwulff
	 */

	if (IsServer(sptr))
	{
		/*
		 * Is this right? Not recreateing the split message if
		 * we have been called recursivly? I hope so, cuz thats
		 * the only way I could make this give the right servers
		 * in the quit msg. -Cabal95
		 */
		if (cptr && !recurse)
		{
			/*
			 * We are sure as we RELY on sptr->srvptr->name and 
			 * sptr->name to be less or equal to HOSTLEN
			 * Waste of strlcpy/strlcat here
			*/
			(void)strcpy(comment1, sptr->srvptr->name);
			(void)strcat(comment1, " ");
			(void)strcat(comment1, sptr->name);
		}
		/*
		 * First, remove the clients on the server itself.
		 */
		for (acptr = client; acptr; acptr = next)
		{
			next = acptr->next;
			if (IsClient(acptr) && (acptr->srvptr == sptr))
				exit_one_client(NULL, acptr,
				    &me, comment1, 1);
		}

		/*
		 * Now, go SQUIT off the servers which are down-stream of
		 * the one we just lost.
		 */
		recurse++;
		for (acptr = client; acptr; acptr = next)
		{
			next = acptr->next;
			if (IsServer(acptr) && acptr->srvptr == sptr)
				exit_client(sptr, acptr, sptr, comment1); /* RECURSION */
			/*
			 * I am not masking SQUITS like I do QUITs.  This
			 * is probobly something we could easily do, but
			 * how much savings is there really in something
			 * like that?
			 */
#ifdef DEBUGMODE
			else if (IsServer(acptr) &&
			    (find_server(acptr->serv->up, NULL) == sptr))
			{
				sendto_ops("WARNING, srvptr!=sptr but "
				    "find_server did!  Server %s on "
				    "%s thought it was on %s while "
				    "losing %s.  Tell coding team.",
				    acptr->name, acptr->serv->up,
				    acptr->srvptr ? acptr->
				    srvptr->name : "<noserver>", sptr->name);
				exit_client(sptr, acptr, sptr, comment1);
			}
#endif
		}
		recurse--;
		RunHook(HOOKTYPE_SERVER_QUIT, sptr);
	}


	/*
	 * Finally, clear out the server we lost itself
	 */
	exit_one_client(cptr, sptr, from, comment, recurse);
	return cptr == sptr ? FLUSH_BUFFER : 0;
}

/*
** Exit one client, local or remote. Assuming all dependants have
** been already removed, and socket closed for local client.
*/
/* DANGER: Ugly hack follows. */
/* Yeah :/ */
static void exit_one_client(aClient *cptr, aClient *sptr, aClient *from, char *comment, int split)
{
	aClient *acptr;
	int  i;
	Link *lp;
	Membership *mp;
	/*
	   **  For a server or user quitting, propagage the information to
	   **  other servers (except to the one where is came from (cptr))
	 */
	if (IsMe(sptr))
	{
		sendto_ops("ERROR: tried to exit me! : %s", comment);
		return;		/* ...must *never* exit self!! */
	}
	else if (IsServer(sptr))
	{
		/*
		   ** Old sendto_serv_but_one() call removed because we now
		   ** need to send different names to different servers
		   ** (domain name matching)
		 */
		for (i = 0; i <= LastSlot; i++)
		{

			if (!(acptr = local[i]) || !IsServer(acptr) || acptr == cptr || IsMe(acptr)
			    || (DontSendQuit(acptr) && split))
				continue;
			/*
			   ** SQUIT going "upstream". This is the remote
			   ** squit still hunting for the target. Use prefixed
			   ** form. "from" will be either the oper that issued
			   ** the squit or some server along the path that
			   ** didn't have this fix installed. --msa
			 */
			if (sptr->from == acptr)
			{
				sendto_one(acptr, ":%s SQUIT %s :%s", from->name, sptr->name, comment);
			}
			else
			{
				sendto_one(acptr, "SQUIT %s :%s", sptr->name, comment);
			}
		}
	}
	else if (!(IsPerson(sptr)))
		/* ...this test is *dubious*, would need
		   ** some thougth.. but for now it plugs a
		   ** nasty hole in the server... --msa
		 */
		;		/* Nothing */
	else if (sptr->name[0])	/* ...just clean all others with QUIT... */
	{
		/*
		   ** If this exit is generated from "m_kill", then there
		   ** is no sense in sending the QUIT--KILL's have been
		   ** sent instead.
		 */
		if ((sptr->flags & FLAGS_KILLED) == 0)
		{
			if (split == 0)
				sendto_serv_butone_token
				    (cptr, sptr->name, MSG_QUIT, TOK_QUIT,
				    ":%s", comment);
			else
				/*
				 * Then this is a split, only old (stupid)
				 * clients need to get quit messages
				 */
				sendto_serv_butone_quit(cptr, ":%s QUIT :%s",
				    sptr->name, comment);
		}
		/*
		   ** If a person is on a channel, send a QUIT notice
		   ** to every client (person) on the same channel (so
		   ** that the client can show the "**signoff" message).
		   ** (Note: The notice is to the local clients *only*)
		 */
		if (sptr->user)
		{
			sendto_common_channels(sptr, ":%s QUIT :%s",
			    sptr->name, comment);

			if (!IsULine(sptr) && !split)
				if (sptr->user->server != me_hash)
					sendto_fconnectnotice(sptr->name, sptr->user, sptr, 1, comment);
			if (!MyClient(sptr))
			{
				RunHook2(HOOKTYPE_REMOTE_QUIT, sptr, comment);
			}
#ifdef JOINTHROTTLE
			cmodej_deluserentries(sptr);
#endif
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
	}

	/* Remove sptr from the client list */
	if (del_from_client_hash_table(sptr->name, sptr) != 1)
		Debug((DEBUG_ERROR, "%#x !in tab %s[%s] %#x %#x %#x %d %d %#x",
		    sptr, sptr->name,
		    sptr->from ? sptr->from->sockhost : "??host",
		    sptr->from, sptr->next, sptr->prev, sptr->fd,
		    sptr->status, sptr->user));
	if (IsRegisteredUser(sptr))
		hash_check_watch(sptr, RPL_LOGOFF);
	remove_client_from_list(sptr);
	return;
}


void checklist(void)
{
	aClient *acptr;
	int  i, j;

	if (!(bootopt & BOOT_AUTODIE))
		return;
	for (j = i = 0; i <= LastSlot; i++)
		if (!(acptr = local[i]))
			continue;
		else if (IsClient(acptr))
			j++;
	if (!j)
	{
		exit(0);
	}
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

	for (acptr = client; acptr; acptr = acptr->next)
	{
		if (IsOper(acptr) && !IsHideOper(acptr))
			counted++;
	}
	if (counted == IRCstats.operators)
		return;
	sprintf(text, "[BUG] operator count bug! value in /lusers is '%d', we counted '%d', "
	               "user='%s', userserver='%s', tag=%s. Corrected. ",
	               IRCstats.operators, counted, orig->name ? orig->name : "<null>",
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

	if (!fastsupport)
		goto Ilovegotos;

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
	if (regex)
	{
Ilovegotos:
		errorcode = regcomp(&expr, s, REG_ICASE|REG_EXTENDED);
		if (errorcode > 0)
		{
			errorbufsize = regerror(errorcode, &expr, NULL, 0)+1;
			errtmp = MyMalloc(errorbufsize);
			regerror(errorcode, &expr, errtmp, errorbufsize);
			strncpyzt(errorbuf, errtmp, sizeof(errorbuf));
			free(errtmp);
			regfree(&expr);
			return errorbuf;
		}
		if (check_broadness && !regexec(&expr, "", 0, NULL, 0))
		{
			strncpyzt(errorbuf, "Regular expression is too broad", sizeof(errorbuf));
			regfree(&expr);
			return errorbuf;
		}
		regfree(&expr);
	}
	return NULL;
}



#define SPF_REGEX_FLAGS (REG_ICASE|REG_EXTENDED|REG_NOSUB)

/** Allocates a new Spamfilter entry and compiles/fills in the info.
 * NOTE: originally I wanted to integrate both badwords and spamfilter
 * into one function, but that was quickly getting ugly :(.
 */
Spamfilter *unreal_buildspamfilter(char *s)
{
Spamfilter *e = MyMallocEx(sizeof(Spamfilter));

	regcomp(&e->expr, s, SPF_REGEX_FLAGS);
	return e;
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

/** Kicks all insecure users on a +z channel */
void kick_insecure_users(aChannel *chptr)
{
	Member *member, *mb2;
	aClient *cptr;
	char *comment = "Insecure user not allowed on secure channel (+z)";
	
	for (member = chptr->members; member; member = mb2)
	{
		mb2 = member->next;
		cptr = member->cptr;
		if (MyClient(cptr) && !IsSecureConnect(cptr) && !IsULine(cptr))
		{
			RunHook5(HOOKTYPE_LOCAL_KICK, &me, &me, cptr, chptr, comment);

			if ((chptr->mode.mode & MODE_AUDITORIUM) && is_chanownprotop(cptr, chptr))
			{
				sendto_chanops_butone(cptr, chptr, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
				sendto_prefix_one(cptr, &me, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
			} 
			else
			{
				sendto_channel_butserv(chptr, &me, ":%s KICK %s %s :%s", me.name, chptr->chname, cptr->name, comment);
			}

			sendto_serv_butone_token(&me, me.name, MSG_KICK, TOK_KICK, "%s %s :%s", chptr->chname, cptr->name, comment);

			remove_user_from_channel(cptr, chptr);
		}
	}
}
