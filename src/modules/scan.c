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
#include "inet.h"
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif
#include "modules/scan.h"
/* IRCd will fill with a pointer to this module */
struct SOCKADDR_IN Scan_endpoint;
int Scan_BanTime = 0, Scan_TimeOut = 0;
static Scan_AddrStruct *Scannings = NULL;
MUTEX Scannings_lock;

DLLFUNC int h_scan_connect(aClient *sptr);
DLLFUNC int	h_config_set_scan(void);
DLLFUNC int h_stats_scan(aClient *sptr, char *stats);

#ifndef DYNAMIC_LINKING
ModuleHeader m_scan_Header
#else
#define m_scan_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"scan",	/* Name of module */
	"$Id$", /* Version */
	"Scanning API", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

EVENT(e_scannings_clean);

static Event	*Scannings_clean = NULL;
static Hook 	*LocConnect = NULL, *ConfUnknown = NULL, *ServerStats = NULL;
ModuleInfo ScanModInfo;
/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_scan_Init(ModuleInfo *modinfo)
#endif
{
	bcopy(modinfo,&ScanModInfo,modinfo->size);
	LocConnect = HookAddEx(ScanModInfo.handle, HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	ConfUnknown = HookAddEx(ScanModInfo.handle, HOOKTYPE_CONFIG_UNKNOWN, h_config_set_scan);
	ServerStats = HookAddEx(ScanModInfo.handle, HOOKTYPE_STATS, h_stats_scan);
	IRCCreateMutex(Scannings_lock);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_scan_Load(int module_load)
#endif
{
	if (Scan_endpoint.SIN_PORT == 0)
	{
		
#ifndef INET6
		Scan_endpoint.SIN_ADDR.S_ADDR = inet_addr("127.0.0.1");
#else
	        inet_pton(AFINET, "127.0.0.1", Scan_endpoint.SIN_ADDR.S_ADDR);
#endif
		Scan_endpoint.SIN_PORT = htons(2121);
		Scan_endpoint.SIN_FAMILY = AFINET;
	}
	if (Scan_BanTime == 0)
		Scan_BanTime = 86400;

	if (Scan_TimeOut == 0)
		Scan_TimeOut = 20;
	LockEventSystem();
	Scannings_clean = EventAddEx(ScanModInfo.handle, "e_scannings_clean", 0, 0, e_scannings_clean,
NULL);
	UnlockEventSystem();
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(void)
#else
int	m_scan_Unload(void)
#endif
{
	int	ret = MOD_SUCCESS;

	IRCMutexLock(Scannings_lock);
	if (Scannings)	
	{
		LockEventSystem();
		EventAddEx(ScanModInfo.handle, "scan_unload", 
			2, /* Should be enough */
			1,
			e_unload_module_delayed,
			(void *)m_scan_Header.name);
		UnlockEventSystem();
		ret = MOD_DELAY;
	}	
	IRCMutexUnlock(Scannings_lock);	
	if (ret != MOD_DELAY)
	{
		HookDel(LocConnect);
		HookDel(ConfUnknown);
		LockEventSystem();
		EventDel(Scannings_clean);
		UnlockEventSystem();
	}
	return ret;
}

/*
 * An little status indicator
 *
*/

int	Scan_IsBeingChecked(struct IN_ADDR *ia)
{
	int		ret = 0;
	Scan_AddrStruct *sr = NULL;

	IRCMutexLock(Scannings_lock);
	for (sr = Scannings; sr; sr = sr->next)
	{
		if (bcmp(&sr->in, ia, sizeof(Scan_AddrStruct)))
		{
			ret = 1;
			break;
		}
	}	
	IRCMutexUnlock(Scannings_lock);
	return ret;
}

/*
 * Event based crack code to clean up the Scannings 
*/

EVENT(e_scannings_clean)
{
	Scan_AddrStruct *sr = NULL;
	Scan_AddrStruct *q, t;
	IRCMutexLock(Scannings_lock);
	for (sr = Scannings; sr; sr = sr->next)
	{
		IRCMutexLock(sr->lock);
		if (sr->refcnt == 0)
		{
			q = sr->next;
			if (sr->prev)
				sr->prev->next = sr->prev;
			else
				Scannings = sr->next;
			if (sr->next)
				sr->next->prev = sr->prev;
			t.next = sr->next;
			IRCMutexUnlock(sr->lock);
			IRCMutexDestroy(sr->lock);
			MyFree(sr);
			sr = &t;			
			continue;
		}
		IRCMutexUnlock(sr->lock);
	}	
	IRCMutexUnlock(Scannings_lock);
	
}

/*
 * Instead of using the honey and bee infested VS_ban method, we simply
 * abuse the fact that events system is now provided with a lock to ensure
 * that we can actually queue in these bloody bans ..
 * Expect that data will be freed at end of routine -Stskeeps
 */
 
EVENT(e_scan_ban)
{
	Scan_Result *sr = (Scan_Result *) data;
	char hostip[128], mo[100], mo2[100];
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
	/* Weirdness */
	if (!sr)
		return;
	
	strcpy(hostip, Inet_ia2p(&sr->in));
	tkllayer[4] = hostip;
	tkllayer[5] = me.name;
	ircsprintf(mo, "%li", Scan_BanTime + TStime());
	ircsprintf(mo2, "%li", TStime());
	tkllayer[6] = mo;
	tkllayer[7] = mo2;
	tkllayer[8] = sr->reason;
	m_tkl(&me, &me, 9, tkllayer);
	MyFree((char *)sr);
	return;
}

void	Eadd_scan(struct IN_ADDR *in, char *reason)
{
	Scan_Result *sr = (Scan_Result *) MyMalloc(sizeof(Scan_Result));
	sr->in = *in;
	strcpy(sr->reason, reason);
	LockEventSystem();
	EventAddEx(ScanModInfo.handle, "scan_ban", 0, 1, e_scan_ban, (void *)sr);
	UnlockEventSystem();
	return;
}

/*
 * Aah, the root of evil. This will create small linked lists with small little
 * and cute mutexes and reference counts.. we really should use semaphores, but ey..
 * This will also start an insanity of scanning threads to make system admins insane..
 *
 * -Stskeeps
*/

DLLFUNC int h_scan_connect(aClient *sptr)
{
	Scan_AddrStruct *sr = NULL;
	Hook		*hook = NULL;
	THREAD		thread;
	THREAD_ATTR	thread_attr;
	
	if (Find_except(Inet_ia2p(&sptr->ip), 0))
		return 0;
	
	if (Scan_IsBeingChecked(&sptr->ip))
		return 0;
	
	sr = MyMalloc(sizeof(Scan_AddrStruct));
	sr->in = sptr->ip;
	sr->refcnt = 0;
	IRCCreateMutex(sr->lock);
	sr->prev = NULL;
	IRCMutexLock(Scannings_lock);
	sr->next = Scannings;
	Scannings = sr;
	IRCMutexUnlock(Scannings_lock);
	for (hook = Hooks[HOOKTYPE_SCAN_HOST]; hook; hook = hook->next)
	{
		IRCMutexLock(sr->lock);
		sr->refcnt++;
		IRCCreateThread(thread, thread_attr, (hook->func.voidfunc), sr);
		IRCMutexUnlock(sr->lock);
	}
	return 1;
}

/*
 * Config file interfacing 
 *
 * We use this format:
 *     set
 *     {
 *         scan {
 *                // This is where we ask the proxies to connect
 *                endpoint [ip]:port;
 *         };
 *    };
 * 
 */

DLLFUNC int	h_config_set_scan(void)
{
	ConfigItem_unknown_ext *sets;
	ConfigEntry *ce;
	char	*ip;
	char	*port;
	int	iport;
	for (sets = conf_unknown_set; sets; 
		sets = (ConfigItem_unknown_ext *)sets->next)
	{
		if (!strcmp(sets->ce_varname, "scan"))
		{
		        for (ce = sets->ce_entries; ce; ce = (ConfigEntry *)ce->ce_next)
			{
				if (!strcmp(ce->ce_varname, "bantime")) {
					if (!ce->ce_vardata) {
						config_error("%s:%i: set::scan::bantime has no value",
								ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						break;
					}
					Scan_BanTime = atime(ce->ce_vardata);
				}
				if (!strcmp(ce->ce_varname, "timeout")) {
					if (!ce->ce_vardata) {
						config_error("%s:%i: set::scan::timeout has no value",
								ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						break;
					}
					Scan_TimeOut = atime(ce->ce_vardata);
				}

				if (!strcmp(ce->ce_varname, "endpoint"))
				{
					if (!ce->ce_vardata)
					{
						config_error("%s:%i: set::scan::endpoint: syntax [ip]:port",
							     ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						break;
					}
					ipport_seperate(ce->ce_vardata, &ip, &port);
					if (!ip || !*ip)
					{
						config_error("%s:%i: set::scan::endpoint: illegal ip",
							     ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						break;
					}
				        if (!port || !*port)
					{
						config_error("%s:%i: set::scan::endpoint: missing/invalid port",
							    ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
					        break;
					}
					iport = atol(port);
					if ((iport < 0) || (iport > 65535))
					{
						config_error("%s:%i: set::scan::endpoint: illegal port",
							     ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
						break;
					}
#ifndef INET6
					Scan_endpoint.SIN_ADDR.S_ADDR = inet_addr(ip);
#else
				        inet_pton(AFINET, ip, Scan_endpoint.SIN_ADDR.S_ADDR);
#endif
					Scan_endpoint.SIN_PORT = htons(iport);
					Scan_endpoint.SIN_FAMILY = AFINET;
				}
				
				
			}
			del_ConfigItem(sets, conf_unknown_set);
		}	
	}
	return 0;
}

DLLFUNC int h_stats_scan(aClient *sptr, char *stats) {
	if (*stats == 'S') {
		sendto_one(sptr, ":%s %i %s :scan::endpoint: %s:%d", me.name, RPL_TEXT, sptr->name,
			Inet_si2p(&Scan_endpoint), ntohs(Scan_endpoint.SIN_PORT));
		sendto_one(sptr, ":%s %i %s :scan::bantime: %d", me.name, RPL_TEXT, sptr->name,
				Scan_BanTime);
		sendto_one(sptr, ":%s %i %s :scan::timeout: %d", me.name, RPL_TEXT, sptr->name,
				Scan_TimeOut);
	}
        return 0;
}
