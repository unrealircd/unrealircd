/*
 *   Unreal Internet Relay Chat Daemon, src/modules/oper.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

#define MSG_OPER        "OPER"  /* OPER */

ModuleHeader MOD_HEADER
  = {
	"oper",	/* Name of module */
	"5.0", /* Version */
	"command /oper", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
CMD_FUNC(cmd_oper);
int _make_oper(Client *client, const char *operblock_name, const char *operclass, ConfigItem_class *clientclass, long modes, const char *snomask, const char *vhost);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAdd(modinfo->handle, EFUNC_MAKE_OPER, _make_oper);
	return MOD_SUCCESS;
}

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_OPER, cmd_oper, MAXPARA, CMD_USER);
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

void set_oper_host(Client *client, const char *host)
{
        char uhost[HOSTLEN + USERLEN + 1];
        char *p;
        
        strlcpy(uhost, host, sizeof(uhost));
        
	if ((p = strchr(uhost, '@')))
	{
	        *p++ = '\0';
		strlcpy(client->user->username, uhost, sizeof(client->user->username));
		sendto_server(NULL, 0, 0, NULL, ":%s SETIDENT %s",
		    client->id, client->user->username);
	        host = p;
	}
	iNAH_host(client, host);
	SetHidden(client);
}

int _make_oper(Client *client, const char *operblock_name, const char *operclass, ConfigItem_class *clientclass, long modes, const char *snomask, const char *vhost)
{
	long old_umodes = client->umodes & ALL_UMODES;

	/* Put in the right class (if any) */
	if (clientclass)
	{
		if (client->local->class)
			client->local->class->clients--;
		client->local->class = clientclass;
		client->local->class->clients++;
	}

	/* set oper user modes */
	client->umodes |= UMODE_OPER;
	if (modes)
		client->umodes |= modes; /* oper::modes */
	else
		client->umodes |= OPER_MODES; /* set::modes-on-oper */

	/* oper::vhost */
	if (vhost)
	{
		set_oper_host(client, vhost);
	} else
	if (IsHidden(client) && !client->user->virthost)
	{
		/* +x has just been set by modes-on-oper and no vhost. cloak the oper! */
		safe_strdup(client->user->virthost, client->user->cloakedhost);
	}

	unreal_log(ULOG_INFO, "oper", "OPER_SUCCESS", client,
		   "$client.details is now an IRC Operator [oper-block: $oper_block] [operclass: $operclass]",
		   log_data_string("oper_block", operblock_name),
		   log_data_string("operclass", operclass));

	/* set oper snomasks */
	if (snomask)
		set_snomask(client, snomask); /* oper::snomask */
	else
		set_snomask(client, OPER_SNOMASK); /* set::snomask-on-oper */

	send_umode_out(client, 1, old_umodes);
	if (client->user->snomask)
		sendnumeric(client, RPL_SNOMASK, client->user->snomask);

	list_add(&client->special_node, &oper_list);

	RunHook(HOOKTYPE_LOCAL_OPER, client, 1, operblock_name, operclass);

	sendnumeric(client, RPL_YOUREOPER);

	/* Update statistics */
	if (IsInvisible(client) && !(old_umodes & UMODE_INVISIBLE))
		irccounts.invisible++;
	if (IsOper(client) && !IsHideOper(client))
		irccounts.operators++;

	if (SHOWOPERMOTD == 1)
	{
		const char *args[1] = { NULL };
		do_cmd(client, NULL, "OPERMOTD", 1, args);
	}

	if (!BadPtr(OPER_AUTO_JOIN_CHANS) && strcmp(OPER_AUTO_JOIN_CHANS, "0"))
	{
		char *chans = strdup(OPER_AUTO_JOIN_CHANS);
		const char *args[3] = {
			client->name,
			chans,
			NULL
		};
		do_cmd(client, NULL, "JOIN", 3, args);
		safe_free(chans);
		/* Theoretically the oper may be killed on join. Would be fun, though */
		if (IsDead(client))
			return 0;
	}

	return 1;
}

