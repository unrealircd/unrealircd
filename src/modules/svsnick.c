/*
 *   IRC - Internet Relay Chat, src/modules/svsnick.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnick command
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

CMD_FUNC(cmd_svsnick);

#define MSG_SVSNICK 	"SVSNICK"	

ModuleHeader MOD_HEADER
  = {
	"svsnick",
	"5.0",
	"command /svsnick", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSNICK, cmd_svsnick, MAXPARA, CMD_SERVER);
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
** cmd_svsnick
**      parv[1] = old nickname
**      parv[2] = new nickname
**      parv[3] = timestamp
*/
CMD_FUNC(cmd_svsnick)
{
	Client *acptr;
	Client *ocptr; /* Other client */
	MessageTag *mtags = NULL;
	char nickname[NICKLEN+1];
	char oldnickname[NICKLEN+1];
	time_t ts;

	if (!IsSvsCmdOk(client) || parc < 4 || (strlen(parv[2]) > NICKLEN))
		return; /* This looks like an error anyway -Studded */

	if (hunt_server(client, NULL, "SVSNICK", 1, parc, parv) != HUNTED_ISME)
		return; /* Forwarded, done */

	strlcpy(nickname, parv[2], sizeof(nickname));
	if (do_nick_name(nickname) == 0)
		return;

	if (!(acptr = find_user(parv[1], NULL)))
		return; /* User not found, bail out */

	if ((ocptr = find_client(nickname, NULL)) && ocptr != acptr) /* Collision */
	{
		exit_client(acptr, NULL,
		                   "Nickname collision due to forced "
		                   "nickname change, your nick was overruled");
		return;
	}

	/* if the new nickname is identical to the old one, ignore it */
	if (!strcmp(acptr->name, nickname))
		return;

	strlcpy(oldnickname, acptr->name, sizeof(oldnickname));

	if (acptr != ocptr)
		acptr->umodes &= ~UMODE_REGNICK;
	ts = atol(parv[3]);

	/* no 'recv_mtags' here, we do not inherit from SVSNICK but generate a new NICK event */
	new_message(acptr, NULL, &mtags);
	mtag_add_issued_by(&mtags, client, recv_mtags);
	RunHook(HOOKTYPE_LOCAL_NICKCHANGE, acptr, mtags, nickname);
	sendto_local_common_channels(acptr, acptr, 0, mtags, ":%s NICK :%s", acptr->name, nickname);
	sendto_one(acptr, mtags, ":%s NICK :%s", acptr->name, nickname);
	sendto_server(NULL, 0, 0, mtags, ":%s NICK %s :%lld", acptr->id, nickname, (long long)ts);

	add_history(acptr, 1, WHOWAS_EVENT_NICK_CHANGE);
	acptr->lastnick = ts; /* needs to be done AFTER add_history() */
	del_from_client_hash_table(acptr->name, acptr);

	unreal_log(ULOG_INFO, "nick", "FORCED_NICK_CHANGE", acptr,
	           "$client.details has been forced to change their nickname to $new_nick_name",
	           log_data_string("new_nick_name", nickname));

	strlcpy(acptr->name, nickname, sizeof acptr->name);
	add_to_client_hash_table(nickname, acptr);
	RunHook(HOOKTYPE_POST_LOCAL_NICKCHANGE, acptr, mtags, oldnickname);
	free_message_tags(mtags);
}
