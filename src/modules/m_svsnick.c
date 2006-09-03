/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnick.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnick command
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

DLLFUNC int m_svsnick(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSNICK 	"SVSNICK"	
#define TOK_SVSNICK 	"e"	

ModuleHeader MOD_HEADER(m_svsnick)
  = {
	"m_svsnick",
	"$Id$",
	"command /svsnick", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_svsnick)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSNICK, TOK_SVSNICK, m_svsnick, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsnick)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsnick)(int module_unload)
{
	if (del_Command(MSG_SVSNICK, TOK_SVSNICK, m_svsnick) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svsnick).name);
	}
	return MOD_SUCCESS;
}
/*
** m_svsnick
**      parv[0] = sender
**      parv[1] = old nickname
**      parv[2] = new nickname
**      parv[3] = timestamp
*/
int  m_svsnick(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
aClient *acptr;
aClient *ocptr; /* Other client */

	if (!IsULine(sptr) || parc < 4 || (strlen(parv[2]) > NICKLEN))
		return -1; /* This looks like an error anyway -Studded */

	if (hunt_server_token(cptr, sptr, MSG_SVSNICK, TOK_SVSNICK, "%s %s :%s", 1, parc, parv) != HUNTED_ISME)
		return 0; /* Forwarded, done */

	if (do_nick_name(parv[2]) == 0)
		return 0;

	if (!(acptr = find_person(parv[1], NULL)))
		return 0; /* User not found, bail out */

	if ((ocptr = find_client(parv[2], NULL)) && ocptr != acptr) /* Collision */
	{
		exit_client(acptr, acptr, sptr,
		                   "Nickname collision due to Services enforced "
		                   "nickname change, your nick was overruled");
		return 0;
	}

	if (acptr != ocptr)
		acptr->umodes &= ~UMODE_REGNICK;
	acptr->lastnick = TS2ts(parv[3]);
	sendto_common_channels(acptr, ":%s NICK :%s", parv[1], parv[2]);
	add_history(acptr, 1);
	sendto_serv_butone_token(NULL, parv[1], MSG_NICK, TOK_NICK,
	                         "%s :%ld", parv[2], TS2ts(parv[3]));

	(void)del_from_client_hash_table(acptr->name, acptr);
	if (acptr != ocptr)
		hash_check_watch(acptr, RPL_LOGOFF);

	sendto_snomask(SNO_NICKCHANGE,
		"*** Notice -- %s (%s@%s) has been forced to change his/her nickname to %s", 
		acptr->name, acptr->user->username, acptr->user->realhost, parv[2]);
	RunHook2(HOOKTYPE_LOCAL_NICKCHANGE, acptr, parv[2]);

	strlcpy(acptr->name, parv[2], sizeof acptr->name);
	add_to_client_hash_table(parv[2], acptr);
	if (acptr != ocptr)
		hash_check_watch(acptr, RPL_LOGON);

	return 0;
}
