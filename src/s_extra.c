/************************************************************************
 *   IRC - Internet Relay Chat, s_extra.c
 *   (C) 1999 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#include "userload.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifndef _WIN32
#include <utmp.h>
#else
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"

ID_CVS("$Id$");
ID_Copyright("(C) Carsten Munk 1999");

/*
    fl->type = 
       0     = set by dccconf.conf
       1     = set by services
       2     = set by ircops by /dccdeny
*/

#define AllocCpy(x,y) x = (char *) MyMalloc(strlen(y) + 1); strcpy(x,y)
#define IRCD_DCCDENY  "dccdeny.conf"
#define IRCD_RESTRICT "chrestrict.conf"
#define IRCD_VHOST    "vhost.conf"

aFline *flines   = NULL;
aCRline *crlines = NULL;
aVhost *vhosts = NULL;
aHush *hushes = NULL;

char   *cannotjoin_msg = NULL;

/* ircd.dcc configuration */

aFline	*dcc_isforbidden(cptr,sptr,target,filename)
aClient *cptr,*sptr,*target;
char    *filename;
{
	aFline *p;
	
	if (!flines || !target || !filename)
		return NULL;

	if (IsOper(sptr) || IsULine(cptr, sptr))
		return NULL;

	if (IsOper(target))
		return NULL;
	if (IsVictim(target))
	{
		return NULL;
	}	
	for (p = flines; p; p = p->next) 
	{
		if (!match(p->mask, filename))
			return p;
	}

	/* no target found */
	return NULL;
}

int	dcc_add_fline(mask, reason, type)
char *mask, *reason;
int  type;
{
	aFline *fl;
	
	fl = (aFline *) MyMalloc(sizeof(aFline));
	
	AllocCpy(fl->mask, mask);
	AllocCpy(fl->reason, reason);
	fl->type = type;
	fl->next = flines;
	fl->prev = NULL;
	if (flines)
		flines->prev = fl;
	flines = fl;
}

aFline     *dcc_del_fline(fl)
aFline *fl;
{
	aFline *p, *q;
        for (p = flines; p; p = p->next) {
                if (p == fl)
                {
                        q = p->next;
                        MyFree((char *) p->mask);
                        MyFree((char *) p->reason);
                        /* chain1 to chain3 */
                        if (p->prev) {
                                p->prev->next = p->next;
                        }
                          else
                        {
                                flines = p->next;
                        }
                        if (p->next) {
                                p->next->prev = p->prev;
                        }
                        MyFree((aFline *) p);
                        return q;
                }
        }
        return NULL;
}

void	dcc_wipe_all(void)
{
	aFline *p,q;
	
	for (p = flines; p; p = p->next)
	{
		q.next = dcc_del_fline(p);
		p = &q;
	}
}

aFline	*dcc_find(mask)
char *mask;
{
	aFline *p,q;
	
	for (p = flines; p; p = p->next)
	{
		if (!strcmp(p->mask, mask))
			return(p);
	}
	return NULL;
}

void   dcc_rehash(void)
{
	aFline *p,q;
	
	for (p = flines; p; p = p->next)
	{
		if ((p->type == 0) || (p->type == 2))
		{
	                 q.next = dcc_del_fline(p);
	           	 p = &q;
		}
	}
	dcc_loadconf();
}

void   dcc_wipe_services(void)
{
	aFline *p,q;
	
	for (p = flines; p; p = p->next)
	{
		if ((p->type == 1))
		{
	                 q.next = dcc_del_fline(p);
	           	 p = &q;
		}
	}
}

void	report_flines(sptr)
aClient *sptr;
{
	aFline *tmp;
	char	*filemask, *reason;
	char	a;
		
	if (flines)
	{
	}
	for (tmp = flines; tmp; tmp = tmp->next)
	{
		filemask = BadPtr(tmp->mask) ? "<NULL>" : tmp->mask;
		reason  = BadPtr(tmp->reason) ? "<NULL>" : tmp->reason;
		if (tmp->type == 0)
			a = 'c';
		if (tmp->type == 1)
			a = 's';
		if (tmp->type == 2)
			a = 'o';
		sendto_one(sptr, ":%s NOTICE %s :*** (dcc) [%c] %-22s %s", me.name, sptr->name, a, filemask, reason);
	}
	
}

