/************************************************************************
 *   Unreal Internet Relay Chat Daemon, src/channel.c
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

ID_CVS("$Id$");
ID_Copyright
    ("(C) 1990 University of Oulu, Computing Center and Jarkko Oikarinen");

#define MAXBOUNCE	5 /** Most sensible */

typedef struct ChLink CHLINK;
struct CHLink {
	aChannel *chan;
};

static int bouncedtimes = 0;

struct CHLink chlbounce[MAXBOUNCE];
int  chbounce = 0;
static long opermode = 0;
aChannel *channel = NullChn;
extern char backupbuf[];
extern aCRline *crlines;
extern char *cannotjoin_msg;
extern ircstats IRCstats;

#ifndef NO_FDLIST
extern int lifesux;
#endif

static void add_invite PROTO((aClient *, aChannel *));
static int add_banid PROTO((aClient *, aChannel *, char *));
static int can_join PROTO((aClient *, aClient *, aChannel *, char *, char *,
    char **));
static int channel_link PROTO((aClient *, aClient *, int, char **));
static void channel_modes PROTO((aClient *, char *, char *, aChannel *));
static int check_channelmask PROTO((aClient *, aClient *, char *));
static int del_banid PROTO((aChannel *, char *));
static int find_banid PROTO((aChannel *, char *));
static void set_mode PROTO((aChannel *, aClient *, int, char **, u_int *,
    char[MAXMODEPARAMS][MODEBUFLEN + 3], int));
static void make_mode_str PROTO((aChannel *, long, long, int,
    char[MAXMODEPARAMS][MODEBUFLEN + 3], char *, char *, char));
static int do_mode_char PROTO((aChannel *, long, char, char *, u_int, aClient *,
    u_int *, char[MAXMODEPARAMS][MODEBUFLEN + 3], char));
static void do_mode PROTO((aChannel *, aClient *, aClient *, int, char **, int,
    int));
static void bounce_mode PROTO((aChannel *, aClient *, int, char **));

static void sub1_from_channel PROTO((aChannel *));

void clean_channelname PROTO((char *));
void del_invite PROTO((aClient *, aChannel *));

static char *PartFmt = ":%s PART %s";
static char *PartFmt2 = ":%s PART %s :%s";
/*
 * some buffers for rebuilding channel/nick lists with ,'s
 */
static char nickbuf[BUFSIZE], buf[BUFSIZE];
static char modebuf[MODEBUFLEN], parabuf[MODEBUFLEN];
#include "sjoin.h"

#ifdef USE_LONGMODE
typedef struct {
	long mode;
	char flag;
	int  halfop;		/* 1 = yes 0 = no */
} aCtab;

// typedef struct CFlagTab aCtab;
#define MODESYS_LINKOK		/* We do this for a TEST  */
aCtab cFlagTab[] = {
	{MODE_LIMIT, 'l', 0},
	{MODE_VOICE, 'v', 1},
	{MODE_HALFOP, 'h', 0},
	{MODE_CHANOP, 'o', 0},
	{MODE_PRIVATE, 'p', 0},
	{MODE_SECRET, 's', 0},
	{MODE_MODERATED, 'm', 1},
	{MODE_NOPRIVMSGS, 'n', 1},
	{MODE_TOPICLIMIT, 't', 1},
	{MODE_INVITEONLY, 'i', 1},
	{MODE_KEY, 'k', 1},
	{MODE_RGSTR, 'r', 0},
	{MODE_RGSTRONLY, 'R', 0},
	{MODE_NOCOLOR, 'c', 0},
	{MODE_CHANPROT, 'a', 0},
	{MODE_CHANOWNER, 'q', 0},
	{MODE_OPERONLY, 'O', 0},
	{MODE_ADMONLY, 'A', 0},
	{MODE_LINK, 'L', 0},
	{MODE_NOKICKS, 'Q', 0},
	{MODE_BAN, 'b', 1},
	{MODE_STRIP, 'S', 0},	/* works? */
	{MODE_EXCEPT, 'e', 1},	/* exception ban */
	{MODE_NOKNOCK, 'K', 0},	/* knock knock (no way!) */
	{MODE_NOINVITE, 'V', 0},	/* no invites */
	{MODE_FLOODLIMIT, 'f', 0},	/* flood limiter */
	{MODE_NOHIDING, 'H', 0},	/* no +I joiners */
#ifdef STRIPBADWORDS
	{MODE_STRIPBADWORDS, 'G', 0},	/* no badwords */
#endif
	{MODE_NOCTCP, 'C', 0},	/* no CTCPs */
	{MODE_AUDITORIUM, 'u', 0},
	{0x0, 0x0, 0x0}
};
#endif


#define	BADOP_BOUNCE	1
#define	BADOP_USER	2
#define	BADOP_SERVER	3
#define	BADOP_OVERRIDE	4

/* is some kind of admin */
#define IsSkoAdmin(sptr) (IsAdmin(sptr) || IsNetAdmin(sptr) || IsTechAdmin(sptr) || IsSAdmin(sptr))

char cmodestring[512];

void make_cmodestr(void)
{
	char *p = &cmodestring[0];
	aCtab *tab = &cFlagTab[0];

	while (tab->mode != 0x0)
	{
		*p = tab->flag;
		p++;
		tab++;
	}
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
static int list_length(lp)
	Link *lp;
{
	int  count = 0;

	for (; lp; lp = lp->next)
		count++;
	return count;
}

/*
** find_chasing
**	Find the client structure for a nick name (user) using history
**	mechanism if necessary. If the client is not found, an error
**	message (NO SUCH NICK) is generated. If the client was found
**	through the history, chasing will be 1 and otherwise 0.
*/
static aClient *find_chasing(sptr, user, chasing)
	aClient *sptr;
	char *user;
	int *chasing;
{
	aClient *who = find_client(user, (aClient *)NULL);

	if (chasing)
		*chasing = 0;
	if (who)
		return who;
	if (!(who = get_history(user, (long)KILLCHASETIMELIMIT)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
		    me.name, sptr->name, user);
		return NULL;
	}
	if (chasing)
		*chasing = 1;
	return who;
}
/*
  Exception functions to work with mode +e
   -sts


/* add_exbanid - add an id to be excepted to the channel bans  (belongs to cptr) */

static int add_exbanid(cptr, chptr, banid)
	aClient *cptr;
	aChannel *chptr;
	char *banid;
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
				if (!match(ban->banstr, banid) ||
				    !match(banid, ban->banstr))
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
static int del_exbanid(chptr, banid)
	aChannel *chptr;
	char *banid;
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
 * find_banid - Find an exact match for a exban
 */
static int find_exbanid(chptr, banid)
	aChannel *chptr;
	char *banid;
{
	Ban **ban;

	if (!banid)
		return -1;
	for (ban = &(chptr->exlist); *ban; ban = &((*ban)->next))
		if (!mycmp(banid, (*ban)->banstr))
			return 1;
	return 0;
}


/*
 * Ban functions to work with mode +b
 */
/* add_banid - add an id to be banned to the channel  (belongs to cptr) */

static int add_banid(cptr, chptr, banid)
	aClient *cptr;
	aChannel *chptr;
	char *banid;
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
				if (!match(ban->banstr, banid) ||
				    !match(banid, ban->banstr))
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
static int del_banid(chptr, banid)
	aChannel *chptr;
	char *banid;
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
 * find_banid - Find an exact match for a ban
 */
static int find_banid(chptr, banid)
	aChannel *chptr;
	char *banid;
{
	Ban **ban;

	if (!banid)
		return -1;
	for (ban = &(chptr->banlist); *ban; ban = &((*ban)->next))
		if (!mycmp(banid, (*ban)->banstr))
			return 1;
	return 0;
}

/*
 * IsMember - returns 1 if a person is joined
 * Moved to struct.h
 */

/*
 * is_banned - returns a pointer to the ban structure if banned else NULL
 */
extern Ban *is_banned(cptr, sptr, chptr)
	aClient *cptr, *sptr;
	aChannel *chptr;
{
	Ban *tmp, *tmp2;
	char *s;
	static char realhost[NICKLEN + USERLEN + HOSTLEN + 6];
	static char virthost[NICKLEN + USERLEN + HOSTLEN + 6];

	int  dovirt = 0;

	if (!IsPerson(cptr))
		return NULL;

	if (strcmp(cptr->user->realhost, cptr->user->virthost))
		dovirt = 1;

	s = make_nick_user_host(cptr->name, cptr->user->username,
	    cptr->user->realhost);
	strcpy(realhost, s);

	s = make_nick_user_host(cptr->name, cptr->user->username,
	    cptr->user->virthost);
	strcpy(virthost, s);
/* We now check +b first, if a +b is found we then see if there is a +e.
 * If a +e was found we return NULL, if not, we return the ban.
 */
	for (tmp = chptr->banlist; tmp; tmp = tmp->next)
		if ((match(tmp->banstr, realhost) == 0) ||
		    (dovirt && (match(tmp->banstr, virthost) == 0)))
		{
			/* Ban found, now check for +e */
			for (tmp2 = chptr->exlist; tmp2; tmp2 = tmp2->next)
				if ((match(tmp2->banstr, realhost) == 0) ||
				    (dovirt
				    && (match(tmp2->banstr, virthost) == 0)))
					return (NULL);

			break;
		}

	return (tmp);
}

/*
 * adds a user to a channel by adding another link to the channels member
 * chain.
 */
static void add_user_to_channel(chptr, who, flags)
	aChannel *chptr;
	aClient *who;
	int  flags;
{
	Link *ptr;

	if (who->user)
	{
		ptr = make_link();
		ptr->value.cptr = who;
		ptr->flags = flags;
		/* we should make this more efficient --stskeeps */
		ptr->flood = (aFloodOpt *) MyMalloc(sizeof(aFloodOpt));
		ptr->flood->nmsg = 0;
		ptr->flood->lastmsg = 0;
		ptr->next = chptr->members;
		chptr->members = ptr;
		chptr->users++;

		ptr = make_link();
		ptr->value.chptr = chptr;
		ptr->next = who->user->channel;
		who->user->channel = ptr;
		who->user->joined++;
	}
}

void remove_user_from_channel(sptr, chptr)
	aClient *sptr;
	aChannel *chptr;
{
	Link **curr;
	Link *tmp;
	Link *lp = chptr->members;

	for (; lp && (lp->value.cptr == sptr); lp = lp->next);
	for (;;)
	{
		for (curr = &chptr->members; (tmp = *curr); curr = &tmp->next)
			if (tmp->value.cptr == sptr)
			{
				if (tmp->flood)
					MyFree((aFloodOpt *) tmp->flood);
				*curr = tmp->next;
				free_link(tmp);
				break;
			}
		for (curr = &sptr->user->channel; (tmp = *curr);
		    curr = &tmp->next)
			if (tmp->value.chptr == chptr)
			{
				*curr = tmp->next;
				free_link(tmp);
				break;
			}
		sptr->user->joined--;
		if (lp)
			break;
		if (chptr->members)
			sptr = chptr->members->value.cptr;
		else
			break;
		sub1_from_channel(chptr);
	}
	sub1_from_channel(chptr);
}


/*
static	int	have_ops(chptr)
aChannel *chptr;
{
		Link	*lp;

	if (chptr)
        {
	  lp=chptr->members;
	  while (lp)
	  {
	    if (lp->flags & CHFL_CHANOP) return(1);
	    lp = lp->next;
	  }
        }
	return 0;
}
*/
int  is_chan_op(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *lp;
/* chanop/halfop ? */
	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			return ((lp->flags & CHFL_CHANOP));

	return 0;
}


/* This was the original function that allowed mode hacking - not
   anymore. -- Barubary */
/*static	int	is_deopped(cptr, chptr)
aClient *cptr;
aChannel *chptr;
{
	if (!IsPerson(cptr)) return 0;
	return !is_chan_op(cptr, chptr);
}
*/

int  has_voice(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *lp;

	if (chptr)
		if ((lp = find_user_link(chptr->members, cptr)))
			return (lp->flags & CHFL_VOICE);

	return 0;
}
int  is_halfop(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *lp;

	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			if (!is_chan_op(cptr, chptr))	/* excessive but needed */
				return (lp->flags & CHFL_HALFOP);

	return 0;
}

int  is_chanowner(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *lp;

	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			return (lp->flags & CHFL_CHANOWNER);

	return 0;
}

int  is_chanprot(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *lp;

	if (chptr)
		if (lp = find_user_link(chptr->members, cptr))
			return (lp->flags & CHFL_CHANPROT);

	return 0;
}

#define CANNOT_SEND_MODERATED 1
#define CANNOT_SEND_NOPRIVMSGS 2
#define CANNOT_SEND_NOCOLOR 3
#define CANNOT_SEND_BAN 4
#define CANNOT_SEND_NOCTCP 5

int  can_send(cptr, chptr, msgtext)
	aClient *cptr;
	aChannel *chptr;
	char *msgtext;
{
	Link *lp;
	int  member;

	/* Moved check here, kinda faster.
	 * Note IsULine only uses the other parameter. -Donwulff */
	if (IsULine(cptr, cptr) || IsServer(cptr))
		return 0;

	if (chptr->mode.mode & MODE_NOCOLOR)
		if (strchr((char *)msgtext, 3) || strchr((char *)msgtext, 27))
			return (CANNOT_SEND_NOCOLOR);

	member = IsMember(cptr, chptr);
	if (chptr->mode.mode & MODE_NOPRIVMSGS && !member)
		return (CANNOT_SEND_NOPRIVMSGS);

	lp = find_user_link(chptr->members, cptr);


	if (chptr->mode.mode & MODE_MODERATED &&
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))))
		return (CANNOT_SEND_MODERATED);

	if (chptr->mode.mode & MODE_NOCTCP &&
	    (!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_CHANOWNER | CHFL_CHANPROT))))
		if (msgtext[0] == 1 && strncmp(&msgtext[1], "ACTION ", 7))
			return (CANNOT_SEND_NOCTCP);

	/* Makes opers able to talk thru bans -Stskeeps suggested by The_Cat */
	if (IsOper(cptr))
		return 0;

	if ((!lp
	    || !(lp->flags & (CHFL_CHANOP | CHFL_VOICE | CHFL_CHANOWNER |
	    CHFL_HALFOP | CHFL_CHANPROT))) && MyClient(cptr)
	    && is_banned(cptr, cptr, chptr))
		return (CANNOT_SEND_BAN);

	return 0;
}

