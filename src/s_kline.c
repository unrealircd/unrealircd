/*
 *   Unreal Internet Relay Chat Daemon, src/s_kline.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *   File to take care of dynamic K:/G:/Z: lines
 *
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
#include "inet.h"

aTKline *tklines[TKLISTLEN];

extern MODVAR char zlinebuf[BUFSIZE];
int MODVAR spamf_ugly_vchanoverride = 0;

/** tkl hash method.
 * NOTE1: the input value 'c' is assumed to be in range a-z or A-Z!
 * NOTE2: don't blindly change the hashmethod, some things depend on
 *        'z'/'Z' getting the same bucket.
 */
inline int tkl_hash(char c)
{
#ifdef DEBUGMODE
	if ((c >= 'a') && (c <= 'z'))
		return c-'a';
	else if ((c >= 'A') && (c <= 'Z'))
		return c-'A';
	else {
		sendto_realops("[BUG] tkl_hash() called with out of range parameter (c = '%c') !!!", c);
		ircd_log(LOG_ERROR, "[BUG] tkl_hash() called with out of range parameter (c = '%c') !!!", c);
		return 0;
	}
#else
	return (isupper(c) ? c-'A' : c-'a');
#endif
}

/** tkl type to tkl character.
 * NOTE: type is assumed to be valid.
 */
char tkl_typetochar(int type)
{
	if (type & TKL_GLOBAL)
	{
		if (type & TKL_KILL)
			return 'G';
		if (type & TKL_ZAP)
			return 'Z';
		if (type & TKL_SHUN)
			return 's';
		if (type & TKL_KILL)
			return 'G';
		if (type & TKL_SPAMF)
			return 'F';
		if (type & TKL_NICK)
			return 'Q';
	} else {
		if (type & TKL_ZAP)
			return 'z';
		if (type & TKL_KILL)
			return 'k';
		if (type & TKL_SPAMF)
			return 'f';
		if (type & TKL_NICK)
			return 'q';
	}
	sendto_realops("[BUG]: tkl_typetochar(): unknown type 0x%x !!!", type);
	ircd_log(LOG_ERROR, "[BUG] tkl_typetochar(): unknown type 0x%x !!!", type);
	return 0;
}

void tkl_init(void)
{
	memset(tklines, 0, sizeof(tklines));
}

/*
 *  type =  TKL_*
 *	usermask@hostmask
 *	reason
 *	setby = whom set it
 *	expire_at = when to expire - 0 if not to expire
 *	set_at    = was set at
 *  spamf_tkl_duration = duration of *line placed by spamfilter [1]
 *  spamf_tkl_reason = escaped reason field for *lines placed by spamfilter [1]
 *
 *  [1]: only relevant for spamfilters, else ignored (eg 0, NULL).
*/

aTKline *tkl_add_line(int type, char *usermask, char *hostmask, char *reason, char *setby,
                  TS expire_at, TS set_at, TS spamf_tkl_duration, char *spamf_tkl_reason)
{
	aTKline *nl;
	int index;

	nl = (aTKline *) MyMallocEx(sizeof(aTKline));

	if (!nl)
		return NULL;

	nl->type = type;
	nl->expire_at = expire_at;
	nl->set_at = set_at;
	strncpyzt(nl->usermask, usermask, sizeof(nl->usermask));
	nl->hostmask = strdup(hostmask);
	nl->reason = strdup(reason);
	nl->setby = strdup(setby);
	if (type & TKL_SPAMF)
	{
		/* Need to set some additional flags like 'targets' and 'action'.. */
		nl->subtype = spamfilter_gettargets(usermask, NULL);
		nl->ptr.spamf = unreal_buildspamfilter(reason);
		nl->ptr.spamf->action = banact_chartoval(*hostmask);
		nl->expire_at = 0; /* temporary spamfilters are NOT supported! (makes no sense) */
		if (!spamf_tkl_reason)
		{
			/* no exttkl support, use default values... */
			nl->ptr.spamf->tkl_duration = SPAMFILTER_BAN_TIME;
			nl->ptr.spamf->tkl_reason = strdup(unreal_encodespace(SPAMFILTER_BAN_REASON));
		} else {
			nl->ptr.spamf->tkl_duration = spamf_tkl_duration;
			nl->ptr.spamf->tkl_reason = strdup(spamf_tkl_reason); /* already encoded */
		}
		if (nl->subtype & SPAMF_USER)
			loop.do_bancheck_spamf_user = 1;
		if (nl->subtype & SPAMF_AWAY)
			loop.do_bancheck_spamf_away = 1;
	}
	else if (type & TKL_KILL || type & TKL_ZAP || type & TKL_SHUN)
	{
		struct irc_netmask tmp;
		if ((tmp.type = parse_netmask(nl->hostmask, &tmp)) != HM_HOST)
		{
			nl->ptr.netmask = MyMallocEx(sizeof(struct irc_netmask));
			bcopy(&tmp, nl->ptr.netmask, sizeof(struct irc_netmask));
		}
	}
	index = tkl_hash(tkl_typetochar(type));
	AddListItem(nl, tklines[index]);

	return nl;
}

aTKline *tkl_del_line(aTKline *tkl)
{
	aTKline *p, *q;
	int index = tkl_hash(tkl_typetochar(tkl->type));

	for (p = tklines[index]; p; p = p->next)
	{
		if (p == tkl)
		{
			q = p->next;
			MyFree(p->hostmask);
			MyFree(p->reason);
			MyFree(p->setby);
			if (p->type & TKL_SPAMF && p->ptr.spamf)
			{
				regfree(&p->ptr.spamf->expr);
				if (p->ptr.spamf->tkl_reason)
					MyFree(p->ptr.spamf->tkl_reason);
				MyFree(p->ptr.spamf);
			}
			if ((p->type & TKL_KILL || p->type & TKL_ZAP || p->type & TKL_SHUN)
			     && p->ptr.netmask)
				MyFree(p->ptr.netmask);
			DelListItem(p, tklines[index]);
			MyFree(p);
			return q;
		}
	}
	return NULL;
}

/*
 * tkl_check_local_remove_shun:
 * removes shun from currently connected users affected by tmp.
 */
static void tkl_check_local_remove_shun(aTKline *tmp)
{
long i1, i;
char *chost, *cname, *cip;
int  is_ip;
aClient *acptr;

	for (i1 = 0; i1 <= 5; i1++)
	{
		/* winlocal
		for (i = 0; i <= (MAXCONNECTIONS - 1); i++)
		*/
		for (i = 0; i <= LastSlot; ++i)
		{
			if ((acptr = local[i]))
				if (MyClient(acptr) && IsShunned(acptr))
				{
					chost = acptr->sockhost;
					cname = acptr->user->username;

	
					cip = GetIP(acptr);

					if (!(*tmp->hostmask < '0') && (*tmp->hostmask > '9'))
						is_ip = 1;
					else
						is_ip = 0;

					if (is_ip ==
					    0 ? (!match(tmp->hostmask,
					    chost)
					    && !match(tmp->usermask,
					    cname)) : (!match(tmp->
					    hostmask, chost)
					    || !match(tmp->hostmask,
					    cip))
					    && !match(tmp->usermask,
					    cname))
					{
						ClearShunned(acptr);
#ifdef SHUN_NOTICES
						sendto_one(acptr,
						    ":%s NOTICE %s :*** You are no longer shunned",
						    me.name,
						    acptr->name);
#endif
					}
				}
		}
	}
}

