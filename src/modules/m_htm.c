/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_htm.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif


#ifndef NO_FDLIST
extern float currentrate;
extern float currentrate2;
extern float highest_rate;
extern float highest_rate2;
extern int lifesux;
extern int noisy_htm;
extern time_t LCF;
extern int LRV;
#endif


DLLFUNC int m_htm(aClient *cptr, aClient *sptr, int parc, char *parv[]);
EVENT(lcf_check);
EVENT(htm_calc);
Event *e_lcf, *e_htmcalc;

/* Place includes here */
#define MSG_HTM         "HTM"
#define TOK_HTM         "BH"

DLLFUNC int htm_config_test(ConfigFile *, ConfigEntry *, int, int *);
DLLFUNC int htm_config_run(ConfigFile *, ConfigEntry *, int);
DLLFUNC int htm_stats(aClient *, char *); 

ModuleInfo HtmModInfo;
static Hook *ConfTest, *ConfRun, *ServerStats;
#ifndef DYNAMIC_LINKING
ModuleHeader m_htm_Header
#else
#define m_htm_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"htm",	/* Name of module */
	"$Id$", /* Version */
	"command /htm", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Test(ModuleInfo *modinfo)
#else
int    m_htm_Test(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	bcopy(modinfo,&HtmModInfo,modinfo->size);
	ConfTest = HookAddEx(HtmModInfo.handle, HOOKTYPE_CONFIGTEST, htm_config_test);
	return MOD_SUCCESS;
}


/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_htm_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_HTM, TOK_HTM, m_htm, MAXPARA);
	ConfRun = HookAddEx(HtmModInfo.handle, HOOKTYPE_CONFIGRUN, htm_config_run);
	ServerStats = HookAddEx(HtmModInfo.handle, HOOKTYPE_STATS, htm_stats);
#ifndef NO_FDLIST
	LockEventSystem();
	e_lcf = EventAddEx(HtmModInfo.handle, "lcf", LCF, 0, lcf_check, NULL);
	e_htmcalc = EventAddEx(HtmModInfo.handle, "htmcalc", 1, 0, htm_calc, NULL);
	UnlockEventSystem();
#endif
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_htm_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_htm_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_HTM, TOK_HTM, m_htm) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_htm_Header.name);
	}
#ifndef NO_FDLIST
	LockEventSystem();
	EventDel(e_lcf);
	EventDel(e_htmcalc);
	UnlockEventSystem();
#endif
	return MOD_SUCCESS;
}

/* m_htm recoded by griever
parameters

HTM [server] [command] [param]
*/

