/*
 *   IRC - Internet Relay Chat, src/modules/m_pass.c
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
#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
#include "proto.h"
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_pass(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_PASS 	"PASS"	
#define TOK_PASS 	"<"	

ModuleHeader MOD_HEADER(m_pass)
  = {
	"m_pass",
	"$Id$",
	"command /pass", 
	NULL,
	NULL 
    };

DLLFUNC int MOD_INIT(m_pass)(ModuleInfo *modinfo)
{
	add_CommandX(MSG_PASS, TOK_PASS, m_pass, 1, M_UNREGISTERED|M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_pass)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_pass)(int module_unload)
{
	if (del_Command(MSG_PASS, TOK_PASS, m_pass) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_pass).name);
	}
	return MOD_SUCCESS;
}

/***************************************************************************
 * m_pass() - Added Sat, 4 March 1989
 ***************************************************************************/

/*
** m_pass
**	parv[0] = sender prefix
**	parv[1] = password
*/
DLLFUNC CMD_FUNC(m_pass)
{
	char *password = parc > 1 ? parv[1] : NULL;
	int  PassLen = 0;
	if (BadPtr(password))
	{
		sendto_one(cptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PASS");
		return 0;
	}
	if (!MyConnect(sptr) || (!IsUnknown(cptr) && !IsHandshake(cptr)))
	{
		sendto_one(cptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		return 0;
	}

	PassLen = strlen(password);
	if (cptr->passwd)
		MyFree(cptr->passwd);
	if (PassLen > (PASSWDLEN))
		PassLen = PASSWDLEN;
	cptr->passwd = MyMalloc(PassLen + 1);
	strncpyzt(cptr->passwd, password, PassLen + 1);

	/* note: the original non-truncated password is supplied as 2nd parameter. */
	RunHookReturnInt2(HOOKTYPE_LOCAL_PASS, sptr, password, !=0);
	return 0;
}
