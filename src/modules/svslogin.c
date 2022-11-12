/*
 *   IRC - Internet Relay Chat, src/modules/svslogin.c
 *   (C) 2022 The UnrealIRCd Team
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

#define MSG_SVSLOGIN "SVSLOGIN"

CMD_FUNC(cmd_svslogin);

ModuleHeader MOD_HEADER
  = {
	"svslogin",
	"6.0",
	"command /SVSLOGIN", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SVSLOGIN, cmd_svslogin, MAXPARA, CMD_USER|CMD_SERVER);
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
 * SVSLOGIN message
 *
 * parv[1]: propagation mask
 * parv[2]: target
 * parv[3]: account name (SVID)
 */
CMD_FUNC(cmd_svslogin)
{
	Client *target;

	if (MyUser(client) || (parc < 3) || !parv[3])
		return;

	/* We actually ignore parv[1] since this is a broadcast message.
	 * It is a broadcast message because we want ALL servers to know
	 * that the user is now logged in under account xyz.
	 */

	target = find_client(parv[2], NULL);
	if (target)
	{
		if (IsServer(target))
			return;

		if (target->user == NULL)
			make_user(target);

		strlcpy(target->user->account, parv[3], sizeof(target->user->account));
		user_account_login(recv_mtags, target);
		if (MyConnect(target) && IsDead(target))
			return; /* was killed due to *LINE on ~a probably */
	} else {
		/* It is perfectly normal for target to be NULL as this
		 * happens during registration phase (pre-connect).
		 * It just means we cannot set any properties for this user,
		 * which is fine in that case, since it will be synced via
		 * the UID message instead.
		 * We still have to broadcast the message, which is why
		 * we do not return here.
		 */
	}

	/* Propagate to the rest of the network */
	sendto_server(client, 0, 0, NULL, ":%s SVSLOGIN %s %s %s",
	              client->name, parv[1], parv[2], parv[3]);
}