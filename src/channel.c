/* Unreal Internet Relay Chat Daemon, src/channel.c
 *   Copyright (C) 1990 Jarkko Oikarinen and
 *                      University of Oulu, Co Center
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
/*
 * 23 Jun 1999
 * Changing unsigned int modes to long
 * --- Sts - 28 May 1999
    Incorporated twilight mode system
*/
/* -- Jto -- 09 Jul 1990
 * Bug fix
 */

/* -- Jto -- 03 Jun 1990
 * Moved m_channel() and related functions from s_msg.c to here
 * Many changes to start changing into string channels...
 */

/* -- Jto -- 24 May 1990
 * Moved is_full() from list.c
 */


#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "channel.h"
#include "msg.h"		/* For TOK_*** and MSG_*** strings  */
#include "hash.h"		/* For CHANNELHASHSIZE */
#include "h.h"
#include "proto.h"
#include <string.h>

ID_Copyright
    ("(C) 1990 University of Oulu, Computing Center and Jarkko Oikarinen");

long opermode = 0;
aChannel *channel = NullChn;
extern char backupbuf[];
extern ircstats IRCstats;

#ifndef NO_FDLIST
extern int lifesux;
#endif

/* Some forward declarations */
void add_invite(aClient *, aChannel *);
char *clean_ban_mask(char *, int, aClient *);
void channel_modes(aClient *, char *, char *, aChannel *);
int check_channelmask(aClient *, aClient *, char *);

void sub1_from_channel(aChannel *);

void clean_channelname(char *);
void del_invite(aClient *, aChannel *);

#ifdef NEWCHFLOODPROT
void chanfloodtimer_del(aChannel *chptr, char mflag, long mbit);
void chanfloodtimer_stopchantimers(aChannel *chptr);
#endif

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static char nickbuf[BUFSIZE], buf[BUFSIZE];
char modebuf[BUFSIZE], parabuf[BUFSIZE];
#include "sjoin.h"

#define MODESYS_LINKOK		/* We do this for a TEST  */
aCtab cFlagTab[] = {
	{MODE_LIMIT, 'l', 0, 1},
	{MODE_VOICE, 'v', 1, 1},
	{MODE_HALFOP, 'h', 0, 1},
	{MODE_CHANOP, 'o', 0, 1},
	{MODE_PRIVATE, 'p', 0, 0},
	{MODE_SECRET, 's', 0, 0},
	{MODE_MODERATED, 'm', 1, 0},
	{MODE_NOPRIVMSGS, 'n', 1, 0},
	{MODE_TOPICLIMIT, 't', 1, 0},
	{MODE_INVITEONLY, 'i', 1, 0},
	{MODE_KEY, 'k', 1, 1},
	{MODE_RGSTR, 'r', 0, 0},
	{MODE_RGSTRONLY, 'R', 0, 0},
	{MODE_NOCOLOR, 'c', 0, 0},
	{MODE_CHANPROT, 'a', 0, 1},
	{MODE_CHANOWNER, 'q', 0, 1},
	{MODE_OPERONLY, 'O', 0, 0},
	{MODE_ADMONLY, 'A', 0, 0},
	{MODE_LINK, 'L', 0, 1},
	{MODE_NOKICKS, 'Q', 0, 0},
	{MODE_BAN, 'b', 1, 1},
	{MODE_STRIP, 'S', 0, 0},	/* works? */
	{MODE_EXCEPT, 'e', 1, 0},	/* exception ban */
	{MODE_INVEX, 'I', 1, 0},	/* exception ban */
	{MODE_NOKNOCK, 'K', 0, 0},	/* knock knock (no way!) */
	{MODE_NOINVITE, 'V', 0, 0},	/* no invites */
	{MODE_FLOODLIMIT, 'f', 0, 1},	/* flood limiter */
	{MODE_MODREG, 'M', 0, 0},	/* Need umode +r to talk */
	{MODE_NOCTCP, 'C', 0, 0},	/* no CTCPs */
	{MODE_AUDITORIUM, 'u', 0, 0},
	{MODE_ONLYSECURE, 'z', 0, 0},
	{MODE_NONICKCHANGE, 'N', 0, 0},
	{0x0, 0x0, 0x0}
};


#define	BADOP_BOUNCE	1
#define	BADOP_USER	2
#define	BADOP_SERVER	3
#define	BADOP_OVERRIDE	4

char cmodestring[512];

inline int op_can_override(aClient *sptr)
{
#ifndef NO_OPEROVERRIDE
	if (!IsOper(sptr))
		return 0;
	if (MyClient(sptr) && !OPCanOverride(sptr))
		return 0;
	return 1;
#else
	return 0;
#endif
}

void make_cmodestr(void)
{
	char *p = &cmodestring[0];
	aCtab *tab = &cFlagTab[0];
#ifdef EXTCMODE
	int i;
#endif
	while (tab->mode != 0x0)
	{
		*p = tab->flag;
		p++;
		tab++;
	}
#ifdef EXTCMODE
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
#endif
	*p = '\0';
}

int  Halfop_mode(long mode)
{
	aCtab *tab = &cFlagTab[0];

	while (tab->mode != 0x0)
	{
		if (tab->mode == mode)
			return (tab->halfop == 1 ? TRUE : FALSE);
		tab++;
	}
	return TRUE;
}


/*
 * return the length (>=0) of a chain of links.
 */
static int list_length(Link *lp)
{
	int  count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

Member	*find_member_link(Member *lp, aClient *ptr)
{
	if (ptr)
		while (lp)
		{
			if (lp->cptr == ptr)
				return (lp);
			lp = lp->next;
		}	
	return NULL;
}

Membership *find_membership_link(Membership *lp, aChannel *ptr)
{
	if (ptr)
		while (lp)
		{
			if (lp->chptr == ptr)
				return (lp);
			lp = lp->next;
		}
	return NULL;
}
/* 
 * Member functions
*/
Member	*make_member(void)
{
	Member *lp;
	unsigned int	i;

	if (freemember == NULL)
	{
		for (i = 1; i <= (4072/sizeof(Member)); ++i)		
		{
			lp = (Member *)MyMalloc(sizeof(Member));
			lp->cptr = NULL;
			lp->flags = 0;
			lp->next = freemember;
			freemember = lp;
		}
	}
	lp = freemember;
	freemember = freemember->next;
	lp->next = NULL;
	return lp;
}

void	free_member(Member *lp)
{
	if (lp)
	{
		lp->next = freemember;
		lp->cptr = NULL;
		lp->flags = 0;
		freemember = lp;
	}
}

/* 
 * Membership functions
*/
Membership	*make_membership(int local)
{
	Membership *lp = NULL;
	MembershipL *lp2 = NULL;
	unsigned int	i;

	if (!local)
	{
		if (freemembership == NULL)
		{
			for (i = 1; i <= (4072/sizeof(Membership)); i++)
			{
				lp = (Membership *)MyMalloc(sizeof(Membership));
				lp->next = freemembership;
				freemembership = lp;
			}
			lp = freemembership;
			freemembership = lp->next;
		}
		else
		{
			lp = freemembership;
			freemembership = freemembership->next;
		}
		bzero(lp, sizeof(Membership));
	}
	else
	{
		if (freemembershipL == NULL)
		{
			for (i = 1; i <= (4072/sizeof(MembershipL)); i++)		
			{
				lp2 = (MembershipL *)MyMalloc(sizeof(MembershipL));
				lp2->next = (Membership *) freemembershipL;
				freemembershipL = lp2;
			}
			lp2 = freemembershipL;
			freemembershipL = (MembershipL *) lp2->next;
			Debug((DEBUG_ERROR, "floodmode::alloc gotone"));
		}
		else
		{
			lp2 = freemembershipL;
			freemembershipL = (MembershipL *) freemembershipL->next;
			Debug((DEBUG_ERROR, "floodmode::freelist gotone"));
		}
		Debug((DEBUG_ERROR, "floodmode:: bzeroing"));	
		bzero(lp2, sizeof(MembershipL));
	}
	if (local)
	{
		return ((Membership *) lp2);
	}
	return lp;
}

void	free_membership(Membership *lp, int local)
{
	if (lp)
	{
		if (!local)
		{
			lp->next = freemembership;
			freemembership = lp;
		}
		else
		{
			lp->next = (Membership *) freemembershipL;
			freemembershipL = (MembershipL *) lp;
		}
	}
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
aClient *find_chasing(aClient *sptr, char *user, int *chasing)
{
	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
	{
		if (!IsServer(who))
			return who;
		else
			return NULL;
	}
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
		    me.name, sptr->name, user);
		return NULL;
	}
	if (chasing)
		*chasing = 1;
	if (!IsServer(who))
		return who;
	else return NULL;
}

/*
 * add_listmode - Add a listmode (+beI) with the specified banid to
 *                the specified channel.
 */

