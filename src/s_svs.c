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

#define STAR1 OFLAG_SADMIN|OFLAG_ADMIN|OFLAG_NETADMIN|OFLAG_COADMIN
#define STAR2 OFLAG_TECHADMIN|OFLAG_ZLINE|OFLAG_HIDE|OFLAG_WHOIS
#define STAR3 OFLAG_INVISIBLE

int oper_access[] = {
	~(STAR1 | STAR2 | STAR3), '*',
	OFLAG_LOCAL, 'o',
	OFLAG_GLOBAL, 'O',
	OFLAG_REHASH, 'r',
	OFLAG_EYES, 'e',
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
	OFLAG_TECHADMIN, 'T',
	OFLAG_UMODEC, 'u',
	OFLAG_UMODEF, 'f',
	OFLAG_ZLINE, 'z',
	OFLAG_WHOIS, 'W',
	OFLAG_HIDE, 'H',
	OFLAG_INVISIBLE, '^',
	0, 0
};

char oflagbuf[128];

char *oflagstr(long oflag)
{
	int *i, flag;
	char m;
	char *p = oflagbuf;

	for (i = &oper_access[6], m = *(i + 1); (flag = *i);
	    i += 2, m = *(i + 1))
	{
		if (oflag & flag)
		{
			*p = m;
			p++;
		}
	}
	*p = '\0';
	return oflagbuf;
}


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

/*
** m_svso - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = nick
**      parv[2] = options
*/

int  m_svso(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	long fLag;

	if (!IsULine(sptr))
		return 0;

	if (parc < 3)
		return 0;

	if (!(acptr = find_client(parv[1], (aClient *)NULL)))
		return 0;

	if (!MyClient(acptr))
	{
		sendto_one(acptr, ":%s SVSO %s %s", parv[0], parv[1], parv[2]);
		return 0;
	}

 	if (*parv[2] == '+')
	{
		int	*i, flag;
		char *m = NULL;
		for (m = (parv[2] + 1); *m; m++)
		{
			for (i = oper_access; flag = *i; i += 2)
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
		send_umode_out(acptr, acptr, fLag);
	}
}


/* ** m_akill;
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int  m_akill(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *hostmask, *usermask, *comment;
	ConfigItem_ban *bconf;

	if (parc < 2 && IsPerson(sptr))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "AKILL");
		return 0;
	}

	if (IsServer(sptr) && parc < 3)
		return 0;

	if (!IsServer(cptr))
	{
		if (!IsOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    sptr->name);
			return 0;
		}
		else
		{
			comment = parc < 3 ? NULL : parv[2];
			if ((hostmask = (char *)index(parv[1], '@')))
			{
				*hostmask = 0;
				hostmask++;
				usermask = parv[1];
			}
			else
			{
				sendto_one(sptr, ":%s NOTICE %s :%s", me.name,
				    sptr->name,
				    "Please use a nick!user@host mask.");
				return 0;
			}
			if (!strchr(hostmask, '.'))
			{
				sendto_one(sptr,
				    "NOTICE %s :*** What a sweeping AKILL.  If only your admin knew you tried that..",
				    parv[0]);
				sendto_realops("%s attempted to /akill *@*",
				    parv[0]);
				return 0;
			}
			if (MyClient(sptr))
			{
				sendto_realops("%s added akill for %s@%s (%s)",
				    sptr->name, usermask, hostmask,
				    !BadPtr(comment) ? comment : "no reason");
				sendto_serv_butone(&me,
				    ":%s GLOBOPS :%s added akill for %s@%s (%s)",
				    me.name, sptr->name, usermask, hostmask,
				    !BadPtr(comment) ? comment : "no reason");
			}
		}
	}
	else
	{
		hostmask = parv[1];
		usermask = parv[2];
		comment = parc < 4 ? NULL : parv[3];
	}

	if (!usermask || !hostmask)
	{
		/*
		 * This is very bad, it should never happen.
		 */
		sendto_ops("Error adding akill from %s!", sptr->name);
		return 0;
	}

	if (!(bconf = Find_ban(make_user_host(usermask, hostmask), CONF_BAN_USER)))
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		bconf->flag.type = CONF_BAN_USER;
		bconf->mask = strdup(make_user_host(usermask, hostmask));
		bconf->reason = comment;
		bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
		add_ConfigItem((ConfigItem *) bconf, (ConfigItem **) &conf_ban);
	}

	if (comment)
		sendto_serv_butone(cptr, ":%s AKILL %s %s :%s",
		    IsServer(cptr) ? parv[0] : me.name, hostmask,
		    usermask, comment);
	else
		sendto_serv_butone(cptr, ":%s AKILL %s %s",
		    IsServer(cptr) ? parv[0] : me.name, hostmask, usermask);


	check_pings(TStime(), 1);

}

/*
** m_rakill;
**      parv[0] = sender prefix
**      parv[1] = hostmask
**      parv[2] = username
**      parv[3] = comment
*/
int  m_rakill(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	char *hostmask, *usermask;
	ConfigItem_ban  *bconf;
	int  result;

	if (parc < 2 && IsPerson(sptr))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "RAKILL");
		return 0;
	}

	if (IsServer(sptr) && parc < 3)
		return 0;

	if (!IsServer(cptr))
	{
		if (!IsOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    sptr->name);
			return 0;
		}
		else
		{
			if ((hostmask = (char *)index(parv[1], '@')))
			{
				*hostmask = 0;
				hostmask++;
				usermask = parv[1];
			}
			else
			{
				sendto_one(sptr, ":%s NOTICE %s :*** %s", me.name,
				    sptr->name, "Please use a user@host mask.");
				return 0;
			}
		}
	}
	else
	{
		hostmask = parv[1];
		usermask = parv[2];
	}

	if (!usermask || !hostmask)
	{
		/*
		 * This is very bad, it should never happen.
		 */
		sendto_realops("Error adding akill from %s!", sptr->name);
		return 0;
	}
	
	if (!(bconf = Find_ban(make_user_host(usermask, hostmask), CONF_BAN_USER)))
	{
		if (!MyClient(sptr))
		{
			sendto_serv_butone(cptr, ":%s RAKILL %s %s",
			    IsServer(cptr) ? parv[0] : me.name, hostmask,
			    usermask);
			return 0;
		}
		sendto_one(sptr, ":%s NOTICE %s :*** AKill %s@%s does not exist.",
		    me.name, sptr->name, usermask, hostmask);
		return 0;
		
	}
	if (bconf->flag.type != CONF_BAN_TYPE_AKILL)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Error: Cannot remove other ban types",
			me.name, sptr->name);
		return 0;
	}
	if (MyClient(sptr))
	{
		sendto_ops("%s removed akill for %s@%s",
		    sptr->name, usermask, hostmask);
		sendto_serv_butone(&me,
		    ":%s GLOBOPS :%s removed akill for %s@%s",
		    me.name, sptr->name, usermask, hostmask);
	}
	
	/* Wipe it out. */
	del_ConfigItem(bconf, &conf_me);
	MyFree(bconf->mask);
	if (bconf->reason)
		MyFree(bconf->reason);
	MyFree(bconf);
	
	sendto_serv_butone(cptr, ":%s RAKILL %s %s",
	    IsServer(cptr) ? parv[0] : me.name, hostmask, usermask);

	check_pings(TStime(), 1);
}
