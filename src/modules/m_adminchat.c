/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_adminchat.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *    Moved to modules by Fish (Justin Hammond)
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

DLLFUNC int m_admins(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_ADMINCHAT	"ADCHAT"
#define TOK_ADMINCHAT	"x"


ModuleHeader MOD_HEADER(m_adminchat)
  = {
	"adminchat",	/* Name of module */
	"$Id$", /* Version */
	"command /adchat", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_adminchat)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_ADMINCHAT, TOK_ADMINCHAT, m_admins, 1);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_adminchat)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_adminchat)(int module_unload)
{
	if (del_Command(MSG_ADMINCHAT, TOK_ADMINCHAT, m_admins) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_adminchat).name);
	}
	return MOD_SUCCESS;
}

/*
** m_admins (Admin chat only) -Potvin
**      parv[0] = sender prefix
**      parv[1] = message text
*/
DLLFUNC int m_admins(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADCHAT");
		return 0;
	}
#ifdef ADMINCHAT
	if (MyClient(sptr) && !IsAdmin(sptr))
#else
	if (MyClient(sptr))
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
   	    MSG_ADMINCHAT, TOK_ADMINCHAT, ":%s", message);	
#ifdef ADMINCHAT
	sendto_umode(UMODE_ADMIN, "*** AdminChat -- from %s: %s",
	    parv[0], message);
	sendto_umode(UMODE_COADMIN, "*** AdminChat -- from %s: %s",
	    parv[0], message);
#endif
	return 0;
}
