/*
 *   IRC - Internet Relay Chat, src/modules/%FILE%
 *   (C) 2004 The UnrealIRCd Team
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
#include "proto.h"
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

DLLFUNC int m_%COMMAND%(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_%UCOMMAND% 	"%UCOMMAND%"	
#define TOK_%UCOMMAND% 	"%TOKEN%"	

ModuleHeader MOD_HEADER(m_%COMMAND%)
  = {
	"m_%COMMAND%",
	"$Id$",
	"command /%COMMAND%", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_%COMMAND%)(ModuleInfo *modinfo)
{
	add_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%, %MAXPARA%);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_%COMMAND%)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_%COMMAND%)(int module_unload)
{
	if (del_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_%COMMAND%).name);
	}
	return MOD_SUCCESS;
}

