/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_kill.c
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

CMD_FUNC(m_kill);
static char buf[BUFSIZE], buf2[BUFSIZE];

/* Place includes here */
#define MSG_KILL        "KILL"  /* KILL */

ModuleHeader MOD_HEADER(m_kill)
  = {
	"kill",	/* Name of module */
	"4.0", /* Version */
	"command /kill", /* Short description of module */
	"3.2-b8-1",
	NULL
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_kill)
{
	CommandAdd(modinfo->handle, MSG_KILL, m_kill, 2, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_kill)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD(m_kill)
{
	return MOD_SUCCESS;
}


/*
** m_kill
**	parv[1] = kill victim(s) - comma separated list
**	parv[2] = kill path
*/
CMD_FUNC(m_kill)
{
	aClient *acptr;
	char inpath[HOSTLEN * 2 + USERLEN + 5];
	char *oinpath = get_client_name(cptr, FALSE);
	char *user, *path, *killer, *nick, *p, *s;
	int  chasing = 0, kcount = 0;
	Hook *h;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "KILL");
		return 0;
	}

	user = parv[1];
	path = parv[2];		/* Either defined or NULL (parc >= 2!!) */

	strlcpy(inpath, oinpath, sizeof inpath);

	if (IsServer(cptr) && (s = (char *)index(inpath, '.')) != NULL)
		*s = '\0';	/* Truncate at first "." -- hmm... why ? */

	if (!IsServer(cptr) && !ValidatePermissionsForPath("kill:global",sptr,NULL,NULL,NULL) && !ValidatePermissionsForPath("kill:local",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
		return 0;
	}

	if (BadPtr(path))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "KILL");
		return 0;
	}

	if (strlen(path) > (size_t)TOPICLEN)
		path[TOPICLEN] = '\0';

	if (MyClient(sptr))
		user = (char *)canonize(user);

	for (p = NULL, nick = strtoken(&p, user, ","); nick;
	    nick = strtoken(&p, NULL, ","))
	{
		acptr = find_person(nick, NULL);

		/* If a local user issued the /KILL then we will "chase" the user.
		 * In other words: we'll check the history for recently changed nicks.
		 * We don't do this for remote KILL requests as we have UID for that.
		 */
		if (!acptr && MyClient(sptr))
		{
			acptr = get_history(nick, KILLCHASETIMELIMIT);
			if (acptr)
				sendnotice(sptr, "*** KILL changed from %s to %s", nick, acptr->name);
		}

		if (!acptr)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name, sptr->name, nick);
			continue;
		}

		if ((!MyConnect(acptr) && MyClient(cptr) && !ValidatePermissionsForPath("kill:global",sptr,acptr,NULL,NULL))
		    || (MyConnect(acptr) && MyClient(cptr)
		    && !ValidatePermissionsForPath("kill:local",sptr,acptr,NULL,NULL)))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, sptr->name);
			continue;
		}

		/* Hooks can plug-in here to reject a kill */
		if (MyClient(sptr))
		{
			int ret = EX_ALLOW;
			for (h = Hooks[HOOKTYPE_PRE_KILL]; h; h = h->next)
			{
				/* note: parameters are: sptr, victim, reason. reason can be NULL !! */
				ret = (*(h->func.intfunc))(sptr, acptr, path);
				if (ret != EX_ALLOW)
					break;
			}
			if ((ret == EX_DENY) || (ret == EX_ALWAYS_DENY))
				continue; /* reject kill for this particular user */
		}

		/* From here on, the kill is probably going to be successful. */

		kcount++;

		if (!IsServer(sptr) && (kcount > MAXKILLS))
		{
			sendnotice(sptr,
			    "*** Too many targets, kill list was truncated. Maximum is %d.",
			    MAXKILLS);
			break;
		}

		if (!IsServer(cptr))
		{
			/*
			   ** The kill originates from this server, initialize path.
			   ** (In which case the 'path' may contain user suplied
			   ** explanation ...or some nasty comment, sigh... >;-)
			   **
			   **   ...!operhost!oper
			   **   ...!operhost!oper (comment)
			 */
			strlcpy(inpath, GetHost(cptr), sizeof inpath);
			if (kcount == 1)
			{
				ircsnprintf(buf, sizeof(buf), "%s (%s)", cptr->name, path);
				path = buf;
			}
		}
		/*
		   ** Notify all *local* opers about the KILL (this includes the one
		   ** originating the kill, if from this server--the special numeric
		   ** reply message is not generated anymore).
		   **
		   ** Note: "acptr->name" is used instead of "user" because we may
		   **    have changed the target because of the nickname change.
		 */

		sendto_snomask_normal(SNO_KILLS,
		    "*** Received KILL message for %s!%s@%s from %s Path: %s!%s",
		    acptr->name, acptr->user->username,
		    IsHidden(acptr) ? acptr->user->virthost : acptr->user->realhost,
		    sptr->name, inpath, path);

		ircd_log(LOG_KILL, "KILL (%s) by  %s(%s!%s)",
			make_nick_user_host(acptr->name, acptr->user->username, GetHost(acptr)),
			sptr->name, inpath, path);

		/* Victim gets a little notification (s)he is about to die */
		if (MyConnect(acptr))
		{
			sendto_prefix_one(acptr, sptr, ":%s KILL %s :%s!%s",
			    sptr->name, acptr->name, inpath, path);
		}

		if (MyConnect(acptr) && MyConnect(sptr))
		{
			/* Local kill. This is handled as if it were a QUIT */
			/* Prepare buffer for exit_client */
			ircsnprintf(buf2, sizeof(buf2), "[%s] Local kill by %s (%s)",
			    me.name, sptr->name,
			    BadPtr(parv[2]) ? sptr->name : parv[2]);
		}
		else
		{
			/* Kill from one server to another (we may be src, victim or something in-between) */

			/* Broadcast it to other SID and non-SID servers (may be a NOOP, obviously) */
			sendto_server(cptr, PROTO_SID, 0, ":%s KILL %s :%s!%s",
			    sptr->name, ID(acptr), inpath, path);
			sendto_server(cptr, 0, PROTO_SID, ":%s KILL %s :%s!%s",
			    sptr->name, acptr->name, inpath, path);

			/* Don't send a QUIT for this */
			acptr->flags |= FLAGS_KILLED;
			
			/* Prepare the buffer for exit_client */
			if ((killer = index(path, ' ')))
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

		if (MyClient(sptr))
			RunHook3(HOOKTYPE_LOCAL_KILL, sptr, acptr, parv[2]);

		if (exit_client(cptr, acptr, sptr, buf2) == FLUSH_BUFFER)
			return FLUSH_BUFFER; /* (return if we killed ourselves) */
	}
	return 0;
}