/*
 * write the "simple" list of channel modes for channel chptr onto buffer mbuf
 * with the parameters in pbuf.
 */
static void channel_modes(cptr, mbuf, pbuf, chptr)
	aClient *cptr;
	char *mbuf, *pbuf;
	aChannel *chptr;
{
	long zode;
	aCtab *tab = &cFlagTab[0];
	char bcbuf[1024];

	*mbuf++ = '+';
	while (tab->mode != 0x0)
	{
		if ((chptr->mode.mode & tab->mode))
		{
			zode = chptr->mode.mode;
			if (!(zode & (MODE_LIMIT | MODE_KEY | MODE_LINK)))
				if (!(zode & (MODE_CHANOP | MODE_VOICE |
				    MODE_CHANOWNER)))
					if (!(zode & (MODE_BAN | MODE_EXCEPT |
					    MODE_CHANPROT)))
						if (!(zode & (MODE_HALFOP)))
							*mbuf++ = tab->flag;
		}
		tab++;
	}
	if (chptr->mode.limit)
	{
		*mbuf++ = 'l';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr, cptr))
			(void)ircsprintf(pbuf, "%d ", chptr->mode.limit);
	}
	if (*chptr->mode.key)
	{
		*mbuf++ = 'k';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr, cptr))
		{
			(void)ircsprintf(bcbuf, "%s ", chptr->mode.key);
			(void)strcat(pbuf, bcbuf);
		}
	}
	if (*chptr->mode.link)
	{
		*mbuf++ = 'L';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr, cptr))
		{
			(void)ircsprintf(bcbuf, "%s ", chptr->mode.link);
			(void)strcat(pbuf, bcbuf);
		}
	}
	/* if we add more parameter modes, add a space to the strings here --Stskeeps */
	if (chptr->mode.per)
	{
		*mbuf++ = 'f';
		if (IsMember(cptr, chptr) || IsServer(cptr)
		    || IsULine(cptr, cptr))
		{
			if (chptr->mode.kmode == 1)
				ircsprintf(bcbuf, "*%i:%i ", chptr->mode.msgs,
				    chptr->mode.per);
			else
				ircsprintf(bcbuf, "%i:%i ", chptr->mode.msgs,
				    chptr->mode.per);
			(void)strcat(pbuf, bcbuf);
		}

	}

	*mbuf++ = '\0';
	return;
}

static int send_mode_list(cptr, chname, creationtime, top, mask, flag)
	aClient *cptr;
	Link *top;
	int  mask;
	char flag, *chname;
	time_t creationtime;
{
	Link *lp;
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
			name = lp->value.cptr->name;
		}
		if (strlen(parabuf) + strlen(name) + 11 < (size_t)MODEBUFLEN)
		{
			if (*parabuf)
				(void)strcat(parabuf, " ");
			(void)strcat(parabuf, name);
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
				(void)strcpy(parabuf, name);
				*cp++ = flag;
			}
			count = 0;
			*cp = '\0';
		}
	}
	return sent;
}

/*
 * send "cptr" a full list of the modes for channel chptr.
 */
void send_channel_modes(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
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
		sendto_one(cptr, ":%s %s %s %s %s %lu", me.name,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
		    modebuf, parabuf, chptr->creationtime);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';

	sent = send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->members, CHFL_HALFOP, 'h');
	if (!sent && chptr->creationtime)
		sendto_one(cptr, ":%s %s %s %s %s %lu", me.name,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), chptr->chname,
		    modebuf, parabuf, chptr->creationtime);
	else if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name,
		    chptr->chname, modebuf, parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->banlist, CHFL_BAN, 'b');
	if (modebuf[1] || *parabuf)
		sendmodeto_one(cptr, me.name, chptr->chname, modebuf,
		    parabuf, chptr->creationtime);

	*parabuf = '\0';
	*modebuf = '+';
	modebuf[1] = '\0';
	(void)send_mode_list(cptr, chptr->chname, chptr->creationtime,
	    chptr->exlist, CHFL_EXCEPT, 'e');
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
/*********/

/*
 * m_samode
 * parv[0] = sender
 * parv[1] = channel
 * parv[2] = modes
 * -taz
 */
int  m_samode(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aChannel *chptr;

	if (!IsPrivileged(cptr) || !IsSAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	if (parc > 2)
	{
		chptr = find_channel(parv[1], NullChn);
		if (chptr == NullChn)
			return 0;
	}
	else
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "MODE");
		return 0;
	}
	opermode = 0;
	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, 0, 1);

	return 0;

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
int  m_mode(cptr, sptr, parc, parv)
	aClient *cptr;
	aClient *sptr;
	int  parc;
	char *parv[];
{
	static char mode_buf[MODEBUFLEN], parabuf[MODEBUFLEN];
	long unsigned sendts = 0;
	Ban *ban;
	aClient *user;
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

	sptr->flags &= ~FLAGS_TS8;

	if (MyConnect(sptr))
		clean_channelname(parv[1]);
	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;

	if (parc < 3)
	{
		*mode_buf = *parabuf = '\0';
		mode_buf[1] = '\0';
		channel_modes(sptr, mode_buf, parabuf, chptr);
		sendto_one(sptr, rpl_str(RPL_CHANNELMODEIS), me.name, parv[0],
		    chptr->chname, mode_buf, parabuf);
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
			struct SLink *member;
			/* send chanowner list */
			/* For our future reference i think there is a 
			   problem doing this the way i am ... i think
			   the way i commented out would work better as
			   the members->value.cptr points to a user in
			   the linked list of users ... however, by doing
			   it the way i have there is risk of missing ppl
			   out ... but doing it the other way seems to bugger
			   it all up ... DrBin */
			/* BTW: i was right =) */

			/* For prosperity ... keep this line, and give it to any new coders to 
			   see if they can a) spot whats wrong and[in both cases] b) correct it
			   *devilish grin* -- DrBin ... after 1 hr of debugging ... realised he
			   was lookin at the same piece of data... many times */
/*for (user=chptr->members->value.cptr ; user; user=/*chptr->members->next->value.cptr *//*user->next *//*) */
			for (member = chptr->members, user = member->value.cptr;
			    member->next;
			    member = member->next, user = member->value.cptr)
			{
				if (is_chanowner(user, chptr))
					sendto_one(sptr, rpl_str(RPL_QLIST),
					    me.name, sptr->name, chptr->chname,
					    user->name);
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
			struct SLink *member;
			/* send chanowner list */
			/* For our future reference i think there is a 
			   problem doing this the way i am ... i think
			   the way i commented out would work better as
			   the members->value.cptr points to a user in
			   the linked list of users ... however, by doing
			   it the way i have there is risk of missing ppl
			   out ... but doing it the other way seems to bugger
			   it all up ... DrBin */
			/* BTW: i was right =) */

			/* For prosperity ... keep this line, and give it to any new coders to 
			   see if they can a) spot whats wrong and[in both cases] b) correct it
			   *devilish grin* -- DrBin ... after 1 hr of debugging ... realised he
			   was lookin at the same piece of data... many times */
/*for (user=chptr->members->value.cptr ; user; user=/*chptr->members->next->value.cptr *//*user->next *//*) */
			for (member = chptr->members, user = member->value.cptr;
			    member->next;
			    member = member->next, user = member->value.cptr)
			{
				if (is_chanprot(user, chptr))
					sendto_one(sptr, rpl_str(RPL_ALIST),
					    me.name, sptr->name, chptr->chname,
					    user->name);
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
		sendto_one(sptr, rpl_str(RPL_ENDOFINVITELIST), me.name,
		    sptr->name, chptr->chname);
		return 0;
	}
	opermode = 0;
	/* opermode stuff --sts */
#ifndef NO_OPEROVERRIDE
	if (IsPerson(sptr) && !IsULine(cptr, sptr) && !is_chan_op(sptr, chptr))
		if (IsOper(sptr))
		{
			sendts = 0;
			opermode = 1;
			goto aftercheck;
		}
#endif
/*-------*/


	if (is_half_op(sptr, chptr))
	{
		opermode = 2;
		goto aftercheck;
	}
	if (IsPerson(sptr) && !IsULine(cptr, sptr) && !is_chan_op(sptr, chptr)
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

	if (IsServer(sptr) && (sendts = atoi(parv[parc - 1]))
	    && !IsULine(cptr, sptr) && chptr->creationtime
	    && sendts > chptr->creationtime)
	{
		if (!(*parv[2] == '&'))	/* & denotes a bounce */
		{
			/* !!! */
			sendto_umode(UMODE_EYES,
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
			if (MyClient(sptr) && !IsULine(cptr, cptr))
				sendto_umode(UMODE_EYES, "*** OperMode [IRCop: %s] - [Channel: %s] - [Mode: %s %s]",
        	 		   sptr->name, chptr->chname, mode_buf, parabuf);
			sendts = 0;
		}
	}	
*/
	/* Filter out the unprivileged FIRST. *
	 * Now, we can actually do the mode.  */

	(void)do_mode(chptr, cptr, sptr, parc - 2, parv + 2, sendts, 0);

	return 0;
}

/* bounce_mode -- written by binary
 *	User or server is NOT authorized to change the mode.  This takes care
 * of making the bounce string and bounce it.  Because of the 1 for the bounce
 * param (last param) of the calls to set_mode and make_mode_str, it will not
 * set the mode, but create the bounce string.
 */
void bounce_mode(chptr, cptr, parc, parv)
	aChannel *chptr;
	aClient *cptr;
	int  parc;
	char *parv[];
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	long oldm, oldl;
	char mode_buf[MODEBUFLEN], parabuf[MODEBUFLEN];

	oldm = chptr->mode.mode;
	oldl = chptr->mode.limit;
	set_mode(chptr, cptr, parc, parv, &pcount, pvar, 1);
	make_mode_str(chptr, oldm, oldl, pcount, pvar, mode_buf, parabuf, 1);

	if (chptr->creationtime)
		sendto_one(cptr, ":%s MODE %s &%s %s %lu", me.name,
		    chptr->chname, mode_buf, parabuf, chptr->creationtime);
	else
		sendto_one(cptr, ":%s MODE %s &%s %s", me.name, chptr->chname,
		    mode_buf, parabuf);

	/* the '&' denotes a bounce so servers won't bounce a bounce */
}

/* do_mode -- written by binary
 *	User or server is authorized to do the mode.  This takes care of
 * setting the mode and relaying it to other users and servers.
 */
void do_mode(chptr, cptr, sptr, parc, parv, sendts, samode)
	aChannel *chptr;
	aClient *cptr, *sptr;
	int  parc, sendts, samode;
	char *parv[];
{
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	int  pcount;
	long oldm, oldl;
	char tschange = 0, isbounce = 0;	/* fwd'ing bounce */
	char mode_buf[MODEBUFLEN], parabuf[MODEBUFLEN];

	if (**parv == '&')
		isbounce = 1;
	oldm = chptr->mode.mode;
	oldl = chptr->mode.limit;

	set_mode(chptr, sptr, parc, parv, &pcount, pvar, 0);
	make_mode_str(chptr, oldm, oldl, pcount, pvar, mode_buf, parabuf, 0);

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
					sendto_umode(UMODE_EYES, "*** TS fix for %s - %lu(ours) %lu(theirs)",
					chptr->chname, chptr->creationtime, sendts);			
					*/
				chptr->creationtime = sendts;
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
	if (*mode_buf == '\0' || (*(mode_buf + 1) == '\0' && (*mode_buf == '+'
	    || *mode_buf == '-')))
	{
		if (tschange || isbounce)	/* relay bounce time changes */
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
	/* opermode for twimodesystem --sts */
#ifndef NO_OPEROVERRIDE
	if (opermode == 1)
	{
		if (MyClient(sptr))
			sendto_umode(UMODE_EYES,
			    "*** OperMode [IRCop: %s] - [Channel: %s] - [Mode: %s %s]",
			    sptr->name, chptr->chname, mode_buf, parabuf);
		sendts = 0;
	}
#endif

	if (IsPerson(sptr) && samode && MyClient(sptr))
	{
		sendto_serv_butone_token(NULL, me.name, MSG_GLOBOPS,
		    TOK_GLOBOPS, ":%s used SAMODE %s (%s %s)", sptr->name,
		    chptr->chname, mode_buf, parabuf);
		sendto_failops_whoare_opers
		    ("from %s: %s used SAMODE %s (%s %s)", me.name, sptr->name,
		    chptr->chname, mode_buf, parabuf);
		sptr = &me;
		sendts = 0;
	}
	sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
	    sptr->name, chptr->chname, mode_buf, parabuf);
	if (IsServer(sptr) && sendts != -1)
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s %lu", chptr->chname, isbounce ? "&" : "",
		    mode_buf, parabuf, sendts);
	else
		sendto_serv_butone_token(cptr, sptr->name, MSG_MODE, TOK_MODE,
		    "%s %s%s %s 0", chptr->chname, isbounce ? "&" : "",
		    mode_buf, parabuf);
	/* tell them it's not a timestamp, in case the last param
	   ** is a number. */
}
/* make_mode_str -- written by binary
 *	Reconstructs the mode string, to make it look clean.  mode_buf will
 *  contain the +x-y stuff, and the parabuf will contain the parameters.
 *  If bounce is set to 1, it will make the string it needs for a bounce.
 */
void make_mode_str(chptr, oldm, oldl, pcount, pvar, mode_buf, parabuf, bounce)
	aChannel *chptr;
	int  pcount;
	long oldm, oldl;
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3], *mode_buf, *parabuf, bounce;
{

	char tmpbuf[MODEBUFLEN], *tmpstr;
	aCtab *tab = &cFlagTab[0];
	char *x = mode_buf;
	int  what, cnt, z;

	what = 0;

	*tmpbuf = '\0';
	*mode_buf = '\0';
	*parabuf = '\0';
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

	*x = '\0';
/* + param-less modes */
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

	*x = '\0';
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
		strncat(parabuf, tmpstr, MODEBUFLEN - 1);
		parabuf[MODEBUFLEN - 1] = '\0';
		z = strlen(parabuf);
		parabuf[z] = ' ';	/* add a space */
		parabuf[z + 1] = '\0';
	}
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
			ircsprintf(parabuf, "%s%d", parabuf, chptr->mode.limit);
		}
	}
	if (bounce)
		chptr->mode.mode = oldm;
	z = strlen(parabuf);
	if (parabuf[z - 1] == ' ')
		parabuf[z - 1] = '\0';
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

