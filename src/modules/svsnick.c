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
	"unrealircd-5",
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

	if (!IsULine(client) || parc < 4 || (strlen(parv[2]) > NICKLEN))
		return; /* This looks like an error anyway -Studded */

	if (hunt_server(client, NULL, ":%s SVSNICK %s %s :%s", 1, parc, parv) != HUNTED_ISME)
		return; /* Forwarded, done */

	if (do_nick_name(parv[2]) == 0)
		return;

	if (!(acptr = find_person(parv[1], NULL)))
		return; /* User not found, bail out */

	if ((ocptr = find_client(parv[2], NULL)) && ocptr != acptr) /* Collision */
	{
		exit_client(acptr, NULL,
		                   "Nickname collision due to Services enforced "
		                   "nickname change, your nick was overruled");
		return;
	}

	/* if the new nickname is identical to the old one, ignore it */
	if (!strcmp(acptr->name, parv[2]))
		return;

	if (acptr != ocptr)
		acptr->umodes &= ~UMODE_REGNICK;
	acptr->lastnick = atol(parv[3]);

	/* no 'recv_mtags' here, we do not inherit from SVSNICK but generate a new NICK event */
	new_message(acptr, NULL, &mtags);
	RunHook3(HOOKTYPE_LOCAL_NICKCHANGE, acptr, mtags, parv[2]);
	sendto_local_common_channels(acptr, acptr, 0, mtags, ":%s NICK :%s", acptr->name, parv[2]);
	sendto_one(acptr, mtags, ":%s NICK :%s", acptr->name, parv[2]);
	sendto_server(NULL, 0, 0, mtags, ":%s NICK %s :%ld", acptr->id, parv[2], atol(parv[3]));
	free_message_tags(mtags);

	add_history(acptr, 1);
	del_from_client_hash_table(acptr->name, acptr);
	hash_check_watch(acptr, RPL_LOGOFF);

	sendto_snomask(SNO_NICKCHANGE,
		"*** %s (%s@%s) has been forced to change their nickname to %s", 
		acptr->name, acptr->user->username, acptr->user->realhost, parv[2]);

	strlcpy(acptr->name, parv[2], sizeof acptr->name);
	add_to_client_hash_table(parv[2], acptr);
	hash_check_watch(acptr, RPL_LOGON);
}
