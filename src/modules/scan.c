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
struct IN_ADDR Scan_bind;
int Scan_BanTime = 0, Scan_TimeOut = 0;
static Scan_AddrStruct *Scannings = NULL;
MUTEX Scannings_lock;
static char	*scan_message;
extern ConfigEntry		*config_find_entry(ConfigEntry *ce, char *name);
DLLFUNC int h_scan_connect(aClient *sptr);
DLLFUNC int h_config_test(ConfigFile *, ConfigEntry *, int);
DLLFUNC int h_config_run(ConfigFile *, ConfigEntry *, int);
DLLFUNC int h_config_posttest();
DLLFUNC int h_stats_scan(aClient *sptr, char *stats);

struct requiredconfig {
	int endpoint :1;
	int timeout :1;
	int bantime :1;
	int bindip :1;
};

struct requiredconfig ReqConf;

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
static Hook 	*LocConnect = NULL, *ConfTest = NULL, *ConfRun = NULL, *ServerStats = NULL;
static Hook	*ConfPostTest = NULL;
static Hooktype *ScanHost = NULL;
static int HOOKTYPE_SCAN_HOST;
ModuleInfo ScanModInfo;
/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_scan_Init(ModuleInfo *modinfo)
#endif
{
	scan_message = NULL;
	bcopy(modinfo,&ScanModInfo,modinfo->size);
	bzero(&ReqConf, sizeof(ReqConf));
	ScanHost = (Hooktype *)HooktypeAdd(modinfo->handle, "HOOKTYPE_SCAN_HOST", &HOOKTYPE_SCAN_HOST);
	LocConnect = HookAddEx(ScanModInfo.handle, HOOKTYPE_LOCAL_CONNECT, h_scan_connect);
	ConfTest = HookAddEx(ScanModInfo.handle, HOOKTYPE_CONFIGTEST, h_config_test);
	ConfRun = HookAddEx(ScanModInfo.handle, HOOKTYPE_CONFIGRUN, h_config_run);
	ConfPostTest = HookAddEx(ScanModInfo.handle, HOOKTYPE_CONFIGPOSTTEST, h_config_posttest);
	ServerStats = HookAddEx(ScanModInfo.handle, HOOKTYPE_STATS, h_stats_scan);
	bzero(&Scan_bind, sizeof(Scan_bind));
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
	LockEventSystem();
	Scannings_clean = EventAddEx(ScanModInfo.handle, "e_scannings_clean", 0, 0, e_scannings_clean, NULL);
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
		HooktypeDel(ScanHost,ScanModInfo.handle);
		HookDel(LocConnect);
		HookDel(ConfTest);
		HookDel(ConfRun);
		HookDel(ConfPostTest);
		HookDel(ServerStats);
		LockEventSystem();
		EventDel(Scannings_clean);
		UnlockEventSystem();
		IRCMutexDestroy(Scannings_lock);
		if (scan_message)
			free(scan_message);
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
		if (!bcmp(&sr->in, ia, sizeof(Scan_AddrStruct)))
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
	
	strlcpy(hostip, Inet_ia2p(&sr->in), sizeof hostip);
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
	strlcpy(sr->reason, reason, sizeof sr->reason);
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
	
	if (Find_except(Inet_ia2p(&sptr->ip), 0))
		return 0;
	
	if (Scan_IsBeingChecked(&sptr->ip))
		return 0;
	if (scan_message)
		sendto_one(sptr, ":%s NOTICE %s :%s",
			me.name, sptr->name, scan_message);
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
		IRCCreateThread(thread, (hook->func.voidfunc), sr);
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
DLLFUNC h_config_test(ConfigFile *cf, ConfigEntry *ce, int type) {
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->ce_varname, "scan"))
	{
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!cep->ce_varname)
			{
				config_error("%s:%i: blank set::scan item",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);	
				errors++;
				continue;
			}
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: set::scan::%s item without value",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "endpoint"))
			{
				char copy[256];
				char *ip, *port;
				int iport;
				strcpy(copy, cep->ce_vardata);
				ipport_seperate(copy, &ip, &port);
				if (!ip || !*ip)
				{
					config_error("%s:%i: set::scan::endpoint: illegal ip:port mask",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return -1;
				}
				if (strchr(ip, '*'))
				{
					config_error("%s:%i: set::scan::endpoint: illegal ip",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return -1;
				}
				if (!port || !*port)
				{
					config_error("%s:%i: set::scan::endpoint: missing port in mask",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return -1;
				}
				iport = atol(port);
				if ((iport < 0) || (iport > 65535))
				{
					config_error("%s:%i: set::scan::endpoint: illegal port (must be 0..65536)",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					return -1;
				}
				ReqConf.endpoint = 1;
			}
			else if (!strcmp(cep->ce_varname, "bind-ip"))
			{
				if (strchr(cep->ce_vardata, '*') && strcmp(cep->ce_vardata, "*"))
				{
					config_error("%s:%i: set::scan::bind-ip: illegal ip, (mask, and not '*')",
						ce->ce_fileptr->cf_filename, ce->ce_varlinenum);
					return -1;
				}
				ReqConf.bindip = 1;
			}
			else if (!strcmp(cep->ce_varname, "message"));
			else if (!strcmp(cep->ce_varname, "bantime"))
				ReqConf.bantime = 1;
			else if (!strcmp(cep->ce_varname, "timeout"))
				ReqConf.timeout = 1;
			else 
			{
				config_error("%s:%i: unknown directive set::scan::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
				errors++;
			}
			
		}
		return errors ? -1 : 1;
	}
	else
		return 0;
}

DLLFUNC h_config_run(ConfigFile *cf, ConfigEntry *ce, int type) {
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;
	if (!strcmp(ce->ce_varname, "scan"))
	{
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "endpoint"))
			{
				char copy[256];
				char *ip, *port;
				int iport;
				strcpy(copy, cep->ce_vardata);
				ipport_seperate(copy, &ip, &port);
#ifndef INET6
				Scan_endpoint.SIN_ADDR.S_ADDR = inet_addr(ip);
#else
	        		inet_pton(AFINET, ip, Scan_endpoint.SIN_ADDR.S_ADDR);
#endif
				iport = atol(port);
				Scan_endpoint.SIN_PORT = htons(iport);
				Scan_endpoint.SIN_FAMILY = AFINET;		
			}
			else if (!strcmp(cep->ce_varname, "bantime"))
				Scan_BanTime = config_checkval(cep->ce_vardata,CFG_TIME);
			else if (!strcmp(cep->ce_varname, "timeout"))
				Scan_TimeOut = config_checkval(cep->ce_vardata,CFG_TIME);
			else if (!strcmp(cep->ce_varname, "bind-ip"))
			{
#ifndef INET6
				Scan_bind.S_ADDR = inet_addr(cep->ce_vardata);
#else	
				inet_pton(AFINET, cep->ce_vardata, Scan_bind.S_ADDR);
#endif
			}
			if (!strcmp(cep->ce_varname, "message"))
				scan_message = strdup(cep->ce_vardata);
		}
		return 1;		
	}
	return 0;
}

