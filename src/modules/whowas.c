/*
 *   IRC - Internet Relay Chat, src/modules/whowas.c
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
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_WHOWAS, cmd_whowas, MAXPARA, CMD_USER);
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
extern WhoWas MODVAR WHOWAS[NICKNAMEHISTORYLENGTH];
extern WhoWas MODVAR *WHOWASHASH[WHOWAS_HASH_TABLE_SIZE];

/*
** cmd_whowas
**      parv[1] = nickname queried
*/
CMD_FUNC(cmd_whowas)
{
	char request[BUFSIZE];
	WhoWas *temp;
	int  cur = 0;
	int  max = -1, found = 0;
	char *p, *nick;

	if (parc < 2)
	{
		sendnumeric(client, ERR_NONICKNAMEGIVEN);
		return;
	}

	if (parc > 2)
		max = atoi(parv[2]);

	if (parc > 3)
	{
		if (hunt_server(client, recv_mtags, "WHOWAS", 3, parc, parv))
			return; /* Not for us */
	}

	if (!MyConnect(client) && (max > 20))
		max = 20;

	strlcpy(request, parv[1], sizeof(request));
	p = strchr(request, ',');
	if (p)
		*p = '\0'; /* cut off at first */

	nick = request;
	temp = WHOWASHASH[hash_whowas_name(nick)];
	found = 0;
	for (; temp; temp = temp->next)
	{
		if (!mycmp(nick, temp->name))
		{
			sendnumeric(client, RPL_WHOWASUSER, temp->name,
			    temp->username,
			    BadPtr(temp->virthost) ? temp->hostname : temp->virthost,
			    temp->realname);
			if (!BadPtr(temp->ip) && ValidatePermissionsForPath("client:see:ip",client,NULL,NULL,NULL))
			{
				sendnumericfmt(client, RPL_WHOISHOST, "%s :was connecting from %s@%s %s",
					temp->name,
					temp->username, temp->hostname,
					temp->ip ? temp->ip : "");
			}
			if (IsOper(client) && !BadPtr(temp->account))
			{
				sendnumericfmt(client, RPL_WHOISLOGGEDIN, "%s %s :was logged in as",
					temp->name,
					temp->account);
			}
			if (!((find_uline(temp->servername)) && !IsOper(client) && HIDE_ULINES))
			{
				sendnumeric(client, RPL_WHOISSERVER, temp->name, temp->servername,
				            myctime(temp->logoff));
			}
			cur++;
			found++;
		}
		if (max > 0 && cur >= max)
			break;
	}
	if (!found)
		sendnumeric(client, ERR_WASNOSUCHNICK, nick);

	sendnumeric(client, RPL_ENDOFWHOWAS, request);
}