int  do_mode_char(chptr, modetype, modechar, param, what, cptr, pcount, pvar,
    bounce)
	char *param, modechar, pvar[MAXMODEPARAMS][MODEBUFLEN + 3], bounce;
	u_int what;
	long modetype;
	u_int *pcount;
	aClient *cptr;
	aChannel *chptr;

{
	aCtab *tab = &cFlagTab[0];


	int  retval = 0;
	Link *member;
	aClient *who;
	unsigned int tmp = 0;
	char tmpbuf[512], *tmpstr;
	char tc = ' ';		/* */
	int  chasing, x;
	int  xxi, xyi, xzi;
	char *xxx;
	char *xp;

	chasing = 0;
	if (opermode == 2 && !is_chan_op(cptr, chptr))
	{
		/* Ugly halfop hack --sts 
		   - this allows halfops to do +b +e +v and so on */
		if (Halfop_mode(modetype) == FALSE)
		{
			while (tab->mode != 0x0)
			{
				if (tab->mode == modetype)
				{
					sendto_one(cptr,
					    err_str(ERR_NOTFORHALFOPS), me.name,
					    cptr->name, tab->flag);
				}
				tab++;
			}
			return (0);
		}
	}
	switch (modetype)
	{
	  case MODE_AUDITORIUM:
		  if (IsULine(cptr, cptr) || IsServer(cptr))
			  goto auditorium_ok;
		  if (!IsNetAdmin(cptr) && !IsTechAdmin(cptr)
		      && !is_chanowner(cptr, chptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Channel mode +u can only be set by the channel owner",
			      me.name, cptr->name);
			  break;
		  }

		auditorium_ok:
		  goto setthephuckingmode;
	  case MODE_OPERONLY:
		  if (!IsAnOper(cptr) && !IsServer(cptr)
		      && !IsULine(cptr, cptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Only IRCops can set that mode",
			      me.name, cptr->name);
			  break;
		  }
		  goto setthephuckingmode;
	  case MODE_ADMONLY:
		  if (!IsSkoAdmin(cptr) && !IsServer(cptr)
		      && !IsULine(cptr, cptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Only admins can set that mode",
			      me.name, cptr->name);
			  break;
		  }
		  goto setthephuckingmode;
	  case MODE_RGSTR:
		  if (!IsServer(cptr) && !IsULine(cptr, cptr))
			  break;
		  goto setthephuckingmode;
	  case MODE_NOHIDING:
		  if (!IsSkoAdmin(cptr) && !IsServer(cptr)
		      && !IsULine(cptr, cptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Only admins can set that mode",
			      me.name, cptr->name);
			  break;
		  }
		  goto setthephuckingmode;
	  case MODE_SECRET:
	  case MODE_PRIVATE:
	  case MODE_MODERATED:
	  case MODE_TOPICLIMIT:
	  case MODE_NOPRIVMSGS:
	  case MODE_INVITEONLY:
	  case MODE_RGSTRONLY:
	  case MODE_NOCOLOR:
	  case MODE_NOKICKS:
	  case MODE_STRIP:
	  case MODE_NOKNOCK:
#ifdef STRIPBADWORDS
	  case MODE_STRIPBADWORDS:
#endif
	  case MODE_NOCTCP:
	  case MODE_NOINVITE:
		setthephuckingmode:
		  /* +sp bugfix.. */
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
		  retval = 0;
		  if (what == MODE_ADD)
			  chptr->mode.mode |= modetype;
		  else
			  chptr->mode.mode &= ~modetype;
		  break;

/* do pro-opping here (popping) */
	  case MODE_CHANOWNER:
		  if (!IsULine(cptr, cptr) && !IsServer(cptr)
		      && !IsNetAdmin(cptr) && !IsTechAdmin(cptr)
		      && !is_chanowner(cptr, chptr))
		  {
			  sendto_one(cptr, err_str(ERR_ONLYSERVERSCANCHANGE),
			      me.name, cptr->name, chptr->chname);
			  break;
		  }
	  case MODE_CHANPROT:
		  if (!IsULine(cptr, cptr) && !IsServer(cptr)
		      && !IsNetAdmin(cptr) && !IsTechAdmin(cptr)
		      && !is_chanowner(cptr, chptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Protected users can only be set by the channel owner.",
			      me.name, cptr->name);
			  break;
		  }

	  case MODE_HALFOP:
	  case MODE_CHANOP:
	  case MODE_VOICE:
		  switch (modetype)
		  {
		    case MODE_CHANOWNER:
			    xxx = "deown";
			    break;
		    case MODE_CHANPROT:
			    xxx = "deprot";
			    break;
		    case MODE_HALFOP:
			    xxx = "dehalfop";
			    break;
		    case MODE_CHANOP:
			    xxx = "deop";
			    break;
		    case MODE_VOICE:
			    xxx = "devoice";
			    break;
		    default:
			    xxx = "whatthefuckisthatmode?";
		  }
		  if (!param || *pcount >= MAXMODEPARAMS)
		  {
			  retval = 0;
			  break;
		  }
		  retval = 1;
		  if (!(who = find_chasing(cptr, param, &chasing)))
			  break;
		  if (!(member = find_user_link(chptr->members, who)))
		  {
			  sendto_one(cptr, err_str(ERR_USERNOTINCHANNEL),
			      me.name, cptr->name, who->name, chptr->chname);
			  break;
		  }
		  /* we make the rules, we bend the rules */
		  if (IsServer(cptr) || IsULine(cptr, cptr))
			  goto breaktherules;

		  if (is_chanowner(member->value.cptr, chptr)
		      && member->value.cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr)
		      && !IsULine(cptr, cptr) && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
				  sendto_one(cptr,
				      ":%s NOTICE %s :*** You cannot %s because %s is %s channel owner (+q)",
				      me.name, cptr->name, xxx,
				      member->value.cptr->name, chptr->chname);
			  }
			  break;
		  }
		  if (is_chanprot(member->value.cptr, chptr)
		      && member->value.cptr != cptr
		      && !is_chanowner(cptr, chptr) && !IsServer(cptr)
		      && modetype != MODE_CHANOWNER && (what == MODE_DEL))
		  {
			  if (MyClient(cptr))
			  {
				  sendto_one(cptr,
				      ":%s NOTICE %s :*** You cannot %s because %s is %s protected user (+a)",
				      me.name, cptr->name, xxx,
				      member->value.cptr->name, chptr->chname);
			  }
			  break;
		  }
		breaktherules:
		  tmp = member->flags;
		  if (what == MODE_ADD)
			  member->flags |= modetype;
		  else
			  member->flags &= ~modetype;
		  if (tmp == member->flags && (bounce || !IsULine(cptr, cptr)))
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
			  if (!bounce)	/* don't do the mode at all. */
				  strncpyzt(chptr->mode.key, param,
				      sizeof(chptr->mode.key));
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
		  tmpstr = pretty_mask(param);
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
		  tmpstr = pretty_mask(param);
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
		  if (IsULine(cptr, cptr) || IsServer(cptr))
		  {
			  goto linkok;
		  }

		  if (!IsNetAdmin(cptr) && !IsTechAdmin(cptr)
		      && !is_chanowner(cptr, chptr))
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Channel mode +L can only be set by the channel owner",
			      me.name, cptr->name);
			  break;
		  }

		linkok:
		  if (!chptr->mode.limit && what == MODE_ADD)
		  {
			  sendto_one(cptr,
			      ":%s NOTICE %s :*** Channel mode +l <max> is requried for +L to be set",
			      me.name, cptr->name);
			  break;
		  }
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
			  if (!param || *pcount >= MAXMODEPARAMS)
			  {
				  retval = 0;
				  break;
			  }

			  if (!stricmp(param, chptr->chname))
			  {
				  if (MyClient(cptr))
					  sendto_one(cptr,
					      ":%s NOTICE %s :*** You can't link %s to itself",
					      me.name, cptr->name,
					      chptr->chname);
				  break;
			  }
			  if (!IsChannelName(param))
			  {
				  if (MyClient(cptr))
					  sendto_one(cptr,
					      err_str(ERR_NOSUCHCHANNEL),
					      me.name, cptr->name, param);
				  break;
			  }
			  if (!bounce)	/* don't do the mode at all. */
				  strncpyzt(chptr->mode.link, param,
				      sizeof(chptr->mode.link));
			  tmpstr = param;
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
				  for (xp = param; *xp; xp++)
				  {
					  /* fast alpha check */
					  if ((*xp < '0')
					      && (*xp > '9')
					      && (*xp != '\0')
					      && (*xp != ':')
					      && (*xp == '*' && xp != param))

						  break;
				  }
				  /* haven't got a : .. oh well */
				  if (!strchr(param, ':'))
				  {
					  break;
				  }
				  /* got multiple :'s .. omg */
				  if (strchr(param, ':') != strrchr(param, ':'))
				  {
					  break;
				  }
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

		  (void)ircsprintf(pvar[*pcount], "%cf%s",
		      what == MODE_ADD ? '+' : '-', tmpstr);
		  (*pcount)++;
		  break;
	}
	return retval;
}

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
void set_mode(chptr, cptr, parc, parv, pcount, pvar, bounce)
	aChannel *chptr;
	aClient *cptr;
	int  parc;
	u_int *pcount;
	char bounce;
	char *parv[], pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
{
	char *curchr;
	u_int what = MODE_ADD;
	long modetype;
	int  paracount;
	char *tmpo;
	aCtab *tab = &cFlagTab[0];
	aCtab foundat;
	int  found = 0;
	paracount = 1;
	*pcount = 0;
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
		  case 'x':
			  if (MyClient(cptr))
			  {
				  sendto_one(cptr,
				      ":%s NOTICE %s :*** Mode not set - Please do not use mode +x as this is now named +c instead (Colorblock)",
				      me.name, cptr->name);
				  break;
			  }
			  else
			  {
				  /* compatiblity */
				  *curchr = 'c';
			  }
			  goto jumpdammit;
		  case 'I':
			  if (MyClient(cptr))
			  {
				  break;
			  }
			  else
			  {
				  /* compatiblity */
				  *curchr = 'V';
			  }
			jumpdammit:
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
				  modetype = foundat.mode;
			  if (found == 0)
				  modetype = 0;

			  if (modetype == 0)
			  {
				  if (!MyClient(cptr))
					  break;
				  /* don't flood other servers */
				  sendto_one(cptr,
				      err_str(ERR_UNKNOWNMODE),
				      me.name, cptr->name, *curchr);
				  break;
			  }
			  if (parv[paracount] &&
			      strlen(parv[paracount]) >= MODEBUFLEN)
				  parv[paracount][MODEBUFLEN - 1] = '\0';
			  paracount +=
			      do_mode_char(chptr, modetype, *curchr,
			      parv[paracount], what, cptr, pcount, pvar,
			      bounce);
			  break;
		}
	}
}

