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

#include "unrealircd.h"

/* Forward declarations */
CMD_FUNC(m_join);
DLLFUNC void _join_channel(aChannel *chptr, aClient *cptr, aClient *sptr, int flags);
CMD_FUNC(_do_join);
DLLFUNC int _can_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *parv[]);
#define MAXBOUNCE   5 /** Most sensible */

/* Externs */
extern MODVAR int spamf_ugly_vchanoverride;
extern int find_invex(aChannel *chptr, aClient *sptr);

/* Local vars */
static int bouncedtimes = 0;

#define MSG_JOIN 	"JOIN"	

ModuleHeader MOD_HEADER(m_join)
  = {
	"m_join",
	"4.0",
	"command /join", 
	"3.2-b8-1",
	NULL 
    };

MOD_TEST(m_join)
{
	MARK_AS_OFFICIAL_MODULE(modinfo);
	EfunctionAddVoid(modinfo->handle, EFUNC_JOIN_CHANNEL, _join_channel);
	EfunctionAdd(modinfo->handle, EFUNC_DO_JOIN, _do_join);
	EfunctionAdd(modinfo->handle, EFUNC_CAN_JOIN, _can_join);
	return MOD_SUCCESS;
}

MOD_INIT(m_join)
{
	CommandAdd(modinfo->handle, MSG_JOIN, m_join, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_join)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_join)
{
	return MOD_SUCCESS;
}

/* This function checks if a locally connected user may join the channel.
 * It also provides an number of hooks where modules can plug in to.
 * Note that the order of checking has been carefully thought of
 * (eg: bans at the end), so don't change it unless you have a good reason
 * to do so -- Syzop.
 */
DLLFUNC int _can_join(aClient *cptr, aClient *sptr, aChannel *chptr, char *key, char *parv[])
{
Link *lp;
Ban *banned;
Hook *h;
int i=0,j=0;

	for (h = Hooks[HOOKTYPE_CAN_JOIN]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(sptr,chptr,key,parv);
		if (i != 0)
			return i;
	}

	for (h = Hooks[HOOKTYPE_OPER_INVITE_BAN]; h; h = h->next)
	{
		j = (*(h->func.intfunc))(sptr,chptr);
		if (j != 0)
			break;
	}

	/* See if we can evade this ban */
	banned = is_banned(sptr, chptr, BANCHK_JOIN);
	if (banned && j == HOOK_DENY)
		return (ERR_BANNEDFROMCHAN);

	for (lp = sptr->user->invited; lp; lp = lp->next)
		if (lp->value.chptr == chptr)
			return 0;

        if (chptr->users >= chptr->mode.limit)
        {
                /* Hmmm.. don't really like this.. and not at this place */
                
                for (h = Hooks[HOOKTYPE_CAN_JOIN_LIMITEXCEEDED]; h; h = h->next) 
                {
                        i = (*(h->func.intfunc))(sptr,chptr,key,parv);
                        if (i != 0)
                                return i;
                }

                /* We later check again for this limit (in case +L was not set) */
        }


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
        if (ValidatePermissionsForPath("override:privsecret",sptr,NULL,chptr,NULL) && (chptr->mode.mode & MODE_SECRET ||
            chptr->mode.mode & MODE_PRIVATE) && !is_autojoin_chan(chptr->chname))
                return (ERR_OPERSPVERIFY);
#endif
#endif

        return 0;
}

