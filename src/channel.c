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
#include "modules.h"
#include <string.h>

ID_Copyright
    ("(C) 1990 University of Oulu, Computing Center and Jarkko Oikarinen");

long opermode = 0;
aChannel *channel = NullChn;
extern char backupbuf[];
extern ircstats IRCstats;

/* Some forward declarations */
char *clean_ban_mask(char *, int, aClient *);
void channel_modes(aClient *cptr, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, aChannel *chptr);

void sub1_from_channel(aChannel *);

void clean_channelname(char *);
void del_invite(aClient *, aChannel *);

/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static char nickbuf[BUFSIZE], buf[BUFSIZE];
MODVAR char modebuf[BUFSIZE], parabuf[BUFSIZE];
#include "sjoin.h"

#define MODESYS_LINKOK		/* We do this for a TEST  */
aCtab cFlagTab[] = {
	{MODE_LIMIT, 'l', 1, 1},
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
	{MODE_CHANPROT, 'a', 0, 1},
	{MODE_CHANOWNER, 'q', 0, 1},
	{MODE_BAN, 'b', 1, 1},
	{MODE_EXCEPT, 'e', 1, 0},	/* exception ban */
	{MODE_INVEX, 'I', 1, 0},	/* exception ban */
	{0x0, 0x0, 0x0}
};


#define	BADOP_BOUNCE	1
#define	BADOP_USER	2
#define	BADOP_SERVER	3
#define	BADOP_OVERRIDE	4

char cmodestring[512];

inline int op_can_override(char* acl, aClient *sptr,aChannel *channel,void* extra)
{
#ifndef NO_OPEROVERRIDE
	if (MyClient(sptr) && !(ValidatePermissionsForPath(acl,sptr,NULL,channel,extra)))
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
	int i;
	while (tab->mode != 0x0)
	{
		*p = tab->flag;
		p++;
		tab++;
	}
	for (i=0; i <= Channelmode_highest; i++)
		if (Channelmode_Table[i].flag)
			*p++ = Channelmode_Table[i].flag;
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
			lp = (Member *)MyMallocEx(sizeof(Member));
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
	if (!lp)
		return;
	moddata_free_member(lp);
	memset(lp, 0, sizeof(Member));
	lp->next = freemember;
	lp->cptr = NULL;
	lp->flags = 0;
	freemember = lp;
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
				lp = (Membership *)MyMallocEx(sizeof(Membership));
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
		}
		else
		{
			lp2 = freemembershipL;
			freemembershipL = (MembershipL *) freemembershipL->next;
		}
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
		moddata_free_membership(lp);
		if (!local)
		{
			memset(lp, 0, sizeof(Membership));
			lp->next = freemembership;
			freemembership = lp;
		}
		else
		{
			memset(lp, 0, sizeof(Membership));
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
#ifdef SOCALLEDSMARTBANNING
			  /* Temp workaround added in b19. -- Syzop */
			  if (!mycmp(ban->banstr, banid) || (!strchr(banid, '\\') && !strchr(ban->banstr, '\\')))
				if (!match(ban->banstr, banid))
					return -1;
#endif
			  if (!mycmp(ban->banstr, banid))
			  	return -1;
			}
		else if (!mycmp(ban->banstr, banid))
			return -1;

	}
	ban = make_ban();
	bzero((char *)ban, sizeof(Ban));
	ban->next = *list;
	ban->banstr = strdup(banid);
	ban->who = strdup(cptr->name);
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

/** ban_check_mask - Checks if the user matches the specified n!u@h mask -or- run an extended ban.
 * @param sptr         Client to check (can be remote client)
 * @param chptr        Channel to check
 * @param banstr       Mask string to check user
 * @param type         Type of ban to check for (BANCHK_*)
 * @param no_extbans   0 to check extbans, nonzero to disable extban checking.
 * @returns            Nonzero if the mask/extban succeeds. Zero if it doesn't.
 * @comments           This is basically extracting the mask and extban check from is_banned_with_nick, but with being a bit more strict in what an extban is.
 *                     Strange things could happen if this is called outside standard ban checking.
 */