DLLFUNC int m_htm(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	int  x = HUNTED_NOSUCH;
	char *command, *param;
	if (!IsOper(sptr))
		return 0;


	switch(parc) {
		case 1:
			break;
		case 2:
			x = hunt_server_token_quiet(cptr, sptr, MSG_HTM, TOK_HTM, "%s", 1, parc, parv);
			break;
		case 3:
			x = hunt_server_token_quiet(cptr, sptr, MSG_HTM, TOK_HTM, "%s %s", 1, parc, parv);
			break;
		default:
			x = hunt_server_token_quiet(cptr, sptr, MSG_HTM, TOK_HTM, "%s %s %s", 1, parc, parv);
	}

	switch (x) {
		case HUNTED_NOSUCH:
			command = (parv[1]);
			param = (parv[2]);
			break;
		case HUNTED_ISME:
			command = (parv[2]);
			param = (parv[3]);
			break;
		default:
			return 0;
	}

#ifndef NO_FDLIST

	if (!command)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Current incoming rate: %0.2f kb/s",
		    me.name, parv[0], currentrate);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Current outgoing rate: %0.2f kb/s",
		    me.name, parv[0], currentrate2);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Highest incoming rate: %0.2f kb/s",
		    me.name, parv[0], highest_rate);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Highest outgoing rate: %0.2f kb/s",
		    me.name, parv[0], highest_rate2);
		sendto_one(sptr,
		    ":%s NOTICE %s :*** High traffic mode is currently \2%s\2",
		    me.name, parv[0], (lifesux ? "ON" : "OFF"));
		sendto_one(sptr,
		    ":%s NOTICE %s :*** High traffic mode is currently in \2%s\2 mode",
		    me.name, parv[0], (noisy_htm ? "NOISY" : "QUIET"));
		sendto_one(sptr,
		    ":%s NOTICE %s :*** HTM will be activated if incoming > %i kb/s",
		    me.name, parv[0], LRV);
	}

	else
	{
		if (!stricmp(command, "ON"))
		{
			EventInfo mod;
			lifesux = 1;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now ON.",
			    me.name, parv[0]);
			sendto_ops
			    ("%s (%s@%s) forced High traffic mode to activate",
			    parv[0], sptr->user->username,
			    GetHost(sptr));
			LCF = 60;	/* 60 seconds */
			mod.flags = EMOD_EVERY;
			mod.every = LCF;
			LockEventSystem();
			EventMod(e_lcf, &mod);
			UnlockEventSystem();
		}
		else if (!stricmp(command, "OFF"))
		{
			EventInfo mod;
			lifesux = 0;
			LCF = LOADCFREQ;
			mod.flags = EMOD_EVERY;
			mod.every = LCF;
			LockEventSystem();
			EventMod(e_lcf, &mod);
			UnlockEventSystem();
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now OFF.",
			    me.name, parv[0]);
			sendto_ops
			    ("%s (%s@%s) forced High traffic mode to deactivate",
			    parv[0], sptr->user->username,
			    GetHost(sptr));
		}
		else if (!stricmp(command, "TO"))
		{
			if (!param)
				sendto_one(sptr,
				    ":%s NOTICE %s :You must specify an integer value",
				    me.name, parv[0]);
			else
			{
				int  new_val = atoi(param);
				if (new_val < 10)
					sendto_one(sptr,
					    ":%s NOTICE %s :New value must be > 10",
					    me.name, parv[0]);
				else
				{
					LRV = new_val;
					sendto_one(sptr,
					    ":%s NOTICE %s :New max rate is %dkb/s",
					    me.name, parv[0], LRV);
					sendto_ops
					    ("%s (%s@%s) changed the High traffic mode max rate to %dkb/s",
					    parv[0], sptr->user->username,
					    GetHost(sptr), LRV);
				}
			}
		}
		else if (!stricmp(command, "QUIET"))
		{
			noisy_htm = 0;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now QUIET",
			    me.name, parv[0]);
			sendto_ops("%s (%s@%s) set High traffic mode to QUIET",
			    parv[0], sptr->user->username,
			    GetHost(sptr));
		}

		else if (!stricmp(command, "NOISY"))
		{
			noisy_htm = 1;
			sendto_one(sptr,
			    ":%s NOTICE %s :High traffic mode is now NOISY",
			    me.name, parv[0]);
			sendto_ops("%s (%s@%s) set High traffic mode to NOISY",
			    parv[0], sptr->user->username,
			    GetHost(sptr));
		}
		else
			sendto_one(sptr, ":%s NOTICE %s :Unknown option: %s",
			    me.name, parv[0], command);
	}


#else
	sendto_one(sptr,
	    ":%s NOTICE %s :*** High traffic mode and fdlists are not enabled on this server",
	    me.name, sptr->name);
#endif
	return 0;
}

#ifndef NO_FDLIST

EVENT(lcf_check)
{
	static int lrv;

	lrv = LRV * LCF;
	if ((me.receiveK - lrv >= lastrecvK) || HTMLOCK == 1)
	{
		if (!lifesux)
		{

			lifesux = 1;
			if (noisy_htm)
				sendto_realops
					    ("Entering high-traffic mode (incoming = %0.2f kb/s (LRV = %dk/s, outgoing = %0.2f kb/s currently)",
					    currentrate, LRV,
						   currentrate2);}
			else
			{
				EventInfo mod;
				lifesux++;	/* Ok, life really sucks! */
				LCF += 2;	/* wait even longer */
				mod.flags = EMOD_EVERY;
				mod.every = LCF;
				EventMod(e_lcf, &mod);
				if (noisy_htm)
					sendto_realops
					    ("Still high-traffic mode %d%s (%d delay): %0.2f kb/s",
					    lifesux,
					    (lifesux >
					    9) ? " (TURBO)" :
					    "", (int)LCF, currentrate);
				/* Reset htm here, because its been on a little too long.
				 * Bad Things(tm) tend to happen with HTM on too long -epi */
				if (lifesux > 15)
				{
					EventInfo mod;
					if (noisy_htm)
						sendto_realops
						    ("Resetting HTM and raising limit to: %dk/s\n",
						    LRV + 5);
					LCF = LOADCFREQ;
					mod.flags = EMOD_EVERY;
					mod.every = LCF;
					EventMod(e_lcf, &mod);
					lifesux = 0;
					LRV += 5;
				}
			}
		}
		else
		{
			EventInfo mod;
			LCF = LOADCFREQ;
			mod.flags = EMOD_EVERY;
			mod.every = LCF;
			EventMod(e_lcf, &mod);
			if (lifesux)
			{
				lifesux = 0;
				if (noisy_htm)
					sendto_realops
					    ("Resuming standard operation (incoming = %0.2f kb/s, outgoing = %0.2f kb/s now)",
					    currentrate, currentrate2);
			}
		}
}

