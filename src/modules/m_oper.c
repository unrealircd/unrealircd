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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[]);


/* Place includes here */
#define MSG_OPER        "OPER"  /* OPER */
#define TOK_OPER        ";"     /* 59 */

typedef struct oper_oflag_ {
	unsigned long oflag;
	long* umode;	/* you just HAD to make them variables */
	char** host;
	char* announce;
} oper_oflag_t;

static oper_oflag_t oper_oflags[10]; 

static void init_operflags()
{
	oper_oflags[0].oflag = OFLAG_NETADMIN;
	oper_oflags[0].umode = &UMODE_NETADMIN;
	oper_oflags[0].host = &netadmin_host;
	oper_oflags[0].announce = "is now a network administrator (N)";
	oper_oflags[1].oflag = OFLAG_SADMIN;
	oper_oflags[1].umode = &UMODE_SADMIN;
	oper_oflags[1].host = &sadmin_host;
	oper_oflags[1].announce = "is now a services administrator (a)";
	oper_oflags[2].oflag = OFLAG_ADMIN;
	oper_oflags[2].umode = &UMODE_ADMIN;
	oper_oflags[2].host = &admin_host;
	oper_oflags[2].announce = "is now a server admin (A)";
	oper_oflags[3].oflag = OFLAG_COADMIN;
	oper_oflags[3].umode = &UMODE_COADMIN;
	oper_oflags[3].host = &coadmin_host;
	oper_oflags[3].announce = "is now a co administrator (C)";
	oper_oflags[4].oflag = OFLAG_ISGLOBAL;
	oper_oflags[4].umode = &UMODE_OPER;
	oper_oflags[4].host = &oper_host;
	oper_oflags[4].announce = "is now an operator (O)";
	oper_oflags[5].oflag = OFLAG_HELPOP;
	oper_oflags[5].umode = &UMODE_HELPOP;
	oper_oflags[5].host = NULL;
	oper_oflags[5].announce = NULL;
	oper_oflags[6].oflag= OFLAG_GLOBOP;
	oper_oflags[6].umode = &UMODE_FAILOP;
	oper_oflags[6].host = NULL;
	oper_oflags[6].announce = NULL;
	oper_oflags[7].oflag = OFLAG_WALLOP;
	oper_oflags[7].umode = &UMODE_WALLOP;
	oper_oflags[7].host = NULL;
	oper_oflags[7].announce = NULL;
	oper_oflags[8].oflag = OFLAG_WHOIS;
	oper_oflags[8].umode = &UMODE_WHOIS;
	oper_oflags[8].host = NULL;
	oper_oflags[8].announce = NULL;
	oper_oflags[9].oflag = 0;
	oper_oflags[9].umode = NULL;
	oper_oflags[9].host = NULL;
	oper_oflags[9].announce = NULL;
}
	

ModuleHeader MOD_HEADER(m_oper)
  = {
	"oper",	/* Name of module */
	"$Id$", /* Version */
	"command /oper", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_oper)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_OPER, TOK_OPER, m_oper, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_oper)(int module_load)
{
	init_operflags();
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_oper)(int module_unload)
{
	if (del_Command(MSG_OPER, TOK_OPER, m_oper) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_oper).name);
	}
	return MOD_SUCCESS;
}


/*
** m_oper
**	parv[0] = sender prefix
**	parv[1] = oper name
**	parv[2] = oper password
*/