int add_listmode(Ban **list, aClient *cptr, aChannel *chptr, char *banid)
{
	Ban *ban;
	int cnt = 0, len;

	if (MyClient(cptr))
		(void)collapse(banid);
	
	len = strlen(banid);
	if (!*list && ((len > MAXBANLENGTH) || (MAXBANS < 1)))
	{
		sendto_one(cptr, err_str(ERR_BANLISTFULL),
			me.name, cptr->name, chptr->chname, banid);
		return -1;
	}
	for (ban = *list; ban; ban = ban->next)
	{
		len += strlen(ban->banstr);
		if (MyClient(cptr))
			if ((len > MAXBANLENGTH) || (++cnt >= MAXBANS))
			{
				sendto_one(cptr, err_str(ERR_BANLISTFULL),
				    me.name, cptr->name, chptr->chname, banid);
				return -1;
			}
			else
			{
			  /* Temp workaround added in b19. -- Syzop */
			  if (!mycmp(ban->banstr, banid) || (!strchr(banid, '\\') && !strchr(ban->banstr, '\\')))
				if (!match(ban->banstr, banid))
					return -1;
			}
		else if (!mycmp(ban->banstr, banid))
			return -1;

	}
	ban = make_ban();
	bzero((char *)ban, sizeof(Ban));
	ban->next = *list;
	ban->banstr = (char *)MyMalloc(strlen(banid) + 1);
	(void)strcpy(ban->banstr, banid);
	ban->who = (char *)MyMalloc(strlen(cptr->name) + 1);
	(void)strcpy(ban->who, cptr->name);
	ban->when = TStime();
	*list = ban;
	return 0;
}
/*
 * del_listmode - delete a listmode (+beI) from a channel
 *                that matches the specified banid.
 */
int del_listmode(Ban **list, aChannel *chptr, char *banid)
{
	Ban **ban;
	Ban *tmp;

	if (!banid)
		return -1;
	for (ban = list; *ban; ban = &((*ban)->next))
	{
		if (mycmp(banid, (*ban)->banstr) == 0)
		{
			tmp = *ban;
			*ban = tmp->next;
			MyFree(tmp->banstr);
			MyFree(tmp->who);
			free_ban(tmp);
			return 0;
		}
	}
	return -1;
}

/*
 * IsMember - returns 1 if a person is joined
 * Moved to struct.h
 */

/* Those 3 pointers can be used by extended ban modules so they
 * don't have to do 3 make_nick_user_host()'s all the time:
 */
char *ban_realhost = NULL, *ban_virthost = NULL, *ban_ip = NULL;

/** is_banned - Check if a user is banned on a channel.
 * @param sptr   Client to check (can be remote client)
 * @param chptr  Channel to check
 * @param type   Type of ban to check for (BANCHK_*)
 * @returns      A pointer to the ban struct if banned, otherwise NULL.
 * @comments     Simple wrapper for is_banned_with_nick()
 */
inline Ban *is_banned(aClient *sptr, aChannel *chptr, int type)
{
	return is_banned_with_nick(sptr, chptr, type, sptr->name);
}

/** is_banned_with_nick - Check if a user is banned on a channel.
 * @param sptr   Client to check (can be remote client)
 * @param chptr  Channel to check
 * @param type   Type of ban to check for (BANCHK_*)
 * @param nick   Nick of the user
 * @returns      A pointer to the ban struct if banned, otherwise NULL.
 */
Ban *is_banned_with_nick(aClient *sptr, aChannel *chptr, int type, char *nick)
{
	Ban *tmp, *tmp2;
	char *s;
	static char realhost[NICKLEN + USERLEN + HOSTLEN + 6];
	static char virthost[NICKLEN + USERLEN + HOSTLEN + 6];
	static char     nuip[NICKLEN + USERLEN + HOSTLEN + 6];
	int dovirt = 0, mine = 0;
	Extban *extban;

	if (!IsPerson(sptr))
		return NULL;

	ban_realhost = realhost;
	ban_ip = ban_virthost = NULL;

	if (MyConnect(sptr)) {
		mine = 1;
		s = make_nick_user_host(nick, sptr->user->username, GetIP(sptr));
		strlcpy(nuip, s, sizeof nuip);
		ban_ip = nuip;
	}

	if (sptr->user->virthost)
		if (strcmp(sptr->user->realhost, sptr->user->virthost))
		{
			dovirt = 1;
		}

	s = make_nick_user_host(nick, sptr->user->username,
	    sptr->user->realhost);
	strlcpy(realhost, s, sizeof realhost);

	if (dovirt)
	{
		s = make_nick_user_host(nick, sptr->user->username,
		    sptr->user->virthost);
		strlcpy(virthost, s, sizeof virthost);
		ban_virthost = virthost;
	}
		/* We now check +b first, if a +b is found we then see if there is a +e.
 * If a +e was found we return NULL, if not, we return the ban.
 */
	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
	{
		if (*tmp->banstr == '~')
		{
			extban = findmod_by_bantype(tmp->banstr[1]);
			if (!extban)
				continue;
			if (!extban->is_banned(sptr, chptr, tmp->banstr, type))
				continue;
		} else {
			if ((match(tmp->banstr, realhost) == 0) ||
			    (dovirt && (match(tmp->banstr, virthost) == 0)) ||
			    (mine && (match(tmp->banstr, nuip) == 0)))
			{
				/* matches.. do nothing */
			} else
				continue;
		}

		/* Ban found, now check for +e */
		for (tmp2 = chptr->exlist; tmp2; tmp2 = tmp2->next)
		{
			if (*tmp2->banstr == '~')
			{
				extban = findmod_by_bantype(tmp2->banstr[1]);
				if (!extban)
					continue;
				if (extban->is_banned(sptr, chptr, tmp2->banstr, type))
					return NULL;
			} else {
				if ((match(tmp2->banstr, realhost) == 0) ||
					(dovirt && (match(tmp2->banstr, virthost) == 0)) ||
					(mine && (match(tmp2->banstr, nuip) == 0)) )
					return NULL;
			}
		}
		break; /* ban found and not on except */
	}

	return (tmp);
}

/*
 * Checks if the "user" IRC is banned, used by +mu.
 */
static int is_irc_banned(aChannel *chptr)
{
	Ban *tmp;
	/* Check for this user, ident/host are "illegal" on purpose */
	char *check = "IRC!\001@\001";
	
	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if (match(tmp->banstr, check) == 0)
		{
			/* Ban found, now check for +e */
			for (tmp = chptr->exlist; tmp; tmp = tmp->next)
				if (match(tmp->banstr, check) == 0)
					return 0; /* In exception list */
			return 1;
		}
	return 0;
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
void add_user_to_channel(aChannel *chptr, aClient *who, int flags)
{
	Member *ptr;
	Membership *ptr2;

	if (who->user)
	{
		ptr = make_member();
		ptr->cptr = who;
		ptr->flags = flags;
		ptr->next = chptr->members;
		chptr->members = ptr;
		chptr->users++;

		ptr2 = make_membership(MyClient(who));
		/* we should make this more efficient --stskeeps 
		   is now, as we only use it in membership */
		ptr2->chptr = chptr;
		ptr2->next = who->user->channel;
		ptr2->flags = flags;
		who->user->channel = ptr2;
		who->user->joined++;
	}
}

void remove_user_from_channel(aClient *sptr, aChannel *chptr)
{
	Member **curr; Membership **curr2;
	Member *tmp; Membership *tmp2;
	Member *lp = chptr->members;

	/* find 1st entry in list that is not user */
	for (; lp && (lp->cptr == sptr); lp = lp->next);
	for (;;)
	{
		for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
			if (tmp->cptr == sptr)
			{
				*curr = tmp->next;
				free_member(tmp);
				break;
			}
		for (curr2 = &sptr->user->channel; (tmp2 = *curr2); curr2 = &tmp2->next)
			if (tmp2->chptr == chptr)
			{
				*curr2 = tmp2->next;
				free_membership(tmp2, MyClient(sptr));
				break;
			}
		sptr->user->joined--;
		if (lp)
			break;
		if (chptr->members)
			sptr = chptr->members->cptr;
		else
			break;
		sub1_from_channel(chptr);
	}
	sub1_from_channel(chptr);
}

long get_access(aClient *cptr, aChannel *chptr)
{
	Membership *lp;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			return lp->flags;
	return 0;
}

int  is_chan_op(aClient *cptr, aChannel *chptr)
{
	Membership *lp;
/* chanop/halfop ? */
	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
#ifdef PREFIX_AQ
			return ((lp->flags & (CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER)));
#else
			return ((lp->flags & CHFL_CHANOP));
#endif

	return 0;
}

int  has_voice(aClient *cptr, aChannel *chptr)
{
	Membership *lp;

	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}
int  is_halfop(aClient *cptr, aChannel *chptr)
{
	Membership *lp;

	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			if (!(lp->flags & CHFL_CHANOP))
				return (lp->flags & CHFL_HALFOP);

	return 0;
}

int  is_chanowner(aClient *cptr, aChannel *chptr)
{
	Membership *lp;

	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			return (lp->flags & CHFL_CHANOWNER);

	return 0;
}

int is_chanownprotop(aClient *cptr, aChannel *chptr) {
	Membership *lp;
		
	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			if (lp->flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP))
				return 1;
	return 0;
}