inline int ban_check_mask(aClient *sptr, aChannel *chptr, char *banstr, int type, int no_extbans)
{
	Extban *extban = NULL;
	if (!no_extbans && banstr[0] == '~' && banstr[1] != '\0' && banstr[2] == ':')
	{
		/* Is an extended ban. */
		extban = findmod_by_bantype(banstr[1]);
		if (!extban)
		{
			return 0;
		}
		else
		{
			return extban->is_banned(sptr, chptr, banstr, type);
		}
	}
	else
	{
		/* Is a n!u@h mask. */
		return match_user(banstr, sptr, MATCH_CHECK_ALL);
	}
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

	/* We check +b first, if a +b is found we then see if there is a +e.
	 * If a +e was found we return NULL, if not, we return the ban.
	 */
	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
	{
		if (!ban_check_mask(sptr, chptr, tmp->banstr, type, 0))
			continue;

		/* Ban found, now check for +e */
		for (tmp2 = chptr->exlist; tmp2; tmp2 = tmp2->next)
		{
			if (ban_check_mask(sptr, chptr, tmp2->banstr, type, 0))
				return NULL; /* except matched */
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
		RunHook2(HOOKTYPE_JOIN_DATA,who,chptr);
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

int  can_send(aClient *cptr, aChannel *chptr, char *msgtext, int notice)
{
	Membership *lp;
	int  member, i = 0;
	Hook *h;
	/* 
	 * #0000053 by |savage|, speedup 
	*/
	
	if (!MyClient(cptr))
	{
		return 0;
	}

	member = IsMember(cptr, chptr);

	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (CANNOT_SEND_NOPRIVMSGS);

	lp = find_membership_link(cptr->user->channel, chptr);
	if (chptr->mode.mode & MODE_MODERATED && !op_can_override("override:message:moderated",cptr,chptr,NULL) &&
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))))
	    {
			return (CANNOT_SEND_MODERATED);
	    }


	for (h = Hooks[HOOKTYPE_CAN_SEND]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(cptr,chptr,msgtext,lp,notice);
		if (i != HOOK_CONTINUE)
			break;
	}

	if (i != HOOK_CONTINUE)
		return i;

	/* Makes opers able to talk thru bans -Stskeeps suggested by The_Cat */
	if (op_can_override("override:message:ban",cptr,chptr,NULL))
		return 0;

	if ((!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))) && MyClient(cptr)
	    && is_banned(cptr, chptr, BANCHK_MSG))
		return (CANNOT_SEND_BAN);

	return 0;
}

