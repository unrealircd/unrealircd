/*
 *   Unreal Internet Relay Chat Daemon, m_rawto.c
 *   (C) 2002 Carsten V. Munk
 *   RAWTO Module - 3rd party
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
 * 
 *   Use of this module will make it a 3rd party module, and will
 *   add to your /version thing. We DO NOT SUPPORT THIS.
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

DLLFUNC int m_rawto(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_RAWTO       "RAWTO" /*  */
#define TOK_RAWTO       "3A"     /* 112 */


#ifndef DYNAMIC_LINKING
ModuleHeader m_rawto_Header
#else
#define m_rawto_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"rawto",	/* Name of module */
	"$Id$", /* Version */
	"command /rawto", /* Short description of module */
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
int    m_rawto_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_RAWTO, TOK_RAWTO, m_rawto, 2);
	tainted++;
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_rawto_Load(int module_load)
#endif
{
	
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_rawto_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_RAWTO, TOK_RAWTO, m_rawto) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_rawto_Header.name);
	}
	tainted--;
	return MOD_SUCCESS;
}


/*
 *  m_rawto                      Send a raw string to anywhere
 *                               if you are U:line
 *    parv[0] = sender prefix
 *    parv[1] = whoto
 *    parv[2] = string
 */

DLLFUNC int m_rawto(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr = NULL;
	if (!IsULine(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}
	if (parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "RAWTO");
		return -1;
	}
	if ((acptr = find_client(parv[1], NULL)))
	{
		if (MyClient(acptr))
		{
			sendto_one(acptr, "%s", parv[2]);
			return 0;
		}		
		else
		{
			sendto_one(acptr, ":%s %s %s :%s",
				parv[0], IsToken(acptr->from) ? TOK_RAWTO : MSG_RAWTO,
					parv[1], parv[2]);
			return 0;
		}
	}
	return 0;
}
