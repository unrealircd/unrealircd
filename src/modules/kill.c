/*
 *   Unreal Internet Relay Chat Daemon, src/modules/kill.c
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

CMD_FUNC(cmd_kill);

ModuleHeader MOD_HEADER
  = {
	"kill",
	"5.0",
	"command /kill",
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, "KILL", cmd_kill, 2, CMD_USER|CMD_SERVER);
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


/** The KILL command - this forcefully terminates a users' connection.
 * parv[1] = kill victim(s) - comma separated list
 * parv[2] = reason
 */
CMD_FUNC(cmd_kill)
{
	char targetlist[BUFSIZE];
	char reason[BUFSIZE];
	char buf2[BUFSIZE];
	char *str;
	char *nick, *save = NULL;
	Client *target;
	Hook *h;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("KILL");

	if ((parc < 3) || BadPtr(parv[2]))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KILL");
		return;
	}

	if (!IsServer(client->direction) && !ValidatePermissionsForPath("kill:global",client,NULL,NULL,NULL) && !ValidatePermissionsForPath("kill:local",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (MyUser(client))
		strlcpy(targetlist, canonize(parv[1]), sizeof(targetlist));
	else
		strlcpy(targetlist, parv[1], sizeof(targetlist));

	strlncpy(reason, parv[2], sizeof(reason), iConf.quit_length);

	for (nick = strtoken(&save, targetlist, ","); nick; nick = strtoken(&save, NULL, ","))
	{
		MessageTag *mtags = NULL;

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, nick, maxtargets, "KILL");
			break;
		}

		target = find_user(nick, NULL);

		/* If a local user issued the /KILL then we will "chase" the user.
		 * In other words: we'll check the history for recently changed nicks.
		 * We don't do this for remote KILL requests as we have UID for that.
		 */
		if (!target && MyUser(client))
		{
			target = get_history(nick, KILLCHASETIMELIMIT);
			if (target)
				sendnotice(client, "*** KILL changed from %s to %s", nick, target->name);
		}

		if (!target)
		{
			sendnumeric(client, ERR_NOSUCHNICK, nick);
			continue;
		}

		if ((!MyConnect(target) && MyUser(client) && !ValidatePermissionsForPath("kill:global",client,target,NULL,NULL))
		    || (MyConnect(target) && MyUser(client)
		    && !ValidatePermissionsForPath("kill:local",client,target,NULL,NULL)))
		{
			sendnumeric(client, ERR_NOPRIVILEGES);
			continue;
		}

		/* Hooks can plug-in here to reject a kill */
		if (MyUser(client))
		{
			int ret = EX_ALLOW;
			for (h = Hooks[HOOKTYPE_PRE_KILL]; h; h = h->next)
			{
				/* note: parameters are: client, victim, reason. reason can be NULL !! */
				ret = (*(h->func.intfunc))(client, target, reason);
				if (ret != EX_ALLOW)
					break;
			}
			if ((ret == EX_DENY) || (ret == EX_ALWAYS_DENY))
				continue; /* reject kill for this particular user */
		}

		/* From here on, the kill is probably going to be successful. */

		unreal_log(ULOG_INFO, "kill", "KILL_COMMAND", client,
		           "Client killed: $target.details [by: $client] ($reason)",
		           log_data_client("target", target),
		           log_data_string("reason", reason));

		new_message(client, recv_mtags, &mtags);

		/* Victim gets a little notification (s)he is about to die */
		if (MyConnect(target))
		{
			sendto_prefix_one(target, client, NULL, ":%s KILL %s :%s",
			    client->name, target->name, reason);
		}

		if (MyConnect(target) && MyConnect(client))
		{
			/* Local kill. This is handled as if it were a QUIT */
		}
		else
		{
			/* Kill from one server to another (we may be src, victim or something in-between) */

			/* Broadcast it to other servers */
			sendto_server(client, 0, 0, mtags, ":%s KILL %s :%s",
			              client->id, target->id, reason);

			/* Don't send a QUIT for this */
			SetKilled(target);
		}

		if (MyUser(client))
			RunHook(HOOKTYPE_LOCAL_KILL, client, target, reason);

		if (iConf.hide_killed_by)
			ircsnprintf(buf2, sizeof(buf2), "Killed (%s)", reason);
		else
			ircsnprintf(buf2, sizeof(buf2), "Killed by %s (%s)", client->name, reason);

		exit_client(target, mtags, buf2);

		free_message_tags(mtags);

		if (IsDead(client))
			return; /* stop processing if we killed ourselves */
	}
}
