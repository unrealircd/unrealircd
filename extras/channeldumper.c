/*
 *   IRC - Internet Relay Chat, src/modules/channeldumper.c
 *   (C) 2002 Carsten V. Munk <stskeeps@tspre.org>
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

#ifndef DYNAMIC_LINKING
ModuleHeader channeldumper_Header
#else
#define channeldumper_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"channeldumper",
	"$Id$",
	"Channel dump to text timed", 
	"3.2-b8-1",
	NULL 
    };

static ModuleInfo	ChannelDumperModInfo;

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    channeldumper_Init(ModuleInfo *modinfo)
#endif
{
	tainted++;
	bcopy(modinfo,&ChannelDumperModInfo, modinfo->size);
	return MOD_SUCCESS;
}

EVENT(e_channeldump);
static Event *ChannelDumpEvent = NULL;
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    channeldumper_Load(int module_load)
#endif
{
	LockEventSystem();
	ChannelDumpEvent = EventAddEx(ChannelDumperModInfo.handle, "e_channeldump", 5, 0, e_channeldump, NULL);
	UnlockEventSystem();
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	channeldumper_Unload(int module_unload)
#endif
{
	tainted--;
	LockEventSystem();
	EventDel(ChannelDumpEvent);
	UnlockEventSystem();
	return MOD_SUCCESS;
}

EVENT(e_channeldump)
{
	aChannel *chptr;
	unsigned int hashnum;
	Member	*m;
	FILE	*f;
	
	f = fopen("ircd.channeldump", "w");
	if (!f)
		return;
	for (hashnum = 0; hashnum < CH_MAX; hashnum++)
	{
		for (chptr = (aChannel *)hash_get_chan_bucket(hashnum); chptr; chptr
			= chptr->hnextch)
		{
			if (SecretChannel(chptr))
				continue;
		 	fprintf(f, "C %s %s\r\n", 
		 		chptr->chname, chptr->topic ? chptr->topic : "");
			for (m = chptr->members; m; m = m->next)
				fprintf(f, "M %s\r\n",
					m->cptr->name);   			
		}
	}
	fclose(f);
	return;
}