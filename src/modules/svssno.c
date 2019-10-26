/*
 *   IRC - Internet Relay Chat, src/modules/svssno.c
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

#include "unrealircd.h"

CMD_FUNC(cmd_svssno);
CMD_FUNC(cmd_svs2sno);

#define MSG_SVSSNO 	"SVSSNO"	
#define MSG_SVS2SNO 	"SVS2SNO"	

ModuleHeader MOD_HEADER
  = {
	"svssno",
	"5.0",
	"command /svssno", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSSNO, cmd_svssno, MAXPARA, CMD_USER|CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_SVS2SNO, cmd_svs2sno, MAXPARA, CMD_USER|CMD_SERVER);
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
 * do_svssno() 
 * parv[1] - username to change snomask for
 * parv[2] - snomasks to change
 * show_change determines whether to show the change to the user
 */
void do_svssno(Client *client, int parc, char *parv[], int show_change)
{
	char *p;
	Client *target;
	int what = MODE_ADD, i;

	if (!IsULine(client))
		return;

	if (parc < 2)
		return;

	if (parv[1][0] == '#') 
		return;

	if (!(target = find_person(parv[1], NULL)))
		return;

	if (hunt_server(client, NULL,
	                      show_change ? ":%s SVS2SNO %s %s" : ":%s SVSSNO %s %s",
	                      1, parc, parv) != HUNTED_ISME)
	{
		return;
	}

	if (MyUser(target))
	{
		if (parc == 2)
			target->user->snomask = 0;
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
					 	 		target->user->snomask |= Snomask_Table[i].mode;
			 			 	else
			 	 				target->user->snomask &= ~Snomask_Table[i].mode;
				 	 	}
				 	 }				
				}
			}
		}
	}

	if (show_change)
		sendnumeric(target, RPL_SNOMASK, get_snomask_string(target));
}

CMD_FUNC(cmd_svssno)
{
	do_svssno(client, parc, parv, 0);
}

CMD_FUNC(cmd_svs2sno)
{
	do_svssno(client, parc, parv, 1);
}
