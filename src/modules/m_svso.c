/*
 *   IRC - Internet Relay Chat, src/modules/m_svso.c
 *   (C) 2001 The UnrealIRCd Team
 *
 *   svso command
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

DLLFUNC int m_svso(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_SVSO 	"SVSO"	
#define TOK_SVSO 	"BB"	

#define STAR1 OFLAG_SADMIN|OFLAG_ADMIN|OFLAG_NETADMIN|OFLAG_COADMIN
#define STAR2 OFLAG_ZLINE|OFLAG_HIDE|OFLAG_WHOIS
static int oper_access[] = {
        ~(STAR1 | STAR2), '*',
        OFLAG_LOCAL, 'o',
        OFLAG_GLOBAL, 'O',
        OFLAG_REHASH, 'r',
        OFLAG_DIE, 'D',
        OFLAG_RESTART, 'R',
        OFLAG_HELPOP, 'h',
        OFLAG_GLOBOP, 'g',
        OFLAG_WALLOP, 'w',
        OFLAG_LOCOP, 'l',
        OFLAG_LROUTE, 'c',
        OFLAG_GROUTE, 'L',
        OFLAG_LKILL, 'k',
        OFLAG_GKILL, 'K',
        OFLAG_KLINE, 'b',
        OFLAG_UNKLINE, 'B',
        OFLAG_LNOTICE, 'n',
        OFLAG_GNOTICE, 'G',
        OFLAG_ADMIN, 'A',
        OFLAG_SADMIN, 'a',
        OFLAG_NETADMIN, 'N',
        OFLAG_COADMIN, 'C',
        OFLAG_ZLINE, 'z',
        OFLAG_WHOIS, 'W',
        OFLAG_HIDE, 'H',
	OFLAG_TKL, 't',
	OFLAG_GZL, 'Z',
	OFLAG_OVERRIDE, 'v', 
        0, 0
};

ModuleHeader MOD_HEADER(m_svso)
  = {
	"m_svso",
	"$Id$",
	"command /svso", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_svso)(ModuleInfo *modinfo)
{
	add_Command(MSG_SVSO, TOK_SVSO, m_svso, MAXPARA);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_svso)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_svso)(int module_unload)
{
	if (del_Command(MSG_SVSO, TOK_SVSO, m_svso) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				MOD_HEADER(m_svso).name);
	}
	return MOD_SUCCESS;
}
/*
** m_svso - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = nick
**      parv[2] = options
*/

int m_svso(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
        aClient *acptr;
        long fLag;

        if (!IsULine(sptr))
                return 0;

        if (parc < 3)
                return 0;

        if (!(acptr = find_person(parv[1], (aClient *)NULL)))
                return 0;

        if (!MyClient(acptr))
        {
                sendto_one(acptr, ":%s SVSO %s %s", parv[0], parv[1], parv[2]);
                return 0;
        }

        if (*parv[2] == '+')
        {
                int     *i, flag;
                char *m = NULL;
                for (m = (parv[2] + 1); *m; m++)
                {
                        for (i = oper_access; (flag = *i); i += 2)
                        {
                                if (*m == (char) *(i + 1))
                                {
                                        acptr->oflag |= flag;
                                        break;
                                }
                        }
                }
        }
        if (*parv[2] == '-')
        {
                fLag = acptr->umodes;
                if (IsOper(acptr) && !IsHideOper(acptr))
                {
                        IRCstats.operators--;
                        VERIFY_OPERCOUNT(acptr, "svso");
                }
                if (IsAnOper(acptr))
                        delfrom_fdlist(acptr->slot, &oper_fdlist);
                acptr->umodes &=
                    ~(UMODE_OPER | UMODE_LOCOP | UMODE_HELPOP |UMODE_SERVICES |
                    UMODE_SADMIN | UMODE_ADMIN | UMODE_COADMIN);
                acptr->umodes &=
                    ~(UMODE_NETADMIN | UMODE_WHOIS);
                acptr->umodes &=
                    ~(UMODE_KIX | UMODE_DEAF | UMODE_HIDEOPER | UMODE_VICTIM);
                acptr->oflag = 0;
		remove_oper_snomasks(acptr);
		RunHook2(HOOKTYPE_LOCAL_OPER, acptr, 0);
                send_umode_out(acptr, acptr, fLag);
        }
	return 0;
}