int is_skochanop(aClient *cptr, aChannel *chptr) {
	Membership *lp;
		
	if (IsServer(cptr))
		return 1;
	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
#ifdef PREFIX_AQ
			if (lp->flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP|CHFL_HALFOP))
#else
			if (lp->flags & (CHFL_CHANOP|CHFL_HALFOP))
#endif
				return 1;
	return 0;
}

int  is_chanprot(aClient *cptr, aChannel *chptr)
{
	Membership *lp;

	if (chptr)
		if ((lp = find_membership_link(cptr->user->channel, chptr)))
			return (lp->flags & CHFL_CHANPROT);

	return 0;
}

#define CANNOT_SEND_MODERATED 1
#define CANNOT_SEND_NOPRIVMSGS 2
#define CANNOT_SEND_NOCOLOR 3
#define CANNOT_SEND_BAN 4
#define CANNOT_SEND_NOCTCP 5
#define CANNOT_SEND_MODREG 6
#define CANNOT_SEND_SWEAR 7 /* This isn't actually used here */
#define CANNOT_SEND_NOTICE 8 

int  can_send(aClient *cptr, aChannel *chptr, char *msgtext, int notice)
{
	Membership *lp;
	int  member;
	/* 
	 * #0000053 by |savage|, speedup 
	*/
	
	if (!MyClient(cptr))
	{
		if (IsClient(cptr))
		{
			/* channelmode +mu is a special case.. sux!. -- Syzop */		

			lp = find_membership_link(cptr->user->channel, chptr);
			if ((chptr->mode.mode & MODE_MODERATED) && (chptr->mode.mode & MODE_AUDITORIUM) &&
			    !IsOper(cptr) &&
		        (!lp || !(lp->flags & (CHFL_CHANOP|CHFL_VOICE|CHFL_CHANOWNER|CHFL_HALFOP|CHFL_CHANPROT))) &&
		        !is_irc_banned(chptr))
		    {
				sendto_chmodemucrap(cptr, chptr, msgtext);
				return (CANNOT_SEND_MODERATED);
			}
		}
		return 0;
	}

	if (chptr->mode.mode & MODE_NOCOLOR)
	{
		/* A bit faster */
		char *c;
		for (c = msgtext; *c; c++)
		{
			if (*c == 3 || *c == 27 || *c == 4)
				return (CANNOT_SEND_NOCOLOR);
		}
	}
	member = IsMember(cptr, chptr);
	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (CANNOT_SEND_NOPRIVMSGS);

	lp = find_membership_link(cptr->user->channel, chptr);
	if ((chptr->mode.mode & MODE_MODREG) && !op_can_override(cptr) && !IsRegNick(cptr) && 
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))))
		return CANNOT_SEND_MODREG;
	if (chptr->mode.mode & MODE_MODERATED && !op_can_override(cptr) &&
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))))
	    {
			if ((chptr->mode.mode & MODE_AUDITORIUM) && !is_irc_banned(chptr) && !is_banned(cptr, chptr, BANCHK_MSG))
				sendto_chmodemucrap(cptr, chptr, msgtext);
			return (CANNOT_SEND_MODERATED);
	    }

	if (chptr->mode.mode & MODE_NOCTCP &&
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_CHANOWNER | CHFL_CHANPROT))))
		if (msgtext[0] == 1 && strncmp(&msgtext[1], "ACTION ", 7))
			return (CANNOT_SEND_NOCTCP);

#ifdef EXTCMODE
	if (notice && (chptr->mode.extmode & EXTMODE_NONOTICE) &&
	   (!lp || !(lp->flags & (CHFL_CHANOP | CHFL_CHANOWNER | CHFL_CHANPROT))))
		return (CANNOT_SEND_NOTICE);
#endif


	/* Makes opers able to talk thru bans -Stskeeps suggested by The_Cat */
	if (IsOper(cptr))
		return 0;

	if ((!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))) && MyClient(cptr)
	    && is_banned(cptr, chptr, BANCHK_MSG))
		return (CANNOT_SEND_BAN);

	return 0;
}

/* [just a helper for channel_modef_string()] */
static inline char *chmodefstrhelper(char *buf, char t, char tdef, unsigned short l, unsigned char a, unsigned char r)
{
char *p;
char tmpbuf[16], *p2 = tmpbuf;

	ircsprintf(buf, "%hd", l);
	p = buf + strlen(buf);
	*p++ = t;
	if (a && ((a != tdef) || r))
	{
		*p++ = '#';
		*p++ = a;
		if (r)
		{
			sprintf(tmpbuf, "%hd", (short)r);
			while ((*p = *p2++))
				p++;
		}
	}
	*p++ = ',';
	return p;
}

/** returns the channelmode +f string (ie: '[5k,40j]:10') */
char *channel_modef_string(ChanFloodProt *x)
{
static char retbuf[512]; /* overkill :p */
char *p = retbuf;
	*p++ = '[';

	/* (alphabetized) */
	if (x->l[FLD_CTCP])
		p = chmodefstrhelper(p, 'c', 'C', x->l[FLD_CTCP], x->a[FLD_CTCP], x->r[FLD_CTCP]);
	if (x->l[FLD_JOIN])
		p = chmodefstrhelper(p, 'j', 'i', x->l[FLD_JOIN], x->a[FLD_JOIN], x->r[FLD_JOIN]);
	if (x->l[FLD_KNOCK])
		p = chmodefstrhelper(p, 'k', 'K', x->l[FLD_KNOCK], x->a[FLD_KNOCK], x->r[FLD_KNOCK]);
	if (x->l[FLD_MSG])
		p = chmodefstrhelper(p, 'm', 'm', x->l[FLD_MSG], x->a[FLD_MSG], x->r[FLD_MSG]);
	if (x->l[FLD_NICK])
		p = chmodefstrhelper(p, 'n', 'N', x->l[FLD_NICK], x->a[FLD_NICK], x->r[FLD_NICK]);
	if (x->l[FLD_TEXT])
		p = chmodefstrhelper(p, 't', '\0', x->l[FLD_TEXT], x->a[FLD_TEXT], x->r[FLD_TEXT]);

	if (*(p - 1) == ',')
		p--;
	*p++ = ']';
	ircsprintf(p, ":%hd", x->per);
	return retbuf;
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
void channel_modes(aClient *cptr, char *mbuf, char *pbuf, aChannel *chptr)
{
	aCtab *tab = &cFlagTab[0];
	char bcbuf[1024];
#ifdef EXTCMODE
	int i;
#endif

	*mbuf++ = '+';
	/* Paramless first */
	while (tab->mode != 0x0)
	{
		if ((chptr->mode.mode & tab->mode))
			if (!tab->parameters)
				*mbuf++ = tab->flag;
		tab++;
	}
#ifdef EXTCMODE
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (Channelmode_Table[i].flag && !Channelmode_Table[i].paracount &&
		    (chptr->mode.extmode & Channelmode_Table[i].mode))
			*mbuf++ = Channelmode_Table[i].flag;
	}
#endif
	if (chptr->mode.limit)
	{
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr))
			(void)ircsprintf(pbuf, "%d ", chptr->mode.limit);
	}
	if (*chptr->mode.key)
	{
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr))
		{
			/* FIXME: hope pbuf is long enough */
			(void)snprintf(bcbuf, sizeof bcbuf, "%s ", chptr->mode.key);
			(void)strcat(pbuf, bcbuf);
		}
	}
	if (*chptr->mode.link)
	{
		*mbuf++ = 'L';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr))
		{
			/* FIXME: is pbuf long enough?  */
			(void)snprintf(bcbuf, sizeof bcbuf, "%s ", chptr->mode.link);
			(void)strcat(pbuf, bcbuf);
		}
	}
	/* if we add more parameter modes, add a space to the strings here --Stskeeps */
#ifdef NEWCHFLOODPROT
	if (chptr->mode.floodprot)
#else
	if (chptr->mode.per)
#endif
	{
		*mbuf++ = 'f';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr))
		{
#ifdef NEWCHFLOODPROT
			ircsprintf(bcbuf, "%s ", channel_modef_string(chptr->mode.floodprot));
#else
			if (chptr->mode.kmode == 1)
				ircsprintf(bcbuf, "*%i:%i ", chptr->mode.msgs, chptr->mode.per);
			else
				ircsprintf(bcbuf, "%i:%i ", chptr->mode.msgs, chptr->mode.per);
#endif
			(void)strcat(pbuf, bcbuf);
		}
	}

#ifdef EXTCMODE
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (Channelmode_Table[i].flag && Channelmode_Table[i].paracount &&
		    (chptr->mode.extmode & Channelmode_Table[i].mode))
		{
			*mbuf++ = Channelmode_Table[i].flag;
			strcat(pbuf, Channelmode_Table[i].get_param(extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[i].flag)));
			strcat(pbuf, " ");
		}
	}
