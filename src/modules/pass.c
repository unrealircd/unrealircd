/*
 *   IRC - Internet Relay Chat, src/modules/pass.c
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

CMD_FUNC(cmd_pass);

#define MSG_PASS 	"PASS"	

ModuleHeader MOD_HEADER
  = {
	"pass",
	"5.0",
	"command /pass", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PASS, cmd_pass, 1, CMD_UNREGISTERED|CMD_USER|CMD_SERVER);
	
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

/***************************************************************************
 * cmd_pass() - Added Sat, 4 March 1989
 ***************************************************************************/
/*
** cmd_pass
**	parv[1] = password
*/
CMD_FUNC(cmd_pass)
{
	const char *password = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(client) || (!IsUnknown(client) && !IsHandshake(client)))
	{
		sendnumeric(client, ERR_ALREADYREGISTRED);
		return;
	}

	if (BadPtr(password))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "PASS");
		return;
	}

	/* Store the password */
	safe_strldup(client->local->passwd, password, PASSWDLEN+1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturn(HOOKTYPE_LOCAL_PASS, !=0, client, password);
}
