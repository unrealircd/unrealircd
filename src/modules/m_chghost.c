/*
 *   IRC - Internet Relay Chat, src/modules/m_chghost.c
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

#define MSG_CHGHOST 	"CHGHOST"
#define TOK_CHGHOST 	"AL"

DLLFUNC int m_chghost(aClient *cptr, aClient *sptr, int parc, char *parv[]);
#ifndef DYNAMIC_LINKING
ModuleInfo m_chghost_info
#else
#define m_chghost_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"chghost",	/* Name of module */
	"$Id$", /* Version */
	"/chghost", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_chghost_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CHGHOST, TOK_CHGHOST, m_chghost, MAXPARA);
}
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_chghost_load(int module_load)
#endif
{
}
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_chghost_unload(void)
#endif
{
	if (del_Command(MSG_CHGHOST, TOK_CHGHOST, m_chghost) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_chghost_info.name);
	}
}

/* 
 * m_chghost - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new hostname>
 * parv[0] - sender
 * parv[1] - nickname
 * parv[2] - hostname
 *
*/

DLLFUNC int m_chghost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char *s;
	int  legalhost = 1;

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, ":%s NOTICE %s :*** The /chghost command is disabled on this server", me.name, sptr->name);
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
		    ":%s NOTICE %s :*** /ChgHost syntax is /ChgHost <nick> <newhost>",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) < 1)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Write atleast something to change the host to!",
		    me.name, sptr->name);
		return 0;
	}

	if (strlen(parv[2]) > (HOSTLEN))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** ChgHost Error: Too long hostname!!",
		    me.name, sptr->name);
		return 0;
	}

	/* illegal?! */
	for (s = parv[2]; *s; s++)
	{
		if (!isallowed(*s) && !(*s == ':'))
		{
			legalhost = 0;
		}
	}

	if (legalhost == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the virtual hostname of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    acptr->user->realhost, parv[2]);
		}
		acptr->umodes |= UMODE_HIDE;
		acptr->umodes |= UMODE_SETHOST;
		sendto_serv_butone_token(cptr, sptr->name,
		    MSG_CHGHOST, TOK_CHGHOST, "%s %s", acptr->name, parv[2]);
		if (acptr->user->virthost)
			MyFree(acptr->user->virthost);
		acptr->user->virthost = MyMalloc(strlen(parv[2]) + 1);
		ircsprintf(acptr->user->virthost, "%s", parv[2]);
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
