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

aConfiguration iConf;

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

/* ok, given a mask, our job is to determine
 * wether or not it's a safe mask to banish...
 *
 * userhost= mask to verify
 * ipstat= TRUE  == it's an ip
 *         FALSE == it's a hostname
 *         UNSURE == we need to find out
 * return value
 *         TRUE  == mask is ok
 *         FALSE == mask is not ok
 *        UNSURE == [unused] something went wrong
 */

int advanced_check(char *userhost, int ipstat)
{
	register int retval = TRUE;
	char *up, *p, *thisseg;
	int  numdots = 0, segno = 0, numseg, i = 0;
	char *ipseg[10 + 2];
	char safebuffer[512] = "";	/* buffer strtoken() can mess up to its heart's content...;> */

	strcpy(safebuffer, userhost);

#define userhost safebuffer
#define IP_WILDS_OK(x) ((x)<2? 0 : 1)

	if (ipstat == UNSURE)
	{
		ipstat = TRUE;
		for (; *up; up++)
		{
			if (*up == '.')
				numdots++;
			if (!isdigit(*up) && !ispunct(*up))
			{
				ipstat = FALSE;
				continue;
			}
		}
		if (numdots != 3)
			ipstat = FALSE;
		if (numdots < 1 || numdots > 9)
			return (0);
	}

	/* fill in the segment set */
	{
		int  l = 0;
		for (segno = 0, i = 0, thisseg = strtoken(&p, userhost, ".");
		    thisseg; thisseg = strtoken(&p, NULL, "."), i++)
		{

			l = strlen(thisseg) + 2;
			ipseg[segno] = calloc(1, l);
			strncpy(ipseg[segno++], thisseg, l);
		}
	}
	if (segno < 2 && ipstat == TRUE)
		retval = FALSE;
	numseg = segno;
	if (ipstat == TRUE)
		for (i = 0; i < numseg; i++)
		{
			if (!IP_WILDS_OK(i) && index(ipseg[i], '*')
			    || index(ipseg[i], '?'))
				retval = FALSE;
			/* The person who wrote this function was braindead --Stskeeps */
			/* MyFree(ipseg[i]); */
		}
	else
	{
		int  wildsok = 0;

		for (i = 0; i < numseg; i++)
		{
			/* for hosts, let the mask extent all the way to 
			   the second-level domain... */
			wildsok = 1;
			if (i == numseg || (i + 1) == numseg)
				wildsok = 0;
			if (wildsok == 0 && (index(ipseg[i], '*')
			    || index(ipseg[i], '?')))
			{
				retval = FALSE;
			}
			/* MyFree(ipseg[i]); */
		}


	}

	return (retval);
#undef userhost
#undef IP_WILDS_OK

}



int m_svsnoop(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
		    				~(UMODE_NETADMIN | UMODE_TECHADMIN | UMODE_WHOIS);
					acptr->umodes &=
					    ~(UMODE_KIX | UMODE_HIDING | UMODE_DEAF | UMODE_HIDEOPER);
					acptr->user->oflag = 0;
					acptr->user->snomask &= ~(SNO_CLIENT|SNO_FLOOD|SNO_FCLIENT|
						SNO_EYES|SNO_VHOST);
				
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

int m_svso(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
					acptr->user->oflag |= flag;
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
		    ~(UMODE_NETADMIN | UMODE_TECHADMIN | UMODE_WHOIS);
		acptr->umodes &=
		    ~(UMODE_KIX | UMODE_HIDING | UMODE_DEAF | UMODE_HIDEOPER);
		acptr->user->oflag = 0;
		acptr->user->snomask &= ~(SNO_CLIENT|SNO_FLOOD|SNO_FCLIENT|SNO_EYES|SNO_VHOST);
		send_umode_out(acptr, acptr, fLag);
	}
}


