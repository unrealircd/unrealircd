/*
 *   IRC - Internet Relay Chat, src/modules/m_sethost.c
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

DLLFUNC int m_sethost(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SETHOST 	"SETHOST"	/* sethost */
#define TOK_SETHOST 	"AA"	/* 127 4ever !;) */
#ifndef DYNAMIC_LINKING
ModuleHeader m_sethost_Header
#define Mod_Header m_sethost_Header
#else
ModuleHeader Mod_Header
#endif
  = {
	"sethost",	/* Name of module */
	"$Id$", /* Version */
	"command /sethost", /* Short description of module */
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
int    m_sethost_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SETHOST, TOK_SETHOST, m_sethost, MAXPARA);
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    m_sethost_Load(int module_load)
#endif
{
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	m_sethost_Unload(int module_unload)
#endif
{
	if (del_Command(MSG_SETHOST, TOK_SETHOST, m_sethost) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				Mod_Header.name);
	}
}

/*
   m_sethost() added by Stskeeps (30/04/1999)
               (modified at 15/05/1999) by Stskeeps | Potvin
   :prefix SETHOST newhost
   parv[0] - sender
   parv[1] - newhost
*/
DLLFUNC int m_sethost(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *vhost, *s;
#ifndef DISABLE_USERMOD
	int  permit = 0;	/* 0 = opers(glob/locop) 1 = global oper 2 = not MY clients.. */
#else
	int  permit = 2;
#endif
	int  legalhost = 1;	/* is legal characters? */


	if (!MyConnect(sptr))
		goto have_permit1;
	switch (permit)
	{
	  case 0:
		  if (!IsAnOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 1:
		  if (!IsOper(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
		  break;
	  case 2:
		  if (MyConnect(sptr))
		  {
			  sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			      parv[0]);
			  return 0;
		  }
	  default:
		  sendto_ops_butone(IsServer(cptr) ? cptr : NULL, sptr,
		      ":%s WALLOPS :[SETHOST] Somebody fixing this corrupted server? !(0|1) !!!",
		      me.name);
		  break;
	}

      have_permit1:
	if (parc < 2)
		vhost = NULL;
	else
		vhost = parv[1];

	/* bad bad bad boys .. ;p */
	if (vhost == NULL)
	{	
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Syntax: /SetHost <new host>",
			    me.name, parv[0]);
		}
		return 0;
	}
	/* uh uh .. too small */
	if (strlen(parv[1]) < 1)
	{
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Atleast write SOMETHING that makes sense (':' string)",
			    me.name, sptr->name);
		return 0;
	}
	/* too large huh? */
	if (strlen(parv[1]) > (HOSTLEN))
	{
		/* ignore us as well if we're not a child of 3k */
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Hostnames are limited to %i characters.",
			    me.name, sptr->name, HOSTLEN);
		return 0;
	}

	/* illegal?! */
	for (s = vhost; *s; s++)
	{
		if (!isallowed(*s) && !(*s == ':'))
		{
			legalhost = 0;
		}
	}

	if (legalhost == 0)
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /SetHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, parv[0]);
		return 0;
	}

	/* hide it */
	sptr->umodes |= UMODE_HIDE;
	sptr->umodes |= UMODE_SETHOST;
	/* get it in */
	if (sptr->user->virthost)
		MyFree(sptr->user->virthost);
	sptr->user->virthost = MyMalloc(strlen(vhost) + 1);
	ircsprintf(sptr->user->virthost, "%s", vhost);
	/* spread it out */
	sendto_serv_butone_token(cptr, sptr->name, MSG_SETHOST, TOK_SETHOST,
	    "%s", parv[1]);

	if (MyConnect(sptr))
	{
		sendto_one(sptr, ":%s MODE %s :+xt", sptr->name, sptr->name);
		sendto_one(sptr,
		    ":%s NOTICE %s :Your nick!user@host-mask is now (%s!%s@%s) - To disable it type /mode %s -x",
		    me.name, parv[0], parv[0], sptr->user->username, vhost,
		    parv[0]);
	}
	return 0;
}
