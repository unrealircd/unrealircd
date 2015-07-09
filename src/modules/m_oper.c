/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_oper.c
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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
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
#include "proto.h"
#include "inet.h"
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[]);


/* Place includes here */
#define MSG_OPER        "OPER"  /* OPER */

ModuleHeader MOD_HEADER(m_oper)
  = {
	"oper",	/* Name of module */
	"$Id$", /* Version */
	"command /oper", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_oper)
{
	CommandAdd(modinfo->handle, MSG_OPER, m_oper, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_oper)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_oper)
{
	return MOD_SUCCESS;
}

void set_oper_host(aClient *sptr, char *host)
{
        char uhost[HOSTLEN + USERLEN + 1];
        char *p;
        
        strlcpy(uhost, host, sizeof(uhost));
        
	if ((p = strchr(uhost, '@')))
	{
	        *p++ = '\0';
		strlcpy(sptr->user->username, uhost, sizeof(sptr->user->username));
		sendto_server(NULL, 0, 0, ":%s SETIDENT %s",
		    sptr->name, sptr->user->username);
	        host = p;
	}
	iNAH_host(sptr, host);
	SetHidden(sptr);
}

/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/

DLLFUNC int  m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_oper *aconf;
	char *name, *password;
	int i = 0, j = 0;
	long old; /* old user modes */

	if (!MyClient(sptr))
		return 0;

	if (parc < 2) {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "OPER");
		return 0;
	}

	if (SVSNOOP) {
		sendnotice(sptr,
		    "*** This server is in NOOP mode, you cannot /oper");
		return 0;
	}

	if (IsOper(sptr)) {
		sendto_one(sptr, rpl_str(RPL_YOUREOPER),
		    me.name, parv[0]);
		// TODO: de-confuse this ? ;)
		return 0;
	}

	name = parv[1];
	password = (parc >= 2) ? parv[2] : "";

	if (!(aconf = Find_oper(name)))
	{
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [unknown oper]",
		    parv[0], sptr->user->username, sptr->sockhost);
		ircd_log(LOG_OPER, "OPER UNKNOWNOPER (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sptr->since += 7;
		return 0;
	}

	if (!unreal_mask_match(sptr, aconf->mask))
	{
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [host doesnt match]",
		    parv[0], sptr->user->username, sptr->sockhost, name);
		ircd_log(LOG_OPER, "OPER NOHOSTMATCH (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sptr->since += 7;
		return 0;
	}

	i = Auth_Check(cptr, aconf->auth, password);
	if (i == -1)
	{
		sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
		if (FAILOPER_WARN)
			sendnotice(sptr,
			    "*** Your attempt has been logged.");
		ircd_log(LOG_OPER, "OPER FAILEDAUTH (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [FAILEDAUTH]",
		    parv[0], sptr->user->username, sptr->sockhost, name);
		sptr->since += 7;
		return 0;
	}
	
	if (i < 2)
		return 0; /* anything below 2 means 'not really authenticated' */

	/* Authentication of the oper succeeded (like, password, ssl cert),
	 * but we still have some other restrictions to check below as well,
	 * like 'require-modes' and 'maxlogins'...
	 */

	/* Check oper::require_modes */
	if (aconf->require_modes & ~sptr->umodes)
	{
		sendto_one(sptr, ":%s %d %s :You are missing user modes required to OPER", me.name, ERR_NOOPERHOST, parv[0]);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) [lacking modes '%s' in oper::require-modes]",
			 parv[0], sptr->user->username, sptr->sockhost, get_modestr(aconf->require_modes & ~sptr->umodes));
		ircd_log(LOG_OPER, "OPER MISSINGMODES (%s) by (%s!%s@%s), needs modes=%s",
			 name, parv[0], sptr->user->username, sptr->sockhost,
			 get_modestr(aconf->require_modes & ~sptr->umodes));
		sptr->since += 7;
		return 0;
	}

	if (aconf->maxlogins && (count_oper_sessions(aconf->name) >= aconf->maxlogins))
	{
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
		sendto_one(sptr, ":%s NOTICE %s :Your maximum number of concurrent oper logins has been reached (%d)",
			me.name, sptr->name, aconf->maxlogins);
		sendto_snomask_global
			(SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [maxlogins reached]",
			parv[0], sptr->user->username, sptr->sockhost, name);
		ircd_log(LOG_OPER, "OPER TOOMANYLOGINS (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sptr->since += 4;
		return 0;
	}

	/* /OPER really succeeded now. Start processing it. */

	/* Store which oper block was used to become IRCOp (for maxlogins and whois) */
	safefree(sptr->user->operlogin);
	sptr->user->operlogin = strdup(aconf->name);

	/* Put in the right class */
	if (sptr->class)
		sptr->class->clients--;
	sptr->class = aconf->class;
	sptr->class->clients++;

	/* oper::swhois */
	if (aconf->swhois)
	{
		swhois_add(sptr, "oper", -100, aconf->swhois, &me, NULL);
	}

	/* set oper user modes */
	sptr->umodes |= UMODE_OPER;
	if (aconf->modes)
		sptr->umodes |= aconf->modes; /* oper::modes */
	else
		sptr->umodes |= OPER_MODES; /* set::modes-on-oper */

	/* oper::vhost */
	if (aconf->vhost)
	{
		set_oper_host(sptr, aconf->vhost);
	} else
	if (IsHidden(sptr) && !sptr->user->virthost)
	{
		/* +x has just been set by modes-on-oper and no vhost. cloak the oper! */
		sptr->user->virthost = strdup(sptr->user->cloakedhost);
	}

	sendto_snomask_global(SNO_OPER,
		"%s (%s@%s) [%s] is now an operator",
		parv[0], sptr->user->username, sptr->sockhost,
		parv[1]);

	ircd_log(LOG_OPER, "OPER (%s) by (%s!%s@%s)", name, parv[0], sptr->user->username,
		sptr->sockhost);

	/* set oper snomasks */
	if (aconf->snomask)
		set_snomask(sptr, aconf->snomask); /* oper::snomask */
	else
		set_snomask(sptr, OPER_SNOMASK); /* set::snomask-on-oper */

	/* some magic to set user mode +s (and snomask +s) if you have any snomasks set */
	if (sptr->user->snomask)
	{
		sptr->user->snomask |= SNO_SNOTICE;
		sptr->umodes |= UMODE_SERVNOTICE;
	}
	
	old = (sptr->umodes & ALL_UMODES);
	send_umode_out(cptr, sptr, old);
	sendto_one(sptr, rpl_str(RPL_SNOMASK),
		me.name, parv[0], get_sno_str(sptr));

	list_add(&sptr->special_node, &oper_list);

	RunHook2(HOOKTYPE_LOCAL_OPER, sptr, 1);

	sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);

	/* Update statistics */
	if (IsInvisible(sptr) && !(old & UMODE_INVISIBLE))
		IRCstats.invisible++;
	if (IsOper(sptr) && !IsHideOper(sptr))
		IRCstats.operators++;

	if (SHOWOPERMOTD == 1)
		do_cmd(cptr, sptr, "OPERMOTD", parc, parv);

	if (!BadPtr(OPER_AUTO_JOIN_CHANS) && strcmp(OPER_AUTO_JOIN_CHANS, "0"))
	{
		char *chans[3] = {
			sptr->name,
			OPER_AUTO_JOIN_CHANS,
			NULL
		};
		if (do_cmd(cptr, sptr, "JOIN", 3, chans) == FLUSH_BUFFER)
			return FLUSH_BUFFER;
	}

	return 0;
}
