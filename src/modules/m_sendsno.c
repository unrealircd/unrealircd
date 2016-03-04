/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_sendsno.c
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

CMD_FUNC(m_sendsno);

#define MSG_SENDSNO   "SENDSNO"

ModuleHeader MOD_HEADER(m_sendsno)
  = {
	"sendsno",	/* Name of module */
	"4.0", /* Version */
	"command /sendsno", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_sendsno)
{
	CommandAdd(modinfo->handle, MSG_SENDSNO, m_sendsno, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_sendsno)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_sendsno)
{
	return MOD_SUCCESS;
}

/*
** m_sendsno - Written by Syzop, bit based on SENDUMODE from Stskeeps
**      parv[1] = target snomask
**      parv[2] = message text
** Servers can use this to:
**   :server.unreal.net SENDSNO e :Hiiiii
*/
CMD_FUNC(m_sendsno)
{
char *sno, *msg, *p;
long snomask = 0;
int i;
aClient *acptr;

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "SENDSNO");
		return 0;
	}
	sno = parv[1];
	msg = parv[2];

	/* Forward to others... */
	sendto_server(cptr, 0, 0, ":%s SENDSNO %s :%s", sptr->name, parv[1], parv[2]);

	for (p = sno; *p; p++)
	{
		for(i = 0; i <= Snomask_highest; i++)
		{
			if (Snomask_Table[i].flag == *p)
			{
				snomask |= Snomask_Table[i].mode;
				break;
			}
		}
	}

	list_for_each_entry(acptr, &oper_list, special_node)
	{
		if (acptr->user->snomask & snomask)
			sendto_one(acptr, ":%s NOTICE %s :%s", sptr->name, acptr->name, msg);
	}

	return 0;
}

