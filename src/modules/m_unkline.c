/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_unkline.c
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

DLLFUNC int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_UNKLINE     "UNKLINE"       /* UNKLINE */
#define TOK_UNKLINE     "X"     /* 88 */  


#ifndef DYNAMIC_LINKING
ModuleInfo m_unkline_info
#else
#define m_unkline_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"unkline",	/* Name of module */
	"$Id$", /* Version */
	"command /unkline", /* Short description of module */
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
int    m_unkline_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_UNKLINE, TOK_UNKLINE, m_unkline, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_unkline_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_unkline_unload(void)
#endif
{
	if (del_Command(MSG_UNKLINE, TOK_UNKLINE, m_unkline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_unkline_info.name);
	}
}

/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

DLLFUNC int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{

	int  result, temp;
	char *hosttemp = parv[1], host[80], name[80];
	ConfigItem_ban *bconf;
	
	
	if (!MyClient(sptr) || !OPCanUnKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Not enough parameters", me.name, parv[0]);
		return 0;
	}
	if (hosttemp = (char *)strchr((char *)parv[1], '@'))
	{
		*hosttemp = 0;
		hosttemp++;
		bzero(name, sizeof(name));
		bzero(host, sizeof(host));
		
		strncpy(name, parv[1], sizeof(name) - 1);
		strncpy(host, hosttemp, sizeof(host) - 1);
		if (name[0] == '\0' || host[0] == '\0')
		{
			Debug((DEBUG_INFO, "UNKLINE: Bad field"));
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Both user and host fields must be non-null",
			    me.name, parv[0]);
			return 0;
		}
		if (!(bconf = Find_banEx(make_user_host(name, host), CONF_BAN_USER, CONF_BAN_TYPE_TEMPORARY)))
		{
			sendto_one(sptr, ":%s NOTICE %s :*** Cannot find user ban %s@%s",
				me.name, parv[0], name, host);
			return 0;
		}
		if (bconf->flag.type2 != CONF_BAN_TYPE_TEMPORARY)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** You cannot remove permament user bans",
				me.name, sptr->name);
			return 0;
		}
		
		del_ConfigItem((ConfigItem *)bconf, (ConfigItem **)&conf_ban);
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		MyFree(bconf);
		
		sendto_one(sptr,
			":%s NOTICE %s :*** Temporary user ban %s@%s is now removed.",
			    me.name, parv[0], name, host);
		sendto_realops("%s removed temporary user ban %s@%s", parv[0],
		    name, host);
		ircd_log(LOG_KLINE,
		    "%s removed temporary user ban %s@%s",
		    parv[0], name, host);
		return 0;
	}
	/* This wasn't here before -- Barubary */
	check_pings(TStime(), 1);
}
