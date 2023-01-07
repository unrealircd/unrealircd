/*
 *   Unreal Internet Relay Chat Daemon, src/modules/svssilence.c
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

CMD_FUNC(cmd_svssilence);

ModuleHeader MOD_HEADER
  = {
	"svssilence",	/* Name of module */
	"5.0", /* Version */
	"command /svssilence", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, "SVSSILENCE", cmd_svssilence, MAXPARA, CMD_SERVER);
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

/* cmd_svssilence()
 * written by Syzop (copied a lot from cmd_silence),
 * suggested by <??>.
 * parv[1] - target nick
 * parv[2] - space delimited silence list (+Blah +Blih -Bluh etc)
 * SERVER DISTRIBUTION:
 * Since UnrealIRCd 5 it is directed to the target client (previously: broadcasted).
 */
CMD_FUNC(cmd_svssilence)
{
	Client *target;
	int mine;
	char *p, *cp, c;
	char request[BUFSIZE];
	
	if (!IsSvsCmdOk(client))
		return;

	if (parc < 3 || BadPtr(parv[2]) || !(target = find_user(parv[1], NULL)))
		return;
	
	if (!MyUser(target))
	{
		sendto_one(target, NULL, ":%s SVSSILENCE %s :%s", client->name, parv[1], parv[2]);
		return;
	}

	/* It's for our client */
	strlcpy(request, parv[2], sizeof(request));
	for (p = strtok(request, " "); p; p = strtok(NULL, " "))
	{
		c = *p;
		if ((c == '-') || (c == '+'))
			p++;
		else if (!(strchr(p, '@') || strchr(p, '.') || strchr(p, '!') || strchr(p, '*')))
		{
			/* "no such nick" */
			continue;
		}
		else
			c = '+';
		cp = pretty_mask(p);
		if ((c == '-' && !del_silence(target, cp)) ||
		    (c != '-' && !add_silence(target, cp, 0)))
		{
			sendto_prefix_one(target, target, NULL, ":%s SILENCE %c%s", client->name, c, cp);
		}
	}
}
