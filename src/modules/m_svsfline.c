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

DLLFUNC int m_svsfline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSFLINE 	"SVSFLINE"	
#define TOK_SVSFLINE 	"BC"	

ModuleHeader MOD_HEADER(m_svsfline)
  = {
	"m_svsfline",
	"$Id$",
	"command /svsfline", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_svsfline)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSFLINE, TOK_SVSFLINE, m_svsfline, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsfline)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsfline)(int module_unload)
{
	if (del_Command(MSG_SVSFLINE, TOK_SVSFLINE, m_svsfline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_svsfline).name);
	}
	return MOD_SUCCESS;
}

DLLFUNC CMD_FUNC(m_svsfline)
{
	if (!IsServer(sptr))
		return 0;

	if (parc < 2)
		return 0;

	switch (*parv[1])
	{
		  /* Allow non-U:lines to send ONLY SVSFLINE +, but don't send it out
		   * unless it is from a U:line -- codemastr */
	  case '+':
	  {
		  if (parc < 4)
			  return 0;
		  if (!Find_deny_dcc(parv[2]))
			  DCCdeny_add(parv[2], parv[3], CONF_BAN_TYPE_AKILL);
		  if (IsULine(sptr))
			  sendto_serv_butone_token(cptr,
			      sptr->name,
			      MSG_SVSFLINE, TOK_SVSFLINE,
			      "+ %s :%s",
			      parv[2], parv[3]);
		  break;
	  }
	  case '-':
	  {
		  ConfigItem_deny_dcc *deny;
		  if (!IsULine(sptr))
			  return 0;
		  if (parc < 3)
			  return 0;
		  if (!(deny = Find_deny_dcc(parv[2])))
			break;
		  DCCdeny_del(deny);
		  sendto_serv_butone_token(cptr, sptr->name,
		 	MSG_SVSFLINE, TOK_SVSFLINE, "%s",
			      parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsULine(sptr))
			  return 0;
		  dcc_wipe_services();
		  sendto_serv_butone_token(cptr, sptr->name,
		      MSG_SVSFLINE, TOK_SVSFLINE,
		      	"*");
		  break;
	  }

	}
	return 0;
}
