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

DLLFUNC CMD_FUNC(m_lusers);

#define MSG_LUSERS 	"LUSERS"	
#define TOK_LUSERS 	"E"	

ModuleHeader MOD_HEADER(m_lusers)
  = {
	"m_lusers",
	"$Id$",
	"command /lusers", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_lusers)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_LUSERS, TOK_LUSERS, m_lusers, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_lusers)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_lusers)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
 * parv[0] = sender
 * parv[1] = server to query
 */
DLLFUNC CMD_FUNC(m_lusers)
{
char flatmap;

	if (hunt_server_token(cptr, sptr, MSG_LUSERS, TOK_LUSERS,
                          ":%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	flatmap = (FLAT_MAP && !IsAnOper(sptr)) ? 1 : 0;

	/* Just to correct results ---Stskeeps */
	if (IRCstats.clients > IRCstats.global_max)
		IRCstats.global_max = IRCstats.clients;
	if (IRCstats.me_clients > IRCstats.me_max)
		IRCstats.me_max = IRCstats.me_clients;

	sendto_one(sptr, rpl_str(RPL_LUSERCLIENT), me.name, parv[0],
	    IRCstats.clients - IRCstats.invisible, IRCstats.invisible,
	    IRCstats.servers);

	if (IRCstats.operators)
		sendto_one(sptr, rpl_str(RPL_LUSEROP),
		    me.name, parv[0], IRCstats.operators);
	if (IRCstats.unknown)
		sendto_one(sptr, rpl_str(RPL_LUSERUNKNOWN),
		    me.name, parv[0], IRCstats.unknown);
	if (IRCstats.channels)
		sendto_one(sptr, rpl_str(RPL_LUSERCHANNELS),
		    me.name, parv[0], IRCstats.channels);
	sendto_one(sptr, rpl_str(RPL_LUSERME),
	    me.name, parv[0], IRCstats.me_clients, flatmap ? 0 : IRCstats.me_servers);
	sendto_one(sptr, rpl_str(RPL_LOCALUSERS),
	    me.name, parv[0], IRCstats.me_clients, IRCstats.me_max);
	sendto_one(sptr, rpl_str(RPL_GLOBALUSERS),
	    me.name, parv[0], IRCstats.clients, IRCstats.global_max);
	if ((IRCstats.me_clients + IRCstats.me_servers) > max_connection_count)
	{
		max_connection_count =
		    IRCstats.me_clients + IRCstats.me_servers;
		if (max_connection_count % 10 == 0)	/* only send on even tens */
			sendto_ops("Maximum connections: %d (%d clients)",
			    max_connection_count, IRCstats.me_clients);
	}
	return 0;
}
