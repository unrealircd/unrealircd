/*
 *   IRC - Internet Relay Chat, src/modules/m_lusers.c
 *   (C) 2005 The UnrealIRCd Team
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

CMD_FUNC(m_lusers);

#define MSG_LUSERS 	"LUSERS"	

ModuleHeader MOD_HEADER(lusers)
  = {
	"lusers",
	"5.0",
	"command /lusers", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(lusers)
{
	CommandAdd(modinfo->handle, MSG_LUSERS, m_lusers, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(lusers)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(lusers)
{
	return MOD_SUCCESS;
}

/*
 * parv[1] = server to query
 */
CMD_FUNC(m_lusers)
{
char flatmap;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s LUSERS :%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	flatmap = (FLAT_MAP && !ValidatePermissionsForPath("server:info:lusers",sptr,NULL,NULL,NULL)) ? 1 : 0;

	/* Just to correct results ---Stskeeps */
	if (IRCstats.clients > IRCstats.global_max)
		IRCstats.global_max = IRCstats.clients;
	if (IRCstats.me_clients > IRCstats.me_max)
		IRCstats.me_max = IRCstats.me_clients;

	sendnumeric(sptr, RPL_LUSERCLIENT,
	    IRCstats.clients - IRCstats.invisible, IRCstats.invisible,
	    IRCstats.servers);

	if (IRCstats.operators)
		sendnumeric(sptr, RPL_LUSEROP, IRCstats.operators);
	if (IRCstats.unknown)
		sendnumeric(sptr, RPL_LUSERUNKNOWN, IRCstats.unknown);
	if (IRCstats.channels)
		sendnumeric(sptr, RPL_LUSERCHANNELS, IRCstats.channels);
	sendnumeric(sptr, RPL_LUSERME, IRCstats.me_clients, flatmap ? 0 : IRCstats.me_servers);
	sendnumeric(sptr, RPL_LOCALUSERS, IRCstats.me_clients, IRCstats.me_max, IRCstats.me_clients, IRCstats.me_max);
	sendnumeric(sptr, RPL_GLOBALUSERS, IRCstats.clients, IRCstats.global_max, IRCstats.clients, IRCstats.global_max);
	if ((IRCstats.me_clients + IRCstats.me_servers) > max_connection_count)
	{
		max_connection_count =
		    IRCstats.me_clients + IRCstats.me_servers;
		if (max_connection_count % 10 == 0)	/* only send on even tens */
			sendto_ops("New record on this server: %d connections (%d clients)",
			    max_connection_count, IRCstats.me_clients);
	}
	return 0;
}
