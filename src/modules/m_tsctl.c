/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_tsctl.c
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

CMD_FUNC(m_tsctl);

/* Place includes here */
#define MSG_TSCTL       "TSCTL"

ModuleHeader MOD_HEADER(m_tsctl)
  = {
	"tsctl",	/* Name of module */
	"4.0", /* Version */
	"command /tsctl", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_tsctl)
{
	CommandAdd(modinfo->handle, MSG_TSCTL, m_tsctl, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_tsctl)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_tsctl)
{
	return MOD_SUCCESS;
}

/*
** m_tsctl - Stskeeps
**      parv[1] = command
**      parv[2] = options
*/
CMD_FUNC(m_tsctl)
{
	time_t timediff;

	if (!ValidatePermissionsForPath("server:tsctl",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (parv[1])
	{
		if (*parv[1] == '\0')
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, sptr->name, "TSCTL");
			return 0;
		}

		if (stricmp(parv[1], "offset") == 0)
		{
			if (!ValidatePermissionsForPath("server:tsctl:set",sptr,NULL,NULL,NULL))
                        {
			    sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
			    return 0;
			}

			if (!parv[2] || !parv[3])
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (*parv[2] == '\0' || *parv[3] == '\0')
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (!(*parv[2] == '+' || *parv[2] == '-'))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;

			}

			switch (*parv[2])
			{
			  case '+':
				  timediff = atol(parv[3]);
				  ircd_log(LOG_ERROR, "TSCTL: Time offset changed by %s to +%li (was %li)",
				      sptr->name, timediff, TSoffset);
				  TSoffset = timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed +%li",
				      sptr->name, timediff);
				  sendto_server(&me, 0, 0,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed +%li",
				      me.name, sptr->name, timediff);
				  break;
			  case '-':
				  timediff = atol(parv[3]);
				  ircd_log(LOG_ERROR, "TSCTL: Time offset changed by %s to -%li (was %li)",
				      sptr->name, timediff, TSoffset);
				  TSoffset = -timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed -%li",
				      sptr->name, timediff);
				  sendto_server(&me, 0, 0,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed -%li",
				      me.name, sptr->name, timediff);
				  break;
			}
			return 0;
		}
		if (stricmp(parv[1], "time") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, TStime(), time(NULL),
			    TSoffset);
			return 0;
		}
		if (stricmp(parv[1], "alltime") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Server=%s TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, me.name, TStime(), time(NULL),
			    TSoffset);
			sendto_server(cptr, 0, 0, ":%s TSCTL alltime",
			    sptr->name);
			return 0;

		}
		if (stricmp(parv[1], "svstime") == 0)
		{
			if (!IsULine(sptr))
			{
				if (MyClient(sptr)) sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
				return 0;
			}

			if (!parv[2] || *parv[2] == '\0')
			{
				if (MyClient(sptr)) sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "TSCTL");
				return 0;
			}

			timediff = atol(parv[2]);
			timediff = timediff - time(NULL);
			ircd_log(LOG_ERROR, "TSCTL: U:line %s set time to be %li (timediff: %li, was %li)",
				 sptr->name, atol(parv[2]), timediff, TSoffset);
			TSoffset = timediff;
			sendto_ops
			    ("TS Control - U:line set time to be %li (timediff: %li)",
			    atol(parv[2]), timediff);
			sendto_server(cptr, 0, 0, ":%s TSCTL svstime %li",
			    sptr->name, atol(parv[2]));
			return 0;
		}
		
		//default: no command was recognized
		sendto_one(sptr, "Invalid syntax for /TSCTL\n");
                return 0;
	}

	//default: no parameter was entered
	sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "TSCTL");

	return 0;
}


