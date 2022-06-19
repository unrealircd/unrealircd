/*
 *   IRC - Internet Relay Chat, src/modules/opermotd.c
 *   (C) 2005 The UnrealIRCd Team
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

CMD_FUNC(cmd_opermotd);

#define MSG_OPERMOTD 	"OPERMOTD"	

ModuleHeader MOD_HEADER
  = {
	"opermotd",
	"5.0",
	"command /opermotd", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_OPERMOTD, cmd_opermotd, MAXPARA, CMD_USER|CMD_SERVER);
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
 * Modified from comstud by codemastr
 */
CMD_FUNC(cmd_opermotd)
{
	MOTDLine *motdline;
	ConfigItem_tld *tld;

	if (!ValidatePermissionsForPath("server:opermotd",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	tld = find_tld(client);
	if (tld && tld->opermotd.lines)
		motdline = tld->opermotd.lines;
	else
		motdline = opermotd.lines;

	if (!motdline)
	{
		sendnumeric(client, ERR_NOOPERMOTD);
		return;
	}
	sendnumeric(client, RPL_MOTDSTART, me.name);
	sendnumeric(client, RPL_MOTD, "IRC Operator Message of the Day");

	while (motdline)
	{
		sendnumeric(client, RPL_MOTD,
			   motdline->line);
		motdline = motdline->next;
	}
	sendnumericfmt(client, RPL_ENDOFMOTD, ":End of /OPERMOTD command.");
}
