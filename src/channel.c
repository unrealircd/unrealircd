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

#define MAXBOUNCE	5 /** Most sensible */

typedef struct ChLink CHLINK;
struct CHLink {
	aChannel *chan;
};

static int bouncedtimes = 0;

struct MODVAR CHLink chlbounce[MAXBOUNCE];
int  MODVAR chbounce = 0;
long opermode = 0;
aChannel *channel = NullChn;
extern char backupbuf[];
extern ircstats IRCstats;
extern int spamf_ugly_vchanoverride;

#ifndef NO_FDLIST
extern int lifesux;
#endif

/* Some forward declarations */
CMD_FUNC(do_join);
void add_invite(aClient *, aChannel *);
char *clean_ban_mask(char *, int, aClient *);
int add_banid(aClient *, aChannel *, char *);
int can_join(aClient *, aClient *, aChannel *, char *, char *,
    char **);
void channel_modes(aClient *, char *, char *, aChannel *);
int check_channelmask(aClient *, aClient *, char *);
int del_banid(aChannel *, char *);
void set_mode(aChannel *, aClient *, int, char **, u_int *,
    char[MAXMODEPARAMS][MODEBUFLEN + 3], int);

#ifdef EXTCMODE
void make_mode_str(aChannel *, long, Cmode_t, long, int,
    char[MAXMODEPARAMS][MODEBUFLEN + 3], char *, char *, char);
#else
void make_mode_str(aChannel *, long, long, int,
    char[MAXMODEPARAMS][MODEBUFLEN + 3], char *, char *, char);
#endif

int do_mode_char(aChannel *, long, char, char *,
	u_int, aClient *,
    u_int *, char[MAXMODEPARAMS][MODEBUFLEN + 3], char);
void do_mode(aChannel *, aClient *, aClient *, int, char **, time_t,
    int);
void bounce_mode(aChannel *, aClient *, int, char **);

void sub1_from_channel(aChannel *);

void clean_channelname(char *);
void del_invite(aClient *, aChannel *);

#ifdef NEWCHFLOODPROT
void chanfloodtimer_del(aChannel *chptr, char mflag, long mbit);
void chanfloodtimer_stopchantimers(aChannel *chptr);
#endif

static char *PartFmt = ":%s PART %s";
static char *PartFmt2 = ":%s PART %s :%s";
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
	{MODE_NOKNOCK, 'K', 0, 0},	/* knock knock (no way!) */
	{MODE_NOINVITE, 'V', 0, 0},	/* no invites */
	{MODE_FLOODLIMIT, 'f', 0, 1},	/* flood limiter */
	{MODE_MODREG, 'M', 0, 0},	/* Need umode +r to talk */
#ifdef STRIPBADWORDS
	{MODE_STRIPBADWORDS, 'G', 0, 0},	/* no badwords */
#endif
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

/* is some kind of admin */
#define IsSkoAdmin(sptr) (IsAdmin(sptr) || IsNetAdmin(sptr) || IsSAdmin(sptr))

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
  Exception functions to work with mode +e
   -sts
*/

/* add_exbanid - add an id to be excepted to the channel bans  (belongs to cptr) */

int add_exbanid(aClient *cptr, aChannel *chptr, char *banid)
{
	Ban *ban;
	int  cnt = 0, len = 0;

	if (MyClient(cptr))
		(void)collapse(banid);
	for (ban = chptr->exlist; ban; ban = ban->next)
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
	/*   ban->flags = CHFL_BAN;                  They're all bans!! */
	ban->next = chptr->exlist;
	ban->banstr = (char *)MyMalloc(strlen(banid) + 1);
	(void)strcpy(ban->banstr, banid);
	ban->who = (char *)MyMalloc(strlen(cptr->name) + 1);
	(void)strcpy(ban->who, cptr->name);
	ban->when = TStime();
	chptr->exlist = ban;
	return 0;
}
/*
 * del_exbanid - delete an id belonging to cptr
 */
int del_exbanid(aChannel *chptr, char *banid)
{
	Ban **ban;
	Ban *tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->exlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->banstr) == 0)
		{
			tmp = *ban;
			*ban = tmp->next;
			MyFree(tmp->banstr);
			MyFree(tmp->who);
			free_ban(tmp);
			return 0;
		}
	return -1;
}



/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

int add_banid(aClient *cptr, aChannel *chptr, char *banid)
{
	Ban *ban;
	int  cnt = 0, len = 0;

	if (MyClient(cptr))
		(void)collapse(banid);
	for (ban = chptr->banlist; ban; ban = ban->next)
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
#ifdef NAZIISH_CHBAN_HANDLING /* why does it do this?? */
				if (!match(ban->banstr, banid) ||
				    !match(banid, ban->banstr))
#else
				if (!match(ban->banstr, banid))
#endif
					return -1;
			}
		else if (!mycmp(ban->banstr, banid))
			return -1;

	}
	ban = make_ban();
	bzero((char *)ban, sizeof(Ban));
	/*   ban->flags = CHFL_BAN;                  They're all bans!! */
	ban->next = chptr->banlist;
	ban->banstr = (char *)MyMalloc(strlen(banid) + 1);
	(void)strcpy(ban->banstr, banid);
	ban->who = (char *)MyMalloc(strlen(cptr->name) + 1);
	(void)strcpy(ban->who, cptr->name);
	ban->when = TStime();
	chptr->banlist = ban;
	return 0;
}
/*
 * del_banid - delete an id belonging to cptr
 */
int del_banid(aChannel *chptr, char *banid)
{
	Ban **ban;
	Ban *tmp;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (mycmp(banid, (*ban)->banstr) == 0)
		{
			tmp = *ban;
			*ban = tmp->next;
			MyFree(tmp->banstr);
			MyFree(tmp->who);
			free_ban(tmp);
			return 0;
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

/** is_banned - checks for bans.
 * PARAMETERS:
 * sptr:	the client to check (can be remote client)
 * chptr:	the channel to check
 * type:	one of BANCHK_*
 * RETURNS:
 * a pointer to the ban structure if banned, else NULL.
 */
Ban *is_banned(aClient *sptr, aChannel *chptr, int type)
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
		s = make_nick_user_host(sptr->name, sptr->user->username, GetIP(sptr));
		strlcpy(nuip, s, sizeof nuip);
		ban_ip = nuip;
	}

	if (sptr->user->virthost)
		if (strcmp(sptr->user->realhost, sptr->user->virthost))
		{
			dovirt = 1;
		}

	s = make_nick_user_host(sptr->name, sptr->user->username,
	    sptr->user->realhost);
	strlcpy(realhost, s, sizeof realhost);

	if (dovirt)
	{
		s = make_nick_user_host(sptr->name, sptr->user->username,
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
			if (lp->flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP|CHFL_HALFOP))
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
		if (strchr((char *)msgtext, 3) || strchr((char *)msgtext, 27))
			return (CANNOT_SEND_NOCOLOR);

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
			if ((chptr->mode.mode & MODE_AUDITORIUM) && !is_irc_banned(chptr))
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
		if ((mask == CHFL_BAN) || (mask == CHFL_EXCEPT))
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

/*
 * m_mode -- written by binary (garryb@binary.islesfan.net)
 *	Completely rewrote it.  The old mode command was 820 lines of ICKY
 * coding, which is a complete waste, because I wrote it in 570 lines of
 * *decent* coding.  This is also easier to read, change, and fine-tune.  Plus,
 * everything isn't scattered; everything's grouped where it should be.
 *
 * parv[0] - sender
 * parv[1] - channel
 */
CMD_FUNC(m_mode)
{
	long unsigned sendts = 0;
	Ban *ban;
	aChannel *chptr;


	/* Now, try to find the channel in question */
	if (parc > 1)
	{
		if (*parv[1] == '#')
		{
			chptr = find_channel(parv[1], NullChn);
			if (chptr == NullChn)
			{
				return m_umode(cptr, sptr, parc, parv);
			}
		}
		else
			return m_umode(cptr, sptr, parc, parv);
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "MODE");
		return 0;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	{
		*modebuf = *parabuf = '\0';
		
		modebuf[1] = '\0';
		channel_modes(sptr, modebuf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
		    chptr->chname, modebuf, parabuf);
		sendto_one(sptr, rpl_str(RPL_CREATIONTIME), me.name, parv[0],
		    chptr->chname, chptr->creationtime);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'b'
	    && parv[2][1] == '\0') || (parv[2][1] == 'b' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr))
			return 0;
		/* send ban list */
		for (ban = chptr->banlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_BANLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(cptr,
		    rpl_str(RPL_ENDOFBANLIST), me.name, sptr->name,
		    chptr->chname);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'e'
	    && parv[2][1] == '\0') || (parv[2][1] == 'e' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr))
			return 0;
		/* send exban list */
		for (ban = chptr->exlist; ban; ban = ban->next)
			sendto_one(sptr, rpl_str(RPL_EXLIST), me.name,
			    sptr->name, chptr->chname, ban->banstr,
			    ban->who, ban->when);
		sendto_one(cptr,
		    rpl_str(RPL_ENDOFEXLIST), me.name, sptr->name,
		    chptr->chname);
		return 0;
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'q'
	    && parv[2][1] == '\0') || (parv[2][1] == 'q' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr))
			return 0;
		{
			Member *member;
			/* send chanowner list */
			/* [Whole story about bad loops removed, sorry ;)]
			 * Now rewritten so it works (was: bad logic) -- Syzop
			 */
			for (member = chptr->members; member; member = member->next)
			{
				if (is_chanowner(member->cptr, chptr))
					sendto_one(sptr, rpl_str(RPL_QLIST),
					    me.name, sptr->name, chptr->chname,
					    member->cptr->name);
			}
			sendto_one(cptr,
			    rpl_str(RPL_ENDOFQLIST), me.name, sptr->name,
			    chptr->chname);
			return 0;
		}
	}

	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'a'
	    && parv[2][1] == '\0') || (parv[2][1] == 'a' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr))
			return 0;
		{
			Member *member;
			/* send chanowner list */
			/* [Whole story about bad loops removed, sorry ;)]
			 * Now rewritten so it works (was: bad logic) -- Syzop
			 */
			for (member = chptr->members; member; member = member->next)
			{
				if (is_chanprot(member->cptr, chptr))
					sendto_one(sptr, rpl_str(RPL_ALIST),
					    me.name, sptr->name, chptr->chname,
					    member->cptr->name);
			}
			sendto_one(cptr,
			    rpl_str(RPL_ENDOFALIST), me.name, sptr->name,
			    chptr->chname);
			return 0;
		}
	}


	if (IsPerson(sptr) && parc < 4 && ((*parv[2] == 'I'
	    && parv[2][1] == '\0') || (parv[2][1] == 'I' && parv[2][2] == '\0'
	    && (*parv[2] == '+' || *parv[2] == '-'))))
	{
		if (!IsMember(sptr, chptr))
			return 0;
		sendto_one(sptr, rpl_str(RPL_ENDOFINVEXLIST), me.name,
		    sptr->name, chptr->chname);
		return 0;
	}
	opermode = 0;

