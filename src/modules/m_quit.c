/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_quit.c
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

DLLFUNC int m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_QUIT        "QUIT"  /* QUIT */
#define TOK_QUIT        ","     /* 44 */


#ifndef DYNAMIC_LINKING
ModuleHeader m_quit_Header
#else
#define m_quit_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"quit",	/* Name of module */
	"$Id$", /* Version */
	"command /quit", /* Short description of module */
	"3.2-b5",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_quit_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_CommandX(MSG_QUIT, TOK_QUIT, m_quit, 1, M_UNREGISTERED|M_USER);
	return MOD_SUCCESS;
	
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_quit_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_quit_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_QUIT, TOK_QUIT, m_quit) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_quit_Header.name);
	}
	return MOD_SUCCESS;
	
}


/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
DLLFUNC int  m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *ocomment = (parc > 1 && parv[1]) ? parv[1] : parv[0];
	static char comment[TOPICLEN];

	if (!IsServer(cptr))
	{
		ircsprintf(comment, "%s ",
		    BadPtr(prefix_quit) ? "Quit:" : prefix_quit);

#ifdef CENSOR_QUIT
		ocomment = (char *)stripbadwords_channel(ocomment);
#endif
		if (!IsAnOper(sptr) && ANTI_SPAM_QUIT_MSG_TIME)
			if (sptr->firsttime+ANTI_SPAM_QUIT_MSG_TIME > TStime())
				ocomment = parv[0];
		strncpy(comment + strlen(comment), ocomment, TOPICLEN - 7);
		comment[TOPICLEN] = '\0';
		return exit_client(cptr, sptr, sptr, comment);
	}
	else
	{
		return exit_client(cptr, sptr, sptr, ocomment);
	}
}

