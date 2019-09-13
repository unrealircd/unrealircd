/*
 *   IRC - Internet Relay Chat, src/modules/m_kick.c
 *   (C) 2004 The UnrealIRCd Team
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

CMD_FUNC(m_kick);

#define MSG_KICK 	"KICK"	

ModuleHeader MOD_HEADER
  = {
	"kick",
	"5.0",
	"command /kick", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_KICK, m_kick, 3, M_USER|M_SERVER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD()
{
	return MOD_SUCCESS;
}

MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
** m_kick
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/

#ifdef PREFIX_AQ
#define CHFL_ISOP (CHFL_CHANOWNER|CHFL_CHANADMIN|CHFL_CHANOP)
#else
#define CHFL_ISOP (CHFL_CHANOP)
#endif

CMD_FUNC(m_kick)
{
	Client *who;
	Channel *chptr;
	int  chasing = 0;
	char *comment, *name, *p = NULL, *user, *p2 = NULL, *badkick;
	Membership *lp;
	Hook *h;
	int ret;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("KICK");
	MessageTag *mtags;

	if (parc < 3 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "KICK");
		return 0;
	}

	comment = (BadPtr(parv[3])) ? sptr->name : parv[3];

	if (!BadPtr(parv[3]) && (strlen(comment) > iConf.kick_length))
		comment[iConf.kick_length] = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		long sptr_flags = 0;
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		{
			sendnumeric(sptr, ERR_NOSUCHCHANNEL, name);
			continue;
		}
		/* Store "sptr" access flags */
		if (IsUser(sptr))
			sptr_flags = get_access(sptr, chptr);
		if (!IsServer(cptr) && !IsULine(sptr) && !op_can_override("channel:override:kick:no-ops",sptr,chptr,NULL)
		    && !(sptr_flags & CHFL_ISOP) && !(sptr_flags & CHFL_HALFOP))
		{
			sendnumeric(sptr, ERR_CHANOPRIVSNEEDED, chptr->chname);
			continue;
		}

		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		{
			long who_flags;

			if (MyUser(sptr) && (++ntargets > maxtargets))
			{
				sendnumeric(sptr, ERR_TOOMANYTARGETS, user, maxtargets, "KICK");
				break;
			}

			if (!(who = find_chasing(sptr, user, &chasing)))
				continue;	/* No such user left! */
			if (!who->user)
				continue;
			if ((lp = find_membership_link(who->user->channel, chptr)))
			{
				if (IsULine(sptr) || IsServer(sptr))
					goto attack;

				/* Note for coders regarding oper override:
				 * always let a remote kick (=from a user on another server) through or
				 * else we will get desynched. In short this means all the denying should
				 * always contain a && MyUser(sptr) [or sptr!=cptr] and at the end
				 * a remote kick should always be allowed (pass through). -- Syzop
				 */

				/* Store "who" access flags */
				who_flags = get_access(who, chptr);

				badkick = NULL;
				ret = EX_ALLOW;
				for (h = Hooks[HOOKTYPE_CAN_KICK]; h; h = h->next) {
					int n = (*(h->func.intfunc))(sptr, who, chptr, comment, sptr_flags, who_flags, &badkick);

					if (n == EX_DENY)
						ret = n;
					else if (n == EX_ALWAYS_DENY)
					{
						ret = n;
						break;
					}
				}

				if (ret == EX_ALWAYS_DENY)
				{
					if (MyUser(sptr) && badkick)
						sendto_one(sptr, NULL, "%s", badkick); /* send error, if any */

					if (MyUser(sptr))
						continue; /* reject the kick (note: we never block remote kicks) */
				}
				
				if (ret == EX_DENY)
				{
					/* If set it means 'not allowed to kick'.. now check if (s)he can override that.. */
					if (op_can_override("channel:override:kick:no-ops",sptr,chptr,NULL))
					{
						sendto_snomask(SNO_EYES,
							"*** OperOverride -- %s (%s@%s) KICK %s %s (%s)",
							sptr->name, sptr->user->username, sptr->user->realhost,
							chptr->chname, who->name, comment);
						ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) KICK %s %s (%s)",
							sptr->name, sptr->user->username, sptr->user->realhost,
							chptr->chname, who->name, comment);
						goto attack; /* all other checks don't matter anymore (and could cause double msgs) */
					} else {
						/* Not an oper overriding */
						if (MyUser(sptr) && badkick)
							sendto_one(sptr, NULL, "%s", badkick); /* send error, if any */

						continue; /* reject the kick */
					}
				}

				/* we are neither +o nor +h, OR..
				 * we are +h but victim is +o, OR...
				 * we are +h and victim is +h
				 */
				if (op_can_override("channel:override:kick:no-ops",sptr,chptr,NULL))
				{
					if ((!(sptr_flags & CHFL_ISOP) && !(sptr_flags & CHFL_HALFOP)) ||
					    ((sptr_flags & CHFL_HALFOP) && (who_flags & CHFL_ISOP)) ||
					    ((sptr_flags & CHFL_HALFOP) && (who_flags & CHFL_HALFOP)))
					{
						sendto_snomask(SNO_EYES,
						    "*** OperOverride -- %s (%s@%s) KICK %s %s (%s)",
						    sptr->name, sptr->user->username, sptr->user->realhost,
						    chptr->chname, who->name, comment);

						/* Logging Implementation added by XeRXeS */
						ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) KICK %s %s (%s)",
							sptr->name, sptr->user->username, sptr->user->realhost,
							chptr->chname, who->name, comment);

						goto attack;
					}	/* is_chan_op */

				}				
				/* victim is +a or +q, we are not +q */
				if ((who_flags & (CHFL_CHANOWNER|CHFL_CHANADMIN))
					 && !(sptr_flags & CHFL_CHANOWNER)) {
					if (sptr == who)
						goto attack; /* kicking self == ok */
					if (op_can_override("channel:override:kick:owner",sptr,chptr,NULL)) /* (and f*ck local ops) */
					{	/* IRCop kicking owner/prot */
						sendto_snomask(SNO_EYES,
						    "*** OperOverride -- %s (%s@%s) KICK %s %s (%s)",
						    sptr->name, sptr->user->username, sptr->user->realhost,
						    chptr->chname, who->name, comment);

						/* Logging Implementation added by XeRXeS */
						ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) KICK %s %s (%s)",
							sptr->name, sptr->user->username, sptr->user->realhost, 
							chptr->chname, who->name, comment);

						goto attack;
					}
					else if (!IsULine(sptr) && (who != sptr) && MyUser(sptr))
					{
						char errbuf[NICKLEN+25];
						if (who_flags & CHFL_CHANOWNER)
							ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel owner", 
								   who->name);
						else
							ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel admin", 
								   who->name);
						sendnumeric(sptr, ERR_CANNOTDOCOMMAND, "KICK",
							   errbuf);
						goto deny;
					}	/* chanadmin/chanowner */
				}
				
				/* victim is +o, we are +h [operoverride is already taken care of 2 blocks above] */
				if ((who_flags & CHFL_ISOP) && (sptr_flags & CHFL_HALFOP)
				    && !(sptr_flags & CHFL_ISOP) && !IsULine(sptr) && MyUser(sptr))
				{
					char errbuf[NICKLEN+30];
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a channel operator", who->name);
					sendnumeric(sptr, ERR_CANNOTDOCOMMAND, "KICK",
						   errbuf);
					goto deny;
				}

				/* victim is +h, we are +h [operoverride is already taken care of 3 blocks above] */
				if ((who_flags & CHFL_HALFOP) && (sptr_flags & CHFL_HALFOP)
				    && !(sptr_flags & CHFL_ISOP) && MyUser(sptr))
				{
					char errbuf[NICKLEN+15];
					ircsnprintf(errbuf, sizeof(errbuf), "%s is a halfop", who->name);
					sendnumeric(sptr, ERR_CANNOTDOCOMMAND, "KICK",
						   errbuf);
					goto deny;
				}	/* halfop */

				/* allowed (either coz access granted or a remote kick), so attack! */
				goto attack;

			      deny:
				continue;

			      attack:
				if (MyConnect(sptr)) {
					int breakit = 0;
					Hook *h;
					for (h = Hooks[HOOKTYPE_PRE_LOCAL_KICK]; h; h = h->next) {
						if((*(h->func.intfunc))(sptr,who,chptr,comment) > 0) {
							breakit = 1;
							break;
						}
					}
					if (breakit)
						continue;
				}

				mtags = NULL;
				new_message_special(sptr, recv_mtags, &mtags, ":%s KICK %s %s", sptr->name, chptr->chname, who->name);
				/* The same message is actually sent at 5 places below (though max 4 at most) */

				if (MyUser(sptr))
					RunHook6(HOOKTYPE_LOCAL_KICK, cptr, sptr, who, chptr, mtags, comment);
				else
					RunHook6(HOOKTYPE_REMOTE_KICK, cptr, sptr, who, chptr, mtags, comment);

				if (lp)
				{
					if (invisible_user_in_channel(who, chptr))
					{
						/* Send it only to chanops & victim */
						sendto_channel(chptr, sptr, who,
						               CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANADMIN, 0,
						               SEND_LOCAL, mtags,
						               ":%s KICK %s %s :%s",
						               sptr->name, chptr->chname, who->name, comment);

						if (MyUser(who))
						{
							sendto_prefix_one(who, sptr, mtags, ":%s KICK %s %s :%s",
								sptr->name, chptr->chname, who->name, comment);
						}
					} else {
						/* NORMAL */
						sendto_channel(chptr, sptr, NULL, 0, 0, SEND_LOCAL, mtags,
						               ":%s KICK %s %s :%s",
						               sptr->name, chptr->chname, who->name, comment);
					}
				}
				sendto_server(cptr, PROTO_SID, 0, mtags, ":%s KICK %s %s :%s",
				    ID(sptr), chptr->chname, ID(who), comment);
				sendto_server(cptr, 0, PROTO_SID, mtags, ":%s KICK %s %s :%s",
				    sptr->name, chptr->chname, who->name, comment);
				free_message_tags(mtags);
				if (lp)
				{
					remove_user_from_channel(who, chptr);
				}
			}
			else if (MyUser(sptr))
				sendnumeric(sptr, ERR_USERNOTINCHANNEL, user, name);
		}		/* loop on parv[2] */
		if (MyUser(cptr))
			break;
	}			/* loop on parv[1] */

	return 0;
}
