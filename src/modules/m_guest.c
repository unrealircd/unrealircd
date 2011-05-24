/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_guest.c
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
//#define snprintf _snprintf
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

DLLFUNC int m_guest(aClient *cptr, aClient *sptr, int parc, char *parv[]);
#ifdef GUEST
static Hook *GuestHook = NULL;
#endif
/* Place includes here */

ModuleHeader MOD_HEADER(m_guest)
  = {
	"guest",	/* Name of module */
	"$Id$", /* Version */
	"command /guest", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

ModuleInfo *ModGuestInfo;
/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_guest)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
#ifdef GUEST
	ModGuestInfo = modinfo;
	GuestHook = HookAddEx(ModGuestInfo->handle, HOOKTYPE_GUEST, m_guest);
#endif
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_guest)(int module_load)
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_guest)(int module_unload)
{
#ifdef GUEST
	HookDel(GuestHook);
#endif
	return MOD_SUCCESS;
}

DLLFUNC int m_guest(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char guestnick[NICKLEN];
	char *param[3];

	snprintf(guestnick, NICKLEN, "Guest%d", getrandom32());

	while(find_client(guestnick, (aClient *)NULL))
	{ 
		snprintf(guestnick, NICKLEN, "Guest%d", getrandom32());
	}
	param[0] = sptr->name;
	param[1] = guestnick;
	param[2] = NULL;
	do_cmd(sptr, cptr, "NICK", 2, param);
	return 0;
}
