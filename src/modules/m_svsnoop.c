/*
 *   IRC - Internet Relay Chat, src/modules/m_svsnoop.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svsnoop command
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

DLLFUNC int m_svsnoop(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSNOOP 	"SVSNOOP"	
#define TOK_SVSNOOP 	"f"


ModuleHeader MOD_HEADER(m_svsnoop)
  = {
	"m_svsnoop",
	"$Id$",
	"command /svsnoop", 
	"3.2-b8-1",
	NULL
    };

DLLFUNC int MOD_INIT(m_svsnoop)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_SVSNOOP, TOK_SVSNOOP, m_svsnoop, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svsnoop)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svsnoop)(int module_unload)
{
	return MOD_SUCCESS;
}
int m_svsnoop(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
aClient *acptr;
long oldumodes;

	if (!(IsULine(sptr) && parc > 2))
		return 0;
	/* svsnoop bugfix --binary */
	if (hunt_server_token(cptr, sptr, MSG_SVSNOOP, TOK_SVSNOOP, "%s :%s", 1,
	                      parc, parv) == HUNTED_ISME)
	{
		if (parv[2][0] == '+')
		{
			SVSNOOP = 1;
			sendto_ops("This server has been placed in NOOP mode");
			list_for_each_entry(acptr, &client_list, client_node)
			{
				if (MyClient(acptr) && IsAnOper(acptr))
				{
					if (IsOper(acptr))
					{
						IRCstats.operators--;
						VERIFY_OPERCOUNT(acptr, "svsnoop");
					}

					if (!list_empty(&acptr->special_node))
						list_del(&acptr->special_node);

					oldumodes = acptr->umodes;
					acptr->umodes &= ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP |
					                   UMODE_SERVICES | UMODE_SADMIN | UMODE_ADMIN |
					                   UMODE_NETADMIN | UMODE_WHOIS | UMODE_KIX |
					                   UMODE_DEAF | UMODE_HIDEOPER | UMODE_FAILOP |
					                   UMODE_COADMIN | UMODE_VICTIM);
					acptr->oflag = 0;
					remove_oper_snomasks(acptr);
					send_umode_out(acptr, acptr, oldumodes);
					RunHook2(HOOKTYPE_LOCAL_OPER, acptr, 0);
				}
			}
		}
		else
		{
			SVSNOOP = 0;
			sendto_ops("This server is no longer in NOOP mode");
		}
	}
	return 0;
}
