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
	CommandAdd(modinfo->handle, MSG_OPER, cmd_oper, MAXPARA, M_USER);
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

void set_oper_host(Client *sptr, char *host)
{
        char uhost[HOSTLEN + USERLEN + 1];
        char *p;
        
        strlcpy(uhost, host, sizeof(uhost));
        
	if ((p = strchr(uhost, '@')))
	{
	        *p++ = '\0';
		strlcpy(sptr->user->username, uhost, sizeof(sptr->user->username));
		sendto_server(NULL, 0, 0, NULL, ":%s SETIDENT %s",
		    sptr->name, sptr->user->username);
	        host = p;
	}
	iNAH_host(sptr, host);
	SetHidden(sptr);
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
	long old_umodes = sptr->umodes & ALL_UMODES;

	if (!MyUser(sptr))
		return 0;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "OPER");
		return 0;
	}

	if (SVSNOOP)
	{
		sendnotice(sptr,
		    "*** This server is in NOOP mode, you cannot /oper");
		return 0;
	}

	if (IsOper(sptr))
	{
		sendnumeric(sptr, RPL_YOUREOPER);
		// TODO: de-confuse this ? ;)
		return 0;
	}

	name = parv[1];
	password = (parc > 2) ? parv[2] : "";

	/* set::plaintext-policy::oper 'deny' */
	if (!IsSecure(sptr) && !IsLocalhost(sptr) && (iConf.plaintext_policy_oper == POLICY_DENY))
	{
		sendnotice(sptr, "%s", iConf.plaintext_policy_oper_message);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [not using SSL/TLS]",
		    sptr->name, sptr->user->username, sptr->local->sockhost);
		ircd_log(LOG_OPER, "OPER NO-SSL/TLS (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sptr->local->since += 7;
		return 0;
	}

	/* set::outdated-tls-policy::oper 'deny' */
	if (IsSecure(sptr) && (iConf.outdated_tls_policy_oper == POLICY_DENY) && outdated_tls_client(sptr))
	{
		sendnotice(sptr, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, sptr));
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [outdated SSL/TLS protocol or cipher]",
		    sptr->name, sptr->user->username, sptr->local->sockhost);
		ircd_log(LOG_OPER, "OPER OUTDATED-SSL/TLS (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sptr->local->since += 7;
		return 0;
	}

	if (!(operblock = Find_oper(name)))
	{
		sendnumeric(sptr, ERR_NOOPERHOST);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [unknown oper]",
		    sptr->name, sptr->user->username, sptr->local->sockhost);
		ircd_log(LOG_OPER, "OPER UNKNOWNOPER (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sptr->local->since += 7;
		return 0;
	}

	if (!unreal_mask_match(sptr, operblock->mask))
	{
		sendnumeric(sptr, ERR_NOOPERHOST);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [host doesnt match]",
		    sptr->name, sptr->user->username, sptr->local->sockhost, name);
		ircd_log(LOG_OPER, "OPER NOHOSTMATCH (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sptr->local->since += 7;
		return 0;
	}

	if (!Auth_Check(cptr, operblock->auth, password))
	{
		sendnumeric(sptr, ERR_PASSWDMISMATCH);
		if (FAILOPER_WARN)
			sendnotice(sptr,
			    "*** Your attempt has been logged.");
		ircd_log(LOG_OPER, "OPER FAILEDAUTH (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [FAILEDAUTH]",
		    sptr->name, sptr->user->username, sptr->local->sockhost, name);
		sptr->local->since += 7;
		return 0;
	}

	/* Authentication of the oper succeeded (like, password, ssl cert),
	 * but we still have some other restrictions to check below as well,
	 * like 'require-modes' and 'maxlogins'...
	 */

	/* Check oper::require_modes */
	if (operblock->require_modes & ~sptr->umodes)
	{
		sendnumericfmt(sptr, ERR_NOOPERHOST, "You are missing user modes required to OPER");
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) [lacking modes '%s' in oper::require-modes]",
			 sptr->name, sptr->user->username, sptr->local->sockhost, get_modestr(operblock->require_modes & ~sptr->umodes));
		ircd_log(LOG_OPER, "OPER MISSINGMODES (%s) by (%s!%s@%s), needs modes=%s",
			 name, sptr->name, sptr->user->username, sptr->local->sockhost,
			 get_modestr(operblock->require_modes & ~sptr->umodes));
		sptr->local->since += 7;
		return 0;
	}

	if (!Find_operclass(operblock->operclass))
	{
		sendnotice(sptr, "ERROR: There is a non-existant oper::operclass specified for your oper block");
		ircd_log(LOG_ERROR, "OPER MISSINGOPERCLASS (%s) by (%s!%s@%s), oper::operclass does not exist: %s",
			name, sptr->name, sptr->user->username, sptr->local->sockhost,
			operblock->operclass);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) [oper::operclass does not exist: '%s']",
			sptr->name, sptr->user->username, sptr->local->sockhost, operblock->operclass);
		return 0;
	}

	if (operblock->maxlogins && (count_oper_sessions(operblock->name) >= operblock->maxlogins))
	{
		sendnumeric(sptr, ERR_NOOPERHOST);
		sendnotice(sptr, "Your maximum number of concurrent oper logins has been reached (%d)",
			operblock->maxlogins);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [maxlogins reached]",
			sptr->name, sptr->user->username, sptr->local->sockhost, name);
		ircd_log(LOG_OPER, "OPER TOOMANYLOGINS (%s) by (%s!%s@%s)", name, sptr->name,
			sptr->user->username, sptr->local->sockhost);
		sptr->local->since += 4;
		return 0;
	}

	/* /OPER really succeeded now. Start processing it. */

	/* Store which oper block was used to become IRCOp (for maxlogins and whois) */
	safe_strdup(sptr->user->operlogin, operblock->name);

	/* Put in the right class */
	if (sptr->local->class)
		sptr->local->class->clients--;
	sptr->local->class = operblock->class;
	sptr->local->class->clients++;

	/* oper::swhois */
	if (operblock->swhois)
	{
		SWhois *s;
		for (s = operblock->swhois; s; s = s->next)
			swhois_add(sptr, "oper", -100, s->line, &me, NULL);
	}

	/* set oper user modes */
	sptr->umodes |= UMODE_OPER;
	if (operblock->modes)
		sptr->umodes |= operblock->modes; /* oper::modes */
	else
		sptr->umodes |= OPER_MODES; /* set::modes-on-oper */

	/* oper::vhost */
	if (operblock->vhost)
	{
		set_oper_host(sptr, operblock->vhost);
	} else
	if (IsHidden(sptr) && !sptr->user->virthost)
	{
		/* +x has just been set by modes-on-oper and no vhost. cloak the oper! */
		safe_strdup(sptr->user->virthost, sptr->user->cloakedhost);
	}

	sendto_snomask_global(SNO_OPER,
		"%s (%s@%s) [%s] is now an operator",
		sptr->name, sptr->user->username, sptr->local->sockhost,
		parv[1]);

	ircd_log(LOG_OPER, "OPER (%s) by (%s!%s@%s)", name, sptr->name, sptr->user->username,
		sptr->local->sockhost);

	/* set oper snomasks */
	if (operblock->snomask)
		set_snomask(sptr, operblock->snomask); /* oper::snomask */
	else
		set_snomask(sptr, OPER_SNOMASK); /* set::snomask-on-oper */

	/* some magic to set user mode +s (and snomask +s) if you have any snomasks set */
	if (sptr->user->snomask)
	{
		sptr->user->snomask |= SNO_SNOTICE;
		sptr->umodes |= UMODE_SERVNOTICE;
	}
	
	send_umode_out(cptr, sptr, old_umodes);
	sendnumeric(sptr, RPL_SNOMASK, get_sno_str(sptr));

	list_add(&sptr->special_node, &oper_list);

	RunHook2(HOOKTYPE_LOCAL_OPER, sptr, 1);

	sendnumeric(sptr, RPL_YOUREOPER);

	/* Update statistics */
	if (IsInvisible(sptr) && !(old_umodes & UMODE_INVISIBLE))
		ircstats.invisible++;
	if (IsOper(sptr) && !IsHideOper(sptr))
		ircstats.operators++;

	if (SHOWOPERMOTD == 1)
		(void)do_cmd(cptr, sptr, NULL, "OPERMOTD", parc, parv);

	if (!BadPtr(OPER_AUTO_JOIN_CHANS) && strcmp(OPER_AUTO_JOIN_CHANS, "0"))
	{
		char *chans[3] = {
			sptr->name,
			OPER_AUTO_JOIN_CHANS,
			NULL
		};
		if (do_cmd(cptr, sptr, NULL, "JOIN", 3, chans) == FLUSH_BUFFER)
			return FLUSH_BUFFER;
	}

	/* set::plaintext-policy::oper 'warn' */
	if (!IsSecure(sptr) && !IsLocalhost(sptr) && (iConf.plaintext_policy_oper == POLICY_WARN))
	{
		sendnotice(sptr, "%s", iConf.plaintext_policy_oper_message);
		sendto_snomask_global
		    (SNO_OPER, "OPER %s [%s] used an insecure (non-SSL/TLS) connection to /OPER.",
		    sptr->name, name);
	}

	/* set::outdated-tls-policy::oper 'warn' */
	if (IsSecure(sptr) && (iConf.outdated_tls_policy_oper == POLICY_WARN) && outdated_tls_client(sptr))
	{
		sendnotice(sptr, "%s", outdated_tls_client_build_string(iConf.outdated_tls_policy_oper_message, sptr));
		sendto_snomask_global
		    (SNO_OPER, "OPER %s [%s] used a connection with an outdated SSL/TLS protocol or cipher to /OPER.",
		    sptr->name, name);
	}

	return 0;
}
