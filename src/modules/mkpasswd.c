/*
 *   IRC - Internet Relay Chat, src/modules/m_mkpasswd.c
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

CMD_FUNC(m_mkpasswd);

#define MSG_MKPASSWD 	"MKPASSWD"	

ModuleHeader MOD_HEADER(mkpasswd)
  = {
	"mkpasswd",
	"5.0",
	"command /mkpasswd", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(mkpasswd)
{
	CommandAdd(modinfo->handle, MSG_MKPASSWD, m_mkpasswd, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(mkpasswd)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(mkpasswd)
{
	return MOD_SUCCESS;
}

/*
** m_mkpasswd
**      parv[1] = password to encrypt
*/
CMD_FUNC(m_mkpasswd)
{
	short	type;
	char	*result = NULL;

	if (!MKPASSWD_FOR_EVERYONE && !IsOper(sptr))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return -1;
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
		return 0;
	}
	/* Don't want to take any risk ;p. -- Syzop */
	if (strlen(parv[2]) > 64)
	{
		sendnotice(sptr, "*** Your parameter (text-to-hash) is too long.");
		return 0;
	}
	if ((type = Auth_FindType(NULL, parv[1])) == -1)
	{
		sendnotice(sptr, "*** %s is not an enabled authentication method", parv[1]);
		return 0;
	}

	if ((type == AUTHTYPE_MD5) || (type == AUTHTYPE_SHA1) ||
	    (type == AUTHTYPE_RIPEMD160) || (type == AUTHTYPE_UNIXCRYPT))
	{
		sendnotice(sptr, "ERROR: Deprecated authentication type '%s'. Use 'argon2' instead. "
		           "See https://www.unrealircd.org/docs/Authentication_types for the complete list.",
		           parv[1]);
		return 0;
	}

	if (!(result = Auth_Hash(type, parv[2])))
	{
		sendnotice(sptr, "*** Authentication method %s failed", parv[1]);
		return 0;
	}

	sendnotice(sptr, "*** Authentication phrase (method=%s, para=%s) is: %s",
		parv[1], parv[2], result);

	return 0;
}
