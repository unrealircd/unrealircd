/*
 *   IRC - Internet Relay Chat, src/modules/m_fingerprint.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   Server to server FINGERPRINT command
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

DLLFUNC int m_fingerprint(aClient *cptr, aClient *sptr, int parc, char *parv[]);
static Command* cmdFingerprint = NULL;

#define MSG_FINGERPRINT "FINGERPRINT"

ModuleHeader MOD_HEADER(m_fingerprint)
  = {
	"m_fingerprint",
	"$Id: m_fingerprint.c,v 1.1.2.15 2010-12-3 20:20:23 -Nath Exp $",
	"Server to Server FINGERPRINT command", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_fingerprint)(ModuleInfo *modinfo)
{
	cmdFingerprint = CommandAdd(modinfo->handle, MSG_FINGERPRINT, m_fingerprint, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_fingerprint)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_fingerprint)(int module_unload)
{
	if (cmdFingerprint)
	{
		CommandDel(cmdFingerprint);
		cmdFingerprint = NULL;
	}
	return MOD_SUCCESS;
}
/*
 * m_fingerprint
 * parv[1] = nickname
 * parv[2] = fingerprint
 *
*/

int m_fingerprint(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (!IsServer(sptr))
		return 0;

	if (parc < 3)
		return 0;

	acptr = find_person(parv[1], (aClient *)NULL);
       if (!acptr)
		return 0;

	strlcpy(acptr->sslfingerprint, parv[2], sizeof(acptr->sslfingerprint));

	sendto_server(cptr, 0, 0,
		"%s :%s", sptr->name, parv[1], parv[2]);
	return 0;
}