/*
 *   IRC - Internet Relay Chat, src/modules/svsnline.c
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

CMD_FUNC(cmd_svsnline);

#define MSG_SVSNLINE 	"SVSNLINE"	/* svsnline */

ModuleHeader MOD_HEADER
  = {
	"svsnline",	/* Name of module */
	"5.0", /* Version */
	"command /svsnline", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSNLINE, cmd_svsnline, MAXPARA, CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

void wipe_svsnlines(void)
{
	ConfigItem_ban *bconf, *next;
	
	for (bconf = conf_ban; bconf; bconf = next)
	{
		next = bconf->next;
		if ((bconf->flag.type == CONF_BAN_REALNAME) &&
			(bconf->flag.type2 == CONF_BAN_TYPE_AKILL))
		{
			DelListItem(bconf, conf_ban);
			safe_free(bconf->mask);
			safe_free(bconf->reason);
			safe_free(bconf);
		}
	}
}

/*
 * cmd_svsnline
 * SVSNLINE + reason_where_is_space :realname mask with spaces
 * SVSNLINE - :realname mask
 * SVSNLINE *     Wipes
 * -Stskeeps
*/
CMD_FUNC(cmd_svsnline)
{
	ConfigItem_ban *bconf;
	char		*s;

	if (parc < 2)
		return;

	switch (*parv[1])
	{
		  /* Allow non-U-Lines to send ONLY SVSNLINE +, but don't send it out
		   * unless it is from a U-Line -- codemastr */
	  case '+':
	  {
		  if (parc < 4)
			  return;
		 
		  if (!find_banEx(NULL, parv[3], CONF_BAN_REALNAME, CONF_BAN_TYPE_AKILL))
		  {
			bconf = safe_alloc(sizeof(ConfigItem_ban));
			bconf->flag.type = CONF_BAN_REALNAME;
			safe_strdup(bconf->mask, parv[3]);
			safe_strdup(bconf->reason, parv[2]);
			for (s = bconf->reason; *s; s++)
				if (*s == '_')
					*s = ' ';
			bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
			AddListItem(bconf, conf_ban);
		  } 
		 
		  if (IsSvsCmdOk(client))
			sendto_server(client, 0, 0, NULL, ":%s SVSNLINE + %s :%s",
			    client->id, parv[2], parv[3]);
		  break;
	  }
	  case '-':
	  {
		  if (!IsSvsCmdOk(client))
			  return;
		  if (parc < 3)
			  return;
		  
		  for (bconf = conf_ban; bconf; bconf = bconf->next)
		  {
			if (bconf->flag.type != CONF_BAN_REALNAME)
				continue;
			if (bconf->flag.type2 != CONF_BAN_TYPE_AKILL)
				continue;
			if (!strcasecmp(bconf->mask, parv[2]))
				break;
		  }
		  if (bconf)
		  {
		  	DelListItem(bconf, conf_ban);
	  		safe_free(bconf->mask);
	  		safe_free(bconf->reason);
		  	safe_free(bconf);
		  	
		  }
		  sendto_server(client, 0, 0, NULL, ":%s SVSNLINE - %s", client->id, parv[2]);
		  break;
	  }
	  case '*':
	  {
		  if (!IsSvsCmdOk(client))
			  return;
	          wipe_svsnlines();
		  sendto_server(client, 0, 0, NULL, ":%s SVSNLINE *", client->id);
		  break;
	  }

	}
}
