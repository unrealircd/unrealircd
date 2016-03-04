/*
 *   IRC - Internet Relay Chat, src/modules/m_chghost.c
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

CMD_FUNC(m_chghost);

ModuleHeader MOD_HEADER(m_chghost)
  = {
	"chghost",	/* Name of module */
	"4.0", /* Version */
	"/chghost", /* Short description of module */
	"3.2-b8-1",
    };

MOD_INIT(m_chghost)
{
	CommandAdd(modinfo->handle, MSG_CHGHOST, m_chghost, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_chghost)
{
	return MOD_SUCCESS;
	
}

MOD_UNLOAD(m_chghost)
{
	return MOD_SUCCESS;	
}

/* 
 * m_chghost - 12/07/1999 (two months after I made SETIDENT) - Stskeeps
 * :prefix CHGHOST <nick> <new hostname>
 * parv[1] - nickname
 * parv[2] - hostname
 *
*/

CMD_FUNC(m_chghost)
{
	aClient *acptr;

	if (MyClient(sptr) && !ValidatePermissionsForPath("client:host",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

#ifdef DISABLE_USERMOD
	if (MyClient(sptr))
	{
		sendto_one(sptr, err_str(ERR_DISABLED), me.name, sptr->name, "CHGHOST",
			"This command is disabled on this server");
		return 0;
	}
#endif

	if ((parc < 3) || !*parv[2])
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "CHGHOST");
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
		switch (UHOST_ALLOWED)
		{
			case UHALLOW_NEVER:
				if (MyClient(sptr))
				{
					sendto_one(sptr, err_str(ERR_DISABLED), me.name, sptr->name, "CHGHOST",
						"This command is disabled on this server");
					return 0;
				}
				break;
			case UHALLOW_ALWAYS:
				break;
			case UHALLOW_NOCHANS:
				if (IsPerson(acptr) && MyClient(sptr) && acptr->user->joined)
				{
					sendnotice(sptr, "*** /ChgHost can not be used while %s is on a channel", acptr->name);
					return 0;
				}
				break;
			case UHALLOW_REJOIN:
				rejoin_leave(acptr);
				/* join sent later when the host has been changed */
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
		sendto_server(cptr, 0, 0, ":%s CHGHOST %s %s",
		    sptr->name, acptr->name, parv[2]);
		if (acptr->user->virthost)
		{
			MyFree(acptr->user->virthost);
			acptr->user->virthost = 0;
		}
		acptr->user->virthost = strdup(parv[2]);
		if (UHOST_ALLOWED == UHALLOW_REJOIN)
			rejoin_joinandmode(acptr);
		
		if (MyClient(acptr))
			sendto_one(acptr, err_str(RPL_HOSTHIDDEN), me.name, acptr->name, parv[2]);
		
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
