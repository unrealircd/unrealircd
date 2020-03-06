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

CMD_FUNC(cmd_oper);


/* Place includes here */
#define MSG_OPER        "OPER"  /* OPER */

ModuleHeader MOD_HEADER
  = {
	"oper",	/* Name of module */
	"5.0", /* Version */
	"command /oper", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_OPER, cmd_oper, MAXPARA, CMD_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void set_oper_host(Client *client, char *host)
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

/*
** cmd_oper
**	parv[1] = oper name
**	parv[2] = oper password
*/
CMD_FUNC(cmd_oper)
{
	ConfigItem_oper *operblock;
	char *name, *password;
	long old_umodes = client->umodes & ALL_UMODES;

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
		sendnumeric(client, RPL_YOUREOPER);
		// TODO: de-confuse this ? ;)
		return;
	}

	name = parv[1];
	password = (parc > 2) ? parv[2] : "";

	/* set::plaintext-policy::oper 'deny' */
	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_oper == POLICY_DENY))
	{
		sendnotice_multiline(client, iConf.plaintext_policy_oper_message);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [not using SSL/TLS]",
		    client->name, client->user->username, client->local->sockhost);
		ircd_log(LOG_OPER, "OPER NO-SSL/TLS (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		client->local->since += 7;
		return;
	}

	/* set::outdated-tls-policy::oper 'deny' */
	if (IsSecure(client) && (iConf.outdated_tls_policy_oper == POLICY_DENY) && outdated_tls_client(client))
	{
		sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, client));
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [outdated SSL/TLS protocol or cipher]",
		    client->name, client->user->username, client->local->sockhost);
		ircd_log(LOG_OPER, "OPER OUTDATED-SSL/TLS (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		client->local->since += 7;
		return;
	}

	if (!(operblock = find_oper(name)))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [unknown oper]",
		    client->name, client->user->username, client->local->sockhost);
		ircd_log(LOG_OPER, "OPER UNKNOWNOPER (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		client->local->since += 7;
		return;
	}

	if (!unreal_mask_match(client, operblock->mask))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [host doesnt match]",
		    client->name, client->user->username, client->local->sockhost, name);
		ircd_log(LOG_OPER, "OPER NOHOSTMATCH (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		client->local->since += 7;
		return;
	}

	if (!Auth_Check(client, operblock->auth, password))
	{
		sendnumeric(client, ERR_PASSWDMISMATCH);
		if (FAILOPER_WARN)
			sendnotice(client,
			    "*** Your attempt has been logged.");
		ircd_log(LOG_OPER, "OPER FAILEDAUTH (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [FAILEDAUTH]",
		    client->name, client->user->username, client->local->sockhost, name);
		client->local->since += 7;
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
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) [lacking modes '%s' in oper::require-modes]",
			 client->name, client->user->username, client->local->sockhost, get_usermode_string_raw(operblock->require_modes & ~client->umodes));
		ircd_log(LOG_OPER, "OPER MISSINGMODES (%s) by (%s!%s@%s), needs modes=%s",
			 name, client->name, client->user->username, client->local->sockhost,
			 get_usermode_string_raw(operblock->require_modes & ~client->umodes));
		client->local->since += 7;
		return;
	}

	if (!find_operclass(operblock->operclass))
	{
		sendnotice(client, "ERROR: There is a non-existant oper::operclass specified for your oper block");
		ircd_log(LOG_ERROR, "OPER MISSINGOPERCLASS (%s) by (%s!%s@%s), oper::operclass does not exist: %s",
			name, client->name, client->user->username, client->local->sockhost,
			operblock->operclass);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) [oper::operclass does not exist: '%s']",
			client->name, client->user->username, client->local->sockhost, operblock->operclass);
		return;
	}

	if (operblock->maxlogins && (count_oper_sessions(operblock->name) >= operblock->maxlogins))
	{
		sendnumeric(client, ERR_NOOPERHOST);
		sendnotice(client, "Your maximum number of concurrent oper logins has been reached (%d)",
			operblock->maxlogins);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [maxlogins reached]",
			client->name, client->user->username, client->local->sockhost, name);
		ircd_log(LOG_OPER, "OPER TOOMANYLOGINS (%s) by (%s!%s@%s)", name, client->name,
			client->user->username, client->local->sockhost);
		client->local->since += 4;
		return;
	}

	/* /OPER really succeeded now. Start processing it. */

	/* Store which oper block was used to become IRCOp (for maxlogins and whois) */
	safe_strdup(client->user->operlogin, operblock->name);

	/* Put in the right class */
	if (client->local->class)
		client->local->class->clients--;
	client->local->class = operblock->class;
	client->local->class->clients++;

	/* oper::swhois */
	if (operblock->swhois)
	{
		SWhois *s;
		for (s = operblock->swhois; s; s = s->next)
			swhois_add(client, "oper", -100, s->line, &me, NULL);
	}

	/* set oper user modes */
	client->umodes |= UMODE_OPER;
	if (operblock->modes)
		client->umodes |= operblock->modes; /* oper::modes */
	else
		client->umodes |= OPER_MODES; /* set::modes-on-oper */

	/* oper::vhost */
	if (operblock->vhost)
	{
		set_oper_host(client, operblock->vhost);
	} else
	if (IsHidden(client) && !client->user->virthost)
	{
		/* +x has just been set by modes-on-oper and no vhost. cloak the oper! */
		safe_strdup(client->user->virthost, client->user->cloakedhost);
	}

	sendto_snomask_global(SNO_OPER,
		"%s (%s@%s) [%s] is now an operator",
		client->name, client->user->username, client->local->sockhost,
		parv[1]);

	ircd_log(LOG_OPER, "OPER (%s) by (%s!%s@%s)", name, client->name, client->user->username,
		client->local->sockhost);

	/* set oper snomasks */
	if (operblock->snomask)
		set_snomask(client, operblock->snomask); /* oper::snomask */
	else
		set_snomask(client, OPER_SNOMASK); /* set::snomask-on-oper */

	/* some magic to set user mode +s (and snomask +s) if you have any snomasks set */
	if (client->user->snomask)
	{
		client->user->snomask |= SNO_SNOTICE;
		client->umodes |= UMODE_SERVNOTICE;
	}
	
	send_umode_out(client, 1, old_umodes);
	sendnumeric(client, RPL_SNOMASK, get_snomask_string(client));

	list_add(&client->special_node, &oper_list);

	RunHook2(HOOKTYPE_LOCAL_OPER, client, 1);

	sendnumeric(client, RPL_YOUREOPER);

	/* Update statistics */
	if (IsInvisible(client) && !(old_umodes & UMODE_INVISIBLE))
		irccounts.invisible++;
	if (IsOper(client) && !IsHideOper(client))
		irccounts.operators++;

	if (SHOWOPERMOTD == 1)
		do_cmd(client, NULL, "OPERMOTD", parc, parv);

	if (!BadPtr(OPER_AUTO_JOIN_CHANS) && strcmp(OPER_AUTO_JOIN_CHANS, "0"))
	{
		char *chans = strdup(OPER_AUTO_JOIN_CHANS);
		char *args[3] = {
			client->name,
			chans,
			NULL
		};
		do_cmd(client, NULL, "JOIN", 3, args);
		safe_free(chans);
		/* Theoretically the oper may be killed on join. Would be fun, though */
		if (IsDead(client))
			return;
	}

	/* set::plaintext-policy::oper 'warn' */
	if (!IsSecure(client) && !IsLocalhost(client) && (iConf.plaintext_policy_oper == POLICY_WARN))
	{
		sendnotice_multiline(client, iConf.plaintext_policy_oper_message);
		sendto_snomask_global
		    (SNO_OPER, "OPER %s [%s] used an insecure (non-SSL/TLS) connection to /OPER.",
		    client->name, name);
	}

	/* set::outdated-tls-policy::oper 'warn' */
	if (IsSecure(client) && (iConf.outdated_tls_policy_oper == POLICY_WARN) && outdated_tls_client(client))
	{
		sendnotice(client, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, client));
		sendto_snomask_global
		    (SNO_OPER, "OPER %s [%s] used a connection with an outdated SSL/TLS protocol or cipher to /OPER.",
		    client->name, name);
	}
}
