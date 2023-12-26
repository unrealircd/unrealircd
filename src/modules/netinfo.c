/*
 *   IRC - Internet Relay Chat, src/modules/netinfo.c
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
	"unrealircd-6",
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
 * parv[4] = cloak key check (> u2302)
 * parv[5] = free(**)
 * parv[6] = free(**)
 * parv[7] = free(**)
 * parv[8] = network name
 */
CMD_FUNC(cmd_netinfo)
{
	long 		lmax;
	long 		endsync, protocol;
	char		buf[512];

	if (parc < 9)
		return;

	/* Only allow from directly connected servers */
	if (!MyConnect(client))
		return;

	if (IsNetInfo(client))
	{
		unreal_log(ULOG_WARNING, "link", "NETINFO_ALREADY_RECEIVED", client,
		           "Got NETINFO from server $client, but we already received it earlier!");
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
		unreal_log(ULOG_INFO, "link", "NEW_GLOBAL_RECORD", client,
		           "Record global users is now $record_global_users (set by server $client)",
		           log_data_integer("record_global_users", lmax));
	}

	unreal_log(ULOG_INFO, "link", "SERVER_SYNCED", client,
	           "Link $client -> $me is now synced "
	           "[secs: $synced_after_seconds, recv: $received_bytes, sent: $sent_bytes]",
	           log_data_client("me", &me),
	           log_data_integer("synced_after_seconds", TStime() - endsync),
	           log_data_integer("received_bytes", client->local->traffic.bytes_received),
	           log_data_integer("sent_bytes", client->local->traffic.bytes_sent));

	if (!(strcmp(NETWORK_NAME, parv[8]) == 0))
	{
		unreal_log(ULOG_WARNING, "link", "NETWORK_NAME_MISMATCH", client,
		           "Network name mismatch: server $client has '$their_network_name', "
		           "server $me has '$our_network_name'.",
		           log_data_client("me", &me),
		           log_data_string("their_network_name", parv[8]),
		           log_data_string("our_network_name", NETWORK_NAME));
	}

	if ((protocol != UnrealProtocol) && (protocol != 0))
	{
		unreal_log(ULOG_INFO, "link", "LINK_PROTOCOL_MISMATCH", client,
		           "Server $client is running UnrealProtocol $their_link_protocol, "
		           "server $me uses $our_link_protocol.",
		           log_data_client("me", &me),
		           log_data_integer("their_link_protocol", protocol),
		           log_data_integer("our_link_protocol", UnrealProtocol));
	}
	strlcpy(buf, CLOAK_KEY_CHECKSUM, sizeof(buf));
	if (*parv[4] != '*' && strcmp(buf, parv[4]))
	{
		unreal_log(ULOG_WARNING, "link", "CLOAK_KEY_MISMATCH", client,
		           "Server $client has a DIFFERENT CLOAK KEY (OR METHOD)!!! You should fix this ASAP!\n"
		           "When the cloaking configuration is different on servers, this will cause "
		           "channel bans on cloaked hosts/IPs not to work correctly, "
		           "meaning users can bypass channel bans!");
	}
	SetNetInfo(client);
}