/* 
   dccdeny.conf
   ------------
# DMSetup trojan
deny dmsetup.exe - Possible infected file. Please join #nohack for more information

*/
int	dcc_loadconf(void)
{
	char	buf[2048];
	char	*x,*y,*z;
	FILE	*f;	
	
	f = fopen(IRCD_DCCDENY, "r");
	if (!f)
		return -1;	
		
	while (fgets(buf, 2048,f))
	{
		iCstrip(buf);
		x = strtok(buf, " ");
		if (strcmp("deny", x)==0)
		{
			y = strtok(NULL, " ");
			z = strtok(NULL, "");
			if (!y || !x)
				continue;
			dcc_add_fline(y,z,0);
		}
	}
	return 0; 
}

int     m_svsfline(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	if (!IsULine(cptr,sptr))
		return 0;
	
	if (parc < 2)
		return 0;
		
	switch (*parv[1])
	{
		case '+': {
				if (parc < 4)
					return 0;  			
				dcc_add_fline(parv[2], parv[3], 1);
				sendto_serv_butone(cptr, ":%s SVSFLINE + %s :%s",
			 		sptr->name, parv[2], parv[3]);
			 	break;
			}
		case '-':
			{
				if (parc < 3)
					return 0;
				dcc_del_fline(dcc_find(parv[2]));
				sendto_serv_butone(cptr, ":%s SVSFLINE - %s",
					sptr->name, parv[2]);
				break;
			}
		case '*':
		{
			dcc_wipe_services();
			sendto_serv_butone(cptr, ":%s SVSFLINE *", sptr->name);
			break;
		}
		
	}
}

/* restrict channel stuff */

 
int	channel_canjoin(sptr,name)
aClient *sptr;
char *name;
{
	aCRline *p;
	
	if (IsOper(sptr))
		return 1;
	if (IsULine(sptr,sptr))
		return 1;	
	if (!crlines)
		return 1;
	for (p = crlines; p; p = p->next)
	{
		if (!match(p->channel, name))
			return 1;
	}
	return 0;
}

int	cr_add(channel, type)
char *channel;
int  type;
{
	aCRline *fl;
	
	fl = (aCRline *) MyMalloc(sizeof(aCRline));
	
	AllocCpy(fl->channel, channel);
	fl->type = type;
	fl->next = crlines;
	fl->prev = NULL;
	if (crlines)
		crlines->prev = fl;
	crlines = fl;
}

aCRline     *cr_del(fl)
aCRline *fl;
{
	aCRline *p, *q;
        for (p = crlines; p; p = p->next) {
                if (p == fl)
                {
                        q = p->next;
                        MyFree((char *) p->channel);
                        /* chain1 to chain3 */
                        if (p->prev) {
                                p->prev->next = p->next;
                        }
                          else
                        {
                                crlines = p->next;
                        }
                        if (p->next) {
                                p->next->prev = p->prev;
                        }
                        MyFree((aCRline *) p);
                        return q;
                }
        }
        return NULL;
}

/* 
   chrestrict.conf
   ------------
allow #cafe
allow #teens
*/
int	cr_loadconf(void)
{
	char	buf[2048];
	char	*x,*y;
	FILE	*f;	
	
	f = fopen(IRCD_RESTRICT, "r");
	if (!f)
		return -1;	
		
	while (fgets(buf, 2048,f))
	{
		iCstrip(buf);
		x = strtok(buf, " ");
		if (strcmp("allow", x)==0)
		{
			y = strtok(NULL, " ");
			if (!y)
				continue;
			cr_add(y,0);
		}
		else
		if (strcmp("msg", x)==0)
		{
			y = strtok(NULL, "");
			if (!y)
				continue;
			if (cannotjoin_msg)
				MyFree((char *) cannotjoin_msg);
			cannotjoin_msg = MyMalloc(strlen(y) + 1);
			strcpy(cannotjoin_msg, y);
		}

	}
	return 0; 
}