aTKline *tkl_expire(aTKline * tmp)
{
	char whattype[512];

	if (!tmp)
		return NULL;

	whattype[0] = 0;

	if ((tmp->expire_at == 0) || (tmp->expire_at > TStime()))
	{
		sendto_ops
		    ("tkl_expire(): expire for not-yet-expired tkline %s@%s",
		    tmp->usermask, tmp->hostmask);
		return (tmp->next);
	}
	/* Using strlcpy here is wasteful, we know it is < 512 */
	if (tmp->type & TKL_GLOBAL)
	{
		if (tmp->type & TKL_KILL)
			strcpy(whattype, "G:Line");
		else if (tmp->type & TKL_ZAP)
			strcpy(whattype, "Global Z:Line");
		else if (tmp->type & TKL_SHUN)
			strcpy(whattype, "Shun");
		else if (tmp->type & TKL_NICK)
			strcpy(whattype, "Global Q:line");
	}
	else
	{
		if (tmp->type & TKL_KILL)
			strcpy(whattype, "K:Line");
		else if (tmp->type & TKL_ZAP)
			strcpy(whattype, "Z:Line");
		else if (tmp->type & TKL_SHUN)
			strcpy(whattype, "Local Shun");
		else if (tmp->type & TKL_NICK)
			strcpy(whattype, "Q:line");
	}
	if (!(tmp->type & TKL_NICK))
	{
		sendto_snomask(SNO_TKL,
		    "*** Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
		    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
		    TStime() - tmp->set_at);
		ircd_log
		    (LOG_TKL, "Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
		    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
		    TStime() - tmp->set_at);
	}
	else if (!(*tmp->usermask == 'H')) /* Q:line but not a hold */
	{
		sendto_snomask(SNO_TKL,
			"*** Expiring %s (%s) made by %s (Reason: %s) set %li seconds ago",
			whattype, tmp->hostmask, tmp->setby, tmp->reason, 
			TStime() - tmp->set_at);
		ircd_log
			(LOG_TKL, "Expiring %s (%s) made by %s (Reason: %s) set %li seconds ago",
			whattype, tmp->hostmask, tmp->setby, tmp->reason, TStime() - tmp->set_at);
	}
	if (tmp->type & TKL_SHUN)
		tkl_check_local_remove_shun(tmp);

	RunHook5(HOOKTYPE_TKL_DEL, NULL, NULL, tmp, 0, NULL);
	return (tkl_del_line(tmp));
}

EVENT(tkl_check_expire)
{
	aTKline *gp, *next;
	TS   nowtime;
	int index;
	
	nowtime = TStime();

	for (index = 0; index < TKLISTLEN; index++)
		for (gp = tklines[index]; gp; gp = next)
		{
			next = gp->next;
			if (gp->expire_at <= nowtime && !(gp->expire_at == 0))
			{
				tkl_expire(gp);
			}
		}
}



/*
	returns <0 if client exists (banned)
	returns 1 if it is excepted
*/

int  find_tkline_match(aClient *cptr, int xx)
{
	aTKline *lp;
	char *chost, *cname, *cip;
	TS   nowtime;
	char msge[1024];
	int	points = 0;
	ConfigItem_except *excepts;
	char host[NICKLEN+USERLEN+HOSTLEN+6], host2[NICKLEN+USERLEN+HOSTLEN+6];
	int match_type = 0;
	int index;
	Hook *tmphook;

	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	nowtime = TStime();
	chost = cptr->sockhost;
	cname = cptr->user ? cptr->user->username : "unknown";
	cip = GetIP(cptr);

	points = 0;
	for (index = 0; index < TKLISTLEN; index++)
	{
		for (lp = tklines[index]; lp; lp = lp->next)
		{
			if ((lp->type & TKL_SHUN) || (lp->type & TKL_SPAMF) || (lp->type & TKL_NICK))
				continue;

			/* If it's tangy and brown, you're in CIDR town! */
			if (lp->ptr.netmask)
			{
				if (match_ip(cptr->ip, NULL, NULL, lp->ptr.netmask) && 
				    !match(lp->usermask, cname))
				{
					points = 1;
					break;
				}
				continue;
			}
			if (!match(lp->usermask, cname) && !match(lp->hostmask, chost))
			{
				points = 1;
				break;
			}
			if (!match(lp->usermask, cname) && !match(lp->hostmask, cip))
			{
				points = 1;
				break;
			}
		}
		if (points)
			break;
	}

	if (points != 1)
		return 1;
	strcpy(host, make_user_host(cname, chost));
	strcpy(host2, make_user_host(cname, cip));
	if (((lp->type & TKL_KILL) || (lp->type & TKL_ZAP)) && !(lp->type & TKL_GLOBAL))
		match_type = CONF_EXCEPT_BAN;
	else
		match_type = CONF_EXCEPT_TKL;
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
		if (excepts->flag.type != match_type || (match_type == CONF_EXCEPT_TKL && 
		    excepts->type != lp->type))
			continue;

		if (excepts->netmask)
		{
			if (match_ip(cptr->ip, host2, excepts->mask, excepts->netmask))
				return 1;		
		} else
		if (!match(excepts->mask, host) || !match(excepts->mask, host2))
			return 1;		
	}

	for (tmphook = Hooks[HOOKTYPE_TKL_EXCEPT]; tmphook; tmphook = tmphook->next)
		if (tmphook->func.intfunc(cptr, lp) > 0)
			return 1;
	
	if ((lp->type & TKL_KILL) && (xx != 2))
	{
		if (lp->type & TKL_GLOBAL)
		{
			ircstp->is_ref++;
			if (GLINE_ADDRESS)
				sendto_one(cptr, ":%s NOTICE %s :*** You are %s from %s (%s)"
					   " Email %s for more information.",
					   me.name, cptr->name,
					   (lp->expire_at ? "banned" : "permanently banned"),
					   ircnetwork, lp->reason, GLINE_ADDRESS);
			else
				sendto_one(cptr, ":%s NOTICE %s :*** You are %s from %s (%s)",
					   me.name, cptr->name,
					   (lp->expire_at ? "banned" : "permanently banned"),
					   ircnetwork, lp->reason);
			ircsprintf(msge, "User has been %s from %s (%s)",
				   (lp->expire_at ? "banned" : "permanently banned"),
				   ircnetwork, lp->reason);
			return (exit_client(cptr, cptr, &me, msge));
		}
		else
		{
			ircstp->is_ref++;
			sendto_one(cptr, ":%s NOTICE %s :*** You are %s from %s (%s)"
				   " Email %s for more information.",
				   me.name, cptr->name,
				   (lp->expire_at ? "banned" : "permanently banned"),
				   me.name, lp->reason, KLINE_ADDRESS);
			ircsprintf(msge, "User is %s (%s)",
				   (lp->expire_at ? "banned" : "permanently banned"),
				   lp->reason);
			return (exit_client(cptr, cptr, &me, msge));

		}
	}
	if (lp->type & TKL_ZAP)
	{
		ircstp->is_ref++;
		ircsprintf(msge, "Z:lined (%s)",lp->reason);
		return exit_client(cptr, cptr, &me, msge);
	}

	return 3;
}

