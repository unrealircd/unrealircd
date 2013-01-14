/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

DLLFUNC int m_connect(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_CONNECT 	"CONNECT"	
#define TOK_CONNECT 	"7"	

ModuleHeader MOD_HEADER(m_connect)
  = {
	"m_connect",
	"$Id$",
	"command /connect", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_connect)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_CONNECT, TOK_CONNECT, m_connect, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_connect)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_connect)(int module_unload)
{
	return MOD_SUCCESS;
}

/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************//*
   ** m_connect
   **  parv[0] = sender prefix
   **  parv[1] = servername
   **  parv[2] = port number
   **  parv[3] = remote server
 */
DLLFUNC CMD_FUNC(m_connect)
{
	int  port, tmpport, retval;
	ConfigItem_link	*aconf;
	ConfigItem_deny_link *deny;
	aClient *acptr;


	if (!IsPrivileged(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	if (MyClient(sptr) && !OPCanGRoute(sptr) && parc > 3)
	{			/* Only allow LocOps to make */
		/* local CONNECTS --SRB      */
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (MyClient(sptr) && !OPCanLRoute(sptr) && parc <= 3)
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (hunt_server_token(cptr, sptr, MSG_CONNECT, TOK_CONNECT, "%s %s :%s",
	    3, parc, parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "CONNECT");
		return -1;
	}

	if ((acptr = find_server_quick(parv[1])))
	{
		sendnotice(sptr, "*** Connect: Server %s %s %s.",
		    parv[1], "already exists from",
		    acptr->from->name);
		return 0;
	}

	for (aconf = conf_link; aconf; aconf = (ConfigItem_link *) aconf->next)
		if (!match(parv[1], aconf->servername))
			break;

	/* Checked first servernames, then try hostnames. */

	if (!aconf)
	{
		sendnotice(sptr,
		    "*** Connect: Server %s is not configured for linking",
		    parv[1]);
		return 0;
	}
	/*
	   ** Get port number from user, if given. If not specified,
	   ** use the default form configuration structure. If missing
	   ** from there, then use the precompiled default.
	 */
	tmpport = port = aconf->port;
	if (parc > 2 && !BadPtr(parv[2]))
	{
		if ((port = atoi(parv[2])) <= 0)
		{
			sendnotice(sptr,
			    "*** Connect: Illegal port number");
			return 0;
		}
	}
	else if (port <= 0 && (port = PORTNUM) <= 0)
	{
		sendnotice(sptr, "*** Connect: missing port number");
		return 0;
	}



/* Evaluate deny link */
	for (deny = conf_deny_link; deny; deny = (ConfigItem_deny_link *) deny->next) {
		if (deny->flag.type == CRULE_ALL && !match(deny->mask, aconf->servername)
			&& crule_eval(deny->rule)) {
			sendnotice(sptr,
				"*** Connect: Disallowed by connection rule");
			return 0;
		}
	}
	if (strchr(aconf->hostname, '*') != NULL || strchr(aconf->hostname, '?') != NULL)
	{
		sendnotice(sptr,
			"*** Connect: You cannot connect to a server with wildcards (* and ?) in the hostname");
		return 0;
	}	
	/*
	   ** Notify all operators about remote connect requests
	 */
	if (!IsAnOper(cptr))
	{
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :Remote CONNECT %s %s from %s",
		    me.name, parv[1], parv[2] ? parv[2] : "",
		    get_client_name(sptr, FALSE));
	}
	/* Interesting */
	aconf->port = port;
	switch (retval = connect_server(aconf, sptr, NULL))
	{
	  case 0:
		  sendnotice(sptr,
		      "*** Connecting to %s[%s].",
		      aconf->servername, aconf->hostname);
		  break;
	  case -1:
		  sendnotice(sptr, "*** Couldn't connect to %s.",
		      aconf->servername);
		  break;
	  case -2:
		  sendnotice(sptr, "*** Resolving hostname '%s'...",
		      aconf->hostname);
		  break;
	  default:
		  sendnotice(sptr, "*** Connection to %s failed: %s",
		      aconf->servername, STRERROR(retval));
	}
	aconf->port = tmpport;
	return 0;
}
