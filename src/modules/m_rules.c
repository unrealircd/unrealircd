/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

CMD_FUNC(m_rules);

#define MSG_RULES 	"RULES"	

ModuleHeader MOD_HEADER(m_rules)
  = {
	"m_rules",
	"4.0",
	"command /rules", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_rules)
{
	CommandAdd(modinfo->handle, MSG_RULES, m_rules, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_rules)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_rules)
{
	return MOD_SUCCESS;
}

/*
 * Heavily modified from the ircu m_motd by codemastr
 * Also svsmotd support added
 */
CMD_FUNC(m_rules)
{
	ConfigItem_tld *ptr;
	aMotdLine *temp;

	temp = NULL;

	if (hunt_server(cptr, sptr, ":%s RULES :%s", 1, parc, parv) != HUNTED_ISME)
		return 0;

	ptr = Find_tld(sptr);

	if (ptr)
		temp = ptr->rules.lines;
	if(!temp)
		temp = rules.lines;

	if (temp == NULL)
	{
		sendto_one(sptr, err_str(ERR_NORULES), me.name, sptr->name);
		return 0;

	}

	sendto_one(sptr, rpl_str(RPL_RULESSTART), me.name, sptr->name, me.name);

	while (temp)
	{
		sendto_one(sptr, rpl_str(RPL_RULES), me.name, sptr->name,
		    temp->line);
		temp = temp->next;
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFRULES), me.name, sptr->name);
	return 0;
}
