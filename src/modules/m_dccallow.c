/*
 *   IRC - Internet Relay Chat, src/modules/m_dccallow
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

DLLFUNC int m_dccallow(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_DCCALLOW 	"DCCALLOW"
#define TOK_DCCALLOW 	""

ModuleHeader MOD_HEADER(m_dccallow)
  = {
	"m_dccallow",
	"$Id$",
	"command /dccallow", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_dccallow)(ModuleInfo *modinfo)
{
	add_Command(MSG_DCCALLOW, TOK_DCCALLOW, m_dccallow, 1);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_dccallow)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_dccallow)(int module_unload)
{
	if (del_Command(MSG_DCCALLOW, TOK_DCCALLOW, m_dccallow) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_dccallow).name);
	}
	return MOD_SUCCESS;
}

/* m_dccallow:
 * HISTORY:
 * Taken from bahamut 1.8.1
 */
DLLFUNC int m_dccallow(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
Link *lp;
char *p, *s;
aClient *acptr, *lastcptr = NULL;
int didlist = 0, didhelp = 0, didanything = 0;
char **ptr;
static char *dcc_help[] =
{
	"/DCCALLOW [<+|->nick[,<+|->nick, ...]] [list] [help]",
	"You may allow DCCs of files which are otherwise blocked by the IRC server",
	"by specifying a DCC allow for the user you want to recieve files from.",
	"For instance, to allow the user Bob to send you file.exe, you would type:",
	"/DCCALLOW +bob",
	"and Bob would then be able to send you files. Bob will have to resend the file",
	"if the server gave him an error message before you added him to your allow list.",
	"/DCCALLOW -bob",
	"Will do the exact opposite, removing him from your dcc allow list.",
	"/dccallow list",
	"Will list the users currently on your dcc allow list.",
	NULL
};

	if (!MyClient(sptr))
		return 0;
	
	if (parc < 2)
	{
		sendnotice(sptr, "No command specified for DCCALLOW. "
			"Type '/DCCALLOW HELP' for more information.");
		return 0;
	}

	for (p = NULL, s = strtoken(&p, parv[1], ", "); s; s = strtoken(&p, NULL, ", "))
	{
		if (*s == '+')
		{
			didanything = 1;
			if (!*++s)
				continue;
			
			acptr = find_person(s, NULL);
			
			if (acptr == sptr)
				continue;
			
			if (!acptr)
			{
				sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, s);
				continue;
			}
			add_dccallow(sptr, acptr);
		} else
		if (*s == '-')
		{
			didanything = 1;
			if (!*++s)
				continue;
			
			acptr = find_person(s, NULL);
			if (acptr == sptr)
				continue;
			if (!acptr)
			{
				sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, s);
				continue;
			}
			del_dccallow(sptr, acptr);
		} else
		if (!didlist && !myncmp(s, "list", 4))
		{
			didanything = didlist = 1;
			sendto_one(sptr, ":%s %d %s :The following users are on your dcc allow list:",
				me.name, RPL_DCCINFO, sptr->name);
			for(lp = sptr->user->dccallow; lp; lp = lp->next)
			{
				if (lp->flags == DCC_LINK_REMOTE)
					continue;
				sendto_one(sptr, ":%s %d %s :%s (%s@%s)", me.name,
					RPL_DCCLIST, sptr->name, lp->value.cptr->name,
					lp->value.cptr->user->username,
					GetHost(lp->value.cptr));
			}
			sendto_one(sptr, rpl_str(RPL_ENDOFDCCLIST), me.name, sptr->name, s);
		} else
		if (!didhelp && !myncmp(s, "help", 4))
		{
			didanything = didhelp = 1;
			for(ptr = dcc_help; *ptr; ptr++)
				sendto_one(sptr, ":%s %d %s :%s", me.name, RPL_DCCINFO, sptr->name, *ptr);
			sendto_one(sptr, rpl_str(RPL_ENDOFDCCLIST), me.name, sptr->name, s);
		}
	}
	if (!didanything)
	{
		sendnotice(sptr, "Invalid syntax for DCCALLOW. Type '/DCCALLOW HELP' for more information.");
		return 0;
	}
	return 0;
}
