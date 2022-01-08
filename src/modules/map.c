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

CMD_FUNC(cmd_map);

#define MSG_MAP 	"MAP"	

static int lmax = 0;
static int umax = 0;

static int dcount(int n)
{
   int cnt = 0;

   while (n != 0)
   {
	   n = n/10;
	   cnt++;
   }

   return cnt;
}

ModuleHeader MOD_HEADER
  = {
	"map",
	"5.0",
	"command /map", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_MAP, cmd_map, MAXPARA, CMD_USER);
	ISupportAdd(modinfo->handle, "MAP", NULL);
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
 * New /MAP format -Potvin
 * dump_map function.
 */
static void dump_map(Client *client, Client *server, char *mask, int prompt_length, int length)
{
	static char prompt[64];
	char *p = &prompt[prompt_length];
	int  cnt = 0;
	Client *acptr;

	*p = '\0';

	if (prompt_length > 60)
		sendnumeric(client, RPL_MAPMORE, prompt, length, server->name);
	else
	{
		char tbuf[256];
		char sid[10];
		int len = length - strlen(server->name) + 1;

		if (len < 0)
			len = 0;
		if (len > 255)
			len = 255;

		tbuf[len--] = '\0';
		while (len >= 0)
			tbuf[len--] = '-';
		if (IsOper(client))
			snprintf(sid, sizeof(sid), " [%s]", server->id);
		sendnumeric(client, RPL_MAP, prompt, server->name, tbuf, umax,
			server->server->users, (double)(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
			(server->server->users * 100.0 / irccounts.clients),
			IsOper(client) ? sid : "");
		cnt = 0;
	}

	if (prompt_length > 0)
	{
		p[-1] = ' ';
		if (p[-2] == '`')
			p[-2] = ' ';
	}
	if (prompt_length > 60)
		return;

	strcpy(p, "|-");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (acptr->uplink != server ||
 		    (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL)))
			continue;
		SetMap(acptr);
		cnt++;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL))
			continue;
		if (acptr->uplink != server)
			continue;
		if (!IsMap(acptr))
			continue;
		if (--cnt == 0)
			*p = '`';
		dump_map(client, acptr, mask, prompt_length + 2, length - 2);
	}

	if (prompt_length > 0)
		p[-1] = '-';
}

void dump_flat_map(Client *client, Client *server, int length)
{
	char buf[4];
	char tbuf[256];
	Client *acptr;
	int cnt = 0, len = 0, hide_ulines;

	hide_ulines = (HIDE_ULINES && !ValidatePermissionsForPath("server:info:map:ulines",client,NULL,NULL,NULL)) ? 1 : 0;

	len = length - strlen(server->name) + 3;
	if (len < 0)
		len = 0;
	if (len > 255)
		len = 255;

	tbuf[len--] = '\0';
	while (len >= 0)
		tbuf[len--] = '-';

	sendnumeric(client, RPL_MAP, "", server->name, tbuf, umax, server->server->users,
		(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
		(server->server->users * 100.0 / irccounts.clients), "");

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if ((IsULine(acptr) && hide_ulines) || (acptr == server))
			continue;
		cnt++;
	}

	strcpy(buf, "|-");
	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if ((IsULine(acptr) && hide_ulines) || (acptr == server))
			continue;
		if (--cnt == 0)
			*buf = '`';

		len = length - strlen(acptr->name) + 1;
		if (len < 0)
			len = 0;
		if (len > 255)
			len = 255;

		tbuf[len--] = '\0';
		while (len >= 0)
			tbuf[len--] = '-';

		sendnumeric(client, RPL_MAP, buf, acptr->name, tbuf, umax, acptr->server->users,
			(lmax < 10) ? 4 : (lmax == 100) ? 6 : 5,
			(acptr->server->users * 100.0 / irccounts.clients), "");
	}
}

/*
** New /MAP format. -Potvin
** cmd_map (NEW)
**
**      parv[1] = server mask
**/
CMD_FUNC(cmd_map)
{
	Client *acptr;
	int  longest = strlen(me.name);
	float avg_users;

	umax = 0;
	lmax = 0;

	if (parc < 2)
		parv[1] = "*";

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		int perc = (acptr->server->users * 100 / irccounts.clients);
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
		if (lmax < perc)
			lmax = perc;
		if (umax < dcount(acptr->server->users))
			umax = dcount(acptr->server->users);
	}

	if (longest > 60)
		longest = 60;
	longest += 2;

	if (FLAT_MAP && !ValidatePermissionsForPath("server:info:map:real-map",client,NULL,NULL,NULL))
		dump_flat_map(client, &me, longest);
	else
		dump_map(client, &me, "*", 0, longest);

	avg_users = irccounts.clients * 1.0 / irccounts.servers;
	sendnumeric(client, RPL_MAPUSERS, irccounts.servers, (irccounts.servers > 1 ? "s" : ""), irccounts.clients,
		(irccounts.clients > 1 ? "s" : ""), avg_users);
	sendnumeric(client, RPL_MAPEND);
}
