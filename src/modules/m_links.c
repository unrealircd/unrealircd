/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

DLLFUNC int m_links(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_LINKS 	"LINKS"	
#define TOK_LINKS 	"0"	

ModuleHeader MOD_HEADER(m_links)
  = {
	"m_links",
	"$Id$",
	"command /links", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_links)(ModuleInfo *modinfo)
{
	add_Command(MSG_LINKS, TOK_LINKS, m_links, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_links)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_links)(int module_unload)
{
	if (del_Command(MSG_LINKS, TOK_LINKS, m_links) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_links).name);
	}
	return MOD_SUCCESS;
}

/*
** m_links
**	parv[0] = sender prefix
** or
**	parv[0] = sender prefix
**
** Recoded by Stskeeps
*/
DLLFUNC CMD_FUNC(m_links)
{
	Link *lp;
	aClient *acptr;

	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;

		/* Some checks */
		if (HIDE_ULINES && IsULine(acptr) && !IsAnOper(sptr))
			continue;
		sendto_one(sptr, rpl_str(RPL_LINKS),
		    me.name, parv[0], acptr->name, acptr->serv->up,
		    acptr->hopcount, (acptr->info[0] ? acptr->info :
		    "(Unknown Location)"));
	}

	sendto_one(sptr, rpl_str(RPL_ENDOFLINKS), me.name, parv[0], "*");
	return 0;
}
