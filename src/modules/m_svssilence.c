/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svssilence.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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
#include <sys/timeb.h>
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_svssilence(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SVSSILENCE       "SVSSILENCE"
#define TOK_SVSSILENCE       "Bs"

ModuleHeader MOD_HEADER(m_svssilence)
  = {
	"svssilence",	/* Name of module */
	"$Id$", /* Version */
	"command /svssilence", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_svssilence)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SVSSILENCE, TOK_SVSSILENCE, m_svssilence, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_svssilence)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_svssilence)(int module_unload)
{
	if (del_Command(MSG_SVSSILENCE, TOK_SVSSILENCE, m_svssilence) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svssilence).name);
	}
	return MOD_SUCCESS;	
}

/* m_svssilence()
 * written by Syzop (copied a lot from m_silence),
 * suggested by <??>.
 * parv[0] - sender
 * parv[1] - target nick
 * parv[2] - space delimited silence list (+Blah +Blih -Bluh etc)
 * SERVER DISTRIBUTION:
 * In contrast to the normal SILENCE command, this is sent to all servers
 * because it can/will add&remove multiple silence entries at once.
 */
CMD_FUNC(m_svssilence)
{
	aClient *acptr;
	int mine;
	char *p, *cp, c;
	
	if (!IsULine(sptr))
		return 0;

	if (parc < 3 || BadPtr(parv[2]) || !(acptr = find_person(parv[1], NULL)))
		return 0;
	
	sendto_serv_butone_token(sptr, parv[0], MSG_SVSSILENCE, TOK_SVSSILENCE, "%s :%s", parv[1], parv[2]);

	mine = MyClient(acptr) ? 1 : 0;

	for (p = strtok(parv[2], " "); p; p = strtok(NULL, " "))
	{
		c = *p;
		if ((c == '-') || (c == '+'))
			p++;
		else if (!(index(p, '@') || index(p, '.') ||
		    index(p, '!') || index(p, '*')))
		{
			/* "no such nick" */
			continue;
		}
		else
			c = '+';
		cp = pretty_mask(p);
		if ((c == '-' && !del_silence(acptr, cp)) ||
			(c != '-' && !add_silence(acptr, cp, 0)))
		{
			if (mine)
				sendto_prefix_one(acptr, acptr, ":%s SILENCE %c%s", parv[0], c, cp);
		}
	}
	return 0;
}
