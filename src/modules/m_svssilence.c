/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svssilence.c
 *   (C) 2003 Bram Matthys (Syzop) and the UnrealIRCd Team
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

CMD_FUNC(m_svssilence);

/* Place includes here */
#define MSG_SVSSILENCE       "SVSSILENCE"

ModuleHeader MOD_HEADER(m_svssilence)
  = {
	"svssilence",	/* Name of module */
	"4.0", /* Version */
	"command /svssilence", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_svssilence)
{
	CommandAdd(modinfo->handle, MSG_SVSSILENCE, m_svssilence, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_svssilence)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_svssilence)
{
	return MOD_SUCCESS;	
}

/* m_svssilence()
 * written by Syzop (copied a lot from m_silence),
 * suggested by <??>.
 * parv[1] - target nick
 * parv[2] - space delimited silence list (+Blah +Blih -Bluh etc)
 * SERVER DISTRIBUTION:
 * In contrast to the normal SILENCE command, this is sent to all servers
 * because it can/will add&remove multiple silence entries at once.
 */
CMD_FUNC(m_svssilence)
{
	aClient *acptr;
	int mine;
	char *p, *cp, c;
	
	if (!IsULine(sptr))
		return 0;

	if (parc < 3 || BadPtr(parv[2]) || !(acptr = find_person(parv[1], NULL)))
		return 0;
	
	sendto_server(sptr, 0, 0, ":%s SVSSILENCE %s :%s", sptr->name, parv[1], parv[2]);

	mine = MyClient(acptr) ? 1 : 0;

	for (p = strtok(parv[2], " "); p; p = strtok(NULL, " "))
	{
		c = *p;
		if ((c == '-') || (c == '+'))
			p++;
		else if (!(index(p, '@') || index(p, '.') ||
		    index(p, '!') || index(p, '*')))
		{
			/* "no such nick" */
			continue;
		}
		else
			c = '+';
		cp = pretty_mask(p);
		if ((c == '-' && !del_silence(acptr, cp)) ||
			(c != '-' && !add_silence(acptr, cp, 0)))
		{
			if (mine)
				sendto_prefix_one(acptr, acptr, ":%s SILENCE %c%s", sptr->name, c, cp);
		}
	}
	return 0;
}
