/*
 *   IRC - Internet Relay Chat, src/modules/m_squit.c
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

CMD_FUNC(m_squit);

#define MSG_SQUIT 	"SQUIT"	

ModuleHeader MOD_HEADER(m_squit)
  = {
	"m_squit",
	"4.0",
	"command /squit", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_squit)
{
	CommandAdd(modinfo->handle, MSG_SQUIT, m_squit, 2, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_squit)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_squit)
{
	return MOD_SUCCESS;
}

/*
** m_squit
**	parv[1] = server name
**	parv[parc-1] = comment
*/
CMD_FUNC(m_squit)
{
	char *server;
	aClient *acptr;
	char *comment = (parc > 2 && parv[parc - 1]) ? parv[parc - 1] : cptr->name;

	if (!ValidatePermissionsForPath("route:local",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (parc > 1)
	{
		server = parv[1];

		acptr = find_server_quickx(server, NULL);
		if (acptr && IsMe(acptr))
		{
			acptr = cptr;
			server = cptr->local->sockhost;
		}
	}
	else
	{
		/*
		   ** This is actually protocol error. But, well, closing
		   ** the link is very proper answer to that...
		 */
		server = cptr->local->sockhost;
		acptr = cptr;
	}

	/*
	   ** SQUIT semantics is tricky, be careful...
	   **
	   ** The old (irc2.2PL1 and earlier) code just cleans away the
	   ** server client from the links (because it is never true
	   ** "cptr == acptr".
	   **
	   ** This logic here works the same way until "SQUIT host" hits
	   ** the server having the target "host" as local link. Then it
	   ** will do a real cleanup spewing SQUIT's and QUIT's to all
	   ** directions, also to the link from which the orinal SQUIT
	   ** came, generating one unnecessary "SQUIT host" back to that
	   ** link.
	   **
	   ** One may think that this could be implemented like
	   ** "hunt_server" (e.g. just pass on "SQUIT" without doing
	   ** nothing until the server having the link as local is
	   ** reached). Unfortunately this wouldn't work in the real life,
	   ** because either target may be unreachable or may not comply
	   ** with the request. In either case it would leave target in
	   ** links--no command to clear it away. So, it's better just
	   ** clean out while going forward, just to be sure.
	   **
	   ** ...of course, even better cleanout would be to QUIT/SQUIT
	   ** dependant users/servers already on the way out, but
	   ** currently there is not enough information about remote
	   ** clients to do this...   --msa
	 */
	if (!acptr)
	{
		sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
		    me.name, sptr->name, server);
		return 0;
	}
	if (MyClient(sptr) && ((!ValidatePermissionsForPath("route:global",sptr,NULL,NULL,NULL) && !MyConnect(acptr)) ||
	    (!ValidatePermissionsForPath("route:local",sptr,NULL,NULL,NULL) && MyConnect(acptr))))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}
	/*
	   **  Notify all opers, if my local link is remotely squitted
	 */
	if (MyConnect(acptr) && !MyClient(cptr))
	{
		sendto_umode_global(UMODE_OPER, "Received SQUIT %s from %s (%s)",
		    acptr->name, get_client_name(sptr, FALSE), comment);
	}
	else if (MyConnect(acptr))
	{
		if (acptr->user)
		{
			sendnotice(sptr,
			    "*** Cannot do fake kill by SQUIT !!!");
			sendto_ops
			    ("%s tried to do a fake kill using SQUIT (%s (%s))",
			    sptr->name, acptr->name, comment);
			sendto_server(&me, 0, 0,
			    ":%s GLOBOPS :%s tried to fake kill using SQUIT (%s (%s))",
			    me.name, sptr->name, acptr->name, comment);
			return 0;
		}
		sendto_umode_global(UMODE_OPER, "Received SQUIT %s from %s (%s)",
		    acptr->name, get_client_name(sptr, FALSE), comment);
	}
	if (IsOper(sptr))
	{
		/*
		 * It was manually /squit'ed by a human being(we hope),
		 * there is a very good chance they don't want us to
		 * reconnect right away.  -Cabal95
		 */
		acptr->flags |= FLAGS_SQUIT;
	}

	return exit_client(cptr, acptr, sptr, comment);
}
