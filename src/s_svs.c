/*
 *   Unreal Internet Relay Chat Daemon, src/s_svs.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
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

#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include <fcntl.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/wait.h>
#else
#include <io.h>
#endif
#include <sys/stat.h>
#ifdef __hpux
#include "inet.h"
#endif
#if defined(PCS) || defined(AIX) || defined(SVR3)
#include <time.h>
#endif
#include <string.h>

#include "h.h"

extern int SVSNOOP;
extern ircstats IRCstats;
int  m_svsnoop(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;

	if (!(check_registered(sptr) && IsULine(sptr) && parc > 2))
		return 0;
	/* svsnoop bugfix --binary */
	if (hunt_server(cptr, sptr, ":%s SVSNOOP %s :%s", 1, parc,
	    parv) == HUNTED_ISME)
	{
		if (parv[2][0] == '+')
		{
			SVSNOOP = 1;
			sendto_ops("This server has been placed in NOOP mode");
			for (acptr = &me; acptr; acptr = acptr->prev)
			{
				if (MyClient(acptr) && IsAnOper(acptr))
				{
					if (IsAnOper(acptr))
						IRCstats.operators--;
					acptr->umodes &=
					    ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP | UMODE_SERVICES |
					    UMODE_SADMIN | UMODE_ADMIN);
					acptr->umodes &=
		    				~(UMODE_NETADMIN | UMODE_TECHADMIN | UMODE_CLIENT |
		 			   UMODE_FLOOD | UMODE_EYES | UMODE_WHOIS);
					acptr->umodes &=
					    ~(UMODE_KIX | UMODE_FCLIENT | UMODE_HIDING |
					    UMODE_DEAF | UMODE_HIDEOPER);
					acptr->oflag = 0;
				
				}
			}

		}
		else
		{
			SVSNOOP = 0;
			sendto_ops("This server is no longer in NOOP mode");
		}
	}
}