#ifndef NO_OPEROVERRIDE
        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && !is_half_op(sptr, chptr) && (MyClient(sptr) ? (IsOper(sptr) &&
	    OPCanOverride(sptr)) : IsOper(sptr)))
        {
                sendts = 0;
                opermode = 1;
                goto aftercheck;
        }

        if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
            && is_half_op(sptr, chptr) && (MyClient(sptr) ? (IsOper(sptr) &&
	    OPCanOverride(sptr)) : IsOper(sptr)))
        {
                opermode = 2;
                goto aftercheck;
        }
#endif

	if (IsPerson(sptr) && !IsULine(sptr) && !is_chan_op(sptr, chptr)
	    && !is_half_op(sptr, chptr)
	    && (cptr == sptr || !IsSAdmin(sptr) || !IsOper(sptr)))
	{
		if (cptr == sptr)
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
			return 0;
		}
		sendto_one(cptr, ":%s MODE %s -oh %s %s 0",
		    me.name, chptr->chname, parv[0], parv[0]);
		/* Tell the other server that the user is
		 * de-opped.  Fix op desyncs. */
		bounce_mode(chptr, cptr, parc - 2, parv + 2);
		return 0;
	}

	if (IsServer(sptr) && (sendts = TS2ts(parv[parc - 1]))
	    && !IsULine(sptr) && chptr->creationtime
	    && sendts > chptr->creationtime)
	{
		if (!(*parv[2] == '&'))	/* & denotes a bounce */
		{
			/* !!! */
			sendto_snomask(SNO_EYES,
			    "*** TS bounce for %s - %lu(ours) %lu(theirs)",
			    chptr->chname, chptr->creationtime, sendts);
			bounce_mode(chptr, cptr, parc - 2, parv + 2);
			return 0;
		}
		/* other server will resync soon enough... */
	}
	if (IsServer(sptr) && !sendts && *parv[parc - 1] != '0')
		sendts = -1;
	if (IsServer(sptr) && sendts != -1)
		parc--;		/* server supplied a time stamp, remove it now */

      aftercheck:
/*	if (IsPerson(sptr) && IsOper(sptr)) {
		if (!is_chan_op(sptr, chptr)) {
			if (MyClient(sptr) && !IsULine(cptr) && mode_buf[1])
				sendto_snomask(SNO_EYES, "*** OperMode [IRCop: %s] - [Channel: %s] - [Mode: %s %s]",
        	 		   sptr->name, chptr->chname, mode_buf, parabuf);
			sendts = 0;
		}
	}	
*/
	/* Filter out the unprivileged FIRST. *
	 * Now, we can actually do the mode.  */

	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, sendts, 0);
	opermode = 0; /* Important since sometimes forgotten. -- Syzop */
	return 0;
}

/* bounce_mode -- written by binary
 *	User or server is NOT authorized to change the mode.  This takes care
 * of making the bounce string and bounce it.  Because of the 1 for the bounce
 * param (last param) of the calls to set_mode and make_mode_str, it will not
 * set the mode, but create the bounce string.
 */
void bounce_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[])
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;

	set_mode(chptr, cptr, parc, parv, &pcount, pvar, 1);

	if (chptr->creationtime)
		sendto_one(cptr, ":%s MODE %s &%s %s %lu", me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s MODE %s &%s %s", me.name, chptr->chname,
		    modebuf, parabuf);

	/* the '&' denotes a bounce so servers won't bounce a bounce */
}

/* do_mode -- written by binary
 *	User or server is authorized to do the mode.  This takes care of
 * setting the mode and relaying it to other users and servers.
 */
void do_mode(aChannel *chptr, aClient *cptr, aClient *sptr, int parc, char *parv[], time_t sendts, int samode)
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	char tschange = 0, isbounce = 0;	/* fwd'ing bounce */

	if (**parv == '&')
		isbounce = 1;

	set_mode(chptr, sptr, parc, parv, &pcount, pvar, 0);

	if (IsServer(sptr))
	{
		if (sendts > 0)
		{
			if (!chptr->creationtime
			    || sendts < chptr->creationtime)
			{
				tschange = 1;
/*
				if (chptr->creationtime != 0)
					sendto_snomask(SNO_EYES, "*** TS fix for %s - %lu(ours) %lu(theirs)",
					chptr->chname, chptr->creationtime, sendts);			
					*/
				chptr->
creationtime = sendts;
#if 0
				if (sendts < 750000)
					sendto_realops(
						"Warning! Possible desynch: MODE for channel %s ('%s %s') has fishy timestamp (%ld) (from %s/%s)"
						chptr->chname, modebuf, parabuf, sendts, cptr->name, sptr->name);
#endif
				/* new chan or our timestamp is wrong */
				/* now works for double-bounce prevention */

			}
			if (sendts > chptr->creationtime && chptr->creationtime)
			{
				/* theirs is wrong but we let it pass anyway */
				sendts = chptr->creationtime;
				sendto_one(cptr, ":%s MODE %s + %lu", me.name,
				    chptr->chname, chptr->creationtime);
			}
		}
		if (sendts == -1 && chptr->creationtime)
			sendts = chptr->creationtime;
	}
	if (*modebuf == '\0' || (*(modebuf + 1) == '\0' && (*modebuf == '+'
	    || *modebuf == '-')))
	{
		if (tschange || isbounce) {	/* relay bounce time changes */
			if (chptr->creationtime)
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE, "%s %s+ %lu",
				    chptr->chname, isbounce ? "&" : "",
				    chptr->creationtime);
			else
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE, "%s %s+ ",
				    chptr->chname, isbounce ? "&" : "");
		return;		/* nothing to send */
		}
	}
	/* opermode for twimodesystem --sts */
#ifndef NO_OPEROVERRIDE
	if (opermode == 1)
	{
		if (modebuf[1])
			sendto_snomask(SNO_EYES,
			    "*** OperOverride -- %s (%s@%s) MODE %s %s %s",
			    sptr->name, sptr->user->username, sptr->user->realhost,
			    chptr->chname, modebuf, parabuf);

			/* Logging Implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) MODE %s %s %s",
				sptr->name, sptr->user->username, sptr->user->realhost,
				chptr->chname, modebuf, parabuf);

		sendts = 0;
	}
#endif

	/* Should stop null modes */
	if (*(modebuf + 1) == '\0')
		return;
	if (IsPerson(sptr) && samode && MyClient(sptr))
	{
		sendto_serv_butone_token(NULL, me.name, MSG_GLOBOPS,
		    TOK_GLOBOPS, ":%s used SAMODE %s (%s%s%s)", sptr->name,
		    chptr->chname, modebuf, *parabuf ? " " : "", parabuf);
		sendto_failops_whoare_opers
		    ("from %s: %s used SAMODE %s (%s%s%s)", me.name, sptr->name,
		    chptr->chname, modebuf, *parabuf ? " " : "", parabuf);
		sptr = &me;
		sendts = 0;
	}

	
	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	    sptr->name, chptr->chname, modebuf, parabuf);
	if (IsServer(sptr) && sendts != -1)
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s %lu", chptr->chname, isbounce ? "&" : "",
		    modebuf, parabuf, sendts);
	else
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s", chptr->chname, isbounce ? "&" : "",
		    modebuf, parabuf);
	/* tell them it's not a timestamp, in case the last param
	   ** is a number. */

	if (MyConnect(sptr))
		RunHook7(HOOKTYPE_LOCAL_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
	else
		RunHook7(HOOKTYPE_REMOTE_CHANMODE, cptr, sptr, chptr, modebuf, parabuf, sendts, samode);
}
/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 *  If bounce is set to 1, it will make the string it needs for a bounce.
 */
#ifdef EXTCMODE
void make_mode_str(aChannel *chptr, long oldm, Cmode_t oldem, long oldl, int pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce)
#else
void make_mode_str(aChannel *chptr, long oldm, long oldl, int pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char *mode_buf, char *para_buf, char bounce)
#endif
{

	char tmpbuf[MODEBUFLEN+3], *tmpstr;
	aCtab *tab = &cFlagTab[0];
	char *x = mode_buf;
	int  what, cnt, z;
#ifdef EXTCMODE
	int i;
#endif
	char *m;
	what = 0;

	*tmpbuf = '\0';
	*mode_buf = '\0';
	*para_buf = '\0';
	what = 0;
	/* + param-less modes */
	tab = &cFlagTab[0];
	while (tab->mode != 0x0)
	{
		if (chptr->mode.mode & tab->mode)
		{
			if (!(oldm & tab->mode))
			{
				if (what != MODE_ADD)
				{
					*x++ = bounce ? '-' : '+';
					what = MODE_ADD;
				}
				*x++ = tab->flag;
			}
		}
		tab++;
	}
#ifdef EXTCMODE
	/* + paramless extmodes... */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!Channelmode_Table[i].flag || Channelmode_Table[i].paracount)
			continue;
		/* have it now and didn't have it before? */
		if ((chptr->mode.extmode & Channelmode_Table[i].mode) &&
		    !(oldem & Channelmode_Table[i].mode))
		{
			if (what != MODE_ADD)
			{
				*x++ = bounce ? '-' : '+';
				what = MODE_ADD;
			}
			*x++ = Channelmode_Table[i].flag;
		}
	}
#endif

	*x = '\0';
	/* - param-less modes */
	tab = &cFlagTab[0];
	while (tab->mode != 0x0)
	{
		if (!(chptr->mode.mode & tab->mode))
		{
			if (oldm & tab->mode)
			{
				if (what != MODE_DEL)
				{
					*x++ = bounce ? '+' : '-';
					what = MODE_DEL;
				}
				*x++ = tab->flag;
			}
		}
		tab++;
	}

#ifdef EXTCMODE
	/* - extmodes (both "param modes" and paramless don't have
	 * any params when unsetting...
	 */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!Channelmode_Table[i].flag /* || Channelmode_Table[i].paracount */)
			continue;
		/* don't have it now and did have it before */
		if (!(chptr->mode.extmode & Channelmode_Table[i].mode) &&
		    (oldem & Channelmode_Table[i].mode))
		{
			if (what != MODE_DEL)
			{
				*x++ = bounce ? '+' : '-';
				what = MODE_DEL;
			}
			*x++ = Channelmode_Table[i].flag;
		}
	}
