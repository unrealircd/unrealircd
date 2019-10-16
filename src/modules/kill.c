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
static char buf[BUFSIZE], buf2[BUFSIZE];

ModuleHeader MOD_HEADER
  = {
	"kill",
	"5.0",
	"command /kill",
	"UnrealIRCd Team",
	"unrealircd-5",
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
	char *targetlist, *reason;
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

	targetlist = parv[1];
	reason = parv[2];

	if (!IsServer(client->direction) && !ValidatePermissionsForPath("kill:global",client,NULL,NULL,NULL) && !ValidatePermissionsForPath("kill:local",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (strlen(reason) > iConf.quit_length)
		reason[iConf.quit_length] = '\0';

	if (MyUser(client))
		targetlist = canonize(targetlist);

	for (nick = strtoken(&save, targetlist, ","); nick; nick = strtoken(&save, NULL, ","))
	{
		MessageTag *mtags = NULL;

		if (MyUser(client) && (++ntargets > maxtargets))
		{
			sendnumeric(client, ERR_TOOMANYTARGETS, nick, maxtargets, "KILL");
			break;
		}

		target = find_person(nick, NULL);

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

		sendto_snomask(SNO_KILLS,
			"*** Received KILL message for %s (%s@%s) from %s: %s",
			target->name, target->user->username, GetHost(target),
			client->name,
			reason);

		ircd_log(LOG_KILL, "KILL (%s) by %s (%s)",
			make_nick_user_host(target->name, target->user->username, GetHost(target)),
			client->name,
			reason);

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

			ircsnprintf(buf2, sizeof(buf2), "Killed by %s (%s)", client->name, reason);
		}

		if (MyUser(client))
			RunHook3(HOOKTYPE_LOCAL_KILL, client, target, reason);

		ircsnprintf(buf2, sizeof(buf2), "Killed by %s (%s)", client->name, reason);
		exit_client(target, mtags, buf2);

		free_message_tags(mtags);

		if (IsDead(client))
			return; /* stop processing if we killed ourselves */
	}
}
