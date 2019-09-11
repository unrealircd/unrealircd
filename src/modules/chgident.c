/*
 *   IRC - Internet Relay Chat, src/modules/m_chgident.c
 *   (C) 1999-2001 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
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

#define MSG_CHGIDENT 	"CHGIDENT"

CMD_FUNC(m_chgident);

ModuleHeader MOD_HEADER(chgident)
  = {
	"chgident",	/* Name of module */
	"5.0", /* Version */
	"/chgident", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(chgident)
{
	CommandAdd(modinfo->handle, MSG_CHGIDENT, m_chgident, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(chgident)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(chgident)
{
	return MOD_SUCCESS;
}

/* 
 * m_chgident - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGIDENT <nick> <new identname>
 * parv[1] - nickname
 * parv[2] - identname
 *
*/

CMD_FUNC(m_chgident)
{
	Client *acptr;
	char *s;
	int legalident = 1;

	if (!ValidatePermissionsForPath("client:set:ident",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}


	if ((parc < 3) || !*parv[2])
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "CHGIDENT");
		return 0;
	}

	if (strlen(parv[2]) > (USERLEN))
	{
		sendnotice(sptr, "*** ChgIdent Error: Requested ident too long -- rejected.");
		return 0;
	}

	/* illegal?! */
	for (s = parv[2]; *s; s++)
	{
		if ((*s == '~') && (s == parv[2]))
			continue;
		if (!isallowed(*s))
		{
			legalident = 0;
		}
	}

	if (legalident == 0)
	{
		sendnotice(sptr, "*** /ChgIdent Error: A ident may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them");
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		userhost_save_current(acptr);

		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendnumeric(sptr, ERR_DISABLED, "CHGIDENT",
						"This command is disabled on this server");
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (IsPerson(acptr) && MyClient(sptr) && acptr->user->joined)
				{
					sendnotice(sptr, "*** /ChgIdent can not be used while %s is on a channel", acptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				/* join sent later when the ident has been changed */
				break;
		}
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the virtual ident of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    GetHost(acptr), parv[2]);
			/* Logging ability added by XeRXeS */
			ircd_log(LOG_CHGCMDS,
				"CHGIDENT: %s changed the virtual ident of %s (%s@%s) to be %s",
				sptr->name, acptr->name, acptr->user->username,    
				GetHost(acptr), parv[2]);
		}

		sendto_server(cptr, 0, 0, NULL, ":%s CHGIDENT %s %s",
		    sptr->name, acptr->name, parv[2]);
		ircsnprintf(acptr->user->username, sizeof(acptr->user->username), "%s", parv[2]);

		userhost_changed(acptr);
		return 0;
	}
	else
	{
		sendnumeric(sptr, ERR_NOSUCHNICK,
		    parv[1]);
		return 0;
	}
	return 0;
}
