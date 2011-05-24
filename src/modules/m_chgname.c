/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_chgname.c
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
/**
 * 2003-01-06
 * - Added ability to log command usage to ircd.log
 * XeRXeS
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_chgname(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_CHGNAME     "CHGNAME"
#define TOK_CHGNAME     "BK"


ModuleHeader MOD_HEADER(m_chgname)
  = {
	"chgname",	/* Name of module */
	"$Id$", /* Version */
	"command /chgname", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_chgname)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CHGNAME, TOK_CHGNAME, m_chgname, 2);
	add_Command(MSG_SVSNAME, NULL, m_chgname, 2);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_chgname)(int module_load)
{
	return MOD_SUCCESS;
	
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_chgname)(int module_unload)
{
	if (del_Command(MSG_CHGNAME, TOK_CHGNAME, m_chgname) < 0)
	{
		sendto_realops("Failed to delete command chgname when unloading %s",
				MOD_HEADER(m_chgname).name);
	}
	if (del_Command(MSG_SVSNAME, NULL, m_chgname) < 0)
	{
		sendto_realops("Failed to delete command svsname when unloading %s",
				MOD_HEADER(m_chgname).name);
	}
	return MOD_SUCCESS;
	
}


/* 
 * m_chgname - Tue May 23 13:06:35 BST 200 (almost a year after I made CHGIDENT) - Stskeeps
 * :prefix CHGNAME <nick> <new realname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - realname
 *
*/

DLLFUNC int m_chgname(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (MyClient(sptr) && !IsAnOper(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
		    parv[0]);
		return 0;
	}

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, err_str(ERR_DISABLED), me.name, sptr->name, "CHGNAME",
			"This command is disabled on this server");
		return 0;
	}
#endif

	if ((parc < 3) || !*parv[2])
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "CHGNAME");
		return 0;
	}

	if (strlen(parv[2]) > (REALLEN))
	{
		sendnotice(sptr, "*** ChgName Error: Requested realname too long -- rejected.");
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (MyClient(sptr) && (IsLocOp(sptr) && !MyClient(acptr)))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
				parv[0]);
			return 0;
		}

		/* set the realname first to make n:line checking work */
		ircsprintf(acptr->info, "%s", parv[2]);
		/* only check for n:lines if the person who's name is being changed is not an oper */
		if (!IsAnOper(acptr) && Find_ban(NULL, acptr->info, CONF_BAN_REALNAME)) {
			int xx;
			xx =
			   exit_client(cptr, sptr, &me,
			   "Your GECOS (real name) is banned from this server");
			return xx;
		}
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the GECOS of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    GetHost(acptr), parv[2]);
			/* Logging ability added by XeRXeS */
			ircd_log(LOG_CHGCMDS,
				"CHGNAME: %s changed the GECOS of %s (%s@%s) to be %s", 
				sptr->name, acptr->name, acptr->user->username,
				GetHost(acptr), parv[2]);
		}


		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGNAME, TOK_CHGNAME, "%s :%s", acptr->name, parv[2]);
		return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name,
		    parv[1]);
		return 0;
	}
	return 0;
}