#endif

	/* Remove the trailing space from the parameters -- codemastr */
	if (*pbuf)
		pbuf[strlen(pbuf)-1]=0;

	*mbuf++ = '\0';
	return;
}

static int send_mode_list(aClient *cptr, char *chname, TS creationtime, Member *top, int mask, char flag)
{
	Member *lp;
	char *cp, *name;
	int  count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)		/* mode +l or +k xx */
		count = 1;
	for (lp = top; lp; lp = lp->next)
	{
		/* 
		 * Okay, since ban's are stored in their own linked
		 * list, we won't even bother to check if CHFL_BAN
		 * is set in the flags. This should work as long
		 * as only ban-lists are feed in with CHFL_BAN mask.
		 * However, we still need to typecast... -Donwulff 
		 */
		if ((mask == CHFL_BAN) || (mask == CHFL_EXCEPT) || (mask == CHFL_INVEX))
		{
/*			if (!(((Ban *)lp)->flags & mask)) continue; */
			name = ((Ban *) lp)->banstr;
		}
		else
		{
			if (!(lp->flags & mask))
				continue;
			name = lp->cptr->name;
		}
		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strlcat(parabuf, " ", sizeof parabuf);
			(void)strlcat(parabuf, name, sizeof parabuf);
			count++;
			*cp++ = flag;
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == RESYNCMODES)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendmodeto_one(cptr, me.name, chname, modebuf,
			    parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != RESYNCMODES)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = flag;
			}
			count = 0;
			*cp = '\0';
		}
	}
	return sent;
}

/* A little kludge to prevent sending double spaces -- codemastr */
static inline void send_channel_mode(aClient *cptr, char *from, aChannel *chptr)
{
	if (*parabuf)
		sendto_one(cptr, ":%s %s %s %s %s %lu", from,
			(IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
			modebuf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s %s %s %s %lu", from,
			(IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
			modebuf, chptr->creationtime);
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void send_channel_modes(aClient *cptr, aChannel *chptr)
{
	int  sent;
/* fixed a bit .. to fit halfops --sts */
	if (*chptr->chname != '#')
		return;

	*parabuf = '\0';
	*modebuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);
	sent = send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOP, 'o');
	if (!sent && chptr->creationtime)
		send_channel_mode(cptr, me.name, chptr);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';

	sent = send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_HALFOP, 'h');
	if (!sent && chptr->creationtime)
		send_channel_mode(cptr, me.name, chptr);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->exlist, CHFL_EXCEPT, 'e');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    (Member *)chptr->invexlist, CHFL_INVEX, 'I');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_VOICE, 'v');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANOWNER, 'q');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_CHANPROT, 'a');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);




}

int  DoesOp(char *modebuf)
{
	modebuf--;		/* Is it possible that a mode starts with o and not +o ? */
	while (*++modebuf)
		if (*modebuf == 'h' || *modebuf == 'o'
		    || *modebuf == 'v' || *modebuf == 'q')
			return (1);
	return 0;
}

int  sendmodeto_one(aClient *cptr, char *from, char *name, char *mode, char *param, TS creationtime)
{
	if ((IsServer(cptr) && DoesOp(mode) && creationtime) ||
	    IsULine(cptr))
		sendto_one(cptr, ":%s %s %s %s %s %lu", from,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), name, mode,
		    param, creationtime);
	else
		sendto_one(cptr, ":%s %s %s %s %s", from,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), name, mode, param);
	return 0;
}

char *pretty_mask(char *mask)
{
	char *cp;
	char *user;
	char *host;

	if ((user = index((cp = mask), '!')))
		*user++ = '\0';
	if ((host = rindex(user ? user : cp, '@')))
	{
		*host++ = '\0';
		if (!user)
			return make_nick_user_host(NULL, cp, host);
	}
	else if (!user && index(cp, '.'))
		return make_nick_user_host(NULL, NULL, cp);
	return make_nick_user_host(cp, user, host);
}

char *trim_str(char *str, int len)
{
	int l;
	if (!str)
		return NULL;
	if ((l = strlen(str)) > len)
	{
		str += l - len;
		*str = '*';
	}
	return str;
}

/* clean_ban_mask:	makes a proper banmask
 * RETURNS: pointer to correct banmask or NULL in case of error
 * NOTES:
 * - A pointer is returned to a static buffer, which is overwritten
 *   on next clean_ban_mask or make_nick_user_host call.
 * - mask is fragged in some cases, this could be bad.
 */