int  DoesOp(modebuf)
	char *modebuf;
{
	modebuf--;		/* Is it possible that a mode starts with o and not +o ? */
	while (*++modebuf)
		if (*modebuf == 'h' || *modebuf == 'o'
		    || *modebuf == 'v' || *modebuf == 'q')
			return (1);
	return 0;
}

int  sendmodeto_one(cptr, from, name, mode, param, creationtime)
	aClient *cptr;
	char *from, *name, *mode, *param;
	time_t creationtime;
{
	if ((IsServer(cptr) && DoesOp(mode) && creationtime) ||
	    IsULine(cptr, cptr))
		sendto_one(cptr, ":%s %s %s %s %s %lu", from,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), name, mode,
		    param, creationtime);
	else
		sendto_one(cptr, ":%s %s %s %s %s", from,
		    (IsToken(cptr) ? TOK_MODE : MSG_MODE), name, mode, param);
}

char *pretty_mask(mask)
	char *mask;
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
/* Now let _invited_ people join thru bans, +i and +l.
 * Checking if an invite exist could be done only if a block exists,
 * but I'm not too fancy of the complicated structure that'd cause,
 * when optimization will hopefully take care of it. Most of the time
 * a user won't have invites on him anyway. -Donwulff
 */

static int can_join(cptr, sptr, chptr, key, link, parv)
	aClient *cptr, *sptr;
	aChannel *chptr;
	char *key, *link;
	char *parv[];
{
	Link *lp;
#ifndef NO_OPEROVERRIDE
	int  ib = 1;
#else
	int  ib = 0;
#endif


	/* 0 = noone 1 = IRCops 2 = Net Admins 3 =Net/Tech admins  (override) */

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
				channel_link(cptr, sptr, 2, parv);
				return -1;
			}
		}
		/* We check this later return (ERR_CHANNELISFULL); */
	}

/*    if ((chptr->mode.mode & MODE_OPERONLY) && IsOper(sptr)) {
    	goto admok;
    } */
	if ((chptr->mode.mode & MODE_OPERONLY) && !IsOper(sptr))
	{
		return (ERR_OPERONLY);
	}
	if ((chptr->mode.mode & MODE_ADMONLY))
	{
		if (!IsSkoAdmin(sptr))
			return (ERR_ADMONLY);
	}

	if ((chptr->mode.mode & MODE_NOHIDING) && IsHiding(sptr))
		return (ERR_NOHIDING);

	if ((IsOper(sptr) && !(chptr->mode.mode & MODE_ADMONLY)))
	{
		return 0;	/* may override */
	}
	if ((IsOper(sptr) && is_banned(cptr, sptr, chptr)
	    && (chptr->mode.mode & MODE_OPERONLY)))
		return (ERR_BANNEDFROMCHAN);	/* banned as an ircop at a +O cannot join */

/*	if (ib == 1)
		if (IsOper(sptr))
			return 0;
	else
		if (ib == 2)
			if (IsNetAdmin(sptr))
				return 0;
*/
	switch (ib)
	{
	  case 1:
		  if (IsOper(sptr))
		  {
			  return 0;
		  }
		  break;
	  case 2:
		  if (IsNetAdmin(sptr))
		  {
			  return 0;
		  }
		  break;
	  case 3:
		  if (IsNetAdmin(sptr) || IsTechAdmin(sptr))
			  return 0;
		  break;
	  default:
		  break;
	}
	if ((chptr->mode.mode & MODE_RGSTRONLY) && !IsARegNick(sptr))
		return (ERR_NEEDREGGEDNICK);

	if (*chptr->mode.key && (BadPtr(key) || mycmp(chptr->mode.key, key)))
		return (ERR_BADCHANNELKEY);

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
			break;

	if ((chptr->mode.mode & MODE_INVITEONLY) && !lp)
		return (ERR_INVITEONLYCHAN);

	if ((chptr->mode.limit && chptr->users >= chptr->mode.limit))
		return (ERR_CHANNELISFULL);

	if (is_banned(sptr, sptr, chptr) && !lp)
		return (ERR_BANNEDFROMCHAN);



	return 0;
}


/*
** Remove bells and commas from channel name
*/

void clean_channelname(cn)
	char *cn;
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
static int check_channelmask(sptr, cptr, chname)
	aClient *sptr, *cptr;
	char *chname;
{
	char *s;

	if (*chname == '&')
		return -1;
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
static aChannel *get_channel(cptr, chname, flag)
	aClient *cptr;
	char *chname;
	int  flag;
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
		chptr->creationtime = MyClient(cptr) ? TStime() : (time_t) 0;
		channel = chptr;
		(void)add_to_channel_hash_table(chname, chptr);
		IRCstats.channels++;
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

static void add_invite(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
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
void del_invite(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
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
static void sub1_from_channel(chptr)
	aChannel *chptr;
{
	Ban *ban;
	Link *lp;

	if (--chptr->users <= 0)
	{
		/*
		 * Now, find all invite links from channel structure
		 */
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
 * Channel Link
 */
int  channel_link(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	static char jbuf[BUFSIZE];
	Link *lp;
	aChannel *chptr;
	char *name, *key = NULL, *link = NULL;
	int  i, i1, flags = 0;
	char *p = NULL, *p2 = NULL;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "JOIN");
		return 0;
	}

	*jbuf = '\0';
	/*
	   ** Rebuild list of channels joined to be the actual result of the
	   ** JOIN.  Note that "JOIN 0" is the destructive problem.
	 */
	bouncedtimes++;
	if (bouncedtimes > MAXBOUNCE)
	{
		/* bounced too many times */
		sendto_one(sptr,
		    ":%s NOTICE %s :*** Couldn't join %s ! - Link setting was too bouncy",
		    me.name, sptr->name, parv[1]);
		return;
	}
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
		if (*name == '&')
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
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name) + 1;
	}
	(void)strcpy(parv[1], jbuf);

	p = NULL;
	if (parv[2])
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
				chptr = lp->value.chptr;
				sendto_channel_butserv(chptr, sptr,
				    PartFmt, parv[0], chptr->chname);
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
			flags =
			    (ChannelExists(name)) ? CHFL_DEOPPED : CHFL_CHANOP;

			if (sptr->user->joined >= MAXCHANNELSPERUSER)
			{
				sendto_one(sptr,
				    err_str(ERR_TOOMANYCHANNELS),
				    me.name, parv[0], name);
				return 0;
			}
		}

		chptr = get_channel(sptr, name, CREATE);

		if (chptr && (lp = find_user_link(chptr->members, sptr)))
			continue;

		if (!MyConnect(sptr))
			flags = CHFL_DEOPPED;
		if (sptr->flags & FLAGS_TS8)
			flags |= CHFL_SERVOPOK;

		i1 = 0;
		if (chptr == NULL)
			return;

		if (!chptr ||
		    (MyConnect(sptr)
		    && (i = can_join(cptr, sptr, chptr, key, link, parv))))
		{
			if (i != -1)
			{
				sendto_one(sptr, err_str(i),
				    me.name, parv[0], name);
			}
			continue;
		}

		/*
		   **  Complete user entry to the new channel (if any)
		 */
		add_user_to_channel(chptr, sptr, flags);
		/*
		   ** notify all other users on the new channel
		 */
		if (!IsHiding(sptr))
			sendto_channel_butserv(chptr, sptr,
			    ":%s JOIN :%s", parv[0], name);
		else
		{
			if (MyClient(sptr))
				sendto_one(sptr, ":%s!%s@%s JOIN :%s",
				    sptr->name, sptr->user->username,
				    (IsHidden(sptr) ? sptr->
				    user->virthost : sptr->user->realhost),
				    name);
			sendto_umode(UMODE_NETADMIN | UMODE_TECHADMIN,
			    "*** Invisible(+I) user %s joined %s", sptr->name,
			    chptr->chname);
		}
		sendto_serv_butone_token(cptr, parv[0], MSG_JOIN,
		    TOK_JOIN, name);

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
				    name, chptr->creationtime);
			}
			del_invite(sptr, chptr);
			if (flags & CHFL_CHANOP)
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE,
				    "%s +o %s %lu",
				    name, parv[0], chptr->creationtime);
			if (chptr->topic)
			{
				sendto_one(sptr, rpl_str(RPL_TOPIC),
				    me.name, parv[0], name, chptr->topic);
				sendto_one(sptr,
				    rpl_str(RPL_TOPICWHOTIME), me.name,
				    parv[0], name, chptr->topic_nick,
				    chptr->topic_time);
			}
			parv[1] = name;
			(void)m_names(cptr, sptr, 2, parv);
			bouncedtimes = 0;
		}

	}
	return 0;
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
int  m_join(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	static char jbuf[BUFSIZE];
	Link *lp;
	aChannel *chptr;
	char *name, *key = NULL, *link = NULL;
	int  i, flags = 0;
	char *p = NULL, *p2 = NULL;

	bouncedtimes = 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "JOIN");
		return 0;
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
		if (*name == '&')
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
			(void)strcat(jbuf, ",");
		(void)strncat(jbuf, name, sizeof(jbuf) - i - 1);
		i += strlen(name) + 1;
	}
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
				chptr = lp->value.chptr;
				sendto_channel_butserv(chptr, sptr,
				    PartFmt2, parv[0], chptr->chname,
				    "Left all channels");
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
					return 0;
				}
/* RESTRICTCHAN */
			if (crlines)
			{
				if (channel_canjoin(sptr, name) != 1)
				{
					if (cannotjoin_msg)
						sendto_one(sptr,
						    ":%s NOTICE %s :%s",
						    me.name, sptr->name,
						    cannotjoin_msg);
					return 0;
				}
			}
		}

		chptr = get_channel(sptr, name, CREATE);
		if (chptr && (lp = find_user_link(chptr->members, sptr)))
			continue;

		if (!MyConnect(sptr))
			flags = CHFL_DEOPPED;
		if (sptr->flags & FLAGS_TS8)
			flags |= CHFL_SERVOPOK;

		if (!chptr ||
		    (MyConnect(sptr)
		    && (i = can_join(cptr, sptr, chptr, key, link, parv))))
		{

			if (i != -1)
				sendto_one(sptr, err_str(i),
				    me.name, parv[0], name);

			/* uhm? was *chptr ??? *NULL = dangerous */
			continue;

		}
		if (is_banned(cptr, sptr, chptr) && IsOper(sptr))
		{
			sendto_umode(UMODE_EYES,
			    "*** Banwalk [IRCop: %s] [Channel: %s]",
			    sptr->name, chptr->chname);
		}
		/*
		   **  Complete user entry to the new channel (if any)
		 */
		add_user_to_channel(chptr, sptr, flags);
		/*
		   ** notify all other users on the new channel
		 */
		if (!(IsHiding(sptr)))
			sendto_channel_butserv(chptr, sptr,
			    ":%s JOIN :%s", parv[0], chptr->chname);
		else
		{
			if (MyClient(sptr))
				sendto_one(sptr, ":%s!%s@%s JOIN :%s",
				    sptr->name, sptr->user->username,
				    (IsHidden(sptr) ? sptr->
				    user->virthost : sptr->user->realhost),
				    chptr->chname);
			if (MyClient(sptr))
			{
				sendto_umode(UMODE_ADMIN,
				    "*** [+I] %s invisible joined %s",
				    sptr->name, chptr->chname);
				sendto_serv_butone_token(&me, me.name,
				    MSG_SMO, TOK_SMO,
				    "A :[+I] %s invisible joined %s",
				    sptr->name, chptr->chname);
			}
		}

		sendto_serv_butone_token(cptr, parv[0], MSG_JOIN,
		    TOK_JOIN, "%s", chptr->chname);

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
				sendto_serv_butone_token(cptr, me.name,
				    MSG_MODE, TOK_MODE, "%s +o %s %lu",
				    chptr->chname, parv[0],
				    chptr->creationtime);
			if (chptr->topic)
			{
				sendto_one(sptr, rpl_str(RPL_TOPIC),
				    me.name, parv[0], name, chptr->topic);
				sendto_one(sptr,
				    rpl_str(RPL_TOPICWHOTIME), me.name,
				    parv[0], name, chptr->topic_nick,
				    chptr->topic_time);
			}
			parv[1] = chptr->chname;
			(void)m_names(cptr, sptr, 2, parv);
		}

	}

	bouncedtimes = 0;
	return 0;
}


