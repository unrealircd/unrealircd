/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnline.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   SVSNLINE Command
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

DLLFUNC int m_svsnline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SVSNLINE 	"SVSNLINE"	/* svsnline */
#define TOK_SVSNLINE 	"BR"	/* 127 4ever !;) */

ModuleHeader MOD_HEADER(m_svsnline)
  = {
	"svsnline",	/* Name of module */
	"$Id$", /* Version */
	"command /svsnline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_svsnline)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SVSNLINE, TOK_SVSNLINE, m_svsnline, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_svsnline)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_svsnline)(int module_unload)
{
	return MOD_SUCCESS;
}

void wipe_svsnlines(void)
{
	ConfigItem_ban *bconf, t;
	
	for (bconf = conf_ban; bconf; bconf = (ConfigItem_ban *) bconf->next)
	{
		if ((bconf->flag.type == CONF_BAN_REALNAME) &&
			(bconf->flag.type2 == CONF_BAN_TYPE_AKILL))
		{
			t.next = (ConfigItem *)DelListItem(bconf, conf_ban);
			if (bconf->mask)
				MyFree(bconf->mask);
			if (bconf->reason)
				MyFree(bconf->reason);
			MyFree(bconf);
			bconf = &t;
		}
	}
}

/*
 * m_svsnline
 * SVSNLINE + reason_where_is_space :realname mask with spaces
 * SVSNLINE - :realname mask
 * SVSNLINE *     Wipes
 * -Stskeeps
*/

DLLFUNC int m_svsnline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban *bconf;
	char		*s;

	if (!IsServer(sptr))
		return 0;

	if (parc < 2)
		return 0;

	switch (*parv[1])
	{
		  /* Allow non-U:lines to send ONLY SVSNLINE +, but don't send it out
		   * unless it is from a U:line -- codemastr */
	  case '+':
	  {
		  if (parc < 4)
			  return 0;
		 
		  if (!Find_banEx(NULL, parv[3], CONF_BAN_REALNAME, CONF_BAN_TYPE_AKILL))
		  {
			bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
			bconf->flag.type = CONF_BAN_REALNAME;
			bconf->mask = strdup(parv[3]);
			bconf->reason = strdup(parv[2]);
			for (s = bconf->reason; *s; s++)
				if (*s == '_')
					*s = ' ';
			bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
			AddListItem(bconf, conf_ban);
		  } 
		 
		  if (IsULine(sptr))
		 	sendto_serv_butone_token(cptr,
		 		sptr->name,
		 		MSG_SVSNLINE,
		 		TOK_SVSNLINE,
		 		"+ %s :%s",
			      parv[2], parv[3]);
		  break;
	  }
	  case '-':
	  {
		  if (!IsULine(sptr))
			  return 0;
		  if (parc < 3)
			  return 0;
		  
		  for (bconf = conf_ban; bconf; bconf = (ConfigItem_ban *)bconf->next)
		  {
			if (bconf->flag.type != CONF_BAN_REALNAME)
				continue;
			if (bconf->flag.type2 != CONF_BAN_TYPE_AKILL)
				continue;
			if (!stricmp(bconf->mask, parv[2]))
				break;
		  }
		  if (bconf)
		  {
		  	DelListItem(bconf, conf_ban);
		  	
		  	if (bconf->mask)
		  		MyFree(bconf->mask);
		  	if (bconf->reason)
		  		MyFree(bconf->reason);
		  	MyFree(bconf);
		  	
		  }
		  sendto_serv_butone_token(cptr,
		      sptr->name, MSG_SVSNLINE, TOK_SVSNLINE, "- %s",
		      parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsULine(sptr))
			  return 0;
	          wipe_svsnlines();
		  sendto_serv_butone_token(cptr, sptr->name,
		      MSG_SVSNLINE, TOK_SVSNLINE, "*");
		  break;
	  }

	}
	return 0;
}