/* ** m_akill;
**	parv[0] = sender prefix
**	parv[1] = hostmask
**	parv[2] = username
**	parv[3] = comment
*/
int m_akill(aClient *cptr, aClient *sptr, int parc, char *parv[])
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

	if (!(bconf = Find_banEx(make_user_host(usermask, hostmask), CONF_BAN_USER, CONF_BAN_TYPE_AKILL)))
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		bconf->flag.type = CONF_BAN_USER;
		bconf->mask = strdup(make_user_host(usermask, hostmask));
		bconf->reason = comment ? strdup(comment) : NULL;
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
int m_rakill(aClient *cptr, aClient *sptr, int parc, char *parv[])
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
	
	if (!(bconf = Find_banEx(make_user_host(usermask, hostmask), CONF_BAN_USER, CONF_BAN_TYPE_AKILL)))
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
	if (bconf->flag.type2 != CONF_BAN_TYPE_AKILL)
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
	del_ConfigItem(bconf, &conf_ban);
	MyFree(bconf->mask);
	if (bconf->reason)
		MyFree(bconf->reason);
	MyFree(bconf);
	
	sendto_serv_butone(cptr, ":%s RAKILL %s %s",
	    IsServer(cptr) ? parv[0] : me.name, hostmask, usermask);

	check_pings(TStime(), 1);
}


/*
 *  m_zline                       add a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 *    parv[2] = reason
 */

int m_zline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char userhost[512 + 2] = "", *in;
	int  uline = 0, i = 0, propo = 0;
	char *reason, *mask, *server, *person;
	aClient *acptr;
	ConfigItem_ban *bconf;
	
	reason = mask = server = person = NULL;

	reason = ((parc >= 3) ? parv[parc - 1] : "Reason unspecified");
	mask = ((parc >= 2) ? parv[parc - 2] : NULL);
	server = ((parc >= 4) ? parv[parc - 1] : NULL);

	if (parc == 4)
	{
		mask = parv[parc - 3];
		server = parv[parc - 2];
		reason = parv[parc - 1];
	}

	uline = IsULine(sptr) ? 1 : 0;

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	if (uline)
	{
		if (parc >= 4 && server)
		{
			if (hunt_server(cptr, sptr, ":%s ZLINE %s %s :%s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			propo = 1;
	}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "ZLINE");
		return -1;
	}

	if (acptr = find_client(parv[1], NULL))
	{
		strcpy(userhost, inetntoa((char *)&acptr->ip));
		person = &acptr->name[0];
		acptr = NULL;
	}
	/* z-lines don't support user@host format, they only 
	   work with ip addresses and nicks */
	else if ((in = index(parv[1], '@')) && (*(in + 1) != '\0'))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}
	else if (in && !(*(in + 1)))	/* sheesh not only specifying a ident@, but
					   omitting the ip...? */
	{
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Hey! z:lines need an ip address...",
		    me.name, sptr->name);
		return -1;
	}
	else
	{
		strcpy(userhost, parv[1]);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** z:lines work only with ip addresses (you cannot specify ident either)",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}

	/* this'll protect against z-lining *.* or something */
	if (advanced_check(userhost, TRUE) == FALSE)
	{
		sendto_ops("Bad z:line mask from %s *@%s [%s]", parv[0],
		    userhost, reason ? reason : "");
		if (MyClient(sptr))
			sendto_one(sptr,
			    ":%s NOTICE %s :** *@%s is a bad z:line mask...",
			    me.name, sptr->name, userhost);
		return;
	}

	if (!(bconf = Find_ban(userhost, CONF_BAN_IP)))
	{
		if (uline == 0)
		{
			if (person)	
				sendto_realops("%s added a temp z:line for %s (*@%s) [%s]",
				    parv[0], person, userhost, reason ? reason : "");
			else
				sendto_realops("%s added a temp z:line *@%s [%s]", parv[0],
				    userhost, reason ? reason : "");
		}
		else
		{
			if (person)
				sendto_ops("%s z:lined %s (*@%s) on %s [%s]", parv[0],
				    person, userhost, server ? server : ircnetwork,
				    reason ? reason : "");
			else
				sendto_ops("%s z:lined *@%s on %s [%s]", parv[0],
				    userhost, server ? server : ircnetwork,
				    reason ? reason : "");
		
		}
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		bconf->flag.type = CONF_BAN_IP;
		bconf->mask = strdup(userhost);
		bconf->reason = strdup(reason);
		bconf->flag.type2 = uline ? CONF_BAN_TYPE_AKILL : CONF_BAN_TYPE_TEMPORARY;
	}
	else
	{
		goto propo_label;
	}
	if (!match(mask, inetntoa((char *)&cptr->ip)))
	{
		sendto_realops("Tried to zap cptr, from %s",
			parv[0]);
		MyFree(bconf);	
	}
	else
	{
		add_ConfigItem((ConfigItem *)bconf, (ConfigItem **)&conf_ban);
	}
propo_label:
	if (propo == 1)		/* propo is if a ulined server is propagating a z-line
				   this should go after the above check */
		sendto_serv_butone(cptr, ":%s ZLINE %s :%s", parv[0], parv[1],
		    reason ? reason : "");

	check_pings(TStime(), 1);
}