/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
int  m_part(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aChannel *chptr;
	Link *lp;
	char *p = NULL, *name;
	register char *comment = (parc > 2 && parv[2]) ? parv[2] : NULL;

	sptr->flags &= ~FLAGS_TS8;

	if (parc < 2 || parv[1][0] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PART");
		return 0;
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
		if (!(lp = find_user_link(chptr->members, sptr)))
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
		/*
		   **  Remove user from the old channel (if any)
		 */
		if (parc < 3)
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s", chptr->chname);
		else
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s :%s", chptr->chname,
			    comment);

		if (1)
		{
			if (!IsHiding(sptr))
			{
				if (parc < 3)
				{
					sendto_channel_butserv(chptr,
					    sptr, PartFmt, parv[0],
					    chptr->chname);
				}
				else
				{
					sendto_channel_butserv(chptr,
					    sptr, PartFmt2, parv[0],
					    chptr->chname, comment);
				}
			}
			else
			{
				if (MyClient(sptr))
				{
					sendto_umode(UMODE_ADMIN,
					    "*** [+I] %s invisible parted %s",
					    sptr->name, chptr->chname);
					sendto_serv_butone_token(&me,
					    me.name, MSG_SMO, TOK_SMO,
					    "A :[+I] %s invisible parted %s",
					    sptr->name, chptr->chname);
				}
				if (MyClient(sptr))
					/* awful hack .. */
					if (parc < 3)
						sendto_one(sptr,
						    ":%s!%s@%s PART %s",
						    sptr->name,
						    sptr->user->username,
						    (IsHidden(sptr) ?
						    sptr->user->virthost :
						    sptr->user->realhost),
						    chptr->chname);
					else
						sendto_one(sptr,
						    ":%s!%s@%s PART %s :%s",
						    sptr->name,
						    sptr->user->username,
						    (IsHidden(sptr) ?
						    sptr->user->virthost :
						    sptr->user->realhost),
						    chptr->chname, comment);
			}
			remove_user_from_channel(sptr, chptr);
		}
	}
	return 0;
}

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/
int  m_kick(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *who;
	aChannel *chptr;
	int  chasing = 0;
	char *comment, *name, *p = NULL, *user, *p2 = NULL;
	Link *lp, *lp2;


	sptr->flags &= ~FLAGS_TS8;

	if (parc < 3 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KICK");
		return 0;
	}

	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];
	if (strlen(comment) > (size_t)TOPICLEN)
		comment[TOPICLEN] = '\0';

	*nickbuf = *buf = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
			    me.name, parv[0], name);
			continue;
		}
		if (check_channelmask(sptr, cptr, name))
			continue;
		if (!IsServer(cptr)
#ifndef NO_OPEROVERRIDE
		    && !IsOper(sptr)
#endif
		    && !IsULine(cptr, sptr) && !is_chan_op(sptr, chptr)
		    && !is_halfop(sptr, chptr))
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
			continue;
		}

		lp2 = find_user_link(chptr->members, sptr);
		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		{
			if (!(who = find_chasing(sptr, user, &chasing)))
				continue;	/* No such user left! */
			if ((lp = find_user_link(chptr->members, who)))
			{
				if (IsULine(cptr, sptr))
					goto attack;
				/* Hiding patch by }{ */
				if (IsHiding(who))
				{
					sendto_one(sptr,
					    err_str(ERR_NOSUCHNICK),
					    me.name, parv[0], user, name);
					sendto_one(who,
					    ":%s NOTICE %s :*** Hidden: %s tried to kick you from channel %s (%s)",
					    me.name, who->name, parv[0],
					    chptr->chname, comment);
					break;
				}

				if ((chptr->mode.mode & MODE_NOKICKS)
				    && !IsULine(cptr, sptr))
				{
					sendto_one(sptr,
					    ":%s NOTICE %s :*** You cannot kick people on %s",
					    me.name, sptr->name, chptr->chname);
					goto deny;
					continue;
				}

				if (IsOper(sptr))
					if (!is_chan_op(sptr, chptr))
					{
						sendto_umode(UMODE_EYES,
						    "*** OperKick [%s @ %s -> %s (%s)]",
						    sptr->name,
						    chptr->chname,
						    who->name, comment);
						goto attack;
					}	/* is_chan_op */
				if (is_chanprot(who, chptr)
				    || is_chanowner(who, chptr)
				    || IsServices(who))
					if (!IsULine(cptr, sptr) && who != sptr)
					{
						sendto_one(sptr,
						    ":%s NOTICE %s :*** You cannot kick %s from %s because %s is channel protected",
						    me.name, sptr->name,
						    who->name,
						    chptr->chname, who->name);
						goto deny;
						continue;
					}	/* chanprot/chanowner */
				if (is_chan_op(who, chptr)
				    && is_halfop(sptr, chptr)
				    && !is_chan_op(sptr, chptr)
				    && !IsULine(cptr, sptr))
				{
					sendto_one(sptr,
					    ":%s NOTICE %s :*** You cannot kick channel operators on %s if you only are halfop",
					    me.name, sptr->name, chptr->chname);
					goto deny;
				}	/* halfop */

				if (IsKix(who) && !IsULine(cptr, sptr))
				{
					if (!(IsNetAdmin(sptr)
					    || IsTechAdmin(sptr)))
					{
						sendto_one(sptr,
						    ":%s NOTICE %s :*** Cannot kick %s from channel %s (usermode +q)",
						    me.name, sptr->name,
						    who->name, chptr->chname);
						sendto_one(who,
						    ":%s NOTICE %s :*** Q: %s tried to kick you from channel %s (%s)",
						    me.name, who->name,
						    parv[0],
						    chptr->chname, comment);
						goto deny;
					}
				}

				goto attack;

			      deny:
				sendto_one(sptr,
				    err_str(ERR_ATTACKDENY), me.name,
				    parv[0], chptr->chname, user);
				continue;

			      attack:
				if (lp)
					sendto_channel_butserv(chptr,
					    sptr, ":%s KICK %s %s :%s",
					    parv[0], name, who->name, comment);
				sendto_serv_butone_token(cptr, parv[0],
				    MSG_KICK, TOK_KICK, "%s %s :%s",
				    name, who->name, comment);
				if (lp)
				{
					remove_user_from_channel(who, chptr);
				}
			}
			else if (MyClient(sptr))
				sendto_one(sptr,
				    err_str(ERR_USERNOTINCHANNEL),
				    me.name, parv[0], user, name);
			if (!IsServer(cptr) || !IsULine(cptr, sptr))
				break;
		}		/* loop on parv[2] */
		if (!IsServer(cptr) || !IsULine(cptr, sptr))
			break;
	}			/* loop on parv[1] */

	return 0;
}


/*
** m_topic
**	parv[0] = sender prefix
**	parv[1] = topic text
**
**	For servers using TS: (Lefler)
**	parv[0] = sender prefix
**	parv[1] = channel name
**	parv[2] = topic nickname
**	parv[3] = topic time
**	parv[4] = topic text
*/
int  m_topic(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aChannel *chptr = NullChn;
	char *topic = NULL, *name, *p = NULL, *tnick = NULL;
	time_t ttime = 0;
	int  topiClen = 0;
	int  nicKlen = 0;

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "TOPIC");
		return 0;
	}
	name = parv[1];

	if (name && IsChannelName(name))
	{
		chptr = find_channel(parv[1], NullChn);
		if (!chptr)
		{
			if (!MyClient(sptr) && !IsULine(cptr, sptr))
			{
				sendto_realops
				    ("Remote TOPIC for unknown channel %s (%s)",
				    parv[1], backupbuf);
			}
			sendto_one(sptr, rpl_str(ERR_NOSUCHCHANNEL),
			    me.name, parv[0], name);
			return 0;
		}
		if (parc > 2)
		{
			if (!IsMember(sptr, chptr) && !IsServer(sptr)
			    && !IsULine(cptr, sptr))
			{
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
				    me.name, parv[0], name);
				return 0;
			}
			topic = parv[2];
		}
		if (parc > 4)
		{
			tnick = parv[2];
			ttime = atol(parv[3]);
			topic = parv[4];

		}

		if (!topic)	/* only asking  for topic  */
		{
			if (!chptr->topic)
				sendto_one(sptr, rpl_str(RPL_NOTOPIC),
				    me.name, parv[0], chptr->chname);
			else
			{
				sendto_one(sptr, rpl_str(RPL_TOPIC),
				    me.name, parv[0],
				    chptr->chname, chptr->topic);
				sendto_one(sptr,
				    rpl_str(RPL_TOPICWHOTIME), me.name,
				    parv[0], chptr->chname,
				    chptr->topic_nick, chptr->topic_time);
			}
		}
		else if (ttime && topic && (IsServer(sptr)
		    || IsULine(cptr, sptr)))
		{
			if (!chptr->topic_time || ttime < chptr->topic_time)
			{
				/* setting a topic */
				topiClen = strlen(topic);
				nicKlen = strlen(tnick);

				if (chptr->topic)
					MyFree(chptr->topic);

				if (topiClen > (TOPICLEN))
					topiClen = TOPICLEN;

				if (nicKlen > (NICKLEN))
					nicKlen = NICKLEN;

				chptr->topic = MyMalloc(topiClen + 1);
				strncpyzt(chptr->topic, topic, topiClen + 1);

				if (chptr->topic_nick)
					MyFree(chptr->topic_nick);

				chptr->topic_nick = MyMalloc(nicKlen + 1);
				strncpyzt(chptr->topic_nick, tnick,
				    nicKlen + 1);

				chptr->topic_time = ttime;
				sendto_serv_butone_token
				    (cptr, parv[0], MSG_TOPIC,
				    TOK_TOPIC, "%s %s %lu :%s",
				    chptr->chname, chptr->topic_nick,
				    chptr->topic_time, chptr->topic);
				sendto_channel_butserv(chptr, sptr,
				    ":%s TOPIC %s :%s (%s)", parv[0],
				    chptr->chname, chptr->topic,
				    chptr->topic_nick);
			}
		}
		else if (((chptr->mode.mode & MODE_TOPICLIMIT) == 0 ||
		    is_chan_op(sptr, chptr)) || IsOper(sptr)
		    || IsULine(cptr, sptr) || is_halfop(sptr, chptr) && topic)
		{
			/* setting a topic */
			if (IsOper(sptr) && !(is_halfop(sptr, chptr)
			    || IsULine(cptr, sptr)
			    || is_chan_op(sptr, chptr))
			    && (chptr->mode.mode & MODE_TOPICLIMIT))
			{
#ifdef NO_OPEROVERRIDE
				continue;
#endif
				sendto_umode(UMODE_EYES,
				    "*** OperTopic [IRCop: %s] - [Channel: %s] - [Topic: %s]",
				    sptr->name, chptr->chname, topic);
			}
			/* setting a topic */
			topiClen = strlen(topic);
			nicKlen = strlen(sptr->name);

			if (chptr->topic)
				MyFree(chptr->topic);

			if (topiClen > (TOPICLEN))
				topiClen = TOPICLEN;

			if (nicKlen > (NICKLEN))
				nicKlen = NICKLEN;

			chptr->topic = MyMalloc(topiClen + 1);
			strncpyzt(chptr->topic, topic, topiClen + 1);

			if (chptr->topic_nick)
				MyFree(chptr->topic_nick);

			chptr->topic_nick = MyMalloc(nicKlen + 1);
			strncpyzt(chptr->topic_nick, sptr->name, nicKlen + 1);

			if (ttime && IsServer(cptr))
				chptr->topic_time = ttime;
			else
				chptr->topic_time = TStime();
			sendto_serv_butone_token
			    (cptr, parv[0], MSG_TOPIC, TOK_TOPIC,
			    "%s %s %lu :%s",
			    chptr->chname,
			    chptr->topic_nick, chptr->topic_time, chptr->topic);
			sendto_channel_butserv(chptr, sptr,
			    ":%s TOPIC %s :%s", parv[0], chptr->chname,
			    chptr->topic);
		}
		else
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
	}
	return 0;
}

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
int  m_invite(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aClient *acptr;
	aChannel *chptr;


	if (parc < 3 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "INVITE");
		return -1;
	}

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
		    me.name, parv[0], parv[1]);
		return 0;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[2]);
	if (check_channelmask(sptr, cptr, parv[2]))
		return 0;
	if (!(chptr = find_channel(parv[2], NullChn)))
	{

		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s",
		    parv[0], parv[1], parv[2]);
		return 0;
	}
	if (chptr->mode.mode & MODE_NOINVITE)
		if (!IsULine(cptr, sptr))
		{
			sendto_one(sptr, err_str(ERR_NOINVITE),
			    me.name, parv[0], parv[2]);
			return -1;
		}
	if (chptr && !IsMember(sptr, chptr) && !IsULine(cptr, sptr))
	{
		sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
		    me.name, parv[0], parv[2]);
		return -1;
	}

	if (IsMember(acptr, chptr))
	{
		sendto_one(sptr, err_str(ERR_USERONCHANNEL),
		    me.name, parv[0], parv[1], parv[2]);
		return 0;
	}
	if (chptr && (chptr->mode.mode & MODE_INVITEONLY))
	{
		if (!is_chan_op(sptr, chptr) && !IsULine(cptr, sptr))
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
			return -1;
		}
		else if (!IsMember(sptr, chptr) && !IsULine(cptr, sptr))
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0],
			    ((chptr) ? (chptr->chname) : parv[2]));
			return -1;
		}
	}

	if (MyConnect(sptr))
	{
		if (check_for_target_limit(sptr, acptr, acptr->name))
			return 0;

		sendto_one(sptr, rpl_str(RPL_INVITING), me.name,
		    parv[0], acptr->name,
		    ((chptr) ? (chptr->chname) : parv[2]));
		if (acptr->user->away)
			sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
			    parv[0], acptr->name, acptr->user->away);
	}
	/* Note: is_banned() here will cause some extra CPU load, 
	 *       and we're really only relying on the existence
	 *       of the limit because we could momentarily have
	 *       less people on channel.
	 */
	if (MyConnect(acptr))
		if (chptr && sptr->user
		    && (is_banned(acptr, sptr, chptr)
		    || (chptr->mode.mode & MODE_INVITEONLY)
		    || chptr->mode.limit) && (is_chan_op(sptr, chptr)
		    || IsULine(cptr, sptr)))
		{
			add_invite(acptr, chptr);
			sendto_channelops_butone(NULL, &me, chptr,
			    ":%s NOTICE @%s :%s invited %s into the channel.",
			    me.name, chptr->chname, sptr->name, acptr->name);
		}
	sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s", parv[0],
	    acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
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
	LOpts *lopt = cptr->lopt;
	int  hashnum;

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
				if ((!lopt->showall)
				    && ((chptr->users < lopt->usermin)
				    || ((lopt->usermax >= 0)
				    && (chptr->users > lopt->usermax))
				    || ((chptr->creationtime
				    || 1) <= lopt->chantimemin)
				    || (chptr->topic_time <
				    lopt->topictimemin)
				    || (chptr->creationtime >=
				    lopt->chantimemax)
				    || (chptr->topic_time >
				    lopt->topictimemax)))
					continue;

				if (lopt->nolist &&
				    (find_str_match_link(&
				    (lopt->nolist), chptr->chname)
				    || (chptr->topic ?
				    find_str_match_link(&(lopt->nolist),
				    chptr->topic) : 0)))
					continue;
				if (lopt->yeslist &&
				    (!find_str_match_link(&
				    (lopt->yeslist), chptr->chname)
				    &&
				    !find_str_match_link(&
				    (lopt->yeslist), chptr->topic)))
					continue;
				if (!IsAnOper(cptr))
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name,
					    ShowChannel(cptr,
					    chptr) ? chptr->chname :
					    "*", chptr->users,
					    ShowChannel(cptr,
					    chptr) ? (chptr->topic ?
					    chptr->topic : "") : "");
				else
					sendto_one(cptr,
					    rpl_str(RPL_LIST), me.name,
					    cptr->name, chptr->chname,
					    chptr->users,
					    (chptr->topic ? chptr->topic : ""));
				numsend--;
			}
		else
			break;
	}

	/* All done */
	if (hashnum == CH_MAX);
	{
		Link *lp, *next;
		sendto_one(cptr, rpl_str(RPL_LISTEND), me.name, cptr->name);
		for (lp = lopt->yeslist; lp; lp = next)
		{
			next = lp->next;
			free_link(lp);
		}
		for (lp = lopt->nolist; lp; lp = next)
		{
			next = lp->next;
			free_link(lp);
		}

		MyFree(cptr->lopt);
		cptr->lopt = NULL;
		return;
	}

	/* 
	 * We've exceeded the limit on the number of channels to send back
	 * at once.
	 */
	lopt->starthash = hashnum;
	return;
}


