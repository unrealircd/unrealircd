/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_unsqline.c
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

CMD_FUNC(m_unsqline);

#define MSG_UNSQLINE    "UNSQLINE"      /* UNSQLINE */

ModuleHeader MOD_HEADER(m_unsqline)
  = {
	"unsqline",	/* Name of module */
	"4.0", /* Version */
	"command /unsqline", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
MOD_INIT(m_unsqline)
{
	CommandAdd(modinfo->handle, MSG_UNSQLINE, m_unsqline, MAXPARA, M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(m_unsqline)
{
	return MOD_SUCCESS;
}

/* Called when module is unloaded */
MOD_UNLOAD(m_unsqline)
{
	return MOD_SUCCESS;
}

/* m_unsqline
**	parv[1] = nickmask
*/
CMD_FUNC(m_unsqline)
{
	char *tkllayer[6] = {
		me.name,           /*0  server.name */
		"-",               /*1  - */
		"Q",               /*2  Q   */
		"*",               /*3  unused */
		parv[1],           /*4  host */
		sptr->name         /*5  whoremoved */
	};

	if (parc < 2)
		return 0;

	m_tkl(&me, &me, 6, tkllayer);
	
	return 0;
}
