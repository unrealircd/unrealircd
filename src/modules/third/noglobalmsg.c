/*
 *   IRC - Internet Relay Chat, src/modules/third/pissnet_relink.c
 *   (C) 2021 Polsaker
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

ModuleHeader MOD_HEADER
  = {
	"third/noglobalmsg",
	"1.0",
	"Disables PRIVMSG $*",
	"Polsaker",
	"unrealircd-5",
    };


CMD_OVERRIDE_FUNC(override_privmsg);

MOD_INIT()
{
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	if (!CommandOverrideAdd(modinfo->handle, "PRIVMSG", override_privmsg))
		return MOD_FAILED;


	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

CMD_OVERRIDE_FUNC(override_privmsg)
{
	char *targetstr, *p;
	for (p = NULL, targetstr = strtoken(&p, parv[1], ","); targetstr; targetstr = strtoken(&p, NULL, ","))
	{
		if (*targetstr == '$')
		{
			sendnumeric(client, ERR_YOUREBANNEDCREEP, "Why would you want to do that?!");
			return;
		}
	}
	CallCommandOverride(ovr, client, recv_mtags, parc, parv);
}