int  find_shun(aClient *cptr)
{
	aTKline *lp;
	char *chost, *cname, *cip;
	TS   nowtime;
	int	points = 0;
	ConfigItem_except *excepts;
	char host[NICKLEN+USERLEN+HOSTLEN+6], host2[NICKLEN+USERLEN+HOSTLEN+6];
	int match_type = 0;
	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	if (IsShunned(cptr))
		return 1;
	if (IsAdmin(cptr))
		return 1;

	nowtime = TStime();
	chost = cptr->sockhost;
	cname = cptr->user ? cptr->user->username : "unknown";
	cip = GetIP(cptr);

	for (lp = tklines[tkl_hash('s')]; lp; lp = lp->next)
	{
		points = 0;
		
		if (!(lp->type & TKL_SHUN))
			continue;

		/* CIDR */
		if (lp->ptr.netmask)
		{
			if (match_ip(cptr->ip, NULL, NULL, lp->ptr.netmask) && 
			    !match(lp->usermask, cname))
			{
				points = 1;
				break;
			}
			continue;
		}

		if (!match(lp->usermask, cname) && !match(lp->hostmask, chost))
		{
			points = 1;
			break;
		}
		if (!match(lp->usermask, cname) && !match(lp->hostmask, cip))
		{
			points = 1;
			break;
		}
		else
			points = 0;
	}

	if (points != 1)
		return 1;
	strcpy(host, make_user_host(cname, chost));
	strcpy(host2, make_user_host(cname, cip));
		match_type = CONF_EXCEPT_TKL;

	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
		if (excepts->flag.type != match_type || (match_type == CONF_EXCEPT_TKL && 
		    excepts->type != lp->type))
			continue;
		if (excepts->netmask)
		{
			if (match_ip(cptr->ip, NULL, NULL, excepts->netmask))
				return 1;		
		}
		else if (!match(excepts->mask, host) || !match(excepts->mask, host2))
			return 1;		
	}
	
	SetShunned(cptr);
	return 2;
}

/** Checks if the user matches a spamfilter of type 'u' (user,
 * nick!user@host:realname ban).
 * Written by: Syzop
 * Assumes: only call for clients, possible assume on local clients [?]
 * Return values: see dospamfilter()
 */
int find_spamfilter_user(aClient *sptr)
{
char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64]; /* n!u@h:r */

	if (IsAnOper(sptr))
		return 0;

	ircsprintf(spamfilter_user, "%s!%s@%s:%s",
		sptr->name, sptr->user->username, sptr->user->realhost, sptr->info);
	return dospamfilter(sptr, spamfilter_user, SPAMF_USER, NULL);
}

aTKline *find_qline(aClient *cptr, char *nick, int *ishold)
{
	aTKline *lp;
	char *chost, *cname, *cip;
	char host[NICKLEN+USERLEN+HOSTLEN+6], hostbuf2[NICKLEN+USERLEN+HOSTLEN+6], *host2 = NULL;
	int	points = 0;
	ConfigItem_except *excepts;
	*ishold = 0;
	if (IsServer(cptr) || IsMe(cptr))
		return NULL;

	for (lp = tklines[tkl_hash('q')]; lp; lp = lp->next)
	{
		points = 0;
		
		if (!(lp->type & TKL_NICK))
			continue;
		if (!match(lp->hostmask, nick))
		{
			points = 1;
			break;	
		}
	}

	if (points != 1)
		return NULL;

	/* It's a services hold */
	if (*lp->usermask == 'H')
	{
		*ishold = 1;
		return lp;
	}

	chost = cptr->user ? cptr->user->realhost : (MyConnect(cptr) ? cptr->sockhost : "unknown");
	cname = cptr->user ? cptr->user->username : "unknown";
	strcpy(host, make_user_host(cname, chost));

	cip = GetIP(cptr);
	if (cip)
	{
		strcpy(hostbuf2, make_user_host(cname, cip));
		host2 = hostbuf2;
	}

	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next)
	{
		if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != TKL_NICK)
			continue;
		if (excepts->netmask)
		{
			if (MyConnect(cptr) && match_ip(cptr->ip, NULL, NULL, excepts->netmask))
				return NULL;
		} else
		if (!match(excepts->mask, host) || (host2 && !match(excepts->mask, host2)))
			return NULL;
	}
	return lp;
}


int  find_tkline_match_zap(aClient *cptr)
{
	aTKline *lp;
	char *cip;
	TS   nowtime;
	char msge[1024];
	ConfigItem_except *excepts;
	Hook *tmphook;
	
	if (IsServer(cptr) || IsMe(cptr))
		return -1;

	nowtime = TStime();
	cip = GetIP(cptr);

	for (lp = tklines[tkl_hash('z')]; lp; lp = lp->next)
	{
		if (lp->type & TKL_ZAP)
		{
			if ((lp->ptr.netmask && match_ip(cptr->ip, NULL, NULL, lp->ptr.netmask))
			    || !match(lp->hostmask, cip))
			{

				for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
					if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != lp->type)
						continue;
					if (excepts->netmask)
					{
						if (match_ip(cptr->ip, NULL, NULL, excepts->netmask))
							return -1;		
					} else if (!match(excepts->mask, cip))
						return -1;		
				}
				for (tmphook = Hooks[HOOKTYPE_TKL_EXCEPT]; tmphook; tmphook = tmphook->next)
					if (tmphook->func.intfunc(cptr, lp) > 0)
						return -1;

				ircstp->is_ref++;
				ircsprintf(msge,
				    "ERROR :Closing Link: [%s] Z:Lined (%s)\r\n",
#ifndef INET6
				    inetntoa((char *)&cptr->ip), lp->reason);
#else
				    inet_ntop(AF_INET6, (char *)&cptr->ip,
				    mydummy, MYDUMMY_SIZE), lp->reason);
#endif
				strlcpy(zlinebuf, msge, sizeof zlinebuf);
				return (1);
			}
		}
	}
	return -1;
}

