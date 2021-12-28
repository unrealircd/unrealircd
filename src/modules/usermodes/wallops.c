/*
 *   IRC - Internet Relay Chat, src/modules/usermodes/wallops.c
 *   (C) 2004-2021 The UnrealIRCd Team
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

CMD_FUNC(cmd_wallops);

#define MSG_WALLOPS 	"WALLOPS"	

ModuleHeader MOD_HEADER
  = {
	"usermodes/wallops",
	"5.0",
	"command /wallops", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

long UMODE_WALLOP = 0L;        /* send wallops to them */

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	CommandAdd(modinfo->handle, MSG_WALLOPS, cmd_wallops, 1, CMD_USER|CMD_SERVER);
	UmodeAdd(modinfo->handle, 'w', UMODE_GLOBAL, 0, umode_allow_all, &UMODE_WALLOP);
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

#define SendWallops(x)          (!IsMe(x) && IsUser(x) && ((x)->umodes & UMODE_WALLOP))

/** Send a message to all wallops, except one.
 * @param one		Skip sending the message to this client/direction
 * @param from		The sender (can not be NULL)
 * @param pattern	The format string / pattern to use.
 * @param ...		Format string parameters.
 */
void sendto_wallops(Client *one, Client *from, FORMAT_STRING(const char *pattern), ...)
{
	va_list vl;
	Client *acptr;

	++current_serial;
	list_for_each_entry(acptr, &client_list, client_node)
	{
		if (!SendWallops(acptr))
			continue;
		if (acptr->direction->local->serial == current_serial)	/* sent message along it already ? */
			continue;
		if (acptr->direction == one)
			continue;	/* ...was the one I should skip */
		acptr->direction->local->serial = current_serial;

		va_start(vl, pattern);
		vsendto_prefix_one(acptr->direction, from, NULL, pattern, vl);
		va_end(vl);
	}
}

/*
** cmd_wallops (write to *all* opers currently online)
**	parv[1] = message text
*/
CMD_FUNC(cmd_wallops)
{
	const char *message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "WALLOPS");
		return;
	}

	if (!ValidatePermissionsForPath("chat:wallops",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	sendto_wallops(client->direction, client, ":%s WALLOPS :%s", client->name, message);
	if (MyUser(client))
		sendto_prefix_one(client, client, NULL, ":%s WALLOPS :%s", client->name, message);
}
