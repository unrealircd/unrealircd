/*
/*
 *   Unreal Internet Relay Chat Daemon, src/s_conf.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Computing Center
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

/* Changed all calls of check_pings so that only when a kline-related command
   is used will a kline check occur -- Barubary */

#define KLINE_RET_AKILL 3
#define KLINE_RET_PERM 2
#define KLINE_RET_DELOK 1
#define KLINE_DEL_ERR 0


#ifndef lint
static char sccsid[] =
    "@(#)s_conf.c	2.56 02 Apr 1994 (C) 1988 University of Oulu, \
Computing Center and Jarkko Oikarinen";
#endif

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <string.h>

ID_Notes("O:line flags in here");
#include "h.h"
#define IN6ADDRSZ (sizeof(struct IN_ADDR))
static int lookup_confhost PROTO((aConfItem *));
static int is_comment PROTO((char *));
static int advanced_check(char *, int);

aSqlineItem *sqline = NULL;
aConfItem *conf = NULL;
extern char zlinebuf[];
extern ircstats IRCstats;




/*
 * remove all conf entries from the client except those which match
 * the status field mask.
 */
void det_confs_butmask(cptr, mask)
	aClient *cptr;
	int  mask;
{
	Link *tmp, *tmp2;

	for (tmp = cptr->confs; tmp; tmp = tmp2)
	{
		tmp2 = tmp->next;
		if ((tmp->value.aconf->status & mask) == 0)
			(void)detach_conf(cptr, tmp->value.aconf);
	}
}

/*
 * Add a temporary line to the configuration
 */
void add_temp_conf(status, host, passwd, name, port, class, temp)
	unsigned int status;
	char *host;
	char *passwd;
	char *name;
	int  port, class, temp;	/* temp: 0 = perm 1 = temp 2 = akill */
{
	aConfItem *aconf;

	aconf = make_conf();

	aconf->tmpconf = temp;
	aconf->status = status;
	if (host)
		DupString(aconf->host, host);
	if (passwd)
		DupString(aconf->passwd, passwd);
	if (name)
		DupString(aconf->name, name);
	aconf->port = port;
	if (class)
		Class(aconf) = find_class(class);
	if (!find_temp_conf_entry(aconf, status))
	{
		aconf->next = conf;
		conf = aconf;
		aconf = NULL;
	}

	if (aconf)
		free_conf(aconf);
}

/*
 * delete a temporary conf line.  *only* temporary conf lines may be deleted.
 */
int  del_temp_conf(status, host, passwd, name, port, class, akill)
	unsigned int status, akill;
	char *host;
	char *passwd;
	char *name;
	int  port, class;
{
	aConfItem *aconf;
	aConfItem *bconf;
	u_int mask;
	u_int result = KLINE_DEL_ERR;

	aconf = make_conf();

	aconf->status = status;
	if (host)
		DupString(aconf->host, host);
	if (passwd)
		DupString(aconf->passwd, passwd);
	if (name)
		DupString(aconf->name, name);
	aconf->port = port;
	if (class)
		Class(aconf) = find_class(class);
	mask = status;
	if (bconf = find_temp_conf_entry(aconf, mask))	/* only if non-null ptr */
	{
/* Completely skirt the akill error messages if akill is set to 1
 * this allows RAKILL to do its thing without having to go through the
 * error checkers.  If it had to it would go kaplooey. --Russell
 */
		if (bconf->tmpconf == KLINE_PERM && (akill != 3))
			result = KLINE_RET_PERM;	/* Kline permanent */
		else if (!akill && (bconf->tmpconf == KLINE_AKILL))
			result = KLINE_RET_AKILL;	/* Akill */
		else if (akill && (bconf->tmpconf != KLINE_AKILL))
			result = KLINE_RET_PERM;
		else
		{
			bconf->status |= CONF_ILLEGAL;	/* just mark illegal */
			result = KLINE_RET_DELOK;	/* same as deletion */
		}

	}
	if (aconf)
		free_conf(aconf);
	return result;		/* if it gets to here, it doesn't exist */
}

/*
 * find the first (best) I line to attach.
 */
int  attach_Iline(cptr, hp, sockhost)
	aClient *cptr;
	struct hostent *hp;
	char *sockhost;
{
	aConfItem *aconf;
	char *hname;
	int  i;
	static char uhost[HOSTLEN + USERLEN + 3];
	static char fullname[HOSTLEN + 1];

	for (aconf = conf; aconf; aconf = aconf->next)
	{
		if (aconf->status != CONF_CLIENT)
			continue;
		if (aconf->port && aconf->port != cptr->acpt->port)
			continue;
		if (!aconf->host || !aconf->name)
			goto attach_iline;
		if (hp)
			for (i = 0, hname = hp->h_name; hname;
			    hname = hp->h_aliases[i++])
			{
				(void)strncpy(fullname, hname,
				    sizeof(fullname) - 1);
				add_local_domain(fullname,
				    HOSTLEN - strlen(fullname));
				Debug((DEBUG_DNS, "a_il: %s->%s",
				    sockhost, fullname));
				if (index(aconf->name, '@'))
				{
					(void)strcpy(uhost, cptr->username);
					(void)strcat(uhost, "@");
				}
				else
					*uhost = '\0';
				(void)strncat(uhost, fullname,
				    sizeof(uhost) - strlen(uhost));
				if (!match(aconf->name, uhost))
					goto attach_iline;
			}

		if (index(aconf->host, '@'))
		{
			strncpyzt(uhost, cptr->username, sizeof(uhost));
			(void)strcat(uhost, "@");
		}
		else
			*uhost = '\0';
		(void)strncat(uhost, sockhost, sizeof(uhost) - strlen(uhost));
		if (!match(aconf->host, uhost))
			goto attach_iline;
		continue;
	      attach_iline:
		if (index(uhost, '@'))
			cptr->flags |= FLAGS_DOID;
		get_sockhost(cptr, uhost);

		if (aconf->passwd && !strcmp(aconf->passwd, "ONE"))
		{
			for (i = highest_fd; i >= 0; i--)
				if (local[i] && MyClient(local[i]) &&
				    local[i]->ip.S_ADDR == cptr->ip.S_ADDR)
					return -1;	/* Already got one with that ip# */
		}

		return attach_conf(cptr, aconf);
	}
	return -1;
}

/*
 * Find the single N line and return pointer to it (from list).
 * If more than one then return NULL pointer.
 */
aConfItem *count_cnlines(lp)
	Link *lp;
{
	aConfItem *aconf, *cline = NULL, *nline = NULL;

	for (; lp; lp = lp->next)
	{
		aconf = lp->value.aconf;
		if (!(aconf->status & CONF_SERVER_MASK))
			continue;
		if (aconf->status == CONF_CONNECT_SERVER && !cline)
			cline = aconf;
		else if (aconf->status == CONF_NOCONNECT_SERVER && !nline)
			nline = aconf;
	}
	return nline;
}

/*
** detach_conf
**	Disassociate configuration from the client.
**      Also removes a class from the list if marked for deleting.
*/
int  detach_conf(cptr, aconf)
	aClient *cptr;
	aConfItem *aconf;
{
	Link **lp, *tmp;

	lp = &(cptr->confs);

	while (*lp)
	{
		if ((*lp)->value.aconf == aconf)
		{
			if ((aconf) && (Class(aconf)))
			{
				if (aconf->status & CONF_CLIENT_MASK)
					if (ConfLinks(aconf) > 0)
						--ConfLinks(aconf);
				if (ConfMaxLinks(aconf) == -1 &&
				    ConfLinks(aconf) == 0)
				{
					free_class(Class(aconf));
					Class(aconf) = NULL;
				}
			}
			if (aconf && !--aconf->clients && IsIllegal(aconf))
				free_conf(aconf);
			tmp = *lp;
			*lp = tmp->next;
			free_link(tmp);
			return 0;
		}
		else
			lp = &((*lp)->next);
	}
	return -1;
}

static int is_attached(aconf, cptr)
	aConfItem *aconf;
	aClient *cptr;
{
	Link *lp;

	for (lp = cptr->confs; lp; lp = lp->next)
		if (lp->value.aconf == aconf)
			break;

	return (lp) ? 1 : 0;
}

/*
** attach_conf
**	Associate a specific configuration entry to a *local*
**	client (this is the one which used in accepting the
**	connection). Note, that this automaticly changes the
**	attachment if there was an old one...
*/
int  attach_conf(cptr, aconf)
	aConfItem *aconf;
	aClient *cptr;
{
	Link *lp;

	if (is_attached(aconf, cptr))
		return 1;
	if (IsIllegal(aconf))
		return -1;
	if ((aconf->status & (CONF_LOCOP | CONF_OPERATOR | CONF_CLIENT)) &&
	    aconf->clients >= ConfMaxLinks(aconf) && ConfMaxLinks(aconf) > 0)
		return -3;	/* Use this for printing error message */
	lp = make_link();
	lp->next = cptr->confs;
	lp->value.aconf = aconf;
	cptr->confs = lp;
	aconf->clients++;
	if (aconf->status & CONF_CLIENT_MASK)
		ConfLinks(aconf)++;
	return 0;
}


aConfItem *find_tline(char *host)
{
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_TLINE)
			if (!match(aconf->host, host))
			{
				return (aconf);
			}
	return (NULL);
}

