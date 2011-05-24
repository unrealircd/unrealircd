/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnolag.c
 *   (C) 2006 Alex Berezhnyak and djGrrr
 *
 *   Fake lag exception - SVSNOLAG and SVS2NOLAG commands
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

DLLFUNC int m_svsnolag(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_svs2nolag(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSNOLAG 	"SVSNOLAG"	
#define TOK_SVSNOLAG 	"sl"
#define MSG_SVS2NOLAG 	"SVS2NOLAG"	
#define TOK_SVS2NOLAG 	"SL"

ModuleHeader MOD_HEADER(m_svsnolag)
  = {
	"m_svsnolag",
	"$Id$",
	"commands /svsnolag and /svs2nolag", 
	"3.2-b8-1",
	NULL
    };

DLLFUNC int MOD_INIT(m_svsnolag)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSNOLAG, TOK_SVSNOLAG, m_svsnolag, MAXPARA);
	add_Command(MSG_SVS2NOLAG, TOK_SVS2NOLAG, m_svs2nolag, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsnolag)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsnolag)(int module_unload)
{
	if (del_Command(MSG_SVSNOLAG, TOK_SVSNOLAG, m_svsnolag) < 0 || del_Command(MSG_SVS2NOLAG, TOK_SVS2NOLAG, m_svs2nolag) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svsnolag).name);
	}
	return MOD_SUCCESS;
}

int do_svsnolag(aClient *cptr, aClient *sptr, int parc, char *parv[], int show_change)
{
	aClient *acptr;
	char *cmd = show_change ? MSG_SVS2NOLAG : MSG_SVSNOLAG;


	if (!IsULine(sptr))
		return 0;

	if (parc < 3)
		return 0;

	if (!(acptr = find_person(parv[2], (aClient *)NULL)))
		return 0;

	if (!MyClient(acptr))
	{
		sendto_one(acptr, ":%s %s %s %s", parv[0], cmd, parv[1], parv[2]);
		return 0;
	}

	if (*parv[1] == '+')
	{
		if (!IsNoFakeLag(acptr))
		{
			SetNoFakeLag(acptr);
			if (show_change)
				sendnotice(acptr, "You are now exempted from fake lag");
		}
	}
	if (*parv[1] == '-')
	{
		if (IsNoFakeLag(acptr))
		{
			ClearNoFakeLag(acptr);
			if (show_change)
				sendnotice(acptr, "You are no longer exempted from fake lag");
		}
	}

	return 0;
}


int m_svsnolag(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return do_svsnolag(cptr, sptr, parc, parv, 0);
}

int m_svs2nolag(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	return do_svsnolag(cptr, sptr, parc, parv, 1);
}
