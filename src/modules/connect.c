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

#include "unrealircd.h"

CMD_FUNC(m_connect);

#define MSG_CONNECT 	"CONNECT"	

ModuleHeader MOD_HEADER(connect)
  = {
	"connect",
	"5.0",
	"command /connect", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(connect)
{
	CommandAdd(modinfo->handle, MSG_CONNECT, m_connect, MAXPARA, M_USER|M_SERVER); /* hmm.. server.. really? */
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(connect)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(connect)
{
	return MOD_SUCCESS;
}

/***********************************************************************
 * m_connect() - Added by Jto 11 Feb 1989
 ***********************************************************************//*
   ** m_connect
   **  parv[1] = servername
 */
CMD_FUNC(m_connect)
{
	int  retval;
	ConfigItem_link	*aconf;
	ConfigItem_deny_link *deny;
	aClient *acptr;


	if (!IsServer(sptr) && MyConnect(sptr) && !ValidatePermissionsForPath("route:global",sptr,NULL,NULL,NULL) && parc > 3)
	{			/* Only allow LocOps to make */
		/* local CONNECTS --SRB      */
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	if (!IsServer(sptr) && MyClient(sptr) && !ValidatePermissionsForPath("route:local",sptr,NULL,NULL,NULL) && parc <= 3)
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}
	if (hunt_server(cptr, sptr, recv_mtags, ":%s CONNECT %s %s :%s", 3, parc, parv) != HUNTED_ISME)
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "CONNECT");
		return -1;
	}

	if ((acptr = find_server_quick(parv[1])))
	{
		sendnotice(sptr, "*** Connect: Server %s %s %s.",
		    parv[1], "already exists from",
		    acptr->from->name);
		return 0;
	}

	for (aconf = conf_link; aconf; aconf = aconf->next)
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

	if (!aconf->outgoing.hostname)
	{
		sendnotice(sptr,
		    "*** Connect: Server %s is not configured to be an outgoing link (has a link block, but no link::outgoing::hostname)",
		    parv[1]);
		return 0;
	}

	/* Evaluate deny link */
	for (deny = conf_deny_link; deny; deny = deny->next)
	{
		if (deny->flag.type == CRULE_ALL && !match(deny->mask, aconf->servername)
			&& crule_eval(deny->rule))
		{
			sendnotice(sptr, "*** Connect: Disallowed by connection rule");
			return 0;
		}
	}

	/* Notify all operators about remote connect requests */
	if (!MyClient(cptr))
	{
		sendto_server(&me, 0, 0, NULL,
		    ":%s GLOBOPS :Remote CONNECT %s %s from %s",
		    me.name, parv[1], parv[2] ? parv[2] : "",
		    get_client_name(sptr, FALSE));
	}

	switch (retval = connect_server(aconf, sptr, NULL))
	{
	  case 0:
		  sendnotice(sptr, "*** Connecting to %s[%s].",
		      aconf->servername, aconf->outgoing.hostname);
		  break;
	  case -1:
		  sendnotice(sptr, "*** Couldn't connect to %s[%s]",
		  	aconf->servername, aconf->outgoing.hostname);
		  break;
	  case -2:
		  sendnotice(sptr, "*** Resolving hostname '%s'...",
		  	aconf->outgoing.hostname);
		  break;
	  default:
		  sendnotice(sptr, "*** Connection to %s[%s] failed: %s",
		  	aconf->servername, aconf->outgoing.hostname, STRERROR(retval));
	}

	return 0;
}