int  find_nline(aClient *cptr)
{
	aConfItem *aconf, *aconf2;

	/* Only check for an E:line if an n:line was found */

		for (aconf = conf; aconf; aconf = aconf->next)
	{
		if (aconf->status & CONF_NLINE
		&& (match(aconf->host, cptr->info) == 0)) {
			for (aconf2 = conf; aconf2; aconf2 = aconf2->next)
		if ((aconf2->status == CONF_EXCEPT) &&
		    aconf2->host && aconf2->name
		    && (match(aconf2->host, cptr->sockhost) == 0)
		    && (!cptr->user->username
		    || match(aconf2->name, cptr->user->username) == 0))
			return 0;
			break;
	}
		}
	if (aconf)
	{
		if (BadPtr(aconf->passwd))
			sendto_one(cptr,
			    ":%s %d %s :*** Your GECOS (real name) is not allowed on this server."
			    "Please change it and reconnect.", me.name,
			    ERR_YOUREBANNEDCREEP, cptr->name);
		else
			sendto_one(cptr,
			    ":%s %d %s :*** Your GECOS (real name) is not allowed on this server:  %s "
			    "Please change it and reconnect.", me.name,
			    ERR_YOUREBANNEDCREEP, cptr->name, aconf->passwd);
	}
	return (aconf ? -1 : 0);
}

aConfItem *find_socksexcept(char *host)
{
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_SOCKSEXCEPT)
			if (!match(aconf->host, host))
			{
				return (aconf);
			}
	return (NULL);
}

aConfItem *find_admin()
{
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ADMIN)
			break;
	return (aconf);
}

/* Find a DR_PASS line for the /DIE or /RESTART command
 * Instead of returning the whole structure we return a
 * char* which is the pass. 
 * Added December 28 1997 -- NikB 
 */
char *find_diepass()
{
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_DRPASS)
			return (aconf->host);

	return NULL;		/* Return NULL (We did not find any) */
}

char *find_restartpass()
{
	aConfItem *aconf;

	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_DRPASS)
			return (aconf->passwd);

	return NULL;		/* Return NULL (We did not find any) */
}

aConfItem *find_me()
{
	aConfItem *aconf;
	for (aconf = conf; aconf; aconf = aconf->next)
		if (aconf->status & CONF_ME)
			break;

	return (aconf);
}

/*
 * attach_confs
 *  Attach a CONF line to a client if the name passed matches that for
 * the conf file (for non-C/N lines) or is an exact match (C/N lines
 * only).  The difference in behaviour is to stop C:*::* and N:*::*.
 */
aConfItem *attach_confs(cptr, name, statmask)
	aClient *cptr;
	char *name;
	int  statmask;
{
	aConfItem *tmp;
	aConfItem *first = NULL;
	int  len = strlen(name);

	if (!name || len > HOSTLEN)
		return NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
	{
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    ((tmp->status & (CONF_SERVER_MASK | CONF_HUB)) == 0)
		    && tmp->name && !match(tmp->name, name))
		{
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		}
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & (CONF_SERVER_MASK | CONF_HUB)) &&
		    tmp->name && !match(tmp->name, name))
		{
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		}
	}
	return (first);
}

/*
 * Added for new access check    meLazy
 */
aConfItem *attach_confs_host(cptr, host, statmask)
	aClient *cptr;
	char *host;
	int  statmask;
{
	aConfItem *tmp;
	aConfItem *first = NULL;
	int  len = strlen(host);

	if (!host || len > HOSTLEN)
		return NULL;

	for (tmp = conf; tmp; tmp = tmp->next)
	{
		if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & CONF_SERVER_MASK) == 0 &&
		    (!tmp->host || match(tmp->host, host) == 0))
		{
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		}
		else if ((tmp->status & statmask) && !IsIllegal(tmp) &&
		    (tmp->status & CONF_SERVER_MASK) &&
		    (tmp->host && mycmp(tmp->host, host) == 0))
		{
			if (!attach_conf(cptr, tmp) && !first)
				first = tmp;
		}
	}
	return (first);
}

/*
 * find a conf entry which matches the hostname and has the same name.
 */
aConfItem *find_conf_exact(name, user, host, statmask)
	char *name, *host, *user;
	int  statmask;
{
	aConfItem *tmp;
	char userhost[USERLEN + HOSTLEN + 3];

	(void)ircsprintf(userhost, "%s@%s", user, host);

	for (tmp = conf; tmp; tmp = tmp->next)
	{
		if (!(tmp->status & statmask) || !tmp->name || !tmp->host ||
		    mycmp(tmp->name, name))
			continue;
		if (tmp->status & CONF_ILLEGAL)
			continue;
		/*
		   ** Accept if the *real* hostname (usually sockecthost)
		   ** socket host) matches *either* host or name field
		   ** of the configuration.
		 */
		if (match(tmp->host, userhost))
			continue;
		if (tmp->status & (CONF_OPERATOR | CONF_LOCOP))
		{
			if (tmp->clients < MaxLinks(Class(tmp)))
				return tmp;
			else
				continue;
		}
		else
			return tmp;
	}
	return NULL;
}

aConfItem *find_conf_name(name, statmask)
	char *name;
	int  statmask;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
	{
		/*
		   ** Accept if the *real* hostname (usually sockecthost)
		   ** matches *either* host or name field of the configuration.
		 */
		if ((tmp->status & statmask) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	}
	return NULL;
}

aConfItem *find_conf_servern(name)
	char *name;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
	{
		/*
		   ** Accept if the *real* hostname (usually sockecthost)
		   ** matches *either* host or name field of the configuration.
		 */
		if ((tmp->status & CONF_NOCONNECT_SERVER) &&
		    (!tmp->name || match(tmp->name, name) == 0))
			return tmp;
	}
	return NULL;
}

aConfItem *find_conf(lp, name, statmask)
	char *name;
	Link *lp;
	int  statmask;
{
	aConfItem *tmp;
	int  namelen = name ? strlen(name) : 0;

	if (namelen > HOSTLEN)
		return (aConfItem *)0;

	for (; lp; lp = lp->next)
	{
		tmp = lp->value.aconf;
		if ((tmp->status & statmask) &&
		    (((tmp->status & (CONF_SERVER_MASK | CONF_HUB)) &&
		    tmp->name && !match(tmp->name, name)) ||
		    ((tmp->status & (CONF_SERVER_MASK | CONF_HUB)) == 0 &&
		    tmp->name && !match(tmp->name, name))))
			return tmp;
	}
	return NULL;
}

/*
 * Added for new access check    meLazy
 */
aConfItem *find_conf_host(lp, host, statmask)
	Link *lp;
	char *host;
	int  statmask;
{
	aConfItem *tmp;
	int  hostlen = host ? strlen(host) : 0;

	if (hostlen > HOSTLEN || BadPtr(host))
		return (aConfItem *)NULL;
	for (; lp; lp = lp->next)
	{
		tmp = lp->value.aconf;
		if (tmp->status & statmask &&
		    (!(tmp->status & CONF_SERVER_MASK || tmp->host) ||
		    (tmp->host && !match(tmp->host, host))))
			return tmp;
	}
	return NULL;
}

/* Written by Raistlin for bahamut */

aConfItem *find_uline(Link *lp, char *host) {
	aConfItem *tmp;
	int         hostlen = host ? strlen(host) : 0;
	
	if (hostlen > HOSTLEN || BadPtr(host))
		return ((aConfItem *) NULL);
	for (; lp; lp = lp->next) {
		tmp = lp->value.aconf;
		if (tmp->status & CONF_UWORLD && (tmp->host && !mycmp(tmp->host, host)))
			return tmp;
	}
	return ((aConfItem *) NULL);
}
/* find_exception        
** find a virtual exception
*/
int  find_exception(char *abba)
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
	{
	}

	return 0;
}

/*
 * find_conf_ip
 *
 * Find a conf line using the IP# stored in it to search upon.
 * Added 1/8/92 by Avalon.
 */
aConfItem *find_conf_ip(lp, ip, user, statmask)
	char *ip, *user;
	Link *lp;
	int  statmask;
{
	aConfItem *tmp;
	char *s;

	for (; lp; lp = lp->next)
	{
		tmp = lp->value.aconf;
		if (!(tmp->status & statmask))
			continue;
		s = index(tmp->host, '@');
		*s = '\0';
		if (match(tmp->host, user))
		{
			*s = '@';
			continue;
		}
		*s = '@';
		if (!bcmp((char *)&tmp->ipnum, ip, sizeof(struct IN_ADDR)))
			return tmp;
	}
	return NULL;
}

/*
 * find_conf_entry
 *
 * - looks for a match on all given fields.
 */
aConfItem *find_conf_entry(aconf, mask)
	aConfItem *aconf;
	u_int mask;
{
	aConfItem *bconf;

	for (bconf = conf, mask &= ~CONF_ILLEGAL; bconf; bconf = bconf->next)
	{
		if (!(bconf->status & mask) || (bconf->port != aconf->port))
			continue;

		if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
		    (BadPtr(aconf->host) && !BadPtr(bconf->host)))
			continue;
		if (!BadPtr(bconf->host) && mycmp(bconf->host, aconf->host))
			continue;

		if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
		    (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
			continue;
		if (!BadPtr(bconf->passwd) && mycmp(bconf->passwd, "ONE") &&
		    mycmp(bconf->passwd, aconf->passwd))
			continue;

		if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
		    (BadPtr(aconf->name) && !BadPtr(bconf->name)))
			continue;
		if (!BadPtr(bconf->name) && mycmp(bconf->name, aconf->name))
			continue;
		break;
	}
	return bconf;
}

