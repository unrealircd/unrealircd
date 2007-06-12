/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_svskill.c
 *   (C) 2004 The UnrealIRCd Team
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

DLLFUNC int m_svskill(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SVSKILL	"SVSKILL"
#define TOK_SVSKILL	"h"

ModuleHeader MOD_HEADER(m_svskill)
  = {
	"svskill",	/* Name of module */
	"$Id$", /* Version */
	"command /svskill", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };


/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_svskill)(ModuleInfo *modinfo)
{
	/*
	 * We call our CommandAdd crap here
	*/
	CommandAdd(modinfo->handle, MSG_SVSKILL, TOK_SVSKILL, m_svskill, MAXPARA, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_svskill)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_svskill)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_svskill
**	parv[0] = servername
**	parv[1] = client
**	parv[2] = kill message
*/
CMD_FUNC(m_svskill)
{
	aClient *acptr;
	/* this is very wierd ? */
	char *comment = NULL;


	if (parc < 2)
		return -1;
	if (parc > 3)
		return -1;
	if (parc == 3)
		comment = parv[2];

	if (parc == 2)
		comment = "SVS Killed";

	if (!IsULine(sptr))
		return -1;

	if (!(acptr = find_person(parv[1], NULL)))
		return 0;

	sendto_serv_butone_token(cptr, parv[0],
	    MSG_SVSKILL, TOK_SVSKILL, "%s :%s", parv[1], comment);

	acptr->flags |= FLAGS_KILLED;

	return exit_client(cptr, acptr, sptr, comment);

}

