/*
 *   Unreal Internet Relay Chat Daemon, src/modules/sqline.c
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

CMD_FUNC(cmd_sqline);

/* Place includes here */
#define MSG_SQLINE      "SQLINE"        /* SQLINE */



ModuleHeader MOD_HEADER
  = {
	"sqline",	/* Name of module */
	"5.0", /* Version */
	"command /sqline", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-6",
    };

/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_SQLINE, cmd_sqline, MAXPARA, CMD_SERVER);
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

/* cmd_sqline
 *	parv[1] = nickmask
 *	parv[2] = reason
 */
CMD_FUNC(cmd_sqline)
{
	char mo[32];
	const char *comment = (parc == 3) ? parv[2] : NULL;
	const char *tkllayer[9] = {
		me.name,        /*0  server.name */
		"+",            /*1  +|- */
		"Q",            /*2  G   */
		"*" ,           /*3  user */
		parv[1],        /*4  host */
		client->name,     /*5  setby */
		"0",            /*6  expire_at */
		NULL,           /*7  set_at */
		"no reason"     /*8  reason */
	};

	if (parc < 2)
		return;

	ircsnprintf(mo, sizeof(mo), "%lld", (long long)TStime());
	tkllayer[7] = mo;
	tkllayer[8] = comment ? comment : "no reason";
	cmd_tkl(&me, NULL, 9, tkllayer);
}
