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

CMD_FUNC(m_map);

#define MSG_MAP 	"MAP"	

ModuleHeader MOD_HEADER(m_map)
  = {
	"m_map",
	"4.0",
	"command /map", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_map)
{
	CommandAdd(modinfo->handle, MSG_MAP, m_map, MAXPARA, M_USER|M_ANNOUNCE);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_map)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_map)
{
	return MOD_SUCCESS;
}

/*
 * New /MAP format -Potvin
 * dump_map function.
 */
static void dump_map(aClient *cptr, aClient *server, char *mask, int prompt_length, int length)
{
	static char prompt[64];
	char *p = &prompt[prompt_length];
	int  cnt = 0;
	aClient *acptr;

	*p = '\0';

	if (prompt_length > 60)
		sendto_one(cptr, rpl_str(RPL_MAPMORE), me.name, cptr->name,
		    prompt, length, server->name);
	else
	{
		sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, prompt,
		    length, server->name, server->serv->users, IsOper(cptr) ? server->id : "");
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
		if (acptr->srvptr != server ||
 		    (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("map:ulines",cptr,NULL,NULL,NULL)))
			continue;
		acptr->flags |= FLAGS_MAP;
		cnt++;
	}

	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if (IsULine(acptr) && HIDE_ULINES && !ValidatePermissionsForPath("map:ulines",cptr,NULL,NULL,NULL))
			continue;
		if (acptr->srvptr != server)
			continue;
		if (!(acptr->flags & FLAGS_MAP))
			continue;
		if (--cnt == 0)
			*p = '`';
		dump_map(cptr, acptr, mask, prompt_length + 2, length - 2);
	}

	if (prompt_length > 0)
		p[-1] = '-';
}

void dump_flat_map(aClient *cptr, aClient *server, int length)
{
char buf[4];
aClient *acptr;
int cnt = 0, hide_ulines;

	hide_ulines = (HIDE_ULINES && !ValidatePermissionsForPath("map:ulines",cptr,NULL,NULL,NULL)) ? 1 : 0;

	sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, "",
	    length, server->name, server->serv->users, "");

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
		sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, buf,
		    length-2, acptr->name, acptr->serv->users, "");
	}
}

/*
** New /MAP format. -Potvin
** m_map (NEW)
**
**      parv[1] = server mask
**/
CMD_FUNC(m_map)
{
	aClient *acptr;
	int  longest = strlen(me.name);


	if (parc < 2)
		parv[1] = "*";
	list_for_each_entry(acptr, &global_server_list, client_node)
	{
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
	}
	if (longest > 60)
		longest = 60;
	longest += 2;
	if (FLAT_MAP && !ValidatePermissionsForPath("map:real-map",sptr,NULL,NULL,NULL))
		dump_flat_map(sptr, &me, longest);
	else
		dump_map(sptr, &me, "*", 0, longest);
	sendto_one(sptr, rpl_str(RPL_MAPEND), me.name, sptr->name);

	return 0;
}
