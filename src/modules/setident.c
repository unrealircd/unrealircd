/*
 *   IRC - Internet Relay Chat, src/modules/setident.c
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

#define MSG_SETIDENT 	"SETIDENT"	/* set ident */

CMD_FUNC(cmd_setident);

ModuleHeader MOD_HEADER
  = {
	"setident",	/* Name of module */
	"5.0", /* Version */
	"/setident", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SETIDENT, cmd_setident, MAXPARA, CMD_USER);
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

/* cmd_setident - 12/05/1999 - Stskeeps
 * :prefix SETIDENT newident
 * parv[1] - newident
 * D: This will set your username to be <x> (like (/setident Root))
 * (if you are IRCop) **efg*
 * Cloning of cmd_sethost at some points - so same authors ;P
*/
CMD_FUNC(cmd_setident)
{
	const char *vident, *s;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		if (MyConnect(client))
			sendnotice(client, "*** Syntax: /SETIDENT <new ident>");
		return;
	}

	vident = parv[1];

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnotice(client, "*** /SETIDENT is disabled");
				return;
			}
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(client) && client->user->joined)
			{
				sendnotice(client, "*** /SETIDENT cannot be used while you are on a channel");
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* dealt with later */
			break;
	}

	if (strlen(vident) > USERLEN)
	{
		if (MyConnect(client))
			sendnotice(client, "*** /SETIDENT Error: Usernames are limited to %i characters.", USERLEN);
		return;
	}

	/* Check if the new ident contains illegal characters */
	if (!valid_username(vident))
	{
		sendnotice(client, "*** /SETIDENT Error: A username may contain a-z, A-Z, 0-9, '-', '~' & '.'.");
		return;
	}

	userhost_save_current(client);

	strlcpy(client->user->username, vident, sizeof(client->user->username));

	sendto_server(client, 0, 0, NULL, ":%s SETIDENT %s", client->id, parv[1]);

	userhost_changed(client);

	if (MyConnect(client))
	{
		sendnotice(client, "Your nick!user@host-mask is now (%s!%s@%s)",
		                 client->name, client->user->username, GetHost(client));
	}
}
