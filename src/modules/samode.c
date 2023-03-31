/*
 *   IRC - Internet Relay Chat, src/modules/samode.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_samode);

#define MSG_SAMODE 	"SAMODE"	

ModuleHeader MOD_HEADER
  = {
	"samode",
	"5.0",
	"command /samode", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SAMODE, cmd_samode, MAXPARA, CMD_USER);
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
 * cmd_samode
 * parv[1] = channel
 * parv[2] = modes
 * -t
 */
CMD_FUNC(cmd_samode)
{
	Channel *channel;
	MessageTag *mtags = NULL;

	if (parc <= 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SAMODE");
		return;
	}

	channel = find_channel(parv[1]);
	if (!channel)
	{
		sendnumeric(client, ERR_NOSUCHCHANNEL, parv[1]);
		return;
	}

	if (!ValidatePermissionsForPath("sacmd:samode", client, NULL, channel, NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	opermode = 0;
	mtag_generate_issued_by_irc(&mtags, client);
	do_mode(channel, client, mtags, parc - 2, parv + 2, 0, 1);
	safe_free_message_tags(mtags);
}
