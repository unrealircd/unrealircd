/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_zline.c
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

DLLFUNC int m_zline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_ZLINE       "ZLINE" /* ZLINE */
#define TOK_ZLINE       "q"     /* 112 */


#ifndef DYNAMIC_LINKING
ModuleHeader m_zline_Header
#else
#define m_zline_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"zline",	/* Name of module */
	"$Id$", /* Version */
	"command /zline", /* Short description of module */
	"3.2-b5",
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    m_zline_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_ZLINE, TOK_ZLINE, m_zline, 2);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_zline_Load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_zline_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_ZLINE, TOK_ZLINE, m_zline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_zline_Header.name);
	}
}


/*
 *  m_zline                       add a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 *    parv[2] = reason
 */

DLLFUNC int m_zline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char userhost[512 + 2] = "", *in;
	int  uline = 0, i = 0, propo = 0;
	char *reason, *mask, *server, *person;
	aClient *acptr;
	ConfigItem_ban *bconf;
	
	reason = mask = server = person = NULL;

	reason = ((parc >= 3) ? parv[parc - 1] : "Reason unspecified");
	mask = ((parc >= 2) ? parv[parc - 2] : NULL);
	server = ((parc >= 4) ? parv[parc - 1] : NULL);

	if (parc == 4)
	{
		mask = parv[parc - 3];
		server = parv[parc - 2];
		reason = parv[parc - 1];
	}

	uline = IsULine(sptr) ? 1 : 0;

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	if (uline)
	{
		if (parc >= 4 && server)
		{
			if (hunt_server_token(cptr, sptr, MSG_ZLINE, TOK_ZLINE, "%s %s :%s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			propo = 1;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ZLINE");
		return -1;
	}

	if (acptr = find_client(parv[1], NULL))
	{
		strcpy(userhost, inetntoa((char *)&acptr->ip));
		person = &acptr->name[0];
		acptr = NULL;
	}
	/* z-lines don't support user@host format, they only 
	   work with ip addresses and nicks */
	else if ((in = index(parv[1], '@')) && (*(in + 1) != '\0'))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return 0;
			}
			in++;
		}
	}
	else if (in && !(*(in + 1)))	/* sheesh not only specifying a ident@, but
					   omitting the ip...? */
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Hey! z:lines need an ip address...",
		    me.name, sptr->name);
		return -1;
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
				    ":%s NOTICE %s :*** z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return 0;
			}
			in++;
		}
	}

	/* this'll protect against z-lining *.* or something */
	if (advanced_check(userhost, TRUE) == FALSE)
	{
		sendto_ops("Bad z:line mask from %s *@%s [%s]", parv[0],
		    userhost, reason ? reason : "");
		if (MyClient(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :** *@%s is a bad z:line mask...",
			    me.name, sptr->name, userhost);
		return 0;
	}

	if (!(bconf = Find_ban(userhost, CONF_BAN_IP)))
	{
		if (uline == 0)
		{
			if (person)	
				sendto_realops("%s added a temp z:line for %s (*@%s) [%s]",
				    parv[0], person, userhost, reason ? reason : "");
			else
				sendto_realops("%s added a temp z:line *@%s [%s]", parv[0],
				    userhost, reason ? reason : "");
		}
		else
		{
			if (person)
				sendto_ops("%s z:lined %s (*@%s) on %s [%s]", parv[0],
				    person, userhost, server ? server : ircnetwork,
				    reason ? reason : "");
			else
				sendto_ops("%s z:lined *@%s on %s [%s]", parv[0],
				    userhost, server ? server : ircnetwork,
				    reason ? reason : "");
		
		}
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		bconf->flag.type = CONF_BAN_IP;
		bconf->mask = strdup(userhost);
		bconf->reason = strdup(reason);
		bconf->flag.type2 = uline ? CONF_BAN_TYPE_AKILL : CONF_BAN_TYPE_TEMPORARY;
	}
	else
	{
		goto propo_label;
	}
	if (!match(mask, inetntoa((char *)&cptr->ip)))
	{
		sendto_realops("Tried to zap cptr, from %s",
			parv[0]);
		MyFree(bconf);	
	}
	else
	{
		AddListItem(bconf, conf_ban);
	}
propo_label:
	if (propo == 1)		/* propo is if a ulined server is propagating a z-line
				   this should go after the above check */
		sendto_serv_butone(cptr, ":%s ZLINE %s :%s", parv[0], parv[1],
		    reason ? reason : "");

	check_pings(TStime(), 1);
}
