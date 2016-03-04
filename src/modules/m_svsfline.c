/*
 *   IRC - Internet Relay Chat, src/modules/m_svsfline.c
 *   (C) 2004-present The UnrealIRCd Team
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

#include "unrealircd.h"

CMD_FUNC(m_svsfline);

#define MSG_SVSFLINE 	"SVSFLINE"	

ModuleHeader MOD_HEADER(m_svsfline)
  = {
	"m_svsfline",
	"4.0",
	"command /svsfline", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_svsfline)
{
	CommandAdd(modinfo->handle, MSG_SVSFLINE, m_svsfline, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svsfline)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svsfline)
{
	return MOD_SUCCESS;
}

CMD_FUNC(m_svsfline)
{
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
			  DCCdeny_add(parv[2], parv[3], DCCDENY_HARD, CONF_BAN_TYPE_AKILL);
		  if (IsULine(sptr))
			  sendto_server(cptr, 0, 0, ":%s SVSFLINE + %s :%s",
			      sptr->name, parv[2], parv[3]);
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
		  sendto_server(cptr, 0, 0, ":%s SVSFLINE %s", sptr->name, parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsULine(sptr))
			  return 0;
		  dcc_wipe_services();
		  sendto_server(cptr, 0, 0, ":%s SVSFLINE *", sptr->name);
		  break;
	  }

	}
	return 0;
}
