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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

#define MSG_CHGHOST 	"CHGHOST"
#define TOK_CHGHOST 	"AL"

DLLFUNC int m_chghost(aClient *cptr, aClient *sptr, int parc, char *parv[]);

ModuleHeader MOD_HEADER(m_chghost)
  = {
	"chghost",	/* Name of module */
	"$Id$", /* Version */
	"/chghost", /* Short description of module */
	"3.2-b8-1",
    };

DLLFUNC int MOD_INIT(m_chghost)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_CHGHOST, TOK_CHGHOST, m_chghost, MAXPARA);
	ModuleSetOptions(modinfo->handle, MOD_OPT_OFFICIAL);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_chghost)(int module_load)
{
	return MOD_SUCCESS;
	
}

DLLFUNC int MOD_UNLOAD(m_chghost)(int module_unload)
{
	if (del_Command(MSG_CHGHOST, TOK_CHGHOST, m_chghost) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_chghost).name);
	}
	return MOD_SUCCESS;
	
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

	if (!valid_host(parv[2]))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	if (parv[2][0] == ':')
	{
		sendto_one(sptr, ":%s NOTICE %s :*** A hostname cannot start with ':'", me.name, sptr->name);
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (!strcmp(GetHost(acptr), parv[2]))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /ChgHost Error: requested host is same as current host.",
			    me.name, parv[0]);
			return 0;
		}
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /ChgHost is disabled", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (IsPerson(acptr) && MyClient(sptr) && acptr->user->joined)
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /ChgHost can not be used while %s is on a channel", me.name, sptr->name, acptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				rejoin_doparts(acptr);
				/* join sent later when the host has been changed */
				break;
		}
				
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
		{
			MyFree(acptr->user->virthost);
			acptr->user->virthost = 0;
		}
		acptr->user->virthost = strdup(parv[2]);
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
