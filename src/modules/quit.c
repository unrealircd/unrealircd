/*
 *   Unreal Internet Relay Chat Daemon, src/modules/quit.c
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

CMD_FUNC(cmd_quit);

#define MSG_QUIT        "QUIT"  /* QUIT */

ModuleHeader MOD_HEADER
  = {
	"quit",	/* Name of module */
	"5.0", /* Version */
	"command /quit", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_QUIT, cmd_quit, 1, M_UNREGISTERED|M_USER|M_VIRUS);
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
** cmd_quit
**	parv[1] = comment
*/
CMD_FUNC(cmd_quit)
{
	char *comment = (parc > 1 && parv[1]) ? parv[1] : sptr->name;
	static char commentbuf[MAXQUITLEN + 1];

	if (parv[1] && (strlen(comment) > iConf.quit_length))
		comment[iConf.quit_length] = '\0';

	if (!IsServer(cptr) && IsUser(sptr))
	{
		int n;
		Hook *tmphook;

		if (STATIC_QUIT)
			return exit_client(cptr, sptr, sptr, recv_mtags, STATIC_QUIT);

		if (IsVirus(sptr))
			return exit_client(cptr, sptr, sptr, recv_mtags, "Client exited");

		n = run_spamfilter(sptr, comment, SPAMF_QUIT, NULL, 0, NULL);
		if (n == FLUSH_BUFFER)
			return n;
		if (n < 0)
			comment = sptr->name;
		
		if (!ValidatePermissionsForPath("immune:anti-spam-quit-message-time",sptr,NULL,NULL,NULL) && ANTI_SPAM_QUIT_MSG_TIME)
		{
			if (sptr->local->firsttime+ANTI_SPAM_QUIT_MSG_TIME > TStime())
				comment = sptr->name;
		}

		if (iConf.part_instead_of_quit_on_comment_change && MyUser(sptr))
		{
			Membership *lp, *lp_next;
			char *newcomment;
			Channel *chptr;

			for (lp = sptr->user->channel; lp; lp = lp_next)
			{
				chptr = lp->chptr;
				newcomment = comment;
				lp_next = lp->next;

				for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_QUIT_CHAN]; tmphook; tmphook = tmphook->next)
				{
					newcomment = (*(tmphook->func.pcharfunc))(sptr, chptr, comment);
					if (!newcomment)
						break;
				}

				if (newcomment && is_banned(sptr, chptr, BANCHK_LEAVE_MSG, &newcomment, NULL))
					newcomment = NULL;

				/* Comment changed? Then PART the user before we do the QUIT. */
				if (comment != newcomment)
				{
					char *parx[4];
					int ret;

					parx[0] = NULL;
					parx[1] = chptr->chname;
					parx[2] = newcomment;
					parx[3] = NULL;

					ret = do_cmd(cptr, sptr, recv_mtags, "PART", newcomment ? 3 : 2, parx);
					/* This would be unusual, but possible (somewhere in the future perhaps): */
					if (ret == FLUSH_BUFFER)
						return ret;
				}
			}
		}

		for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_QUIT]; tmphook; tmphook = tmphook->next)
		{
			comment = (*(tmphook->func.pcharfunc))(sptr, comment);
			if (!comment)
			{			
				comment = sptr->name;
				break;
			}
		}

		if (PREFIX_QUIT)
			snprintf(commentbuf, sizeof(commentbuf), "%s: %s", PREFIX_QUIT, comment);
		else
			strlcpy(commentbuf, comment, sizeof(commentbuf));

		return exit_client(cptr, sptr, sptr, recv_mtags, commentbuf);
	}
	else
	{
		/* Remote quits and non-person quits always use their original comment.
		 * Also pass recv_mtags so to keep the msgid and such.
		 */
		return exit_client(cptr, sptr, sptr, recv_mtags, comment);
	}
}
