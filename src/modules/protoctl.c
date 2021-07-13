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
	"unrealircd-5",
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
				char buf[512];
				snprintf(buf, sizeof(buf), "Server %s has utf8 in set::allowed-nickchars but %s does not. Link rejected.",
					me.name, *client->name ? client->name : "other side");
				sendto_realops("\002ERROR\001 %s", buf);
				exit_client(client, NULL, buf);
				return;
			}
			/* We compare the character sets to see if we should warn opers about any mismatch... */
			if (strcmp(value, charsys_get_current_languages()))
			{
				sendto_realops("\002WARNING!!!!\002 Link %s does not have the same set::allowed-nickchars settings (or is "
							"a different UnrealIRCd version), this MAY cause display issues. Our charset: '%s', theirs: '%s'",
					get_client_name(client, FALSE), charsys_get_current_languages(), value);
			}
			if (client->serv)
				safe_strdup(client->serv->features.nickchars, value);

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
				char linkerr[256];
				ircsnprintf(linkerr, sizeof(linkerr),
					"Link rejected. Server %s has set::allowed-channelchars '%s' "
					"while %s has a value of '%s'. "
					"Please choose the same value on all servers.",
					client->name, value,
					me.name, allowed_channelchars_valtostr(iConf.allowed_channelchars));
				sendto_realops("ERROR: %s", linkerr);
				exit_client(client, NULL, linkerr);
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
				sendto_one(client, NULL, "ERROR :SID %s already exists from %s", aclient->id, aclient->name);
				sendto_snomask(SNO_SNOTICE, "Link %s rejected - SID %s already exists from %s",
						get_client_name(client, FALSE), aclient->id, aclient->name);
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
			
			servername = strtoken(&p, buf, ",");
			if (!servername || (strlen(servername) > HOSTLEN) || !strchr(servername, '.'))
			{
				sendto_one(client, NULL, "ERROR :Bogus server name in EAUTH (%s)", servername ? servername : "");
				sendto_snomask
				    (SNO_JUNK,
				    "WARNING: Bogus server name (%s) from %s in EAUTH (maybe just a fishy client)",
				    servername ? servername : "", get_client_name(client, TRUE));
				exit_client(client, NULL, "Bogus server name");
				return;
			}
			
			
			protocol = strtoken(&p, NULL, ",");
			if (protocol)
			{
				flags = strtoken(&p, NULL, ",");
				if (flags)
				{
					software = strtoken(&p, NULL, ",");
				}
			}
			
			if (!verify_link(client, servername, &aconf))
				return;

			/* note: software, protocol and flags may be NULL */
			if (!check_deny_version(client, software, protocol ? atoi(protocol) : 0, flags))
				return;

			SetEAuth(client);
			make_server(client); /* allocate and set client->serv */
			/* Set client->name but don't add to hash list. The real work on
			 * that is done in cmd_server. We just set it here for display
			 * purposes of error messages (such as reject due to clock).
			 */
			strlcpy(client->name, servername, sizeof(client->name));
			if (protocol)
				client->serv->features.protocol = atoi(protocol);
			if (software)
				safe_strdup(client->serv->features.software, software);
			if (!IsHandshake(client) && aconf) /* Send PASS early... */
				sendto_one(client, NULL, "PASS :%s", (aconf->auth->type == AUTHTYPE_PLAINTEXT) ? aconf->auth->data : "*");
		}
		else if (!strcmp(name, "SERVERS") && value)
		{
			Client *aclient, *srv;
			char *sid = NULL;
			
			if (!IsEAuth(client))
				continue;
				
			if (client->serv->features.protocol < 2351)
				continue; /* old SERVERS= version */
			
			/* Other side lets us know which servers are behind it.
			 * SERVERS=<sid-of-server-1>[,<sid-of-server-2[,..etc..]]
			 * Eg: SERVER=001,002,0AB,004,005
			 */

			add_pending_net(client, value);

			aclient = find_non_pending_net_duplicates(client);
			if (aclient)
			{
				sendto_one(client, NULL, "ERROR :Server with SID %s (%s) already exists",
					aclient->id, aclient->name);
				sendto_realops("Link %s cancelled, server with SID %s (%s) already exists",
					get_client_name(aclient, TRUE), aclient->id, aclient->name);
				exit_client(client, NULL, "Server Exists (or non-unique me::sid)");
				return;
			}
			
			aclient = find_pending_net_duplicates(client, &srv, &sid);
			if (aclient)
			{
				sendto_one(client, NULL, "ERROR :Server with SID %s is being introduced by another server as well. "
				                 "Just wait a moment for it to synchronize...", sid);
				sendto_realops("Link %s cancelled, server would introduce server with SID %s, which "
				               "server %s is also about to introduce. Just wait a moment for it to synchronize...",
				               get_client_name(aclient, TRUE), sid, get_client_name(srv, TRUE));
				exit_client(client, NULL, "Server Exists (just wait a moment)");
				return;
			}

			/* Send our PROTOCTL SERVERS= back if this was NOT a response */
			if (*value != '*')
				send_protoctl_servers(client, 1);
		}
		else if (!strcmp(name, "TS") && value && (IsServer(client) || IsEAuth(client)))
		{
			long t = atol(value);
			char msg[512], linkerr[512];
			
			if (t < 10000)
				continue; /* ignore */
			
			*msg = *linkerr = '\0';
			
			if ((TStime() - t) > MAX_SERVER_TIME_OFFSET)
			{
				snprintf(linkerr, sizeof(linkerr),
				         "Your clock is %lld seconds behind my clock. "
				         "Please verify both your clock and mine, "
				         "fix it and try linking again.",
				         (long long)(TStime() - t));
				snprintf(msg, sizeof(msg),
				         "Rejecting link %s: our clock is %lld seconds ahead. "
				         "Please verify the clock on both %s (them) and %s (us). "
				         "Correct time is very important for IRC servers, "
				         "see https://www.unrealircd.org/docs/FAQ#fix-your-clock",
				         get_client_name(client, TRUE),
				         (long long)(TStime() - t),
				         client->name, me.name);
			} else
			if ((t - TStime()) > MAX_SERVER_TIME_OFFSET)
			{
				snprintf(linkerr, sizeof(linkerr),
				         "Your clock is %lld seconds ahead of my clock. "
				         "Please verify both your clock and mine, fix it, "
				         "and try linking again.",
				         (long long)(t - TStime()));
				snprintf(msg, sizeof(msg),
				         "Rejecting link %s: our clock is %lld seconds behind. "
				         "Please verify the clock on both %s (them) and %s (us). "
				         "Correct time is very important for IRC servers, "
				         "see https://www.unrealircd.org/docs/FAQ#fix-your-clock",
					get_client_name(client, TRUE),
					(long long)(t - TStime()),
					client->name, me.name);
			}
			
			if (*msg)
			{
				sendto_realops("%s", msg);
				ircd_log(LOG_ERROR, "%s", msg);
				exit_client(client, NULL, linkerr);
				return;
			}
		}
		else if (!strcmp(name, "MLOCK"))
		{
			client->local->proto |= PROTO_MLOCK;
		}
		else if (!strcmp(name, "CHANMODES") && value && client->serv)
		{
			parse_chanmodes_protoctl(client, value);
			/* If this is a runtime change (so post-handshake): */
			if (IsServer(client))
				broadcast_sinfo(client, NULL, client);
		}
		else if (!strcmp(name, "USERMODES") && value && client->serv)
		{
			safe_strdup(client->serv->features.usermodes, value);
			/* If this is a runtime change (so post-handshake): */
			if (IsServer(client))
				broadcast_sinfo(client, NULL, client);
		}
		else if (!strcmp(name, "BOOTED") && value && client->serv)
		{
			client->serv->boottime = atol(value);
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

	if (first_protoctl && IsHandshake(client) && client->serv && !IsServerSent(client)) /* first & outgoing connection to server */
	{
		/* SERVER message moved from completed_connection() to here due to EAUTH/SERVERS PROTOCTL stuff,
		 * which needed to be delayed until after both sides have received SERVERS=xx (..or not.. in case
		 * of older servers).
		 */
		send_server_message(client);
	}
}
