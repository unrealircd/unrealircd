/*
 *   IRC - Internet Relay Chat, src/modules/motd.c
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

CMD_FUNC(cmd_motd);

#define MSG_MOTD 	"MOTD"	

ModuleHeader MOD_HEADER
  = {
	"motd",
	"5.0",
	"command /motd", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_MOTD, cmd_motd, MAXPARA, CMD_USER|CMD_SERVER);
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
CMD_FUNC(cmd_motd)
{
	ConfigItem_tld *tld;
	MOTDFile *themotd;
	MOTDLine *motdline;
	int  svsnofile = 0;

	if (IsServer(client))
		return;

	if (hunt_server(client, recv_mtags, "MOTD", 1, parc, parv) != HUNTED_ISME)
	{
		if (MyUser(client))
			add_fake_lag(client, 15000);
		return;
	}

	tld = find_tld(client);

	if (tld && tld->motd.lines)
		themotd = &tld->motd;
	else
		themotd = &motd;

	if (themotd == NULL || themotd->lines == NULL)
	{
		sendnumeric(client, ERR_NOMOTD);
		svsnofile = 1;
		goto svsmotd;
	}

	sendnumeric(client, RPL_MOTDSTART, me.name);

	/* tm_year should be zero only if the struct is zero-ed */
	if (themotd && themotd->lines && themotd->last_modified.tm_year)
	{
		sendnumericfmt(client, RPL_MOTD, ":- %.04d-%.02d-%.02d %.02d:%02d",
			themotd->last_modified.tm_year + 1900,
			themotd->last_modified.tm_mon + 1,
			themotd->last_modified.tm_mday,
			themotd->last_modified.tm_hour,
			themotd->last_modified.tm_min);
	}

	motdline = NULL;
	if (themotd)
		motdline = themotd->lines;
	while (motdline)
	{
		sendnumeric(client, RPL_MOTD,
			motdline->line);
		motdline = motdline->next;
	}
	svsmotd:

	motdline = svsmotd.lines;
	while (motdline)
	{
		sendnumeric(client, RPL_MOTD,
			motdline->line);
		motdline = motdline->next;
	}
	if (svsnofile == 0)
		sendnumeric(client, RPL_ENDOFMOTD);
}
