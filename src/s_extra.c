/*
 *   Unreal Internet Relay Chat Daemon, src/s_extra.c
 *   (C) 1999-2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

ID_Copyright("(C) Carsten Munk 1999");

/*
    fl->type = 
       0     = set by dccconf.conf
       1     = set by services
       2     = set by ircops by /dccdeny
*/

#define AllocCpy(x,y) x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)

/* ircd.dcc configuration */

ConfigItem_deny_dcc *dcc_isforbidden(aClient *cptr, aClient *sptr, aClient *target, char *filename)
{
	ConfigItem_deny_dcc *p;

	if (!conf_deny_dcc || !target || !filename)
		return NULL;

	if (IsOper(sptr) || IsULine(sptr))
		return NULL;

	if (IsOper(target))
		return NULL;
	if (IsVictim(target))
	{
		return NULL;
	}
	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (!match(p->filename, filename))
		{
			return p;
		}
	}

	/* no target found */
	return NULL;
}

void dcc_sync(aClient *sptr)
{
	ConfigItem_deny_dcc *p;
	for (p = conf_deny_dcc; p; p = (ConfigItem_deny_dcc *) p->next)
	{
		if (p->flag.type2 == CONF_BAN_TYPE_AKILL)
			sendto_one(sptr, ":%s %s + %s :%s", me.name,
			    (IsToken(sptr) ? TOK_SVSFLINE : MSG_SVSFLINE),
			    p->filename, p->reason);
	}
}

void report_flines(aClient *sptr)
{
	ConfigItem_deny_dcc *tmp;
	char *filemask, *reason;
	char a;

	for (tmp = conf_deny_dcc; tmp; tmp = (ConfigItem_deny_dcc *) tmp->next)
	{
		filemask = BadPtr(tmp->filename) ? "<NULL>" : tmp->filename;
		reason = BadPtr(tmp->reason) ? "<NULL>" : tmp->reason;
		if (tmp->flag.type2 == CONF_BAN_TYPE_CONF)
			a = 'c';
		if (tmp->flag.type2 == CONF_BAN_TYPE_AKILL)
			a = 's';
		if (tmp->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
			a = 'o';
		sendto_one(sptr, ":%s %i %s :%c %s %s", me.name, RPL_TEXT,
		    sptr->name, a, filemask, reason);
	}

}

void	DCCdeny_add(char *filename, char *reason, int type)
{
	ConfigItem_deny_dcc *deny = NULL;
	
	deny = (ConfigItem_deny_dcc *) MyMallocEx(sizeof(ConfigItem_deny_dcc));
	deny->filename = strdup(filename);
	deny->reason = strdup(reason);
	deny->flag.type2 = type;
	add_ConfigItem((ConfigItem *)deny, (ConfigItem **)&conf_deny_dcc);
}

void	DCCdeny_del(ConfigItem_deny_dcc *deny)
{
	del_ConfigItem((ConfigItem *)deny, (ConfigItem **)&conf_deny_dcc);
	if (deny->filename)
		MyFree(deny->filename);
	if (deny->reason)
		MyFree(deny->reason);
	MyFree(deny);
}

/* Add a temporary dccdeny line
 *
 * parv[0] - sender
 * parv[1] - file
 * parv[2] - reason
 */

int m_dccdeny(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_deny_dcc	*deny;
	if (!MyClient(sptr))
		return 0;

	if (!IsAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	/* fixup --Stskeeps */
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "DCCDENY");
		return 0;
	}
	
	if (BadPtr(parv[2]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "DCCDENY");
		return 0;
	}
	if (!Find_deny_dcc(parv[1]))
	{
		sendto_ops("%s added a temp dccdeny for %s (%s)", parv[0],
		    parv[1], parv[2]);
		DCCdeny_add(parv[1], parv[2], CONF_BAN_TYPE_TEMPORARY);		
		return 0;
	}
	else
		sendto_one(sptr, "NOTICE %s :*** %s already has a dccdeny", parv[0],
		    parv[1]);
}

/* Remove a temporary dccdeny line
 * parv[0] - sender
 * parv[1] - file/mask
 */
int m_undccdeny(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_deny_dcc *p;
	if (!MyClient(sptr))
		return 0;

	if (!IsAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "UNDCCDENY");
		return 0;
	}

	if (BadPtr(parv[1]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "UNDCCDENY");
		return 0;
	}
/* If we find an exact match even if it is a wild card only remove the exact match -- codemastr */
	if ((p = Find_deny_dcc(parv[1])) && p->flag.type2 == CONF_BAN_TYPE_TEMPORARY)
	{
		sendto_ops("%s removed a temp dccdeny for %s", parv[0],
		    parv[1]);
		DCCdeny_del(p);
		return 1;
	}
/* Next search using the wild card -- codemastr */
/* Uncommented by Stskeeps:
	else if (dcc_del_wild_match(parv[1]) == 1)
		sendto_ops
		    ("%s removed a temp dccdeny for all dccdenys matching %s",
		    parv[0], parv[1]);
*/
/* If still no match, give an error */
	else
		sendto_one(sptr,
		    "NOTICE %s :*** Unable to find a temp dccdeny matching %s",
		    parv[0], parv[1]);

}