#endif

	*x = '\0';
	/* user limit */
	if (chptr->mode.limit != oldl)
	{
		if ((!bounce && chptr->mode.limit == 0) ||
		    (bounce && chptr->mode.limit != 0))
		{
			if (what != MODE_DEL)
			{
				*x++ = '-';
				what = MODE_DEL;
			}
			if (bounce)
				chptr->mode.limit = 0;	/* set it back */
			*x++ = 'l';
		}
		else
		{
			if (what != MODE_ADD)
			{
				*x++ = '+';
				what = MODE_ADD;
			}
			*x++ = 'l';
			if (bounce)
				chptr->mode.limit = oldl;	/* set it back */
			ircsprintf(para_buf, "%s%d ", para_buf, chptr->mode.limit);
		}
	}
	/* reconstruct bkov chain */
	for (cnt = 0; cnt < pcount; cnt++)
	{
		if ((*(pvar[cnt]) == '+') && what != MODE_ADD)
		{
			*x++ = bounce ? '-' : '+';
			what = MODE_ADD;
		}
		if ((*(pvar[cnt]) == '-') && what != MODE_DEL)
		{
			*x++ = bounce ? '+' : '-';
			what = MODE_DEL;
		}
		*x++ = *(pvar[cnt] + 1);
		tmpstr = &pvar[cnt][2];
		z = (MODEBUFLEN * MAXMODEPARAMS);
		m = para_buf;
		while ((*m)) { m++; }
		while ((*tmpstr) && ((m-para_buf) < z))
		{
			*m = *tmpstr; 
			m++;
			tmpstr++;
		}
		*m++ = ' ';
		*m = '\0';
	}
	if (bounce)
	{
		chptr->mode.mode = oldm;
#ifdef EXTCMODE
		chptr->mode.extmode = oldem;
#endif
	}
	z = strlen(para_buf);
	if (para_buf[z - 1] == ' ')
		para_buf[z - 1] = '\0';
	*x = '\0';
	if (*mode_buf == '\0')
	{
		*mode_buf = '+';
		mode_buf++;
		*mode_buf = '\0';
		/* Don't send empty lines. */
	}
	return;
}


/* do_mode_char
 *  processes one mode character
 *  returns 1 if it ate up a param, otherwise 0
 *	written by binary
 *  modified for Unreal by stskeeps..
 */

int  do_mode_char(aChannel *chptr, long modetype, char modechar, char *param, 
	u_int what, aClient *cptr,
	 u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], char bounce)
{
	aCtab *tab = &cFlagTab[0];


	int  retval = 0;
	Member *member = NULL;
	Membership *membership = NULL;
	aClient *who;
	unsigned int tmp = 0;
	char tmpbuf[512], *tmpstr;
	char tc = ' ';		/* */
	int  chasing, x;
	int xxi, xyi, xzi, hascolon;
	char *xp;
	int  notsecure;
	chasing = 0;
	if (is_half_op(cptr, chptr) && !is_chan_op(cptr, chptr) && !IsULine(cptr)
	    && !op_can_override(cptr))
	{
		/* Ugly halfop hack --sts 
		   - this allows halfops to do +b +e +v and so on */
		/* (Syzop/20040413: Allow remote halfop modes */
		if ((Halfop_mode(modetype) == FALSE) && MyClient(cptr))
		{
			int eaten = 0;
			while (tab->mode != 0x0)
			{
				if (tab->mode == modetype)
				{
					sendto_one(cptr,
					    err_str(ERR_NOTFORHALFOPS), me.name,
					    cptr->name, tab->flag);
					eaten = tab->parameters;
					break;
				}
				tab++;
			}
			return eaten;
		}
	}
	switch (modetype)
	{
	  case MODE_AUDITORIUM:
		  if (IsULine(cptr) || IsServer(cptr))
			  goto auditorium_ok;
		  if (!IsNetAdmin(cptr) && !is_chanowner(cptr, chptr))
		  {
			sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name,
				   chptr->chname);
			break;
		  }

		auditorium_ok:
		  goto setthephuckingmode;
	  case MODE_OPERONLY:
		  if (!IsAnOper(cptr) && !IsServer(cptr)
		      && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_ADMONLY:
		  if (!IsSkoAdmin(cptr) && !IsServer(cptr)
		      && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_NOPRIVILEGES), me.name, cptr->name);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_RGSTR:
		  if (!IsServer(cptr) && !IsULine(cptr))
		  {
			sendto_one(cptr, err_str(ERR_ONLYSERVERSCANCHANGE), me.name, cptr->name,
				   chptr->chname);
			break;
		  }
		  goto setthephuckingmode;
	  case MODE_SECRET:
	  case MODE_PRIVATE:
	  case MODE_MODERATED:
	  case MODE_TOPICLIMIT:
	  case MODE_NOPRIVMSGS:
	  case MODE_RGSTRONLY:
	  case MODE_MODREG:
	  case MODE_NOCOLOR:
	  case MODE_NOKICKS:
	  case MODE_STRIP:
	  	goto setthephuckingmode;

	  case MODE_INVITEONLY:
		if (what == MODE_DEL && modetype == MODE_INVITEONLY && (chptr->mode.mode & MODE_NOKNOCK))
			chptr->mode.mode &= ~MODE_NOKNOCK;
		goto setthephuckingmode;
	  case MODE_NOKNOCK:
		if (what == MODE_ADD && modetype == MODE_NOKNOCK && !(chptr->mode.mode & MODE_INVITEONLY))
		{
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
				me.name, cptr->name, 'K', "+i must be set");
			break;
		}
		goto setthephuckingmode;
	  case MODE_ONLYSECURE:
	  	notsecure = 0;
	  	if (what == MODE_ADD && modetype == MODE_ONLYSECURE && !(IsServer(cptr) || IsULine(cptr)))
		{
		  for (member = chptr->members; member; member = member->next)
		  {
		    if (!IsSecureConnect(member->cptr) && !IsULine(member->cptr))
		    {
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
				   me.name, cptr->name, 'z', 
				   "all members must be connected via SSL");
			notsecure = 1;
			break;
		    }
		  }
		  member = NULL;
		  /* first break nailed the for loop, this one nails switch() */
		  if (notsecure == 1) break;
		}
		goto setthephuckingmode;
#ifdef STRIPBADWORDS
	  case MODE_STRIPBADWORDS:
#endif
	  case MODE_NOCTCP:
	  case MODE_NONICKCHANGE:
	  case MODE_NOINVITE:
		setthephuckingmode:
		  retval = 0;
		  if (what == MODE_ADD) {
			  /* +sp bugfix.. (by JK/Luke)*/
		 	 if (modetype == MODE_SECRET
			      && (chptr->mode.mode & MODE_PRIVATE))
				  chptr->mode.mode &= ~MODE_PRIVATE;
			  if (modetype == MODE_PRIVATE
			      && (chptr->mode.mode & MODE_SECRET))
				  chptr->mode.mode &= ~MODE_SECRET;
			  if (modetype == MODE_NOCOLOR
			      && (chptr->mode.mode & MODE_STRIP))
				  chptr->mode.mode &= ~MODE_STRIP;
			  if (modetype == MODE_STRIP
			      && (chptr->mode.mode & MODE_NOCOLOR))
				  chptr->mode.mode &= ~MODE_NOCOLOR;
			  chptr->mode.mode |= modetype;
		  }
		  else
		  {
			  chptr->mode.mode &= ~modetype;
#ifdef NEWCHFLOODPROT
			  /* reset joinflood on -i, reset msgflood on -m, etc.. */
			  if (chptr->mode.floodprot)
			  {
				switch(modetype)
				{
				case MODE_NOCTCP:
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'C', MODE_NOCTCP);
					break;
				case MODE_NONICKCHANGE:
					chptr->mode.floodprot->c[FLD_NICK] = 0;
					chanfloodtimer_del(chptr, 'N', MODE_NONICKCHANGE);
					break;
				case MODE_MODERATED:
					chptr->mode.floodprot->c[FLD_MSG] = 0;
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'm', MODE_MODERATED);
					break;
				case MODE_NOKNOCK:
					chptr->mode.floodprot->c[FLD_KNOCK] = 0;
					chanfloodtimer_del(chptr, 'K', MODE_NOKNOCK);
					break;
				case MODE_INVITEONLY:
					chptr->mode.floodprot->c[FLD_JOIN] = 0;
					chanfloodtimer_del(chptr, 'i', MODE_INVITEONLY);
					break;
				case MODE_MODREG:
					chptr->mode.floodprot->c[FLD_MSG] = 0;
					chptr->mode.floodprot->c[FLD_CTCP] = 0;
					chanfloodtimer_del(chptr, 'M', MODE_MODREG);
					break;
				case MODE_RGSTRONLY:
					chptr->mode.floodprot->c[FLD_JOIN] = 0;
					chanfloodtimer_del(chptr, 'R', MODE_RGSTRONLY);
					break;
				default:
					break;
				}
			  }
#endif
		  }
		  break;

