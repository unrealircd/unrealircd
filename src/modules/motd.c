/*
 *   IRC - Internet Relay Chat, src/modules/m_motd.c
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
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_MOTD, cmd_motd, MAXPARA, M_USER|M_SERVER);
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
	ConfigItem_tld *ptr;
	MOTDFile *themotd;
	MOTDLine *motdline;
	int  svsnofile = 0;


	if (IsServer(sptr))
		return 0;

	if (hunt_server(cptr, sptr, recv_mtags, ":%s MOTD :%s", 1, parc, parv) != HUNTED_ISME)
	{
		if (MyUser(sptr))
			sptr->local->since += 15;
		return 0;
	}

	ptr = Find_tld(sptr);

	if (ptr)
		themotd = &ptr->motd;
	else
		themotd = &motd;

	if (themotd == NULL || themotd->lines == NULL)
	{
		sendnumeric(sptr, ERR_NOMOTD);
		svsnofile = 1;
		goto svsmotd;
	}

	sendnumeric(sptr, RPL_MOTDSTART, me.name);

	/* tm_year should be zero only if the struct is zero-ed */
	if (themotd && themotd->lines && themotd->last_modified.tm_year)
	{
		sendnumericfmt(sptr, RPL_MOTD, "- %d/%d/%d %d:%02d", themotd->last_modified.tm_mday,
			themotd->last_modified.tm_mon + 1,
			themotd->last_modified.tm_year + 1900,
			themotd->last_modified.tm_hour,
			themotd->last_modified.tm_min);
	}

	motdline = NULL;
	if(themotd)
		motdline = themotd->lines;
	while (motdline)
	{
		sendnumeric(sptr, RPL_MOTD,
			motdline->line);
		motdline = motdline->next;
	}
	svsmotd:

	motdline = svsmotd.lines;
	while (motdline)
	{
		sendnumeric(sptr, RPL_MOTD,
			motdline->line);
		motdline = motdline->next;
	}
	if (svsnofile == 0)
		sendnumeric(sptr, RPL_ENDOFMOTD);
	return 0;
}
