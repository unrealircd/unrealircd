/*
 *   IRC - Internet Relay Chat, src/modules/m_svslusers.c
 *   (C) 2002 codemastr [Dominick Meglio] (codemastr@unrealircd.com)
 *
 *   SVSLUSERS command, allows remote setting of local and global max user count
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
DLLFUNC int m_svslusers(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSLUSERS 	"SVSLUSERS"	
#define TOK_SVSLUSERS 	"BU"	

ModuleHeader MOD_HEADER(m_svslusers)
  = {
	"m_svslusers",
	"$Id$",
	"command /svslusers", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_svslusers)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SVSLUSERS, TOK_SVSLUSERS, m_svslusers, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svslusers)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svslusers)(int module_unload)
{
	return MOD_SUCCESS;
}
/*
** m_svslusers
**      parv[0] = sender
**      parv[1] = server to update
**      parv[2] = max global users
**      parv[3] = max local users
**      If -1 is specified for either number, it is ignored and the current count
**      is kept.
*/
int  m_svslusers(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        if (!IsULine(sptr) || parc < 4)
		return -1;  
        if (hunt_server_token(cptr, sptr, MSG_SVSLUSERS, TOK_SVSLUSERS, "%s %s :%s", 1, parc,
		parv) == HUNTED_ISME)
        {
		int temp;
		temp = atoi(parv[2]);
		if (temp >= 0)
			IRCstats.global_max = temp;
		temp = atoi(parv[3]);
		if (temp >= 0) 
			IRCstats.me_max = temp;
        }
        return 0;
}