void   cr_rehash(void)
{
	aCRline *p,q;
	
	for (p = crlines; p; p = p->next)
	{
		if ((p->type == 0) || (p->type == 2))
		{
	                 q.next = cr_del(p);
	           	 p = &q;
		}
	}
	cr_loadconf();
}

void	cr_report(sptr)
aClient *sptr;
{
	aCRline *tmp;
	char	*filemask, *reason;
	char	a;
		
	if (crlines)
	{
	}
	for (tmp = crlines; tmp; tmp = tmp->next)
	{
		filemask = BadPtr(tmp->channel) ? "<NULL>" : tmp->channel;
		if (tmp->type == 0)
			a = 'c';
		if (tmp->type == 1)
			a = 's';
		if (tmp->type == 2)
			a = 'o';
		sendto_one(sptr, ":%s NOTICE %s :*** (allow) [%c] %s", me.name, sptr->name, a, filemask);
	}
	
}

/* vhost configuration (vhost.conf) 
   vhost - login password vhost
*/

int	vhost_add(vhost, login, password, usermask, hostmask)
char *vhost, *login, *password, *usermask, *hostmask;
{
	aVhost *fl;
	
	fl = (aVhost *) MyMalloc(sizeof(aVhost));
	
	AllocCpy(fl->virthost, vhost);
	AllocCpy(fl->usermask, usermask);
	AllocCpy(fl->hostmask, hostmask);
	AllocCpy(fl->login, login);
	AllocCpy(fl->password, password);
	fl->next = vhosts;
	fl->prev = NULL;
	if (vhosts)
		vhosts->prev = fl;
	vhosts = fl;
}

aVhost     *vhost_del(fl)
aVhost *fl;
{
	aVhost *p, *q;
        for (p = vhosts; p; p = p->next) {
                if (p == fl)
                {
                        q = p->next;
			MyFree((char *) (fl->virthost));
			MyFree((char *) (fl->usermask));
			MyFree((char *) (fl->hostmask));
			MyFree((char *) (fl->login));
			MyFree((char *) (fl->password));
                        /* chain1 to chain3 */
                        if (p->prev) {
                                p->prev->next = p->next;
                        }
                          else
                        {
                                vhosts = p->next;
                        }
                        if (p->next) {
                                p->next->prev = p->prev;
                        }
                        MyFree((aVhost *) p);
                        return q;
                }
        }
        return NULL;
}

/* 
  vhost.conf
   ------------
# vhost virtualhost username password mask

vhost microsoft.com billgates ilovelinux *@*
*/
int	vhost_loadconf(void)
{
	char	buf[2048];
	char	*x,*y, *login, *password, *mask, *usermask, *hostmask;
	FILE	*f;	
/* _not_ a failsafe routine .. */	
	f = fopen(IRCD_VHOST, "r");
	if (!f)
		return -1;	
		
	while (fgets(buf, 2048,f))
	{
		iCstrip(buf);
		x = strtok(buf, " ");
		if (strcmp("vhost", x)==0)
		{
			y = strtok(NULL, " ");
			if (!y)
				continue;
			login = strtok(NULL, " ");
			password = strtok(NULL, " ");
			mask = strtok(NULL, "");
			usermask = strtok(mask, "@");
			hostmask = strtok(NULL, " ");
			vhost_add(y, login,password,usermask, hostmask);
		}
	}
	return 0; 
}

void   vhost_rehash(void)
{
	aVhost *p,q;
	
	for (p = vhosts; p; p = p->next)
	{
	                 q.next = vhost_del(p);
	           	 p = &q;
	}
	vhost_loadconf();
}

void	vhost_report(sptr)
aClient *sptr;
{
	aVhost *tmp;
	char	*filemask, *reason;
	char	a;
		
	for (tmp = vhosts; tmp; tmp = tmp->next)
	{
		filemask = BadPtr(tmp->virthost) ? "<NULL>" : tmp->virthost;
		a = 'V';
		sendto_one(sptr, ":%s NOTICE %s :*** (vhost) [%c] %s %s (%s@%s)", me.name, sptr->name, a, filemask, tmp->login, tmp->usermask, tmp->hostmask);
	}
	
}

