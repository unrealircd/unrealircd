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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "channel.h"
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_QUIT        "QUIT"  /* QUIT */
#define TOK_QUIT        ","     /* 44 */

ModuleHeader MOD_HEADER(m_quit)
  = {
	"quit",	/* Name of module */
	"$Id$", /* Version */
	"command /quit", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_quit)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	add_CommandX(MSG_QUIT, TOK_QUIT, m_quit, 1, M_UNREGISTERED|M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_quit)(int module_load)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_quit)(int module_unload)
{
	if (del_Command(MSG_QUIT, TOK_QUIT, m_quit) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_quit).name);
	}
	return MOD_SUCCESS;
}

/*
** m_quit
**	parv[0] = sender prefix
**	parv[1] = comment
*/
DLLFUNC int  m_quit(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *ocomment = (parc > 1 && parv[1]) ? parv[1] : parv[0];
	static char comment[TOPICLEN + 1];
	Membership *lp;

	if (!IsServer(cptr) && IsPerson(sptr))
	{
#ifdef STRIPBADWORDS
		int blocked = 0;
#endif
		char *s = comment;
		Hook *tmphook;
		if (STATIC_QUIT)
			return exit_client(cptr, sptr, sptr, STATIC_QUIT);

		if (!prefix_quit || strcmp(prefix_quit, "no"))
			s = ircsprintf(comment, "%s ",
		    		BadPtr(prefix_quit) ? "Quit:" : prefix_quit);
#ifdef STRIPBADWORDS
		ocomment = (char *)stripbadwords_quit(ocomment, &blocked);
		if (blocked)
			ocomment = parv[0];
#endif
		if (!IsAnOper(sptr) && ANTI_SPAM_QUIT_MSG_TIME)
			if (sptr->firsttime+ANTI_SPAM_QUIT_MSG_TIME > TStime())
				ocomment = parv[0];

		/* Strip color codes if any channel is +S, use nick as reason if +c. */
		if (IsPerson(sptr) && (strchr(ocomment, '\003')))
		{
			unsigned char filtertype = 0; /* 1=filter, 2=block, highest wins. */
			for (lp = sptr->user->channel; lp; lp = lp->next)
			{
				if (lp->chptr->mode.mode & MODE_NOCOLOR)
				{
					filtertype = 2;
					break;
				}
				if (lp->chptr->mode.mode & MODE_STRIP)
				{
					if (!filtertype)
						filtertype = 1;
				}
			}
			if (filtertype == 1)
			{
				ocomment = StripColors(ocomment);
				if (*ocomment == '\0')
					ocomment = parv[0];
			} else
			if (filtertype == 2)
				ocomment = parv[0];
		} /* (strip color codes) */

                for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_QUIT]; tmphook; tmphook = tmphook->next)
		{
                	ocomment = (*(tmphook->func.pcharfunc))(sptr, ocomment);
                        if (!ocomment)
			{			
				ocomment = parv[0];
                                break;
                        }
                }

		strncpy(s, ocomment, TOPICLEN - (s - comment));
		comment[TOPICLEN] = '\0';
		return exit_client(cptr, sptr, sptr, comment);
	}
	else
	{
		return exit_client(cptr, sptr, sptr, ocomment);
	}
}

