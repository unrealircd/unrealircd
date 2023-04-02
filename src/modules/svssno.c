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
	"unrealircd-6",
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
void do_svssno(Client *client, MessageTag *recv_mtags, int parc, const char *parv[], int show_change)
{
	const char *p;
	Client *target;
	int what = MODE_ADD, i;

	if (!IsSvsCmdOk(client))
		return;

	if (parc < 2)
		return;

	if (parv[1][0] == '#') 
		return;

	if (!(target = find_user(parv[1], NULL)))
		return;

	if (hunt_server(client, recv_mtags, show_change ? "SVS2SNO" : "SVSSNO", 1, parc, parv) != HUNTED_ISME)
		return;

	if (MyUser(target))
	{
		if (parc == 2)
			set_snomask(target, NULL);
		else
			set_snomask(target, parv[2]);
	}

	if (show_change && target->user->snomask)
	{
		MessageTag *mtags = NULL;
		new_message(client, recv_mtags, &mtags);
		// TODO: sendnumeric has no mtags support :D
		sendnumeric(target, RPL_SNOMASK, target->user->snomask);
		safe_free_message_tags(mtags);
	}
}

CMD_FUNC(cmd_svssno)
{
	do_svssno(client, recv_mtags, parc, parv, 0);
}

CMD_FUNC(cmd_svs2sno)
{
	do_svssno(client, recv_mtags, parc, parv, 1);
}
