/*
 *   IRC - Internet Relay Chat, src/modules/m_sethost.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

CMD_FUNC(m_sethost);

/* Place includes here */
#define MSG_SETHOST 	"SETHOST"	/* sethost */

ModuleHeader MOD_HEADER(m_sethost)
  = {
	"sethost",	/* Name of module */
	"4.0", /* Version */
	"command /sethost", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_sethost)
{
	CommandAdd(modinfo->handle, MSG_SETHOST, m_sethost, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_sethost)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_sethost)
{
	return MOD_SUCCESS;
}

/*
   m_sethost() added by Stskeeps (30/04/1999)
               (modified at 15/05/1999) by Stskeeps | Potvin
   :prefix SETHOST newhost
   parv[1] - newhost
*/
CMD_FUNC(m_sethost)
{
	char *vhost;

	if (MyClient(sptr) && !ValidatePermissionsForPath("client:host",sptr,NULL,NULL,NULL))
	{
  		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
	        sptr->name);
		return 0;
	}

	if (parc < 2)
		vhost = NULL;
	else
		vhost = parv[1];

	/* bad bad bad boys .. ;p */
	if (vhost == NULL)
	{	
		if (MyConnect(sptr))
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Syntax: /SetHost <new host>",
			    me.name, sptr->name);
		}
		return 0;
	}

	if (strlen(parv[1]) < 1)
	{
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Atleast write SOMETHING that makes sense (':' string)",
			    me.name, sptr->name);
		return 0;
	}
	/* too large huh? */
	if (strlen(parv[1]) > (HOSTLEN))
	{
		/* ignore us as well if we're not a child of 3k */
		if (MyConnect(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :*** /SetHost Error: Hostnames are limited to %i characters.",
			    me.name, sptr->name, HOSTLEN);
		return 0;
	}

	if (!valid_host(vhost))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /SetHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them",
		    me.name, sptr->name);
		return 0;
	}
	if (vhost[0] == ':')
	{
		sendto_one(sptr, ":%s NOTICE %s :*** A hostname cannot start with ':'", me.name, sptr->name);
		return 0;
	}

	if (MyClient(sptr) && !strcmp(GetHost(sptr), vhost))
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** /SetHost Error: requested host is same as current host.",
		    me.name, sptr->name);
		return 0;
	}

	{
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /SetHost is disabled", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (MyClient(sptr) && sptr->user->joined)
				{
					sendto_one(sptr, ":%s NOTICE %s :*** /SetHost can not be used while you are on a channel", me.name, sptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				rejoin_leave(sptr);
				/* join sent later when the host has been changed */
				break;
		}

		/* hide it */
		sptr->umodes |= UMODE_HIDE;
		sptr->umodes |= UMODE_SETHOST;
		/* get it in */
		if (sptr->user->virthost)
		{
			MyFree(sptr->user->virthost);
			sptr->user->virthost = NULL;
		}
		sptr->user->virthost = strdup(vhost);
		/* spread it out */
		sendto_server(cptr, 0, 0, ":%s SETHOST %s", sptr->name, parv[1]);

		if (UHOST_ALLOWED == UHALLOW_REJOIN)
			rejoin_joinandmode(sptr);
	}

	if (MyConnect(sptr))
	{
		sendto_one(sptr, ":%s MODE %s :+xt", sptr->name, sptr->name);
		sendto_one(sptr, err_str(RPL_HOSTHIDDEN), me.name, sptr->name, vhost);
		sendto_one(sptr,
		    ":%s NOTICE %s :Your nick!user@host-mask is now (%s!%s@%s) - To disable it type /mode %s -x",
		    me.name, sptr->name, sptr->name, sptr->user->username, vhost,
		    sptr->name);
	}
	return 0;
}
