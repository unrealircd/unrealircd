/*
 *   IRC - Internet Relay Chat, src/modules/scan.c
 *   (C) 2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#include <pthread.h>

#define MSG_SCAN 	"SCAN"	/* scan */
#define TOK_SCAN "??"
 /* 
 * Structure containing what hosts currently being checked.
 * refcnt = 0 means it is doomed for cleanup
*/
HStruct			Hosts[SCAN_AT_ONCE];
/* 
 * If it is legal to edit Hosts table
*/
pthread_mutex_t		HSlock;

/* Some prototypes .. aint they sweet? */
DLLFUNC int			h_scan_connect(aClient *sptr);
DLLFUNC EVENT		(HS_Cleanup);
DLLFUNC int			h_scan_info(aClient *sptr);
DLLFUNC int			m_scan(aClient *cptr, aClient *sptr, int parc, char *parv[]);

ModuleInfo m_scan_info
  = {
  	1,
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
DLLFUNC void	mod_init(void)
#else
void    m_scan_init(void)
#endif
{
	/* extern variable to export m_dummy_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	module_buffer = &m_scan_info;
	add_Hook(HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	add_Hook(HOOKTYPE_SCAN_INFO, h_scan_info);
	bzero(Hosts, sizeof(Hosts));
	EventAdd("hscleanup", 1, 0, HS_Cleanup, NULL);
	pthread_mutex_init(&HSlock, NULL);
	add_Command(MSG_SCAN, TOK_SCAN, m_scan, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(void)
#else
void    m_scan_load(void)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_scan_unload(void)
#endif
{
	if (del_Command(MSG_SCAN, TOK_SCAN, m_scan) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_scan_info.name);
	}
	del_Hook(HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	del_Hook(HOOKTYPE_SCAN_INFO, h_scan_info);
	EventDel("hscleanup");
	/* We need to catch it, and throw it out as soon as we get it */
	pthread_mutex_lock(&HSlock);
	pthread_mutex_destroy(&HSlock);
}

HStruct	*HS_Add(char *host)
{
	int	i;
	
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (!(*Hosts[i].host))
			break;
	if (!*Hosts[i].host && (i != SCAN_AT_ONCE))
	{
		strcpy(Hosts[i].host, host);
		Hosts[i].refcnt = 0;	
		return (&Hosts[i]);
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

DLLFUNC EVENT(HS_Cleanup)
{
	int i;

	/* If it is called as a event, get lock */
	if (data == NULL)
		pthread_mutex_lock(&HSlock);			
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if ((Hosts[i].refcnt < 1) && *Hosts[i].host)
		{
			*Hosts[i].host = '\0';
			Hosts[i].refcnt = 0;
		}
	if (data == NULL)
		pthread_mutex_unlock(&HSlock);
}

DLLFUNC int h_scan_connect(aClient *sptr)
{
	Hook			*hook;
	HStruct			*h;
	vFP				*vfp;
	pthread_t		thread;
    pthread_attr_t	thread_attr;

	pthread_mutex_lock(&HSlock);
	HS_Cleanup((void *)1);
	if (HS_Find(sptr->sockhost))
	{
		/* Not gonna scan, already scanning */
		pthread_mutex_unlock(&HSlock);
		return 0;
	}
	if (h = HS_Add(sptr->sockhost))
	{
		/* Run scanning threads, refcnt++ for each thread that uses the struct */
		/* Use hooks, making it easy, remember to convert to vFP */
		for (hook = Hooks[HOOKTYPE_SCAN_HOST]; hook; hook = hook->next)
		{
	        h->refcnt++;
			/* Create thread for connection */
			pthread_create( &thread, NULL,
                                 (void*)(hook->func.voidfunc), (void*) h);			
		}
		pthread_mutex_unlock(&HSlock);
		return 1;
	}
	else
	{
		/* We got no more slots back .. actually .. shouldn't we call HS_cleanup 
		   And run h_scan_connect again?. Is this too loopy?
		*/
		sendto_realops("Problem: We ran out of Host slots. Cannot scan %s. increase SCAN_AT_ONCE",
			sptr->sockhost);
		pthread_mutex_unlock(&HSlock);
		return 0;
	}
}

DLLFUNC int h_scan_info(aClient *sptr)
{
	int i;
	/* We're gonna read from Hosts, so we better get a lock */
	pthread_mutex_lock(&HSlock);
	sendto_one(sptr, ":%s NOTICE %s :*** scan API $Id$ by Stskeeps",
			me.name, sptr->name);
	sendto_one(sptr, ":%s NOTICE %s :*** Currently scanning:",
		me.name, sptr->name);
	for (i = 0; i <= SCAN_AT_ONCE; i++)
		if (*Hosts[i].host)
			sendto_one(sptr, ":%s NOTICE %s :*** IP: %s refcnt: %i",
				me.name, sptr->name, Hosts[i].host, Hosts[i].refcnt);
		
	pthread_mutex_unlock(&HSlock);
	return 0;
}

DLLFUNC int m_scan(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	HS_Cleanup(NULL);
	RunHook(HOOKTYPE_SCAN_INFO, sptr);
	return 0;
}
