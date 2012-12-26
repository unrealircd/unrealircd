/*
 *   IRC - Internet Relay Chat, src/modules/out.c
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

DLLFUNC int m_help(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_HELP 	"HELP"	
#define TOK_HELP 	"4"	
#define MSG_HELPOP	"HELPOP"

ModuleHeader MOD_HEADER(m_help)
  = {
	"m_help",
	"$Id$",
	"command /help", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_help)(ModuleInfo *modinfo)
{
	add_Command(MSG_HELP, TOK_HELP, m_help, 1);
	add_Command(MSG_HELPOP, NULL, m_help, 1);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_help)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_help)(int module_unload)
{
	if (del_Command(MSG_HELP, TOK_HELP, m_help) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_help).name);
	}
	if (del_Command(MSG_HELPOP, NULL, m_help) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_help).name);
	}
	return MOD_SUCCESS;
}

/*
** m_help (help/write to +h currently online) -Donwulff
**	parv[0] = sender prefix
**	parv[1] = optional message text
*/
CMD_FUNC(m_help)
{
	char *message, *s;
	Link *tmpl;

	message = parc > 1 ? parv[1] : NULL;

	if (MyConnect(sptr))
	{
		/* New syntax: ?... never goes out, !... always does. */
		if (BadPtr(message)) {
			parse_help(sptr, parv[0], NULL);
			return 0;
		}
		else if (message[0] == '?') {
			parse_help(sptr, parv[0], message+1);
			return 0;
		}
		else if (message[0] == '!') {
			message++;
		}
		else {
			if (parse_help(sptr, parv[0], message))
				return 0;
		}
	}

	return 0;
}
