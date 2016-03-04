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

ModuleHeader MOD_HEADER(m_chgident)
  = {
	"chgident",	/* Name of module */
	"4.0", /* Version */
	"/chgident", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_chgident)
{
	CommandAdd(modinfo->handle, MSG_CHGIDENT, m_chgident, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_chgident)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_chgident)
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
	aClient *acptr;
	char *s;
	int legalident = 1;

	if (!ValidatePermissionsForPath("client:ident",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}


#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, err_str(ERR_DISABLED), me.name, sptr->name, "CHGIDENT",
			"This command is disabled on this server");
		return 0;
	}
#endif

	if ((parc < 3) || !*parv[2])
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "CHGIDENT");
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
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, err_str(ERR_DISABLED), me.name, sptr->name, "CHGIDENT",
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
				rejoin_leave(acptr);
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



		sendto_server(cptr, 0, 0, ":%s CHGIDENT %s %s",
		    sptr->name, acptr->name, parv[2]);
		ircsnprintf(acptr->user->username, sizeof(acptr->user->username), "%s", parv[2]);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
			rejoin_joinandmode(acptr);
		return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name,
		    parv[1]);
		return 0;
	}
	return 0;
}