int     m_vhost(cptr, sptr, parc, parv)
aClient *cptr, *sptr;
int parc;
char *parv[];
{
	aVhost *p;
	char	*user, *pwd;
	if (check_registered(sptr))
		return 0;
	
	if (parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			   me.name, parv[0], "VHOST");
		return 0;

	}
	if (!MyClient(sptr))
		return 0;
		
	user = parv[1];
	pwd  = parv[2];
	
	for (p = vhosts; p; p = p->next)
	{
		if (!strcmp(p->login, user))
		{
			/* First check hostmask.. */
			if (!match(p->hostmask, sptr->user->realhost) && !match(p->usermask, sptr->user->username))
			{
				/* that was okay, lets check password */
				if (!strcmp(p->password, pwd))
				{
					/* let's vhost him .. */
					strcpy(sptr->user->virthost, p->virthost);
					sptr->umodes |= UMODE_HIDE;
					sptr->umodes |= UMODE_SETHOST;
					sendto_serv_butone(cptr, ":%s SETHOST %s", sptr->name, p->virthost);
					sendto_one(sptr, ":%s MODE %s :+tx", sptr->name, sptr->name);
					sendto_one(sptr, ":%s NOTICE %s :*** Your hostname is now %s", me.name, sptr->name, p->virthost);
					sendto_umode(UMODE_EYES, "[\2vhost\2] %s (%s!%s@%s) is now using vhost %s", 
						user, sptr->name, sptr->user->username, sptr->user->realhost, p->virthost);
					return 0;
				}
					else
				{
					sendto_one(sptr, ":%s NOTICE %s :*** [\2vhost\2] Login for %s failed - password incorrect", me.name, sptr->name, user);
					return 0;
				}
			}
		}	
	}
	sendto_one(sptr, ":%s NOTICE %s :*** No vHost lines available for your host", me.name, sptr->name);
	return 0;				
}

/* hush */
#ifdef MOO
int	hush_add(vhost, login, password, usermask, hostmask)
char *vhost, *login, *password, *usermask, *hostmask;
{
	aHush *fl;
	
	fl = (aHush *) MyMalloc(sizeof(aHush));
	
	AllocCpy(fl->virthost, vhost);
	AllocCpy(fl->usermask, usermask);
	AllocCpy(fl->hostmask, hostmask);
	AllocCpy(fl->login, login);
	AllocCpy(fl->password, password);
	fl->next = vhosts;
	fl->prev = NULL;
	if (vhosts)
		vhosts->prev = fl;
	vhosts = fl;
}

aVhost     *vhost_del(fl)
aVhost *fl;
{
	aVhost *p, *q;
        for (p = vhosts; p; p = p->next) {
                if (p == fl)
                {
                        q = p->next;
			MyFree((char *) (fl->virthost));
			MyFree((char *) (fl->usermask));
			MyFree((char *) (fl->hostmask));
			MyFree((char *) (fl->login));
			MyFree((char *) (fl->password));
                        /* chain1 to chain3 */
                        if (p->prev) {
                                p->prev->next = p->next;
                        }
                          else
                        {
                                vhosts = p->next;
                        }
                        if (p->next) {
                                p->next->prev = p->prev;
                        }
                        MyFree((aVhost *) p);
                        return q;
                }
        }
        return NULL;
}
#endif
/* irc logs.. */
void	ircd_log(char *format, ...)
{
	va_list ap;
        FILE	*f;
              
        va_start(ap, format);
	f = fopen(lPATH, "a+");
	if (!f)
	{
#if !defined(_WIN32) && !defined(_AMIGA)
		sendto_realops("Couldn't write to %s - %s", lPATH, strerror(errno));
#endif
		return;
	}
	fprintf(f, "(%s) ", myctime(TStime()));
	vfprintf(f, format, ap);
	fprintf(f, "\n");
	fclose(f);
	va_end(ap);        
}             

