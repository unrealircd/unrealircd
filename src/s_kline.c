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


aTKline *tklines = NULL;

#define AllocCpy(x,y) x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)
#define GFreeStr(x) MyFree((char *) x)
#define GFreeGline(x) MyFree((aGline *) x)
extern char zlinebuf[];

/*

 *     type =  TKL_*
 *	usermask@hostmask
 *	reason
 *	setby = whom set it
 *	expire_at = when to expire - 0 if not to expire
 *	set_at    = was set at
*/

int  tkl_add_line(int type, char *usermask, char *hostmask, char *reason, char *setby, TS expire_at, TS set_at)
{
	aTKline *nl;

	nl = (aTKline *) MyMalloc(sizeof(aTKline));

	if (!nl)
		return -1;

	nl->type = type;
	nl->expire_at = expire_at;
	nl->set_at = set_at;
	AllocCpy(nl->usermask, usermask);
	AllocCpy(nl->hostmask, hostmask);
	AllocCpy(nl->reason, reason);
	AllocCpy(nl->setby, setby);
	AddListItem(nl, tklines);
}

aTKline *tkl_del_line(aTKline *tkl)
{
	aTKline *p, *q;

	for (p = tklines; p; p = p->next)
	{
		if (p == tkl)
		{
			q = p->next;
			GFreeStr(p->hostmask);
			GFreeStr(p->usermask);
			GFreeStr(p->reason);
			GFreeStr(p->setby);
			DelListItem(p, tklines);
			MyFree((aTKline *) p);
			return q;
		}
	}
	return NULL;

}

