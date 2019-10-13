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

/* Place includes here */
#define MSG_KILL        "KILL"  /* KILL */

ModuleHeader MOD_HEADER
  = {
	"kill",	/* Name of module */
	"5.0", /* Version */
	"command /kill", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_KILL, cmd_kill, 2, CMD_USER|CMD_SERVER);
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
** cmd_kill
**	parv[1] = kill victim(s) - comma separated list
**	parv[2] = kill path
*/
CMD_FUNC(cmd_kill)
{
	Client *target;
	char inpath[HOSTLEN * 2 + USERLEN + 5];
	char *oinpath = get_client_name(client->direction, FALSE);
	char *user, *path, *killer, *nick, *p, *s;
	int kcount = 0;
	Hook *h;
	int ntargets = 0, n;
	int maxtargets = max_targets_for_command("KILL");

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KILL");
		return;
	}

	// FIXME: clean all this killpath shit, nobody cares anymore these days.
	// FYI, I just replaced 'cptr' with 'client->direction' for now, but
	// we should clean up this mess later, see the FIXME.

	user = parv[1];
	path = parv[2];		/* Either defined or NULL (parc >= 2!!) */

	strlcpy(inpath, oinpath, sizeof inpath);

	if (IsServer(client->direction) && (s = strchr(inpath, '.')) != NULL)
		*s = '\0';	/* Truncate at first "." -- hmm... why ? */

	if (!IsServer(client->direction) && !ValidatePermissionsForPath("kill:global",client,NULL,NULL,NULL) && !ValidatePermissionsForPath("kill:local",client,NULL,NULL,NULL))
	{
		sendnumeric(client, ERR_NOPRIVILEGES);
		return;
	}

	if (BadPtr(path))
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "KILL");
		return;
	}

	if (strlen(path) > iConf.quit_length)
		path[iConf.quit_length] = '\0';

	if (MyUser(client))
		user = canonize(user);

	for (p = NULL, nick = strtoken(&p, user, ","); nick; nick = strtoken(&p, NULL, ","))
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
				ret = (*(h->func.intfunc))(client, target, path);
				if (ret != EX_ALLOW)
					break;
			}
			if ((ret == EX_DENY) || (ret == EX_ALWAYS_DENY))
				continue; /* reject kill for this particular user */
		}

		/* From here on, the kill is probably going to be successful. */

		kcount++;

		if (!IsServer(client) && (kcount > MAXKILLS))
		{
			sendnotice(client,
			    "*** Too many targets, kill list was truncated. Maximum is %d.",
			    MAXKILLS);
			break;
		}

		if (!IsServer(client->direction))
		{
			/*
			   ** The kill originates from this server, initialize path.
			   ** (In which case the 'path' may contain user suplied
			   ** explanation ...or some nasty comment, sigh... >;-)
			   **
			   **   ...!operhost!oper
			   **   ...!operhost!oper (comment)
			 */
			strlcpy(inpath, GetHost(client->direction), sizeof inpath);
			if (kcount == 1)
			{
				ircsnprintf(buf, sizeof(buf), "%s (%s)", client->direction->name, path);
				path = buf;
			}
		}
		/*
		   ** Notify all *local* opers about the KILL (this includes the one
		   ** originating the kill, if from this server--the special numeric
		   ** reply message is not generated anymore).
		   **
		   ** Note: "target->name" is used instead of "user" because we may
		   **    have changed the target because of the nickname change.
		 */

		sendto_snomask(SNO_KILLS,
		    "*** Received KILL message for %s!%s@%s from %s Path: %s!%s",
		    target->name, target->user->username,
		    IsHidden(target) ? target->user->virthost : target->user->realhost,
		    client->name, inpath, path);

		ircd_log(LOG_KILL, "KILL (%s) by %s(%s!%s)",
			make_nick_user_host(target->name, target->user->username, GetHost(target)),
			client->name, inpath, path);

		new_message(client, recv_mtags, &mtags);

		/* Victim gets a little notification (s)he is about to die */
		if (MyConnect(target))
		{
			sendto_prefix_one(target, client, NULL, ":%s KILL %s :%s!%s",
			    client->name, target->name, inpath, path);
		}

		if (MyConnect(target) && MyConnect(client))
		{
			/* Local kill. This is handled as if it were a QUIT */
			/* Prepare buffer for exit_client */
			ircsnprintf(buf2, sizeof(buf2), "[%s] Local kill by %s (%s)",
			    me.name, client->name,
			    BadPtr(parv[2]) ? client->name : parv[2]);
		}
		else
		{
			/* Kill from one server to another (we may be src, victim or something in-between) */

			/* Broadcast it to other servers */
			sendto_server(client, 0, 0, mtags, ":%s KILL %s :%s!%s",
			    client->name, ID(target), inpath, path);

			/* Don't send a QUIT for this */
			SetKilled(target);
			
			/* Prepare the buffer for exit_client */
			if ((killer = strchr(path, ' ')))
			{
				while ((killer >= path) && *killer && *killer != '!')
					killer--;
				if (!*killer)
					killer = path;
				else
					killer++;
			}
			else
				killer = path;
			ircsnprintf(buf2, sizeof(buf2), "Killed (%s)", killer);
		}

		if (MyUser(client))
			RunHook3(HOOKTYPE_LOCAL_KILL, client, target, parv[2]);

		exit_client(target, mtags, buf2);

		free_message_tags(mtags);

		if (IsDead(client))
			return; /* stop processing if we killed ourselves */
	}
}
