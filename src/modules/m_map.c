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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_map(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_MAP 	"MAP"	
#define TOK_MAP 	"u"	

ModuleHeader MOD_HEADER(m_map)
  = {
	"m_map",
	"$Id$",
	"command /map", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_map)(ModuleInfo *modinfo)
{
	add_Command(MSG_MAP, TOK_MAP, m_map, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_map)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_map)(int module_unload)
{
	if (del_Command(MSG_MAP, TOK_MAP, m_map) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_map).name);
	}
	return MOD_SUCCESS;
}

char servername[128][128];
int  server_usercount[128];
int  numservers = 0;

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
	Link *lp;

	*p = '\0';

	if (prompt_length > 60)
		sendto_one(cptr, rpl_str(RPL_MAPMORE), me.name, cptr->name,
		    prompt, server->name);
	else
	{
		sendto_one(cptr, rpl_str(RPL_MAP), me.name, cptr->name, prompt,
		    length, server->name, server->serv->users,
		    (server->serv->numeric ? (char *)my_itoa(server->serv->
		    numeric) : ""));
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


	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (acptr->srvptr != server ||
 		    (IsULine(acptr) && !IsOper(cptr) && HIDE_ULINES))
			continue;
		acptr->flags |= FLAGS_MAP;
		cnt++;
	}

	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if (IsULine(acptr) && HIDE_ULINES && !IsOper(cptr))
			continue;
		if (acptr->srvptr != server)
			continue;
		if (!acptr->flags & FLAGS_MAP)
			continue;
		if (--cnt == 0)
			*p = '`';
		dump_map(cptr, acptr, mask, prompt_length + 2, length - 2);

	}

	if (prompt_length > 0)
		p[-1] = '-';
}

/*
** New /MAP format. -Potvin
** m_map (NEW)
**
**      parv[0] = sender prefix
**      parv[1] = server mask
**/
DLLFUNC CMD_FUNC(m_map)
{
	Link *lp;
	aClient *acptr;
	int  longest = strlen(me.name);


	if (parc < 2)
		parv[1] = "*";
	for (lp = Servers; lp; lp = lp->next)
	{
		acptr = lp->value.cptr;
		if ((strlen(acptr->name) + acptr->hopcount * 2) > longest)
			longest = strlen(acptr->name) + acptr->hopcount * 2;
	}
	if (longest > 60)
		longest = 60;
	longest += 2;
	dump_map(sptr, &me, "*", 0, longest);
	sendto_one(sptr, rpl_str(RPL_MAPEND), me.name, parv[0]);

	return 0;
}
