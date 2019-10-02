/*
 *   Unreal Internet Relay Chat Daemon, src/modules/sendumode.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

CMD_FUNC(cmd_sendumode);

/* Place includes here */
#define MSG_SENDUMODE   "SENDUMODE"
#define MSG_SMO         "SMO"

ModuleHeader MOD_HEADER
  = {
	"sendumode",	/* Name of module */
	"5.0", /* Version */
	"command /sendumode", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SENDUMODE, cmd_sendumode, MAXPARA, CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_SMO, cmd_sendumode, MAXPARA, CMD_SERVER);
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

/*
** cmd_sendumode - Stskeeps
**      parv[1] = target
**      parv[2] = message text
** Pretty handy proc.. 
** Servers can use this to f.x:
**   :server.unreal.net SENDUMODE o :Client connecting at server server.unreal.net port 4141 usw..
**
** Silly half-snomask support ripped out in 2015. Very confusing, and broken.
** We have SENDSNO for snomask sending since 2004. -- Syzop
*/
CMD_FUNC(cmd_sendumode)
{
	MessageTag *mtags = NULL;
	char *message;
	char *p;
	int i;
	long umode_s = 0;

	Client* acptr;

	message = (parc > 3) ? parv[3] : parv[2];

	if (parc < 3)
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "SENDUMODE");
		return 0;
	}

	new_message(sptr, recv_mtags, &mtags);

	sendto_server(sptr, 0, 0, mtags,
	    ":%s SENDUMODE %s :%s", sptr->name, parv[1], message);

	for (p = parv[1]; *p; p++)
	{
		for(i = 0; i <= Usermode_highest; i++)
		{
			if (!Usermode_Table[i].flag)
				continue;
			if (Usermode_Table[i].flag == *p)
			{
				umode_s |= Usermode_Table[i].mode;
				break;
			}
		}
	}

	list_for_each_entry(acptr, &oper_list, special_node)
	{
	    if (acptr->umodes & umode_s)
			sendto_one(acptr, mtags, ":%s NOTICE %s :%s", sptr->name, acptr->name, message);
	}

	free_message_tags(mtags);

	return 0;
}

