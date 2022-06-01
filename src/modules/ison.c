/*
 *   IRC - Internet Relay Chat, src/modules/ison.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(cmd_ison);

#define MSG_ISON 	"ISON"	

ModuleHeader MOD_HEADER
  = {
	"ison",
	"5.0",
	"command /ison", 
	"UnrealIRCd Team",
	"unrealircd-6",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_ISON, cmd_ison, 1, CMD_USER);
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

/*
 * cmd_ison added by Darren Reed 13/8/91 to act as an efficent user indicator
 * with respect to cpu/bandwidth used. Implemented for NOTIFY feature in
 * clients. Designed to reduce number of whois requests. Can process
 * nicknames in batches as long as the maximum buffer length.
 *
 * format:
 * ISON :nicklist
 */

CMD_FUNC(cmd_ison)
{
	char buf[BUFSIZE];
	char request[BUFSIZE];
	char namebuf[USERLEN + HOSTLEN + 4];
	Client *acptr;
	char *s, *user;
	char *p = NULL;

	if (!MyUser(client))
		return;

	if (parc < 2)
	{
		sendnumeric(client, ERR_NEEDMOREPARAMS, "ISON");
		return;
	}

	ircsnprintf(buf, sizeof(buf), ":%s %d %s :", me.name, RPL_ISON, client->name);

	strlcpy(request, parv[1], sizeof(request));
	for (s = strtoken(&p, request, " "); s; s = strtoken(&p, NULL, " "))
	{
		if ((user = strchr(s, '!')))
			*user++ = '\0';
		if ((acptr = find_user(s, NULL)))
		{
			if (user)
			{
				ircsnprintf(namebuf, sizeof(namebuf), "%s@%s", acptr->user->username, GetHost(acptr));
				if (!match_simple(user, namebuf)) continue;
				*--user = '!';
			}

			strlcat(buf, s, sizeof(buf));
			strlcat(buf, " ", sizeof(buf));
		}
	}

	sendto_one(client, NULL, "%s", buf);
}
