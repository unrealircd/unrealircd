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

#include "unrealircd.h"

CMD_FUNC(m_svsnolag);
CMD_FUNC(m_svs2nolag);

#define MSG_SVSNOLAG 	"SVSNOLAG"	
#define MSG_SVS2NOLAG 	"SVS2NOLAG"	

ModuleHeader MOD_HEADER(m_svsnolag)
  = {
	"m_svsnolag",
	"4.0",
	"commands /svsnolag and /svs2nolag", 
	"3.2-b8-1",
	NULL
    };

MOD_INIT(m_svsnolag)
{
	CommandAdd(modinfo->handle, MSG_SVSNOLAG, m_svsnolag, MAXPARA, M_SERVER);
	CommandAdd(modinfo->handle, MSG_SVS2NOLAG, m_svs2nolag, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_svsnolag)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_svsnolag)
{
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
		sendto_one(acptr, ":%s %s %s %s", sptr->name, cmd, parv[1], parv[2]);
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

CMD_FUNC(m_svsnolag)
{
	return do_svsnolag(cptr, sptr, parc, parv, 0);
}

CMD_FUNC(m_svs2nolag)
{
	return do_svsnolag(cptr, sptr, parc, parv, 1);
}