/* do pro-opping here (popping) */
	  case MODE_CHANOWNER:
		  if (!IsULine(cptr) && !IsServer(cptr)
		       && !is_chanowner(cptr, chptr))
		  {
			  if (IsNetAdmin(cptr))
			  {
				if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
				   opermode = 1;
			  }
			  else if (MyClient(cptr))
			  {
				  sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED),
				      me.name, cptr->name, chptr->chname);
				  break;
			  }
		  }
	  case MODE_CHANPROT:
		  if (!IsULine(cptr) && !IsServer(cptr)
		      && !is_chanowner(cptr, chptr))
		  {
			  if (IsNetAdmin(cptr))
			  {
				if (!is_halfop(cptr, chptr)) /* htrig will take care of halfop override notices */
				   opermode = 1;
			  }
			  else if (MyClient(cptr))
			  {
				sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name,
					   chptr->chname);
				break;
			  }
		  }
		 

	  case MODE_HALFOP:
	  case MODE_CHANOP:
	  case MODE_VOICE:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  if (!(who = find_chasing(cptr, param, &chasing)))
			  break;
		  if (!who->user)
		  	break;
   		  /* codemastr: your patch is a good idea here, but look at the
   		     member->flags stuff longer down. this caused segfaults */
   		  if (!(membership = find_membership_link(who->user->channel, chptr)))
		  {
			  sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
			      me.name, cptr->name, who->name, chptr->chname);
			  break;
		  }
		  member = find_member_link(chptr->members, who);
		  if (!member)
		  {
		  	/* should never happen */
		  	sendto_realops("crap! find_membership_link && !find_member_link !!. Report to unreal team");
		  	break;
		  }
		  /* we make the rules, we bend the rules */
		  if (IsServer(cptr) || IsULine(cptr))
			  goto breaktherules;
		
		  /* Services are special! */
		  if (IsServices(member->cptr) && MyClient(cptr) && !IsNetAdmin(cptr) && (what == MODE_DEL))
		  {
			char errbuf[NICKLEN+50];
			ircsprintf(errbuf, "%s is a network service", member->cptr->name);
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
				   modechar, errbuf);
			break;
		  }

		  if (is_chanowner(member->cptr, chptr)
		      && member->cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr)
		      && !IsULine(cptr) && !opermode && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
				char errbuf[NICKLEN+30];
				ircsprintf(errbuf, "%s is a channel owner", member->cptr->name);
				sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
				   modechar, errbuf);
				break;
			  } else
			  if (IsOper(cptr))
			      opermode = 1;
		  }
		  if (is_chanprot(member->cptr, chptr)
		      && member->cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr) && !opermode
		      && modetype != MODE_CHANOWNER && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
				char errbuf[NICKLEN+30];
				ircsprintf(errbuf, "%s is a channel admin", member->cptr->name);
				sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), me.name, cptr->name,
				   modechar, errbuf);
				break;
			  } else
			  if (IsOper(cptr))
			      opermode = 1;
		  }
		breaktherules:
		  tmp = member->flags;
		  if (what == MODE_ADD)
			  member->flags |= modetype;
		  else
			  member->flags &= ~modetype;
		  if ((tmp == member->flags) && (bounce || !IsULine(cptr)))
			  break;
		  /* It's easier to undo the mode here instead of later
		   * when you call make_mode_str for a bounce string.
		   * Why set it if it will be instantly removed?
		   * Besides, pvar keeps a log of it. */
		  if (bounce)
			  member->flags = tmp;
		  if (modetype == MODE_CHANOWNER)
			  tc = 'q';
		  if (modetype == MODE_CHANPROT)
			  tc = 'a';
		  if (modetype == MODE_CHANOP)
			  tc = 'o';
		  if (modetype == MODE_HALFOP)
			  tc = 'h';
		  if (modetype == MODE_VOICE)
			  tc = 'v';
		  /* Make sure membership->flags and member->flags is the same */
		  membership->flags = member->flags;
		  (void)ircsprintf(pvar[*pcount], "%c%c%s",
		      what == MODE_ADD ? '+' : '-', tc, who->name);
		  (*pcount)++;
		  break;
	  case MODE_LIMIT:
		  if (what == MODE_ADD)
		  {
			  if (!param)
			  {
				  retval = 0;
				  break;
			  }
			  retval = 1;
			  tmp = atoi(param);
			  if (chptr->mode.limit == tmp)
				  break;
			  chptr->mode.limit = tmp;
		  }
		  else
		  {
			  retval = 0;
			  if (!chptr->mode.limit)
				  break;
			  chptr->mode.limit = 0;
		  }
		  break;
	  case MODE_KEY:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'k')
			  {	/* don't allow user to change key
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
		  if (what == MODE_ADD)
		  {
			  if (!bounce) {	/* don't do the mode at all. */
				  char *tmp;
				  if ((tmp = strchr(param, ' ')))
					*tmp = '\0';
				  if ((tmp = strchr(param, ':')))
					*tmp = '\0';
				  if ((tmp = strchr(param, ',')))
					*tmp = '\0';
				  if (*param == '\0')
					break;
				  if (strlen(param) > KEYLEN)
				    param[KEYLEN] = '\0';
				  if (!strcmp(chptr->mode.key, param))
					break;
				  strncpyzt(chptr->mode.key, param,
				      sizeof(chptr->mode.key));
			  }
			  tmpstr = param;
		  }
		  else
		  {
			  if (!*chptr->mode.key)
				  break;	/* no change */
			  strncpyzt(tmpbuf, chptr->mode.key, sizeof(tmpbuf));
			  tmpstr = tmpbuf;
			  if (!bounce)
				  strcpy(chptr->mode.key, "");
		  }
		  retval = 1;

		  (void)ircsprintf(pvar[*pcount], "%ck%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;

	  case MODE_BAN:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  tmpstr = clean_ban_mask(param, what, cptr);
		  if (BadPtr(tmpstr))
		      break; /* ignore ban, but eat param */
		  if ((tmpstr[0] == '~') && MyClient(cptr) && !bounce)
		  {
		      /* extban: check access if needed */
		      Extban *p = findmod_by_bantype(tmpstr[1]);
		      if (p && p->is_ok)
		      {
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS, what, EXBTYPE_BAN))
		        {
		            if (IsAnOper(cptr))
		            {
		                /* TODO: send operoverride notice */
  		            } else {
		                p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS_ERR, what, EXBTYPE_BAN);
		                break;
		            }
		        }
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_PARAM, what, EXBTYPE_BAN))
		            break;
		     }
		  }
		  /* For bounce, we don't really need to worry whether
		   * or not it exists on our server.  We'll just always
		   * bounce it. */
		  if (!bounce &&
		      ((what == MODE_ADD && add_banid(cptr, chptr, tmpstr))
		      || (what == MODE_DEL && del_banid(chptr, tmpstr))))
			  break;	/* already exists */
		  (void)ircsprintf(pvar[*pcount], "%cb%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_EXCEPT:
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  tmpstr = clean_ban_mask(param, what, cptr);
		  if (BadPtr(tmpstr))
		     break; /* ignore except, but eat param */
		  if ((tmpstr[0] == '~') && MyClient(cptr) && !bounce)
		  {
		      /* extban: check access if needed */
		      Extban *p = findmod_by_bantype(tmpstr[1]);
		      if (p && p->is_ok)
       		      {
			 if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS, what, EXBTYPE_EXCEPT))
		         {
		            if (IsAnOper(cptr))
		            {
		                /* TODO: send operoverride notice */
		            } else {
		                p->is_ok(cptr, chptr, tmpstr, EXBCHK_ACCESS_ERR, what, EXBTYPE_EXCEPT);
		                break;
		            }
		        }
			if (!p->is_ok(cptr, chptr, tmpstr, EXBCHK_PARAM, what, EXBTYPE_EXCEPT))
		            break;
		     }
		  }
		  /* For bounce, we don't really need to worry whether
		   * or not it exists on our server.  We'll just always
		   * bounce it. */
		  if (!bounce &&
		      ((what == MODE_ADD && add_exbanid(cptr, chptr, tmpstr))
		      || (what == MODE_DEL && del_exbanid(chptr, tmpstr))))
			  break;	/* already exists */
		  (void)ircsprintf(pvar[*pcount], "%ce%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_LINK:
		  if (IsULine(cptr) || IsServer(cptr))
		  {
			  goto linkok;
		  }

		  if (!IsNetAdmin(cptr) && !is_chanowner(cptr, chptr))
		  {
			sendto_one(cptr, err_str(ERR_CHANOWNPRIVNEEDED), me.name, cptr->name,
				   chptr->chname);
			break;
		  }

		  if (!chptr->mode.limit && what == MODE_ADD)
		  {
			sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
				   me.name, cptr->name, 'L', "+l must be set");
			break;
		  }
		linkok:
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'L')
			  {	/* don't allow user to change link
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
		  if (what == MODE_ADD)
		  {
		      char *tmp;
			  if (!param || *pcount >= MAXMODEPARAMS)
			  {
				  retval = 0;
				  break;
			  }
			  if (strchr(param, ','))
				  break;
			  if (!IsChannelName(param))
			  {
				  if (MyClient(cptr))
					  sendto_one(cptr,
					      err_str(ERR_NOSUCHCHANNEL),
					      me.name, cptr->name, param);
				  break;
			  }
			  /* Now make it a clean channelname.. This has to be done before all checking
			   * because it could have been changed later to something disallowed (like
			   * self-linking). -- Syzop
			   */
			  strlcpy(tmpbuf, param, CHANNELLEN+1);
			  clean_channelname(tmpbuf);
			  /* don't allow linking to local chans either.. */
			  if ((tmp = strchr(tmpbuf, ':')))
				*tmp = '\0';

			  if (!stricmp(tmpbuf, chptr->mode.link))
				break;
			  if (!stricmp(tmpbuf, chptr->chname))
			  {
				if (MyClient(cptr))
					sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
						   me.name, cptr->name, 'L', 
					    	   "a channel cannot be linked to itself");
				break;
			  }
			  if (!bounce)	/* don't do the mode at all. */
			  {
				  strncpyzt(chptr->mode.link, tmpbuf,
				      sizeof(chptr->mode.link));
			      tmpstr = tmpbuf;
			  } else
			      tmpstr = param; /* Use the original value if bounce?? -- Syzop */
		  }
		  else
		  {
			  if (!*chptr->mode.link)
				  break;	/* no change */
			  strncpyzt(tmpbuf, chptr->mode.link, sizeof(tmpbuf));
			  tmpstr = tmpbuf;
			  if (!bounce)
			  {
				  strcpy(chptr->mode.link, "");
			  }
		  }
		  retval = 1;

		  (void)ircsprintf(pvar[*pcount], "%cL%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	  case MODE_FLOODLIMIT:
		  retval = 1;
		  for (x = 0; x < *pcount; x++)
		  {
			  if (pvar[x][1] == 'f')
			  {	/* don't allow user to change flood
				 * more than once per command. */
				  retval = 0;
				  break;
			  }
		  }
		  if (retval == 0)	/* you can't break a case from loop */
			  break;
#ifndef NEWCHFLOODPROT
		  if (what == MODE_ADD)
		  {
			  if (!bounce)	/* don't do the mode at all. */
			  {
				  if (!param || *pcount >= MAXMODEPARAMS)
				  {
					  retval = 0;
					  break;
				  }

				  /* like 1:1 and if its less than 3 chars then ahem.. */
				  if (strlen(param) < 3)
				  {
					  break;
				  }
				  /* may not contain other chars 
				     than 0123456789: & NULL */
				  hascolon = 0;
				  for (xp = param; *xp; xp++)
				  {
					  if (*xp == ':')
						hascolon++;
					  /* fast alpha check */
					  if (((*xp < '0') || (*xp > '9'))
					      && (*xp != ':')
					      && (*xp != '*'))
						goto break_flood;
					  /* uh oh, not the first char */
					  if (*xp == '*' && (xp != param))
						goto break_flood;
				  }
				  /* We can avoid 2 strchr() and a strrchr() like this
				   * it should be much faster. -- codemastr
				   */
				  if (hascolon != 1)
					break;
				  if (*param == '*')
				  {
					  xzi = 1;
					  //                      chptr->mode.kmode = 1;
				  }
				  else
				  {
					  xzi = 0;

					  //                   chptr->mode.kmode = 0;
				  }
				  xp = index(param, ':');
				  *xp = '\0';
				  xxi =
				      atoi((*param ==
				      '*' ? (param + 1) : param));
				  xp++;
				  xyi = atoi(xp);
				  if (xxi > 500 || xyi > 500)
					break;
				  xp--;
				  *xp = ':';
				  if ((xxi == 0) || (xyi == 0))
					  break;
				  if ((chptr->mode.msgs == xxi)
				      && (chptr->mode.per == xyi)
				      && (chptr->mode.kmode == xzi))
					  break;
				  chptr->mode.msgs = xxi;
				  chptr->mode.per = xyi;
				  chptr->mode.kmode = xzi;
			  }
			  tmpstr = param;
			  retval = 1;
		  }
		  else
		  {
			  if (!chptr->mode.msgs || !chptr->mode.per)
				  break;	/* no change */
			  ircsprintf(tmpbuf,
			      (chptr->mode.kmode > 0 ? "*%i:%i" : "%i:%i"),
			      chptr->mode.msgs, chptr->mode.per);
			  tmpstr = tmpbuf;
			  if (!bounce)
			  {
				  chptr->mode.msgs = chptr->mode.per =
				      chptr->mode.kmode = 0;
			  }
			  retval = 0;
		  }
#else
		/* NEW */
		if (what == MODE_ADD)
		{
			if (!bounce)	/* don't do the mode at all. */
			{
				ChanFloodProt newf;
				memset(&newf, 0, sizeof(newf));

				if (!param || *pcount >= MAXMODEPARAMS)
				{
					retval = 0;
					break;
				}

				/* old +f was like +f 10:5 or +f *10:5
				 * new is +f [5c,30j,10t#b]:15
				 * +f 10:5  --> +f [10t]:5
				 * +f *10:5 --> +f [10t#b]:5
				 */
				if (param[0] != '[')
				{
					/* <<OLD +f>> */
				  /* like 1:1 and if its less than 3 chars then ahem.. */
				  if (strlen(param) < 3)
				  {
					  break;
				  }
				  /* may not contain other chars 
				     than 0123456789: & NULL */
				  hascolon = 0;
				  for (xp = param; *xp; xp++)
				  {
					  if (*xp == ':')
						hascolon++;
					  /* fast alpha check */
					  if (((*xp < '0') || (*xp > '9'))
					      && (*xp != ':')
					      && (*xp != '*'))
						goto break_flood;
					  /* uh oh, not the first char */
					  if (*xp == '*' && (xp != param))
						goto break_flood;
				  }
				  /* We can avoid 2 strchr() and a strrchr() like this
				   * it should be much faster. -- codemastr
				   */
				  if (hascolon != 1)
					break;
				  if (*param == '*')
				  {
					  xzi = 1;
					  //                      chptr->mode.kmode = 1;
				  }
				  else
				  {
					  xzi = 0;

					  //                   chptr->mode.kmode = 0;
				  }
				  xp = index(param, ':');
				  *xp = '\0';
				  xxi =
				      atoi((*param ==
				      '*' ? (param + 1) : param));
				  xp++;
				  xyi = atoi(xp);
				  if (xxi > 500 || xyi > 500)
					break;
				  xp--;
				  *xp = ':';
				  if ((xxi == 0) || (xyi == 0))
					  break;

				  /* ok, we passed */
				  newf.l[FLD_TEXT] = xxi;
				  newf.per = xyi;
				  if (xzi == 1)
				      newf.a[FLD_TEXT] = 'b';
				} else {
					/* NEW +F */
					char xbuf[256], c, a, *p, *p2, *x = xbuf+1;
					int v;
					unsigned short warnings = 0, breakit;
					unsigned char r;

					/* '['<number><1 letter>[optional: '#'+1 letter],[next..]']'':'<number> */
					strlcpy(xbuf, param, sizeof(xbuf));
					p2 = strchr(xbuf+1, ']');
					if (!p2)
						break;
					*p2 = '\0';
					if (*(p2+1) != ':')
						break;
					breakit = 0;
					for (x = strtok(xbuf+1, ","); x; x = strtok(NULL, ","))
					{
						/* <number><1 letter>[optional: '#'+1 letter] */
						p = x;
						while(isdigit(*p)) { p++; }
						if ((*p == '\0') ||
						    !((*p == 'c') || (*p == 'j') || (*p == 'k') ||
						      (*p == 'm') || (*p == 'n') || (*p == 't')))
						{
							if (MyClient(cptr) && *p && (warnings++ < 3))
								sendto_one(cptr, ":%s NOTICE %s :warning: channelmode +f: floodtype '%c' unknown, ignored.",
									me.name, cptr->name, *p);
							continue; /* continue instead of break for forward compatability. */
						}
						c = *p;
						*p = '\0';
						v = atoi(x);
						if ((v < 1) || (v > 999)) /* out of range... */
						{
							if (MyClient(cptr))
							{
								sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE),
									   me.name, cptr->name, 
									   'f', "value should be from 1-999");
								breakit = 1;
								break;
							} else
								continue; /* just ignore for remote servers */
						}
						p++;
						a = '\0';
						r = MyClient(cptr) ? MODEF_DEFAULT_UNSETTIME : 0;
						if (*p != '\0')
						{
							if (*p == '#')
							{
								p++;
								a = *p;
								p++;
								if (*p != '\0')
								{
									int tv;
									tv = atoi(p);
									if (tv <= 0)
										tv = 0; /* (ignored) */
									if (tv > (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255))
										tv = (MyClient(cptr) ? MODEF_MAX_UNSETTIME : 255); /* set to max */
									r = (unsigned char)tv;
								}
							}
						}

						switch(c)
						{
							case 'c':
								newf.l[FLD_CTCP] = v;
								if ((a == 'm') || (a == 'M'))
									newf.a[FLD_CTCP] = a;
								else
									newf.a[FLD_CTCP] = 'C';
								newf.r[FLD_CTCP] = r;
								break;
							case 'j':
								newf.l[FLD_JOIN] = v;
								if (a == 'R')
									newf.a[FLD_JOIN] = a;
								else
									newf.a[FLD_JOIN] = 'i';
								newf.r[FLD_JOIN] = r;
								break;
							case 'k':
								newf.l[FLD_KNOCK] = v;
								newf.a[FLD_KNOCK] = 'K';
								newf.r[FLD_KNOCK] = r;
								break;
							case 'm':
								newf.l[FLD_MSG] = v;
								if (a == 'M')
									newf.a[FLD_MSG] = a;
								else
									newf.a[FLD_MSG] = 'm';
								newf.r[FLD_MSG] = r;
								break;
							case 'n':
								newf.l[FLD_NICK] = v;
								newf.a[FLD_NICK] = 'N';
								newf.r[FLD_NICK] = r;
								break;
							case 't':
								newf.l[FLD_TEXT] = v;
								if (a == 'b')
									newf.a[FLD_TEXT] = a;
								/** newf.r[FLD_TEXT] ** not supported */
								break;
							default:
								breakit=1;
								break;
						}
						if (breakit)
							break;
					} /* for */
					if (breakit)
						break;
					/* parse 'per' */
					p2++;
					if (*p2 != ':')
						break;
					p2++;
					if (!*p2)
						break;
					v = atoi(p2);
					if ((v < 1) || (v > 999)) /* 'per' out of range */
					{
						if (MyClient(cptr))
							sendto_one(cptr, err_str(ERR_CANNOTCHANGECHANMODE), 
								   me.name, cptr->name, 'f', 
								   "time range should be 1-999");
						break;
					}
					newf.per = v;
					
					/* Is anything turned on? (to stop things like '+f []:15' */
					breakit = 1;
					for (v=0; v < NUMFLD; v++)
						if (newf.l[v])
							breakit=0;
					if (breakit)
						break;
					
				} /* if param[0] == '[' */ 

				if (chptr->mode.floodprot &&
				    !memcmp(chptr->mode.floodprot, &newf, sizeof(ChanFloodProt)))
					break; /* They are identical */

				/* Good.. store the mode (and alloc if needed) */
				if (!chptr->mode.floodprot)
					chptr->mode.floodprot = MyMalloc(sizeof(ChanFloodProt));
				memcpy(chptr->mode.floodprot, &newf, sizeof(ChanFloodProt));
				strcpy(tmpbuf, channel_modef_string(chptr->mode.floodprot));
				tmpstr = tmpbuf;
			} else {
				/* bounce... */
				tmpstr = param;
			}
			retval = 1;
		} else
		{ /* MODE_DEL */
			if (!chptr->mode.floodprot)
				break; /* no change */
			if (!bounce)
			{
				strcpy(tmpbuf, channel_modef_string(chptr->mode.floodprot));
				tmpstr = tmpbuf;
				free(chptr->mode.floodprot);
				chptr->mode.floodprot = NULL;
				chanfloodtimer_stopchantimers(chptr);
			} else {
				/* bounce.. */
				tmpstr = param;
			}
			retval = 0; /* ??? copied from previous +f code. */
		}