char *clean_ban_mask(char *mask, int what, aClient *cptr)
{
	char *cp;
	char *user;
	char *host;
	Extban *p;

	cp = index(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Strip any ':' at beginning coz that desynchs clients/banlists */
	for (; (*mask && (*mask == ':')); mask++);
	if (!*mask)
		return NULL;

	/* Extended ban? */
	if ((*mask == '~') && mask[1] && (mask[2] == ':'))
	{
		if (RESTRICT_EXTENDEDBANS && MyClient(cptr) && !IsAnOper(cptr))
		{
			if (!strcmp(RESTRICT_EXTENDEDBANS, "*"))
			{
				sendnotice(cptr, "Setting/removing of extended bans has been disabled");
				return NULL;
			}
			if (strchr(RESTRICT_EXTENDEDBANS, mask[1]))
			{
				sendnotice(cptr, "Setting/removing of extended bantypes '%s' has been disabled",
					RESTRICT_EXTENDEDBANS);
				return NULL;
			}
		}
		p = findmod_by_bantype(mask[1]);
		if (!p)
		{
			/* extended bantype not supported, what to do?
			 * Here are the rules:
			 * - if from a remote client/server: allow it (easy upgrading,
			 *   no desynch)
			 * - if from a local client trying to REMOVE the extban,
			 *   allow it too (so you don't get "unremovable" extbans).
			 */
			if (!MyClient(cptr) || (what == MODE_DEL))
				return mask; /* allow it */
			return NULL; /* reject */
		}
		if (p->conv_param)
			return p->conv_param(mask);
		/* else, do some basic sanity checks and cut it off at 80 bytes */
		if ((cp[1] != ':') || (cp[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not and extended ban and not a ~user@host ban either. */

	if ((user = index((cp = mask), '!')))
		*user++ = '\0';
	if ((host = rindex(user ? user : cp, '@')))
	{
		*host++ = '\0';

		if (!user)
			return make_nick_user_host(NULL, trim_str(cp,USERLEN), 
				trim_str(host,HOSTLEN));
	}
	else if (!user && index(cp, '.'))
		return make_nick_user_host(NULL, NULL, trim_str(cp,HOSTLEN));
	return make_nick_user_host(trim_str(cp,NICKLEN), trim_str(user,USERLEN), 
		trim_str(host,HOSTLEN));
}

int find_invex(aChannel *chptr, aClient *sptr)
{
	Ban *inv;
	char *s;
	static char realhost[NICKLEN + USERLEN + HOSTLEN + 6];
	static char virthost[NICKLEN + USERLEN + HOSTLEN + 6];
	static char     nuip[NICKLEN + USERLEN + HOSTLEN + 6];
	int dovirt = 0, mine = 0;

	if (MyConnect(sptr)) 
	{
		mine = 1;
		s = make_nick_user_host(sptr->name, sptr->user->username, GetIP(sptr));
		strlcpy(nuip, s, sizeof nuip);
	}

	if (sptr->user->virthost)
		if (strcmp(sptr->user->realhost, sptr->user->virthost))
			dovirt = 1;

	s = make_nick_user_host(sptr->name, sptr->user->username,
	    sptr->user->realhost);
	strlcpy(realhost, s, sizeof realhost);

	if (dovirt)
	{
		s = make_nick_user_host(sptr->name, sptr->user->username,
		    sptr->user->virthost);
		strlcpy(virthost, s, sizeof virthost);
	}
	for (inv = chptr->invexlist; inv; inv = inv->next)
	{
		if ((match(inv->banstr, realhost) == 0) ||
			(dovirt && (match(inv->banstr, virthost) == 0)) ||
			(mine && (match(inv->banstr, nuip) == 0)))
			return 1;
	}
	return 0;
}

/*
** Remove bells and commas from channel name
*/

void clean_channelname(char *cn)
{
	u_char *ch = (u_char *)cn;


	for (; *ch; ch++)
		/* Don't allow any control chars, the space, the comma,
		 * or the "non-breaking space" in channel names.
		 * Might later be changed to a system where the list of
		 * allowed/non-allowed chars for channels was a define
		 * or some such.
		 *   --Wizzu
		 */
		if (*ch < 33 || *ch == ',' || *ch == 160)
		{
			*ch = '\0';
			return;
		}
}

/*
** Return -1 if mask is present and doesnt match our server name.
*/
int check_channelmask(aClient *sptr, aClient *cptr, char *chname)
{
	char *s;

	s = rindex(chname, ':');
	if (!s)
		return 0;

	s++;
	if (match(s, me.name) || (IsServer(cptr) && match(s, cptr->name)))
	{
		if (MyClient(sptr))
			sendto_one(sptr, err_str(ERR_BADCHANMASK),
			    me.name, sptr->name, chname);
		return -1;
	}
	return 0;
}

/*
**  Get Channel block for i (and allocate a new channel
**  block, if it didn't exists before).
*/
aChannel *get_channel(aClient *cptr, char *chname, int flag)
{
	aChannel *chptr;
	int  len;

	if (BadPtr(chname))
		return NULL;

	len = strlen(chname);
	if (MyClient(cptr) && len > CHANNELLEN)
	{
		len = CHANNELLEN;
		*(chname + CHANNELLEN) = '\0';
	}
	if ((chptr = find_channel(chname, (aChannel *)NULL)))
		return (chptr);
	if (flag == CREATE)
	{
		chptr = (aChannel *)MyMalloc(sizeof(aChannel) + len);
		bzero((char *)chptr, sizeof(aChannel));
		strncpyzt(chptr->chname, chname, len + 1);
		if (channel)
			channel->prevch = chptr;
		chptr->topic = NULL;
		chptr->topic_nick = NULL;
		chptr->prevch = NULL;
		chptr->nextch = channel;
		chptr->creationtime = MyClient(cptr) ? TStime() : (TS)0;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
		IRCstats.channels++;
		RunHook2(HOOKTYPE_CHANNEL_CREATE, cptr, chptr);
	}
	return chptr;
}

/*
 * Slight changes in routine, now working somewhat symmetrical:
 *   First try to remove the client & channel pair to avoid duplicates
 *   Second check client & channel invite-list lengths and remove tail
 *   Finally add new invite-links to both client and channel
 * Should U-lined clients have higher limits?   -Donwulff
 */

void add_invite(aClient *cptr, aChannel *chptr)
{
	Link *inv, *tmp;

	del_invite(cptr, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(cptr->user->invited) >= MAXCHANNELSPERUSER)
	{
/*		This forgets the channel side of invitation     -Vesa
		inv = cptr->user->invited;
		cptr->user->invited = inv->next;
		free_link(inv);
*/
		for (tmp = cptr->user->invited; tmp->next; tmp = tmp->next)
			;
		del_invite(cptr, tmp->value.chptr);

	}
	/* We get pissy over too many invites per channel as well now,
	 * since otherwise mass-inviters could take up some major
	 * resources -Donwulff
	 */
	if (list_length(chptr->invites) >= MAXCHANNELSPERUSER)
	{
		for (tmp = chptr->invites; tmp->next; tmp = tmp->next)
			;
		del_invite(tmp->value.cptr, chptr);
	}
	/*
	 * add client to the beginning of the channel invite list
	 */
	inv = make_link();
	inv->value.cptr = cptr;
	inv->next = chptr->invites;
	chptr->invites = inv;
	/*
	 * add channel to the beginning of the client invite list
	 */
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = cptr->user->invited;
	cptr->user->invited = inv;
}

/*
 * Delete Invite block from channel invite list and client invite list
 */
void del_invite(aClient *cptr, aChannel *chptr)
{
	Link **inv, *tmp;

	for (inv = &(chptr->invites); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.cptr == cptr)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}

	for (inv = &(cptr->user->invited); (tmp = *inv); inv = &tmp->next)
		if (tmp->value.chptr == chptr)
		{
			*inv = tmp->next;
			free_link(tmp);
			break;
		}
}

/*
**  Subtract one user from channel i (and free channel
**  block, if channel became empty).
*/
void sub1_from_channel(aChannel *chptr)
{
	Ban *ban;
	Link *lp;

        /* if (--chptr->users <= 0) */
	if (chptr->users == 0 || --chptr->users == 0)
	{
		/*
		 * Now, find all invite links from channel structure
		 */
		RunHook(HOOKTYPE_CHANNEL_DESTROY, chptr);
		while ((lp = chptr->invites))
			del_invite(lp->value.cptr, chptr);

		while (chptr->banlist)
		{
			ban = chptr->banlist;
			chptr->banlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while (chptr->exlist)
		{
			ban = chptr->exlist;
			chptr->exlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
		while (chptr->invexlist)
		{
			ban = chptr->invexlist;
			chptr->invexlist = ban->next;
			MyFree(ban->banstr);
			MyFree(ban->who);
			free_ban(ban);
		}
#ifdef EXTCMODE
		/* free extcmode params */
		extcmode_free_paramlist(chptr->mode.extmodeparam);
		chptr->mode.extmodeparam = NULL;
#endif
#ifdef NEWCHFLOODPROT
		chanfloodtimer_stopchantimers(chptr);
		if (chptr->mode.floodprot)
			MyFree(chptr->mode.floodprot);
#endif
#ifdef JOINTHROTTLE
		cmodej_delchannelentries(chptr);
#endif
		if (chptr->topic)
			MyFree(chptr->topic);
		if (chptr->topic_nick)
			MyFree(chptr->topic_nick);
		if (chptr->prevch)
			chptr->prevch->nextch = chptr->nextch;
		else
			channel = chptr->nextch;
		if (chptr->nextch)
			chptr->nextch->prevch = chptr->prevch;
		(void)del_from_channel_hash_table(chptr->chname, chptr);
		IRCstats.channels--;
		MyFree((char *)chptr);
	}
}

/*
 * The function which sends the actual channel list back to the user.
 * Operates by stepping through the hashtable, sending the entries back if
 * they match the criteria.
 * cptr = Local client to send the output back to.
 * numsend = Number (roughly) of lines to send back. Once this number has
 * been exceeded, send_list will finish with the current hash bucket,
 * and record that number as the number to start next time send_list
 * is called for this user. So, this function will almost always send
 * back more lines than specified by numsend (though not by much,
 * assuming CH_MAX is was well picked). So be conservative in your choice
 * of numsend. -Rak
 */

/* Taken from bahamut, modified for Unreal by codemastr */

void send_list(aClient *cptr, int numsend)
{
	aChannel *chptr;
	LOpts *lopt = cptr->user->lopt;
	unsigned int  hashnum;

	/* Begin of /list? then send official channels. */
	if ((lopt->starthash == 0) && conf_offchans)
	{
		ConfigItem_offchans *x;
		for (x = conf_offchans; x; x = (ConfigItem_offchans *)x->next)
		{
			if (find_channel(x->chname, (aChannel *)NULL))
				continue; /* exists, >0 users.. will be sent later */
			sendto_one(cptr,
			    rpl_str(RPL_LIST), me.name,
			    cptr->name, x->chname,
			    0,
#ifdef LIST_SHOW_MODES
			    "",
#endif					    
			    x->topic ? x->topic : "");
		}
	}

	for (hashnum = lopt->starthash; hashnum < CH_MAX; hashnum++)
	{
		if (numsend > 0)
			for (chptr =
			    (aChannel *)hash_get_chan_bucket(hashnum);
			    chptr; chptr = chptr->hnextch)
			{
				if (SecretChannel(chptr)
				    && !IsMember(cptr, chptr)
				    && !IsAnOper(cptr))
					continue;

				/* Much more readable like this -- codemastr */
				if ((!lopt->showall))
				{
					/* User count must be in range */
					if ((chptr->users < lopt->usermin) || 
					    ((lopt->usermax >= 0) && (chptr->users > 
					    lopt->usermax)))
						continue;

					/* Creation time must be in range */
					if ((chptr->creationtime && (chptr->creationtime <
					    lopt->chantimemin)) || (chptr->creationtime >
					    lopt->chantimemax))
						continue;

					/* Topic time must be in range */
					if ((chptr->topic_time < lopt->topictimemin) ||
					    (chptr->topic_time > lopt->topictimemax))
						continue;

					/* Must not be on nolist (if it exists) */
					if (lopt->nolist && find_str_match_link(lopt->nolist,
					    chptr->chname))
						continue;

					/* Must be on yeslist (if it exists) */
					if (lopt->yeslist && !find_str_match_link(lopt->yeslist,
					    chptr->chname))
						continue;
				}
#ifdef LIST_SHOW_MODES
				modebuf[0] = '[';
				channel_modes(cptr, &modebuf[1], parabuf, chptr);
				if (modebuf[2] == '\0')
					modebuf[0] = '\0';
				else
					strlcat(modebuf, "]", sizeof modebuf);
#endif
				if (!IsAnOper(cptr))
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name,
					    ShowChannel(cptr,
					    chptr) ? chptr->chname :
					    "*", chptr->users,
#ifdef LIST_SHOW_MODES
					    ShowChannel(cptr, chptr) ?
					    modebuf : "",
#endif
					    ShowChannel(cptr,
					    chptr) ? (chptr->topic ?
					    chptr->topic : "") : "");
				else
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name, chptr->chname,
					    chptr->users,
#ifdef LIST_SHOW_MODES
					    modebuf,
#endif					    
					    (chptr->topic ? chptr->topic : ""));
				numsend--;
			}
		else
			break;
	}

	/* All done */
	if (hashnum == CH_MAX)
	{
		sendto_one(cptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
		free_str_list(cptr->user->lopt->yeslist);
		free_str_list(cptr->user->lopt->nolist);
		MyFree(cptr->user->lopt);
		cptr->user->lopt = NULL;
		return;
	}

	/* 
	 * We've exceeded the limit on the number of channels to send back
	 * at once.
	 */
	lopt->starthash = hashnum;
	return;
}


int  check_for_chan_flood(aClient *cptr, aClient *sptr, aChannel *chptr)
{
	Membership *lp;
	MembershipL *lp2;
	int c_limit, t_limit, banthem;

	if (!MyClient(sptr))
		return 0;
	if (IsOper(sptr) || IsULine(sptr))
		return 0;
	if (is_skochanop(sptr, chptr))
		return 0;

	if (!(lp = find_membership_link(sptr->user->channel, chptr)))
		return 0;

	lp2 = (MembershipL *) lp;

#ifdef NEWCHFLOODPROT
	if (!chptr->mode.floodprot || !chptr->mode.floodprot->l[FLD_TEXT])
		return 0;
	c_limit = chptr->mode.floodprot->l[FLD_TEXT];
	t_limit = chptr->mode.floodprot->per;
	banthem = (chptr->mode.floodprot->a[FLD_TEXT] == 'b') ? 1 : 0;
#else
	if ((chptr->mode.msgs < 1) || (chptr->mode.per < 1))
		return 0;
	c_limit = chptr->mode.msgs;
	t_limit = chptr->mode.per;
	banthem = chptr->mode.kmode;
#endif
	/* if current - firstmsgtime >= mode.per, then reset,
	 * if nummsg > mode.msgs then kick/ban
	 */
	Debug((DEBUG_ERROR, "Checking for flood +f: firstmsg=%d (%ds ago), new nmsgs: %d, limit is: %d:%d",
		lp2->flood.firstmsg, TStime() - lp2->flood.firstmsg, lp2->flood.nmsg + 1,
		c_limit, t_limit));
	if ((TStime() - lp2->flood.firstmsg) >= t_limit)
	{
		/* reset */
		lp2->flood.firstmsg = TStime();
		lp2->flood.nmsg = 1;
		return 0; /* forget about it.. */
	}

	/* increase msgs */
	lp2->flood.nmsg++;

	if ((lp2->flood.nmsg) > c_limit)
	{
		char comment[1024], mask[1024];
		ircsprintf(comment,
		    "Flooding (Limit is %i lines per %i seconds)",
		    c_limit, t_limit);
		if (banthem)
		{		/* ban. */
			ircsprintf(mask, "*!*@%s", GetHost(sptr));
			add_listmode(&chptr->banlist, &me, chptr, mask);
			sendto_serv_butone(&me, ":%s MODE %s +b %s 0",
			    me.name, chptr->chname, mask);
			sendto_channel_butserv(chptr, &me,
			    ":%s MODE %s +b %s", me.name, chptr->chname, mask);
		}
		sendto_channel_butserv(chptr, &me,
		    ":%s KICK %s %s :%s", me.name,
		    chptr->chname, sptr->name, comment);
		sendto_serv_butone_token(cptr, me.name,
			MSG_KICK, TOK_KICK, 
			"%s %s :%s",
		   chptr->chname, sptr->name, comment);
		remove_user_from_channel(sptr, chptr);
		return 1;
	}
	return 0;
}

/************************************************************************
 * m_names() - Added by Jto 27 Apr 1989
 * 12 Feb 2000 - geesh, time for a rewrite -lucas
 ************************************************************************/
/*
** m_names
**	parv[0] = sender prefix
**	parv[1] = channel
*/
#define TRUNCATED_NAMES 64
CMD_FUNC(m_names)
{
	int  mlen = strlen(me.name) + NICKLEN + 7;
	aChannel *chptr;
	aClient *acptr;
	int  member;
	Member *cm;
	int  idx, flag = 1, spos;
	char *s, *para = parv[1];


	if (parc < 2 || !MyConnect(sptr))
	{
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
		    parv[0], "*");
		return 0;
	}

	if (parc > 1 &&
	    hunt_server_token(cptr, sptr, MSG_NAMES, TOK_NAMES, "%s %s", 2, parc, parv))
		return 0;

	for (s = para; *s; s++)
	{
		if (*s == ',')
		{
			if (strlen(para) > TRUNCATED_NAMES)
				para[TRUNCATED_NAMES] = '\0';
			sendto_realops("names abuser %s %s",
			    get_client_name(sptr, FALSE), para);
			sendto_one(sptr, err_str(ERR_TOOMANYTARGETS),
			    me.name, sptr->name, "NAMES");
			return 0;
		}
	}

	chptr = find_channel(para, (aChannel *)NULL);

	if (!chptr || (!ShowChannel(sptr, chptr) && !IsAnOper(sptr)))
	{
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
		    parv[0], para);
		return 0;
	}

	/* cache whether this user is a member of this channel or not */
	member = IsMember(sptr, chptr);

	if (PubChannel(chptr))
		buf[0] = '=';
	else if (SecretChannel(chptr))
		buf[0] = '@';
	else
		buf[0] = '*';

	idx = 1;
	buf[idx++] = ' ';
	for (s = chptr->chname; *s; s++)
		buf[idx++] = *s;
	buf[idx++] = ' ';
	buf[idx++] = ':';

	/* If we go through the following loop and never add anything,
	   we need this to be empty, otherwise spurious things from the
	   LAST /names call get stuck in there.. - lucas */
	buf[idx] = '\0';

	spos = idx;		/* starting point in buffer for names! */

	for (cm = chptr->members; cm; cm = cm->next)
	{
		acptr = cm->cptr;
		if (IsInvisible(acptr) && !member && !IsNetAdmin(sptr))
			continue;
		if (chptr->mode.mode & MODE_AUDITORIUM)
			if (!is_chan_op(sptr, chptr)
			    && !is_chanprot(sptr, chptr)
			    && !is_chanowner(sptr, chptr))
				if (!(cm->
				    flags & (CHFL_CHANOP | CHFL_CHANPROT |
				    CHFL_CHANOWNER)) && acptr != sptr)
					continue;

#ifdef PREFIX_AQ
		if (cm->flags & CHFL_CHANOWNER)
			buf[idx++] = '~';
		else if (cm->flags & CHFL_CHANPROT)
			buf[idx++] = '&';
		else
#endif
		if (cm->flags & CHFL_CHANOP)
			buf[idx++] = '@';
		else if (cm->flags & CHFL_HALFOP)
			buf[idx++] = '%';
		else if (cm->flags & CHFL_VOICE)
			buf[idx++] = '+';
		for (s = acptr->name; *s; s++)
			buf[idx++] = *s;
		buf[idx++] = ' ';
		buf[idx] = '\0';
		flag = 1;
		if (mlen + idx + NICKLEN > BUFSIZE - 3)
		{
			sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name,
			    parv[0], buf);
			idx = spos;
			flag = 0;
		}
	}

	if (flag)
		sendto_one(sptr, rpl_str(RPL_NAMREPLY), me.name, parv[0], buf);

	sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name, parv[0], para);

	return 0;

}

void send_user_joins(aClient *cptr, aClient *user)
{
	Membership *lp;
	aChannel *chptr;
	int  cnt = 0, len = 0, clen;
	char *mask;

	snprintf(buf, sizeof buf, ":%s %s ", user->name,	
	    (IsToken(cptr) ? TOK_JOIN : MSG_JOIN));
	len = strlen(buf);

	for (lp = user->user->channel; lp; lp = lp->next)
	{
		chptr = lp->chptr;
		if ((mask = index(chptr->chname, ':')))
			if (match(++mask, cptr->name))
				continue;
		if (*chptr->chname == '&')
			continue;
		clen = strlen(chptr->chname);
		if (clen + 1 + len > BUFSIZE - 3)
		{
			if (cnt)
			{
				buf[len - 1] = '\0';
				sendto_one(cptr, "%s", buf);
			}
			snprintf(buf, sizeof buf, ":%s %s ", user->name,
			    (IsToken(cptr) ? TOK_JOIN : MSG_JOIN));
			len = strlen(buf);
			cnt = 0;
		}
		(void)strlcpy(buf + len, chptr->chname, sizeof buf-len);
		cnt++;
		len += clen;
		if (lp->next)
		{
			len++;
			(void)strlcat(buf, ",", sizeof buf);
		}
	}
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}

static int send_ban_list(aClient *cptr, char *chname, TS creationtime, aChannel *channel)
{
	Ban *top;

	Ban *lp;
	char *cp, *name;
	int  count = 0, send = 0, sent = 0;

	cp = modebuf + strlen(modebuf);
	if (*parabuf)		/* mode +l or +k xx */
		count = 1;
	top = channel->banlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'b';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'b';
			}
			count = 0;
			*cp = '\0';
		}
	}
	top = channel->exlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'e';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'e';
			}
			count = 0;
			*cp = '\0';
		}
	}
	top = channel->invexlist;
	for (lp = top; lp; lp = lp->next)
	{
		name = ((Ban *) lp)->banstr;

		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
			count++;
			*cp++ = 'I';
			*cp = '\0';
		}
		else if (*parabuf)
			send = 1;
		if (count == MODEPARAMS)
			send = 1;
		if (send)
		{
			/* cptr is always a server! So we send creationtimes */
			sendto_one(cptr, "%s %s %s %s %lu",
			    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
			    chname, modebuf, parabuf, creationtime);
			sent = 1;
			send = 0;
			*parabuf = '\0';
			cp = modebuf;
			*cp++ = '+';
			if (count != MODEPARAMS)
			{
				(void)strlcpy(parabuf, name, sizeof parabuf);
				*cp++ = 'I';
			}
			count = 0;
			*cp = '\0';
		}
	}
	return sent;
}