/*
 * find_temp_conf_entry
 *
 * - looks for a match on all given fields for a TEMP conf line.
 *  Right now the passwd,port, and class fields are ignored, because it's
 *  only useful for k:lines anyway.  -Russell   11/22/95
 *  1/21/95 Now looks for any conf line.  I'm leaving this routine and its
 *  call in because this routine has potential in future upgrades. -Russell
 */
aConfItem *find_temp_conf_entry(aconf, mask)
	aConfItem *aconf;
	u_int mask;
{
	aConfItem *bconf;

	for (bconf = conf, mask &= ~CONF_ILLEGAL; bconf; bconf = bconf->next)
	{
		/* kline/unkline/kline fix -- Barubary */
		if (bconf->status & CONF_ILLEGAL)
			continue;
		if (!(bconf->status & mask) || (bconf->port != aconf->port))
			continue;
/*                if (!bconf->tempconf) continue;*/
		if ((BadPtr(bconf->host) && !BadPtr(aconf->host)) ||
		    (BadPtr(aconf->host) && !BadPtr(bconf->host)))
			continue;
		if (!BadPtr(bconf->host) && mycmp(bconf->host, aconf->host))
			continue;

/*                if ((BadPtr(bconf->passwd) && !BadPtr(aconf->passwd)) ||
                    (BadPtr(aconf->passwd) && !BadPtr(bconf->passwd)))
                        continue;
                if (!BadPtr(bconf->passwd) &&
                    mycmp(bconf->passwd, aconf->passwd))
                        continue;*/

		if ((BadPtr(bconf->name) && !BadPtr(aconf->name)) ||
		    (BadPtr(aconf->name) && !BadPtr(bconf->name)))
			continue;
		if (!BadPtr(bconf->name) && mycmp(bconf->name, aconf->name))
			continue;
		break;
	}
	return bconf;
}

aSqlineItem *find_sqline_nick(nickmask)
	char *nickmask;
{
	aSqlineItem *asqline;

	for (asqline = sqline; asqline; asqline = asqline->next)
	{
		if (!BadPtr(asqline->sqline) && (asqline->status !=
		    CONF_ILLEGAL) && !mycmp(asqline->sqline, nickmask))
			return asqline;
	}
	return NULL;
}

aSqlineItem *find_sqline_match(nickname)
	char *nickname;
{
	aSqlineItem *asqline;

	for (asqline = sqline; asqline; asqline = asqline->next)
	{
		if (!BadPtr(asqline->sqline) && (asqline->status !=
		    CONF_ILLEGAL) && !match(asqline->sqline, nickname))
			return asqline;
	}
	return NULL;
}

/*
**      parv[0] = sender prefix
**      parv[1] = server
**      parv[2] = +/-
**
*/

int SVSNOOP = 0;

int  m_svsnoop(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;

	/* Intially this was wrong, as check_registered returns -1 if
	 * they aren't registered and 0 if they are. Thus a ! is required
	 * in front of it otherwise the entire logic statement is flawed
	 * and makes the function generally useless.
	 * --Luke
	 */
	if (!(!check_registered(sptr) && IsULine(sptr) && parc > 2))
		return 0;
	
	/* svsnoop bugfix --binary
	 * Forgot a : before the first %s :P
	 * --Luke
	 */
	if (hunt_server(cptr, sptr, ":%s SVSNOOP %s :%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		if (parv[2][0] == '+')
		{
			SVSNOOP = 1;
			sendto_ops("This server has been placed in NOOP mode");
			for (acptr = &me; acptr; acptr = acptr->prev)
			{
				if (MyClient(acptr) && IsAnOper(acptr))
				{
					if (IsOper(acptr))
						IRCstats.operators--;
					acptr->umodes &=
					    ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP | UMODE_SERVICES |
					    UMODE_SADMIN | UMODE_ADMIN);
					acptr->umodes &=
		    				~(UMODE_NETADMIN | UMODE_CLIENT |
		 			   UMODE_FLOOD | UMODE_EYES | UMODE_WHOIS);
					acptr->umodes &=
					    ~(UMODE_KIX | UMODE_FCLIENT | UMODE_HIDING |
					    UMODE_DEAF | UMODE_HIDEOPER);
					acptr->oflag = 0;
				
				}
			}

		}
		else
		{
			SVSNOOP = 0;
			sendto_ops("This server is no longer in NOOP mode");
		}
	}
}

#define doDebug debugNotice( __FILE__, __LINE__)

void debugNotice(char *file, long line)
{
	/* a little handy debug tool --sts */
#ifdef STSDEBUG
	sendto_ops("# !Debug! # %s:%i", file, line);
	flush_connections(me.fd);
#endif
}

/*
 * rehash
 *
 * Actual REHASH service routine. Called with sig == 0 if it has been called
 * as a result of an operator issuing this command, else assume it has been
 * called as a result of the server receiving a HUP signal.
 */
