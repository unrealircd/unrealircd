/*
 * Block CTCP UnrealIRCd Module (from all users)
 * (C) Copyright 2000-.. Bram Matthys (Syzop) and the UnrealIRCd team
 *
 * Modified from m_noctcp.c by Robin Kirkman.
 *
 * Blocks all CTCP from users, and disconnects any user attempting to use CTCP.
 * Useful for preventing accidental information leaks between clients,
 * such as CTCP VERSION requests.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 1, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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
#ifdef _WIN32
#include "version.h"
#endif

CMD_FUNC(nouserctcp);

ModuleHeader MOD_HEADER(nouserctcp)
  = {
	"m_nouserctcp",
	"4.0",
	"No CTCP from users",
	"3.2-b8-1",
	NULL 
    };

DLLFUNC char *nouserctcp_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice);
DLLFUNC char *nouserctcp_preusermsg(aClient *sptr, aClient *acptr, char *text, int notice);

MOD_TEST(nouserctcp)
{
	return MOD_SUCCESS;
}

MOD_INIT(nouserctcp)
{
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_CHANMSG, 0, nouserctcp_prechanmsg);
	HookAddPChar(modinfo->handle, HOOKTYPE_PRE_USERMSG, 0, nouserctcp_preusermsg);
	
	return MOD_SUCCESS;
}

MOD_LOAD(nouserctcp)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(nouserctcp)
{
	return MOD_SUCCESS;
}

static int IsACTCP(char *s)
{
	if (!s)
		return 0;

	if ((*s == '\001') && strncmp(&s[1], "ACTION ", 7))
		return 1;

	return 0;
}

DLLFUNC char *nouserctcp_prechanmsg(aClient *sptr, aChannel *chptr, char *text, int notice)
{
	if (MyClient(sptr) && IsACTCP(text))
	{
		exit_client(sptr, sptr, &me, "CTCP is prohibited");
		return NULL;
	}
	return text;
}

DLLFUNC char *nouserctcp_preusermsg(aClient *sptr, aClient *acptr, char *text, int notice)
{
	if (MyClient(sptr) && IsACTCP(text))
	{
		exit_client(sptr, sptr, &me, "CTCP is prohibited");
		return NULL;
	}
	return text;
}
