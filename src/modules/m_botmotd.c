/*
 *   IRC - Internet Relay Chat, src/modules/m_botmotd.c
 *   (C) 2005 The UnrealIRCd Team
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
#include "proto.h"
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

DLLFUNC CMD_FUNC(m_botmotd);

#define MSG_BOTMOTD 	"BOTMOTD"	
#define TOK_BOTMOTD 	"BF"	

ModuleHeader MOD_HEADER(m_botmotd)
  = {
	"m_botmotd",
	"$Id$",
	"command /botmotd", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_botmotd)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_BOTMOTD, TOK_BOTMOTD, m_botmotd, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_botmotd)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_botmotd)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * Modified from comstud by codemastr
 */
DLLFUNC CMD_FUNC(m_botmotd)
{
	aMotdLine *motdline;
	ConfigItem_tld *tld;
	char userhost[HOSTLEN + USERLEN + 6];

	if (hunt_server_token(cptr, sptr, MSG_BOTMOTD, TOK_BOTMOTD, ":%s", 1, parc,
	    parv) != HUNTED_ISME)
		return 0;

	if (!IsPerson(sptr))
		return 0;

	strlcpy(userhost, make_user_host(sptr->user->username, sptr->user->realhost), sizeof(userhost));
	tld = Find_tld(sptr, userhost);

	motdline = NULL;
	if (tld)
		motdline = tld->botmotd.lines;
	if (!motdline)
		motdline = botmotd.lines;

	if (!motdline)
	{
		sendto_one(sptr, ":%s NOTICE %s :BOTMOTD File not found",
		    me.name, sptr->name);
		return 0;
	}
	sendto_one(sptr, ":%s NOTICE %s :- %s Bot Message of the Day - ",
	    me.name, sptr->name, me.name);

	while (motdline)
	{
		sendto_one(sptr, ":%s NOTICE %s :- %s", me.name, sptr->name, motdline->line);
		motdline = motdline->next;
	}
	sendto_one(sptr, ":%s NOTICE %s :End of /BOTMOTD command.", me.name, sptr->name);
	return 0;
}
