/*
 *   IRC - Internet Relay Chat, src/modules/mkpasswd.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   mkpasswd command
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

CMD_FUNC(cmd_mkpasswd);

#define MSG_MKPASSWD 	"MKPASSWD"	

ModuleHeader MOD_HEADER
  = {
	"mkpasswd",
	"5.0",
	"command /mkpasswd", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_MKPASSWD, cmd_mkpasswd, MAXPARA, CMD_USER);
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
** cmd_mkpasswd
**      parv[1] = password to encrypt
*/
CMD_FUNC(cmd_mkpasswd)
{
	short	type;
	char	*result = NULL;

	if (!MKPASSWD_FOR_EVERYONE && !IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return;
	}
	if (!IsOper(sptr))
	{
		/* Non-opers /mkpasswd usage: lag them up, and send a notice to eyes snomask.
		 * This notice is always sent, even in case of bad usage/bad auth methods/etc.
		 */
		sptr->local->since += 7;
		sendto_snomask(SNO_EYES, "*** /mkpasswd used by %s (%s@%s)",
			sptr->name, sptr->user->username, GetHost(sptr));
	}

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendnotice(sptr, "*** Syntax: /mkpasswd <authmethod> :parameter");
		return;
	}
	/* Don't want to take any risk ;p. -- Syzop */
	if (strlen(parv[2]) > 64)
	{
		sendnotice(sptr, "*** Your parameter (text-to-hash) is too long.");
		return;
	}
	if ((type = Auth_FindType(NULL, parv[1])) == -1)
	{
		sendnotice(sptr, "*** %s is not an enabled authentication method", parv[1]);
		return;
	}

	if (!(result = Auth_Hash(type, parv[2])))
	{
		sendnotice(sptr, "*** Authentication method %s failed", parv[1]);
		return;
	}

	sendnotice(sptr, "*** Authentication phrase (method=%s, para=%s) is: %s",
		parv[1], parv[2], result);
}
