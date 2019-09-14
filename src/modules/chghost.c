/*
 *   IRC - Internet Relay Chat, src/modules/chghost.c
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

#define MSG_CHGHOST 	"CHGHOST"

CMD_FUNC(cmd_chghost);

ModuleHeader MOD_HEADER
  = {
	"chghost",	/* Name of module */
	"5.0", /* Version */
	"/chghost", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_CHGHOST, cmd_chghost, MAXPARA, M_USER|M_SERVER);
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
 * cmd_chghost - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new hostname>
 * parv[1] - nickname
 * parv[2] - hostname
 *
*/

CMD_FUNC(cmd_chghost)
{
	Client *acptr;

	if (MyUser(sptr) && !ValidatePermissionsForPath("client:set:host",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_NOPRIVILEGES);
		return 0;
	}

	if ((parc < 3) || !*parv[2])
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "CHGHOST");
		return 0;
	}

	if (strlen(parv[2]) > (HOSTLEN))
	{
		sendnotice(sptr, "*** ChgName Error: Requested hostname too long -- rejected.");
		return 0;
	}

	if (!valid_host(parv[2]))
	{
		sendnotice(sptr, "*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them");
		return 0;
	}

	if (parv[2][0] == ':')
	{
		sendnotice(sptr, "*** A hostname cannot start with ':'");
		return 0;
	}

	if ((acptr = find_person(parv[1], NULL)))
	{
		if (!strcmp(GetHost(acptr), parv[2]))
		{
			sendnotice(sptr, "*** /ChgHost Error: requested host is same as current host.");
			return 0;
		}

		userhost_save_current(acptr);

		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyUser(sptr))
				{
					sendnumeric(sptr, ERR_DISABLED, "CHGHOST",
						"This command is disabled on this server");
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (IsUser(acptr) && MyUser(sptr) && acptr->user->joined)
				{
					sendnotice(sptr, "*** /ChgHost can not be used while %s is on a channel", acptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				/* rejoin sent later when the host has been changed */
				break;
		}
				
		if (!IsULine(sptr))
		{
			sendto_snomask(SNO_EYES,
			    "%s changed the virtual hostname of %s (%s@%s) to be %s",
			    sptr->name, acptr->name, acptr->user->username,
			    acptr->user->realhost, parv[2]);
			/* Logging added by XeRXeS */
 		      	ircd_log(LOG_CHGCMDS,                                         
				"CHGHOST: %s changed the virtual hostname of %s (%s@%s) to be %s",
				sptr->name, acptr->name, acptr->user->username, acptr->user->realhost, parv[2]); 
		}
 
                  
		acptr->umodes |= UMODE_HIDE;
		acptr->umodes |= UMODE_SETHOST;
		sendto_server(cptr, 0, 0, NULL, ":%s CHGHOST %s %s",
		    sptr->name, acptr->name, parv[2]);
		if (acptr->user->virthost)
		{
			safe_free(acptr->user->virthost);
			acptr->user->virthost = 0;
		}
		acptr->user->virthost = strdup(parv[2]);
		
		userhost_changed(acptr);

		if (MyUser(acptr))
			sendnumeric(acptr, RPL_HOSTHIDDEN, parv[2]);
		
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