int  check_for_chan_flood(cptr, sptr, chptr)
	aClient *cptr, *sptr;
	aChannel *chptr;
{
	Link *lp;

	if (!MyClient(sptr))
		return 0;
	if (IsOper(sptr) || IsULine(cptr, sptr))
		return 0;
	if (is_chan_op(sptr, chptr))
		return 0;

	if (!(lp = find_user_link(chptr->members, sptr)))
		return 0;

	if ((chptr->mode.msgs < 1) || (chptr->mode.per < 1))
		return 0;

	/* Theory here is 
	   If current - lastmsgtime <= mode.per
	   and nummsg is higher than mode.msgs
	   then kick 
	 */

	if ((TStime() - (lp->flood->lastmsg)) >=	/* current - lastmsgtime */
	    chptr->mode.per)	/* mode.per */
	{
		/* reset the message counter */
		lp->flood->lastmsg = TStime();
		lp->flood->nmsg = 1;
		return 0;	/* forget about it.. */
	}

	/* increase msgs */
	lp->flood->nmsg++;
	lp->flood->lastmsg = TStime();

	if ((lp->flood->nmsg) > chptr->mode.msgs)
	{
		char comment[1024], mask[1024];

		ircsprintf(comment,
		    "Flooding (Limit is %i lines per %i seconds)",
		    chptr->mode.msgs, chptr->mode.per);
		if (chptr->mode.kmode == 1)
		{		/* ban. */
			ircsprintf(mask, "*!*@%s",
			    (IsHidden(sptr) ? sptr->user->
			    virthost : sptr->user->realhost));
			add_banid(&me, chptr, mask);
			sendto_serv_butone(&me, ":%s MODE %s +b %s 0",
			    me.name, chptr->chname, mask);
			sendto_channel_butserv(chptr, &me,
			    ":%s MODE %s +b %s", me.name, chptr->chname, mask);
		}
		sendto_channel_butserv(chptr, &me,
		    ":%s KICK %s %s :%s", me.name,
		    chptr->chname, sptr->name, comment);
		sendto_serv_butone(cptr, ":%s KICK %s %s :%s",
		    me.name, chptr->chname, sptr->name, comment);
		remove_user_from_channel(sptr, chptr);
		return 1;
	}
}

/* Originally from bahamut, modified a bit for Unreal by codemastr
 * also Opers can now see +s channels and can still use /list while
 * HTM is active -- codemastr */

/*
 * * m_list *      parv[0] = sender prefix *      parv[1] = channel
 */
int  m_list(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aChannel *chptr;
	time_t currenttime = time(NULL);
	char *name, *p = NULL;
	LOpts *lopt = NULL;
	Link *lp, *next;
	int  usermax, usermin, error = 0, doall = 0;
	time_t chantimemin, chantimemax;
	time_t topictimemin, topictimemax;
	Link *yeslist = NULL, *nolist = NULL;

	static char *usage[] = {
		"   Usage: /LIST <options>",
		"",
		"If you don't include any options, the default is to send you the",
		"entire unfiltered list of channels. Below are the options you can",
		"use, and what channels LIST will return when you use them.",
		">number  List channels with more than <number> people.",
		"<number  List channels with less than <number> people.",
		"C>number List channels created between now and <number> minutes ago.",
		"C<number List channels created earlier than <number> minutes ago.",
		"T>number List channels whose topics are older than <number> minutes",
		"         (Ie, they have not changed in the last <number> minutes.",
		"T<number List channels whose topics are not older than <number> minutes.",
		"*mask*   List channels that match *mask*",
		"!*mask*  List channels that do not match *mask*",
		NULL
	};

	/* Some starting san checks -- No interserver lists allowed. */
	if (cptr != sptr || !sptr->user)
		return 0;

	/* If a /list is in progress, then another one will cancel it */
	if ((lopt = sptr->lopt) != NULL)
	{
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		for (lp = lopt->yeslist; lp; lp = next)
		{
			next = lp->next;
			free_link(lp);
		}
		for (lp = lopt->nolist; lp; lp = next)
		{
			next = lp->next;
			free_link(lp);
		}
		MyFree(sptr->lopt);
		sptr->lopt = NULL;
		return 0;
	}

	/* if HTM, drop this too */
	if (lifesux && !IsOper(cptr))
	{
		sendto_one(sptr, err_str(ERR_HTMDISABLED), me.name,
		    sptr->name, "/List");
		/* We need this for mIRC compatibility -- codemastr */
		sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);
		return 0;
	}

	if (parc < 2 || BadPtr(parv[1]))
	{

		sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);
		lopt = sptr->lopt = (LOpts *) MyMalloc(sizeof(LOpts));
		memset(lopt, '\0', sizeof(LOpts));

		lopt->showall = 1;

		if (DBufLength(&cptr->sendQ) < 2048)
			send_list(cptr, 64);

		return 0;
	}

	if ((parc == 2) && (parv[1][0] == '?') && (parv[1][1] == '\0'))
	{
		char **ptr = usage;
		for (; *ptr; ptr++)
			sendto_one(sptr, rpl_str(RPL_LISTSYNTAX),
			    me.name, cptr->name, *ptr);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTSTART), me.name, parv[0]);

	chantimemax = topictimemax = currenttime + 86400;
	chantimemin = topictimemin = 0;
	usermin = 1;		/* Minimum of 1 */
	usermax = -1;		/* No maximum */

	for (name = strtoken(&p, parv[1], ","); name && !error;
	    name = strtoken(&p, (char *)NULL, ","))
	{

		switch (*name)
		{
		  case '<':
			  usermax = atoi(name + 1) - 1;
			  doall = 1;
			  break;
		  case '>':
			  usermin = atoi(name + 1) + 1;
			  doall = 1;
			  break;
		  case 'C':
		  case 'c':	/* Channel TS time -- creation time? */
			  ++name;
			  switch (*name++)
			  {
			    case '<':
				    chantimemax = currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    case '>':
				    chantimemin = currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    default:
				    sendto_one(sptr,
					err_str(ERR_LISTSYNTAX),
					me.name, cptr->name,
					"Bad list syntax, type /list ?");
				    error = 1;
			  }
			  break;
		  case 'T':
		  case 't':
			  ++name;
			  switch (*name++)
			  {
			    case '<':
				    topictimemax =
					currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    case '>':
				    topictimemin =
					currenttime - 60 * atoi(name);
				    doall = 1;
				    break;
			    default:
				    sendto_one(sptr,
					err_str(ERR_LISTSYNTAX),
					me.name, cptr->name,
					"Bad list syntax, type /list ?");
				    error = 1;
			  }
			  break;
		  default:	/* A channel, possibly with wildcards.
				 * Thought for the future: Consider turning wildcard
				 * processing on the fly.
				 * new syntax: !channelmask will tell ircd to ignore
				 * any channels matching that mask, and then
				 * channelmask will tell ircd to send us a list of
				 * channels only masking channelmask. Note: Specifying
				 * a channel without wildcards will return that
				 * channel even if any of the !channelmask masks
				 * matches it.
				 */
			  if (*name == '!')
			  {
				  doall = 1;
				  lp = make_link();
				  lp->next = nolist;
				  nolist = lp;
				  DupString(lp->value.cp, name + 1);
			  }
			  else if (strchr(name, '*') || strchr(name, '*'))
			  {
				  doall = 1;
				  lp = make_link();
				  lp->next = yeslist;
				  yeslist = lp;
				  DupString(lp->value.cp, name);
			  }
			  else	/* Just a normal channel */
			  {
				  chptr = find_channel(name, NullChn);
				  if (chptr && ShowChannel(sptr, chptr))
					  sendto_one(sptr,
					      rpl_str(RPL_LIST),
					      me.name, parv[0],
					      ShowChannel(sptr,
					      chptr) ? name : "*",
					      chptr->users,
					      (chptr->topic ? chptr->topic :
					      ""));
			  }
		}		/* switch */
	}			/* while */

	if (doall)
	{
		lopt = sptr->lopt = (LOpts *) MyMalloc(sizeof(LOpts));
		memset(lopt, '\0', sizeof(LOpts));
		lopt->usermin = usermin;
		lopt->usermax = usermax;
		lopt->topictimemax = topictimemax;
		lopt->topictimemin = topictimemin;
		lopt->chantimemax = chantimemax;
		lopt->chantimemin = chantimemin;
		lopt->nolist = nolist;
		lopt->yeslist = yeslist;

		if (DBufLength(&cptr->sendQ) < 2048)
			send_list(cptr, 64);
		return 0;
	}

	sendto_one(sptr, rpl_str(RPL_LISTEND), me.name, parv[0]);

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
int  m_names(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	int  mlen = strlen(me.name) + NICKLEN + 7;
	aChannel *chptr;
	aClient *acptr;
	int  member;
	Link *cm;
	int  idx, flag = 1, spos;
	char *s, *para = parv[1];


	if (parc < 2 || !MyConnect(sptr))
	{
		sendto_one(sptr, rpl_str(RPL_ENDOFNAMES), me.name,
		    parv[0], "*");
		return 0;
	}

	if (parc > 1 &&
	    hunt_server(cptr, sptr, ":%s NAMES %s %s", 2, parc, parv))
		return 0;

	for (s = para; *s; s++)
	{
		if (*s == ',')
		{
			para[TRUNCATED_NAMES] = '\0';
			sendto_realops("names abuser %s %s",
			    get_client_name(sptr, FALSE), para);
			sendto_one(sptr, err_str(ERR_TOOMANYTARGETS),
			    me.name, sptr->name, "NAMES");
			return 0;
		}
	}

	chptr = find_channel(para, (aChannel *)NULL);

	if (!chptr || !ShowChannel(sptr, chptr))
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
		acptr = cm->value.cptr;
		if (IsInvisible(acptr) && !member)
			continue;
		if (IsHiding(acptr))
			continue;
		if (chptr->mode.mode & MODE_AUDITORIUM)
			if (!is_chan_op(sptr, chptr))
				if (!(cm->flags & CHFL_CHANOP))
					continue;

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

void send_user_joins(cptr, user)
	aClient *cptr, *user;
{
	Link *lp;
	aChannel *chptr;
	int  cnt = 0, len = 0, clen;
	char *mask;

	ircsprintf(buf, ":%s %s ", user->name,
	    (IsToken(cptr) ? TOK_JOIN : MSG_JOIN));
	len = strlen(buf);

	for (lp = user->user->channel; lp; lp = lp->next)
	{
		chptr = lp->value.chptr;
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
			ircsprintf(buf, ":%s %s ", user->name,
			    (IsToken(cptr) ? TOK_JOIN : MSG_JOIN));
			len = strlen(buf);
			cnt = 0;
		}
		(void)strcpy(buf + len, chptr->chname);
		cnt++;
		len += clen;
		if (lp->next)
		{
			len++;
			(void)strcat(buf, ",");
		}
	}
	if (*buf && cnt)
		sendto_one(cptr, "%s", buf);

	return;
}


