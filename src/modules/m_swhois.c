/*
 *   IRC - Internet Relay Chat, src/modules/m_swhois.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SWHOIS command
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

DLLFUNC int m_swhois(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SWHOIS 	"SWHOIS"	
#define TOK_SWHOIS 	"BA"	

ModuleHeader MOD_HEADER(m_swhois)
  = {
	"m_swhois",
	"$Id$",
	"command /swhois", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_swhois)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SWHOIS, TOK_SWHOIS, m_swhois, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_swhois)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_swhois)(int module_unload)
{
	return MOD_SUCCESS;
}
/*
 * m_swhois
 * parv[1] = nickname
 * parv[2] = new swhois
 *
*/

int m_swhois(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient *acptr;

        if (!(IsServer(sptr) || IsULine(sptr)))
                return 0;
        if (parc < 3)
                return 0;
        acptr = find_person(parv[1], (aClient *)NULL);
        if (!acptr)
                return 0;

        if (acptr->user->swhois)
                MyFree(acptr->user->swhois);
        acptr->user->swhois = MyMalloc(strlen(parv[2]) + 1);
        strcpy(acptr->user->swhois, parv[2]);
        sendto_serv_butone_token(cptr, sptr->name,
           MSG_SWHOIS, TOK_SWHOIS, "%s :%s", parv[1], parv[2]);
        return 0;
}
