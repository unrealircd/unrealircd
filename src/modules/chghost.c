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
	CommandAdd(modinfo->handle, MSG_CHGHOST, cmd_chghost, MAXPARA, CMD_USER|CMD_SERVER);
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
 * parv[1] - target user
 * parv[2] - hostname
 *
*/

CMD_FUNC(cmd_chghost)
{
	Client *target;

	if (MyUser(client) && !ValidatePermissionsForPath("client:set:host",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "CHGHOST");
		return;
	}

	if (strlen(parv[2]) > (HOSTLEN))
	{
		sendnotice(client, "*** ChgName Error: Requested hostname too long -- rejected.");
		return;
	}

	if (!valid_host(parv[2]))
	{
		sendnotice(client, "*** /ChgHost Error: A hostname may contain a-z, A-Z, 0-9, '-' & '.' - Please only use them");
		return;
	}

	if (parv[2][0] == ':')
	{
		sendnotice(client, "*** A hostname cannot start with ':'");
		return;
	}

	if (!(target = find_person(parv[1], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	if (!strcmp(GetHost(target), parv[2]))
	{
		sendnotice(client, "*** /ChgHost Error: requested host is same as current host.");
		return;
	}

	userhost_save_current(target);

	switch (UHOST_ALLOWED)
	{
		case UHALLOW_NEVER:
			if (MyUser(client))
			{
				sendnumeric(client, ERR_DISABLED, "CHGHOST",
					"This command is disabled on this server");
				return;
			}
			break;
		case UHALLOW_ALWAYS:
			break;
		case UHALLOW_NOCHANS:
			if (IsUser(target) && MyUser(client) && target->user->joined)
			{
				sendnotice(client, "*** /ChgHost can not be used while %s is on a channel", target->name);
				return;
			}
			break;
		case UHALLOW_REJOIN:
			/* rejoin sent later when the host has been changed */
			break;
	}

	if (!IsULine(client))
	{
		sendto_snomask(SNO_EYES,
		    "%s changed the virtual hostname of %s (%s@%s) to be %s",
		    client->name, target->name, target->user->username,
		    target->user->realhost, parv[2]);
		/* Logging added by XeRXeS */
		ircd_log(LOG_CHGCMDS,                                         
			"CHGHOST: %s changed the virtual hostname of %s (%s@%s) to be %s",
			client->name, target->name, target->user->username, target->user->realhost, parv[2]); 
	}

	target->umodes |= UMODE_HIDE;
	target->umodes |= UMODE_SETHOST;
	sendto_server(client, 0, 0, NULL, ":%s CHGHOST %s %s", client->id, target->id, parv[2]);
	safe_strdup(target->user->virthost, parv[2]);
	
	userhost_changed(target);

	if (MyUser(target))
		sendnumeric(target, RPL_HOSTHIDDEN, parv[2]);
}
