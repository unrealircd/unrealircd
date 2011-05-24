/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_sendsno.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_sendsno(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SENDSNO   "SENDSNO"
#define TOK_SENDSNO   "Ss"

ModuleHeader MOD_HEADER(m_sendsno)
  = {
	"sendsno",	/* Name of module */
	"$Id$", /* Version */
	"command /sendsno", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_sendsno)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_SENDSNO, TOK_SENDSNO, m_sendsno, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_sendsno)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_sendsno)(int module_unload)
{
	if (del_Command(MSG_SENDSNO, TOK_SENDSNO, m_sendsno) < 0)
	{
		sendto_realops("Failed to delete command sendsno when unloading %s",
				MOD_HEADER(m_sendsno).name);
	}
	return MOD_SUCCESS;
}

/*
** m_sendsno - Written by Syzop, bit based on SENDUMODE from Stskeeps
**      parv[0] = sender prefix
**      parv[1] = target snomask
**      parv[2] = message text
** Servers can use this to:
**   :server.unreal.net SENDSNO e :Hiiiii
*/
DLLFUNC int m_sendsno(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
char *sno, *msg, *p;
long snomask = 0;
int i;
#ifndef NO_FDLIST
int j;
#endif
aClient *acptr;

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0], "SENDSNO");
		return 0;
	}
	sno = parv[1];
	msg = parv[2];

	/* Forward to others... */
	sendto_serv_butone_token(cptr, sptr->name, MSG_SENDSNO, TOK_SENDSNO,
		"%s :%s", parv[1], parv[2]);

	for (p = sno; *p; p++)
	{
		for(i = 0; i <= Snomask_highest; i++)
		{
			if (Snomask_Table[i].flag == *p)
			{
				snomask |= Snomask_Table[i].mode;
				break;
			}
		}
	}

#ifdef NO_FDLIST
	for(i = 0; i <= LastSlot; i++)
#else
	for (i = oper_fdlist.entry[j = 1]; j <= oper_fdlist.last_entry; i = oper_fdlist.entry[++j])
#endif
		if ((acptr = local[i]) && IsPerson(acptr) && IsAnOper(acptr) &&
		    (acptr->user->snomask & snomask))
		{
			sendto_one(acptr, ":%s NOTICE %s :%s", me.name, acptr->name, msg);
		}

	return 0;
}

