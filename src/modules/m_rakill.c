/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_rakill.c
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

DLLFUNC int m_rakill(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_RAKILL      "RAKILL"        /* RAKILL */
#define TOK_RAKILL      "Y"     /* 89 */

ModuleHeader MOD_HEADER(m_rakill)
  = {
	"rakill",	/* Name of module */
	"$Id$", /* Version */
	"command /rakill", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_rakill)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_RAKILL, TOK_RAKILL, m_rakill, MAXPARA);
	ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_rakill)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_rakill)(int module_unload)
{
	if (del_Command(MSG_RAKILL, TOK_RAKILL, m_rakill) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_rakill).name);
	}
	return MOD_SUCCESS;
}

/*
** m_rakill;
**      parv[0] = sender prefix
**      parv[1] = hostmask
**      parv[2] = username
**      parv[3] = comment
*/
DLLFUNC int m_rakill(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *tkllayer[6] = {
		me.name,	/*0  server.name */
		"-",		/*1  - */
		"G",		/*2  G   */
		NULL,		/*3  user */
		NULL,		/*4  host */
		NULL		/*5  whoremoved */
	};

	if (parc < 2 && IsPerson(sptr))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "RAKILL");
		return 0;
	}

	if (IsServer(sptr) && parc < 3)
		return 0;

	if (!IsServer(cptr))
	{
		if (!IsOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    sptr->name);
		}
		else
		{
			sendto_one(sptr, ":%s NOTICE %s :*** RAKILL is depreciated and should not be used. Please use /gline -user@host instead", 
				me.name, sptr->name);
		}
		return 0;
	}
	
	tkllayer[3] = parv[2];
	tkllayer[4] = parv[1];	
	tkllayer[5] = sptr->name;
	m_tkl(&me, &me, 6, tkllayer);
	loop.do_bancheck = 1;
	return 0;
}
