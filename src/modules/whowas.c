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

CMD_FUNC(cmd_whowas);

#define MSG_WHOWAS 	"WHOWAS"	

ModuleHeader MOD_HEADER
  = {
	"whowas",
	"5.0",
	"command /whowas", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_WHOWAS, cmd_whowas, MAXPARA, M_USER);
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

/* externally defined functions */
extern aWhowas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
extern aWhowas MODVAR *WHOWASHASH[WHOWAS_HASH_TABLE_SIZE];

/*
** cmd_whowas
**      parv[1] = nickname queried
*/
CMD_FUNC(cmd_whowas)
{
	aWhowas *temp;
	int  cur = 0;
	int  max = -1, found = 0;
	char *p, *nick;

	if (parc < 2)
	{
		sendnumeric(sptr, ERR_NONICKNAMEGIVEN);
		return 0;
	}
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server(cptr, sptr, recv_mtags, ":%s WHOWAS %s %s :%s", 3, parc, parv))
			return 0;

	if (!MyConnect(sptr) && (max > 20))
		max = 20;

	p = strchr(parv[1], ',');
	if (p)
		*p = '\0';
	nick = parv[1];
	temp = WHOWASHASH[hash_whowas_name(nick)];
	found = 0;
	for (; temp; temp = temp->next)
	{
		if (!mycmp(nick, temp->name))
		{
			sendnumeric(sptr, RPL_WHOWASUSER, temp->name,
			    temp->username,
			    (IsOper(sptr) ? temp->hostname :
			    (*temp->virthost !=
			    '\0') ? temp->virthost : temp->hostname),
			    temp->realname);
                	if (!((Find_uline(temp->servername)) && !IsOper(sptr) && HIDE_ULINES))
				sendnumeric(sptr, RPL_WHOISSERVER, temp->name, temp->servername,
				    myctime(temp->logoff));
			cur++;
			found++;
		}
		if (max > 0 && cur >= max)
			break;
	}
	if (!found)
		sendnumeric(sptr, ERR_WASNOSUCHNICK, nick);

	sendnumeric(sptr, RPL_ENDOFWHOWAS, parv[1]);
	return 0;
}
