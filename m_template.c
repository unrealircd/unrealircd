/*
 *   IRC - Internet Relay Chat, src/modules/%FILE%
 *   (C) 2001 The UnrealIRCd Team
 *
 *   %DESC%
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

DLLFUNC int m_%COMMAND%(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_%UCOMMAND% 	"%UCOMMAND%"	
#define TOK_%UCOMMAND% 	"%TOKEN%"	


#ifndef DYNAMIC_LINKING
ModuleInfo m_%COMMAND%_info
#else
#define m_%COMMAND%_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"test",
	"$Id$",
	"command /%COMMAND%", 
	NULL,
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_%COMMAND%_init(int module_load)
#endif
{
	add_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%, %MAXPARA%);
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_%COMMAND%_load(int module_load)
#endif
{
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_%COMMAND%_unload(void)
#endif
{
	if (del_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_%COMMAND%_info.name);
	}
}
