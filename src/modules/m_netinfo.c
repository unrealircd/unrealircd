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

CMD_FUNC(m_netinfo);

#define MSG_NETINFO 	"NETINFO"	

ModuleHeader MOD_HEADER(m_netinfo)
  = {
	"m_netinfo",
	"4.0",
	"command /netinfo", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_netinfo)
{
	CommandAdd(modinfo->handle, MSG_NETINFO, m_netinfo, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_netinfo)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_netinfo)
{
	return MOD_SUCCESS;
}

/*
** m_netinfo
** by Stskeeps
**  parv[1] = max global count
**  parv[2] = time of end sync
**  parv[3] = unreal protocol using (numeric)
**  parv[4] = cloak-crc (> u2302)
**  parv[5] = free(**)
**  parv[6] = free(**)
**  parv[7] = free(**)
**  parv[8] = ircnet
**/

CMD_FUNC(m_netinfo)
{
	long 		lmax;
	time_t	 	xx;
	long 		endsync, protocol;
	char		buf[512];

	if (parc < 9)
		return 0;

	if (GotNetInfo(cptr))
	{
		sendto_realops("Already got NETINFO from Link %s", cptr->name);
		return 0;
	}

	/* is a long therefore not ATOI */
	lmax = atol(parv[1]);
	endsync = atol(parv[2]);
	protocol = atol(parv[3]);

	/* max global count != max_global_count --sts */
	if (lmax > IRCstats.global_max)
	{
		IRCstats.global_max = lmax;
		sendto_realops("Max Global Count is now %li (set by link %s)",
		    lmax, cptr->name);
	}

	xx = TStime();
	if ((xx - endsync) < 0)
	{
		char *emsg = "";
		if (xx - endsync < -10)
		{
			emsg = " [\002PLEASE SYNC YOUR CLOCKS!\002]";
		}
		sendto_realops
		    ("Possible negative TS split at link %s (%li - %li = %li)%s",
		    cptr->name, (xx), (endsync), (xx - endsync), emsg);
		sendto_server(&me, 0, 0,
		    ":%s SMO o :\2(sync)\2 Possible negative TS split at link %s (%li - %li = %li)%s",
		    me.name, cptr->name, (xx), (endsync), (xx - endsync), emsg);
	}
	sendto_realops
	    ("Link %s -> %s is now synced [secs: %li recv: %ld.%hu sent: %ld.%hu]",
	    cptr->name, me.name, (TStime() - endsync), sptr->local->receiveK,
	    sptr->local->receiveB, sptr->local->sendK, sptr->local->sendB);

	sendto_server(&me, 0, 0,
	    ":%s SMO o :\2(sync)\2 Link %s -> %s is now synced [secs: %li recv: %ld.%hu sent: %ld.%hu]",
	    me.name, cptr->name, me.name, (TStime() - endsync), sptr->local->receiveK,
	    sptr->local->receiveB, sptr->local->sendK, sptr->local->sendB);

	if (!(strcmp(ircnetwork, parv[8]) == 0))
	{
		sendto_realops("Network name mismatch from link %s (%s != %s)",
		    cptr->name, parv[8], ircnetwork);
		sendto_server(&me, 0, 0,
		    ":%s SMO o :\2(sync)\2 Network name mismatch from link %s (%s != %s)",
		    me.name, cptr->name, parv[8], ircnetwork);
	}

	if ((protocol != UnrealProtocol) && (protocol != 0))
	{
		sendto_realops
		    ("Link %s is running Protocol u%li while we are running %d!",
		    cptr->name, protocol, UnrealProtocol);
		sendto_server(&me, 0, 0,
		    ":%s SMO o :\2(sync)\2 Link %s is running u%li while %s is running %d!",
		    me.name, cptr->name, protocol, me.name, UnrealProtocol);

	}
	strlcpy(buf, CLOAK_KEYCRC, sizeof(buf));
	if (*parv[4] != '*' && strcmp(buf, parv[4]))
	{
		sendto_realops
			("Link %s has a DIFFERENT CLOAK KEY - %s != %s. \002YOU SHOULD CORRECT THIS ASAP\002.",
				cptr->name, parv[4], buf);
	}
	SetNetInfo(cptr);
	return 0;
}
