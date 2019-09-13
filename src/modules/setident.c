/*
 *   IRC - Internet Relay Chat, src/modules/m_setident.c
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
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SETIDENT, cmd_setident, MAXPARA, M_USER);
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

	char *vident, *s;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		if (MyConnect(sptr))
			sendnotice(sptr, "*** Syntax: /SETIDENT <new ident>");
		return 1;
	}

	vident = parv[1];

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NEVER:
			if (MyUser(sptr))
			{
				sendnotice(sptr, "*** /SETIDENT is disabled");
				return 0;
			}
			break;
		case UHALLOW_NOCHANS:
			if (MyUser(sptr) && sptr->user->joined)
			{
				sendnotice(sptr, "*** /SETIDENT cannot be used while you are on a channel");
				return 0;
			}
			break;
		case UHALLOW_REJOIN:
			/* dealt with later */
			break;
	}

	if (strlen(vident) > USERLEN)
	{
		if (MyConnect(sptr))
			sendnotice(sptr, "*** /SETIDENT Error: Usernames are limited to %i characters.", USERLEN);
		return 0;
	}

	/* Check if the new ident contains illegal characters */
	for (s = vident; *s; s++)
	{
		if ((*s == '~') && (s == vident))
			continue;
		if (!isallowed(*s))
		{
			sendnotice(sptr, "*** /SETIDENT Error: A username may contain a-z, A-Z, 0-9, '-', '~' & '.'.");
			return 0;
		}
	}

	userhost_save_current(sptr);

	strlcpy(sptr->user->username, vident, sizeof(sptr->user->username));

	sendto_server(cptr, 0, 0, NULL, ":%s SETIDENT %s", sptr->name, parv[1]);

	userhost_changed(sptr);

	if (MyConnect(sptr))
	{
		sendnotice(sptr, "Your nick!user@host-mask is now (%s!%s@%s)",
		                 sptr->name, sptr->user->username, GetHost(sptr));
	}
	return 0;
}