#endif

		  (void)ircsprintf(pvar[*pcount], "%cf%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break_flood:
		  break;
	}
	return retval;
}

#ifdef EXTCMODE
/** Check access and if granted, set the extended chanmode to the requested value in memory.
  * note: if bounce is requested then the mode will not be set.
  * @returns amount of params eaten (0 or 1)
  */
int do_extmode_char(aChannel *chptr, int modeindex, char *param, u_int what,
                    aClient *cptr, u_int *pcount, char pvar[MAXMODEPARAMS][MODEBUFLEN + 3],
                    char bounce)
{
int paracnt = (what == MODE_ADD) ? Channelmode_Table[modeindex].paracount : 0;
int x;

	/* Expected a param and it isn't there? */
	if (paracnt && (!param || (*pcount >= MAXMODEPARAMS)))
		return 0;

	if (MyClient(cptr))
	{
		x = Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS, what);
		if ((x == EX_ALWAYS_DENY) ||
		    ((x == EX_DENY) && !op_can_override(cptr)))
		{
			Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS_ERR, what);
			return paracnt; /* Denied & error msg sent */
		}
		if (x == EX_DENY)
			opermode = 1; /* override in progress... */
	} else {
		/* remote user: we only need to check if we need to generate an operoverride msg */
		if (!IsULine(cptr) && IsPerson(cptr) && op_can_override(cptr) &&
		    (Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_ACCESS, what) != EX_ALLOW))
			opermode = 1; /* override in progress... */
	}

	/* Check for multiple changes in 1 command (like +y-y+y 1 2, or +yy 1 2). */
	for (x = 0; x < *pcount; x++)
	{
		if (pvar[x][1] == Channelmode_Table[modeindex].flag)
		{
			/* this is different than the old chanmode system, coz:
			 * "mode #chan +kkL #a #b #c" will get "+kL #a #b" which is wrong :p.
			 * we do eat the parameter. -- Syzop
			 */
			return paracnt;
		}
	}

	/* w00t... a parameter mode */
	if (Channelmode_Table[modeindex].paracount)
	{
		if (what == MODE_DEL)
		{
			if (!(chptr->mode.extmode & Channelmode_Table[modeindex].mode))
				return paracnt; /* There's nothing to remove! */
			/* del means any parameter is ok, the one-who-is-set will be used */
			ircsprintf(pvar[*pcount], "-%c", Channelmode_Table[modeindex].flag);
		} else {
			/* add: is the parameter ok? */
			if (Channelmode_Table[modeindex].is_ok(cptr, chptr, param, EXCHK_PARAM, what) == FALSE)
				return paracnt;
			/* is it already set at the same value? if so, ignore it. */
			if (chptr->mode.extmode & Channelmode_Table[modeindex].mode)
			{
				char *p, *p2;
				p = Channelmode_Table[modeindex].get_param(extcmode_get_struct(chptr->mode.extmodeparam,Channelmode_Table[modeindex].flag));
				p2 = Channelmode_Table[modeindex].conv_param(param);
				if (p && p2 && !strcmp(p, p2))
					return paracnt; /* ignore... */
			}
				ircsprintf(pvar[*pcount], "+%c%s",
					Channelmode_Table[modeindex].flag, Channelmode_Table[modeindex].conv_param(param));
			(*pcount)++;
		}
	}

	if (bounce) /* bounce here means: only check access and return return value */
		return paracnt;
	
	if (what == MODE_ADD)
	{	/* + */
		chptr->mode.extmode |= Channelmode_Table[modeindex].mode;
		if (Channelmode_Table[modeindex].paracount)
		{
			CmodeParam *p = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[modeindex].flag);
			CmodeParam *r;
			r = Channelmode_Table[modeindex].put_param(p, param);
			if (r != p)
				AddListItem(r, chptr->mode.extmodeparam);
		}
	} else
	{	/* - */
		chptr->mode.extmode &= ~(Channelmode_Table[modeindex].mode);
		if (Channelmode_Table[modeindex].paracount)
		{
			CmodeParam *p = extcmode_get_struct(chptr->mode.extmodeparam, Channelmode_Table[modeindex].flag);
			if (p)
			{
				DelListItem(p, chptr->mode.extmodeparam);
				Channelmode_Table[modeindex].free_param(p);
			}
		}
	}
	return paracnt;
}
#endif /* EXTCMODE */