EVENT(htm_calc)
{
	static time_t last = 0;
	if (last == 0)
		last = TStime();
	
	if (timeofday - last == 0)
		return;
		
	currentrate =
		   ((float)(me.receiveK -
		    lastrecvK)) / ((float)(timeofday - last));
	currentrate2 =
		   ((float)(me.sendK -
			 lastsendK)) / ((float)(timeofday - last));
	if (currentrate > highest_rate)
			highest_rate = currentrate;
	if (currentrate2 > highest_rate2)
			highest_rate2 = currentrate2;
	last = TStime();
}
#endif

DLLFUNC int htm_config_test(ConfigFile *cf, ConfigEntry *ce, int type, int *errs) {
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;

	if (!strcmp(ce->ce_varname, "htm"))
	{
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!cep->ce_varname)
			{
				config_error("%s:%i: blank set::htm item",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum);	
				errors++;
				continue;
			}
			if (!cep->ce_vardata)
			{
				config_error("%s:%i: set::htm::%s item without value",
					cep->ce_fileptr->cf_filename,
					cep->ce_varlinenum, cep->ce_varname);
				errors++;
				continue;
			}
			if (!strcmp(cep->ce_varname, "mode"))
			{

				if (stricmp(cep->ce_vardata, "noisy") && stricmp(cep->ce_vardata, "quiet"))
				{
					config_error("%s%i: set::htm::mode: illegal mode",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
			}
			else if (!strcmp(cep->ce_varname, "incoming-rate"))
			{
				int value = config_checkval(cep->ce_vardata, CFG_SIZE);
				if (value < 10240)
				{
					config_error("%s%i: set::htm::incoming-rate: must be at least 10kb",
						cep->ce_fileptr->cf_filename, cep->ce_varlinenum);
					errors++;
				}
			}
			else 
			{
				config_error("%s:%i: unknown directive set::htm::%s",
					cep->ce_fileptr->cf_filename, cep->ce_varlinenum,
					cep->ce_varname);
				errors++;
			}
			
		}
		*errs = errors;
		return errors ? -1 : 1;
	}
	else
		return 0;
}

DLLFUNC int htm_config_run(ConfigFile *cf, ConfigEntry *ce, int type) {
	ConfigEntry *cep;
	int errors = 0;

	if (type != CONFIG_SET)
		return 0;
	if (!strcmp(ce->ce_varname, "htm"))
	{
		for (cep = ce->ce_entries; cep; cep = cep->ce_next)
		{
			if (!strcmp(cep->ce_varname, "mode"))
			{
				if (!stricmp(cep->ce_vardata, "noisy"))
					noisy_htm = 1;
				else
					noisy_htm = 0;
			}
			else if (!strcmp(cep->ce_varname, "incoming-rate"))
				LRV = config_checkval(cep->ce_vardata, CFG_SIZE);
		}
		return 1;		
	}
	return 0;
}

DLLFUNC int htm_stats(aClient *sptr, char *stats) {
	if (*stats == 'S') {
		sendto_one(sptr, ":%s %i %s :htm::mode: %s", me.name, RPL_TEXT,
			   sptr->name, noisy_htm ? "noisy" : "quiet");
		sendto_one(sptr, ":%s %i %s :htm::incoming-rate: %d", me.name, RPL_TEXT,
			   sptr->name, LRV);
	}
        return 0;
}