/*
** m_knock
**	parv[0] - sender prefix
**	parv[1] - channel
**	parv[2] - reason
**
** Coded by Stskeeps
** Additional bugfixes/ideas by codemastr
** (C) codemastr & Stskeeps
** 
*/
int  m_knock(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	aChannel *chptr;

	if (parc < 3 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KNOCK");
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);

	if (check_channelmask(sptr, cptr, parv[1]))
		return 0;
	/* bugfix for /knock PRv Please? */
	if (*parv[1] != '#' && *parv[1] != '&')
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name,
		    parv[1], "Remember to use a # prefix in channel name");

		return 0;
	}
	if (!(chptr = find_channel(parv[1], NullChn)))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, parv[1], "Channel does not exist!");
		return 0;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(sptr, chptr) == 1)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "You're already there!");
		return 0;
	}
	if (chptr->mode.mode & MODE_NOKNOCK)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "No knocks are allowed! (+K)");
		return 0;
	}

	if (chptr->mode.mode & MODE_INVITEONLY)
	{
	}
	else
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "Channel is not invite only!");
		return 0;
	}

	if (is_banned(cptr, sptr, chptr))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, chptr->chname, "You're banned!");
		return 0;
	}

	if (chptr->mode.mode & MODE_NOINVITE)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name,
		    chptr->chname, "You can not get invited anyways! (+I)");

		return 0;
	}

	sendto_channelops_butone(NULL, &me, chptr,
	    ":%s NOTICE @%s :[Knock] by %s!%s@%s (%s) ",
	    me.name, chptr->chname, sptr->name,
	    sptr->user->username,
	    (IsHidden(sptr) ? sptr->user->virthost : sptr->user->realhost),
	    parv[2]);

	sendto_one(sptr, ":%s NOTICE %s :Knocked on to %s", me.name,
	    sptr->name, chptr->chname);
	return 0;
}

typedef struct xParv aParv;
struct xParv {
	int  parc;
	char *parv[256];
};

aParv pparv;

aParv *mp2parv(char *xmbuf, char *parmbuf)
{
	int  c;
	char *p;
	pparv.parv[0] = xmbuf;
	c = 1;
	for (p = (char *)strtok((char *)parmbuf, " "); p;
	    p = (char *)strtok((char *)NULL, " "))
	{
		c++;
		pparv.parv[c - 1] = p;
	} pparv.parc = c;
	return (&pparv);
}

/*
   **      m_sjoin  
   **
   **   SJOIN will synch channels and channelmodes using the new STS1 protocol
   **      that is based on the EFnet TS3 protocol.
   **                           -GZ (gz@starchat.net)
   **         
   **  Modified for UnrealIRCd by Stskeeps
   **  Recoded by Stskeeps
   **      parv[0] = sender prefix
   **      parv[1] = channel timestamp
   **      parv[2] = channel name
   ** 	  
   **      if (parc == 3) 
   **		parv[3] = nick names + modes - all in one parameter
   **	   if (parc == 4)
   **		parv[3] = channel modes
   **		parv[4] = nick names + modes - all in one parameter
   **	   if (parc > 4)
   **		parv[3] = channel modes
   **		parv[4 to parc - 2] = mode parameters
   **		parv[parc - 1] = nick names + modes
 */

/* Some ugly macros, but useful */
#define Addit(x,y) modebuf[b] = x; strcat(parabuf, y); \
  strcat(parabuf, " "); \
  if (b == RESYNCMODES) \
  { modebuf[b + 1] = '\0'; sendto_serv_butone_sjoin(cptr, \
  	":%s MODE %s %s %s %lu", sptr->name, chptr->chname,  \
  	modebuf, parabuf, chptr->creationtime); \
  	sendto_channel_butserv(chptr, sptr, \
  	"%s MODE %s %s %s", sptr->name, chptr->chname, \
  	modebuf, parabuf); \
  	parabuf[0] = '\0'; \
  	b = 1; \
  	} else b++

#define Addsingle(x) modebuf[b] = x; b++
#define CheckStatus(x,y) if (modeflags & (y)) { Addit((x), nick); }