/*
 * ListBits(bitvalue, bitlength);
 * written by Stskeeps
*/
#ifdef DEVELOP
char *ListBits(long bits, long length)
{
	char *bitstr, *p;
	long l, y;
	y = 1;
	bitstr = (char *)MyMalloc(length + 1);
	p = bitstr;
	for (l = 1; l <= length; l++)
	{
		if (bits & y)
			*p = '1';
		else
			*p = '0';
		p++;
		y = y + y;
	}
	*p = '\0';
	return (bitstr);
}
#endif


/* set_mode
 *	written by binary
 */
void set_mode(aChannel *chptr, aClient *cptr, int parc, char *parv[], u_int *pcount, 
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], int bounce)
{
	char *curchr;
	u_int what = MODE_ADD;
	long modetype = 0;
	int  paracount = 1;
#ifdef DEVELOP
	char *tmpo = NULL;
#endif
	aCtab *tab = &cFlagTab[0];
	aCtab foundat;
	int  found = 0;
	unsigned int htrig = 0;
	long oldm, oldl;
	int checkrestr = 0, warnrestr = 1;
#ifdef EXTCMODE
	int extm = 1000000; /* (default value not used but stops gcc from complaining) */
	Cmode_t oldem;
#endif
	paracount = 1;
	*pcount = 0;

	oldm = chptr->mode.mode;
	oldl = chptr->mode.limit;
#ifdef EXTCMODE
	oldem = chptr->mode.extmode;
#endif
	if (RESTRICT_CHANNELMODES && MyClient(cptr) && !IsAnOper(cptr) && !IsServer(cptr)) /* "cache" this */
		checkrestr = 1;

	for (curchr = parv[0]; *curchr; curchr++)
	{
		switch (*curchr)
		{
		  case '+':
			  what = MODE_ADD;
			  break;
		  case '-':
			  what = MODE_DEL;
			  break;
#ifdef DEVELOP
		  case '^':
			  tmpo = (char *)ListBits(chptr->mode.mode, 64);
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** %s mode is %li (0x%lx) [%s]",
			      me.name, cptr->name, chptr->chname,
			      chptr->mode.mode, chptr->mode.mode, tmpo);
			  MyFree(tmpo);
			  break;
#endif
		  default:
			  found = 0;
			  tab = &cFlagTab[0];
			  while ((tab->mode != 0x0) && found == 0)
			  {
				  if (tab->flag == *curchr)
				  {
					  found = 1;
					  foundat = *tab;
				  }
				  tab++;
			  }
			  if (found == 1)
			  {
				  modetype = foundat.mode;
			  } else {
#ifdef EXTCMODE
					/* Maybe in extmodes */
					for (extm=0; extm <= Channelmode_highest; extm++)
					{
						if (Channelmode_Table[extm].flag == *curchr)
						{
							found = 2;
							break;
						}
					}
#endif
			  }
			  if (found == 0) /* Mode char unknown */
			  {
			      /* temporary hack: eat parameters of certain future chanmodes.. */
			      if (*curchr == 'I')
				      paracount++;
				  if ((*curchr == 'j') && (what == MODE_ADD))
					  paracount++;

				  if (MyClient(cptr))
					  sendto_one(cptr, err_str(ERR_UNKNOWNMODE),
					     me.name, cptr->name, *curchr);
				  break;
			  }

			  if (checkrestr && strchr(RESTRICT_CHANNELMODES, *curchr))
			  {
				  if (warnrestr)
				  {
					sendto_one(cptr, ":%s %s %s :Setting/removing of channelmode(s) '%s' has been disabled.",
						me.name, IsWebTV(cptr) ? "PRIVMSG" : "NOTICE", cptr->name, RESTRICT_CHANNELMODES);
					warnrestr = 0;
				  }
				  paracount += foundat.parameters;
				  break;
			  }

#ifndef NO_OPEROVERRIDE
				if (found == 1)
				{
                          if ((Halfop_mode(modetype) == FALSE) && opermode == 2 && htrig != 1)
                          {
				opermode = 0;
				htrig = 1;
                          }
				}
#ifdef EXTCMODE
				else if (found == 2) {
					/* Extended mode: all override stuff is in do_extmode_char which will set
					 * opermode if appropriate. -- Syzop
					 */
				}
#endif /* EXTCMODE */
#endif /* !NO_OPEROVERRIDE */

			  /* We can afford to send off a param */
			  if (parc <= paracount)
			  	parv[paracount] = NULL;
			  if (parv[paracount] &&
			      strlen(parv[paracount]) >= MODEBUFLEN)
			        parv[paracount][MODEBUFLEN-1] = '\0';
			if (found == 1)
			{
			  paracount +=
			      do_mode_char(chptr, modetype, *curchr,
			      parv[paracount], what, cptr, pcount, pvar,
			      bounce);
			}
#ifdef EXTCMODE
			else if (found == 2)
			{
				paracount += do_extmode_char(chptr, extm, parv[paracount],
				                             what, cptr, pcount, pvar, bounce);
			}
#endif /* EXTCMODE */
			  break;
		}
	}

#ifdef EXTCMODE
	make_mode_str(chptr, oldm, oldem, oldl, *pcount, pvar, modebuf, parabuf, bounce);
#else
	make_mode_str(chptr, oldm, oldl, *pcount, pvar, modebuf, parabuf, bounce);
#endif

#ifndef NO_OPEROVERRIDE
        if (htrig == 1)
        {
                /* This is horrible. Just horrible. */
                if (!((modebuf[0] == '+' || modebuf[0] == '-') && modebuf[1] == '\0'))
                sendto_snomask(SNO_EYES, "*** OperOverride -- %s (%s@%s) MODE %s %s %s",
                      cptr->name, cptr->user->username, cptr->user->realhost,
                      chptr->chname, modebuf, parabuf);

		/* Logging Implementation added by XeRXeS */
		ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) MODE %s %s %s",
			cptr->name, cptr->user->username, cptr->user->realhost,
			chptr->chname, modebuf, parabuf);		

                htrig = 0;
                opermode = 0; /* stop double override notices... but is this ok??? -- Syzop */
        }
#endif

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

/* This function adds as an extra (weird) operoverride.
 * Currently it's only used if you try to operoverride for a +z channel,
 * if you then do '/join #chan override' it will put the channel -z and allow you directly in.
 * This is to avoid attackers from using 'race conditions' to prevent you from joining.
 * PARAMETERS: sptr = the client, chptr = the channel, mval = mode value (eg MODE_ONLYSECURE),
 *             mchar = mode char (eg 'z')
 * RETURNS: 1 if operoverride, 0 if not.
 */
int extended_operoverride(aClient *sptr, aChannel *chptr, char *key, int mval, char mchar)
{
unsigned char invited = 0;
Link *lp;

	if (!IsAnOper(sptr) || !OPCanOverride(sptr))
		return 0;

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
		{
			invited = 1;
			break;
		}
	if (invited)
	{
		if (key && !strcasecmp(key, "override"))
		{
			sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				":%s NOTICE @%s :setting channel -%c due to OperOverride request from %s",
				me.name, chptr->chname, mchar, sptr->name);
			sendto_serv_butone(&me, ":%s MODE %s -%c 0", me.name, chptr->chname, mchar);
			sendto_channel_butserv(chptr, &me, ":%s MODE %s -%c", me.name, chptr->chname, mchar);
			chptr->mode.mode &= ~mval;
			return 1;
		}
	}
	return 0;
}

/* Now let _invited_ people join thru bans, +i and +l.
 * Checking if an invite exist could be done only if a block exists,
 * but I'm not too fancy of the complicated structure that'd cause,
 * when optimization will hopefully take care of it. Most of the time
 * a user won't have invites on him anyway. -Donwulff
 */

