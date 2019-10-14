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

CMD_FUNC(cmd_netinfo);

#define MSG_NETINFO 	"NETINFO"	

ModuleHeader MOD_HEADER
  = {
	"netinfo",
	"5.0",
	"command /netinfo", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_NETINFO, cmd_netinfo, MAXPARA, CMD_SERVER);
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

/** NETINFO: Share configuration settings with directly linked server.
 * Originally written by Stskeeps
 *
 * Technical documentation:
 * https://www.unrealircd.org/docs/Server_protocol:NETINFO_command
 *
 * parv[1] = max global count
 * parv[2] = time of end sync
 * parv[3] = unreal protocol using (numeric)
 * parv[4] = cloak-crc (> u2302)
 * parv[5] = free(**)
 * parv[6] = free(**)
 * parv[7] = free(**)
 * parv[8] = network name
 */
CMD_FUNC(cmd_netinfo)
{
	long 		lmax;
	time_t	 	xx;
	long 		endsync, protocol;
	char		buf[512];

	if (parc < 9)
		return;

	/* Only allow from directly connected servers */
	if (!MyConnect(client))
		return;

	if (IsNetInfo(client))
	{
		sendto_realops("Already got NETINFO from Link %s", client->name);
		return;
	}

	/* is a long therefore not ATOI */
	lmax = atol(parv[1]);
	endsync = atol(parv[2]);
	protocol = atol(parv[3]);

	/* max global count != max_global_count --sts */
	if (lmax > irccounts.global_max)
	{
		irccounts.global_max = lmax;
		sendto_realops("Max Global Count is now %li (set by link %s)",
		    lmax, client->name);
	}

	xx = TStime();
	if ((xx - endsync) < -2)
	{
		char *emsg = "";
		if (xx - endsync < -10)
		{
			emsg = " [\002PLEASE SYNC YOUR CLOCKS!\002]";
		}
		sendto_umode_global(UMODE_OPER,
			"Possible negative TS split at link %s (%lld - %lld = %lld)%s",
			client->name, (long long)(xx), (long long)(endsync), (long long)(xx - endsync), emsg);
	}
	sendto_umode_global(UMODE_OPER,
	    "Link %s -> %s is now synced [secs: %lld recv: %ld.%hu sent: %ld.%hu]",
	    client->name, me.name, (long long)(TStime() - endsync), client->local->receiveK,
	    client->local->receiveB, client->local->sendK, client->local->sendB);

	if (!(strcmp(ircnetwork, parv[8]) == 0))
	{
		sendto_umode_global(UMODE_OPER,
			"Network name mismatch from link %s (%s != %s)",
			client->name, parv[8], ircnetwork);
	}

	if ((protocol != UnrealProtocol) && (protocol != 0))
	{
		sendto_umode_global(UMODE_OPER,
			"Link %s is running Protocol %li while %s is running %d",
			client->name, protocol, me.name, UnrealProtocol);
	}
	strlcpy(buf, CLOAK_KEYCRC, sizeof(buf));
	if (*parv[4] != '*' && strcmp(buf, parv[4]))
	{
		sendto_realops
			("Link %s has a DIFFERENT CLOAK KEY - %s != %s. \002YOU SHOULD CORRECT THIS ASAP\002.",
				client->name, parv[4], buf);
	}
	SetNetInfo(client);
}