int  m_sjoin(cptr, sptr, parc, parv)
	aClient *cptr, *sptr;
	int  parc;
	char *parv[];
{
	unsigned short nopara;
	unsigned short nomode;
	unsigned short removeours;
	unsigned short removetheirs;
	unsigned short merge;	/* same timestamp */
	char pvar[MAXMODEPARAMS][MODEBUFLEN + 3];
	char paraback[1024], modeback[1024];
	char nick[NICKLEN + 1];
	aClient *acptr, *tempptr;
	aChannel *chptr;
	Link *lp;
	Ban *ban;
	aParv *ap;
	int  ts, oldts, pcount, x, y, z, i;
	unsigned short b;
	Mode oldmode;
	char *t;
	long modeflags;

	if (IsClient(sptr) || parc < 3 || !IsServer(sptr))
		return 0;

	if (!IsChannelName(parv[2]))
		return 0;

	merge = nopara = nomode = removeours = removetheirs = 0;

	if (SupportSJOIN(sptr) && !strncmp(parv[4], "<none>", 6))
		nopara = 1;
	if (SupportSJOIN2(sptr) && !strncmp(parv[4], "<->", 6))
		nopara = 1;
	if (SupportSJ3(sptr) && (parc < 5))
		nopara = 1;

	if (SupportSJ3(sptr))
	{
		if (parc < 4)
			nomode = 1;
	}
	else
	{
		if (parv[3][1] == '\0')
			nomode = 1;
	}
	chptr = get_channel(cptr, parv[2], CREATE);

	ts = atol(parv[1]);

	if (chptr->creationtime > ts)
	{
		removeours = 1;
		oldts = chptr->creationtime;
		chptr->creationtime = ts;
	}
	else if ((chptr->creationtime < ts) && (chptr->creationtime != 0))
		removetheirs = 1;
	else if (chptr->creationtime == ts)
		merge = 1;

	if (chptr->creationtime == 0)
	{
		oldts = -1;
		chptr->creationtime = ts;
	}
	else
		oldts = chptr->creationtime;

	if (ts < 750000)
		if (ts != 0)
			sendto_ops
			    ("Warning! Possible desynch: SJOIN for channel %s has a fishy timestamp (%ld)",
			    chptr->chname, ts);
	parabuf[0] = '\0';
	modebuf[0] = '+';
	modebuf[1] = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);
	if (removeours)
	{
		modebuf[0] = '-';
		/* remove our modes if any */
		if (modebuf[1] != '\0')
		{
			int  b;
			Ban *ban;

			ap = mp2parv(modebuf, parabuf);
			set_mode(chptr, cptr, ap->parc, ap->parv, &pcount,
			    pvar, 0);
			sendto_serv_butone_sjoin(cptr,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname, modebuf, parabuf,
			    chptr->creationtime);
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);

		}
		/* remove bans */
		b = 1;
		parabuf[0] = '\0';
		for (ban = chptr->banlist; ban; ban = ban->next)
		{
			Addit('b', ban->banstr);
		}
		for (ban = chptr->exlist; ban; ban = ban->next)
		{
			Addit('e', ban->banstr);
		}
		for (lp = chptr->members; lp; lp = lp->next)
		{
			if (lp->flags & MODE_CHANOWNER)
			{
				lp->flags &= ~MODE_CHANOWNER;
				Addit('q', lp->value.cptr->name);
			}
			if (lp->flags & MODE_CHANPROT)
			{
				lp->flags &= ~MODE_CHANPROT;
				Addit('a', lp->value.cptr->name);
			}
			if (lp->flags & MODE_CHANOP)
			{
				lp->flags &= ~MODE_CHANOP;
				Addit('o', lp->value.cptr->name);
			}
			if (lp->flags & MODE_HALFOP)
			{
				lp->flags &= ~MODE_HALFOP;
				Addit('h', lp->value.cptr->name);
			}
			if (lp->flags & MODE_VOICE)
			{
				lp->flags &= ~MODE_VOICE;
				Addit('v', lp->value.cptr->name);
			}
		}
		if (b > 1)
		{
			modebuf[b] = '\0';
			sendto_serv_butone_sjoin(cptr,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname,
			    modebuf, parabuf, chptr->creationtime);
			sendto_channel_butserv(chptr,
			    sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);

		}
	}
	if (!merge && !removetheirs && !nomode)
	{
		strcpy(modebuf, parv[3]);
		parabuf[0] = '\0';
		if (!nopara)
			for (b = 4; b <= (parc - 2); b++)
			{
				strcat(parabuf, parv[b]);
				strcat(parabuf, " ");
			}
		strcpy(paraback, parabuf);
		ap = mp2parv(modebuf, parabuf);
		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);
		sendto_serv_butone_sjoin(cptr,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, paraback,
		    chptr->creationtime);
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, paraback);
	}
	if (merge && !nomode)
	{
		aCtab *acp;

		bcopy(&chptr->mode, &oldmode, sizeof(oldmode));
		/* merge the modes */
		strcpy(modebuf, parv[3]);
		parabuf[0] = '\0';
		if (!nopara)
			for (b = 4; b <= (parc - 2); b++)
			{
				strcat(parabuf, parv[b]);
				strcat(parabuf, " ");
			}
		strcpy(paraback, parabuf);
		ap = mp2parv(modebuf, parabuf);
		set_mode(chptr, cptr, ap->parc, ap->parv, &pcount, pvar, 0);

		/* Good, now we got modes, now for the differencing and outputting of modes
		   We first see if any para modes are set

		 */
		strcpy(modebuf, "-");
		parabuf[0] = '\0';
		b = 1;
		/* however, is this really going to happen at all? may be unneeded */
		if (oldmode.limit && !chptr->mode.limit)
		{
			Addit('l', (char *)my_itoa(oldmode.limit));
		}
		if (oldmode.key[0] && !chptr->mode.key[0])
		{
			Addit('k', oldmode.key);
		}
		if (oldmode.link[0] && !chptr->mode.link[0])
		{
			Addit('L', oldmode.link);
		}
		if ((oldmode.msgs || oldmode.per || oldmode.kmode)
		    && ((chptr->mode.msgs == 0) && (chptr->mode.per == 0)
		    && (chptr->mode.kmode == 0)))
		{
			ircsprintf(modeback, "%s%i:%i",
			    (oldmode.kmode == 1 ? "*" : ""),
			    oldmode.msgs, oldmode.per);
			Addit('f', modeback);
		}

		for (acp = cFlagTab; acp->mode; acp++)
		{
			if ((oldmode.mode & acp->mode) &&
			    !(chptr->mode.mode & acp->mode))
			{
				Addsingle(acp->flag);
			}
		}
		if (b > 1)
		{
			Addsingle('+');
		}
		else
		{
			strcpy(modebuf, "+");
			b = 1;
		}
		for (acp = cFlagTab; acp->mode; acp++)
		{
			if (!(oldmode.mode & acp->mode) &&
			    (chptr->mode.mode & acp->mode))
			{
				Addsingle(acp->flag);
			}
		}
		/* first we check if it has been set, we did unset longer up */
		if (!oldmode.limit && chptr->mode.limit)
		{
			Addit('l', (char *)my_itoa(chptr->mode.limit));
		}
		if (!oldmode.key[0] && chptr->mode.key[0])
		{
			Addit('k', chptr->mode.key);
		}
		if (!oldmode.link[0] && chptr->mode.link[0])
		{
			Addit('L', chptr->mode.link);
		}
		if (!(oldmode.msgs || oldmode.per || oldmode.kmode)
		    && (chptr->mode.msgs || chptr->mode.per
		    || chptr->mode.kmode))
		{
			ircsprintf(modeback, "%s%i:%i",
			    (chptr->mode.kmode == 1 ? "*" : ""),
			    chptr->mode.msgs, chptr->mode.per);
			Addit('f', modeback);

		}
		/* now, if we had diffent para modes - this loop really could be done better, but */

		/* do we have an difference? */
		if (oldmode.limit && chptr->mode.limit
		    && (oldmode.limit != chptr->mode.limit))
		{
			chptr->mode.limit =
			    MAX(oldmode.limit, chptr->mode.limit);
			if (oldmode.limit != chptr->mode.limit)
			{
				Addit('l', (char *)my_itoa(chptr->mode.limit));
			}
		}
		/* sketch, longest key wins */
		if (oldmode.key[0] && chptr->mode.key[0]
		    && strcmp(oldmode.key, chptr->mode.key))
		{
			x = strlen(oldmode.key);
			y = strlen(chptr->mode.key);
			z = MAX(x, y);
			if (z == x)
			{
				strcpy(chptr->mode.key, oldmode.key);
			}
			else
			{
				Addit('k', chptr->mode.key);
			}
		}
		/* same as above .. */
		if (oldmode.link[0] && chptr->mode.link[0]
		    && strcmp(oldmode.link, chptr->mode.link))
		{
			x = strlen(oldmode.link);
			y = strlen(chptr->mode.link);
			z = MAX(x, y);
			if (z == x)
			{
				strcpy(chptr->mode.link, oldmode.link);
			}
			else
			{
				Addit('L', chptr->mode.link);
			}
		}
		/* 
		 * run a max on each?
		 */
		if ((oldmode.kmode != chptr->mode.kmode)
		    || (oldmode.msgs != chptr->mode.msgs)
		    || (oldmode.per != chptr->mode.per))
		{
			chptr->mode.kmode =
			    MAX(chptr->mode.kmode, oldmode.kmode);
			chptr->mode.msgs = MAX(chptr->mode.msgs, oldmode.msgs);
			chptr->mode.per = MAX(chptr->mode.per, oldmode.per);
			if ((oldmode.kmode != chptr->mode.kmode)
			    || (oldmode.msgs != chptr->mode.msgs)
			    || (oldmode.per != chptr->mode.per))
			{
				ircsprintf(modeback, "%s%i:%i",
				    (chptr->mode.kmode == 1 ? "*" : ""),
				    chptr->mode.msgs, chptr->mode.per);
				Addit('f', modeback);
			}
		}

		Addsingle('\0');

		if (modebuf[1])
		{
			sendto_serv_butone_sjoin(cptr,
			    ":%s MODE %s %s %s %lu",
			    sptr->name, chptr->chname, modebuf, parabuf,
			    chptr->creationtime);
			sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
			    sptr->name, chptr->chname, modebuf, parabuf);
		}
	}
	/* Mode setting done :), now for our beloved clients */
	parabuf[0] = 0;
	modebuf[0] = '+';
	t = parv[parc - 1];
	*(--t) = ' ';		/* set to leading space */
	b = 1;
	strcpy(modebuf, "+");
	while (*t != '\0')
	{
		if (*t == ' ')
		{
			i = 0;
			t++;
			modeflags = 0;
			while (
			    (*t == '@') || (*t == '+') || (*t == '%')
			    || (*t == '*') || (*t == '~') || (*t == '&')
			    || (*t == '"'))
			{
				switch (*(t++))
				{
				  case '@':
					  modeflags |= CHFL_CHANOP;
					  break;
				  case '%':
					  modeflags |= CHFL_HALFOP;
					  break;
				  case '+':
					  modeflags |= CHFL_VOICE;
					  break;
				  case '*':
					  modeflags |= CHFL_CHANOWNER;
					  break;
				  case '~':
					  modeflags |= CHFL_CHANPROT;
					  break;
				  case '&':
					  modeflags |= CHFL_BAN;
					  goto getnick;
					  break;
				  case '"':
					  modeflags |= CHFL_EXCEPT;
					  goto getnick;
					  break;
				}
			}
		      getnick:
			i = 0;
			while ((*t != ' ') && (*t != '\0'))
				nick[i++] = *(t++);	/* get nick */
			nick[i] = '\0';
			if (nick[0] == ' ')
				continue;
			if (nick[0] == '\0')
				continue;
			if (!(modeflags & CHFL_BAN)
			    && !(modeflags & CHFL_EXCEPT))
			{
				if (!(acptr = find_person(nick, NULL)))
				{
					sendto_realops
					    ("Missing user %s in SJOIN for %s from %s (%s)",
					    nick, chptr->chname, sptr->name,
					    backupbuf);
					continue;
				}
				if (acptr->from != sptr)
				{
					/* The client is not from the server, send a kick for it */
					sendto_one(sptr,
					    ":%s KICK %s %s :Fake direction",
					    me.name, acptr->name);
					sendto_ops
					    ("Fake direction from user %s in SJOIN from %s at %s",
					    acptr->from, sptr->name,
					    chptr->chname);
					continue;
				}

				if (removetheirs)
				{
					modeflags = 0;
				}
				add_user_to_channel(chptr, acptr, modeflags);
				if (!IsHiding(acptr))
					sendto_channel_butserv(chptr, acptr,
					    ":%s JOIN :%s", nick,
					    chptr->chname);
				sendto_serv_butone_sjoin(cptr, ":%s JOIN :%s",
				    nick, chptr->chname);

				CheckStatus('q', CHFL_CHANOWNER);
				CheckStatus('a', CHFL_CHANPROT);
				CheckStatus('o', CHFL_CHANOP);
				CheckStatus('h', CHFL_HALFOP);
				CheckStatus('v', CHFL_VOICE);
			}
			else
			{
				if (removetheirs)
					continue;
				if (modeflags & CHFL_BAN)
				{
					add_banid(sptr, chptr, nick);
					Addit('b', nick);
					sendto_serv_butone_token_opt(cptr, OPT_SJOIN2|OPT_NOT_SJ3,
						sptr->name, MSG_MODE, TOK_MODE, "%s +b %s %li", chptr->chname,
							nick, TStime());
				}
				if (modeflags & CHFL_EXCEPT)
				{
					add_exbanid(sptr, chptr, nick);
					Addit('e', nick);
					sendto_serv_butone_token_opt(cptr, OPT_SJOIN2|OPT_NOT_SJ3,
						sptr->name, MSG_MODE, TOK_MODE, "%s +e %s %li", chptr->chname,
							nick, TStime());
				}
			}
		}
	}
	if (modebuf[1])
	{
		sendto_serv_butone_sjoin(cptr,
		    ":%s MODE %s %s %s %lu",
		    sptr->name, chptr->chname, modebuf, parabuf,
		    chptr->creationtime);
		sendto_channel_butserv(chptr, sptr, ":%s MODE %s %s %s",
		    sptr->name, chptr->chname, modebuf, parabuf);
	}
	/* we should be synched by now, */
	if (oldts != -1)
		if (oldts != chptr->creationtime)
			sendto_channel_butserv(chptr, &me, 
			":%s NOTICE %s :*** Notice -- TS for %s changed from %ld to %ld",
			me.name, chptr->chname, chptr->chname,
			oldts, chptr->creationtime);	
        
       
        strcpy(parabuf, "");
        for (i = 1; i <= (parc - 2); i++)
        {
        	strcat(parabuf, parv[i]);
        }

	/* This sends out to SJ3 servers .. */
        sendto_serv_butone_token(cptr, OPT_SJOIN|OPT_SJ3,
        	MSG_SJOIN, TOK_SJOIN, "%s :%s", parabuf, parv[parc - 1]);
        
	/* We strip out & and " here, for SJ2 */
        strcpy(parabuf, ":");
        t = parv[parc - 1];
        ap = mp2parv("*", t);
	for (i = 2; i <= ap->parc; i++)
	{
		if (*ap->parv[i] == '&')
			continue;
		if (*ap->parv[i] == '"')
			continue;
	 	
		strcat(parabuf, ap->parv[i]);
		strcat(parabuf, " ");
	}
	
	if (nomode)
	{
		sendto_serv_butone_token(cptr, OPT_SJOIN|OPT_SJOIN2|OPT_NOT_SJ3,
			MSG_SJOIN, TOK_SJOIN, "%s %s + <-> :%s", parv[1],
				parv[2], parabuf);
		return 0;
	}
	if (nopara)
	{
		sendto_serv_butone_token(cptr, OPT_SJOIN|OPT_SJOIN2|OPT_NOT_SJ3,
			MSG_SJOIN, TOK_SJOIN, "%s %s %s <-> :%s", parv[1],
				parv[2], parv[3], parabuf);
		return 0;
		
	}
	strcpy(paraback, "");
        ap = mp2parv("*", parv[4]);
	for (i = 2; i <= ap->parc; i++)
	{
		strcat(paraback, ap->parv[i]);
		strcat(paraback, " ");
	}
	sendto_serv_butone_token(cptr, OPT_SJOIN|OPT_SJOIN2|OPT_NOT_SJ3,
		MSG_SJOIN, TOK_SJOIN, "%s %s %s %s :%s",
			parv[1], parv[2], parv[3], paraback, parabuf);
	
}


static int send_ban_list(cptr, chname, creationtime, channel)
	aClient *cptr;
	char *chname;
	time_t creationtime;
	aChannel *channel;
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
		if (count == RESYNCMODES)
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
			if (count != RESYNCMODES)
			{
				(void)strcpy(parabuf, name);
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
		if (count == RESYNCMODES)
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
			if (count != RESYNCMODES)
			{
				(void)strcpy(parabuf, name);
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

void send_channel_modes_sjoin(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{

	Link *members;
	Link *lp;
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
			strcpy(parabuf, "<none>");
		else
			strcpy(parabuf, "<->");
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



		name = lp->value.cptr->name;

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


void send_channel_modes_sjoin3(cptr, chptr)
	aClient *cptr;
	aChannel *chptr;
{
	Link *members;
	Link *lp;
	Ban *ban;
	char *name;
	char *bufptr;
	short	nomode, nopara;
	char bbuf[1024];
	int  n = 0;

	if (*chptr->chname != '#')
		return;

	nomode = 0;
	nopara = 1;
	members = chptr->members;

	/* First we'll send channel, channel modes and members and status */

	*modebuf = *parabuf = '\0';
	channel_modes(cptr, modebuf, parabuf, chptr);
	
	if (!modebuf[1])
		nomode = 1;
	if (!(*parabuf))
		nopara = 1;
	
	
	if (nomode)
	{
		ircsprintf(buf, "%s %ld %s :",
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    chptr->creationtime, chptr->chname);
	}
		else
	if (nopara)
	{
		ircsprintf(buf, "%s %ld %s %s",
		    (IsToken(cptr) ? TOK_SJOIN : MSG_SJOIN),
		    chptr->creationtime, chptr->chname, modebuf);

	}
		else
	if (!nopara && !nomode)
	{
		ircsprintf(buf, "%s %ld %s %s %s",
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



		name = lp->value.cptr->name;

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