int  rehash(cptr, sptr, sig)
	aClient *cptr, *sptr;
	int  sig;
{
	aConfItem **tmp = &conf, *tmp2;
	aClass *cltmp;
	aClient *acptr;
	int  i;
	int  ret = 0;
	/* One of the REHASH bugs -- sts */
	flush_connections(me.fd);
	if (sig == 1)
	{
		sendto_ops("Got signal SIGHUP, reloading ircd conf. file");
#ifdef	ULTRIX
		if (fork() > 0)
			exit(0);
		write_pidfile();
#endif
	}
	for (i = 0; i <= highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		{
			/*
			 * Nullify any references from client structures to
			 * this host structure which is about to be freed.
			 * Could always keep reference counts instead of
			 * this....-avalon
			 */
			acptr->hostp = NULL;
		}
	while ((tmp2 = *tmp))
		if (tmp2->clients || tmp2->status & CONF_LISTEN_PORT)
		{
			/*
			   ** Configuration entry is still in use by some
			   ** local clients, cannot delete it--mark it so
			   ** that it will be deleted when the last client
			   ** exits...
			 */
			if (!(tmp2->status & (CONF_LISTEN_PORT | CONF_CLIENT)))
			{
				*tmp = tmp2->next;
				tmp2->next = NULL;
			}
			else
				tmp = &tmp2->next;
			tmp2->status |= CONF_ILLEGAL;
		}
		else
		{
			*tmp = tmp2->next;
			/* free expression trees of connect rules */
			if ((tmp2->status & (CONF_CRULEALL | CONF_CRULEAUTO))
			    && (tmp2->passwd != NULL))
				crule_free(&(tmp2->passwd));
			free_conf(tmp2);
		}
	/*
	 * We don't delete the class table, rather mark all entries
	 * for deletion. The table is cleaned up by check_class. - avalon
	 */
	for (cltmp = NextClass(FirstClass()); cltmp; cltmp = NextClass(cltmp))
		MaxLinks(cltmp) = -1;
#ifndef NEWDNS
	if (sig != 2)
		flush_cache();
#endif /*NEWDNS*/
	
	(void)initconf(0);
	close_listeners();
	/*
	 * flush out deleted I and P lines although still in use.
	 */
	for (tmp = &conf; (tmp2 = *tmp);)
		if (!(tmp2->status & CONF_ILLEGAL))
			tmp = &tmp2->next;
		else
		{
			*tmp = tmp2->next;
			tmp2->next = NULL;
			if (!tmp2->clients)
				free_conf(tmp2);
		}
	/* Added to make sure K-lines are checked -- Barubary */
	check_pings(TStime(), 1);
	/* Recheck all U-lines -- Barubary */
	for (i = 0; i < highest_fd; i++)
		if ((acptr = local[i]) && !IsMe(acptr))
		{
			if (find_conf_host(acptr->from->confs, acptr->name,
			    CONF_UWORLD) || (acptr->user
			    && find_conf_host(acptr->from->confs,
			    acptr->user->server, CONF_UWORLD)))
				acptr->flags |= FLAGS_ULINE;
			else
				acptr->flags &= ~FLAGS_ULINE;
		}
	reset_help();		/* Reinitialize help-system. -Donwulff */
	return ret;
}

/*
 * openconf
 *
 * returns -1 on any error or else the fd opened from which to read the
 * configuration file from.  This may either be th4 file direct or one end
 * of a pipe from m4.
 */
int  openconf()
{
	return open(configfile, O_RDONLY);
}
extern char *getfield();

#define STAR1 OFLAG_SADMIN|OFLAG_ADMIN|OFLAG_NETADMIN|OFLAG_COADMIN
#define STAR2 OFLAG_ZLINE|OFLAG_HIDE|OFLAG_WHOIS
#define STAR3 OFLAG_INVISIBLE
static int oper_access[] = {
	~(STAR1 | STAR2 | STAR3), '*',
	OFLAG_LOCAL, 'o',
	OFLAG_GLOBAL, 'O',
	OFLAG_REHASH, 'r',
	OFLAG_EYES, 'e',
	OFLAG_DIE, 'D',
	OFLAG_RESTART, 'R',
	OFLAG_HELPOP, 'h',
	OFLAG_GLOBOP, 'g',
	OFLAG_WALLOP, 'w',
	OFLAG_LOCOP, 'l',
	OFLAG_LROUTE, 'c',
	OFLAG_GROUTE, 'L',
	OFLAG_LKILL, 'k',
	OFLAG_GKILL, 'K',
	OFLAG_KLINE, 'b',
	OFLAG_UNKLINE, 'B',
	OFLAG_LNOTICE, 'n',
	OFLAG_GNOTICE, 'G',
	OFLAG_ADMIN, 'A',
	OFLAG_SADMIN, 'a',
	OFLAG_NETADMIN, 'N',
	OFLAG_COADMIN, 'C',
	OFLAG_UMODEC, 'u',
	OFLAG_UMODEF, 'f',
	OFLAG_ZLINE, 'z',
	OFLAG_WHOIS, 'W',
	OFLAG_HIDE, 'H',
/*        OFLAG_AGENT,	'S',*/
	OFLAG_INVISIBLE, '^',
	0, 0
};

char oflagbuf[128];

char *oflagstr(long oflag)
{
	int *i, flag;
	char m;
	char *p = oflagbuf;

	for (i = &oper_access[6], m = *(i + 1); (flag = *i);
	    i += 2, m = *(i + 1))
	{
		if (oflag & flag)
		{
			*p = m;
			p++;
		}
	}
	*p = '\0';
	return oflagbuf;
}

/*
** initconf() 
**    Read configuration file.
**
**    returns -1, if file cannot be opened
**             0, if file opened
*/

#define MAXCONFLINKS 150

int  initconf(opt)
	int  opt;
{
	static char quotes[9][2] = { {'b', '\b'}, {'f', '\f'}, {'n', '\n'},
	{'r', '\r'}, {'t', '\t'}, {'v', '\v'},
	{'\\', '\\'}, {0, 0}
	};
	char *tmp, *s;
	int  fd, i;
	char line[512], c[80];
	int  ccount = 0, ncount = 0;
	aConfItem *aconf = NULL;

	Debug((DEBUG_DEBUG, "initconf(): ircd.conf = %s", configfile));
	if ((fd = openconf()) == -1)
	{
		return -1;
	}
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */
	while ((i = dgets(fd, line, sizeof(line) - 1)) > 0)
	{
		line[i] = '\0';
		iCstrip(line);
/*		while (dgets(fd, c, sizeof(c) - 1) > 0)
		{
		} */
		/*
		 * Do quoting of characters and # detection.
		 */
		for (tmp = line; *tmp; tmp++)
		{
			if (*tmp == '\\')
			{
				for (i = 0; quotes[i][0]; i++)
					if (quotes[i][0] == *(tmp + 1))
					{
						*tmp = quotes[i][1];
						break;
					}
				if (!quotes[i][0])
					*tmp = *(tmp + 1);
				if (!*(tmp + 1))
					break;
				else
					for (s = tmp; *s = *(s + 1); s++)
						;
			}
			else if (*tmp == '#')
				*tmp = '\0';
		}
		if (!*line || line[0] == '#' || line[0] == '\n' ||
		    line[0] == ' ' || line[0] == '\t')
			continue;

		/* Could we test if it's conf line at all?      -Vesa */
		if (line[1] != ':')
		{
			Debug((DEBUG_ERROR, "Bad config line: %s", line));
			continue;
		}
		if (aconf)
			free_conf(aconf);
		aconf = make_conf();

		tmp = getfield(line);
		if (!tmp)
			continue;
		switch (*tmp)
		{
		  case 'A':	/* Name, e-mail address of administrator */
			  aconf->status = CONF_ADMIN;
			  break;
		  case 'a':	/* of this server. */
			  aconf->status = CONF_SADMIN;
			  break;
		  case 'C':	/* Server where I should try to connect */
		  case 'c':	/* in case of lp failures             */
			  ccount++;
			  aconf->status = CONF_CONNECT_SERVER;
			  break;
			  /* Connect rule */
		  case 'D':
			  aconf->status = CONF_CRULEALL;
			  break;
			  /* Connect rule - autos only */
		  case 'd':
			  aconf->status = CONF_CRULEAUTO;
			  break;
		  case 'e':
			  aconf->status = CONF_SOCKSEXCEPT;
			  break;
		  case 'E':
			  aconf->status = CONF_EXCEPT;
			  break;
		  case 'G':
		  case 'g':
			  /* General config options */
			  aconf->status = CONF_CONFIG;
			  break;
		  case 'H':	/* Hub server line */
		  case 'h':
			  aconf->status = CONF_HUB;
			  break;
		  case 'I':	/* Just plain normal irc client trying  */
		  case 'i':	/* to connect me */
			  aconf->status = CONF_CLIENT;
			  break;
		  case 'K':	/* Kill user line on ircd.conf */
		  case 'k':
			  aconf->status = CONF_KILL;
			  break;
			  /* Operator. Line should contain at least */
			  /* password and host where connection is  */
		  case 'L':	/* guaranteed leaf server */
		  case 'l':
			  aconf->status = CONF_LEAF;
			  break;
			  /* Me. Host field is name used for this host */
			  /* and port number is the number of the port */
		  case 'M':
		  case 'm':
			  aconf->status = CONF_ME;
			  break;
		  case 'n':
			  aconf->status = CONF_NLINE;
			  break;
		  case 'N':	/* Server where I should NOT try to     */
			  /* connect in case of lp failures     */
			  /* but which tries to connect ME        */
			  ++ncount;
			  aconf->status = CONF_NOCONNECT_SERVER;
			  break;
		  case 'O':
			  aconf->status = CONF_OPERATOR;
			  break;
			  /* Local Operator, (limited privs --SRB)
			   * Not anymore, OperFlag access levels. -Cabal95 */
		  case 'o':
			  aconf->status = CONF_OPERATOR;
			  break;
		  case 'P':	/* listen port line */
		  case 'p':
			  aconf->status = CONF_LISTEN_PORT;
			  break;
		  case 'Q':	/* reserved nicks */
			  aconf->status = CONF_QUARANTINED_NICK;
			  break;
		  case 'q':	/* a server that you don't want in your */
			  /* network. USE WITH CAUTION! */
			  aconf->status = CONF_QUARANTINED_SERVER;

			  break;
		  case 'S':	/* Service. Same semantics as   */
		  case 's':	/* CONF_OPERATOR                */
			  aconf->status = CONF_SERVICE;
			  break;
		  case 'T':
			  aconf->status = CONF_TLINE;
			  break;
		  case 'U':	/* Underworld server, allowed to hack modes */
		  case 'u':	/* *Every* server on the net must define the same !!! */
			  aconf->status = CONF_UWORLD;
			  break;
		  case 'V':
		  case 'v':
			  aconf->status = CONF_VERSION;
			  break;
		  case 'Y':
		  case 'y':
			  aconf->status = CONF_CLASS;
			  break;
		  case 'Z':
		  case 'z':
			  aconf->status = CONF_ZAP;
			  break;
		  case 'X':
		  case 'x':
			  aconf->status = CONF_DRPASS;
			  break;
		  default:
			  Debug((DEBUG_ERROR, "Error in config file: %s",
			      line));
			  break;
		}
		if (IsIllegal(aconf))
			continue;

		for (;;)	/* Fake loop, that I can use break here --msa */
		{
			/* Yes I know this could be much cleaner, but I did not
			 * want to put it into its own separate function, but  
			 * I believe the X:should be like this:
			 * X:restartpass:diepass
			 * which leaves this code untouched. This is already indented
			 * enough to justify that...
			 */
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->host, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->passwd, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			DupString(aconf->name, tmp);
			if ((tmp = getfield(NULL)) == NULL)
				break;
			if (aconf->status & CONF_OPS)
			{
				int *i, flag;
				char *m = "*";
				/*
				 * Now we use access flags to define
				 * what an operator can do with their O.
				 */
				for (m = (*tmp) ? tmp : m; *m; m++)
				{
					for (i = oper_access; (flag = *i);
					    i += 2)
						if (*m == (char)(*(i + 1)))
						{
							aconf->port |= flag;
							break;
						}
				}
				if (!(aconf->port & OFLAG_ISGLOBAL))
					aconf->status = CONF_LOCOP;
			}
			else
				aconf->port = atoi(tmp);
			if ((tmp = getfield(NULL)) == NULL)
			{
				break;
			}
			Class(aconf) = find_class(atoi(tmp));
			if (aconf->status == CONF_ME)
			{
				if (me.serv->numeric)
				{
					if (atoi(tmp) != me.serv->numeric)
					{
						if (IRCstats.servers > 1)
						{
							sendto_ops("You cannot change numeric when servers are connected");
						}
							else
						{
							me.serv->numeric = atoi(tmp);
						}
					}
				} else
					me.serv->numeric = atoi(tmp);
			}
			if ((tmp = getfield(NULL)) == NULL)
			{
				break;
			}
			if (aconf->status == CONF_CONNECT_SERVER)
			{
				char	*cp = tmp;
				
				aconf->options = 0;
				for (; *cp; cp++)
				{
					if (*cp == 'S')
						aconf->options |= CONNECT_SSL;
				}
			}
			break;
		}
		/*
		   ** If conf line is a general config, just
		   ** see if we recognize the keyword, and set
		   ** the appropriate global.  We don't use a "standard"
		   ** config link here, because these are things which need
		   ** to be tested SO often that a simple global test
		   ** is much better!  -Aeto
		 */
		if ((aconf->status & CONF_CONFIG) == CONF_CONFIG)
		{
			continue;
		}

		/* Check for bad Z-lines masks as they are *very* dangerous
		   if not correct!!! */
		if (aconf->status == CONF_ZAP)
		{
			char *tempc = aconf->host;
			if (!tempc)
			{
				free_conf(aconf);
				aconf = NULL;
				continue;
			}
			for (; *tempc; tempc++)
				if ((*tempc >= '0') && (*tempc <= '9'))
					goto zap_safe;
			free_conf(aconf);
			aconf = NULL;
			continue;
		      zap_safe:;
		}
		/*
		   ** T:Line protection stuff..
		   **
		   **
		 */
		if (aconf->status & CONF_TLINE)
		{
			if (*aconf->passwd == '/')
				goto badtline;
			if (strstr(aconf->passwd, ".."))
				goto badtline;
			if (strchr(aconf->passwd, '~'))
				goto badtline;
			if (!strcmp(aconf->passwd, configfile)
			    || !strcmp(aconf->name, configfile))
				goto badtline;
			if (!strcmp(aconf->passwd, CPATH)
			    || !strcmp(aconf->name, CPATH))
				goto badtline;
			if (!strcmp(aconf->passwd, ZCONF)
			    || !strcmp(aconf->name, ZCONF))
				goto badtline;
			if (!strcmp(aconf->passwd, OPATH)
			    || !strcmp(aconf->name, OPATH))
				goto badtline;
			if (!strcmp(aconf->passwd, lPATH)
			    || !strcmp(aconf->name, lPATH))
				goto badtline;

			goto tline_safe;
		      badtline:
			free_conf(aconf);
			aconf = NULL;
			continue;
		      tline_safe:;
		}
		/*
		   ** If conf line is a class definition, create a class entry
		   ** for it and make the conf_line illegal and delete it.
		 */
		if (aconf->status & CONF_CLASS)
		{
			add_class(atoi(aconf->host), atoi(aconf->passwd),
			    atoi(aconf->name), aconf->port,
			    tmp ? atoi(tmp) : 0);
			continue;
		}
		/*
		   ** associate each conf line with a class by using a pointer
		   ** to the correct class record. -avalon
		 */
		if (aconf->status & (CONF_CLIENT_MASK | CONF_LISTEN_PORT))
		{
			if (Class(aconf) == 0)
				Class(aconf) = find_class(0);
			if (MaxLinks(Class(aconf)) < 0)
				Class(aconf) = find_class(0);
		}
		if (aconf->status & (CONF_LISTEN_PORT | CONF_CLIENT))
		{
			aConfItem *bconf;

			if (bconf = find_conf_entry(aconf, aconf->status))
			{
				delist_conf(bconf);
				bconf->status &= ~CONF_ILLEGAL;
				if (aconf->status == CONF_CLIENT)
				{
					bconf->class->links -= bconf->clients;
					bconf->class = aconf->class;
					if (bconf->class)
						bconf->class->links +=
						    bconf->clients;
				}
				free_conf(aconf);
				aconf = bconf;
			}
			else if (aconf->host &&
			    aconf->status == CONF_LISTEN_PORT)
				(void)add_listener(aconf);
		}
		if (aconf->status & CONF_SERVER_MASK)
			if (ncount > MAXCONFLINKS || ccount > MAXCONFLINKS ||
			    !aconf->host || !aconf->name)
				continue;

		if (aconf->status &
		    (CONF_SERVER_MASK | CONF_LOCOP | CONF_OPERATOR))
			if (!index(aconf->host, '@') && *aconf->host != '/')
			{
				char *newhost;
				int  len = 3;	/* *@\0 = 3 */

				len += strlen(aconf->host);
				newhost = (char *)MyMalloc(len);
				(void)ircsprintf(newhost, "*@%s", aconf->host);
				MyFree(aconf->host);
				aconf->host = newhost;
			}
		if (aconf->status & CONF_SERVER_MASK)
		{
			if (BadPtr(aconf->passwd))
				continue;
			else if (!(opt & BOOT_QUICK))
				(void)lookup_confhost(aconf);
		}

		/* Create expression tree from connect rule...
		   ** If there's a parsing error, nuke the conf structure */
		if (aconf->status & (CONF_CRULEALL | CONF_CRULEAUTO))
		{
			MyFree(aconf->passwd);
			if ((aconf->passwd =
			    (char *)crule_parse(aconf->name)) == NULL)
			{
				free_conf(aconf);
				aconf = NULL;
				continue;
			}
		}

		/*
		   ** Own port and name cannot be changed after the startup.
		   ** (or could be allowed, but only if all links are closed
		   ** first).
		   ** Configuration info does not override the name and port
		   ** if previously defined. Note, that "info"-field can be
		   ** changed by "/rehash".
		 */
		if (aconf->status == CONF_ME)
		{
			strncpyzt(me.info, aconf->name, sizeof(me.info));
			if (me.name[0] == '\0' && !strchr(aconf->host, '.')) {
				ircd_log("ERROR: Invalid Server Name %s, Reason: Servername must contain at least one \".\"\n",aconf->host);
				exit(-1);
			}
			if (me.name[0] == '\0' && aconf->host[0])
				strncpyzt(me.name, aconf->host,
				    sizeof(me.name));
#ifndef INET6
			if (aconf->passwd[0] && (aconf->passwd[0] != '*'))
			{
				me.ip.S_ADDR = inet_addr(aconf->passwd);

			}
			else
				me.ip.S_ADDR = INADDR_ANY;
#else
			if (aconf->passwd[0] && (aconf->passwd[0] != '*'))
			{
				inet_pton(AFINET, aconf->passwd, me.ip.S_ADDR);
			}
			else
				me.ip = in6addr_any;

#endif
			if (portnum < 0 && aconf->port >= 0)
				portnum = aconf->port;
		}
		if (aconf->status == CONF_EXCEPT)
			aconf->tmpconf = KLINE_EXCEPT;
		if (aconf->status == CONF_KILL)
			aconf->tmpconf = KLINE_PERM;
		(void)collapse(aconf->host);
		(void)collapse(aconf->name);
		Debug((DEBUG_NOTICE,
		    "Read Init: (%d) (%s) (%s) (%s) (%d) (%d)",
		    aconf->status, aconf->host, aconf->passwd,
		    aconf->name, aconf->port, Class(aconf)));
		aconf->next = conf;
		conf = aconf;
		aconf = NULL;
	}
	if (aconf)
		free_conf(aconf);
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */
	(void)close(fd);
	check_class();
	nextping = nextconnect = TStime();
	return 0;
}

/*
 * lookup_confhost
 *   Do (start) DNS lookups of all hostnames in the conf line and convert
 * an IP addresses in a.b.c.d number for to IP#s.
 */
static int lookup_confhost(aconf)
	aConfItem *aconf;
{
	char *s;
	struct hostent *hp;
	Link ln;

	if (BadPtr(aconf->host) || BadPtr(aconf->name))
		goto badlookup;
	if ((s = index(aconf->host, '@')))
		s++;
	else
		s = aconf->host;
	/*
	   ** Do name lookup now on hostnames given and store the
	   ** ip numbers in conf structure.
	 */
	if (!isalpha(*s) && !isdigit(*s))
		goto badlookup;

	/*
	   ** Prepare structure in case we have to wait for a
	   ** reply which we get later and store away.
	 */
	ln.value.aconf = aconf;
	ln.flags = ASYNC_CONF;

	if (isdigit(*s))
#ifdef INET6
		if (!inet_pton(AF_INET6, s, aconf->ipnum.s6_addr))
			bcopy(minus_one, aconf->ipnum.s6_addr, IN6ADDRSZ);
#else
		aconf->ipnum.S_ADDR = inet_addr(s);
#endif

#ifndef NEWDNS
	else if ((hp = gethost_byname(s, &ln)))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
		    sizeof(struct IN_ADDR));
#else /*NEWDNS*/
	else if (hp = newdns_checkcachename(s))
		bcopy(hp->h_addr, (char *)&(aconf->ipnum),
		    sizeof(struct IN_ADDR));
#endif /*NEWDNS*/
	
	
#ifdef INET6
	if (AND16(aconf->ipnum.s6_addr) == 255)
#else
	if (aconf->ipnum.S_ADDR == -1)
#endif
		goto badlookup;
	return 0;
      badlookup:
#ifndef INET6
	if (aconf->ipnum.S_ADDR == -1)
#else
	if (AND16(aconf->ipnum.s6_addr) == 255)
#endif
		bzero((char *)&aconf->ipnum, sizeof(struct IN_ADDR));
	Debug((DEBUG_ERROR, "Host/server name error: (%s) (%s)",
	    aconf->host, aconf->name));
	return -1;
}