/* 
 * This will send "cptr" a full list of the modes for channel chptr,
 */

void send_channel_modes_sjoin(aClient *cptr, aChannel *chptr)
{

	Member *members;
	Member *lp;
	char *name;
	char *bufptr;

	int  n = 0;

	if (*chptr->chname != '#')
		return;

	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (*parabuf)
	{
	}
	else
	{
		if (!SupportSJOIN2(cptr))
			strlcpy(parabuf, "<none>", sizeof parabuf);
		else
			strlcpy(parabuf, "<->", sizeof parabuf);
	}
	ircsprintf(buf, "%s %ld %s %s %s :",
	    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
	    chptr->creationtime, chptr->chname, modebuf, parabuf);

	bufptr = buf + strlen(buf);

	for (lp = members; lp; lp = lp->next)
	{

		if (lp->flags & MODE_CHANOP)
			*bufptr++ = '@';

		if (lp->flags & MODE_VOICE)
			*bufptr++ = '+';

		if (lp->flags & MODE_HALFOP)
			*bufptr++ = '%';
		if (lp->flags & MODE_CHANOWNER)
			*bufptr++ = '*';
		if (lp->flags & MODE_CHANPROT)
			*bufptr++ = '~';



		name = lp->cptr->name;

		strcpy(bufptr, name);
		bufptr += strlen(bufptr);
		*bufptr++ = ' ';
		n++;

		if (bufptr - buf > BUFSIZE - 80)
		{
			*bufptr++ = '\0';
			if (bufptr[-1] == ' ')
				bufptr[-1] = '\0';
			sendto_one(cptr, "%s", buf);

			ircsprintf(buf, "%s %ld %s %s %s :",
			    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
			    chptr->creationtime, chptr->chname, modebuf,
			    parabuf);
			n = 0;

			bufptr = buf + strlen(buf);
		}
	}
	if (n)
	{
		*bufptr++ = '\0';
		if (bufptr[-1] == ' ')
			bufptr[-1] = '\0';
		sendto_one(cptr, "%s", buf);
	}
	/* Then we'll send the ban-list */

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	send_ban_list(cptr, chptr->chname, chptr->creationtime, chptr);

	if (modebuf[1] || *parabuf)
		sendto_one(cptr, "%s %s %s %s %lu",
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE),
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	return;
}

