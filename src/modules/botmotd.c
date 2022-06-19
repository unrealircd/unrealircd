/*
 *   IRC - Internet Relay Chat, src/modules/botmotd.c
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

CMD_FUNC(cmd_botmotd);

#define MSG_BOTMOTD 	"BOTMOTD"	

ModuleHeader MOD_HEADER
  = {
	"botmotd",
	"5.0",
	"command /botmotd", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_BOTMOTD, cmd_botmotd, MAXPARA, CMD_USER|CMD_SERVER);
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
CMD_FUNC(cmd_botmotd)
{
	MOTDLine *motdline;
	ConfigItem_tld *tld;

	if (hunt_server(client, recv_mtags, "BOTMOTD", 1, parc, parv) != HUNTED_ISME)
		return;

	if (!IsUser(client))
		return;

	tld = find_tld(client);
	if (tld && tld->botmotd.lines)
		motdline = tld->botmotd.lines;
	else
		motdline = botmotd.lines;

	if (!motdline)
	{
		sendnotice(client, "BOTMOTD File not found");
		return;
	}
	sendnotice(client, "- %s Bot Message of the Day - ", me.name);

	while (motdline)
	{
		sendnotice(client, "- %s", motdline->line);
		motdline = motdline->next;
	}
	sendnotice(client, "End of /BOTMOTD command.");
}