char *areason = NULL;

int  find_kill(cptr)
	aClient *cptr;
{
	char reply[256], *host, *name;
	aConfItem *tmp, *tmp2;

	if (!cptr->user)
		return 0;

	host = cptr->sockhost;
	name = cptr->user->username;

	if (strlen(host) > (size_t)HOSTLEN ||
	    (name ? strlen(name) : 0) > (size_t)HOSTLEN)
		return (0);

	reply[0] = '\0';

	/* Only search for E:lines if a K:line was found -- codemastr */

	for (tmp = conf; tmp; tmp = tmp->next)
		if ((tmp->status == CONF_KILL) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
		    (!name || match(tmp->name, name) == 0) &&
			(!tmp->port || (tmp->port == cptr->acpt->port))) {
		for (tmp2 = conf; tmp2; tmp2 = tmp2->next)
		if ((tmp2->status == CONF_EXCEPT) && tmp2->host && tmp2->name &&
		    (match(tmp2->host, host) == 0) &&
		    (!name || match(tmp2->name, name) == 0) &&
		    (!tmp2->port || (tmp2->port == cptr->acpt->port)))
			return 0;

			if (BadPtr(tmp->passwd))
				break;
			else if (is_comment(tmp->passwd))
				break;
		}

	if (reply[0])
		sendto_one(cptr, reply,
		    me.name, ERR_YOUREBANNEDCREEP, cptr->name, KLINE_ADDRESS);
	else if (tmp)
		if (BadPtr(tmp->passwd))
			sendto_one(cptr,
			    ":%s %d %s :*** You are not welcome on this server."
			    "  Email %s for more information.",
			    me.name, ERR_YOUREBANNEDCREEP, cptr->name,
			    KLINE_ADDRESS);
		else
#ifdef COMMENT_IS_FILE
			m_killcomment(cptr, cptr->name, tmp->passwd);
#else
		{
			if (*tmp->passwd == '|' && !strchr(tmp->passwd, '/')
			    && match("|kc.*", tmp->passwd))
			{
				m_killcomment(cptr, cptr->name,
				    (tmp->passwd) + 1);
			}
			else if (tmp->tmpconf == KLINE_AKILL)
				sendto_one(cptr,
				    ":%s %d %s :*** %s",
				    me.name, ERR_YOUREBANNEDCREEP, cptr->name,
				    tmp->passwd);
			else
				sendto_one(cptr,
				    ":%s %d %s :*** You are not welcome on this server: "
				    "%s.  Email %s for more information.",
				    me.name, ERR_YOUREBANNEDCREEP, cptr->name,
				    tmp->passwd, KLINE_ADDRESS);
		}
#endif /* COMMENT_IS_FILE */
	if (tmp)
	{
		areason = !BadPtr(tmp->passwd) ? tmp->passwd : NULL;
	}
	else
		areason = NULL;

	return (tmp ? -1 : 0);
}