DLLFUNC int  m_oper(aClient *cptr, aClient *sptr, int parc, char *parv[]) {
	ConfigItem_oper *aconf;
	ConfigItem_oper_from *oper_from;
	char *name, *password, nuhhost[NICKLEN+USERLEN+HOSTLEN+6], nuhhost2[NICKLEN+USERLEN+HOSTLEN+6];
	char* host = 0;
	int i = 0, j = 0;
	char* announce = 0;

	if (!MyClient(sptr))
		return 0;

	if (parc < 3) {
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "OPER");
		return 0;
	}

	if (SVSNOOP) {
		sendto_one(sptr,
		    ":%s %s %s :*** This server is in NOOP mode, you cannot /oper",
		    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name);
		return 0;
	}

	if (IsAnOper(sptr)) {
		sendto_one(sptr, rpl_str(RPL_YOUREOPER),
		    me.name, parv[0]);
		return 0;
	}

	name = parv[1];
	password = parv[2];

	if (!(aconf = Find_oper(name))) {
		sendto_one(sptr, err_str(ERR_NOOPERHOST), me.name, parv[0]);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) [unknown oper]",
		    parv[0], sptr->user->username, sptr->sockhost);
		ircd_log(LOG_OPER, "OPER UNKNOWNOPER (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sptr->since += 7;
		return 0;
	}
	strlcpy(nuhhost, make_user_host(sptr->user->username, sptr->user->realhost), sizeof(nuhhost));
	strlcpy(nuhhost2, make_user_host(sptr->user->username, Inet_ia2p(&sptr->ip)), sizeof(nuhhost2));
	for (oper_from = (ConfigItem_oper_from *) aconf->from;
	    oper_from; oper_from = (ConfigItem_oper_from *) oper_from->next)
		if (!match(oper_from->name, nuhhost) || !match(oper_from->name, nuhhost2))
			break;
	if (!oper_from)	{
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
	if (i > 1)
	{
		int  old = (sptr->umodes & ALL_UMODES);

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

		if (sptr->user->operlogin)
			MyFree(sptr->user->operlogin);
		sptr->user->operlogin = strdup(aconf->name);

		/* Put in the right class */
		if (sptr->class)
			sptr->class->clients--;

		sptr->class = aconf->class;
		sptr->class->clients++;
		sptr->oflag = 0;
		if (aconf->swhois) {
			if (sptr->user->swhois)
				MyFree(sptr->user->swhois);
			sptr->user->swhois = MyMalloc(strlen(aconf->swhois) +1);
			strcpy(sptr->user->swhois, aconf->swhois);
			sendto_serv_butone_token(cptr, me.name,
				MSG_SWHOIS, TOK_SWHOIS, "%s :%s", sptr->name, aconf->swhois);
		}

/* new oper code */

		if (aconf->modes)
			sptr->umodes |= aconf->modes;
		else
			sptr->umodes |= OPER_MODES;

/* handle oflags that trigger umodes */
		
		while(oper_oflags[j].umode) {
			if(aconf->oflags & oper_oflags[j].oflag) {	/* we match this oflag */
				if (!announce && oper_oflags[j].announce) { /* we haven't matched an oper_type yet */
					host = *oper_oflags[j].host;	/* set the iNAH host */
					announce = oper_oflags[j].announce; /* set the announcement */
				}
				sptr->umodes |= 
					*oper_oflags[j].umode; /* add the umode for this oflag */
			}
			j++;
		}

		sptr->oflag = aconf->oflags;
		if ((aconf->oflags & OFLAG_HIDE) && iNAH && !BadPtr(host)) {
			char *c;
			char *vhost = host;

			if ((c = strchr(host, '@')))
			{
				vhost =	c+1;
				strncpy(sptr->user->username, host, c-host);
				sptr->user->username[c-host] = 0;
				sendto_serv_butone_token(NULL, sptr->name, MSG_SETIDENT, 
							 TOK_SETIDENT, "%s", 
							 sptr->user->username);
			}
			iNAH_host(sptr, vhost);
			SetHidden(sptr);
		} else
		if (IsHidden(sptr) && !sptr->user->virthost) {
			/* +x has just been set by modes-on-oper and iNAH is off */
			sptr->user->virthost = strdup(sptr->user->cloakedhost);
		}

		if (!IsOper(sptr))
		{
			sptr->umodes |= UMODE_LOCOP;
			if ((aconf->oflags & OFLAG_HIDE) && iNAH && !BadPtr(locop_host)) {
				iNAH_host(sptr, locop_host);
				SetHidden(sptr);
			}
			sendto_ops("%s (%s@%s) is now a local operator (o)",
			    parv[0], sptr->user->username, GetHost(sptr));
		}


		if (announce != NULL)
			sendto_snomask_global(SNO_OPER,
			    "%s (%s@%s) [%s] %s",
			    parv[0], sptr->user->username, GetHost(sptr),
			    parv[1], announce);
		if (aconf->snomask)
			set_snomask(sptr, aconf->snomask);
		else
			set_snomask(sptr, OPER_SNOMASK);
		if (sptr->user->snomask)
		{
			sptr->user->snomask |= SNO_SNOTICE; /* set +s if needed */
			sptr->umodes |= UMODE_SERVNOTICE;
		}
		/* This is for users who have both 'admin' and 'coadmin' in their conf */
		if (IsCoAdmin(sptr) && IsAdmin(sptr))
		{
			sptr->umodes &= ~UMODE_COADMIN;
			sptr->oflag &= ~OFLAG_COADMIN;
		}
		send_umode_out(cptr, sptr, old);
		sendto_one(sptr, rpl_str(RPL_SNOMASK),
			me.name, parv[0], get_sno_str(sptr));

#ifndef NEW_IO
		addto_fdlist(sptr->slot, &oper_fdlist);
#else /* ifndef NEW_IO */
#endif /* ifndef NEW_IO */
		RunHook2(HOOKTYPE_LOCAL_OPER, sptr, 1);
		sendto_one(sptr, rpl_str(RPL_YOUREOPER), me.name, parv[0]);
		if (IsInvisible(sptr) && !(old & UMODE_INVISIBLE))
			IRCstats.invisible++;
		if (IsOper(sptr) && !IsHideOper(sptr))
			IRCstats.operators++;

		if (SHOWOPERMOTD == 1)
			do_cmd(cptr, sptr, "OPERMOTD", parc, parv);
		if (!BadPtr(OPER_AUTO_JOIN_CHANS)
		    && strcmp(OPER_AUTO_JOIN_CHANS, "0"))
		{
			char *chans[3] = {
				sptr->name,
				OPER_AUTO_JOIN_CHANS,
				NULL
			};
			if (do_cmd(cptr, sptr, "JOIN", 3, chans) == FLUSH_BUFFER)
				return FLUSH_BUFFER;
		}
		ircd_log(LOG_OPER, "OPER (%s) by (%s!%s@%s)", name, parv[0], sptr->user->username,
			sptr->sockhost);

	}
	if (i == -1)
	{
		sendto_one(sptr, err_str(ERR_PASSWDMISMATCH), me.name, parv[0]);
		if (FAILOPER_WARN)
			sendto_one(sptr,
			    ":%s %s %s :*** Your attempt has been logged.", me.name,
			    IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name);
		ircd_log(LOG_OPER, "OPER FAILEDAUTH (%s) by (%s!%s@%s)", name, parv[0],
			sptr->user->username, sptr->sockhost);
		sendto_snomask_global
		    (SNO_OPER, "Failed OPER attempt by %s (%s@%s) using UID %s [FAILEDAUTH]",
		    parv[0], sptr->user->username, sptr->sockhost, name);
		sptr->since += 7;
	}
	/* Belay that order, number One. (-2) */
	return 0;
}
