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
#include "whowas.h"

DLLFUNC int m_whowas(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_WHOWAS 	"WHOWAS"	
#define TOK_WHOWAS 	"$"	

ModuleHeader MOD_HEADER(m_whowas)
  = {
	"m_whowas",
	"$Id$",
	"command /whowas", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_whowas)(ModuleInfo *modinfo)
{
	add_Command(MSG_WHOWAS, TOK_WHOWAS, m_whowas, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_whowas)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_whowas)(int module_unload)
{
	if (del_Command(MSG_WHOWAS, TOK_WHOWAS, m_whowas) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_whowas).name);
	}
	return MOD_SUCCESS;
}

/* externally defined functions */
extern unsigned int hash_whowas_name(char *);
extern aWhowas WHOWAS[NICKNAMEHISTORYLENGTH];
extern aWhowas *WHOWASHASH[WW_MAX];

/*
** m_whowas
**      parv[0] = sender prefix
**      parv[1] = nickname queried
*/
DLLFUNC CMD_FUNC(m_whowas)
{
	aWhowas *temp;
	int  cur = 0;
	int  max = -1, found = 0;
	char *p, *nick;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NONICKNAMEGIVEN),
		    me.name, parv[0]);
		return 0;
	}
	if (parc > 2)
		max = atoi(parv[2]);
	if (parc > 3)
		if (hunt_server_token(cptr, sptr, MSG_WHOWAS, TOK_WHOWAS, "%s %s :%s", 3, parc,
		    parv))
			return 0;

	if (!MyConnect(sptr) && (max > 20))
		max = 20;

	p = (char *)strchr(parv[1], ',');
	if (p)
		*p = '\0';
	nick = parv[1];
	temp = WHOWASHASH[hash_whowas_name(nick)];
	found = 0;
	for (; temp; temp = temp->next)
	{
		if (!mycmp(nick, temp->name))
		{
			sendto_one(sptr, rpl_str(RPL_WHOWASUSER),
			    me.name, parv[0], temp->name,
			    temp->username,
			    (IsOper(sptr) ? temp->hostname :
			    (*temp->virthost !=
			    '\0') ? temp->virthost : temp->hostname),
			    temp->realname);
                	if (!((Find_uline(temp->servername)) && !IsOper(sptr) && HIDE_ULINES))
				sendto_one(sptr, rpl_str(RPL_WHOISSERVER), me.name,
				    parv[0], temp->name, temp->servername,
				    myctime(temp->logoff));
			cur++;
			found++;
		}
		if (max > 0 && cur >= max)
			break;
	}
	if (!found)
		sendto_one(sptr, err_str(ERR_WASNOSUCHNICK),
		    me.name, parv[0], nick);

	sendto_one(sptr, rpl_str(RPL_ENDOFWHOWAS), me.name, parv[0], parv[1]);
	return 0;
}