int can_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[])
{
Link *lp;
Ban *banned;

	if ((chptr->mode.mode & MODE_ONLYSECURE) && !(sptr->umodes & UMODE_SECURE))
	{
		if (!extended_operoverride(sptr, chptr, key, MODE_ONLYSECURE, 'z'))
			return (ERR_SECUREONLYCHAN);
		else
			return 0;
	}

	if ((chptr->mode.mode & MODE_OPERONLY) && !IsAnOper(sptr))
		return (ERR_OPERONLY);

	if ((chptr->mode.mode & MODE_ADMONLY) && !IsSkoAdmin(sptr))
		return (ERR_ADMONLY);

	/* Admin, Coadmin, Netadmin, and SAdmin can still walk +b in +O */
	banned = is_banned(sptr, chptr, BANCHK_JOIN);
	if (banned && (chptr->mode.mode & MODE_OPERONLY) &&
	    IsAnOper(sptr) && !IsSkoAdmin(sptr) && !IsCoAdmin(sptr))
		return (ERR_BANNEDFROMCHAN);

	/* Only NetAdmin/SAdmin can walk +b in +A */
	if (banned && (chptr->mode.mode & MODE_ADMONLY) &&
	    IsAnOper(sptr) && !IsNetAdmin(sptr) && !IsSAdmin(sptr))
		return (ERR_BANNEDFROMCHAN);

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
			return 0;

        if ((chptr->mode.limit && chptr->users >= chptr->mode.limit))
        {
                if (chptr->mode.link)
                {
                        if (*chptr->mode.link != '\0')
                        {
                                /* We are linked. */
                                sendto_one(sptr,
                                    err_str(ERR_LINKCHANNEL), me.name,
                                    sptr->name, chptr->chname,
                                    chptr->mode.link);
                                parv[0] = sptr->name;
                                parv[1] = (chptr->mode.link);
                                do_join(cptr, sptr, 2, parv);
                                return -1;
                        }
                }
                /* We check this later return (ERR_CHANNELISFULL); */
        }

        if ((chptr->mode.mode & MODE_RGSTRONLY) && !IsARegNick(sptr))
                return (ERR_NEEDREGGEDNICK);

        if (*chptr->mode.key && (BadPtr(key) || strcmp(chptr->mode.key, key)))
                return (ERR_BADCHANNELKEY);

        if ((chptr->mode.mode & MODE_INVITEONLY))
                return (ERR_INVITEONLYCHAN);

        if ((chptr->mode.limit && chptr->users >= chptr->mode.limit))
                return (ERR_CHANNELISFULL);

        if (banned)
                return (ERR_BANNEDFROMCHAN);

#ifndef NO_OPEROVERRIDE
#ifdef OPEROVERRIDE_VERIFY
        if (IsOper(sptr) && (chptr->mode.mode & MODE_SECRET ||
            chptr->mode.mode & MODE_PRIVATE))
                return (ERR_OPERSPVERIFY);
#endif
#endif

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
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
CMD_FUNC(m_join)
{
int r;

	if (bouncedtimes)
		sendto_realops("m_join: bouncedtimes=%d??? [please report at http://bugs.unrealircd.org/]", bouncedtimes);
	bouncedtimes = 0;
	if (IsServer(sptr))
		return 0;
	r = do_join(cptr, sptr, parc, parv);
	bouncedtimes = 0;
	return r;
}

/* Routine that actually makes a user join the channel
 * this does no actual checking (banned, etc.) it just adds the user
 */
void join_channel(aChannel *chptr, aClient *cptr, aClient *sptr, int flags)
{
	char *parv[] = { 0, 0 };
	/*
	   **  Complete user entry to the new channel (if any)
	 */
	add_user_to_channel(chptr, sptr, flags);
	/*
	   ** notify all other users on the new channel
	 */
	if (chptr->mode.mode & MODE_AUDITORIUM)
	{
		if (MyClient(sptr))
			sendto_one(sptr, ":%s!%s@%s JOIN :%s",
			    sptr->name, sptr->user->username,
			    GetHost(sptr), chptr->chname);
		sendto_chanops_butone(NULL, chptr, ":%s!%s@%s JOIN :%s",
		    sptr->name, sptr->user->username,
		    GetHost(sptr), chptr->chname);
	}
	else
		sendto_channel_butserv(chptr, sptr,
		    ":%s JOIN :%s", sptr->name, chptr->chname);
	
	sendto_serv_butone_token_opt(cptr, OPT_NOT_SJ3, sptr->name, MSG_JOIN,
		    TOK_JOIN, "%s", chptr->chname);

#ifdef JOIN_INSTEAD_OF_SJOIN_ON_REMOTEJOIN
	if ((MyClient(sptr) && !(flags & CHFL_CHANOP)) || !MyClient(sptr))
		sendto_serv_butone_token_opt(cptr, OPT_SJ3, sptr->name, MSG_JOIN,
		    TOK_JOIN, "%s", chptr->chname);
	if (flags & CHFL_CHANOP)
	{
#endif
		/* I _know_ that the "@%s " look a bit wierd
		   with the space and all .. but its to get around
		   a SJOIN bug --stskeeps */
		sendto_serv_butone_token_opt(cptr, OPT_SJ3|OPT_SJB64,
			me.name, MSG_SJOIN, TOK_SJOIN,
			"%B %s :%s%s ", chptr->creationtime, 
			chptr->chname, flags & CHFL_CHANOP ? "@" : "", sptr->name);
		sendto_serv_butone_token_opt(cptr, OPT_SJ3|OPT_NOT_SJB64,
			me.name, MSG_SJOIN, TOK_SJOIN,
			"%li %s :%s%s ", chptr->creationtime, 
			chptr->chname, flags & CHFL_CHANOP ? "@" : "", sptr->name);
#ifdef JOIN_INSTEAD_OF_SJOIN_ON_REMOTEJOIN
	}
#endif		

	if (MyClient(sptr))
	{
		/*
		   ** Make a (temporal) creationtime, if someone joins
		   ** during a net.reconnect : between remote join and
		   ** the mode with TS. --Run
		 */
		if (chptr->creationtime == 0)
		{
			chptr->creationtime = TStime();
			sendto_serv_butone_token(cptr, me.name,
			    MSG_MODE, TOK_MODE, "%s + %lu",
			    chptr->chname, chptr->creationtime);
		}
		del_invite(sptr, chptr);
		if (flags & CHFL_CHANOP)
			sendto_serv_butone_token_opt(cptr, OPT_NOT_SJ3, 
			    me.name,
			    MSG_MODE, TOK_MODE, "%s +o %s %lu",
			    chptr->chname, sptr->name,
			    chptr->creationtime);
		if (chptr->topic)
		{
			sendto_one(sptr, rpl_str(RPL_TOPIC),
			    me.name, sptr->name, chptr->chname, chptr->topic);
			sendto_one(sptr,
			    rpl_str(RPL_TOPICWHOTIME), me.name,
			    sptr->name, chptr->chname, chptr->topic_nick,
			    chptr->topic_time);
		}
		if (chptr->users == 1 && (MODES_ON_JOIN
#ifdef EXTCMODE
		    || iConf.modes_on_join.extmodes)
#endif
		)
		{
#ifdef EXTCMODE
			int i;
			chptr->mode.extmode =  iConf.modes_on_join.extmodes;
			/* Param fun */
			for (i = 0; i <= Channelmode_highest; i++)
			{
				if (!Channelmode_Table[i].flag || !Channelmode_Table[i].paracount)
					continue;
				if (chptr->mode.extmode & Channelmode_Table[i].mode)
				{
					CmodeParam *p;
					p = Channelmode_Table[i].put_param(NULL, iConf.modes_on_join.extparams[i]);
					AddListItem(p, chptr->mode.extmodeparam);
				}
			}
#endif
			chptr->mode.mode = MODES_ON_JOIN;
#ifdef NEWCHFLOODPROT
			if (iConf.modes_on_join.floodprot.per)
			{
				chptr->mode.floodprot = MyMalloc(sizeof(ChanFloodProt));
				memcpy(chptr->mode.floodprot, &iConf.modes_on_join.floodprot, sizeof(ChanFloodProt));
			}
#else
			chptr->mode.kmode = iConf.modes_on_join.kmode;
			chptr->mode.per = iConf.modes_on_join.per;
			chptr->mode.msgs = iConf.modes_on_join.msgs;
#endif
			*modebuf = *parabuf = 0;
			channel_modes(sptr, modebuf, parabuf, chptr);
			/* This should probably be in the SJOIN stuff */
			sendto_serv_butone_token(&me, me.name, MSG_MODE, TOK_MODE, 
				"%s %s %s %lu", chptr->chname, modebuf, parabuf, 
				chptr->creationtime);
			sendto_one(sptr, ":%s MODE %s %s %s", me.name, chptr->chname, modebuf, parabuf);
		}
		parv[0] = sptr->name;
		parv[1] = chptr->chname;
		(void)m_names(cptr, sptr, 2, parv);
		RunHook4(HOOKTYPE_LOCAL_JOIN, cptr, sptr,chptr,parv);
	} else {
		RunHook4(HOOKTYPE_REMOTE_JOIN, cptr, sptr, chptr, parv); /* (rarely used) */
	}

#ifdef NEWCHFLOODPROT
	/* I'll explain this only once:
	 * 1. if channel is +f
	 * 2. local client OR synced server
	 * 3. then, increase floodcounter
	 * 4. if we reached the limit AND only if source was a local client.. do the action (+i).
	 * Nr 4 is done because otherwise you would have a noticeflood with 'joinflood detected'
	 * from all servers.
	 */
	if (chptr->mode.floodprot && (MyClient(sptr) || sptr->srvptr->serv->flags.synced) && 
	    !IsULine(sptr) && do_chanflood(chptr->mode.floodprot, FLD_JOIN) && MyClient(sptr))
	{
		do_chanflood_action(chptr, FLD_JOIN, "join");
	}
#endif
}

/** User request to join a channel.
 * This routine can be called from both m_join or via do_join->can_join->do_join
 * if the channel is 'linked' (chmode +L). We use a counter 'bouncedtimes' which
 * is set to 0 in m_join, increased every time we enter this loop and decreased
 * anytime we leave the loop. So be carefull ;p.
 */
CMD_FUNC(do_join)
{
	char jbuf[BUFSIZE];
	Membership *lp;
	aChannel *chptr;
	char *name, *key = NULL, *link = NULL;
	int  i, flags = 0;
	char *p = NULL, *p2 = NULL;

#define RET(x) { bouncedtimes--; return x; }

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "JOIN");
		return 0;
	}
	bouncedtimes++;
	/* don't use 'return x;' but 'RET(x)' from here ;p */

	if (bouncedtimes > MAXBOUNCE)
	{
		/* bounced too many times */
		sendto_one(sptr,
		    ":%s %s %s :*** Couldn't join %s ! - Link setting was too bouncy",
		    me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, parv[1]);
		RET(0)
	}

	*jbuf = '\0';
	/*
	   ** Rebuild list of channels joined to be the actual result of the
	   ** JOIN.  Note that "JOIN 0" is the destructive problem.
	 */
	for (i = 0, name = strtoken(&p, parv[1], ","); name;
	    name = strtoken(&p, NULL, ","))
	{
		/* pathological case only on longest channel name.
		   ** If not dealt with here, causes desynced channel ops
		   ** since ChannelExists() doesn't see the same channel
		   ** as one being joined. cute bug. Oct 11 1997, Dianora/comstud
		   ** Copied from Dianora's "hybrid 5" ircd.
		 */

		if (strlen(name) > CHANNELLEN)	/* same thing is done in get_channel() */
			name[CHANNELLEN] = '\0';

		if (MyConnect(sptr))
			clean_channelname(name);
		if (check_channelmask(sptr, cptr, name) == -1)
			continue;
		if (*name == '0' && !atoi(name))
		{
			(void)strcpy(jbuf, "0");
			i = 1;
			continue;
		}
		else if (!IsChannelName(name))
		{
			if (MyClient(sptr))
				sendto_one(sptr,
				    err_str(ERR_NOSUCHCHANNEL), me.name,
				    parv[0], name);
			continue;
		}
		if (*jbuf)
			(void)strlcat(jbuf, ",", sizeof jbuf);
		(void)strlncat(jbuf, name, sizeof jbuf, sizeof(jbuf) - i - 1);
		i += strlen(name) + 1;
	}
	/* This strcpy should be safe since jbuf contains the "filtered"
	 * result of parv[1] which should never be larger than the source.
	 */
	(void)strcpy(parv[1], jbuf);

	p = NULL;
	if (parv[2])
		key = strtoken(&p2, parv[2], ",");
	parv[2] = NULL;		/* for m_names call later, parv[parc] must == NULL */
	for (name = strtoken(&p, jbuf, ","); name;
	    key = (key) ? strtoken(&p2, NULL, ",") : NULL,
	    name = strtoken(&p, NULL, ","))
	{
		/*
		   ** JOIN 0 sends out a part for all channels a user
		   ** has joined.
		 */
		if (*name == '0' && !atoi(name))
		{
			while ((lp = sptr->user->channel))
			{
				chptr = lp->chptr;
				sendto_channel_butserv(chptr, sptr,
				    PartFmt2, parv[0], chptr->chname,
				    "Left all channels");
				if (MyConnect(sptr))
					RunHook4(HOOKTYPE_LOCAL_PART, cptr, sptr, chptr, "Left all channels");
				remove_user_from_channel(sptr, chptr);
			}
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_JOIN, TOK_JOIN, "0");
			continue;
		}

		if (MyConnect(sptr))
		{
			/*
			   ** local client is first to enter previously nonexistant
			   ** channel so make them (rightfully) the Channel
			   ** Operator.
			 */
			/* Where did this come from? Potvin ? --Stskeeps
			   flags = (ChannelExists(name)) ? CHFL_DEOPPED :
			   CHFL_CHANOWNER;

			 */

			flags =
			    (ChannelExists(name)) ? CHFL_DEOPPED : CHFL_CHANOP;

			if (!IsAnOper(sptr))	/* opers can join unlimited chans */
				if (sptr->user->joined >= MAXCHANNELSPERUSER)
				{
					sendto_one(sptr,
					    err_str
					    (ERR_TOOMANYCHANNELS),
					    me.name, parv[0], name);
					RET(0)
				}
/* RESTRICTCHAN */
			if (conf_deny_channel)
			{
				if (!IsOper(sptr) && !IsULine(sptr))
				{
					ConfigItem_deny_channel *d;
					if ((d = Find_channel_allowed(name)))
					{
						if (d->warn)
						{
							sendto_snomask(SNO_EYES, "*** %s tried to join forbidden channel %s",
								get_client_name(sptr, 1), name);
						}
						if (d->reason)
							sendto_one(sptr, 
							":%s %s %s :*** Can not join %s: %s",
							me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, name, d->reason);
						if (d->redirect)
						{
							sendto_one(sptr,
							":%s %s %s :*** Redirecting you to %s",
							me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE", sptr->name, d->redirect);
							parv[0] = sptr->name;
							parv[1] = d->redirect;
							do_join(cptr, sptr, 2, parv);
						}
						continue;
					}
				}
			}
			/* ugly set::spamfilter::virus-help-channel-deny hack.. */
			if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
			    !strcasecmp(name, SPAMFILTER_VIRUSCHAN) &&
			    !IsAnOper(sptr) && !spamf_ugly_vchanoverride)
			{
				int invited = 0;
				Link *lp;
				aChannel *chptr = find_channel(name, NULL);
				
				if (chptr)
				{
					for (lp = sptr->user->invited; lp; lp = lp->next)
						if (lp->value.chptr == chptr)
							invited = 1;
				}
				if (!invited)
				{
					sendnotice(sptr, "*** Cannot join '%s' because it's the virus-help-channel which is "
					                 "reserved for infected users only", name);
					continue;
				}
			}
		}

		chptr = get_channel(sptr, name, CREATE);
		if (chptr && (lp = find_membership_link(sptr->user->channel, chptr)))
			continue;

		if (!chptr)
			continue;

		i = HOOK_CONTINUE;
		if (!MyConnect(sptr))
			flags = CHFL_DEOPPED;
		else
		{
			Hook *h;
			for (h = Hooks[HOOKTYPE_PRE_LOCAL_JOIN]; h; h = h->next) 
			{
				i = (*(h->func.intfunc))(sptr,chptr,parv);
				if (i == HOOK_DENY || i == HOOK_ALLOW)
					break;
			}
			/* Denied, get out now! */
			if (i == HOOK_DENY)
			{
				/* Rejected... if we just created a new chan we should destroy it too. -- Syzop */
				if (!chptr->users)
					sub1_from_channel(chptr);
				continue;
			}
			/* If they are allowed, don't check can_join */
			if (i != HOOK_ALLOW && 
			   (i = can_join(cptr, sptr, chptr, key, link, parv)))
			{
				if (i != -1)
					sendto_one(sptr, err_str(i),
					    me.name, parv[0], name);
				continue;
			}
		}

		join_channel(chptr, cptr, sptr, flags);
	}
	RET(0)