#define BY_MASK 0x1
#define BY_REASON 0x2
#define NOT_BY_MASK 0x4
#define NOT_BY_REASON 0x8
#define BY_SETBY 0x10
#define NOT_BY_SETBY 0x20

typedef struct {
	int flags;
	char *mask;
	char *reason;
	char *setby;
} TKLFlag;

void parse_tkl_para(char *para, TKLFlag *flag)
{
	static char paratmp[512]; /* <- copy of para, because it gets fragged by strtok() */
	char *flags, *tmp;
	char what = '+';

	strncpyzt(paratmp, para, sizeof(paratmp));
	flags = strtok(paratmp, " ");

	bzero(flag, sizeof(TKLFlag));
	for (; *flags; flags++)
	{
		switch (*flags)
		{
			case '+':
				what = '+';
				break;
			case '-':
				what = '-';
				break;
			case 'm':
				if (flag->mask || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_MASK;
				else
					flag->flags |= NOT_BY_MASK;
				flag->mask = tmp;
				break;
			case 'r':
				if (flag->reason || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_REASON;
				else
					flag->flags |= NOT_BY_REASON;
				flag->reason = tmp;
				break;
			case 's':
				if (flag->setby || !(tmp = strtok(NULL, " ")))
					continue;
				if (what == '+')
					flag->flags |= BY_SETBY;
				else
					flag->flags |= NOT_BY_SETBY;
				flag->setby = tmp;
				break;
		}
	}
}	

void tkl_stats(aClient *cptr, int type, char *para)
{
	aTKline *tk;
	TS   curtime;
	TKLFlag tklflags;
	int index;
	/*
	   We output in this row:
	   Glines,GZlines,KLine, ZLIne
	   Character:
	   G, Z, K, z
	 */

	if (!BadPtr(para))
		parse_tkl_para(para, &tklflags);
	tkl_check_expire(NULL);
	curtime = TStime();
	for (index = 0; index < TKLISTLEN; index++)
	 for (tk = tklines[index]; tk; tk = tk->next)
	 {
		if (type && tk->type != type)
			continue;
		if (!BadPtr(para))
		{
			if (tklflags.flags & BY_MASK)
			{
				if (tk->type & TKL_NICK)
				{
					if (match(tklflags.mask, tk->hostmask))
						continue;
				}
				else if (match(tklflags.mask, make_user_host(tk->usermask,
					tk->hostmask)))
					continue;
			}
			if (tklflags.flags & NOT_BY_MASK)
			{
				if (tk->type & TKL_NICK)
				{
					if (!match(tklflags.mask, tk->hostmask))
						continue;
				}
				else if (!match(tklflags.mask, make_user_host(tk->usermask,
					tk->hostmask)))
					continue;
			}
			if (tklflags.flags & BY_REASON)
				if (match(tklflags.reason, tk->reason))
					continue;
			if (tklflags.flags & NOT_BY_REASON)
				if (!match(tklflags.reason, tk->reason))
					continue;
			if (tklflags.flags & BY_SETBY)
				if (match(tklflags.setby, tk->setby))
					continue;
			if (tklflags.flags & NOT_BY_SETBY)
				if (!match(tklflags.setby, tk->setby))
					continue;
		}
		if (tk->type == (TKL_KILL | TKL_GLOBAL))
		{
			sendto_one(cptr, rpl_str(RPL_STATSGLINE), me.name,
			    cptr->name, 'G', tk->usermask, tk->hostmask,
			    (tk->expire_at !=
			    0) ? (tk->expire_at - curtime) : 0,
			    (curtime - tk->set_at), tk->setby, tk->reason);
		}
		if (tk->type == (TKL_ZAP | TKL_GLOBAL))
		{
			sendto_one(cptr, rpl_str(RPL_STATSGLINE), me.name,
			    cptr->name, 'Z', tk->usermask, tk->hostmask,
			    (tk->expire_at !=
			    0) ? (tk->expire_at - curtime) : 0,
			    (curtime - tk->set_at), tk->setby, tk->reason);
		}
		if (tk->type == (TKL_SHUN | TKL_GLOBAL))
		{
			sendto_one(cptr, rpl_str(RPL_STATSGLINE), me.name,
			    cptr->name, 's', tk->usermask, tk->hostmask,
			    (tk->expire_at !=
			    0) ? (tk->expire_at - curtime) : 0,
			    (curtime - tk->set_at), tk->setby, tk->reason);
		}
		if (tk->type == (TKL_KILL))
		{
			sendto_one(cptr, rpl_str(RPL_STATSGLINE), me.name,
			    cptr->name, 'K', tk->usermask, tk->hostmask,
			    (tk->expire_at !=
			    0) ? (tk->expire_at - curtime) : 0,
			    (curtime - tk->set_at), tk->setby, tk->reason);
		}
		if (tk->type == (TKL_ZAP))
		{
			sendto_one(cptr, rpl_str(RPL_STATSGLINE), me.name,
			    cptr->name, 'z', tk->usermask, tk->hostmask,
			    (tk->expire_at !=
			    0) ? (tk->expire_at - curtime) : 0,
			    (curtime - tk->set_at), tk->setby, tk->reason);
		}
		if (tk->type & TKL_SPAMF)
		{
			sendto_one(cptr, rpl_str(RPL_STATSSPAMF), me.name,
				cptr->name,
				(tk->type & TKL_GLOBAL) ? 'F' : 'f',
				spamfilter_target_inttostring(tk->subtype),
				banact_valtostring(tk->ptr.spamf->action),
				(tk->expire_at != 0) ? (tk->expire_at - curtime) : 0,
				curtime - tk->set_at,
				tk->ptr.spamf->tkl_duration, tk->ptr.spamf->tkl_reason,
				tk->setby,
				tk->reason);
		}
		if (tk->type & TKL_NICK)
			sendto_one(cptr, rpl_str(RPL_STATSQLINE), me.name,
				cptr->name, (tk->type & TKL_GLOBAL) ? 'Q' : 'q',
				tk->hostmask, (tk->expire_at != 0) ? (tk->expire_at - curtime) : 0,
				curtime - tk->set_at, tk->setby, tk->reason); 
	 }

}

void tkl_synch(aClient *sptr)
{
	aTKline *tk;
	char typ = 0;
	int index;
	
	for (index = 0; index < TKLISTLEN; index++)
		for (tk = tklines[index]; tk; tk = tk->next)
		{
			if (tk->type & TKL_GLOBAL)
			{
				if (tk->type & TKL_KILL)
					typ = 'G';
				if (tk->type & TKL_ZAP)
					typ = 'Z';
				if (tk->type & TKL_SHUN)
					typ = 's';
				if (tk->type & TKL_SPAMF)
					typ = 'F';
				if (tk->type & TKL_NICK)
					typ = 'Q';
				if ((tk->type & TKL_SPAMF) && (sptr->proto & PROTO_TKLEXT))
				{
					sendto_one(sptr,
					    ":%s %s + %c %s %s %s %li %li %li %s :%s", me.name,
					    IsToken(sptr) ? TOK_TKL : MSG_TKL,
					    typ,
					    tk->usermask, tk->hostmask, tk->setby,
					    tk->expire_at, tk->set_at,
					    tk->ptr.spamf->tkl_duration, tk->ptr.spamf->tkl_reason,
					    tk->reason);
				} else
					sendto_one(sptr,
					    ":%s %s + %c %s %s %s %li %li :%s", me.name,
					    IsToken(sptr) ? TOK_TKL : MSG_TKL,
					    typ,
					    tk->usermask ? tk->usermask : "*", tk->hostmask, tk->setby,
					    tk->expire_at, tk->set_at, tk->reason);
			}
		}
}

/*
 * m_tkl:
 * HISTORY:
 * This was originall called Timed KLines, but today it's
 * used by various *line types eg: zline, gline, gzline, shun,
 * but also by spamfilter etc...
 * USAGE:
 * This routine is used both internally by the ircd (to
 * for example add local klines, zlines, etc) and over the
 * network (glines, gzlines, spamfilter, etc).
 *           add:      remove:    spamfilter:    spamfilter+TKLEXT  sqline:
 * parv[ 1]: +         -          +/-            +                  +/-
 * parv[ 2]: type      type       type           type               type
 * parv[ 3]: user      user       target         target             hold
 * parv[ 4]: host      host       action         action             host
 * parv[ 5]: setby     removedby  (un)setby      setby              setby
 * parv[ 6]: expire_at            expire_at (0)  expire_at (0)      expire_at
 * parv[ 7]: set_at               set_at         set_at             set_at
 * parv[ 8]: reason               regex          tkl duration       reason
 * parv[ 9]:                                     tkl reason [A]        
 * parv[10]:                                     regex              
 *
 * [A] tkl reason field must be escaped by caller [eg: use unreal_encodespace()
 *     if m_tkl is called internally].
 *
 */
int m_tkl(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aTKline *tk;
	int  type;
	int  found = 0;
	char gmt[256], gmt2[256];
	char txt[256];
	TS   expiry_1, setat_1, spamf_tklduration = 0;
	char *reason = NULL;

	if (!IsServer(sptr) && !IsOper(sptr) && !IsMe(sptr))
		return 0;
	if (parc < 2)
		return 0;

	tkl_check_expire(NULL);

	switch (*parv[1])
	{
	  case '+':
	  {
		  /* we relay on servers to be failsafe.. */
		  if (!IsServer(sptr) && !IsMe(sptr))
			  return 0;
		  if (parc < 9)
			  return 0;

		  if (parv[2][0] == 'G')
			  type = TKL_KILL | TKL_GLOBAL;
		  else if (parv[2][0] == 'Z')
			  type = TKL_ZAP | TKL_GLOBAL;
		  else if (parv[2][0] == 'z')
			  type = TKL_ZAP;
		  else if (parv[2][0] == 'k')
			  type = TKL_KILL;
		  else if (parv[2][0] == 's')
			  type = TKL_SHUN | TKL_GLOBAL;
		  else if (parv[2][0] == 'f')
			  type = TKL_SPAMF;
		  else if (parv[2][0] == 'F')
			  type = TKL_SPAMF | TKL_GLOBAL;
		  else if (parv[2][0] == 'Q')
			  type = TKL_NICK | TKL_GLOBAL;
		  else if (parv[2][0] == 'q')
			  type = TKL_NICK;
		  else
			  return 0;

		  expiry_1 = atol(parv[6]);
		  setat_1 = atol(parv[7]);
		  reason = parv[8];

		  found = 0;
		  if ((type & TKL_SPAMF) && (parc >= 11))
		  {
		  	reason = parv[10];
		  	spamf_tklduration = config_checkval(parv[8], CFG_TIME); /* was: atol(parv[8]); */
		  }
		  for (tk = tklines[tkl_hash(parv[2][0])]; tk; tk = tk->next)
		  {
			  if (tk->type == type)
			  {
				  if ((tk->type & TKL_NICK) && !stricmp(tk->hostmask, parv[4]))
				  {
					  found = 1;
					  break;
				  }
				  else if (!strcmp(tk->hostmask, parv[4]) && !strcmp(tk->usermask, parv[3]) &&
				     (!(type & TKL_SPAMF) || !stricmp(tk->reason, reason)))
				  {
					  found = 1;
					  break;
				  }
			  }
		  }
		  /* *:Line already exists! */
		  if (found == 1)
		  {
		  		/* SYZTAG: TODO: check for tklreason/tklduration differnces */
				/* do they differ in ANY way? */
				if (type & TKL_NICK)
				{
					/* for sqline: usermask = H overrides */

					if (*parv[3] == 'H')
						*tk->usermask = 'H';
				}

				if ((setat_1 != tk->set_at) || (expiry_1 != tk->expire_at) ||
				    strcmp(tk->reason, reason) || strcmp(tk->setby, parv[5]))
				{
					/* here's how it goes:
					 * set_at: oldest wins
			 		 * expire_at: longest wins
			 		 * reason: highest strcmp wins
			 		 * setby: highest strcmp wins
			 		 * We broadcast the result of this back to all servers except
			 		 * cptr's direction, because cptr will do the same thing and
			 		 * send it back to his servers (except us)... no need for a
			 		 * double networkwide flood ;p. -- Syzop
			 		 */
					tk->set_at = MIN(tk->set_at, setat_1);
					if (!tk->expire_at || !expiry_1)
			 			tk->expire_at = 0;
			 		else
			 			tk->expire_at = MAX(tk->expire_at, expiry_1);
			 		if (strcmp(tk->reason, reason) < 0)
			 		{
			 			MyFree(tk->reason);
			 			tk->reason = strdup(reason);
			 		}
			 		if (strcmp(tk->setby, parv[5]) < 0)
			 		{
			 			MyFree(tk->setby);
			 			tk->setby = strdup(parv[5]);
			 		}
					if (tk->type & TKL_NICK)
					{
						if (!(*tk->usermask) == 'H')
					 		sendto_snomask(SNO_JUNK, "tkl update for %s/reason='%s'/by=%s/set=%ld/expire=%ld [causedby: %s]",
					 			tk->hostmask, tk->reason, tk->setby, tk->set_at, tk->expire_at, sptr->name);
					}
					else
				 		sendto_snomask(SNO_JUNK, "tkl update for %s@%s/reason='%s'/by=%s/set=%ld/expire=%ld [causedby: %s]",
				 			tk->usermask, tk->hostmask, tk->reason, tk->setby, tk->set_at, tk->expire_at, sptr->name);
				 	if ((parc == 11) && (type & TKL_SPAMF))
					{
						/* I decided to only send updates to OPT_TKLEXT in this case,
						 * it's pretty useless to send it also to OPT_NOT_TKLEXT because
						 * spamfilter entries are permanent (no expire time), the only stuff
						 * that can differ for non-opt is the 'setby' and 'setat' field...
						 */
				 		sendto_serv_butone_token_opt(cptr, OPT_TKLEXT, sptr->name,
				 			MSG_TKL, TOK_TKL,
				 			"%s %s %s %s %s %ld %ld %ld %s :%s",
				 			parv[1], parv[2], parv[3], parv[4],
				 			tk->setby, tk->expire_at, tk->set_at, tk->ptr.spamf->tkl_duration,
				 			tk->ptr.spamf->tkl_reason, tk->reason);
				 	} 
					else if (type & TKL_GLOBAL)
				 		sendto_serv_butone(cptr,
				 			":%s TKL %s %s %s %s %s %ld %ld :%s", sptr->name,
				 			parv[1], parv[2], parv[3], parv[4],
				 			tk->setby, tk->expire_at, tk->set_at, tk->reason);
		      }
			  return 0;
		  }

		  /* there is something fucked here? */
		  if ((type & TKL_SPAMF) && (parc >= 11))
			tk = tkl_add_line(type, parv[3], parv[4], reason, parv[5],
				expiry_1, setat_1, spamf_tklduration, parv[9]);
		  else
			tk = tkl_add_line(type, parv[3], parv[4], reason, parv[5],
				expiry_1, setat_1, 0, NULL);

		  if (tk)
		  	RunHook5(HOOKTYPE_TKL_ADD, cptr, sptr, tk, parc, parv);

		  strncpyzt(gmt, asctime(gmtime((TS *)&setat_1)), sizeof(gmt));
		  strncpyzt(gmt2, asctime(gmtime((TS *)&expiry_1)), sizeof(gmt2));
		  iCstrip(gmt);
		  iCstrip(gmt2);
		  switch (type)
		  {
		    case TKL_KILL:
			    strcpy(txt, "K:Line");
			    break;
		    case TKL_ZAP:
			    strcpy(txt, "Z:Line");
			    break;
		    case TKL_KILL | TKL_GLOBAL:
			    strcpy(txt, "G:Line");
			    break;
		    case TKL_ZAP | TKL_GLOBAL:
			    strcpy(txt, "Global Z:line");
			    break;
		    case TKL_SHUN | TKL_GLOBAL:
			    strcpy(txt, "Shun");
			    break;
		    case TKL_NICK | TKL_GLOBAL:
			    strcpy(txt, "Global Q:line");
			    break;
		    case TKL_NICK:
			    strcpy(txt, "Q:line");
			    break;
		    default:
			    strcpy(txt, "Unknown *:Line");
		  }
		  if (type & TKL_SPAMF)
		  {
		  	  char buf[512];
			  snprintf(buf, 512,
			      "Spamfilter added: '%s' [target: %s] [action: %s] [reason: %s] on %s GMT (from %s)",
			      reason, parv[3], banact_valtostring(banact_chartoval(*parv[4])),
			      parc >= 10 ? unreal_decodespace(parv[9]) : SPAMFILTER_BAN_REASON,
			      gmt, parv[5]);
			  sendto_snomask(SNO_TKL, "*** %s", buf);
			  ircd_log(LOG_TKL, "%s", buf);
		  } else {
			char buf[512];
			  if (expiry_1 != 0)
			  {
				if (type & TKL_NICK)
				{
					if (*parv[3] != 'H')
						snprintf(buf, 512, "%s added for %s on %s GMT (from %s to expire at %s GMT: %s)",
							txt, parv[4], gmt, parv[5], gmt2, reason);
				}
				else
					snprintf(buf, 512, "%s added for %s@%s on %s GMT (from %s to expire at %s GMT: %s)",
						txt, parv[3], parv[4], gmt, parv[5], gmt2, reason);
			  }
			  else
			  {
				if (type & TKL_NICK)
				{
					if (*parv[3] != 'H')
						snprintf(buf, 512, "Permanent %s added for %s on %s GMT (from %s: %s)",
							txt, parv[4], gmt, parv[5], reason);
				}
				else
					snprintf(buf, 512, "Permanent %s added for %s@%s on %s GMT (from %s: %s)",
						txt, parv[3], parv[4], gmt, parv[5], reason);
			  }
			if (!((type & TKL_NICK) && *parv[3] == 'H'))
			{
				sendto_snomask(SNO_TKL, "*** %s", buf);
				ircd_log(LOG_TKL, "%s", buf);
			}
		  }
		  loop.do_bancheck = 1;
		  /* Makes check_pings be run ^^  */
		  if (type & TKL_GLOBAL)
		  {
		  	if ((parc == 11) && (type & TKL_SPAMF))
		  	{
				sendto_serv_butone_token_opt(cptr, OPT_TKLEXT, sptr->name,
					MSG_TKL, TOK_TKL,
					"%s %s %s %s %s %s %s %s %s :%s",
					parv[1], parv[2], parv[3], parv[4], parv[5],
					parv[6], parv[7], parv[8], parv[9], parv[10]);
				sendto_serv_butone_token_opt(cptr, OPT_NOT_TKLEXT, sptr->name,
					MSG_TKL, TOK_TKL,
					"%s %s %s %s %s %s %s :%s",
					parv[1], parv[2], parv[3], parv[4], parv[5],
					parv[6], parv[7], parv[10]);
			} else
				sendto_serv_butone(cptr,
					":%s TKL %s %s %s %s %s %s %s :%s", sptr->name,
					parv[1], parv[2], parv[3], parv[4], parv[5],
					parv[6], parv[7], parv[8]);
		  } /* TKL_GLOBAL */
		  return 0;
	  }
	  case '-':
		  if (!IsServer(sptr) && !IsMe(sptr))
			  return 0;
		  if (*parv[2] == 'G')
			  type = TKL_KILL | TKL_GLOBAL;
		  else if (*parv[2] == 'Z')
			  type = TKL_ZAP | TKL_GLOBAL;
		  else if (*parv[2] == 'z')
			  type = TKL_ZAP;
		  else if (*parv[2] == 'k')
			  type = TKL_KILL;
		  else if (*parv[2] == 's')
			  type = TKL_SHUN | TKL_GLOBAL;
		  else if (*parv[2] == 'Q')
			  type = TKL_NICK | TKL_GLOBAL;
		  else if (*parv[2] == 'q')
			  type = TKL_NICK;
		  else if (*parv[2] == 'F')
		  {
		      if (parc < 8)
		      {
		          sendto_realops("[BUG] m_tkl called with bogus spamfilter removal request [F], from=%s, parc=%d",
		          	sptr->name, parc);
		          return 0; /* bogus */
		      }
			  type = TKL_SPAMF | TKL_GLOBAL;
			  if (parc >= 11)
			  	reason = parv[10];
			  else
			  	reason = parv[8];
		  }
		  else if (*parv[2] == 'f')
		  {
		      if (parc < 8)
			  {
		          sendto_realops("[BUG] m_tkl called with bogus spamfilter removal request [f], from=%s, parc=%d",
		          	sptr->name, parc);
		          return 0; /* bogus */
		      }
			  type = TKL_SPAMF;
			  if (parc >= 11)
			  	reason = parv[10];
			  else
			  	reason = parv[8];
		  }
		  else
			  return 0;

		  switch (type)
		  {
		    case TKL_KILL:
			    strcpy(txt, "K:Line");
			    break;
		    case TKL_ZAP:
			    strcpy(txt, "Z:Line");
			    break;
		    case TKL_KILL | TKL_GLOBAL:
			    strcpy(txt, "G:Line");
			    break;
		    case TKL_ZAP | TKL_GLOBAL:
			    strcpy(txt, "Global Z:line");
			    break;
		    case TKL_SHUN | TKL_GLOBAL:
			    strcpy(txt, "Shun");
			    break;
		    case TKL_NICK | TKL_GLOBAL:
			    strcpy(txt, "Global Q:line");
			    break;
		    case TKL_NICK:
			    strcpy(txt, "Q:line");
			    break;
		    default:
			    strcpy(txt, "Unknown *:Line");
		  }

		  found = 0;
		  for (tk = tklines[tkl_hash(parv[2][0])]; tk; tk = tk->next)
		  {
			  if (tk->type == type)
			  {
				int match = 0;
				if (type & TKL_NICK)
				{
					if (!stricmp(tk->hostmask, parv[4]))
						match = 1;
				} else
				if (type & TKL_SPAMF)
				{
					if (!strcmp(tk->hostmask, parv[4]) && !strcmp(tk->usermask, parv[3]) && 
					    !stricmp(tk->reason, reason))
						match = 1;
				} else /* all other types... */
				if (!stricmp(tk->hostmask, parv[4]) && !stricmp(tk->usermask, parv[3]))
					match = 1;

				  if (match)
				  {
					  strncpyzt(gmt, asctime(gmtime((TS *)&tk->set_at)), sizeof(gmt));
					  iCstrip(gmt);
					  /* broadcast remove msg to opers... */
					  if (type & TKL_NICK)
					  {
						if (!(*parv[3] == 'H'))
						{
							sendto_snomask(SNO_TKL, "%s removed %s %s (set at %s - reason: %s)",
								parv[5], txt, tk->hostmask, gmt, tk->reason);
							ircd_log(LOG_TKL, "%s removed %s %s (set at %s - reason: %s)",
								parv[5], txt, tk->hostmask, gmt, tk->reason);
						}
					  }
					  else if (type & TKL_SPAMF)
					  {
					  	  sendto_snomask(SNO_TKL, "%s removed Spamfilter '%s' (set at %s)",
					  	     parv[5], tk->reason, gmt);
					  	  ircd_log(LOG_TKL, "%s removed Spamfilter '%s' (set at %s)",
					  	     parv[5], tk->reason, gmt);
					  } else {
						  sendto_snomask(SNO_TKL,
						      "%s removed %s %s@%s (set at %s - reason: %s)",
						      parv[5], txt, tk->usermask,
						      tk->hostmask, gmt, tk->reason);
						  ircd_log(LOG_TKL, "%s removed %s %s@%s (set at %s - reason: %s)",
						      parv[5], txt, tk->usermask, tk->hostmask,
						      gmt, tk->reason);
					  }
					  if (type & TKL_SHUN)
					      tkl_check_local_remove_shun(tk);
					  RunHook5(HOOKTYPE_TKL_DEL, cptr, sptr, tk, parc, parv);
					  tkl_del_line(tk);
					  if (type & TKL_GLOBAL)
					  {
					 	  if (parc < 8)
							  sendto_serv_butone(cptr,
							      ":%s TKL %s %s %s %s %s",
							      sptr->name, parv[1], parv[2], parv[3], parv[4], parv[5]);
						  else
							  sendto_serv_butone(cptr,
							      ":%s TKL %s %s %s %s %s %s %s :%s",
							      sptr->name, parv[1], parv[2], parv[3], parv[4], parv[5],
							      parv[6], parv[7], reason);
				      }
					  break;
				  }
			  }
		  }

		  break;

	  case '?':
		  if (IsAnOper(sptr))
			  tkl_stats(sptr,0,NULL);
	}
	return 0;
}

/* execute_ban_action, a tkl helper. (Syzop/2003)
 * PARAMETERS:
 * sptr:     the client which is affected
 * action:   type of ban (BAN_ACT*)
 * reason:   ban reason
 * duration: duration of ban in seconds
 * WHAT IT DOES:
 * This function will shun/kline/gline/zline the user.
 * If the action field is 0 (BAN_ACT_KILL) the user is
 * just killed (and the time parameter is ignored).
 * ASSUMES:
 * This function assumes that sptr is locally connected.
 * RETURN VALUE:
 * -1 in case of block/tempshun, FLUSH_BUFFER in case of
 * kill/zline/gline/etc.. (you should NOT read from 'sptr'
 * after you got FLUSH_BUFFER!!!)
 */
int place_host_ban(aClient *sptr, int action, char *reason, long duration)
{
	switch(action)
	{
		case BAN_ACT_TEMPSHUN:
			/* We simply mark this connection as shunned and do not add a ban record */
			sendto_snomask(SNO_TKL, "Temporary shun added at user %s (%s@%s) [%s]",
				sptr->name,
				sptr->user ? sptr->user->username : "unknown",
				sptr->user ? sptr->user->realhost : GetIP(sptr),
				reason);
			SetShunned(sptr);
			break;
		case BAN_ACT_SHUN:
		case BAN_ACT_KLINE:
		case BAN_ACT_ZLINE:
		case BAN_ACT_GLINE:
		case BAN_ACT_GZLINE:
		{
			char hostip[128], mo[100], mo2[100];
			char *tkllayer[9] = {
				me.name,	/*0  server.name */
				"+",		/*1  +|- */
				"?",		/*2  type */
				"*",		/*3  user */
				NULL,		/*4  host */
				NULL,
				NULL,		/*6  expire_at */
				NULL,		/*7  set_at */
				NULL		/*8  reason */
			};

			strlcpy(hostip, GetIP(sptr), sizeof(hostip));

			if (action == BAN_ACT_KLINE)
				tkllayer[2] = "k";
			else if (action == BAN_ACT_ZLINE)
				tkllayer[2] = "z";
			else if (action == BAN_ACT_GZLINE)
				tkllayer[2] = "Z";
			else if (action == BAN_ACT_GLINE)
				tkllayer[2] = "G";
			else if (action == BAN_ACT_SHUN)
				tkllayer[2] = "s";
			tkllayer[4] = hostip;
			tkllayer[5] = me.name;
			if (!duration)
				strcpy(mo, "0"); /* perm */
			else
				ircsprintf(mo, "%li", duration + TStime());
			ircsprintf(mo2, "%li", TStime());
			tkllayer[6] = mo;
			tkllayer[7] = mo2;
			tkllayer[8] = reason;
			m_tkl(&me, &me, 9, tkllayer);
			if (action == BAN_ACT_SHUN)
			{
				find_shun(sptr);
				return -1;
			} else
				return find_tkline_match(sptr, 0);
		}
		case BAN_ACT_KILL:
		default:
			return exit_client(sptr, sptr, sptr, reason);
	}
	return -1;
}

/** Checks if 'target' is on the spamfilter exception list.
 * RETURNS 1 if found in list, 0 if not.
 */
int target_is_spamexcept(char *target)
{
SpamExcept *e;

	for (e = iConf.spamexcept; e; e = e->next)
	{
		if (!match(e->name, target))
			return 1;
	}
	return 0;
}

/** dospamfilter: executes the spamfilter onto the string.
 * str:		the text (eg msg text, notice text, part text, quit text, etc
 * type:	the spamfilter type (SPAMF_*)
 * RETURN VALUE:
 * 0 if not matched, non-0 if it should be blocked.
 * Return value can be FLUSH_BUFFER (-2) which means 'sptr' is
 * _NOT_ valid anymore so you should return immediately
 * (like from m_message, m_part, m_quit, etc).
 */
 
int dospamfilter(aClient *sptr, char *str_in, int type, char *target)
{
aTKline *tk;
char *str;

	if (type == SPAMF_USER)
		str = str_in;
	else
		str = (char *)StripControlCodes(str_in);

	/* (note: using sptr->user check here instead of IsPerson()
	 * due to SPAMF_USER where user isn't marked as client/person yet.
	 */
	if (!sptr->user || IsAnOper(sptr) || IsULine(sptr))
		return 0;

	for (tk = tklines[tkl_hash('F')]; tk; tk = tk->next)
	{
		if (!(tk->subtype & type))
			continue;
		if (!regexec(&tk->ptr.spamf->expr, str, 0, NULL, 0))
		{
			/* matched! */
			char buf[1024];
			char targetbuf[48];
			if (target) {
				targetbuf[0] = ' ';
				strlcpy(targetbuf+1, target, sizeof(targetbuf)-1); /* cut it off */
			} else
				targetbuf[0] = '\0';

			/* Hold on.. perhaps it's on the exceptions list... */
			if (target && target_is_spamexcept(target))
				return 0; /* No problem! */

			ircsprintf(buf, "[Spamfilter] %s!%s@%s matches filter '%s': [%s%s: '%s'] [%s]",
				sptr->name, sptr->user->username, sptr->user->realhost,
				tk->reason,
				spamfilter_inttostring_long(type), targetbuf, str,
				unreal_decodespace(tk->ptr.spamf->tkl_reason));

			sendto_snomask(SNO_SPAMF, "%s", buf);
			sendto_serv_butone_token(NULL, me.name, MSG_SENDSNO, TOK_SENDSNO, "S :%s", buf);
			ircd_log(LOG_SPAMFILTER, "%s", buf);

			if (tk->ptr.spamf->action == BAN_ACT_BLOCK)
			{
				switch(type)
				{
					case SPAMF_USERMSG:
					case SPAMF_USERNOTICE:
						sendnotice(sptr, "Message to %s blocked: %s",
							target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
						break;
					case SPAMF_CHANMSG:
					case SPAMF_CHANNOTICE:
						sendto_one(sptr, ":%s 404 %s %s :Message blocked: %s",
							me.name, sptr->name, target,
							unreal_decodespace(tk->ptr.spamf->tkl_reason));
						break;
					case SPAMF_DCC:
						sendnotice(sptr, "DCC to %s blocked: %s",
							target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
						break;
					case SPAMF_AWAY:
						/* hack to deal with 'after-away-was-set-filters' */
						if (sptr->user->away && !strcmp(str_in, sptr->user->away))
						{
							/* free away & broadcast the unset */
							MyFree(sptr->user->away);
							sptr->user->away = NULL;
							sendto_serv_butone_token(sptr, sptr->name, MSG_AWAY, TOK_AWAY, "");
						}
						break;
					case SPAMF_TOPIC:
						//...
						sendnotice(sptr, "Setting of topic on %s to that text is blocked: %s",
							target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
						break;
					default:
						break;
				}
				return -1;
			} else
			if (tk->ptr.spamf->action == BAN_ACT_DCCBLOCK)
			{
				if (type == SPAMF_DCC)
				{
					sendnotice(sptr, "DCC to %s blocked: %s",
						target, unreal_decodespace(tk->ptr.spamf->tkl_reason));
					sendnotice(sptr, "*** You have been blocked from sending files, "
					           "reconnect to regain permission to send files");
					sptr->flags |= FLAGS_DCCBLOCK;
				}
				return -1;
			} else
			if (tk->ptr.spamf->action == BAN_ACT_VIRUSCHAN)
			{
				char *xparv[3], chbuf[CHANNELLEN + 16];
				aChannel *chptr;
				int ret;
				
				if (IsVirus(sptr)) /* Already tagged */
					return 0;
				ircsprintf(buf, "0,%s", SPAMFILTER_VIRUSCHAN);
				xparv[0] = sptr->name;
				xparv[1] = buf;
				xparv[2] = NULL;
				/* RECURSIVE CAUTION in case we ever add blacklisted chans */
				spamf_ugly_vchanoverride = 1;
				ret = do_cmd(sptr, sptr, "JOIN", 2, xparv);
				spamf_ugly_vchanoverride = 0;
				if (ret == FLUSH_BUFFER)
					return FLUSH_BUFFER; /* don't ask me how we could have died... */
				sendnotice(sptr, "You are now restricted to talking in %s: %s",
					SPAMFILTER_VIRUSCHAN, unreal_decodespace(tk->ptr.spamf->tkl_reason));
				/* todo: send notice to channel? */
				chptr = find_channel(SPAMFILTER_VIRUSCHAN, NULL);
				if (chptr)
				{
					ircsprintf(chbuf, "@%s", chptr->chname);
					ircsprintf(buf, "[Spamfilter] %s matched filter '%s' [%s%s] [%s]",
						sptr->name, tk->reason, spamfilter_inttostring_long(type), targetbuf,
						unreal_decodespace(tk->ptr.spamf->tkl_reason));
					sendto_channelprefix_butone_tok(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
						MSG_NOTICE, TOK_NOTICE, chbuf, buf, 0);
				}
				SetVirus(sptr);
				return -1;
			} else
				return place_host_ban(sptr, tk->ptr.spamf->action,
					unreal_decodespace(tk->ptr.spamf->tkl_reason), tk->ptr.spamf->tkl_duration);
		}
	}
	return 0;
}
