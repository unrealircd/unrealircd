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

CMD_FUNC(cmd_sethost);

/* Place includes here */
#define MSG_SETHOST 	"SETHOST"	/* sethost */

ModuleHeader MOD_HEADER
  = {
	"sethost",	/* Name of module */
	"5.0", /* Version */
	"command /sethost", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SETHOST, cmd_sethost, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
   cmd_sethost() added by Stskeeps (30/04/1999)
               (modified at 15/05/1999) by Stskeeps | Potvin
   :prefix SETHOST newhost
   parv[1] - newhost
*/
CMD_FUNC(cmd_sethost)
{
	char *vhost;

	if (MyUser(sptr) && !ValidatePermissionsForPath("self:set:host",sptr,NULL,NULL,NULL))
	{
  		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if (parc < 2)
		vhost = NULL;
	else
		vhost = parv[1];

	if (BadPtr(vhost))
	{	
		if (MyConnect(sptr))
			sendnotice(sptr, "*** Syntax: /SetHost <new host>");
		return 0;
	}

	if (strlen(parv[1]) > (HOSTLEN))
	{
		if (MyConnect(sptr))
			sendnotice(sptr, "*** /SetHost Error: Hostnames are limited to %i characters.", HOSTLEN);
		return 0;
	}

	if (!valid_host(vhost))
	{
		sendnotice(sptr, "*** /SetHost Error: A hostname may only contain a-z, A-Z, 0-9, '-' & '.'.");
		return 0;
	}
	if (vhost[0] == ':')
	{
		sendnotice(sptr, "*** A hostname cannot start with ':'");
		return 0;
	}

	if (MyUser(sptr) && !strcmp(GetHost(sptr), vhost))
	{
		sendnotice(sptr, "/SetHost Error: requested host is same as current host.");
		return 0;
	}

	userhost_save_current(sptr);

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(sptr))
			{
				sendnotice(sptr, "*** /SetHost is disabled");
				return 0;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(sptr) && sptr->user->joined)
			{
				sendnotice(sptr, "*** /SetHost can not be used while you are on a channel");
				return 0;
			}
			break;
		case UHALLOW_REJOIN:
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
	sendto_server(cptr, 0, 0, NULL, ":%s SETHOST %s", sptr->name, parv[1]);

	userhost_changed(sptr);

	if (MyConnect(sptr))
	{
		sendto_one(sptr, NULL, ":%s MODE %s :+xt", sptr->name, sptr->name);
		sendnumeric(sptr, RPL_HOSTHIDDEN, vhost);
		sendnotice(sptr, 
		    "Your nick!user@host-mask is now (%s!%s@%s) - To disable it type /mode %s -x",
		     sptr->name, sptr->user->username, vhost,
		    sptr->name);
	}
	return 0;
}