/*
 *  m_unzline                        remove a temporary zap line
 *    parv[0] = sender prefix
 *    parv[1] = host
 */

int m_unzline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char userhost[512 + 2] = "", *in;
	int  result = 0, uline = 0, akill = 0;
	char *mask, *server;
	ConfigItem_ban *bconf;
	uline = IsULine(sptr) ? 1 : 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "UNZLINE");
		return -1;
	}


	if (parc < 3 || !uline)
	{
		mask = parv[parc - 1];
		server = NULL;
	}
	else if (parc == 3)
	{
		mask = parv[parc - 2];
		server = parv[parc - 1];
	}

	if (!uline && (!MyConnect(sptr) || !OPCanZline(sptr) || !IsOper(sptr)))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return -1;
	}

	/* before we even check ourselves we need to do the uline checks
	   because we aren't supposed to add a z:line if the message is
	   destined to be passed on... */

	if (uline)
	{
		if (parc == 3 && server)
		{
			if (hunt_server(cptr, sptr, ":%s UNZLINE %s %s", 2,
			    parc, parv) != HUNTED_ISME)
				return 0;
			else;
		}
		else
			sendto_serv_butone(cptr, ":%s UNZLINE %s", parv[0],
			    parv[1]);

	}


	/* parse the removal mask the same way so an oper can just use
	   the same thing to remove it if they specified *@ or something... */
	if ((in = index(parv[1], '@')))
	{
		strcpy(userhost, in + 1);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}
	else
	{
		strcpy(userhost, parv[1]);
		in = &userhost[0];
		while (*in)
		{
			if (!isdigit(*in) && !ispunct(*in))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** it's not possible to have a z:line that's not an ip addresss...",
				    me.name, sptr->name);
				return;
			}
			in++;
		}
	}

	akill = 0;
      retry_unzline:

	bconf = Find_ban(userhost, CONF_BAN_IP);
	if (!bconf)
	{
		if (MyClient(sptr))
			sendto_one(sptr, ":%s NOTICE %s :*** Cannot find z:line %s",
				me.name, sptr->name, userhost);
		return 0;
	}
	
	if (uline == 0)
	{
		if (bconf->flag.type2 != CONF_BAN_TYPE_TEMPORARY)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** You may not remove permanent z:lines.",
				me.name, sptr->name);
			return 0;
		}			
	}
	del_ConfigItem((ConfigItem *)bconf, (ConfigItem **)&conf_ban);
	sendto_realops("%s removed z:line %s", parv[0], userhost);
	if (bconf->mask)
		MyFree(bconf->mask);
	if (bconf->reason)
		MyFree(bconf->reason);
	MyFree(bconf);

	return 0;
}


/*
** m_kline;
**	parv[0] = sender prefix
**	parv[1] = nickname
**	parv[2] = comment or filename
*/
int m_kline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *host, *tmp, *hosttemp;
	char uhost[80], name[80];
	int  ip1, ip2, ip3, temp;
	aClient *acptr;
	ConfigItem_ban *bconf;

	if (!MyClient(sptr) || !OPCanKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}


	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KLINE");
		return 0;
	}