aTKline *tkl_expire(aTKline * tmp)
{
	char whattype[512];
	long i, i1;
	char *chost, *cname, *cip;
	int  is_ip;
	aClient *acptr;
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

	if (tmp->type & TKL_GLOBAL)
	{
		if (tmp->type & TKL_KILL)
		{
			strcpy(whattype, "G:Line");
		}
		else if (tmp->type & TKL_ZAP)
		{
			strcpy(whattype, "Global Z:Line");
		}
		else if (tmp->type & TKL_SHUN)
			strcpy(whattype, "Shun");
	}
	else
	{
		if (tmp->type & TKL_KILL)
		{
			strcpy(whattype, "Timed K:Line");
		}
		else if (tmp->type & TKL_ZAP)
		{
			strcpy(whattype, "Timed Z:Line");
		}
		else if (tmp->type & TKL_SHUN)
			strcpy(whattype, "Local Shun");
	}
	sendto_snomask(SNO_TKL,
	    "*** Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
	    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
	    TStime() - tmp->set_at);

	ircd_log
	    (LOG_TKL, "Expiring %s (%s@%s) made by %s (Reason: %s) set %li seconds ago",
	    whattype, tmp->usermask, tmp->hostmask, tmp->setby, tmp->reason,
	    TStime() - tmp->set_at);

	if (tmp->type & TKL_SHUN)
	{
		for (i1 = 0; i1 <= 5; i1++)
		{
			/* winlocal
			for (i = 0; i <= (MAXCONNECTIONS - 1); i++)
			*/
			for (i = 0; i <= LastSlot; ++i)
			{
				if (acptr = local[i])
					if (MyClient(acptr) && IsShunned(acptr))
					{
						chost = acptr->sockhost;
						cname = acptr->user->username;

	
						cip = (char *)Inet_ia2p(&acptr->ip);

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


	return (tkl_del_line(tmp));
}

EVENT(tkl_check_expire)
{
	aTKline *gp, *next;
	TS   nowtime;

	nowtime = TStime();

	for (gp = tklines; gp; gp = next)
	{
		next = gp->next;
		if (gp->expire_at <= nowtime && !(gp->expire_at == 0))
		{
			tkl_expire(gp);
		}
	}
}



/*
	returns -1 if no tkline found
	returns >= 0 if client exits
*/

int  find_tkline_match(aClient *cptr, int xx)
{
	aTKline *lp;
	char *chost, *cname, *cip;
	TS   nowtime;
	int  is_ip;
	char msge[1024];
	char gmt2[256];
	int	points = 0;
	ConfigItem_except *excepts;
	char host[NICKLEN+USERLEN+HOSTLEN+6], host2[NICKLEN+USERLEN+HOSTLEN+6];
	if (IsServer(cptr) || IsMe(cptr))
		return -1;


	nowtime = TStime();
	chost = cptr->sockhost;
	cname = cptr->user ? cptr->user->username : "unknown";
	cip = (char *)Inet_ia2p(&cptr->ip);


	for (lp = tklines; lp; lp = lp->next)
	{
		points = 0;

		if (!match(lp->usermask, cname) && !match(lp->hostmask, chost))
			points = 1;
		if (!match(lp->usermask, cname) && !match(lp->hostmask, cip))
			points = 1;
		if (points == 1)
			break;
		else
			points = 0;
	}

	if (points != 1)
		return -1;
	strcpy(host, make_user_host(cname, chost));
	strcpy(host2, make_user_host(cname, cip));
	for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
		if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != lp->type)
			continue;
		if (!match(excepts->mask, host) || !match(excepts->mask, host2))
			return -1;		
	}
	
	if ((lp->type & TKL_KILL) && (xx != 2))
	{
		if (lp->type & TKL_GLOBAL)
		{
			ircstp->is_ref++;
			sendto_one(cptr,
				":%s NOTICE %s :*** You are %s from %s (%s)",
					me.name, cptr->name,
					(lp->expire_at ? "banned" : "permanently banned"),
					ircnetwork, lp->reason);
			ircsprintf(msge, "User has been %s from %s (%s)",
				(lp->expire_at ? "banned" : "permanently banned"),
				ircnetwork, lp->reason);
			return (exit_client(cptr, cptr, &me,
				msge));
		}
		else
		{
			ircstp->is_ref++;
			sendto_one(cptr,
				":%s NOTICE %s :*** You are %s from %s (%s)",
					me.name, cptr->name,
					(lp->expire_at ? "banned" : "permanently banned"),
				me.name, lp->reason);
			ircsprintf(msge, "User is %s (%s)",
				(lp->expire_at ? "banned" : "permanently banned"),
				lp->reason);
			return (exit_client(cptr, cptr, &me,
				msge));

		}
	}
	if (lp->type & TKL_ZAP)
	{
		ircstp->is_ref++;
		ircsprintf(msge,
		    "Z:lined (%s)",lp->reason);
		return exit_client(cptr, cptr, &me, msge);
	}
	if (lp->type & (TKL_SHUN))
	{
		if (IsShunned(cptr))
			return -1;
		if (IsAdmin(cptr))
			return -1;
		SetShunned(cptr);
		return -1;
	}

	return -1;
}

int  find_tkline_match_zap(aClient *cptr)
{
	aTKline *lp;
	char *cip;
	TS   nowtime;
	char msge[1024];
	ConfigItem_except *excepts;
	if (IsServer(cptr) || IsMe(cptr))
		return -1;


	nowtime = TStime();
	cip = (char *)Inet_ia2p(&cptr->ip);

	for (lp = tklines; lp; lp = lp->next)
	{
		if (lp->type & TKL_ZAP)
		{

			if (!match(lp->hostmask, cip))
			{
				for (excepts = conf_except; excepts; excepts = (ConfigItem_except *)excepts->next) {
					if (excepts->flag.type != CONF_EXCEPT_TKL || excepts->type != lp->type)
						continue;
					if (!match(excepts->mask, cip))
						return -1;		
				}
				ircstp->is_ref++;
				ircsprintf(msge,
				    "ERROR :Closing Link: [%s] Z:Lined (%s)\r\n",
#ifndef INET6
				    inetntoa((char *)&cptr->ip), lp->reason);
#else
				    inet_ntop(AF_INET6, (char *)&cptr->ip,
				    mydummy, MYDUMMY_SIZE));
#endif
				strcpy(zlinebuf, msge);
				return (1);
			}
		}
	}
	return -1;
}


int  tkl_sweep()
{
	/* just sweeps local for people that should be killed */
	aClient *acptr;
	long i;

	tkl_check_expire(NULL);
	for (i = 0; i <= (MAXCONNECTIONS - 1); i++)
	{
		if (acptr = local[i])
			find_tkline_match(acptr, 0);
	}
	return 1;
}


void tkl_stats(aClient *cptr)
{
	aTKline *tk;
	TS   curtime;

	/*
	   We output in this row:
	   Glines,GZlines,KLine, ZLIne
	   Character:
	   G, Z, K, z
	 */
	if (!IsAnOper(cptr) && OPER_ONLY_STATS && (strchr(OPER_ONLY_STATS, 'G') || strchr(OPER_ONLY_STATS, 'g'))) {
		sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
		return;
	}
	tkl_check_expire(NULL);
	curtime = TStime();
	for (tk = tklines; tk; tk = tk->next)
	{
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
	}

}

void tkl_synch(aClient *sptr)
{
	aTKline *tk;
	char typ;

	for (tk = tklines; tk; tk = tk->next)
	{
		if (tk->type & TKL_GLOBAL)
		{
			if (tk->type & TKL_KILL)
				typ = 'G';
			if (tk->type & TKL_ZAP)
				typ = 'Z';
			if (tk->type & TKL_SHUN)
				typ = 's';
			sendto_one(sptr,
			    ":%s %s + %c %s %s %s %li %li :%s", me.name,
			    IsToken(sptr) ? TOK_TKL : MSG_TKL,
			    typ,
			    tk->usermask, tk->hostmask, tk->setby,
			    tk->expire_at, tk->set_at, tk->reason);
		}
	}
}

/*
  Service function for timed *:lines

  add:  TKL + type user host setby expire_at set_at reason
  del:  TKL - type user host removedby
  list: TKL ?

  only global lines are spread out this way.
     type= G = G:Line
           Z = Z:Line
*/

int m_tkl(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aTKline *tk;
	int  type;
	int  found = 0;
	char gmt[256], gmt2[256];
	char txt[256];
	TS   expiry_1, setat_1;


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
		  else
			  return 0;

		  found = 0;
		  for (tk = tklines; tk; tk = tk->next)
		  {
			  if (tk->type == type)
			  {
				  if (!strcmp(tk->hostmask, parv[4])
				      && !strcmp(tk->usermask, parv[3]))
				  {
					  found = 1;
					  break;
				  }
			  }
		  }
		  /* G:Line already exists, don't add */
		  if (found == 1)
			  return 0;

		  expiry_1 = atol(parv[6]);
		  setat_1 = atol(parv[7]);

		  /* there is something fucked here? */
		  tkl_add_line(type, parv[3], parv[4], parv[8], parv[5],
		      expiry_1, setat_1);

		  strncpy(gmt, asctime(gmtime((TS *)&setat_1)), sizeof(gmt));
		  strncpy(gmt2, asctime(gmtime((TS *)&expiry_1)), sizeof(gmt2));
		  gmt[strlen(gmt) - 1] = '\0';
		  gmt2[strlen(gmt2) - 1] = '\0';

		  switch (type)
		  {
		    case TKL_KILL:
			    strcpy(txt, "Timed K:Line");
			    break;
		    case TKL_ZAP:
			    strcpy(txt, "Timed Z:Line");
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
		    default:
			    strcpy(txt, "Unknown *:Line");
		  }
		  if (expiry_1 != 0)
		  {
			  sendto_snomask(SNO_TKL,
			      "*** %s added for %s@%s on %s GMT (from %s to expire at %s GMT: %s)",
			      txt, parv[3], parv[4], gmt, parv[5], gmt2,
			      parv[8]);
			  ircd_log
			      (LOG_TKL, "%s added for %s@%s on %s GMT (from %s to expire at %s GMT: %s)",
			      txt, parv[3], parv[4], gmt, parv[5], gmt2,
			      parv[8]);
		  }
		  else
		  {
			  sendto_snomask(SNO_TKL,
			      "*** Permanent %s added for %s@%s on %s GMT (from %s: %s)",
			      txt, parv[3], parv[4], gmt, parv[5], parv[8]);
			  ircd_log
			      (LOG_TKL, "Permanent %s added for %s@%s on %s GMT (from %s: %s)",
			      txt, parv[3], parv[4], gmt, parv[5], parv[8]);
		  }
		  loop.do_tkl_sweep = 1;
		  if (type & TKL_GLOBAL)
		  {
			  sendto_serv_butone(cptr,
			      ":%s TKL %s %s %s %s %s %s %s :%s", sptr->name,
			      parv[1], parv[2], parv[3], parv[4], parv[5],
			      parv[6], parv[7], parv[8]);
		  }
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
		  else
			  return 0;

		  switch (type)
		  {
		    case TKL_KILL:
			    strcpy(txt, "Timed K:Line");
			    break;
		    case TKL_ZAP:
			    strcpy(txt, "Timed Z:Line");
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
		    default:
			    strcpy(txt, "Unknown *:Line");
		  }

		  found = 0;
		  for (tk = tklines; tk; tk = tk->next)
		  {
			  if (tk->type == type)
			  {
				  if (!strcmp(tk->hostmask, parv[4])
				      && !strcmp(tk->usermask, parv[3]))
				  {
					  strncpy(gmt,
					      asctime(gmtime((TS *)&tk->
					      set_at)), sizeof(gmt));
					  gmt[strlen(gmt) - 1] = '\0';
					  sendto_snomask(SNO_TKL,
					      "%s removed %s %s@%s (set at %s - reason: %s)",
					      parv[5], txt, tk->usermask,
					      tk->hostmask, gmt, tk->reason);
					  tkl_del_line(tk);
					  if (type & TKL_GLOBAL)
						  sendto_serv_butone(cptr,
						      ":%s TKL %s %s %s %s %s",
						      sptr->name, parv[1],
						      parv[2], parv[3], parv[4],
						      parv[5]);
					  break;
				  }
			  }
		  }

		  break;

	  case '?':
		  if (IsAnOper(sptr))
			  tkl_stats(sptr);
	}
}