/*
** m_join
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
DLLFUNC void _join_channel(aChannel *chptr, aClient *cptr, aClient *sptr, int flags)
{
	Hook *h;
	int i = 0;
	char *parv[] = { 0, 0 };
	/*
	   **  Complete user entry to the new channel (if any)
	 */
	add_user_to_channel(chptr, sptr, flags);

	/*
	  ** Check if we should notify users of new join
	 */
	for (h = Hooks[HOOKTYPE_VISIBLE_IN_CHANNEL]; h; h = h->next)
		{
			i = (*(h->func.intfunc))(sptr,chptr);
			if (i != 0)
				break;
		}

	/*
	   ** notify all other users on the new channel
	 */
	if (i != 0)
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
	
	sendto_server(cptr, 0, PROTO_SJ3, ":%s JOIN :%s", sptr->name, chptr->chname);

	/* I _know_ that the "@%s " look a bit wierd
	   with the space and all .. but its to get around
	   a SJOIN bug --stskeeps */
	sendto_server(cptr, PROTO_SID | PROTO_SJ3, 0, ":%s SJOIN %li %s :%s%s ",
		me.id, chptr->creationtime,
		chptr->chname, chfl_to_sjoin_symbol(flags), ID(sptr));
	sendto_server(cptr, PROTO_SJ3, PROTO_SID, ":%s SJOIN %li %s :%s%s ",
		me.name, chptr->creationtime,
		chptr->chname, chfl_to_sjoin_symbol(flags), sptr->name);

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
			sendto_server(cptr, 0, 0, ":%s MODE %s + %lu",
			    me.name, chptr->chname, chptr->creationtime);
		}
		del_invite(sptr, chptr);
		if (flags && !(flags & CHFL_DEOPPED))
		{
#ifndef PREFIX_AQ
			if ((flags & CHFL_CHANOWNER) || (flags & CHFL_CHANPROT))
			{
				/* +ao / +qo for when PREFIX_AQ is off */
				sendto_server(cptr, 0, PROTO_SJ3, ":%s MODE %s +o%c %s %s %lu",
				    me.name,
				    chptr->chname, chfl_to_chanmode(flags), sptr->name, sptr->name,
				    chptr->creationtime);
			} else {
#endif
				/* +v/+h/+o (and +a/+q if PREFIX_AQ is on) */
				sendto_server(cptr, 0, PROTO_SJ3, ":%s MODE %s +%c %s %lu",
				    me.name,
				    chptr->chname, chfl_to_chanmode(flags), sptr->name,
				    chptr->creationtime);
#ifndef PREFIX_AQ
			}
#endif
		}

		if (chptr->topic)
		{
			sendto_one(sptr, rpl_str(RPL_TOPIC),
			    me.name, sptr->name, chptr->chname, chptr->topic);
			sendto_one(sptr,
			    rpl_str(RPL_TOPICWHOTIME), me.name,
			    sptr->name, chptr->chname, chptr->topic_nick,
			    chptr->topic_time);
		}
		
		/* Set default channel modes (set::modes-on-join).
		 * Set only if it's the 1st user and only if no other modes have been set
		 * already (eg: +P, permanent).
		 */
		if ((chptr->users == 1) && !chptr->mode.mode && !chptr->mode.extmode &&
		    (MODES_ON_JOIN || iConf.modes_on_join.extmodes))
		{
			int i;
			chptr->mode.extmode =  iConf.modes_on_join.extmodes;
			/* Param fun */
			for (i = 0; i <= Channelmode_highest; i++)
			{
				if (!Channelmode_Table[i].flag || !Channelmode_Table[i].paracount)
					continue;
				if (chptr->mode.extmode & Channelmode_Table[i].mode)
				        cm_putparameter(chptr, Channelmode_Table[i].flag, iConf.modes_on_join.extparams[i]);
			}

			chptr->mode.mode = MODES_ON_JOIN;

			*modebuf = *parabuf = 0;
			channel_modes(sptr, modebuf, parabuf, sizeof(modebuf), sizeof(parabuf), chptr);
			/* This should probably be in the SJOIN stuff */
			sendto_server(&me, 0, 0, ":%s MODE %s %s %s %lu",
			    me.name, chptr->chname, modebuf, parabuf, chptr->creationtime);
			sendto_one(sptr, ":%s MODE %s %s %s", me.name, chptr->chname, modebuf, parabuf);
		}

		parv[0] = sptr->name;
		parv[1] = chptr->chname;
		(void)do_cmd(cptr, sptr, "NAMES", 2, parv);

		RunHook4(HOOKTYPE_LOCAL_JOIN, cptr, sptr,chptr,parv);
	} else {
		RunHook4(HOOKTYPE_REMOTE_JOIN, cptr, sptr, chptr, parv); /* (rarely used) */
	}
}