char *mystpcpy(char *dst, const char *src)
{
	for (; *src; src++)
		*dst++ = *src;
	*dst = '\0';
	return dst;
}



/** This will send "cptr" a full list of the modes for channel chptr,
 *
 * Half of it recoded by Syzop: the whole buffering and size checking stuff
 * looked weird and just plain inefficient. We now fill up our send-buffer
 * really as much as we can, without causing any overflows of course.
 */
void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr)
{
	Member *members;
	Member *lp;
	Ban *ban;
	char *name;
	short nomode, nopara;
	char tbuf[512]; /* work buffer, for temporary data */
	char buf[1024]; /* send buffer */
	char *bufptr; /* points somewhere in 'buf' */
	char *p; /* points to somewhere in 'tbuf' */
	int prebuflen = 0; /* points to after the <sjointoken> <TS> <chan> <fixmodes> <fixparas <..>> : part */

	if (*chptr->chname != '#')
		return;

	nomode = 0;
	nopara = 0;
	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);

	if (!modebuf[1])
		nomode = 1;
	if (!(*parabuf))
		nopara = 1;


	if (nomode && nopara)
	{
		ircsprintf(buf,
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s :" : "%s %ld %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname);
	}
	if (nopara && !nomode)
	{
		ircsprintf(buf, 
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s :" : "%s %ld %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname, modebuf);
	}
	if (!nopara && !nomode)
	{
		ircsprintf(buf,
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s %s :" : "%s %ld %s %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    (long)chptr->creationtime, chptr->chname, modebuf, parabuf);
	}

	prebuflen = strlen(buf);
	bufptr = buf + prebuflen;

	/* RULES:
	 * - Use 'tbuf' as a working buffer, use 'p' to advance in 'tbuf'.
	 *   Thus, be sure to do a 'p = tbuf' at the top of the loop.
	 * - When one entry has been build, check if strlen(buf) + strlen(tbuf) > BUFSIZE - 8,
	 *   if so, do not concat but send the current result (buf) first to the server
	 *   and reset 'buf' to only the prebuf part (all until the ':').
	 *   Then, in both cases, concat 'tbuf' to 'buf' and continue
	 * - Be sure to ALWAYS zero terminate (*p = '\0') when the entry has been build.
	 * - Be sure to add a space after each entry ;)
	 *
	 * For a more illustrated view, take a look at the first for loop, the others
	 * are pretty much the same.
	 *
	 * Follow these rules, and things would be smooth and efficient (network-wise),
	 * if you ignore them, expect crashes and/or heap corruption, aka: HELL.
	 * You have been warned.
	 *
	 * Side note: of course things would be more efficient if the prebuf thing would
	 * not be sent every time, but that's another story
	 *      -- Syzop
	 */

	for (lp = members; lp; lp = lp->next)
	{
		p = tbuf;
		if (lp->flags & MODE_CHANOP)
			*p++ = '@';
		if (lp->flags & MODE_VOICE)
			*p++ = '+';
		if (lp->flags & MODE_HALFOP)
			*p++ = '%';
		if (lp->flags & MODE_CHANOWNER)
			*p++ = '*';
		if (lp->flags & MODE_CHANPROT)
			*p++ = '~';

		p = mystpcpy(p, lp->cptr->name);
		*p++ = ' ';
		*p = '\0';

		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->banlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '&';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->exlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '"';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	for (ban = chptr->invexlist; ban; ban = ban->next)
	{
		p = tbuf;
		*p++ = '\'';
		p = mystpcpy(p, ban->banstr);
		*p++ = ' ';
		*p = '\0';
		
		/* this is: if (strlen(tbuf) + strlen(buf) > BUFSIZE - 8) */
		if ((p - tbuf) + (bufptr - buf) > BUFSIZE - 8)
		{
			/* Would overflow, so send our current stuff right now (except new stuff) */
			sendto_one(cptr, "%s", buf);
			bufptr = buf + prebuflen;
			*bufptr = '\0';
		}
		/* concat our stuff.. */
		bufptr = mystpcpy(bufptr, tbuf);
	}

	if (buf[prebuflen])
		sendto_one(cptr, "%s", buf);
}

void add_send_mode_param(aChannel *chptr, aClient *from, char what, char mode, char *param) {
	static char *modes = modebuf, lastwhat;
	static short count = 0;
	short send = 0;
	if (!modebuf[0]) {
		modes = modebuf;
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
		*parabuf = 0;
		count = 0;
	}
	if (lastwhat != what) {
		*modes++ = what;
		*modes = 0;
		lastwhat = what;
	}
	if (strlen(parabuf) + strlen(param) + 11 < MODEBUFLEN) {
		if (*parabuf) 
			strcat(parabuf, " ");
		strcat(parabuf, param);
		*modes++ = mode;
		*modes = 0;
		count++;
	}
	else if (*parabuf) 
		send = 1;

	if (count == MAXMODEPARAMS)
		send = 1;

	if (send) {
		sendto_channel_butserv(chptr, from, ":%s MODE %s %s %s",
			from->name, chptr->chname, modebuf, parabuf);
		sendto_serv_butone(NULL, ":%s MODE %s %s %s", from->name, chptr->chname, modebuf, parabuf);
		send = 0;
		*parabuf = 0;
		modes = modebuf;
		*modes++ = what;
		lastwhat = what;
		if (count != MAXMODEPARAMS) {
			strcpy(parabuf, param);
			*modes++ = mode;
			count = 1;
		}
		else 
			count = 0;
		*modes = 0;
	}
}

/*
 * rejoin_doparts:
 * sends a PART to all channels (to local users only)
 */
