/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_sendumode.c
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_sendumode(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SENDUMODE   "SENDUMODE"
#define TOK_SENDUMODE   "AP"
#define MSG_SMO         "SMO"
#define TOK_SMO         "AU"

extern int sno_mask[]; /* someone is going to make this static, I just know it */

#ifndef DYNAMIC_LINKING
ModuleHeader m_sendumode_Header
#else
#define m_sendumode_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"sendumode",	/* Name of module */
	"$Id$", /* Version */
	"command /sendumode", /* Short description of module */
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
int    m_sendumode_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SENDUMODE, TOK_SENDUMODE, m_sendumode, MAXPARA);
	add_Command(MSG_SMO, TOK_SMO, m_sendumode, MAXPARA);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_sendumode_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_sendumode_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SENDUMODE, TOK_SENDUMODE, m_sendumode) < 0)
	{
		sendto_realops("Failed to delete command sendumode when unloading %s",
				m_sendumode_Header.name);
	}
	if (del_Command(MSG_SMO, TOK_SMO, m_sendumode) < 0)
	{
		sendto_realops("Failed to delete command smo when unloading %s",
				m_sendumode_Header.name);
	}
	return MOD_SUCCESS;
	

}

/*
** m_sendumode - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = target
**      parv[2] = message text
** Pretty handy proc.. 
** Servers can use this to f.x:
**   :server.unreal.net SENDUMODE F :Client connecting at server server.unreal.net port 4141 usw..
** or for sending msgs to locops.. :P
*/
DLLFUNC int m_sendumode(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *message;
	char *p;
	int i;
	long umode_s = 0;
	long snomask = 0;
	int and = 0;

	aClient* acptr;

	message = (parc > 3) ? parv[3] : parv[2];

	if (parc < 3)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "SENDUMODE");
		return 0;
	}

	if (!IsServer(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	sendto_serv_butone(IsServer(cptr) ? cptr : NULL,
	    ":%s SMO %s :%s", parv[0], parv[1], message);

	for (p = parv[1]; *p; p++)
	{
		umode_s = 0;
		
		for(i = 0; Usermode_Table[i].flag; i++)
		{
			if (Usermode_Table[i].flag == *p)
			{
				umode_s |= Usermode_Table[i].mode;
			}
		}
		if (Usermode_Table[i].flag)
			break;

		for (i = 1; sno_mask[i]; i += 2)
		{
			if (sno_mask[i] ==  *p) 	
			{
				snomask |= sno_mask[i - 1];
				break;
			}
		}
	}

	if (parc > 3)
	    for(p = parv[2]; *p; p++)
	{
		for (i = 1; sno_mask[i]; i += 2)
		{
			if (sno_mask[i] ==  *p) 	
			{
				snomask |= sno_mask[i - 1];
				break;
			}
		}
	}

	for(i = 0; i <= LastSlot; i++)
	    if((acptr = local[i]) && IsPerson(acptr) && ((acptr->user->snomask & snomask) ||
		(acptr->umodes & umode_s)))
	{
		sendto_one(acptr, ":%s NOTICE %s :%s", me.name, acptr->name, message);
	}

	return 0;
}

