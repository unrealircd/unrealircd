/*
 *   IRC - Internet Relay Chat, src/modules/m_chgident.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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
/**
 * 2003-01-06
 * - Added ability to log command use to ircd.log
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

#define MSG_CHGIDENT 	"CHGIDENT"
#define TOK_CHGIDENT 	"AZ"

DLLFUNC int m_chgident(aClient *cptr, aClient *sptr, int parc, char *parv[]);


ModuleHeader MOD_HEADER(m_chgident)
  = {
	"chgident",	/* Name of module */
	"$Id$", /* Version */
	"/chgident", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_chgident)(ModuleInfo *modinfo)
{
	/* extern variable to export m_chgident_info to temporary
           ModuleHeader *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CHGIDENT, TOK_CHGIDENT, m_chgident, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_chgident)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_chgident)(int module_unload)
{
	if (del_Command(MSG_CHGIDENT, TOK_CHGIDENT, m_chgident) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_chgident).name);
	}
	return MOD_SUCCESS;
}

/* 
 * m_chgident - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGIDENT <nick> <new identname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - identname
 *
*/

int m_chgident(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char *s;
	int  legalident = 1;

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :*** The /chgident command is disabled on this server", me.name, sptr->name);
		return 0;
	}
#endif

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;

		}

	if (parc < 3)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgIdent syntax is /ChgIdent <nick> <newident>",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Write atleast something to change the ident to!",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) > (USERLEN))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** ChgIdent Error: Too long ident!!",
		    me.name, sptr->name);
		return 0;
	}

	/* illegal?! */
	for (s = parv[2]; *s; s++)
	{
                if ((*s == '~') && (s == parv[2]))
                        continue;
		if (!isallowed(*s))
		{
			legalident = 0;
		}
	}

	if (legalident == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgIdent Error: A ident may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /ChgIdent is disabled", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (IsPerson(acptr) && MyClient(sptr) && acptr->user->joined)
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /ChgIdent can not be used while %s is on a channel", me.name, sptr->name, acptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				rejoin_doparts(acptr);
				/* join sent later when the ident has been changed */
				break;
		}
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the virtual ident of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    GetHost(acptr), parv[2]);
		}

		/* Logging ability added by XeRXeS */
		ircd_log(LOG_CHGCMDS,
		"CHGIDENT: %s changed the virtual ident of %s (%s@%s) to be %s",
		sptr->name, acptr->name, acptr->user->username,    
		GetHost(acptr), parv[2]);


		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGIDENT,
		    TOK_CHGIDENT, "%s %s", acptr->name, parv[2]);
		ircsprintf(acptr->user->username, "%s", parv[2]);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
			rejoin_dojoinandmode(acptr);
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
