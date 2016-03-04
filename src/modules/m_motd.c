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

CMD_FUNC(m_motd);

#define MSG_MOTD 	"MOTD"	

ModuleHeader MOD_HEADER(m_motd)
  = {
	"m_motd",
	"4.0",
	"command /motd", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_motd)
{
	CommandAdd(modinfo->handle, MSG_MOTD, m_motd, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_motd)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_motd)
{
	return MOD_SUCCESS;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
CMD_FUNC(m_motd)
{
	ConfigItem_tld *ptr;
	aMotdFile *themotd;
	aMotdLine *motdline;
	int  svsnofile = 0;


	if (IsServer(sptr))
		return 0;

	if (hunt_server(cptr, sptr, ":%s MOTD :%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	ptr = Find_tld(sptr);

	if (ptr)
		themotd = &ptr->motd;
	else
		themotd = &motd;

      playmotd:
	if (themotd == NULL || themotd->lines == NULL)
	{
		sendto_one(sptr, err_str(ERR_NOMOTD), me.name, sptr->name);
		svsnofile = 1;
		goto svsmotd;
	}

	sendto_one(sptr, rpl_str(RPL_MOTDSTART), me.name, sptr->name,
		   me.name);

	/* tm_year should be zero only if the struct is zero-ed */
	if (themotd && themotd->lines && themotd->last_modified.tm_year)
	{
		sendto_one(sptr, ":%s %d %s :- %d/%d/%d %d:%02d",
			me.name, RPL_MOTD, sptr->name,
			themotd->last_modified.tm_mday,
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
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
		    motdline->line);
		motdline = motdline->next;
	}
      svsmotd:

	motdline = svsmotd.lines;
	while (motdline)
	{
		sendto_one(sptr, rpl_str(RPL_MOTD), me.name, sptr->name,
		    motdline->line);
		motdline = motdline->next;
	}
	if (svsnofile == 0)
		sendto_one(sptr, rpl_str(RPL_ENDOFMOTD), me.name, sptr->name);
	return 0;
}
