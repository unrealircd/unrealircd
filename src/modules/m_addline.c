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

DLLFUNC int m_addline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_ADDLINE 	"ADDLINE"	
#define TOK_ADDLINE 	"z"	

ModuleHeader MOD_HEADER(m_addline)
  = {
	"m_addline",
	"$Id$",
	"command /addline", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_addline)(ModuleInfo *modinfo)
{
	add_Command(MSG_ADDLINE, TOK_ADDLINE, m_addline, 1);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_addline)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_addline)(int module_unload)
{
	if (del_Command(MSG_ADDLINE, TOK_ADDLINE, m_addline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_addline).name);
	}
	return MOD_SUCCESS;
}

/*
** m_addline (write a line to unrealircd.conf)
**
** De-Potvinized by codemastr
*/
DLLFUNC CMD_FUNC(m_addline)
{
	FILE *conf;
	char *text;
	text = parc > 1 ? parv[1] : NULL;

	if (!(IsAdmin(sptr) || IsCoAdmin(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ADDLINE");
		return 0;
	}
	/* writes to current -f */
	conf = fopen(configfile, "a");
	if (conf == NULL)
	{
		return 0;
	}
	/* Display what they wrote too */
	sendto_one(sptr, ":%s %s %s :*** Wrote (%s) to %s",
	    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", parv[0], text, configfile);
	fprintf(conf, "// Added by %s\n", make_nick_user_host(sptr->name,
	    sptr->user->username, sptr->user->realhost));
/*	for (i=1 ; i<parc ; i++)
	{
		if (i!=parc-1)
			fprintf (conf,"%s ",parv[i]);
		else
			fprintf (conf,"%s\n",parv[i]);
	}
	 * I dunno what Potvin was smoking when he made this code, but it plain SUX
	 * this should work just as good, and no need for a loop -- codemastr */
	fprintf(conf, "%s\n", text);

	fclose(conf);
	return 1;
}
