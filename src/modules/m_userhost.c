/*
 *   IRC - Internet Relay Chat, src/modules/m_userhost.c
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

DLLFUNC int m_userhost(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_USERHOST 	"USERHOST"	
#define TOK_USERHOST 	"J"	

ModuleHeader MOD_HEADER(m_userhost)
  = {
	"m_userhost",
	"$Id$",
	"command /userhost", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_userhost)(ModuleInfo *modinfo)
{
	add_Command(MSG_USERHOST, TOK_USERHOST, m_userhost, 1);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_userhost)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_userhost)(int module_unload)
{
	if (del_Command(MSG_USERHOST, TOK_USERHOST, m_userhost) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_userhost).name);
	}
	return MOD_SUCCESS;
}

/*
 * m_userhost added by Darren Reed 13/8/91 to aid clients and reduce
 * the need for complicated requests like WHOIS. It returns user/host
 * information only (no spurious AWAY labels or channels).
 * Re-written by Dianora 1999
 */
DLLFUNC CMD_FUNC(m_userhost)
{

	char *p;		/* scratch end pointer */
	char *cn;		/* current name */
	struct Client *acptr;
	char response[5][NICKLEN * 2 + CHANNELLEN + USERLEN + HOSTLEN + 30];
	int  i;			/* loop counter */

	if (parc < 2)
	{
		sendto_one(sptr, rpl_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "USERHOST");
		return 0;
	}

	/* The idea is to build up the response string out of pieces
	 * none of this strlen() nonsense.
	 * 5 * (NICKLEN*2+CHANNELLEN+USERLEN+HOSTLEN+30) is still << sizeof(buf)
	 * and our ircsprintf() truncates it to fit anyway. There is
	 * no danger of an overflow here. -Dianora
	 */
	response[0][0] = response[1][0] = response[2][0] =
	    response[3][0] = response[4][0] = '\0';

	cn = parv[1];

	for (i = 0; (i < 5) && cn; i++)
	{
		if ((p = strchr(cn, ' ')))
			*p = '\0';

		if ((acptr = find_person(cn, NULL)))
		{
			ircsprintf(response[i], "%s%s=%c%s@%s",
			    acptr->name,
			    (IsAnOper(acptr) && (!IsHideOper(acptr) || sptr == acptr || IsAnOper(sptr)))
				? "*" : "",
			    (acptr->user->away) ? '-' : '+',
			    acptr->user->username,
			    ((acptr != sptr) && !IsOper(sptr)
			    && IsHidden(acptr) ? acptr->user->virthost :
			    acptr->user->realhost));
		}
		if (p)
			p++;
		cn = p;
	}

	sendto_one(sptr, rpl_str(RPL_USERHOST), me.name, parv[0],
	    response[0], response[1], response[2], response[3], response[4]);

	return 0;
}
