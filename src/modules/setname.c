/*
 *   IRC - Internet Relay Chat, src/modules/setname.c
 *   (c) 1999-2001 Dominick Meglio (codemastr) <codemastr@unrealircd.com>
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

CMD_FUNC(cmd_setname);
char *setname_isupport_param(void);

#define MSG_SETNAME 	"SETNAME"	/* setname */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

long CAP_SETNAME = 0L;

ModuleHeader MOD_HEADER
  = {
	"setname",	/* Name of module */
	"5.0", /* Version */
	"command /setname", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	ClientCapabilityInfo cap;
	ClientCapability *c;
	MARK_AS_OFFICIAL_MODULE(modinfo);

	CommandAdd(modinfo->handle, MSG_SETNAME, cmd_setname, 1, CMD_USER);

	memset(&cap, 0, sizeof(cap));
	cap.name = "setname";
	c = ClientCapabilityAdd(modinfo->handle, &cap, &CAP_SETNAME);
	if (!c)
	{
		config_error("[%s] Failed to request setname cap: %s", MOD_HEADER.name, ModuleGetErrorStr(modinfo->handle));
		return MOD_FAILED;
	}

	return MOD_SUCCESS;
}

MOD_LOAD()
{
	ISupportAdd(modinfo->handle, "NAMELEN", setname_isupport_param());
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

char *setname_isupport_param(void){
	return STR(REALLEN);
}

/* cmd_setname - 12/05/1999 - Stskeeps
 * :prefix SETNAME :gecos
 * parv[1] - gecos
 * D: This will set your gecos to be <x> (like (/setname :The lonely wanderer))
   this is now compatible with IRCv3 SETNAME --k4be
*/ 
CMD_FUNC(cmd_setname)
{
	int xx;
	char oldinfo[REALLEN + 1];
	char spamfilter_user[NICKLEN + USERLEN + HOSTLEN + REALLEN + 64];
	ConfigItem_ban *bconf;
	MessageTag *mtags = NULL;

	if ((parc < 2) || BadPtr(parv[1]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "SETNAME");
		return;
	}

	if (strlen(parv[1]) > REALLEN)
	{
		if (!MyConnect(client))
			return;
		if (HasCapabilityFast(client, CAP_SETNAME))
		{
			new_message(client, recv_mtags, &mtags);
			sendto_one(client, mtags, ":%s FAIL SETNAME INVALID_REALNAME :\"Real names\" may maximum be %i characters of length", me.name, REALLEN);
			free_message_tags(mtags);
		}
		else
		{
			sendnotice(client, "*** /SetName Error: \"Real names\" may maximum be %i characters of length", REALLEN);
		}
		return;
	}

	strlcpy(oldinfo, client->info, sizeof(oldinfo));

	if (MyUser(client))
	{
		/* set the new name before we check, but don't send to servers unless it is ok */
		strlcpy(client->info, parv[1], sizeof(client->info));
		spamfilter_build_user_string(spamfilter_user, client->name, client);
		if (match_spamfilter(client, spamfilter_user, SPAMF_USER, "SETNAME", NULL, 0, NULL))
		{
			if (IsDead(client))
			        return; /* Killed, don't bother anymore */

			/* Was rejected by spamfilter, restore the realname */
			if (HasCapabilityFast(client, CAP_SETNAME))
			{
				new_message(client, recv_mtags, &mtags);
				sendto_one(client, mtags, "%s FAIL SETNAME CANNOT_CHANGE_REALNAME :Rejected by server", me.name);
				free_message_tags(mtags);
			}
			strlcpy(client->info, oldinfo, sizeof(client->info));
			return;
		}

		/* Check for realname bans here too */
		if (!ValidatePermissionsForPath("immune:server-ban:ban-realname",client,NULL,NULL,NULL) &&
		    ((bconf = find_ban(NULL, client->info, CONF_BAN_REALNAME))))
		{
			banned_client(client, "realname", bconf->reason?bconf->reason:"", 0, 0);
			return;
		}
	} else {
		/* remote user */
		strlcpy(client->info, parv[1], sizeof(client->info));
	}

	new_message(client, recv_mtags, &mtags);
	sendto_local_common_channels(client, client, CAP_SETNAME, mtags, ":%s SETNAME :%s", client->name, client->info);
	sendto_server(client, 0, 0, mtags, ":%s SETNAME :%s", client->id, parv[1]);

	/* notify the sender */
	if (MyConnect(client))
	{
		if (HasCapabilityFast(client, CAP_SETNAME))
			sendto_prefix_one(client, client, mtags, ":%s SETNAME :%s", client->name, client->info);
		else
			sendnotice(client, "Your \"real name\" is now set to be %s - you have to set it manually to undo it", parv[1]);
	}
	free_message_tags(mtags);
	
	RunHook(HOOKTYPE_REALNAME_CHANGE, client, oldinfo);
}
