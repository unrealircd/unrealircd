/*
 *   IRC - Internet Relay Chat, src/modules/protoctl.c
 *   (C) 2004- The UnrealIRCd Team
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

CMD_FUNC(cmd_protoctl);

#define MSG_PROTOCTL 	"PROTOCTL"	

ModuleHeader MOD_HEADER
  = {
	"protoctl",
	"5.0",
	"command /protoctl", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PROTOCTL, cmd_protoctl, MAXPARA, CMD_UNREGISTERED|CMD_SERVER|CMD_USER);
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

#define MAX_SERVER_TIME_OFFSET 60

/* The PROTOCTL command is used for negotiating capabilities with
 * directly connected servers.
 * See https://www.unrealircd.org/docs/Server_protocol:PROTOCTL_command
 * for all technical documentation, especially if you are a server
 * or services coder.
 */
CMD_FUNC(cmd_protoctl)
{
	int  i;
	int first_protoctl = IsProtoctlReceived(client) ? 0 : 1; /**< First PROTOCTL we receive? Special ;) */
	char proto[512];
	char *name, *value, *p;

	if (!MyConnect(client))
		return; /* Remote PROTOCTL's are not supported */

	SetProtoctlReceived(client);

	for (i = 1; i < parc; i++)
	{
		strlcpy(proto, parv[i], sizeof proto);
		p = strchr(proto, '=');
		if (p)
		{
			name = proto;
			*p++ = '\0';
			value = p;
		} else {
			name = proto;
			value = NULL;
		}

		if (!strcmp(name, "NAMESX"))
		{
			SetCapability(client, "multi-prefix");
		}
		else if (!strcmp(name, "UHNAMES") && UHNAMES_ENABLED)
		{
			SetCapability(client, "userhost-in-names");
		}
		else if (IsUser(client))
		{
			return;
		}
		else if (!strcmp(name, "VL"))
		{
			SetVL(client);
		}
		else if (!strcmp(name, "VHP"))
		{
			SetVHP(client);
		}
		else if (!strcmp(name, "CLK"))
		{
			SetCLK(client);
		}
		else if (!strcmp(name, "SJSBY") && iConf.ban_setter_sync)
		{
			SetSJSBY(client);
		}
		else if (!strcmp(name, "MTAGS"))
		{
			SetMTAGS(client);
		}
		else if (!strcmp(name, "NEXTBANS"))
		{
			SetNEXTBANS(client);
		}
		else if (!strcmp(name, "NICKCHARS") && value)
		{
			if (!IsServer(client) && !IsEAuth(client) && !IsHandshake(client))
				continue;
			/* Ok, server is either authenticated, or is an outgoing connect... */
			/* Some combinations are fatal because they would lead to mass-kills:
			 * - use of 'utf8' on our server but not on theirs
			 */
			if (strstr(charsys_get_current_languages(), "utf8") && !strstr(value, "utf8"))
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_CHARSYS_INCOMPATIBLE", client,
					   "Server link $client rejected. Server $me_name has utf8 in set::allowed-nickchars but $client does not.",
					   log_data_string("me_name", me.name));
				exit_client(client, NULL, "Incompatible set::allowed-nickchars setting");
				return;
			}
			/* We compare the character sets to see if we should warn opers about any mismatch... */
			if (strcmp(value, charsys_get_current_languages()))
			{
				unreal_log(ULOG_WARNING, "link", "LINK_WARNING_CHARSYS", client,
					   "Server link $client does not have the same set::allowed-nickchars settings, "
					   "this may possibly cause display issues. Our charset: '$our_charsys', theirs: '$their_charsys'",
					   log_data_string("our_charsys", charsys_get_current_languages()),
					   log_data_string("their_charsys", value));
			}
			if (client->server)
				safe_strdup(client->server->features.nickchars, value);

			/* If this is a runtime change (so post-handshake): */
			if (IsServer(client))
				broadcast_sinfo(client, NULL, client);
		}
		else if (!strcmp(name, "CHANNELCHARS") && value)
		{
			int their_value;

			if (!IsServer(client) && !IsEAuth(client) && !IsHandshake(client))
				continue;

			their_value = allowed_channelchars_strtoval(value);
			if (their_value != iConf.allowed_channelchars)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_ALLOWED_CHANNELCHARS_INCOMPATIBLE", client,
					   "Server link $client rejected. Server has set::allowed-channelchars setting "
					   "of $their_allowed_channelchars, while we have $our_allowed_channelchars.\n"
					   "Please set set::allowed-channelchars to the same value on all servers.",
					   log_data_string("their_allowed_channelchars", value),
					   log_data_string("our_allowed_channelchars", allowed_channelchars_valtostr(iConf.allowed_channelchars)));
				exit_client(client, NULL, "Incompatible set::allowed-channelchars setting");
				return;
			}
		}
		else if (!strcmp(name, "SID") && value)
		{
			Client *aclient;
			char *sid = value;

			if (!IsServer(client) && !IsEAuth(client) && !IsHandshake(client))
			{
				exit_client(client, NULL, "Got PROTOCTL SID before EAUTH, that's the wrong order!");
				return;
			}

			if (*client->id && (strlen(client->id)==3))
			{
				exit_client(client, NULL, "Got PROTOCTL SID twice");
				return;
			}

			if (!valid_sid(value))
			{
				exit_client(client, NULL, "Invalid SID. The first character must be a digit and the other two characters must be A-Z0-9. Eg: 0AA.");
				return;
			}

			if (IsServer(client))
			{
				exit_client(client, NULL, "Got PROTOCTL SID after SERVER, that's the wrong order!");
				return;
			}

			if ((aclient = hash_find_id(sid, NULL)) != NULL)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_SID_COLLISION", client,
					   "Server link $client rejected. Server with SID $sid already exist via uplink $existing_client.server.uplink.",
					   log_data_string("sid", sid),
					   log_data_client("existing_client", aclient));
				exit_client(client, NULL, "SID collision");
				return;
			}

			if (*client->id)
				del_from_id_hash_table(client->id, client); /* delete old UID entry (created on connect) */
			strlcpy(client->id, sid, IDLEN);
			add_to_id_hash_table(client->id, client); /* add SID */
		}
		else if (!strcmp(name, "EAUTH") && value)
		{
			/* Early authorization: EAUTH=servername,protocol,flags,software
			 * (Only servername is mandatory, rest is optional)
			 */
			int ret;
			char *p;
			char *servername = NULL, *protocol = NULL, *flags = NULL, *software = NULL;
			char buf[512];
			ConfigItem_link *aconf = NULL;

			if (IsEAuth(client))
			{
				exit_client(client, NULL, "PROTOCTL EAUTH received twice");
				return;
			}

			strlcpy(buf, value, sizeof(buf));
			p = strchr(buf, ' ');
			if (p)
			{
				*p = '\0';
				p = NULL;
			}
			
			servername = strtoken_noskip(&p, buf, ",");
			if (!servername || !valid_server_name(servername))
			{
				exit_client(client, NULL, "Bogus server name");
				return;
			}
			
			
			protocol = strtoken_noskip(&p, NULL, ",");
			if (protocol)
			{
				flags = strtoken_noskip(&p, NULL, ",");
				if (flags)
					software = strtoken_noskip(&p, NULL, ",");
			}
			
			/* Set client->name but don't add to hash list, this gives better
			 * log messages and should be safe. See CMTSRV941 in server.c.
			 */
			strlcpy(client->name, servername, sizeof(client->name));

			if (!(aconf = verify_link(client)))
				return;

			/* note: software, protocol and flags may be NULL */
			if (!check_deny_version(client, software, protocol ? atoi(protocol) : 0, flags))
				return;

			SetEAuth(client);
			make_server(client); /* allocate and set client->server */
			if (protocol)
				client->server->features.protocol = atoi(protocol);
			if (software)
				safe_strdup(client->server->features.software, software);
			if (is_services_but_not_ulined(client))
			{
				exit_client_fmt(client, NULL, "Services detected but no ulines { } for server name %s", client->name);
				return;
			}
			if (!IsHandshake(client) && aconf) /* Send PASS early... */
				sendto_one(client, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");
		}
		else if (!strcmp(name, "SERVERS") && value)
		{
			Client *aclient, *srv;
			char *sid = NULL;
			
			if (!IsEAuth(client))
				continue;
				
			if (client->server->features.protocol < 2351)
				continue; /* old SERVERS= version */
			
			/* Other side lets us know which servers are behind it.
			 * SERVERS=<sid-of-server-1>[,<sid-of-server-2[,..etc..]]
			 * Eg: SERVERS=001,002,0AB,004,005
			 */

			add_pending_net(client, value);

			aclient = find_non_pending_net_duplicates(client);
			if (aclient)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_DUPLICATE_SID", client,
					   "Denied server $client: Server with SID $existing_client.id ($existing_client) is already linked.",
					   log_data_client("existing_client", aclient));
				exit_client(client, NULL, "Server Exists (or non-unique me::sid)");
				return;
			}
			
			aclient = find_pending_net_duplicates(client, &srv, &sid);
			if (aclient)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_DUPLICATE_SID_LINKED", client,
					   "Denied server $client: Server would (later) introduce SID $sid, "
					   "but we already have SID $sid linked ($existing_client)\n"
					   "Possible race condition, just wait a moment for the network to synchronize...",
					   log_data_string("sid", sid),
					   log_data_client("existing_client", aclient));
				exit_client(client, NULL, "Server Exists (just wait a moment...)");
				return;
			}

			/* Send our PROTOCTL SERVERS= back if this was NOT a response */
			if (*value != '*')
				send_protoctl_servers(client, 1);
		}
		else if (!strcmp(name, "TS") && value && (IsServer(client) || IsEAuth(client)))
		{
			long t = atol(value);

			if (t < 10000)
				continue; /* ignore */

			if ((TStime() - t) > MAX_SERVER_TIME_OFFSET)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_CLOCK_INCORRECT", client,
				           "Denied server $client: clock on server $client is $time_delta "
				           "seconds behind the clock of $me_name.\n"
				           "Correct time is very important for IRC servers, "
				           "see https://www.unrealircd.org/docs/FAQ#fix-your-clock",
				           log_data_integer("time_delta", TStime() - t),
				           log_data_string("me_name", me.name));
				exit_client_fmt(client, NULL, "Incorrect clock. Our clocks are %lld seconds apart.",
				                (long long)(TStime() - t));
				return;
			} else
			if ((t - TStime()) > MAX_SERVER_TIME_OFFSET)
			{
				unreal_log(ULOG_ERROR, "link", "LINK_DENIED_CLOCK_INCORRECT", client,
				           "Denied server $client: clock on server $client is $time_delta "
				           "seconds ahead the clock of $me_name.\n"
				           "Correct time is very important for IRC servers, "
				           "see https://www.unrealircd.org/docs/FAQ#fix-your-clock",
				           log_data_integer("time_delta", t - TStime()),
				           log_data_string("me_name", me.name));
				exit_client_fmt(client, NULL, "Incorrect clock. Our clocks are %lld seconds apart.",
				                (long long)(t - TStime()));
				return;
			}
		}
		else if (!strcmp(name, "MLOCK"))
		{
			client->local->proto |= PROTO_MLOCK;
		}
		else if (!strcmp(name, "CHANMODES") && value && client->server)
		{
			parse_chanmodes_protoctl(client, value);
			/* If this is a runtime change (so post-handshake): */
			if (IsServer(client))
				broadcast_sinfo(client, NULL, client);
		}
		else if (!strcmp(name, "USERMODES") && value && client->server)
		{
			safe_strdup(client->server->features.usermodes, value);
			/* If this is a runtime change (so post-handshake): */
			if (IsServer(client))
				broadcast_sinfo(client, NULL, client);
		}
		else if (!strcmp(name, "BOOTED") && value && client->server)
		{
			client->server->boottime = atol(value);
		}
		else if (!strcmp(name, "EXTSWHOIS"))
		{
			client->local->proto |= PROTO_EXTSWHOIS;
		}
		/* You can add protocol extensions here.
		 * Use 'name' and 'value' (the latter may be NULL).
		 *
		 * DO NOT error or warn on unknown proto; we just don't
		 * support it.
		 */
	}

	if (first_protoctl && IsHandshake(client) && client->server && !IsServerSent(client)) /* first & outgoing connection to server */
	{
		/* SERVER message moved from completed_connection() to here due to EAUTH/SERVERS PROTOCTL stuff,
		 * which needed to be delayed until after both sides have received SERVERS=xx (..or not.. in case
		 * of older servers).
		 */
		send_server_message(client);
	}
}
