/*
 *   IRC - Internet Relay Chat, src/modules/m_setname.c
 *   (c) 1999-2001 Dominick Meglio (codemastr) <codemastr@unrealircd.com>
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

DLLFUNC int m_setname(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SETNAME 	"SETNAME"	/* setname */
#define TOK_SETNAME 	"AE"	

#ifndef DYNAMIC_LINKING
ModuleHeader m_setname_Header
#else
#define m_setname_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"setname",	/* Name of module */
	"$Id$", /* Version */
	"command /setname", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    m_setname_Init(ModuleInfo *modinfo)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SETNAME, TOK_SETNAME, m_setname, 1);
	return MOD_SUCCESS;
	
}
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_setname_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
	
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_setname_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SETNAME, TOK_SETNAME, m_setname) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_setname_Header.name);
	}
	return MOD_SUCCESS;
	
}

/* m_setname - 12/05/1999 - Stskeeps
 * :prefix SETNAME :gecos
 * parv[0] - sender
 * parv[1] - gecos
 * D: This will set your gecos to be <x> (like (/setname :The lonely wanderer))
   yes it is experimental but anyways ;P
    FREEDOM TO THE USERS! ;) 
*/ 
DLLFUNC int m_setname(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
        if (parc < 2)
                return 0;
        if (strlen(parv[1]) > (REALLEN))
       {
                if (MyConnect(sptr))
                {
                        sendto_one(sptr,
                            ":%s NOTICE %s :*** /SetName Error: \"Real names\" may maximum be %i characters of length",
                            me.name, sptr->name, REALLEN);
                }
                return 0;
        }
        if (strlen(parv[1]) < 1)
        {
                sendto_one(sptr,
                    ":%s NOTICE %s :Couldn't change realname - Nothing in parameter",
		    me.name, sptr->name);
                return 0;
        }
        /* set the new name before we check, but don't send to servers unless it is ok */
        else
                ircsprintf(sptr->info, "%s", parv[1]);
        /* Check for n:lines here too */
        if (!IsAnOper(sptr) && Find_ban(sptr->info, CONF_BAN_REALNAME))
        {
                int xx;
                xx =
                    exit_client(cptr, sptr, &me,
                    "Your GECOS (real name) is banned from this server");
                return xx;
        }
        sendto_serv_butone_token(cptr, sptr->name, MSG_SETNAME, TOK_SETNAME,
            ":%s", parv[1]);
        if (MyConnect(sptr))
                sendto_one(sptr,
                    ":%s NOTICE %s :Your \"real name\" is now set to be %s - you have to set it manually to undo it",
                    me.name, parv[0], parv[1]);
        return 0;
}