/** Returns 1 if channel has this channel mode set and 0 if not */
int has_channel_mode(aChannel *chptr, char mode)
{
	aCtab *tab = &cFlagTab[0];
	int i;

	/* Extended channel modes */
	for (i=0; i <= Channelmode_highest; i++)
	{
		if ((Channelmode_Table[i].flag == mode) && (chptr->mode.extmode & Channelmode_Table[i].mode))
			return 1;
	}

	/* Built-in channel modes */
	while (tab->mode != 0x0)
	{
		if ((chptr->mode.mode & tab->mode) && (tab->flag == mode))
			return 1;
		tab++;
	}

	/* Special handling for +l (needed??) */
	if (chptr->mode.limit && (mode == 'l'))
		return 1;

	/* Special handling for +k (needed??) */
	if (chptr->mode.key[0] && (mode == 'k'))
		return 1;
		
	return 0; /* Not found */
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
/* TODO: this function has many security issues and needs an audit, maybe even a recode */
void channel_modes(aClient *cptr, char *mbuf, char *pbuf, size_t mbuf_size, size_t pbuf_size, aChannel *chptr)
{
	aCtab *tab = &cFlagTab[0];
	char bcbuf[1024];
	int ismember;
	int i;

	if (!(mbuf_size && pbuf_size)) return;

	ismember = (IsMember(cptr, chptr) || IsServer(cptr) || IsULine(cptr)) ? 1 : 0;

	*pbuf = '\0';

	*mbuf++ = '+';
	mbuf_size--;
	/* Paramless first */
	while (mbuf_size && tab->mode != 0x0)
	{
		if ((chptr->mode.mode & tab->mode))
			if (!tab->parameters) {
				*mbuf++ = tab->flag;
				mbuf_size--;
			}
		tab++;
	}
	for (i=0; i <= Channelmode_highest; i++)
	{
		if (!mbuf_size) break;
		if (Channelmode_Table[i].flag && !Channelmode_Table[i].paracount &&
		    (chptr->mode.extmode & Channelmode_Table[i].mode)) {
			*mbuf++ = Channelmode_Table[i].flag;
			mbuf_size--;
		}
	}
	if (chptr->mode.limit)
	{
		if (mbuf_size) {
			*mbuf++ = 'l';
			mbuf_size--;
		}
		if (ismember) {
			ircsnprintf(pbuf, pbuf_size, "%d ", chptr->mode.limit);
			pbuf_size-=strlen(pbuf);
			pbuf+=strlen(pbuf);
		}
	}
	if (*chptr->mode.key)
	{
		if (mbuf_size) {
			*mbuf++ = 'k';
			mbuf_size--;
		}
		if (ismember && pbuf_size) {
			ircsnprintf(pbuf, pbuf_size, "%s ", chptr->mode.key);
			pbuf_size-=strlen(pbuf);
			pbuf+=strlen(pbuf);
		}
	}

	for (i=0; i <= Channelmode_highest; i++)
	{
		if (Channelmode_Table[i].flag && Channelmode_Table[i].paracount &&
		    (chptr->mode.extmode & Channelmode_Table[i].mode)) {
		        char flag = Channelmode_Table[i].flag;
			if (mbuf_size) {
				*mbuf++ = flag;
				mbuf_size--;
			}
			if (ismember)
			{
				ircsnprintf(pbuf, pbuf_size, "%s ", cm_getparameter(chptr, flag));
				pbuf_size-=strlen(pbuf);
				pbuf+=strlen(pbuf);
			}
		}
	}

	/* Remove the trailing space from the parameters -- codemastr */
	if (*pbuf) pbuf[strlen(pbuf)-1]=0;

	if (!mbuf_size) mbuf--;
	*mbuf++ = '\0';
	return;
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
		sendto_one(cptr, ":%s MODE %s %s %s %lu", from,
		    name, mode, param, creationtime);
	else
		sendto_one(cptr, ":%s MODE %s %s %s", from, name, mode, param);

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
	char *cp, *x;
	char *user;
	char *host;
	Extban *p;

	cp = index(mask, ' ');
	if (cp)
		*cp = '\0';

	/* Strip any ':' at beginning since that would cause a desynch */
	for (; (*mask && (*mask == ':')); mask++);
	if (!*mask)
		return NULL;

	/* Forbid ASCII <= 32 in all bans */
	for (x = mask; *x; x++)
		if (*x <= ' ')
			return NULL;

	/* Extended ban? */
	if ((*mask == '~') && mask[1] && (mask[2] == ':'))
	{
		if (RESTRICT_EXTENDEDBANS && MyClient(cptr) && !ValidatePermissionsForPath("channel:extbans",cptr,NULL,NULL,NULL))
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
		if ((mask[1] != ':') || (mask[2] == '\0'))
		    return NULL; /* require a ":<char>" after extban type */
		if (strlen(mask) > 80)
			mask[80] = '\0';
		return mask;
	}

	if ((*mask == '~') && !strchr(mask, '@'))
		return NULL; /* not an extended ban and not a ~user@host ban either. */

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

	for (inv = chptr->invexlist; inv; inv = inv->next)
		if (ban_check_mask(sptr, chptr, inv->banstr, BANCHK_JOIN, 0))
			return 1;

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
		if (*ch < 33 || *ch == ',' || *ch == ':' || *ch == 160)
		{
			*ch = '\0';
			return;
		}
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
		chptr = MyMallocEx(sizeof(aChannel) + len);
		strlcpy(chptr->chname, chname, len + 1);
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

void add_invite(aClient *from, aClient *to, aChannel *chptr)
{
	Link *inv, *tmp;

	del_invite(to, chptr);
	/*
	 * delete last link in chain if the list is max length
	 */
	if (list_length(to->user->invited) >= MAXCHANNELSPERUSER)
	{
		for (tmp = to->user->invited; tmp->next; tmp = tmp->next)
			;
		del_invite(to, tmp->value.chptr);

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
	inv->value.cptr = to;
	inv->next = chptr->invites;
	chptr->invites = inv;
	/*
	 * add channel to the beginning of the client invite list
	 */
	inv = make_link();
	inv->value.chptr = chptr;
	inv->next = to->user->invited;
	to->user->invited = inv;

	RunHook3(HOOKTYPE_INVITE, from, to, chptr);
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
	int should_destroy = 1;

	--chptr->users;
	if (chptr->users <= 0)
	{
		chptr->users = 0;

		/*
		 * Now, find all invite links from channel structure
		 */
		RunHook2(HOOKTYPE_CHANNEL_DESTROY, chptr, &should_destroy);
		if (!should_destroy)
			return;

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

		/* free extcmode params */
		extcmode_free_paramlist(chptr->mode.extmodeparams);

		if (chptr->mode_lock)
			MyFree(chptr->mode_lock);
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

void send_user_joins(aClient *cptr, aClient *user)
{
	Membership *lp;
	aChannel *chptr;
	int  cnt = 0, len = 0, clen;
	char *mask;

	snprintf(buf, sizeof buf, ":%s JOIN ", user->name);
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
			snprintf(buf, sizeof buf, ":%s JOIN ", user->name);
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

/*
 * rejoin_leave:
 * sends a PART to all channels (to local users only)
 * TODO: use QUIT instead of PART if configured to do so
 */
void rejoin_leave(aClient *sptr)
{
	Membership *tmp;
	aChannel *chptr;
	Hook *h;
	char *comment = "Changing host";
	int i = 0;

	for (tmp = sptr->user->channel; tmp; tmp = tmp->next)
	{
                tmp->flags &= ~CHFL_REJOINING;

		chptr = tmp->chptr;
		if (!chptr)
			continue; /* Possible? */
                
		/* If the user is banned, don't do it */
		if (is_banned(sptr, chptr, BANCHK_JOIN))
			continue;
		
		/* Ok, we will now part/quit/whatever the user, so tag it.. */
		tmp->flags |= CHFL_REJOINING;


		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
			{
				i = (*(h->func.intfunc))(sptr,chptr);
				if (i != 0)
					break;
			}

		if ((i != 0) &&
		    !(tmp->flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP|CHFL_HALFOP|CHFL_VOICE)))
		{
			sendto_chanops_butone(sptr, chptr, ":%s!%s@%s PART %s :%s", sptr->name, sptr->user->username, GetHost(sptr), chptr->chname, comment);
		} else
			sendto_channel_butserv_butone(chptr, sptr, sptr, ":%s PART %s :%s", sptr->name, chptr->chname, comment);
	}
}

/*
 * rejoin_joinandmode:
 * sends a JOIN and a MODE (if needed) to restore qaohv modes (to local users only)
 */
void rejoin_joinandmode(aClient *sptr)
{
	Membership *tmp;
	aChannel *chptr;
	int i, j = 0, n, flags;
	Hook *h;
	int k = 0;
	char flagbuf[8]; /* For holding "qohva" and "*~@%+" */

	for (tmp = sptr->user->channel; tmp; tmp = tmp->next)
	{
		flags = tmp->flags;
		chptr = tmp->chptr;
		if (!chptr)
			continue; /* Is it possible? */

		/* If the user is banned, don't do it */
		if (!(flags & CHFL_REJOINING))
			continue;

		for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			k = (*(h->func.intfunc))(sptr,chptr);
			if (k != 0)
				break;
		}


		if ((k != 0) &&
		    !(flags & (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP|CHFL_HALFOP|CHFL_VOICE)))
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
		
		tmp->flags &= ~CHFL_REJOINING; /* esthetics.. ;) */
	}
}

/* set_channel_mlock()
 *
 * inputs	- client, source, channel, params
 * output	- 
 * side effects - channel mlock is changed / MLOCK is propagated
 */
void set_channel_mlock(aClient *cptr, aClient *sptr, aChannel *chptr, const char *newmlock, int propagate)
{
	if (chptr->mode_lock)
		MyFree(chptr->mode_lock);
	chptr->mode_lock = (newmlock != NULL) ? strdup(newmlock) : NULL;

	if (propagate)
	{
		sendto_server(cptr,0, 0, ":%s MLOCK %lu %s :%s",
			      cptr->name, chptr->creationtime, chptr->chname,
			      BadPtr(chptr->mode_lock) ? "" : chptr->mode_lock);
	}
}

/** Parse a channelmode line.
 * @in pm A ParseMode struct, used to return values and to maintain internal state.
 * @in modebuf_in Buffer pointing to mode characters (eg: +snk-l)
 * @in parabuf_in Buffer pointing to all parameters (eg: key 123)
 * @retval Returns 1 if we have valid data to return, 0 if at end of mode line.
 * @example
 *
 * ParseMode pm;
 * int ret;
 * for (ret = parse_chanmode(&pm, modebuf, parabuf); ret; ret = parse_chanmode(&pm, NULL, NULL))
 * {
 *         ircd_log(LOG_ERROR, "Got %c%c %s",
 *                  pm.what == MODE_ADD ? '+' : '-',
 *                  pm.modechar,
 *                  pm.param ? pm.param : "");
 * }
 */
int parse_chanmode(ParseMode *pm, char *modebuf_in, char *parabuf_in)
{
	if (modebuf_in)
	{
		/* Initialize */
		memset(pm, 0, sizeof(ParseMode));
		pm->modebuf = modebuf_in;
		pm->parabuf = parabuf_in;
		pm->what = MODE_ADD;
	}

	while(1)
	{
		if (*pm->modebuf == '\0')
			return 0;
		else if (*pm->modebuf == '+')
		{
			pm->what = MODE_ADD;
			pm->modebuf++;
			continue;
		}
		else if (*pm->modebuf == '-')
		{
			pm->what = MODE_DEL;
			pm->modebuf++;
			continue;
		}
		else
		{
			aCtab *tab = &cFlagTab[0];
			int i;
			int eatparam = 0;

			/* Set some defaults */
			pm->extm = NULL;
			pm->modechar = *pm->modebuf;
			pm->param = NULL;

			while (tab->mode != 0x0)
			{
				if (tab->flag == *pm->modebuf)
					break;
				tab++;
			}

			if (tab->mode)
			{
				/* INTERNAL MODE */
				if (tab->parameters)
				{
					if ((pm->what == MODE_DEL) && (tab->flag == 'l'))
						eatparam = 0; /* -l is special: no parameter required */
					else
						eatparam = 1; /* all other internal parameter modes do require a parameter on unset */
				}
			} else {
				/* EXTENDED CHANNEL MODE */
				int found = 0;
				for (i=0; i <= Channelmode_highest; i++)
					if (Channelmode_Table[i].flag == *pm->modebuf)
					{
						found = 1;
						break;
					}
				if (!found)
				{
					/* Not found. Will be ignored, just move on.. */
					pm->modebuf++;
					continue;
				}
				pm->extm = &Channelmode_Table[i];
				if (Channelmode_Table[i].paracount == 1)
				{
					if (pm->what == MODE_ADD)
						eatparam = 1;
					else if (Channelmode_Table[i].unset_with_param)
						eatparam = 1;
					/* else 0 (if MODE_DEL && !unset_with_param) */
				}
			}

			if (eatparam)
			{
				/* Hungry.. */
				if (pm->parabuf && *pm->parabuf)
				{
					char *start, *end;
					for (; *pm->parabuf == ' '; pm->parabuf++); /* skip whitespace */
					start = pm->parabuf;
					if (*pm->parabuf == '\0')
					{
						pm->modebuf++;
						continue; /* invalid, got mode but no parameter available */
					}
					end = strchr(start, ' ');
					/* copy start .. end (where end may be null, then just copy all) */
					if (end)
					{
						pm->parabuf = end + 1; /* point to next param, or \0 */
						if (end - start + 1 > sizeof(pm->buf))
							end = start + sizeof(pm->buf); /* 'never' reached */
						strlcpy(pm->buf, start, end - start + 1);
					}
					else
					{
						strlcpy(pm->buf, start, sizeof(pm->buf));
						pm->parabuf = pm->parabuf + strlen(pm->parabuf); /* point to \0 at end */
					}
					pm->param = pm->buf;
				} else {
					pm->modebuf++;
					continue; /* invalid, got mode but no parameter available */
				}
			}
		}
		pm->modebuf++; /* advance pointer */
		return 1;
	}
}
