/*
 *   cmd_ircops - /IRCOPS command that lists IRC Operators
 *   (C) Copyright 2004-2016 Syzop <syzop@vulnscan.org>
 *   (C) Copyright 2003-2004 AngryWolf <angrywolf@flashmail.com>
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

#define MSG_IRCOPS        "IRCOPS"
#define IsAway(x)         (x)->user->away

CMD_FUNC(cmd_ircops);

ModuleHeader MOD_HEADER
  = {
	"ircops",
	"3.71",
	"/IRCOPS command that lists IRC Operators",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	if (CommandExists(MSG_IRCOPS))
	{
		config_error("Command " MSG_IRCOPS " already exists");
		return MOD_FAILED;
	}
	CommandAdd(modinfo->handle, MSG_IRCOPS, cmd_ircops, MAXPARA, CMD_USER);

	if (ModuleGetError(modinfo->handle) != MODERR_NOERROR)
	{
		config_error("Error adding command " MSG_IRCOPS ": %s",
			ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

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
 * cmd_ircops
 *
 *     parv[0]: sender prefix
 *
 *     Originally comes from TR-IRCD, but I changed it in several places.
 *     In addition, I didn't like to display network name. In addition,
 *     instead of realname, servername is shown. See the original
 *     header below.
 */

/************************************************************************
 * IRC - Internet Relay Chat, modules/ircops.c
 *
 *   Copyright (C) 2000-2002 TR-IRCD Development
 *
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

CMD_FUNC(cmd_ircops)
{
	Client *acptr;
	char buf[512];
	int opers = 0, total = 0, aways = 0;

	list_for_each_entry(acptr, &client_list, client_node)
	{
		/* List only real IRC Operators */
		if (IsULine(acptr) || !IsUser(acptr) || !IsOper(acptr))
			continue;
		/* Don't list +H users */
		if (!IsOper(client) && IsHideOper(acptr))
			continue;

		sendto_one(client, NULL, ":%s %d %s :\2%s\2 is %s on %s" "%s",
			me.name, RPL_TEXT, client->name,
			acptr->name,
			"an IRC Operator", /* find_otype(acptr->umodes), */
			acptr->user->server,
			(IsAway(acptr) ? " [Away]" : ""));

		if (IsAway(acptr))
			aways++;
		else
			opers++;

	}

	total = opers + aways;

	snprintf(buf, sizeof(buf),
		"Total: \2%d\2 IRCOP%s online - \2%d\2 Oper%s available and \2%d\2 Away",
		total, (total) != 1 ? "s" : "",
		opers, opers != 1 ? "s" : "",
		aways);

	sendnumericfmt(client, RPL_TEXT, ":%s", buf);
	sendnumericfmt(client, RPL_TEXT, ":End of /IRCOPS list");
}