/** User request to join a channel.
 * This routine is normally called from m_join but can also be called from
 * do_join->can_join->link module->do_join if the channel is 'linked' (chmode +L).
 * We therefore use a counter 'bouncedtimes' which is set to 0 in m_join,
 * increased every time we enter this loop and decreased anytime we leave the
 * loop. So be carefull not to use a simple 'return' after bouncedtimes++. -- Syzop
 */
CMD_FUNC(_do_join)
{
	char jbuf[BUFSIZE];
	Membership *lp;
	aChannel *chptr;
	char *name, *key = NULL;
	int  i, flags = 0, ishold;
	char *p = NULL, *p2 = NULL;
	aTKline *tklban;

#define RET(x) { bouncedtimes--; return x; }

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "JOIN");
		return 0;
	}
	bouncedtimes++;
	/* don't use 'return x;' but 'RET(x)' from here ;p */

	if (bouncedtimes > MAXBOUNCE)
	{
		/* bounced too many times. yeah.. should be in the link module, I know.. then again, who cares.. */
		sendnotice(sptr,
		    "*** Couldn't join %s ! - Link setting was too bouncy",
		    parv[1]);
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
		if (*name == '0' && !atoi(name))
		{
			(void)strcpy(jbuf, "0");
			i = 1;
			continue;
		}
		else if (!IsChannelName(name))
		{
			if (MyClient(sptr))
				sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL), me.name, sptr->name, name);
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
				    PARTFMT2, sptr->name, chptr->chname,
				    "Left all channels");
				if (MyConnect(sptr))
					RunHook4(HOOKTYPE_LOCAL_PART, cptr, sptr, chptr, "Left all channels");
				remove_user_from_channel(sptr, chptr);
			}
			sendto_server(cptr, 0, 0, ":%s JOIN 0", sptr->name);
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
			    (ChannelExists(name)) ? CHFL_DEOPPED : LEVEL_ON_JOIN;

			if (!ValidatePermissionsForPath("immune:channellimit",sptr,NULL,NULL,NULL))	/* opers can join unlimited chans */
				if (sptr->user->joined >= MAXCHANNELSPERUSER)
				{
					sendto_one(sptr,
					    err_str
					    (ERR_TOOMANYCHANNELS),
					    me.name, sptr->name, name);
					RET(0)
				}
/* RESTRICTCHAN */
			if (conf_deny_channel)
			{
				if (!ValidatePermissionsForPath("immune:forbiddenchan",sptr,NULL,NULL,NULL))
				{
					ConfigItem_deny_channel *d;
					if ((d = Find_channel_allowed(cptr, name)))
					{
						if (d->warn)
						{
							sendto_snomask(SNO_EYES, "*** %s tried to join forbidden channel %s",
								get_client_name(sptr, 1), name);
						}
						if (d->reason)
							sendto_one(sptr, err_str(ERR_FORBIDDENCHANNEL), me.name, sptr->name, name, d->reason);
						if (d->redirect)
						{
							sendnotice(sptr, "*** Redirecting you to %s", d->redirect);
							parv[0] = sptr->name;
							parv[1] = d->redirect;
							do_join(cptr, sptr, 2, parv);
						}
						if (d->class)
							sendnotice(sptr, "*** Can not join %s: Your class is not allowed", name);
						continue;
					}
				}
			}
			if (ValidatePermissionsForPath("immune:forbiddenchan",sptr,NULL,NULL,NULL) && (tklban = find_qline(sptr, name, &ishold)))
			{
				sendto_one(sptr, err_str(ERR_FORBIDDENCHANNEL), me.name, sptr->name, name, tklban->reason);
				continue;
			}
			/* ugly set::spamfilter::virus-help-channel-deny hack.. */
			if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
			    !strcasecmp(name, SPAMFILTER_VIRUSCHAN) &&
			    !ValidatePermissionsForPath("immune:viruschan",sptr,NULL,NULL,NULL) && !spamf_ugly_vchanoverride)
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
			   (i = can_join(cptr, sptr, chptr, key, parv)))
			{
				if (i != -1)
				{
					sendto_one(sptr, err_str(i), me.name, sptr->name, name);
				}
				continue;
			}
		}

		join_channel(chptr, cptr, sptr, flags);
	}
	RET(0)
#undef RET
}
