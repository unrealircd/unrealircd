/*
 *   IRC - Internet Relay Chat, src/modules/m_svssno.c
 *   (C) 2004 Dominick Meglio (codemastr) and the UnrealIRCd Team
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

DLLFUNC int m_svssno(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_svs2sno(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSSNO 	"SVSSNO"	
#define TOK_SVSSNO	"BV"	
#define MSG_SVS2SNO 	"SVS2SNO"	
#define TOK_SVS2SNO	"BW"	

ModuleHeader MOD_HEADER(m_svssno)
  = {
	"m_svssno",
	"$Id$",
	"command /svssno", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_svssno)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSSNO, TOK_SVSSNO, m_svssno, MAXPARA);
	add_Command(MSG_SVS2SNO, TOK_SVS2SNO, m_svs2sno, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svssno)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svssno)(int module_unload)
{
	if (del_Command(MSG_SVSSNO, TOK_SVSSNO, m_svssno) < 0 || del_Command(MSG_SVS2SNO, TOK_SVS2SNO, m_svs2sno) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svssno).name);
	}
	return MOD_SUCCESS;
}

/*
 * do_svssno() 
 * parv[0] - sender
 * parv[1] - username to change snomask for
 * parv[2] - snomasks to change
 * show_change determines whether to show the change to the user
 */
int  do_svssno(aClient *cptr, aClient *sptr, int parc, char *parv[], int show_change)
{
	char *p;
	aClient *acptr;
	int what = MODE_ADD, i;

	if (!IsULine(sptr))
		return 0;

	if (parc < 2)
		return 0;

	if (parv[1][0] == '#') 
		return 0;

	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	if (hunt_server_token(cptr, sptr,
	                      show_change ? MSG_SVS2SNO : MSG_SVSSNO,
	                      show_change ? TOK_SVS2SNO : TOK_SVSSNO,
	                      "%s %s", 1, parc, parv) != HUNTED_ISME)
	{
		return 0;
	}

	if (MyClient(acptr))
	{
		if (parc == 2)
			acptr->user->snomask = 0;
		else
		{
			for (p = parv[2]; p && *p; p++) {
				switch (*p) {
					case '+':
						what = MODE_ADD;
						break;
					case '-':
						what = MODE_DEL;
						break;
					default:
				 	 for (i = 0; i <= Snomask_highest; i++)
				 	 {
				 	 	if (!Snomask_Table[i].flag)
				 	 		continue;
		 	 			if (*p == Snomask_Table[i].flag)
				 	 	{
				 	 		if (what == MODE_ADD)
					 	 		acptr->user->snomask |= Snomask_Table[i].mode;
			 			 	else
			 	 				acptr->user->snomask &= ~Snomask_Table[i].mode;
				 	 	}
				 	 }				
				}
			}
		}
	}

	if (show_change)
		sendto_one(acptr, rpl_str(RPL_SNOMASK), me.name, acptr->name, get_sno_str(acptr));

	return 0;
}

CMD_FUNC(m_svssno)
{
	return do_svssno(cptr, sptr, parc, parv, 0);
}

CMD_FUNC(m_svs2sno)
{
	return do_svssno(cptr, sptr, parc, parv, 1);
}