DLLFUNC int h_config_posttest() {
	int errors = 0;
	if (!ReqConf.endpoint)
	{
		config_error("set::scan::endpoint missing");
		errors++;
	}
	if (!ReqConf.bantime)
	{
		config_error("set::scan::bantime missing");
		errors++;
	}
	if (!ReqConf.timeout)
	{
		config_error("set::scan::timeout missing");
		errors++;
	}
	if (!ReqConf.bindip)
	{
		config_error("set::scan::bind-ip missing");
		errors++;
	}
	return errors ? -1 : 1;	
}

DLLFUNC int h_stats_scan(aClient *sptr, char *stats) {
	if (*stats == 'S') {
		sendto_one(sptr, ":%s %i %s :scan::endpoint: %s:%d", me.name, RPL_TEXT, sptr->name,
			Inet_si2p(&Scan_endpoint), ntohs(Scan_endpoint.SIN_PORT));
		sendto_one(sptr, ":%s %i %s :scan::bantime: %d", me.name, RPL_TEXT, sptr->name,
				Scan_BanTime);
		sendto_one(sptr, ":%s %i %s :scan::timeout: %d", me.name, RPL_TEXT, sptr->name,
				Scan_TimeOut);
		sendto_one(sptr, ":%s %i %s :scan::bind-ip: %s",
			me.name, RPL_TEXT, sptr->name, Inet_ia2p(&Scan_bind));
		sendto_one(sptr, ":%s %i %s :scan::message: %s",
			me.name, RPL_TEXT, sptr->name, scan_message ? scan_message : "<NULL>");
	}
        return 0;
}
