/*
 *   IRC - Internet Relay Chat, src/modules/m_ison.c
 *   (C) 2004 The UnrealIRCd Team
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

DLLFUNC int m_ison(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_ISON 	"ISON"	
#define TOK_ISON 	"K"	

ModuleHeader MOD_HEADER(m_ison)
  = {
	"m_ison",
	"$Id$",
	"command /ison", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_ison)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_ISON, TOK_ISON, m_ison, 1, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_ison)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_ison)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * m_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

static char buf[BUFSIZE];
DLLFUNC CMD_FUNC(m_ison)
{
	char namebuf[USERLEN + HOSTLEN + 4];
	aClient *acptr;
	char *s, **pav = parv, *user;
	int  len;
	char *p = NULL;


	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ISON");
		return 0;
	}

	(void)ircsprintf(buf, rpl_str(RPL_ISON), me.name, *parv);
	len = strlen(buf);

	for (s = strtoken(&p, *++pav, " "); s; s = strtoken(&p, NULL, " "))
	{
		if ((user = index(s, '!')))
			*user++ = '\0';
		if ((acptr = find_person(s, NULL)))
		{
			if (user)
			{
				strcpy(namebuf, acptr->user->username);
				strcat(namebuf, "@");
				strcat(namebuf, GetHost(acptr));
				if (match(user, namebuf))
					continue;
				*--user = '!';
			}

			(void)strncat(buf, s, sizeof(buf) - len);
			len += strlen(s);
			(void)strncat(buf, " ", sizeof(buf) - len);
			len++;
		}
	}

	sendto_one(sptr, "%s", buf);
	return 0;
}
