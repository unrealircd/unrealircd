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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

#define MSG_CHGIDENT 	"CHGIDENT"
#define TOK_CHGIDENT 	"AZ"

DLLFUNC int m_chgident(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#ifndef DYNAMIC_LINKING
ModuleHeader m_chgident_Header
#else
#define m_chgident_Header Mod_Header
ModuleHeader Mod_Header
#endif
  = {
	"chgident",	/* Name of module */
	"$Id$", /* Version */
	"/chgident", /* Short description of module */
	"3.2-b5",
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int   m_chgident_Init(int module_load)
#endif
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
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int   m_chgident_Load(int module_load)
#endif
{
	return MOD_SUCCESS;
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_chgident_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_CHGIDENT, TOK_CHGIDENT, m_chgident) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_chgident_Header.name);
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
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the virtual ident of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    (acptr->umodes & UMODE_HIDE ? acptr->
			    user->realhost : acptr->user->realhost), parv[2]);
		}
		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGIDENT,
		    TOK_CHGIDENT, "%s %s", acptr->name, parv[2]);
		ircsprintf(acptr->user->username, "%s", parv[2]);
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
