/*
 *   IRC - Internet Relay Chat, src/modules/rules.c
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

CMD_FUNC(cmd_rules);

#define MSG_RULES 	"RULES"	

ModuleHeader MOD_HEADER
  = {
	"rules",
	"5.0",
	"command /rules", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_RULES, cmd_rules, MAXPARA, CMD_USER);
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
 * Heavily modified from the ircu cmd_motd by codemastr
 * Also svsmotd support added
 */
CMD_FUNC(cmd_rules)
{
	ConfigItem_tld *tld;
	MOTDLine *temp;

	if (hunt_server(client, recv_mtags, "RULES", 1, parc, parv) != HUNTED_ISME)
		return;

	tld = find_tld(client);
	if (tld && tld->rules.lines)
		temp = tld->rules.lines;
	else
		temp = rules.lines;

	if (temp == NULL)
	{
		sendnumeric(client, ERR_NORULES);
		return;

	}

	sendnumeric(client, RPL_RULESSTART, me.name);

	while (temp)
	{
		sendnumeric(client, RPL_RULES,
		    temp->line);
		temp = temp->next;
	}
	sendnumeric(client, RPL_ENDOFRULES);
}
