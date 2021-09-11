/*
 *   IRC - Internet Relay Chat, src/modules/pass.c
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

#include "unrealircd.h"

CMD_FUNC(cmd_pass);

#define MSG_PASS 	"PASS"	

ModuleHeader MOD_HEADER
  = {
	"pass",
	"5.0",
	"command /pass", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* Forward declarations */
int _check_banned(Client *client, int exitflags);

MOD_TEST()
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAdd(modinfo->handle, EFUNC_CHECK_BANNED, _check_banned);
	
	return MOD_SUCCESS;
}

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PASS, cmd_pass, 1, CMD_UNREGISTERED|CMD_USER|CMD_SERVER);
	
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

/** Handles zlines/gzlines/throttling/unknown connections
 * @param client     Client to be checked
 * @param exitflags  Special flag (NO_EXIT_CLIENT) -- only used in very early stages of the connection
 * @returns 1 if user is banned and is or should be killed, 0 if not.
 */
int _check_banned(Client *client, int exitflags)
{
	TKL *tk;

	if ((tk = find_tkline_match_zap(client)))
	{
		banned_client(client, "Z-Lined", tk->ptr.serverban->reason, (tk->type & TKL_GLOBAL)?1:0, exitflags);
		return 1;
	}
	else
	{
		int val;
		char zlinebuf[512];

		if (!(val = throttle_can_connect(client)))
		{
			if (exitflags & NO_EXIT_CLIENT)
			{
				ircsnprintf(zlinebuf, sizeof(zlinebuf),
					"ERROR :Closing Link: [%s] (Throttled: Reconnecting too fast) - "
					"Email %s for more information.\r\n",
					client->ip, KLINE_ADDRESS);
				(void)send(client->local->fd, zlinebuf, strlen(zlinebuf), 0);
				return 1;
			} else {
				ircsnprintf(zlinebuf, sizeof(zlinebuf),
				            "Throttled: Reconnecting too fast - "
				            "Email %s for more information.",
				            KLINE_ADDRESS);
				exit_client(client, NULL, zlinebuf);
				return 1;
			}
		}
		else if (val == 1)
			add_throttling_bucket(client);
	}

	return 0;
}

/***************************************************************************
 * cmd_pass() - Added Sat, 4 March 1989
 ***************************************************************************/
/*
** cmd_pass
**	parv[1] = password
*/
CMD_FUNC(cmd_pass)
{
	const char *password = parc > 1 ? parv[1] : NULL;

	if (!MyConnect(client) || (!IsUnknown(client) && !IsHandshake(client)))
	{
		sendnumeric(client, ERR_ALREADYREGISTRED);
		return;
	}

	if (BadPtr(password))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "PASS");
		return;
	}

	/* Store the password */
	safe_strldup(client->local->passwd, password, PASSWDLEN+1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturn(HOOKTYPE_LOCAL_PASS, !=0, client, password);
}