char *find_zap(aClient *cptr, int dokillmsg)
{
	aConfItem *tmp;
	char *retval = NULL;
	for (tmp = conf; tmp; tmp = tmp->next)
		if ((tmp->status == CONF_ZAP) && tmp->host &&
		    !match(tmp->host, inetntoa((char *)&cptr->ip)))
		{
			retval = (tmp->passwd) ? tmp->passwd :
			    "Reason unspecified";
			break;
		}
	if (dokillmsg && retval)
		sendto_one(cptr,
		    ":%s %d %s :*** You are not welcome on this server: "
		    "%s.  Email %s for more information.",
		    me.name, ERR_YOUREBANNEDCREEP, cptr->name,
		    retval, KLINE_ADDRESS);
	if (!dokillmsg && retval)
	{
		ircsprintf(zlinebuf,
		    "ERROR :Closing Link: [%s] (You are not welcome on "
		    "this server: %s.  Email %s for more"
		    " information.)\r\n", inetntoa((char *)&cptr->ip),
		    retval, KLINE_ADDRESS);
		retval = zlinebuf;
	}
	return retval;
}

int  find_kill_byname(host, name)
	char *host, *name;
{
	aConfItem *tmp;

	for (tmp = conf; tmp; tmp = tmp->next)
	{
		if ((tmp->status == CONF_KILL) && tmp->host && tmp->name &&
		    (match(tmp->host, host) == 0) &&
		    (!name || match(tmp->name, name) == 0))
			return 1;
	}

	return 0;
}


/*
**  output the reason for being k lined from a file  - Mmmm
** sptr is server    
** parv is the sender prefix
** filename is the file that is to be output to the K lined client
*/
int  m_killcomment(sptr, parv, filename)
	aClient *sptr;
	char *parv, *filename;
{
	int  fd;
	char line[80];
	char *tmp;
	struct stat sb;
	struct tm *tm;

	/*
	 * stop NFS hangs...most systems should be able to open a file in
	 * 3 seconds. -avalon (curtesy of wumpus)
	 */
	fd = open(filename, O_RDONLY);
	if (fd == -1)
	{
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, parv);
		sendto_one(sptr,
		    ":%s %d %s :*** You are not welcome to this server.",
		    me.name, ERR_YOUREBANNEDCREEP, parv);
		return 0;
	}
	(void)fstat(fd, &sb);
	tm = localtime(&sb.st_mtime);
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */
	while (dgets(fd, line, sizeof(line) - 1) > 0)
	{
		if ((tmp = (char *)index(line, '\n')))
			*tmp = '\0';
		if ((tmp = (char *)index(line, '\r')))
			*tmp = '\0';
		/* sendto_one(sptr,
		   ":%s %d %s : %s.",
		   me.name, ERR_YOUREBANNEDCREEP, parv,line); */
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, parv, line);
	}
	sendto_one(sptr,
	    ":%s %d %s :*** You are not welcome to this server.",
	    me.name, ERR_YOUREBANNEDCREEP, parv);
	(void)dgets(-1, NULL, 0);	/* make sure buffer is at empty pos */
	(void)close(fd);
	return 0;
}


/*
** is the K line field an interval or a comment? - Mmmm
*/

static int is_comment(comment)
	char *comment;
{
	int  i;
	for (i = 0; i < strlen(comment); i++)
		if ((comment[i] != ' ') && (comment[i] != '-')
		    && (comment[i] != ',')
		    && ((comment[i] < '0') || (comment[i] > '9')))
			return (1);

	return (0);
}



/*
** m_rakill;
**      parv[0] = sender prefix
**      parv[1] = hostmask
**      parv[2] = username
**      parv[3] = comment
*/
int  m_rakill(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *hostmask, *usermask;
	int  result;

	if (parc < 2 && IsPerson(sptr))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "AKILL");
		return 0;
	}

	if (IsServer(sptr) && parc < 3)
		return 0;

	if (!IsServer(cptr))
	{
		if (!IsOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    sptr->name);
			return 0;
		}
		else
		{
			if ((hostmask = (char *)index(parv[1], '@')))
			{
				*hostmask = 0;
				hostmask++;
				usermask = parv[1];
			}
			else
			{
				sendto_one(sptr, ":%s NOTICE %s :%s", me.name,
				    sptr->name, "Please use a user@host mask.");
				return 0;
			}
		}
	}
	else
	{
		hostmask = parv[1];
		usermask = parv[2];
	}

	if (!usermask || !hostmask)
	{
		/*
		 * This is very bad, it should never happen.
		 */
		sendto_ops("Error adding akill from %s!", sptr->name);
		return 0;
	}

	result = del_temp_conf(CONF_KILL, hostmask, NULL, usermask, 0, 0, 2);
	if (result == KLINE_DEL_ERR)
	{
		if (!MyClient(sptr))
		{
			sendto_serv_butone(cptr, ":%s RAKILL %s %s",
			    IsServer(cptr) ? parv[0] : me.name, hostmask,
			    usermask);
			return 0;
		}
		sendto_one(sptr, ":%s NOTICE %s :Akill %s@%s does not exist.",
		    me.name, sptr->name, usermask, hostmask);
		return 0;
	}

	if (MyClient(sptr))
	{
		sendto_ops("%s removed akill for %s@%s",
		    sptr->name, usermask, hostmask);
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :%s removed akill for %s@%s",
		    me.name, sptr->name, usermask, hostmask);
	}

	sendto_serv_butone(cptr, ":%s RAKILL %s %s",
	    IsServer(cptr) ? parv[0] : me.name, hostmask, usermask);

	check_pings(TStime(), 1);
}

/* ** m_akill;
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int  m_akill(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *hostmask, *usermask, *comment;


	if (parc < 2 && IsPerson(sptr))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "AKILL");
		return 0;
	}

	if (IsServer(sptr) && parc < 3)
		return 0;

	if (!IsServer(cptr))
	{
		if (!IsOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    sptr->name);
			return 0;
		}
		else
		{
			comment = parc < 3 ? NULL : parv[2];
			if ((hostmask = (char *)index(parv[1], '@')))
			{
				*hostmask = 0;
				hostmask++;
				usermask = parv[1];
			}
			else
			{
				sendto_one(sptr, ":%s NOTICE %s :%s", me.name,
				    sptr->name,
				    "Please use a nick!user@host mask.");
				return 0;
			}
			if (!strcmp(usermask, "*") || !strchr(hostmask, '.'))
			{
				sendto_one(sptr,
				    "NOTICE %s :*** What a sweeping AKILL.  If only your admin knew you tried that..",
				    parv[0]);
				sendto_realops("%s attempted to /akill *@*",
				    parv[0]);
				return 0;
			}
			if (MyClient(sptr))
			{
				sendto_ops("%s added akill for %s@%s (%s)",
				    sptr->name, usermask, hostmask,
				    !BadPtr(comment) ? comment : "no reason");
				sendto_serv_butone(&me,
				    ":%s GLOBOPS :%s added akill for %s@%s (%s)",
				    me.name, sptr->name, usermask, hostmask,
				    !BadPtr(comment) ? comment : "no reason");
			}
		}
	}
	else
	{
		hostmask = parv[1];
		usermask = parv[2];
		comment = parc < 4 ? NULL : parv[3];
	}

	if (!usermask || !hostmask)
	{
		/*
		 * This is very bad, it should never happen.
		 */
		sendto_ops("Error adding akill from %s!", sptr->name);
		return 0;
	}

	if (!find_kill_byname(hostmask, usermask))
	{

#ifndef COMMENT_IS_FILE
		add_temp_conf(CONF_KILL, hostmask, comment, usermask, 0, 0, 2);
#else
		add_temp_conf(CONF_KILL, hostmask, NULL, usermask, 0, 0, 2);
#endif
	}

	if (comment)
		sendto_serv_butone(cptr, ":%s AKILL %s %s :%s",
		    IsServer(cptr) ? parv[0] : me.name, hostmask,
		    usermask, comment);
	else
		sendto_serv_butone(cptr, ":%s AKILL %s %s",
		    IsServer(cptr) ? parv[0] : me.name, hostmask, usermask);


	check_pings(TStime(), 1);

}

