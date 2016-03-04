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

#include "unrealircd.h"

CMD_FUNC(m_svsnline);

#define MSG_SVSNLINE 	"SVSNLINE"	/* svsnline */

ModuleHeader MOD_HEADER(m_svsnline)
  = {
	"svsnline",	/* Name of module */
	"4.0", /* Version */
	"command /svsnline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_svsnline)
{
	CommandAdd(modinfo->handle, MSG_SVSNLINE, m_svsnline, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_svsnline)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_svsnline)
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
CMD_FUNC(m_svsnline)
{
	ConfigItem_ban *bconf;
	char		*s;

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
			sendto_server(cptr, 0, 0, ":%s SVSNLINE + %s :%s",
			    sptr->name, parv[2], parv[3]);
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
		  sendto_server(cptr, 0, 0, ":%s SVSNLINE - %s", sptr->name, parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsULine(sptr))
			  return 0;
	          wipe_svsnlines();
		  sendto_server(cptr, 0, 0, ":%s SVSNLINE *", sptr->name);
		  break;
	  }

	}
	return 0;
}
