/*
 *   IRC - Internet Relay Chat, src/modules/trace.c
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

CMD_FUNC(cmd_trace);

#define MSG_TRACE 	"TRACE"	

ModuleHeader MOD_HEADER
  = {
	"trace",
	"5.0",
	"command /trace", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_TRACE, cmd_trace, MAXPARA, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
** cmd_trace
**	parv[1] = servername
*/
CMD_FUNC(cmd_trace)
{
	int  i;
	Client *acptr;
	ConfigItem_class *cltmp;
	const char *tname;
	int  doall, link_s[MAXCONNECTIONS], link_u[MAXCONNECTIONS];
	int  cnt = 0, wilds, dow;
	time_t now;

	/* This is one of the (few) commands that cannot be handled
	 * by labeled-response because responses from multiple servers
	 * are involved.
	 */
	labeled_response_inhibit = 1;

	if (parc > 2)
		if (hunt_server(client, NULL, "TRACE", 2, parc, parv))
			return;

	if (parc > 1)
		tname = parv[1];
	else
		tname = me.name;

	if (!ValidatePermissionsForPath("client:see:trace:global",client,NULL,NULL,NULL))
	{
		if (ValidatePermissionsForPath("client:see:trace:local",client,NULL,NULL,NULL))
		{
			/* local opers may not /TRACE remote servers! */
			if (strcasecmp(tname, me.name))
			{
				sendnotice(client, "You can only /TRACE local servers as a locop");
				sendnumeric(client, ERR_NOPRIVILEGES);
				return;
			}
		} else {
			sendnumeric(client, ERR_NOPRIVILEGES);
			return;
		}
	}

	switch (hunt_server(client, NULL, "TRACE", 1, parc, parv))
	{
	  case HUNTED_PASS:	/* note: gets here only if parv[1] exists */
	  {
		  Client *ac2ptr;

		  ac2ptr = find_client(tname, NULL);
		  sendnumeric(client, RPL_TRACELINK,
		      version, debugmode, tname, ac2ptr->direction->name);
		  return;
	  }
	  case HUNTED_ISME:
		  break;
	  default:
		  return;
	}

	doall = (parv[1] && (parc > 1)) ? match_simple(tname, me.name) : TRUE;
	wilds = !parv[1] || strchr(tname, '*') || strchr(tname, '?');
	dow = wilds || doall;

	for (i = 0; i < MAXCONNECTIONS; i++)
		link_s[i] = 0, link_u[i] = 0;


	if (doall) {
		list_for_each_entry(acptr, &client_list, client_node)
		{
			if (acptr->direction->local->fd < 0)
				continue;
			if (IsUser(acptr))
				link_u[acptr->direction->local->fd]++;
			else if (IsServer(acptr))
				link_s[acptr->direction->local->fd]++;
		}
	}

	/* report all direct connections */

	now = TStime();
	list_for_each_entry(acptr, &lclient_list, lclient_node)
	{
		const char *name;
		const char *class;

		if (!ValidatePermissionsForPath("client:see:trace:invisible-users",client,acptr,NULL,NULL) && (acptr != client))
			continue;
		if (!doall && wilds && !match_simple(tname, acptr->name))
			continue;
		if (!dow && mycmp(tname, acptr->name))
			continue;
		name = get_client_name(acptr, FALSE);
		class = acptr->local->class ? acptr->local->class->name : "default";
		switch (acptr->status)
		{
			case CLIENT_STATUS_CONNECTING:
				sendnumeric(client, RPL_TRACECONNECTING, class, name);
				cnt++;
				break;

			case CLIENT_STATUS_HANDSHAKE:
				sendnumeric(client, RPL_TRACEHANDSHAKE, class, name);
				cnt++;
				break;

			case CLIENT_STATUS_ME:
				break;

			case CLIENT_STATUS_UNKNOWN:
				sendnumeric(client, RPL_TRACEUNKNOWN, class, name);
				cnt++;
				break;

			case CLIENT_STATUS_USER:
				/* Only opers see users if there is a wildcard
				 * but anyone can see all the opers.
				 */
				if (ValidatePermissionsForPath("client:see:trace:invisible-users",client,acptr,NULL,NULL) ||
				    (!IsInvisible(acptr) && ValidatePermissionsForPath("client:see:trace",client,acptr,NULL,NULL)))
				{
					if (ValidatePermissionsForPath("client:see:trace",client,acptr,NULL,NULL) || ValidatePermissionsForPath("client:see:trace:invisible-users",client,acptr,NULL,NULL))
						sendnumeric(client, RPL_TRACEOPERATOR,
						    class, acptr->name,
						    GetHost(acptr),
						    (long long)(now - acptr->local->last_msg_received));
					else
						sendnumeric(client, RPL_TRACEUSER,
						    class, acptr->name,
						    acptr->user->realhost,
						    (long long)(now - acptr->local->last_msg_received));
					cnt++;
				}
				break;

			case CLIENT_STATUS_SERVER:
				sendnumeric(client, RPL_TRACESERVER, class, acptr->local->fd >= 0 ? link_s[acptr->local->fd] : -1,
				    acptr->local->fd >= 0 ? link_u[acptr->local->fd] : -1, name, *(acptr->server->by) ?
				    acptr->server->by : "*", "*", me.name,
				    (long long)(now - acptr->local->last_msg_received));
				cnt++;
				break;

			case CLIENT_STATUS_LOG:
				sendnumeric(client, RPL_TRACELOG, LOGFILE, acptr->local->port);
				cnt++;
				break;

			case CLIENT_STATUS_TLS_CONNECT_HANDSHAKE:
				sendnumeric(client, RPL_TRACENEWTYPE, "TLS-Connect-Handshake", name);
				cnt++;
				break;

			case CLIENT_STATUS_TLS_ACCEPT_HANDSHAKE:
				sendnumeric(client, RPL_TRACENEWTYPE, "TLS-Accept-Handshake", name);
				cnt++;
				break;

			default:	/* ...we actually shouldn't come here... --msa */
				sendnumeric(client, RPL_TRACENEWTYPE, "<newtype>", name);
				cnt++;
				break;
		}
	}
	/*
	 * Add these lines to summarize the above which can get rather long
	 * and messy when done remotely - Avalon
	 */
	if (!ValidatePermissionsForPath("client:see:trace",client,acptr,NULL,NULL) || !cnt)
		return;

	for (cltmp = conf_class; doall && cltmp; cltmp = cltmp->next)
	/*	if (cltmp->clients > 0) */
			sendnumeric(client, RPL_TRACECLASS, cltmp->name ? cltmp->name : "[noname]", cltmp->clients);
}
