/*
 *   IRC - Internet Relay Chat, src/modules/m_pass.c
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

CMD_FUNC(m_pass);

#define MSG_PASS 	"PASS"	

ModuleHeader MOD_HEADER(pass)
  = {
	"pass",
	"5.0",
	"command /pass", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* Forward declarations */
int _check_banned(Client *cptr, int exitflags);

MOD_TEST(pass)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);

	EfunctionAdd(modinfo->handle, EFUNC_CHECK_BANNED, _check_banned);
	
	return MOD_SUCCESS;
}

MOD_INIT(pass)
{
	CommandAdd(modinfo->handle, MSG_PASS, m_pass, 1, M_UNREGISTERED|M_USER|M_SERVER);
	
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(pass)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(pass)
{
	return MOD_SUCCESS;
}

/** Handles zlines/gzlines/throttling/unknown connections */
int _check_banned(Client *cptr, int exitflags)
{
	TKL *tk;

	if ((tk = find_tkline_match_zap(cptr)))
	{
		return banned_client(cptr, "Z-Lined", tk->ptr.serverban->reason, (tk->type & TKL_GLOBAL)?1:0, exitflags);
	}
	else
	{
		int val;
		char zlinebuf[512];

		if (!(val = throttle_can_connect(cptr)))
		{
			if (exitflags & NO_EXIT_CLIENT)
			{
				ircsnprintf(zlinebuf, sizeof(zlinebuf),
					"ERROR :Closing Link: [%s] (Throttled: Reconnecting too fast) - "
					"Email %s for more information.\r\n",
					cptr->ip, KLINE_ADDRESS);
				(void)send(cptr->local->fd, zlinebuf, strlen(zlinebuf), 0);
				return FLUSH_BUFFER;
			} else {
				ircsnprintf(zlinebuf, sizeof(zlinebuf),
				            "Throttled: Reconnecting too fast - "
				            "Email %s for more information.",
				            KLINE_ADDRESS);
				return exit_client(cptr, cptr, &me, NULL, zlinebuf);
			}
		}
		else if (val == 1)
			add_throttling_bucket(cptr);
	}

	return 0;
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/
/*
** m_pass
**	parv[1] = password
*/
CMD_FUNC(m_pass)
{
	char *password = parc > 1 ? parv[1] : NULL;

	if (BadPtr(password))
	{
		sendnumeric(cptr, ERR_NEEDMOREPARAMS, "PASS");
		return 0;
	}
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	{
		sendnumeric(cptr, ERR_ALREADYREGISTRED);
		return 0;
	}

	/* Store the password */
	safestrldup(cptr->local->passwd, password, PASSWDLEN+1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturnInt2(HOOKTYPE_LOCAL_PASS, sptr, password, !=0);
	return 0;
}
