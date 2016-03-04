/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_sqline.c
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

CMD_FUNC(m_sqline);

/* Place includes here */
#define MSG_SQLINE      "SQLINE"        /* SQLINE */



ModuleHeader MOD_HEADER(m_sqline)
  = {
	"sqline",	/* Name of module */
	"4.0", /* Version */
	"command /sqline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_sqline)
{
	CommandAdd(modinfo->handle, MSG_SQLINE, m_sqline, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_sqline)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD(m_sqline)
{
	return MOD_SUCCESS;
}

/* m_sqline
 *	parv[1] = nickmask
 *	parv[2] = reason
 */
CMD_FUNC(m_sqline)
{
	char    mo[1024];
	char *comment = (parc == 3) ? parv[2] : NULL;
	char *tkllayer[9] = {
                me.name,        /*0  server.name */
                "+",            /*1  +|- */
                "Q",            /*2  G   */
                "*" ,           /*3  user */
                parv[1],           /*4  host */
                sptr->name,           /*5  setby */
                "0",            /*6  expire_at */
                NULL,           /*7  set_at */
                "no reason"     /*8  reason */
        };

	if (parc < 2)
		return 0;

	ircsnprintf(mo, sizeof(mo), "%li", TStime());
	tkllayer[7] = mo;
        tkllayer[8] = comment ? comment : "no reason";
        return m_tkl(&me, &me, 9, tkllayer);
}
