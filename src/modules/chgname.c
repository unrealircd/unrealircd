/*
 *   Unreal Internet Relay Chat Daemon, src/modules/chgname.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

#define MSG_CHGNAME     "CHGNAME"
#define MSG_SVSNAME     "SVSNAME"

CMD_FUNC(cmd_chgname);

ModuleHeader MOD_HEADER
  = {
	"chgname",	/* Name of module */
	"5.0", /* Version */
	"command /chgname", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };


/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_CHGNAME, cmd_chgname, 2, CMD_USER|CMD_SERVER);
	CommandAdd(modinfo->handle, MSG_SVSNAME, cmd_chgname, 2, CMD_USER|CMD_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}


/*
 * cmd_chgname - Tue May 23 13:06:35 BST 200 (almost a year after I made CHGIDENT) - Stskeeps
 * :prefix CHGNAME <nick> <new realname>
 * parv[1] - nickname
 * parv[2] - realname
 *
*/
CMD_FUNC(cmd_chgname)
{
	Client *target;
	ConfigItem_ban *bconf;

	if (!ValidatePermissionsForPath("client:set:name",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if ((parc < 3) || !*parv[2])
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "CHGNAME");
		return;
	}

	if (strlen(parv[2]) > (REALLEN))
	{
		sendnotice(client, "*** ChgName Error: Requested realname too long -- rejected.");
		return;
	}

	if (!(target = find_user(parv[1], NULL)))
	{
		sendnumeric(client, ERR_NOSUCHNICK, parv[1]);
		return;
	}

	/* Let's log this first */
	if (!IsULine(client))
	{
		const char *issuer = command_issued_by_rpc(recv_mtags);
		if (issuer)
		{
			unreal_log(ULOG_INFO, "chgcmds", "CHGNAME_COMMAND", client,
				   "CHGNAME: $issuer changed the realname of $target.details to be $new_realname",
				   log_data_string("issuer", issuer),
				   log_data_string("change_type", "realname"),
				   log_data_client("target", target),
				   log_data_string("new_realname", parv[2]));
		} else {
			unreal_log(ULOG_INFO, "chgcmds", "CHGNAME_COMMAND", client,
				   "CHGNAME: $client changed the realname of $target.details to be $new_realname",
				   log_data_string("change_type", "realname"),
				   log_data_client("target", target),
				   log_data_string("new_realname", parv[2]));
		}
	}

	/* set the realname to make ban checking work */
	ircsnprintf(target->info, sizeof(target->info), "%s", parv[2]);

	if (MyUser(target))
	{
		/* only check for realname bans if the person who's name is being changed is NOT an oper */
		if (!ValidatePermissionsForPath("immune:server-ban:ban-realname",target,NULL,NULL,NULL) &&
		    ((bconf = find_ban(NULL, target->info, CONF_BAN_REALNAME))))
		{
			banned_client(target, "realname", bconf->reason?bconf->reason:"", 0, 0);
			return;
		}
	}

	sendto_server(client, 0, 0, recv_mtags, ":%s CHGNAME %s :%s",
	    client->id, target->name, parv[2]);
}