/*    m_sqline
**	parv[0] = sender
**	parv[1] = nickmask
**	parv[2] = reason
*/
int  m_sqline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aSqlineItem *asqline;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	if (parv[2])
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s :%s", parv[1], parv[2]);
	else
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s", parv[1]);

	asqline = make_sqline();

	if (parv[2])
		DupString(asqline->reason, parv[2]);
	if (parv[1])
		DupString(asqline->sqline, parv[1]);

	if (!find_sqline_nick(parv[1]))
	{
		asqline->next = sqline;
		sqline = asqline;
		asqline = NULL;
	}

	if (asqline)
		free_sqline(asqline);
}

/*    m_unsqline
**	parv[0] = sender
**	parv[1] = nickmask
*/
int  m_unsqline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aSqlineItem *asqline;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	sendto_serv_butone_token(cptr, parv[0], MSG_UNSQLINE, TOK_UNSQLINE,
	    "%s", parv[1]);

	if (!(asqline = find_sqline_nick(parv[1])))
		return;

	asqline->status = CONF_ILLEGAL;

}

/*
** m_kline;
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2] = comment or filename
*/
int  m_kline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *host, *tmp, *hosttemp;
	char uhost[80], name[80];
	int  ip1, ip2, ip3, temp;
	aClient *acptr;
	FILE *aLog;

	if (!MyClient(sptr) || !OPCanKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}


	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KLINE");
		return 0;
	}


/* This patch allows opers to quote kline by address as well as nick
 * --Russell
 */
	if (hosttemp = (char *)strchr((char *)parv[1], '@'))
	{
		temp = 0;
		while (temp <= 20)
			name[temp++] = 0;
		strcpy(uhost, ++hosttemp);
		strncpy(name, parv[1], hosttemp - 1 - parv[1]);
		if (name[0] == '\0' || uhost[0] == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field!"));
			sendto_one(sptr,
			    "NOTICE %s :If you're going to add a userhost, at LEAST specify both fields",
			    parv[0]);
			return 0;
		}
		if (!strcmp(uhost, "*") || !strchr(uhost, '.'))
		{
			sendto_one(sptr,
			    "NOTICE %s :*** What a sweeping K:Line.  If only your admin knew you tried that..",
			    parv[0]);
			sendto_realops("%s attempted to /kline *@*", parv[0]);
			return 0;
		}
	}

/* by nick */
	else
	{
		if (!(acptr = find_client(parv[1], NULL)))
		{
			if (!(acptr =
			    get_history(parv[1], (long)KILLCHASETIMELIMIT)))
			{
				sendto_one(sptr,
				    "NOTICE %s :Can't find user %s to add KLINE",
				    parv[0], parv[1]);
				return 0;
			}
		}

		if (!acptr->user)
			return 0;

		strcpy(name, acptr->user->username);
		if (MyClient(acptr))
			host = acptr->sockhost;
		else
			host = acptr->user->realhost;

		/* Sanity checks */

		if (name == '\0' || host == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field"));
			sendto_one(sptr, "NOTICE %s :Bad field!", parv[0]);
			return 0;
		}

		/* Add some wildcards */


		strcpy(uhost, host);
		if (isdigit(host[strlen(host) - 1]))
		{
			if (sscanf(host, "%d.%d.%d.%*d", &ip1, &ip2, &ip3))
				ircsprintf(uhost, "%d.%d.%d.*", ip1, ip2, ip3);
		}
		else if (sscanf(host, "%*[^.].%*[^.].%s", uhost))
		{		/* Not really... */
			tmp = (char *)strchr(host, '.');
			ircsprintf(uhost, "*%s", tmp);
		}
	}

	sendto_ops("%s added a temp k:line for %s@%s %s", parv[0], name, uhost,
	    parv[2] ? parv[2] : "");
	aLog = fopen(lPATH, "a");
	if (aLog)
	{
		fprintf(aLog, "(%s) %s added a temp k:line for %s@%s %s",
		    myctime(TStime()), parv[0], name, uhost,
		    parv[2] ? parv[2] : "");
		fclose(aLog);
	}

	add_temp_conf(CONF_KILL, uhost, parv[2], name, 0, 0, 1);
	check_pings(TStime(), 1);
}


/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

int  m_unkline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{

	int  result, temp;
	char *hosttemp = parv[1], host[80], name[80];
	FILE *aLog;

	if (!MyClient(sptr) || !OPCanUnKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, "NOTICE %s :Not enough parameters", parv[0]);
		return 0;
	}
	if (hosttemp = (char *)strchr((char *)parv[1], '@'))
	{
		temp = 0;
		while (temp <= 20)
			name[temp++] = 0;
		strcpy(host, ++hosttemp);
		strncpy(name, parv[1], hosttemp - 1 - parv[1]);
		if (name[0] == '\0' || host[0] == '\0')
		{
			Debug((DEBUG_INFO, "UNKLINE: Bad field"));
			sendto_one(sptr,
			    "NOTICE %s : Both user and host fields must be non-null",
			    parv[0]);
			return 0;
		}
		result = del_temp_conf(CONF_KILL, host, NULL, name,
		    NULL, NULL, 0);
		if (result == KLINE_RET_AKILL)
		{		/* akill - result = 3 */
			sendto_one(sptr,
			    "NOTICE %s :You may not remove autokills.  Only U:lined clients may.",
			    parv[0]);
			return 0;
		}
		if (result == KLINE_RET_PERM)
		{		/* Not a temporary line - result =2 */
			sendto_one(sptr,
			    "NOTICE %s :You may not remove permanent K:Lines - talk to the admin",
			    parv[0]);
			return 0;
		}
		if (result == KLINE_RET_DELOK)
		{		/* Successful result = 1 */
			sendto_one(sptr,
			    "NOTICE %s :Temp K:Line %s@%s is now removed.",
			    parv[0], name, host);
			sendto_ops("%s removed temp k:line %s@%s", parv[0],
			    name, host);
			aLog = fopen(lPATH, "a");
			if (aLog)
			{
				fprintf(aLog,
				    "(%s) %s removed temp k:line %s@%s",
				    myctime(TStime()), parv[0], name, host);
				fclose(aLog);
			}
			return 0;
		}
		if (result == KLINE_DEL_ERR)
		{		/* Unsuccessful result = 0 */
			sendto_one(sptr,
			    "NOTICE %s :Temporary K:Line %s@%s not found",
			    parv[0], name, host);
			return 0;
		}
	}
	/* This wasn't here before -- Barubary */
	check_pings(TStime(), 1);
}

/*
 *  m_zline                       add a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 *    parv[2] = reason
 */

int  m_zline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char userhost[512 + 2] = "", *in;
	int  uline = 0, i = 0, propo = 0;
	char *reason, *mask, *server, *person;
	aClient *acptr;

	reason = mask = server = person = NULL;

	reason = ((parc >= 3) ? parv[parc - 1] : "Reason unspecified");
	mask = ((parc >= 2) ? parv[parc - 2] : NULL);
	server = ((parc >= 4) ? parv[parc - 1] : NULL);

	if (parc == 4)
	{
		mask = parv[parc - 3];
		server = parv[parc - 2];
		reason = parv[parc - 1];
	}

	uline = IsULine(sptr) ? 1 : 0;

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	if (uline)
	{
		if (parc >= 4 && server)
		{
			if (hunt_server(cptr, sptr, ":%s ZLINE %s %s :%s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			propo = 1;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ZLINE");
		return -1;
	}

	if (acptr = find_client(parv[1], NULL))
	{
		strcpy(userhost, inetntoa((char *)&acptr->ip));
		person = &acptr->name[0];
		acptr = NULL;
	}
	/* z-lines don't support user@host format, they only 
	   work with ip addresses and nicks */
	else if ((in = index(parv[1], '@')) && (*(in + 1) != '\0'))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}
	else if (in && !(*(in + 1)))	/* sheesh not only specifying a ident@, but
					   omitting the ip...? */
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :Hey! z:lines need an ip address...",
		    me.name, sptr->name);
		return -1;
	}
	else
	{
		strcpy(userhost, parv[1]);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}

	/* this'll protect against z-lining *.* or something */
	if (advanced_check(userhost, TRUE) == FALSE)
	{
		sendto_ops("Bad z:line mask from %s *@%s [%s]", parv[0],
		    userhost, reason ? reason : "");
		if (MyClient(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*@%s is a bad z:line mask...",
			    me.name, sptr->name, userhost);
		return;
	}

	if (uline == 0)
	{
		if (person)
			sendto_ops("%s added a temp z:line for %s (*@%s) [%s]",
			    parv[0], person, userhost, reason ? reason : "");
		else
			sendto_ops("%s added a temp z:line *@%s [%s]", parv[0],
			    userhost, reason ? reason : "");
		(void)add_temp_conf(CONF_ZAP, userhost, reason, NULL, 0, 0,
		    KLINE_TEMP);
	}
	else
	{
		if (person)
			sendto_ops("%s z:lined %s (*@%s) on %s [%s]", parv[0],
			    person, userhost, server ? server : ircnetwork,
			    reason ? reason : "");
		else
			sendto_ops("%s z:lined *@%s on %s [%s]", parv[0],
			    userhost, server ? server : ircnetwork,
			    reason ? reason : "");
		(void)add_temp_conf(CONF_ZAP, userhost, reason, NULL, 0, 0,
		    KLINE_AKILL);
	}

	/* something's wrong if i'm
	   zapping the command source... */
	if (find_zap(cptr, 0) || find_zap(sptr, 0))
	{
		sendto_failops_whoare_opers
		    ("z:line error: mask=%s parsed=%s I tried to zap cptr",
		    mask, userhost);
		sendto_serv_butone(NULL,
		    ":%s GLOBOPS :z:line error: mask=%s parsed=%s I tried to zap cptr",
		    me.name, mask, userhost);
		flush_connections(me.fd);
		(void)rehash(&me, &me, 0);
		return;
	}
#ifdef PRECISE_CHECK
	for (i = highest_fd; i > 0; i--)
	{
		if (!(acptr = local[i]) || IsLog(acptr) || IsMe(acptr));
		continue;
#else
	for (acptr = &me; acptr; acptr = acptr->prev)
	{
		if (IsMe(cptr) || !MyClient(cptr) || IsLog(cptr))
			continue;
#endif

		if (find_zap(acptr, 1))
		{
			if (!IsServer(acptr))
			{
				sendto_one(sptr, ":%s NOTICE %s :*** %s %s",
				    me.name, sptr->name,
				    acptr->name[0] ? acptr->name : "<unknown>");
				exit_client(acptr, acptr, acptr, "z-lined");
			}
			else
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** exiting %s", me.name,
				    sptr->name, acptr->name);
				sendto_ops("dropping server %s (z-lined)",
				    acptr->name);
				sendto_serv_butone(cptr,
				    "GNOTICE :dropping server %s (z-lined)",
				    acptr->name);
				exit_client(acptr, acptr, acptr, "z-lined");

			}
		}
	}

	if (propo == 1)		/* propo is if a ulined server is propagating a z-line
				   this should go after the above check */
		sendto_serv_butone(cptr, ":%s ZLINE %s :%s", parv[0], parv[1],
		    reason ? reason : "");

	check_pings(TStime(), 1);

}