/*
** cmd_oper
**	parv[1] = oper name
**	parv[2] = oper password
*/
CMD_FUNC(cmd_oper)
{
	ConfigItem_oper *operblock;
	const char *operblock_name, *password;

	if (!MyUser(client))
		return;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "OPER");
		return;
	}

	if (SVSNOOP)
	{
		sendnotice(client,
		    "*** This server is in NOOP mode, you cannot /oper");
		return;
	}

	if (IsOper(client))
	{
		sendnotice(client, "You are already an IRC Operator. If you want to re-oper then de-oper first via /MODE yournick -o");
		return;
	}

	operblock_name = parv[1];
	password = (parc > 2) ? parv[2] : "";

	/* set::plaintext-policy::oper 'deny' */
	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_oper == POLICY_DENY))
	{
		sendnotice_multiline(client, iConf.plaintext_policy_oper_message);
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Not using TLS"),
		           log_data_string("fail_type", "NO_TLS"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	/* set::outdated-tls-policy::oper 'deny' */
	if (IsSecure(client) && (iConf.outdated_tls_policy_oper == POLICY_DENY) && outdated_tls_client(client))
	{
		sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, client));
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Outdated TLS protocol or cipher"),
		           log_data_string("fail_type", "OUTDATED_TLS_PROTOCOL_OR_CIPHER"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	if (!(operblock = find_oper(operblock_name)))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Unknown oper operblock_name"),
		           log_data_string("fail_type", "UNKNOWN_OPER_NAME"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	/* Below here, the oper block exists, any errors here we take (even)
	 * more seriously, they are logged as errors instead of warnings.
	 */

	if (!unreal_mask_match(client, operblock->mask))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		unreal_log(ULOG_ERROR, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Host does not match"),
		           log_data_string("fail_type", "NO_HOST_MATCH"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	if (!Auth_Check(client, operblock->auth, password))
	{
		sendnumeric(client, ERR_PASSWDMISMATCH);
		if (FAILOPER_WARN)
			sendnotice(client,
			    "*** Your attempt has been logged.");
		unreal_log(ULOG_ERROR, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Authentication failed"),
		           log_data_string("fail_type", "AUTHENTICATION_FAILED"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	/* Authentication of the oper succeeded (like, password, ssl cert),
	 * but we still have some other restrictions to check below as well,
	 * like 'require-modes' and 'maxlogins'...
	 */

	/* Check oper::require_modes */
	if (operblock->require_modes & ~client->umodes)
	{
		sendnumericfmt(client, ERR_NOOPERHOST, ":You are missing user modes required to OPER");
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Not matching oper::require-modes"),
		           log_data_string("fail_type", "REQUIRE_MODES_NOT_SATISFIED"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 7000);
		return;
	}

	if (!find_operclass(operblock->operclass))
	{
		sendnotice(client, "ERROR: There is a non-existant oper::operclass specified for your oper block");
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "Config error: invalid oper::operclass"),
		           log_data_string("fail_type", "OPER_OPERCLASS_INVALID"),
		           log_data_string("oper_block", parv[1]));
		return;
	}

	if (operblock->maxlogins && (count_oper_sessions(operblock->name) >= operblock->maxlogins))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		sendnotice(client, "Your maximum number of concurrent oper logins has been reached (%d)",
			operblock->maxlogins);
		unreal_log(ULOG_WARNING, "oper", "OPER_FAILED", client,
		           "Failed OPER attempt by $client.details [reason: $reason] [oper-block: $oper_block]",
		           log_data_string("reason", "oper::maxlogins limit reached"),
		           log_data_string("fail_type", "OPER_MAXLOGINS_LIMIT"),
		           log_data_string("oper_block", parv[1]));
		add_fake_lag(client, 4000);
		return;
	}

	/* /OPER really succeeded now. Start processing it. */

	/* Store which oper block was used to become IRCOp (for maxlogins and whois) */
	safe_strdup(client->user->operlogin, operblock->name);

	/* oper::swhois */
	if (operblock->swhois)
	{
		SWhois *s;
		for (s = operblock->swhois; s; s = s->next)
			swhois_add(client, "oper", -100, s->line, &me, NULL);
	}

	make_oper(client, operblock->name, operblock->operclass, operblock->class, operblock->modes, operblock->snomask, operblock->vhost);

	/* set::plaintext-policy::oper 'warn' */
	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_oper == POLICY_WARN))
	{
		sendnotice_multiline(client, iConf.plaintext_policy_oper_message);
		unreal_log(ULOG_WARNING, "oper", "OPER_UNSAFE", client,
			   "Insecure (non-TLS) connection used to OPER up by $client.details [oper-block: $oper_block]",
			   log_data_string("oper_block", parv[1]),
		           log_data_string("warn_type", "NO_TLS"));
	}

	/* set::outdated-tls-policy::oper 'warn' */
	if (IsSecure(client) && (iConf.outdated_tls_policy_oper == POLICY_WARN) && outdated_tls_client(client))
	{
		sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, client));
		unreal_log(ULOG_WARNING, "oper", "OPER_UNSAFE", client,
			   "Outdated TLS protocol/cipher used to OPER up by $client.details [oper-block: $oper_block]",
			   log_data_string("oper_block", parv[1]),
		           log_data_string("warn_type", "OUTDATED_TLS_PROTOCOL_OR_CIPHER"));
	}
}
