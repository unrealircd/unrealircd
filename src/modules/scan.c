/*
 *   IRC - Internet Relay Chat, src/modules/scan.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *
 *   Generic scanning API for Unreal3.2 and up 
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
#include "config.h"
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
#else
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>

#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#define IS_SCAN_C

#include "modules/scan.h"
#include "modules/blackhole.h"

#define MSG_SCAN 	"SCAN"	/* scan */
#define TOK_SCAN "??"
 /* 
 * Structure containing what hosts currently being checked.
 * refcnt = 0 means it is doomed for cleanup
*/
HStruct			Hosts[SCAN_AT_ONCE];
VHStruct		VHosts[SCAN_AT_ONCE];

/* 
 * If it is legal to edit Hosts table
*/
MUTEX				HSlock;
MUTEX				VSlock;

MUTEX				blackhole_mutex;
ConfigItem_blackhole		blackhole_conf;
char				blackhole_stop = 0;
SOCKET				blackholefd = 0;
struct SOCKADDR_IN		blackholesin;
/* Some prototypes .. aint they sweet? */
DLLFUNC int			h_scan_connect(aClient *sptr);
DLLFUNC EVENT			(HS_Cleanup);
DLLFUNC EVENT			(VS_Ban);
DLLFUNC int			h_scan_info(aClient *sptr);
DLLFUNC int			m_scan(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int			h_config_set_blackhole(void);
DLLFUNC int			h_config_set_blackhole_rehash(void);
DLLFUNC void 			blackhole(void *p);
THREAD				acceptthread;
THREAD_ATTR			acceptthread_attr;

#ifndef DYNAMIC_LINKING
ModuleInfo m_scan_info
#else
#define m_scan_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"scan",	/* Name of module */
	"$Id$", /* Version */
	"Scanning API", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_scan_init(int module_load)
#endif
{
	add_Hook(HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	add_Hook(HOOKTYPE_SCAN_INFO, h_scan_info);
	bzero(Hosts, sizeof(Hosts));
	bzero(VHosts, sizeof(VHosts));
	EventAdd("hscleanup", 1, 0, HS_Cleanup, NULL);
	EventAdd("vsban", 1, 0, VS_Ban, NULL);
	IRCCreateMutex(HSlock);
	IRCCreateMutex(VSlock);
	add_Command(MSG_SCAN, TOK_SCAN, m_scan, MAXPARA);
	blackhole_stop = 0;
	bzero(&blackhole_conf, sizeof(blackhole_conf));
	add_Hook(HOOKTYPE_CONFIG_UNKNOWN, h_config_set_blackhole);
	add_Hook(HOOKTYPE_REHASH, h_config_set_blackhole_rehash);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_scan_load(int module_load)
#endif
{
	if (!blackhole_conf.ip || !blackhole_conf.port)
	{
		config_progress("set::blackhole: missing ip/port mask or configuration not loaded. using %s:6013",
			conf_listen->ip);
		blackhole_conf.ip = strdup(conf_listen->ip);
		blackhole_conf.port = 6013;
	}
	if ((blackholefd = socket(AFINET, SOCK_STREAM, 0)) == -1)
	{
		config_error("set::blackhole: could not create socket");
		blackholefd = -1;
		return -1;
	}		
	blackholesin.SIN_ADDR.S_ADDR = inet_addr(blackhole_conf.ip);
	blackholesin.SIN_PORT = htons(blackhole_conf.port);
	blackholesin.SIN_FAMILY = AFINET;
	blackhole_stop = 0;
	if (bind(blackholefd, (struct SOCKADDR *)&blackholesin, sizeof(blackholesin)))
	{
		CLOSE_SOCK(blackholefd);
		config_error("set::blackhole: could not bind to %s:%i - %s", blackhole_conf.ip, blackhole_conf.port,
			strerror(errno)); 
		blackholefd = -1;
		return -1;
	}

	listen(blackholefd, LISTEN_SIZE);
	/* Create blackhole accept() thread */
	IRCCreateThread(acceptthread, acceptthread_attr, blackhole, NULL);
	return 0;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_scan_unload(void)
#endif
{
	SOCKET fd;
	int j,i;
	if (del_Command(MSG_SCAN, TOK_SCAN, m_scan) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_scan_info.name);
	}
	del_Hook(HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	del_Hook(HOOKTYPE_SCAN_INFO, h_scan_info);
	EventDel("hscleanup");
	EventDel("vsban");
	i = 1;
	while (i)
	{
		i = 0;
		HS_Cleanup(NULL);
		IRCMutexLock(HSlock);
		for (j = 0; j <= SCAN_AT_ONCE;  j++)
			if (Hosts[i].host[0])
				i++;
		IRCMutexUnlock(HSlock);
	}
	i = 1;
	while (i)
	{
		i = 0;
		VS_Ban(NULL);
		IRCMutexLock(VSlock);
		for (j = 0; j <= SCAN_AT_ONCE;  j++)
			if (VHosts[i].host[0])
				i++;
		IRCMutexUnlock(VSlock);
		
	}
	/* We need to catch it, and throw it out as soon as we get it */	
	IRCMutexLock(HSlock);
	IRCMutexDestroy(HSlock);
	IRCMutexLock(VSlock);
	IRCMutexDestroy(VSlock);
	del_Hook(HOOKTYPE_CONFIG_UNKNOWN, h_config_set_blackhole);
	del_Hook(HOOKTYPE_REHASH, h_config_set_blackhole_rehash);
	blackhole_stop = 1;
	if (blackholefd)
	{
		fd = socket(AFINET, SOCK_STREAM, 0);
		if (fd >-1)
		{
			connect(fd, (struct sockaddr *) &blackholesin, sizeof(blackholesin));
		}
		IRCJoinThread(acceptthread, NULL);
		CLOSE_SOCK(fd);
	}
	if (blackhole_conf.ip)
		MyFree(blackhole_conf.ip);
}

HStruct	*HS_Add(char *host)
{
	int	i;
	
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (!(*Hosts[i].host))
		{
			strcpy(Hosts[i].host, host);
			return (&Hosts[i]);
		}
	return NULL;
}

VHStruct	*VS_Add(char *host, char *reason)
{
	int	i;
	
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (!(*VHosts[i].host))
		{
			strcpy(VHosts[i].host, host);
			strcpy(VHosts[i].reason, reason);
			return (&VHosts[i]);
		}
	return NULL;
}


HStruct *HS_Find(char *host)
{
	int	i;

	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (!strcmp(Hosts[i].host, host))
		{
			return (&Hosts[i]);
		}
	return NULL;
}

VHStruct *VS_Find(char *host)
{
	int	i;

	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (!strcmp(VHosts[i].host, host))
		{
			return (&VHosts[i]);
		}
	return NULL;
}

DLLFUNC EVENT(HS_Cleanup)
{
	int i;

	/* If it is called as a event, get lock */
	if (data == NULL)
		IRCMutexLock(HSlock);			
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (Hosts[i].host[0] && (Hosts[i].refcnt <= 0))
		{
			*(Hosts[i].host) = '\0';	
			Hosts[i].refcnt = 0;
		}
	if (data == NULL)
		IRCMutexUnlock(HSlock);
}

DLLFUNC EVENT(VS_Ban)
{
	int i;
	char hostip[128], mo[100], mo2[100], reason[256];
	char *tkllayer[9] = {
		me.name,	/*0  server.name */
		"+",		/*1  +|- */
		"z",		/*2  G   */
		"*",		/*3  user */
		NULL,		/*4  host */
		NULL,
		NULL,		/*6  expire_at */
		NULL,		/*7  set_at */
		NULL		/*8  reason */
	};

	if (data == NULL)
		IRCMutexLock(VSlock);			
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (*VHosts[i].host && *VHosts[i].reason)
		{
			
			strcpy(hostip, VHosts[i].host);
			tkllayer[4] = hostip;
			tkllayer[5] = me.name;
			ircsprintf(mo, "%li", SOCKSBANTIME + TStime());
			ircsprintf(mo2, "%li", TStime());
			tkllayer[6] = mo;
			tkllayer[7] = mo2;
			strcpy(reason, VHosts[i].reason);
			tkllayer[8] = reason;
			m_tkl(&me, &me, 9, tkllayer);
			/* De-stroy */
			*VHosts[i].host = '\0';
		    *VHosts[i].reason = '\0';
		}
	if (data == NULL)
		IRCMutexUnlock(VSlock);

	
}

DLLFUNC int h_scan_connect(aClient *sptr)
{
	Hook			*hook;
	HStruct			*h;
	vFP			*vfp;
	THREAD			thread;
	THREAD_ATTR		thread_attr;

	IRCMutexLock(HSlock);
	HS_Cleanup((void *)1);
	if (HS_Find((char *)inet_ntoa(sptr->ip)))
	{
		/* Not gonna scan, already scanning */
		IRCMutexUnlock(HSlock);
		return 0;
	}
	if (h = HS_Add((char *)inet_ntoa(sptr->ip)))
	{
		/* Run scanning threads, refcnt++ for each thread that uses the struct */
		/* Use hooks, making it easy, remember to convert to vFP */
		for (hook = Hooks[HOOKTYPE_SCAN_HOST]; hook; hook = hook->next)
		{
	        h->refcnt++;
			/* Create thread for connection */
			IRCCreateThread(thread, thread_attr, (hook->func.voidfunc), h); 
		}
		IRCMutexUnlock(HSlock);
		return 1;
	}
	else
	{
		/* We got no more slots back .. actually .. shouldn't we call HS_cleanup 
		   And run h_scan_connect again?. Is this too loopy?
		*/
		sendto_realops("Problem: We ran out of Host slots. Cannot scan %s. increase SCAN_AT_ONCE",
			sptr->sockhost);
		IRCMutexUnlock(HSlock);
		return 0;
	}
}

DLLFUNC int h_scan_info(aClient *sptr)
{
	int i;
	/* We're gonna read from Hosts, so we better get a lock */
	IRCMutexLock(HSlock);
	IRCMutexLock(VSlock);
	sendto_one(sptr, ":%s NOTICE %s :*** scan API $Id$ by Stskeeps",
			me.name, sptr->name);
	sendto_one(sptr, ":%s NOTICE %s :*** Currently scanning:",
		me.name, sptr->name);
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (*Hosts[i].host)
			sendto_one(sptr, ":%s NOTICE %s :*** IP: %s refcnt: %i",
				me.name, sptr->name, Hosts[i].host, Hosts[i].refcnt);
	sendto_one(sptr, ":%s NOTICE %s :*** Currently banning:",
		me.name, sptr->name);
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (*VHosts[i].host)
			sendto_one(sptr, ":%s NOTICE %s :*** IP: %s Reason: %s",
				me.name, sptr->name, VHosts[i].host, VHosts[i].reason);
	
	IRCMutexUnlock(HSlock);
	IRCMutexUnlock(VSlock);
	return 0;
}

DLLFUNC int m_scan(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	if (!IsOper(sptr))
		return 0;
	HS_Cleanup(NULL);
	RunHook(HOOKTYPE_SCAN_INFO, sptr);
	return 0;
}

DLLFUNC int     h_config_set_blackhole_rehash(void)
{
	if (blackhole_conf.ip)
		MyFree(blackhole_conf.ip);
	if (blackhole_conf.outip)
		MyFree(blackhole_conf.outip);
	blackhole_conf.ip = NULL;
	blackhole_conf.outip = NULL;
}

DLLFUNC int	h_config_set_blackhole(void)
{
	ConfigItem_unknown_ext *sets;
	
	char	*ip;
	char	*port;
	int	iport;
	for (sets = conf_unknown_set; sets; 
		sets = (ConfigItem_unknown_ext *)sets->next)
	{
		if (!strcmp(sets->ce_varname, "blackhole"))
		{
			if (!sets->ce_vardata)
			{
				config_error("%s:%i: set::blackhole - missing parameter");
				goto explodeblackhole;
			}
			if (sets->ce_entries)
			{
				blackhole_conf.outip = strdup(sets->ce_entries->ce_varname);
			}
			ipport_seperate(sets->ce_vardata, &ip, &port);
			if (!ip || !*ip)
			{
				config_error("%s:%i: set::blackhole: illegal ip:port mask",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			if (strchr(ip, '*'))
			{
				config_error("%s:%i: set::blackhole: illegal ip, (mask)",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			if (!port || !*port)
			{
				config_error("%s:%i: set::blackhole: missing port in mask",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
			iport = atol(port);
			if ((iport < 0) || (iport > 65535))
			{
				config_error("%s:%i: set::blackhole: illegal port (must be 0..65536)",
					sets->ce_fileptr->cf_filename, sets->ce_varlinenum);
				goto explodeblackhole;
			}
 			blackhole_conf.ip = strdup(ip);
 			blackhole_conf.port = iport;			
	explodeblackhole:
			del_ConfigItem((ConfigItem *)sets, 
				(ConfigItem **) &conf_unknown_set);
			continue;
		}	
	}
	if (!blackhole_conf.ip || !blackhole_conf.port)
	{
		config_error("set::blackhole: missing ip/port mask");
	}
	return 0;
}

DLLFUNC void blackhole(void *p)
{
	int	callerfd;
	while (blackhole_stop != 1)
	{
		callerfd = accept(blackholefd, NULL, NULL);
		CLOSE_SOCK(callerfd);
	}
	CLOSE_SOCK(blackholefd);
	blackhole_stop = 0;
	IRCExitThread(NULL);	
}