/* This patch allows opers to quote kline by address as well as nick
 * --Russell
 */
	if (hosttemp = (char *)strchr((char *)parv[1], '@'))
	{
		*hosttemp = 0;
		hosttemp++;
		bzero(name, sizeof(name));
		bzero(uhost, sizeof(uhost));
		
		strncpy(name, parv[1], sizeof(name) - 1);
		strncpy(uhost, hosttemp, sizeof(uhost) - 1);
		
		if (name[0] == '\0' || uhost[0] == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field!"));
			sendto_one(sptr,
			    "NOTICE %s :*** If you're going to add a userhost, at LEAST specify both fields",
			    parv[0]);
			return 0;
		}
		if (!strcmp(uhost, "*") || !strchr(uhost, '.'))
		{
			sendto_one(sptr,
			    "NOTICE %s :*** What a sweeping K:Line.  If only your admin knew you tried that..",
			    parv[0]);
			sendto_realops("%s attempted to /kline *@*", parv[0]);
			return 0;
		}
	}

/* by nick */
	else
	{
		if (!(acptr = find_client(parv[1], NULL)))
		{
			if (!(acptr =
			    get_history(parv[1], (long)KILLCHASETIMELIMIT)))
			{
				sendto_one(sptr,
				    "NOTICE %s :*** Can't find user %s to add KLINE",
				    parv[0], parv[1]);
				return 0;
			}
		}

		if (!acptr->user)
			return 0;

		strcpy(name, acptr->user->username);
		if (MyClient(acptr))
			host = acptr->sockhost;
		else
			host = acptr->user->realhost;

		/* Sanity checks */

		if (name == '\0' || host == '\0')
		{
			Debug((DEBUG_INFO, "KLINE: Bad field"));
			sendto_one(sptr, "NOTICE %s :*** Bad field!", parv[0]);
			return 0;
		}

		/* Add some wildcards */


		strcpy(uhost, host);
		if (isdigit(host[strlen(host) - 1]))
		{
			if (sscanf(host, "%d.%d.%d.%*d", &ip1, &ip2, &ip3))
				ircsprintf(uhost, "%d.%d.%d.*", ip1, ip2, ip3);
		}
		else if (sscanf(host, "%*[^.].%*[^.].%s", uhost))
		{		/* Not really... */
			tmp = (char *)strchr(host, '.');
			ircsprintf(uhost, "*%s", tmp);
		}
	}

	sendto_realops("%s added a temporary user ban for %s@%s %s", parv[0], name, uhost,
	    parv[2] ? parv[2] : "");
	ircd_log(LOG_KLINE, "%s added a temporary user ban for %s@%s %s",
	   parv[0], name, uhost,
	    parv[2] ? parv[2] : "");
	bconf = (ConfigItem_ban *)MyMallocEx(sizeof(ConfigItem_ban));
	bconf->flag.type = CONF_BAN_USER;
	bconf->mask = strdup(make_user_host(name, uhost));
	bconf->reason = parv[2] ? strdup(parv[2]) : NULL;
	bconf->flag.type2 = CONF_BAN_TYPE_TEMPORARY;
	add_ConfigItem((ConfigItem *) bconf, (ConfigItem **) &conf_ban);
	check_pings(TStime(), 1);
}

/*
 *  m_unkline
 *    parv[0] = sender prefix
 *    parv[1] = userhost
 */

