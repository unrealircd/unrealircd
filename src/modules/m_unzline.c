/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_unzline.c
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_unzline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_UNZLINE     "UNZLINE"       /* UNZLINE */
#define TOK_UNZLINE     "r"     /* 113 */ 


#ifndef DYNAMIC_LINKING
ModuleInfo m_unzline_info
#else
#define m_unzline_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"unzline",	/* Name of module */
	"$Id$", /* Version */
	"command /unzline", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_unzline_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_UNZLINE, TOK_UNZLINE, m_unzline, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_unzline_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_unzline_unload(void)
#endif
{
	if (del_Command(MSG_UNZLINE, TOK_UNZLINE, m_unzline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_unzline_info.name);
	}
}


/*
 *  m_unzline                        remove a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 */

DLLFUNC int m_unzline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char userhost[512 + 2] = "", *in;
	int  result = 0, uline = 0, akill = 0;
	char *mask, *server;
	ConfigItem_ban *bconf;
	uline = IsULine(sptr) ? 1 : 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "UNZLINE");
		return -1;
	}


	if (parc < 3 || !uline)
	{
		mask = parv[parc - 1];
		server = NULL;
	}
	else if (parc == 3)
	{
		mask = parv[parc - 2];
		server = parv[parc - 1];
	}

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	/* before we even check ourselves we need to do the uline checks
	   because we aren't supposed to add a z:line if the message is
	   destined to be passed on... */

	if (uline)
	{
		if (parc == 3 && server)
		{
			if (hunt_server(cptr, sptr, ":%s UNZLINE %s %s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			sendto_serv_butone(cptr, ":%s UNZLINE %s", parv[0],
			    parv[1]);

	}


	/* parse the removal mask the same way so an oper can just use
	   the same thing to remove it if they specified *@ or something... */
	if ((in = index(parv[1], '@')))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}
	else
	{
		strcpy(userhost, parv[1]);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}

	akill = 0;
      retry_unzline:

	bconf = Find_ban(userhost, CONF_BAN_IP);
	if (!bconf)
	{
		if (MyClient(sptr))
			sendto_one(sptr, ":%s NOTICE %s :*** Cannot find z:line %s",
				me.name, sptr->name, userhost);
		return 0;
	}
	
	if (uline == 0)
	{
		if (bconf->flag.type2 != CONF_BAN_TYPE_TEMPORARY)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** You may not remove permanent z:lines.",
				me.name, sptr->name);
			return 0;
		}			
	}
	DelListItem(bconf, conf_ban);
	sendto_realops("%s removed z:line %s", parv[0], userhost);
	if (bconf->mask)
		MyFree(bconf->mask);
	if (bconf->reason)
		MyFree(bconf->reason);
	MyFree(bconf);

	return 0;
}