int  m_svsfline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!IsServer(sptr))
		return 0;

	if (parc < 2)
		return 0;

	switch (*parv[1])
	{
		  /* Allow non-U:lines to send ONLY SVSFLINE +, but don't send it out
		   * unless it is from a U:line -- codemastr */
	  case '+':
	  {
		  if (parc < 4)
			  return 0;
		  if (!Find_deny_dcc(parv[2]))
			  DCCdeny_add(parv[2], parv[3], CONF_BAN_TYPE_AKILL);
		  if (IsULine(sptr))
			  sendto_serv_butone(cptr, ":%s %s + %s :%s",
			      sptr->name,
			      (IsToken(cptr) ? TOK_SVSFLINE : MSG_SVSFLINE),
			      parv[2], parv[3]);
		  break;
	  }
	  case '-':
	  {
		  if (!IsULine(sptr))
			  return 0;
		  if (parc < 3)
			  return 0;
		  
		  DCCdeny_del(Find_deny_dcc(parv[2]));
		  sendto_serv_butone(cptr, ":%s %s - %s",
		      sptr->name, (IsToken(cptr) ? TOK_SVSFLINE : MSG_SVSFLINE),
		      parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsULine(sptr))
			  return 0;
		  /* FIXME
		  dcc_wipe_services();
		  */
		  sendto_serv_butone(cptr, ":%s %s *", sptr->name,
		      (IsToken(cptr) ? TOK_SVSFLINE : MSG_SVSFLINE));
		  break;
	  }

	}
}

/* restrict channel stuff */


int  channel_canjoin(aClient *sptr, char *name)
{
	ConfigItem_deny_channel *p;

	if (IsOper(sptr))
		return 1;
	if (IsULine(sptr))
		return 1;
	if (!conf_deny_channel)
		return 1;
	p = Find_channel_allowed(name);
	if (p)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** %s",
			me.name, sptr->name, p->reason);
		return 0;
	}
	return 1;
}


int  m_vhost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_vhost *vhost;
	ConfigItem_oper_from *from;
	char *user, *pwd, *host;

	if (parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "VHOST");
		return 0;

	}
	if (!MyClient(sptr))
		return 0;

	user = parv[1];
	pwd = parv[2];

	if (!(vhost = Find_vhost(user))) {
		sendto_umode(UMODE_EYES,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - incorrect password",
		    user, sptr->name,
		    sptr->user->username,
		    sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** [\2vhost\2] Login for %s failed - password incorrect",
		    me.name, sptr->name, user);
		return 0;
	}
	host = make_user_host(sptr->user->username, sptr->user->realhost);
	for (from = (ConfigItem_oper_from *)vhost->from; from; from = (ConfigItem_oper_from *)from->next) {
		if (!match(from->name, host))
			break;
	}
	if (!from) {
		sendto_umode(UMODE_EYES,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - host does not match",
		    user, sptr->name, sptr->user->username, sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** No vHost lines available for your host",
		    me.name, sptr->name);
		return 0;
	}
	if (!strcmp(vhost->password, pwd)) {
		char olduser[USERLEN+1];
		if (sptr->user->virthost)
			MyFree(sptr->user->virthost);
		sptr->user->virthost = MyMalloc(strlen(vhost->virthost) + 1);
		strncpy(sptr->user->virthost, vhost->virthost, HOSTLEN);
		if (vhost->virtuser) {
			strcpy(olduser, sptr->user->username);
			strncpy(sptr->user->username, vhost->virtuser, USERLEN);
		}
		sptr->umodes |= UMODE_HIDE;
		sptr->umodes |= UMODE_SETHOST;
		sendto_serv_butone_token(cptr, sptr->name,
			MSG_SETHOST, TOK_SETHOST,
			"%s", vhost->virthost);
		sendto_one(sptr, ":%s MODE %s :+tx",
		    sptr->name, sptr->name);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Your vhost is now %s%s%s",
		    me.name, sptr->name, vhost->virtuser ? vhost->virtuser : "", 
			vhost->virtuser ? "@" : "", vhost->virthost);
		sendto_umode(UMODE_EYES,
		    "[\2vhost\2] %s (%s!%s@%s) is now using vhost %s%s%s",
		    user, sptr->name,
		    vhost->virtuser ? olduser : sptr->user->username,
		    sptr->user->realhost, vhost->virtuser ? vhost->virtuser : "", 
		    	vhost->virtuser ? "@" : "", vhost->virthost);
		return 0;
	}
	else {
		sendto_umode(UMODE_EYES,
		    "[\2vhost\2] Failed login for vhost %s by %s!%s@%s - incorrect password",
		    user, sptr->name,
		    sptr->user->username,
		    sptr->user->realhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** [\2vhost\2] Login for %s failed - password incorrect",
		    me.name, sptr->name, user);
		return 0;
	}
		
}

/* irc logs.. */
void ircd_log(int flags, char *format, ...)
{
	va_list ap;
	ConfigItem_log *logs;
	char buf[2048], timebuf[128];
	int fd;
	va_start(ap, format);
	ircvsprintf(buf, format, ap);	
	strcat(buf, "\n");
	sprintf(timebuf, "[%s] - ", myctime(TStime()));
	for (logs = conf_log; logs; logs = (ConfigItem_log *) logs->next) {
		if (logs->flags & flags) {
#ifndef _WIN32
			fd = open(logs->file, O_CREAT|O_APPEND|O_WRONLY, S_IRUSR|S_IWUSR);
#else
			fd = open(logs->file, O_CREAT|O_APPEND|O_WRONLY);
#endif
			if (fd == -1)
				continue;
			write(fd, timebuf, strlen(timebuf));
			write(fd, buf, strlen(buf));
			close(fd);
		}
	}
	va_end(ap);
}
