/*
 *   IRC - Internet Relay Chat, src/modules/m_join.c
 *   (C) 2005 The UnrealIRCd Team
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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

/* Forward declarations */
DLLFUNC CMD_FUNC(m_join);
DLLFUNC void _join_channel(aChannel *chptr, aClient *cptr, aClient *sptr, int flags);
DLLFUNC CMD_FUNC(_do_join);
DLLFUNC int _can_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[]);
static int extended_operoverride(aClient *sptr, aChannel *chptr, char *key, int mval, char mchar);
#define MAXBOUNCE   5 /** Most sensible */

/* Externs */
extern MODVAR int spamf_ugly_vchanoverride;
extern int find_invex(aChannel *chptr, aClient *sptr);

/* Local vars */
static int bouncedtimes = 0;

#define MSG_JOIN 	"JOIN"	
#define TOK_JOIN 	"C"	

ModuleHeader MOD_HEADER(m_join)
  = {
	"m_join",
	"$Id$",
	"command /join", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_TEST(m_join)(ModuleInfo *modinfo)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_JOIN_CHANNEL, _join_channel);
	EfunctionAdd(modinfo->handle, EFUNC_DO_JOIN, _do_join);
	EfunctionAdd(modinfo->handle, EFUNC_CAN_JOIN, _can_join);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_INIT(m_join)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_JOIN, TOK_JOIN, m_join, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_join)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_join)(int module_unload)
{
	return MOD_SUCCESS;
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


/** can_join function.
 * This checks whether a user is allowed to join a channel or not.
 * @notes In case of a +L, the user is automatically joined!
 * @return Zero if allowed to join, otherwise an integer like ERR_SECUREONLYCHAN
 *         which maps to err_str(retval), or 0 if allowed to join.
 */
DLLFUNC int _can_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *link, char *parv[])
{
Link *lp;
Ban *banned;
char *err;
int i;
Hook *h;

	for (h = Hooks[HOOKTYPE_CAN_JOIN]; h; h = h->next) 
	{
		i = (*(h->func.intfunc))(sptr,chptr,key,link,parv);
		if (i != 0)
			return i;
	}

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

        if ((chptr->mode.mode & MODE_INVITEONLY) && !find_invex(chptr, sptr))
                return (ERR_INVITEONLYCHAN);

        if ((chptr->mode.limit && chptr->users >= chptr->mode.limit))
                return (ERR_CHANNELISFULL);

        if (banned)
                return (ERR_BANNEDFROMCHAN);

#ifndef NO_OPEROVERRIDE
#ifdef OPEROVERRIDE_VERIFY
        if (IsOper(sptr) && (chptr->mode.mode & MODE_SECRET ||
            chptr->mode.mode & MODE_PRIVATE) && !is_autojoin_chan(chptr->chname))
                return (ERR_OPERSPVERIFY);
#endif
#endif

        return 0;
}

/*
** m_join
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = channel password (key)
*/
DLLFUNC CMD_FUNC(m_join)
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
DLLFUNC void _join_channel(aChannel *chptr, aClient *cptr, aClient *sptr, int flags)
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
			"%B %s :%s%s ", (long)chptr->creationtime, 
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
					cm_putparameter(chptr, Channelmode_Table[i].flag, iConf.modes_on_join.extparams[i]);
					//CmodeParam *p;
					//p = Channelmode_Table[i].put_param(NULL, iConf.modes_on_join.extparams[i]);
					//AddListItem(p, chptr->mode.extmodeparam);
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
		do_cmd(cptr, sptr, "NAMES", 2, parv);
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
DLLFUNC CMD_FUNC(_do_join)
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
				    PARTFMT2, parv[0], chptr->chname,
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