/*
 *  m_unzline                        remove a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 */

int  m_unzline(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char userhost[512 + 2] = "", *in;
	int  result = 0, uline = 0, akill = 0;
	char *mask, *server;

	uline = IsULine(sptr) ? 1 : 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "UNZLINE");
		return -1;
	}


	if (parc < 3 || !uline)
	{
		mask = parv[parc - 1];
		server = NULL;
	}
	else if (parc == 3)
	{
		mask = parv[parc - 2];
		server = parv[parc - 1];
	}

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	/* before we even check ourselves we need to do the uline checks
	   because we aren't supposed to add a z:line if the message is
	   destined to be passed on... */

	if (uline)
	{
		if (parc == 3 && server)
		{
			if (hunt_server(cptr, sptr, ":%s UNZLINE %s %s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			sendto_serv_butone(cptr, ":%s UNZLINE %s", parv[0],
			    parv[1]);

	}


	/* parse the removal mask the same way so an oper can just use
	   the same thing to remove it if they specified *@ or something... */
	if ((in = index(parv[1], '@')))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}
	else
	{
		strcpy(userhost, parv[1]);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}

	akill = 0;
      retry_unzline:

	if (uline == 0)
	{
		result =
		    del_temp_conf(CONF_ZAP, userhost, NULL, NULL, 0, 0, akill);
		if ((result) == KLINE_RET_DELOK)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :temp z:line *@%s removed", me.name,
			    parv[0], userhost);
			sendto_ops("%s removed temp z:line *@%s", parv[0],
			    userhost);
		}
		else if (result == KLINE_RET_PERM)
			sendto_one(sptr,
			    ":%s NOTICE %s :You may not remove permanent z:lines talk to your admin...",
			    me.name, sptr->name);

		else if (result == KLINE_RET_AKILL
		    && !(sptr->umodes & UMODE_SADMIN))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :You may not remove z:lines placed by services...",
			    me.name, sptr->name);
		}
		else if (result == KLINE_RET_AKILL && !akill)
		{
			akill = 1;
			goto retry_unzline;
		}
		else
			sendto_one(sptr,
			    ":%s NOTICE %s :Couldn't find/remove zline for *@%s",
			    me.name, sptr->name, userhost);

	}
	else
	{			/* services did it, services should be able to remove
				   both types...;> */
		if (del_temp_conf(CONF_ZAP, userhost, NULL, NULL, 0, 0,
		    1) == KLINE_RET_DELOK
		    || del_temp_conf(CONF_ZAP, userhost, NULL, NULL, 0, 0,
		    0) == KLINE_RET_DELOK)
		{
			if (MyClient(sptr))
				sendto_one(sptr,
				    "NOTICE %s :temp z:line *@%s removed",
				    parv[0], userhost);
			sendto_ops("%s removed temp z:line *@%s", parv[0],
			    userhost);
		}
		else
			sendto_one(sptr, ":%s NOTICE %s :ERROR Removing z:line",
			    me.name, sptr->name);
	}

}


/* ok, given a mask, our job is to determine
 * wether or not it's a safe mask to banish...
 *
 * userhost= mask to verify
 * ipstat= TRUE  == it's an ip
 *         FALSE == it's a hostname
 *         UNSURE == we need to find out
 * return value
 *         TRUE  == mask is ok
 *         FALSE == mask is not ok
 *        UNSURE == [unused] something went wrong
 */

int advanced_check(char *userhost, int ipstat)
{
	register int retval = TRUE;
	char *up, *p, *thisseg;
	int  numdots = 0, segno = 0, numseg, i = 0;
	char *ipseg[10 + 2];
	char safebuffer[512] = "";	/* buffer strtoken() can mess up to its heart's content...;> */

	strcpy(safebuffer, userhost);

#define userhost safebuffer
#define IP_WILDS_OK(x) ((x)<2? 0 : 1)

	if (ipstat == UNSURE)
	{
		ipstat = TRUE;
		for (; *up; up++)
		{
			if (*up == '.')
				numdots++;
			if (!isdigit(*up) && !ispunct(*up))
			{
				ipstat = FALSE;
				continue;
			}
		}
		if (numdots != 3)
			ipstat = FALSE;
		if (numdots < 1 || numdots > 9)
			return (0);
	}

	/* fill in the segment set */
	{
		int  l = 0;
		for (segno = 0, i = 0, thisseg = strtoken(&p, userhost, ".");
		    thisseg; thisseg = strtoken(&p, NULL, "."), i++)
		{

			l = strlen(thisseg) + 2;
			ipseg[segno] = calloc(1, l);
			strncpy(ipseg[segno++], thisseg, l);
		}
	}
	if (segno < 2 && ipstat == TRUE)
		retval = FALSE;
	numseg = segno;
	if (ipstat == TRUE)
		for (i = 0; i < numseg; i++)
		{
			if (!IP_WILDS_OK(i) && index(ipseg[i], '*')
			    || index(ipseg[i], '?'))
				retval = FALSE;
			/* The person who wrote this function was braindead --Stskeeps */
			/* MyFree(ipseg[i]); */
		}
	else
	{
		int  wildsok = 0;

		for (i = 0; i < numseg; i++)
		{
			/* for hosts, let the mask extent all the way to 
			   the second-level domain... */
			wildsok = 1;
			if (i == numseg || (i + 1) == numseg)
				wildsok = 0;
			if (wildsok == 0 && (index(ipseg[i], '*')
			    || index(ipseg[i], '?')))
			{
				retval = FALSE;
			}
			/* MyFree(ipseg[i]); */
		}


	}

	return (retval);
#undef userhost
#undef IP_WILDS_OK

}

/*
** m_svso - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = nick
**      parv[2] = options
*/

int  m_svso(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	long fLag;

	if (!IsULine(sptr))
		return 0;

	if (parc < 3)
		return 0;

	if (!(acptr = find_client(parv[1], (aClient *)NULL)))
		return 0;

	if (!MyClient(acptr))
	{
		sendto_one(acptr, ":%s SVSO %s %s", parv[0], parv[1], parv[2]);
		return 0;
	}

 	if (*parv[2] == '+')
	{
		int	*i, flag;
		char *m = NULL;
		for (m = (parv[2] + 1); *m; m++)
		{
			for (i = oper_access; flag = *i; i += 2)
			{
				if (*m == (char) *(i + 1))
				{
					acptr->oflag |= flag;
					break;
				}
			}
		}
	}
	if (*parv[2] == '-')
	{
		fLag = acptr->umodes;
		if (IsOper(acptr))
			IRCstats.operators--;
		acptr->umodes &=
		    ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP | UMODE_SERVICES |
		    UMODE_SADMIN | UMODE_ADMIN);
		acptr->umodes &=
		    ~(UMODE_NETADMIN | UMODE_CLIENT |
		    UMODE_FLOOD | UMODE_EYES | UMODE_WHOIS);
		acptr->umodes &=
		    ~(UMODE_KIX | UMODE_FCLIENT | UMODE_HIDING |
		    UMODE_DEAF | UMODE_HIDEOPER);
		acptr->oflag = 0;
		send_umode_out(acptr, acptr, fLag);
	}
}

