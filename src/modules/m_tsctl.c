/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_tsctl.c
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

DLLFUNC int m_tsctl(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_TSCTL       "TSCTL"
#define TOK_TSCTL       "AW"

ModuleHeader MOD_HEADER(m_tsctl)
  = {
	"tsctl",	/* Name of module */
	"$Id$", /* Version */
	"command /tsctl", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_tsctl)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_TSCTL, TOK_TSCTL, m_tsctl, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_tsctl)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_tsctl)(int module_unload)
{
	if (del_Command(MSG_TSCTL, TOK_TSCTL, m_tsctl) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_tsctl).name);
	}
	return MOD_SUCCESS;
}

/*
** m_tsctl - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = command
**      parv[2] = options
*/

DLLFUNC int m_tsctl(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	time_t timediff;


	if (!MyClient(sptr))
		goto doit;
	if (!IsAdmin(sptr) && !IsCoAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
      doit:
	if (parv[1])
	{
		if (*parv[1] == '\0')
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "TSCTL");
			return 0;
		}

		if (stricmp(parv[1], "offset") == 0)
		{
			if (!parv[3])
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (*parv[2] == '\0' || *parv[3] == '\0')
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (!(*parv[2] == '+' || *parv[2] == '-'))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;

			}

			switch (*parv[2])
			{
			  case '+':
				  timediff = atol(parv[3]);
				  ircd_log(LOG_ERROR, "TSCTL: Time offset changed by %s to +%li (was %li)",
				      sptr->name, timediff, TSoffset);
				  TSoffset = timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed +%li",
				      sptr->name, timediff);
				  sendto_serv_butone_token(&me, me.name, MSG_GLOBOPS, TOK_GLOBOPS,
				      ":TS Control - %s set TStime to be diffed +%li",
				      sptr->name, timediff);
				  break;
			  case '-':
				  timediff = atol(parv[3]);
				  ircd_log(LOG_ERROR, "TSCTL: Time offset changed by %s to -%li (was %li)",
				      sptr->name, timediff, TSoffset);
				  TSoffset = -timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed -%li",
				      sptr->name, timediff);
				  sendto_serv_butone_token(&me, me.name, MSG_GLOBOPS, TOK_GLOBOPS,
				      ":TS Control - %s set TStime to be diffed -%li",
				      sptr->name, timediff);
				  break;
			}
			return 0;
		}
		if (stricmp(parv[1], "time") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, TStime(), time(NULL),
			    TSoffset);
			return 0;
		}
		if (stricmp(parv[1], "alltime") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Server=%s TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, me.name, TStime(), time(NULL),
			    TSoffset);
			sendto_serv_butone_token(cptr, sptr->name, MSG_TSCTL, TOK_TSCTL, "alltime");
			return 0;

		}
		if (stricmp(parv[1], "svstime") == 0)
		{
			if (!parv[2] || *parv[2] == '\0')
			{
				return 0;
			}
			if (!IsULine(sptr))
			{
				return 0;
			}

			timediff = atol(parv[2]);
			timediff = timediff - time(NULL);
		    ircd_log(LOG_ERROR, "TSCTL: U:line %s set time to be %li (timediff: %li, was %li)",
				sptr->name, atol(parv[2]), timediff, TSoffset);
			TSoffset = timediff;
			sendto_ops
			    ("TS Control - U:line set time to be %li (timediff: %li)",
			    atol(parv[2]), timediff);
			sendto_serv_butone_token(cptr, sptr->name, MSG_TSCTL, TOK_TSCTL, "svstime %li",
			    sptr->name, atol(parv[2]));
			return 0;
		}
	}
	return 0;
}


