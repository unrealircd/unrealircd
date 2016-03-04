/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_quit.c
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

CMD_FUNC(m_quit);

#define MSG_QUIT        "QUIT"  /* QUIT */

ModuleHeader MOD_HEADER(m_quit)
  = {
	"quit",	/* Name of module */
	"4.0", /* Version */
	"command /quit", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_quit)
{
	CommandAdd(modinfo->handle, MSG_QUIT, m_quit, 1, M_UNREGISTERED|M_USER|M_VIRUS);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_quit)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_quit)
{
	return MOD_SUCCESS;
}

/*
** m_quit
**	parv[1] = comment
*/
CMD_FUNC(m_quit)
{
	char *comment = (parc > 1 && parv[1]) ? parv[1] : sptr->name;
	static char commentbuf[TOPICLEN + 1];
	Membership *lp;

	if (!IsServer(cptr) && IsPerson(sptr))
	{
		int n;
		Hook *tmphook;

		if (STATIC_QUIT)
			return exit_client(cptr, sptr, sptr, STATIC_QUIT);

		if (IsVirus(sptr))
			return exit_client(cptr, sptr, sptr, "Client exited");

		n = dospamfilter(sptr, comment, SPAMF_QUIT, NULL, 0, NULL);
		if (n == FLUSH_BUFFER)
			return n;
		if (n < 0)
			comment = sptr->name;
		
		if (!ValidatePermissionsForPath("immune:antispamtimer",sptr,NULL,NULL,NULL) && ANTI_SPAM_QUIT_MSG_TIME)
		{
			if (sptr->local->firsttime+ANTI_SPAM_QUIT_MSG_TIME > TStime())
				comment = sptr->name;
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

		return exit_client(cptr, sptr, sptr, commentbuf);
	}
	else
	{
		/* Remote quits and non-person quits always use their original comment */
		return exit_client(cptr, sptr, sptr, comment);
	}
}