int m_unkline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{

	int  result, temp;
	char *hosttemp = parv[1], host[80], name[80];
	ConfigItem_ban *bconf;
	
	
	if (!MyClient(sptr) || !OPCanUnKline(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	if (parc < 2)
	{
		sendto_one(sptr, ":%s NOTICE %s :*** Not enough parameters", me.name, parv[0]);
		return 0;
	}
	if (hosttemp = (char *)strchr((char *)parv[1], '@'))
	{
		*hosttemp = 0;
		hosttemp++;
		bzero(name, sizeof(name));
		bzero(host, sizeof(host));
		
		strncpy(name, parv[1], sizeof(name) - 1);
		strncpy(host, hosttemp, sizeof(host) - 1);
		if (name[0] == '\0' || host[0] == '\0')
		{
			Debug((DEBUG_INFO, "UNKLINE: Bad field"));
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Both user and host fields must be non-null",
			    me.name, parv[0]);
			return 0;
		}
		if (!(bconf = Find_banEx(make_user_host(name, host), CONF_BAN_USER, CONF_BAN_TYPE_TEMPORARY)))
		{
			sendto_one(sptr, ":%s NOTICE %s :*** Cannot find user ban %s@%s",
				me.name, parv[0], name, host);
			return 0;
		}
		if (bconf->flag.type2 != CONF_BAN_TYPE_TEMPORARY)
		{
			sendto_one(sptr, ":%s NOTICE %s :*** You cannot remove permament user bans",
				me.name, sptr->name);
			return 0;
		}
		
		del_ConfigItem((ConfigItem *)bconf, (ConfigItem **)&conf_ban);
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		MyFree(bconf);
		
		sendto_one(sptr,
			":%s NOTICE %s :*** Temporary user ban %s@%s is now removed.",
			    me.name, parv[0], name, host);
		sendto_realops("%s removed temporary user ban %s@%s", parv[0],
		    name, host);
		ircd_log(LOG_KLINE,
		    "%s removed temporary user ban %s@%s",
		    parv[0], name, host);
		return 0;
	}
	/* This wasn't here before -- Barubary */
	check_pings(TStime(), 1);
}


/*    m_sqline
**	parv[0] = sender
**	parv[1] = nickmask
**	parv[2] = reason
*/
int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban	*bconf;
	/* So we do not make double entries */
	int		addit = 0;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	if (parv[2])
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s :%s", parv[1], parv[2]);
	else
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s", parv[1]);

	/* Only replaces AKILL (global ban nick)'s */
	if (bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL))
	{
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		bconf->mask = NULL;
		bconf->reason = NULL;
		addit = 0;
	}
	else
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		addit = 1;
	}
	if (parv[2])
		DupString(bconf->reason, parv[2]);
	if (parv[1])
		DupString(bconf->mask, parv[1]);
		
	/* CONF_BAN_NICK && CONF_BAN_TYPE_AKILL == SQLINE */
	bconf->flag.type = CONF_BAN_NICK;
	bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
	if (addit == 1)
		add_ConfigItem((ConfigItem *) bconf, (ConfigItem **) &conf_ban);

}
/*    m_unsqline
**	parv[0] = sender
**	parv[1] = nickmask
*/
int m_unsqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban *bconf;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	sendto_serv_butone_token(cptr, parv[0], MSG_UNSQLINE, TOK_UNSQLINE,
	    "%s", parv[1]);

	if (bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL))
	{
		del_ConfigItem(bconf, &conf_ban);
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		MyFree(bconf);
	}
	else
		return;

	return 0;
}

int m_alias(aClient *cptr, aClient *sptr, int parc, char *parv[], char *cmd) {
	ConfigItem_alias *alias;
	aClient *acptr;
	if (parc < 2 || *parv[1] == '\0') {
		sendto_one(sptr, err_str(ERR_NOTEXTTOSEND), me.name, parv[0]);
		return -1;
	}
	if (!(alias = Find_alias(cmd))) {
		sendto_one(sptr, ":%s %d %s %s :Unknown command",
			me.name, ERR_UNKNOWNCOMMAND, parv[0], cmd);
		return 0;
	}

	if (alias->type == ALIAS_SERVICES) {
		if (SERVICES_NAME && (acptr = find_person(alias->nick, NULL)))
			sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", parv[0],
				alias->nick, SERVICES_NAME, parv[1]);
		else
			sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
				parv[0], alias->nick);
	}
	else if (alias->type == ALIAS_STATS) {
		if (STATS_SERVER && (acptr = find_person(alias->nick, NULL)))
			sendto_one(acptr, ":%s PRIVMSG %s@%s :%s", parv[0],
				alias->nick, STATS_SERVER, parv[1]);
		else
			sendto_one(sptr, err_str(ERR_SERVICESDOWN), me.name,
				parv[0], alias->nick);
	}
	else if (alias->type == ALIAS_NORMAL) {
		if ((acptr = find_person(alias->nick, NULL)))
			sendto_one(acptr, ":%s PRIVMSG %s :%s", parv[0],
				alias->nick, parv[1]);
		else
			sendto_one(sptr, err_str(ERR_NOSUCHNICK), me.name,
				parv[0], alias->nick);
	}			
	return 0;
}
