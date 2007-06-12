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

ModuleHeader MOD_HEADER(m_setname)
  = {
	"setname",	/* Name of module */
	"$Id$", /* Version */
	"command /setname", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_setname)(ModuleInfo *modinfo)
{
	/*
	 * We call our CommandAdd crap here
	*/
	CommandAdd(modinfo->handle, MSG_SETNAME, TOK_SETNAME, m_setname, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_setname)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_setname)(int module_unload)
{
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
DLLFUNC CMD_FUNC(m_setname)
{
    int xx;
    char tmpinfo[REALLEN + 1];
    char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64];

 	if ((parc < 2) || BadPtr(parv[1]))
 	{
 		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SETNAME");
 		return 0;
	}

	if (strlen(parv[1]) > REALLEN)
	{
		if (MyConnect(sptr))
		{
			sendnotice(sptr, "*** /SetName Error: \"Real names\" may maximum be %i characters of length",
				REALLEN);
		}
		return 0;
	}

    /* set temp info for spamfilter check*/
    strcpy(tmpinfo, sptr->info);
    /* set the new name before we check, but don't send to servers unless it is ok */
    strcpy(sptr->info, parv[1]);
    spamfilter_build_user_string(spamfilter_user, sptr->name, sptr);
    xx = dospamfilter(sptr, spamfilter_user, SPAMF_USER, NULL, 0, NULL);
    if (xx < 0) {
        if (sptr)
            strcpy(sptr->info, tmpinfo);
        return xx;
    }

	/* Check for n:lines here too */
	if (!IsAnOper(sptr) && Find_ban(NULL, sptr->info, CONF_BAN_REALNAME))
		return exit_client(cptr, sptr, &me,
		                   "Your GECOS (real name) is banned from this server");

	sendto_serv_butone_token(cptr, sptr->name, MSG_SETNAME, TOK_SETNAME, ":%s", parv[1]);

	if (MyConnect(sptr))
		sendnotice(sptr, "Your \"real name\" is now set to be %s - you have to set it manually to undo it",
			parv[1]);

	return 0;
}
