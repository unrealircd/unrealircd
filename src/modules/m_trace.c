/*
 *   IRC - Internet Relay Chat, src/modules/m_trace.c
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

#include "unrealircd.h"

CMD_FUNC(m_trace);

#define MSG_TRACE 	"TRACE"	

ModuleHeader MOD_HEADER(m_trace)
  = {
	"m_trace",
	"4.0",
	"command /trace", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_trace)
{
	CommandAdd(modinfo->handle, MSG_TRACE, m_trace, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_trace)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_trace)
{
	return MOD_SUCCESS;
}

/*
** m_trace
**	parv[1] = servername
*/
CMD_FUNC(m_trace)
{
	int  i;
	aClient *acptr;
	ConfigItem_class *cltmp;
	char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;


	if (parc > 2)
		if (hunt_server(cptr, sptr, ":%s TRACE %s :%s", 2, parc, parv))
			return 0;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	if (!ValidatePermissionsForPath("trace:global",sptr,NULL,NULL,NULL))
	{
		if (ValidatePermissionsForPath("trace:local",sptr,NULL,NULL,NULL))
		{
			/* local opers may not /TRACE remote servers! */
			if (strcasecmp(tname, me.name))
			{
				sendnotice(sptr, "You can only /TRACE local servers as a locop");
				sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
				return 0;
			}
		} else {
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
			return 0;
		}
	}

	switch (hunt_server(cptr, sptr, ":%s TRACE :%s", 1, parc, parv))
	{
	  case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
	  {
		  aClient *ac2ptr;

		  ac2ptr = find_client(tname, NULL);
		  sendto_one(sptr, rpl_str(RPL_TRACELINK), me.name, sptr->name,
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
		list_for_each_entry(acptr, &client_list, client_node)
		{
			if (acptr->from->fd < 0)
				continue;
#ifdef	SHOW_INVISIBLE_LUSERS
			if (IsPerson(acptr))
				link_u[acptr->from->fd]++;
#else
			if (IsPerson(acptr) &&
			    (!IsInvisible(acptr) || ValidatePermissionsForPath("trace:invisible-users",sptr,acptr,NULL,NULL)))
				link_u[acptr->from->fd]++;
#endif
			else if (IsServer(acptr))
				link_s[acptr->from->fd]++;
		}
	}

	/* report all direct connections */

	now = TStime();
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		char *name;
		char *class;

		if (!ValidatePermissionsForPath("trace:invisible-users",sptr,acptr,NULL,NULL) && (acptr != sptr))
			continue;
		if (!doall && wilds && match(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr, FALSE);
		class = acptr->local->class ? acptr->local->class->name : "default";
		switch (acptr->status)
		{
		  case STAT_CONNECTING:
			  sendto_one(sptr, rpl_str(RPL_TRACECONNECTING),
			      me.name, sptr->name, class, name);
			  cnt++;
			  break;
		  case STAT_HANDSHAKE:
			  sendto_one(sptr, rpl_str(RPL_TRACEHANDSHAKE), me.name,
			      sptr->name, class, name);
			  cnt++;
			  break;
		  case STAT_ME:
			  break;
		  case STAT_UNKNOWN:
			  sendto_one(sptr, rpl_str(RPL_TRACEUNKNOWN),
			      me.name, sptr->name, class, name);
			  cnt++;
			  break;
		  case STAT_CLIENT:
			  /* Only opers see users if there is a wildcard
			   * but anyone can see all the opers.
			   */
			  if (ValidatePermissionsForPath("trace:invisible-users",sptr,acptr,NULL,NULL) ||
			      (!IsInvisible(acptr) && ValidatePermissionsForPath("trace",sptr,acptr,NULL,NULL)))
			  {
				  if (ValidatePermissionsForPath("trace",sptr,acptr,NULL,NULL) || ValidatePermissionsForPath("trace:invisible-users",sptr,acptr,NULL,NULL))
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEOPERATOR),
					      me.name,
					      sptr->name, class, acptr->name,
					      GetHost(acptr),
					      now - acptr->local->lasttime);
				  else
					  sendto_one(sptr,
					      rpl_str(RPL_TRACEUSER), me.name,
					      sptr->name, class, acptr->name,
					      acptr->user->realhost,
					      now - acptr->local->lasttime);
				  cnt++;
			  }
			  break;
		  case STAT_SERVER:
			  if (acptr->serv->user)
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, sptr->name, class, acptr->fd >= 0 ? link_s[acptr->fd] : -1,
				      acptr->fd >= 0 ? link_u[acptr->fd] : -1, name, acptr->serv->by,
				      acptr->serv->user->username,
				      acptr->serv->user->realhost,
				      now - acptr->local->lasttime);
			  else
				  sendto_one(sptr, rpl_str(RPL_TRACESERVER),
				      me.name, sptr->name, class, acptr->fd >= 0 ? link_s[acptr->fd] : -1,
				      acptr->fd >= 0 ? link_u[acptr->fd] : -1, name, *(acptr->serv->by) ?
				      acptr->serv->by : "*", "*", me.name,
				      now - acptr->local->lasttime);
			  cnt++;
			  break;
		  case STAT_LOG:
			  sendto_one(sptr, rpl_str(RPL_TRACELOG), me.name,
			      sptr->name, LOGFILE, acptr->local->port);
			  cnt++;
			  break;
#ifdef USE_SSL
		  case STAT_SSL_CONNECT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 sptr->name, "SSL-Connect-Handshake", name); 
			cnt++;
			break;
		  case STAT_SSL_ACCEPT_HANDSHAKE:
		  	sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
		  	 sptr->name, "SSL-Accept-Handshake", name); 
			cnt++;
			break;
#endif
		  default:	/* ...we actually shouldn't come here... --msa */
			  sendto_one(sptr, rpl_str(RPL_TRACENEWTYPE), me.name,
			      sptr->name, "<newtype>", name);
			  cnt++;
			  break;
		}
	}
	/*
	 * Add these lines to summarize the above which can get rather long
	 * and messy when done remotely - Avalon
	 */
	if (!ValidatePermissionsForPath("trace",sptr,acptr,NULL,NULL) || !cnt)
	{
		if (cnt)
			return 0;
		/* let the user have some idea that its at the end of the
		 * trace
		 */
		sendto_one(sptr, rpl_str(RPL_TRACESERVER),
		    me.name, sptr->name, "0", link_s[me.fd],
		    link_u[me.fd], me.name, "*", "*", me.name, 0L);
		return 0;
	}
	for (cltmp = conf_class; doall && cltmp; cltmp = (ConfigItem_class *) cltmp->next)
	/*	if (cltmp->clients > 0) */
			sendto_one(sptr, rpl_str(RPL_TRACECLASS), me.name,
			    sptr->name, cltmp->name ? cltmp->name : "[noname]", cltmp->clients);
	return 0;
}