#undef RET
}


/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
CMD_FUNC(m_part)
{
	aChannel *chptr;
	Membership *lp;
	char *p = NULL, *name;
	char *commentx = (parc > 2 && parv[2]) ? parv[2] : NULL;
	char *comment;
	int n;
	
	if (parc < 2 || parv[1][0] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PART");
		return 0;
	}

	if (MyClient(sptr))
	{
		if (IsShunned(sptr))
			commentx = NULL;
		if (STATIC_PART)
		{
			if (!strcasecmp(STATIC_PART, "yes") || !strcmp(STATIC_PART, "1"))
				commentx = NULL;
			else
				commentx = STATIC_PART;
		}
		if (commentx)
		{
			n = dospamfilter(sptr, commentx, SPAMF_PART, parv[1]);
			if (n == FLUSH_BUFFER)
				return n;
			if (n < 0)
				commentx = NULL;
		}
	}

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
			    me.name, parv[0], name);
			continue;
		}
		if (check_channelmask(sptr, cptr, name))
			continue;

		/* 'commentx' is the general part msg, but it can be changed
		 * per-channel (eg some chans block badwords, strip colors, etc)
		 * so we copy it to 'comment' and use that in this for loop :)
		 */
		comment = commentx;

		if (!(lp = find_membership_link(sptr->user->channel, chptr)))
		{
			/* Normal to get get when our client did a kick
			   ** for a remote client (who sends back a PART),
			   ** so check for remote client or not --Run
			 */
			if (MyClient(sptr))
				sendto_one(sptr,
				    err_str(ERR_NOTONCHANNEL), me.name,
				    parv[0], name);
			continue;
		}

		if (!IsAnOper(sptr) && !is_chanownprotop(sptr, chptr)) {
#ifdef STRIPBADWORDS
			int blocked = 0;
#endif
			if ((chptr->mode.mode & MODE_NOCOLOR) && comment) {
				if (strchr((char *)comment, 3) || strchr((char *)comment, 27)) {
					comment = NULL;
				}
			}
			if ((chptr->mode.mode & MODE_MODERATED) && comment &&
				 !has_voice(sptr, chptr) && !is_halfop(sptr, chptr))
			{
				comment = NULL;
			}
			if ((chptr->mode.mode & MODE_STRIP) && comment) {
				comment = (char *)StripColors(comment);
			}
#ifdef STRIPBADWORDS
 #ifdef STRIPBADWORDS_CHAN_ALWAYS
			if (comment)
			{
				comment = (char *)stripbadwords_channel(comment, &blocked);
			}
 #else
			if ((chptr->mode.mode & MODE_STRIPBADWORDS) && comment) {
				comment = (char *)stripbadwords_channel(comment, &blocked);
			}
 #endif
#endif
			
		}
		/* +M and not +r? */
		if ((chptr->mode.mode & MODE_MODREG) && !IsRegNick(sptr) && !IsAnOper(sptr))
			comment = NULL;

		if (MyConnect(sptr))
		{
			Hook *tmphook;
			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_PART]; tmphook; tmphook = tmphook->next) {
				comment = (*(tmphook->func.pcharfunc))(sptr, chptr, comment);
				if (!comment)
					break;
			}
		}

		/* Send to other servers... */
		if (!comment)
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s", chptr->chname);
		else
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s :%s", chptr->chname,
			    comment);

		if (1)
		{
			if ((chptr->mode.mode & MODE_AUDITORIUM) && !is_chanownprotop(sptr, chptr))
			{
				if (!comment)
				{
					sendto_chanops_butone(NULL,
					    chptr, ":%s!%s@%s PART %s",
					    sptr->name, sptr->user->username, GetHost(sptr),
					    chptr->chname);
					if (!is_chan_op(sptr, chptr) && MyClient(sptr))
						sendto_one(sptr, ":%s!%s@%s PART %s",
						    sptr->name, sptr->user->username, GetHost(sptr), chptr->chname);
				}
				else
				{
					sendto_chanops_butone(NULL,
					    chptr,
					    ":%s!%s@%s PART %s %s",
					    sptr->name,
					    sptr->user->username,
					    GetHost(sptr),
					    chptr->chname, comment);
					if (!is_chan_op(cptr, chptr) && MyClient(sptr))
						sendto_one(sptr,
						    ":%s!%s@%s PART %s %s",
						    sptr->name, sptr->user->username, GetHost(sptr),
						    chptr->chname, comment);
				}
			}
			else
			{


				if (!comment)

					sendto_channel_butserv(chptr,
					    sptr, PartFmt, parv[0],
					    chptr->chname);
				else
					sendto_channel_butserv(chptr,
					    sptr, PartFmt2, parv[0],
					    chptr->chname, comment);
			}
			if (MyClient(sptr))
				RunHook4(HOOKTYPE_LOCAL_PART, cptr, sptr, chptr, comment);
			else
				RunHook4(HOOKTYPE_REMOTE_PART, cptr, sptr, chptr, comment);

			remove_user_from_channel(sptr, chptr);
		}
	}
	return 0;
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
	if (is_chan_op(sptr, chptr))
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
			add_banid(&me, chptr, mask);
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

/* 
 * This will send "cptr" a full list of the modes for channel chptr,
 */


void send_channel_modes_sjoin3(aClient *cptr, aChannel *chptr)
{
	Member *members;
	Member *lp;
	Ban *ban;
	char *name;
	char *bufptr;
	short nomode, nopara;
	char bbuf[1024];
	int  n = 0;

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
		    chptr->creationtime, chptr->chname);
	}
	if (nopara && !nomode)
	{
		ircsprintf(buf, 
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s :" : "%s %ld %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    chptr->creationtime, chptr->chname, modebuf);

	}
	if (!nopara && !nomode)
	{
		ircsprintf(buf,
		    (cptr->proto & PROTO_SJB64 ? "%s %B %s %s %s :" : "%s %ld %s %s %s :"),
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    chptr->creationtime, chptr->chname, modebuf, parabuf);
	}
	strcpy(bbuf, buf);

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

			strcpy(buf, bbuf);
			n = 0;

			bufptr = buf + strlen(buf);
		}
	}
	for (ban = chptr->banlist; ban; ban = ban->next)
	{
		*bufptr++ = '&';
		strcpy(bufptr, ban->banstr);
		bufptr += strlen(bufptr);
		*bufptr++ = ' ';
		n++;
		if (bufptr - buf > BUFSIZE - 80)
		{
			*bufptr++ = '\0';
			if (bufptr[-1] == ' ')
				bufptr[-1] = '\0';
			sendto_one(cptr, "%s", buf);

			strcpy(buf, bbuf);
			n = 0;

			bufptr = buf + strlen(buf);
		}

	}
	for (ban = chptr->exlist; ban; ban = ban->next)
	{
		*bufptr++ = '"';
		strcpy(bufptr, ban->banstr);
		bufptr += strlen(bufptr);
		*bufptr++ = ' ';
		n++;
		if (bufptr - buf > BUFSIZE - 80)
		{
			*bufptr++ = '\0';
			if (bufptr[-1] == ' ')
				bufptr[-1] = '\0';
			sendto_one(cptr, "%s", buf);

			strcpy(buf, bbuf);
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