void rejoin_doparts(aClient *sptr, char did_parts[])
{
	Membership *tmp;
	aChannel *chptr;
	char *comment = "Rejoining because of user@host change";
	int i = 0;

	for (tmp = sptr->user->channel; tmp; tmp = tmp->next)
	{
		chptr = tmp->chptr;
		if (!chptr)
			continue; /* Possible? */

		/* If the user is banned, don't do it */
		if (is_banned(sptr, chptr, BANCHK_JOIN))
		{
			did_parts[i++] = 0;
			continue;
		}
		did_parts[i++] = 1;

		if ((chptr->mode.mode & MODE_AUDITORIUM) &&
		    !(tmp->flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP)))
		{
			sendto_chanops_butone(sptr, chptr, ":%s!%s@%s PART %s :%s", sptr->name, sptr->user->username, GetHost(sptr), chptr->chname, comment);
		} else
			sendto_channel_butserv_butone(chptr, sptr, sptr, ":%s PART %s :%s", sptr->name, chptr->chname, comment);
	}
}

/*
 * rejoin_dojoinandmode:
 * sends a JOIN and a MODE (if needed) to restore qaohv modes (to local users only)
 */
void rejoin_dojoinandmode(aClient *sptr, char did_parts[])
{
	Membership *tmp;
	aChannel *chptr;
	int i, j = 0, n, flags;
	char flagbuf[8]; /* For holding "qohva" and "*~@%+" */

	for (tmp = sptr->user->channel; tmp; tmp = tmp->next)
	{
		flags = tmp->flags;
		chptr = tmp->chptr;
		if (!chptr)
			continue; /* Is it possible? */

		/* If the user is banned, don't do it */
		if (!did_parts[j++])
			continue;

		if ((chptr->mode.mode & MODE_AUDITORIUM) && 
		    !(flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP)))
		{
			sendto_chanops_butone(sptr, chptr, ":%s!%s@%s JOIN :%s", sptr->name, sptr->user->username, GetHost(sptr), chptr->chname);
		} else
			sendto_channel_butserv_butone(chptr, sptr, sptr, ":%s JOIN :%s", sptr->name, chptr->chname);

		/* Set the modes (if any) */
		if (flags)
		{
			char *p = flagbuf;
			if (flags & MODE_CHANOP)
				*p++ = 'o';
			if (flags & MODE_VOICE)
				*p++ = 'v';
			if (flags & MODE_HALFOP)
				*p++ = 'h';
			if (flags & MODE_CHANOWNER)
				*p++ = 'q';
			if (flags & MODE_CHANPROT)
				*p++ = 'a';
			*p = '\0';
			parabuf[0] = '\0';
			n = strlen(flagbuf);
			if (n)
			{
				for (i=0; i < n; i++)
				{
					strcat(parabuf, sptr->name);
					if (i < n - 1)
						strcat(parabuf, " ");
				}
				sendto_channel_butserv_butone(chptr, &me, sptr, ":%s MODE %s +%s %s",
					me.name, chptr->chname, flagbuf, parabuf);
			}
		}
	}
}

#ifdef NEWCHFLOODPROT
MODVAR RemoveFld *removefld_list = NULL;

RemoveFld *chanfloodtimer_find(aChannel *chptr, char mflag)
{
RemoveFld *e;

	for (e=removefld_list; e; e=e->next)
	{
		if ((e->chptr == chptr) && (e->m == mflag))
			return e;
	}
	return NULL;
}

/*
 * Adds a "remove channelmode set by +f" timer.
 * chptr	Channel
 * mflag	Mode flag, eg 'C'
 * mbit		Mode bitflag, eg MODE_NOCTCP
 * when		when it should be removed
 * NOTES:
 * - This function takes care of overwriting of any previous timer
 *   for the same modechar.
 * - The function takes care of chptr->mode.floodprot->timer_flags,
 *   do not modify it yourself.
 * - chptr->mode.floodprot is asumed to be non-NULL.
 */
void chanfloodtimer_add(aChannel *chptr, char mflag, long mbit, time_t when)
{
RemoveFld *e = NULL;
unsigned char add=1;

	if (chptr->mode.floodprot->timer_flags & mbit)
	{
		/* Already exists... */
		e = chanfloodtimer_find(chptr, mflag);
		if (e)
			add = 0;
	}

	if (add)
		e = MyMallocEx(sizeof(RemoveFld));

	e->chptr = chptr;
	e->m = mflag;
	e->when = when;

	if (add)
		AddListItem(e, removefld_list);

	chptr->mode.floodprot->timer_flags |= mbit;
}

void chanfloodtimer_del(aChannel *chptr, char mflag, long mbit)
{
RemoveFld *e;

	if (chptr->mode.floodprot && !(chptr->mode.floodprot->timer_flags & mbit))
		return; /* nothing to remove.. */
	e = chanfloodtimer_find(chptr, mflag);
	if (!e)
		return;

	DelListItem(e, removefld_list);

	if (chptr->mode.floodprot)
		chptr->mode.floodprot->timer_flags &= ~mbit;
}

long get_chanbitbychar(char m)
{
aCtab *tab = &cFlagTab[0];
	while(tab->mode != 0x0)
	{
		if (tab->flag == m)
			return tab->mode;
		tab++;;
	}
	return 0;
}

EVENT(modef_event)
{
RemoveFld *e = removefld_list;
time_t now;
long mode;

	now = TStime();
	
	while(e)
	{
		if (e->when <= now)
		{
			/* Remove chanmode... */
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c EXPIRED", e->chptr->chname, e->m);
#endif
			mode = get_chanbitbychar(e->m);
			if (e->chptr->mode.mode & mode)
			{
				sendto_serv_butone(&me, ":%s MODE %s -%c 0", me.name, e->chptr->chname, e->m);
				sendto_channel_butserv(e->chptr, &me, ":%s MODE %s -%c", me.name, e->chptr->chname, e->m);
				e->chptr->mode.mode &= ~mode;
			}
			
			/* And delete... */
			e = (RemoveFld *)DelListItem(e, removefld_list);
		} else {
#ifdef NEWFLDDBG
			sendto_realops("modef_event: chan %s mode -%c about %d seconds",
				e->chptr->chname, e->m, e->when - now);
#endif
			e = e->next;
		}
	}
}

void init_modef()
{
	EventAddEx(NULL, "modef_event", 10, 0, modef_event, NULL);
}

void chanfloodtimer_stopchantimers(aChannel *chptr)
{
RemoveFld *e = removefld_list;
	while(e)
	{
		if (e->chptr == chptr)
			e = (RemoveFld *)DelListItem(e, removefld_list);
		else
			e = e->next;
	}
}



int do_chanflood(ChanFloodProt *chp, int what)
{

	if (!chp || !chp->l[what]) /* no +f or not restricted */
		return 0;
	if (TStime() - chp->t[what] >= chp->per)
	{
		chp->t[what] = TStime();
		chp->c[what] = 1;
	} else
	{
		chp->c[what]++;
		if ((chp->c[what] > chp->l[what]) &&
		    (TStime() - chp->t[what] < chp->per))
		{
			/* reset it too (makes it easier for chanops to handle the situation) */
			/*
			 *XXchp->t[what] = TStime();
			 *XXchp->c[what] = 1;
			 * 
			 * BAD.. there are some situations where we might 'miss' a flood
			 * because of this. The reset has been moved to -i,-m,-N,-C,etc.
			*/
			return 1; /* flood detected! */
		}
	}
	return 0;
}

void do_chanflood_action(aChannel *chptr, int what, char *text)
{
long modeflag = 0;
aCtab *tab = &cFlagTab[0];
char m;

	m = chptr->mode.floodprot->a[what];
	if (!m)
		return;

	/* [TODO: add extended channel mode support] */
	
	while(tab->mode != 0x0)
	{
		if (tab->flag == m)
		{
			modeflag = tab->mode;
			break;
		}
		tab++;
	}

	if (!modeflag)
		return;
		
	if (!(chptr->mode.mode & modeflag))
	{
		char comment[1024], target[CHANNELLEN + 8];
		ircsprintf(comment, "*** Channel %sflood detected (limit is %d per %d seconds), setting mode +%c",
			text, chptr->mode.floodprot->l[what], chptr->mode.floodprot->per, m);
		ircsprintf(target, "%%%s", chptr->chname);
		sendto_channelprefix_butone_tok(NULL, &me, chptr,
			PREFIX_HALFOP|PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
			MSG_NOTICE, TOK_NOTICE, target, comment, 0);
		sendto_serv_butone(&me, ":%s MODE %s +%c 0", me.name, chptr->chname, m);
		sendto_channel_butserv(chptr, &me, ":%s MODE %s +%c", me.name, chptr->chname, m);
		chptr->mode.mode |= modeflag;
		if (chptr->mode.floodprot->r[what]) /* Add remove-chanmode timer... */
		{
			chanfloodtimer_add(chptr, m, modeflag, TStime() + ((long)chptr->mode.floodprot->r[what] * 60) - 5);
			/* (since the chanflood timer event is called every 10s, we do -5 here so the accurancy will
			 *  be -5..+5, without it it would be 0..+10.)
			 */
		}
	}
}
#endif
