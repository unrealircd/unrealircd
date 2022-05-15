/*
 *   IRC - Internet Relay Chat, src/modules/sethost.c
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
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SETHOST, cmd_sethost, MAXPARA, CMD_USER);
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
	const char *vhost;

	if (MyUser(client) && !ValidatePermissionsForPath("self:set:host",client,NULL,NULL,NULL))
	{
  		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (parc < 2)
		vhost = NULL;
	else
		vhost = parv[1];

	if (BadPtr(vhost))
	{	
		if (MyConnect(client))
			sendnotice(client, "*** Syntax: /SetHost <new host>");
		return;
	}

	if (strlen(parv[1]) > (HOSTLEN))
	{
		if (MyConnect(client))
			sendnotice(client, "*** /SetHost Error: Hostnames are limited to %i characters.", HOSTLEN);
		return;
	}

	if (!valid_host(vhost, 0))
	{
		sendnotice(client, "*** /SetHost Error: A hostname may only contain a-z, A-Z, 0-9, '-' & '.'.");
		return;
	}
	if (vhost[0] == ':')
	{
		sendnotice(client, "*** A hostname cannot start with ':'");
		return;
	}

	if (MyUser(client) && !strcmp(GetHost(client), vhost))
	{
		sendnotice(client, "/SetHost Error: requested host is same as current host.");
		return;
	}

	userhost_save_current(client);

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnotice(client, "*** /SetHost is disabled");
				return;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(client) && client->user->joined)
			{
				sendnotice(client, "*** /SetHost can not be used while you are on a channel");
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* join sent later when the host has been changed */
			break;
	}

	/* hide it */
	client->umodes |= UMODE_HIDE;
	client->umodes |= UMODE_SETHOST;
	/* get it in */
	safe_strdup(client->user->virthost, vhost);
	/* spread it out */
	sendto_server(client, 0, 0, NULL, ":%s SETHOST %s", client->id, parv[1]);

	userhost_changed(client);

	if (MyConnect(client))
	{
		sendto_one(client, NULL, ":%s MODE %s :+xt", client->name, client->name);
		sendnotice(client, 
		    "Your nick!user@host-mask is now (%s!%s@%s) - To disable it type /mode %s -x",
		     client->name, client->user->username, vhost,
		    client->name);
	}
}
