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

DLLFUNC int m_trace(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_TRACE 	"TRACE"	
#define TOK_TRACE 	"b"	

ModuleHeader MOD_HEADER(m_trace)
  = {
	"m_trace",
	"$Id$",
	"command /trace", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_trace)(ModuleInfo *modinfo)
{
	add_Command(MSG_TRACE, TOK_TRACE, m_trace, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_trace)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_trace)(int module_unload)
{
	if (del_Command(MSG_TRACE, TOK_TRACE, m_trace) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_trace).name);
	}
	return MOD_SUCCESS;
}

/*
** m_trace
**	parv[0] = sender prefix
**	parv[1] = servername
*/
DLLFUNC CMD_FUNC(m_trace)
{
	int  i;
	aClient *acptr;
	ConfigItem_class *cltmp;
	char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;


	if (parc > 2)
		if (hunt_server_token(cptr, sptr, MSG_TRACE, TOK_TRACE, "%s :%s", 2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	if (!IsOper(sptr))
	{
		if (IsAnOper(sptr))
		{
			/* local opers may not /TRACE remote servers! */
			if (strcasecmp(tname, me.name))
			{
				sendnotice(sptr, "You can only /TRACE local servers as a locop");
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
				return 0;
			}
		} else {
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
			return 0;
		}
	}

	switch (hunt_server_token(cptr, sptr, MSG_TRACE, TOK_TRACE, ":%s", 1, parc, parv))
	{
	  case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
	  {
		  aClient *ac2ptr;

		  ac2ptr = find_client(tname, NULL);
		  sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, parv[0],
		      version, debugmode, tname, ac2ptr->from->name);
		  return 0;
	  }
	  case HUNTED_ISME:
		  break;
	  default:
		  return 0;
	}

	doall = (parv[1] && (parc > 1)) ? !match(tname, me.name) : TRUE;
	wilds = !parv[1] || index(tname, '*') || index(tname, '?');
	dow = wilds || doall;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;


	if (doall) {
		list_for_each_entry(acptr, &client_list, client_list)
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->slot]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || IsOper(sptr)))
				link_u[acptr->from->slot]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->slot]++;
	}

	/* report all direct connections */

	now = TStime();
	for (i = 0; i <= LastSlot; i++)
	{
		char *name;
		char *class;

		if (!(acptr = local[i]))	/* Local Connection? */
			continue;
/* More bits of code to allow oers to see all users on remote traces
 *		if (IsInvisible(acptr) && dow &&
 *		if (dow &&
 *		    !(MyConnect(sptr) && IsOper(sptr)) && */
		if (!IsOper(sptr) && !IsAnOper(acptr) && (acptr != sptr))
			continue;
		if (!doall && wilds && match(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr, FALSE);
		class = acptr->class ? acptr->class->name : "default";
		switch (acptr->status)
		{
		  case STAT_CONNECTING:
			  sendto_one(sptr, rpl_str(RPL_TRACECONNECTING),
			      me.name, parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_HANDSHAKE:
			  sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE), me.name,
			      parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_ME:
			  break;
		  case STAT_UNKNOWN:
			  sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
			      me.name, parv[0], class, name);
			  cnt++;
			  break;
		  case STAT_CLIENT:
			  /* Only opers see users if there is a wildcard
			   * but anyone can see all the opers.
			   */
/*			if (IsOper(sptr) &&
 * Allow opers to see invisible users on a remote trace or wildcard
 * search  ... sure as hell  helps to find clonebots.  --Russell
 *			    (MyClient(sptr) || !(dow && IsInvisible(acptr)))
 *                           || !dow || IsAnOper(acptr)) */
			  if (IsOper(sptr) ||
			      (IsAnOper(acptr) && !IsInvisible(acptr)))
			  {
				  if (IsAnOper(acptr))
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEOPERATOR),
					      me.name,
					      parv[0], class, acptr->name,
					      GetHost(acptr),
					      now - acptr->lasttime);
				  else
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEUSER), me.name,
					      parv[0], class, acptr->name,
					      acptr->user->realhost,
					      now - acptr->lasttime);
				  cnt++;
			  }
			  break;
		  case STAT_SERVER:
			  if (acptr->serv->user)
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, parv[0], class, link_s[i],
				      link_u[i], name, acptr->serv->by,
				      acptr->serv->user->username,
				      acptr->serv->user->realhost,
				      now - acptr->lasttime);
			  else
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, parv[0], class, link_s[i],
				      link_u[i], name, *(acptr->serv->by) ?
				      acptr->serv->by : "*", "*", me.name,
				      now - acptr->lasttime);
			  cnt++;
			  break;
		  case STAT_LOG:
			  sendto_one(sptr, rpl_str(RPL_TRACELOG), me.name,
			      parv[0], LOGFILE, acptr->port);
			  cnt++;
			  break;
#ifdef USE_SSL
		  case STAT_SSL_CONNECT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 parv[0], "SSL-Connect-Handshake", name); 
			cnt++;
			break;
		  case STAT_SSL_ACCEPT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 parv[0], "SSL-Accept-Handshake", name); 
			cnt++;
			break;
#endif
		  default:	/* ...we actually shouldn't come here... --msa */
			  sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
			      parv[0], "<newtype>", name);
			  cnt++;
			  break;
		}
	}
	/*
	 * Add these lines to summarize the above which can get rather long
	 * and messy when done remotely - Avalon
	 */
	if (!IsAnOper(sptr) || !cnt)
	{
		if (cnt)
			return 0;
		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one(sptr, rpl_str(RPL_TRACESERVER),
		    me.name, parv[0], "0", link_s[me.slot],
		    link_u[me.slot], me.name, "*", "*", me.name, 0L);
		return 0;
	}
	for (cltmp = conf_class; doall && cltmp; cltmp = (ConfigItem_class *) cltmp->next)
	/*	if (cltmp->clients > 0) */
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
			    parv[0], cltmp->name ? cltmp->name : "[noname]", cltmp->clients);
	return 0;
}
