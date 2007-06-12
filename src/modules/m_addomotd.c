/*
 *   IRC - Internet Relay Chat, src/modules/out.c
 *   (C) 2004 The UnrealIRCd Team
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
#include "proto.h"
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

DLLFUNC int m_addomotd(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_ADDOMOTD 	"ADDOMOTD"	
#define TOK_ADDOMOTD 	"AR"	

ModuleHeader MOD_HEADER(m_addomotd)
  = {
	"m_addomotd",
	"$Id$",
	"command /addomotd", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_addomotd)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_ADDOMOTD, TOK_ADDOMOTD, m_addomotd, 1, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_addomotd)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_addomotd)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_addomotd (write a line to opermotd)
**
** De-Potvinized by codemastr
*/
DLLFUNC CMD_FUNC(m_addomotd)
{
	FILE *conf;
	char *text;

	text = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(sptr))
		return 0;

	if (!(IsAdmin(sptr) || IsCoAdmin(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADDOMOTD");
		return 0;
	}
	conf = fopen(OPATH, "a");
	if (conf == NULL)
	{
		return 0;
	}
	sendto_one(sptr, ":%s %s %s :*** Wrote (%s) to OperMotd",
	    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], text);
	/*      for (i=1 ; i<parc ; i++)
	   {
	   if (i!=parc-1)
	   fprintf (conf,"%s ",parv[i]);
	   else
	   fprintf (conf,"%s\n",parv[i]);
	   } */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}
