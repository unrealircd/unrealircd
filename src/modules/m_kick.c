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

DLLFUNC int m_kick(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_KICK 	"KICK"	
#define TOK_KICK 	"H"	

ModuleHeader MOD_HEADER(m_kick)
  = {
	"m_kick",
	"$Id$",
	"command /kick", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_kick)(ModuleInfo *modinfo)
{
	add_Command(MSG_KICK, TOK_KICK, m_kick, 3);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_kick)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_kick)(int module_unload)
{
	if (del_Command(MSG_KICK, TOK_KICK, m_kick) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_kick).name);
	}
	return MOD_SUCCESS;
}

/*
** m_kick
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = client to kick
**	parv[3] = kick comment
*/

#ifdef PREFIX_AQ
#define CHFL_ISOP (CHFL_CHANOWNER|CHFL_CHANPROT|CHFL_CHANOP)
#else
#define CHFL_ISOP (CHFL_CHANOP)
#endif

CMD_FUNC(m_kick)
{
	aClient *who;
	aChannel *chptr;
	int  chasing = 0;
	char *comment, *name, *p = NULL, *user, *p2 = NULL;
	Membership *lp;

	if (parc < 3 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KICK");
		return 0;
	}

	comment = (BadPtr(parv[3])) ? parv[0] : parv[3];

	if (strlen(comment) > (size_t)TOPICLEN)
		comment[TOPICLEN] = '\0';

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		long sptr_flags = 0;
		chptr = get_channel(sptr, name, !CREATE);
		if (!chptr)
		{
			sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
			    me.name, parv[0], name);
			continue;
		}
		if (check_channelmask(sptr, cptr, name))
			continue;
		/* Store "sptr" access flags */
		if (IsPerson(sptr))
			sptr_flags = get_access(sptr, chptr);
		if (!IsServer(cptr) && !IsULine(sptr) && !op_can_override(sptr)
		    && !(sptr_flags & CHFL_ISOP) && !(sptr_flags & CHFL_HALFOP))
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			    me.name, parv[0], chptr->chname);
			continue;
		}

		for (; (user = strtoken(&p2, parv[2], ",")); parv[2] = NULL)
		{
			long who_flags;
			if (!(who = find_chasing(sptr, user, &chasing)))
				continue;	/* No such user left! */
			if (!who->user)
				continue;
			if ((lp = find_membership_link(who->user->channel, chptr)))
			{
				if (IsULine(sptr) || IsServer(sptr))
					goto attack;

				/* Note for coders regarding oper override:
				 * always let a remote kick (=from a user on another server) trough or
				 * else we will get desynched. In short this means all the denying should
				 * always contain a && MyClient(sptr) [or sptr!=cptr] and at the end
				 * a remote kick should always be allowed (pass trough). -- Syzop
				 */

				/* applies to everyone (well except remote/ulines :p) */
				if (IsKix(who) && !IsULine(sptr) && MyClient(sptr))
				{
					if (!IsNetAdmin(sptr))
					{
						char errbuf[NICKLEN+10];
						ircsprintf(errbuf, "%s is +q", who->name);
						sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND), 
							   me.name, sptr->name, "KICK", 
							   errbuf);
						sendto_one(who,
						    ":%s %s %s :*** Q: %s tried to kick you from channel %s (%s)",
						    me.name, IsWebTV(who) ? "PRIVMSG" : "NOTICE", who->name,
						    parv[0],
						    chptr->chname, comment);
						goto deny;
					}
				}

				if (chptr->mode.mode & MODE_NOKICKS)
				{
					if (!op_can_override(sptr))
					{
						if (!MyClient(sptr))
							goto attack; /* lag? yes.. kick crossing +Q... allow */
						sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND),
							   me.name, sptr->name, "KICK",
							   "channel is +Q");
						goto deny;
					}
					sendto_snomask(SNO_EYES,
						"*** OperOverride -- %s (%s@%s) KICK %s %s (%s)",
						sptr->name, sptr->user->username, sptr->user->realhost,
						chptr->chname, who->name, comment);
					ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) KICK %s %s (%s)",
						sptr->name, sptr->user->username, sptr->user->realhost,
						chptr->chname, who->name, comment);
					goto attack; /* No reason to continue.. */
				}
				/* Store "who" access flags */
				who_flags = get_access(who, chptr);
				/* we are neither +o nor +h, OR..
				 * we are +h but victim is +o, OR...
				 * we are +h and victim is +h
				 */
				if (op_can_override(sptr))
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
				if ((who_flags & (CHFL_CHANOWNER|CHFL_CHANPROT) || IsServices(who))
					 && !(sptr_flags & CHFL_CHANOWNER)) {
					if (sptr == who)
						goto attack; /* kicking self == ok */
					if (op_can_override(sptr)) /* (and f*ck local ops) */
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
					else if (!IsULine(sptr) && (who != sptr) && MyClient(sptr))
					{
						char errbuf[NICKLEN+25];
						if (who_flags & CHFL_CHANOWNER)
							ircsprintf(errbuf, "%s is a channel owner", 
								   who->name);
						else
							ircsprintf(errbuf, "%s is a channel admin", 
								   who->name);
						sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND),
							   me.name, sptr->name, "KICK",
							   errbuf);
						goto deny;
						continue;
					}	/* chanprot/chanowner */
				}
				
				/* victim is +o, we are +h [operoverride is already taken care of 2 blocks above] */
				if ((who_flags & CHFL_ISOP) && (sptr_flags & CHFL_HALFOP)
				    && !(sptr_flags & CHFL_ISOP) && !IsULine(sptr) && MyClient(sptr))
				{
					char errbuf[NICKLEN+30];
					ircsprintf(errbuf, "%s is a channel operator", who->name);
					sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND),
						   me.name, sptr->name, "KICK",
						   errbuf);
					goto deny;
				}

				/* victim is +h, we are +h [operoverride is already taken care of 3 blocks above] */
				if ((who_flags & CHFL_HALFOP) && (sptr_flags & CHFL_HALFOP)
				    && !(sptr_flags & CHFL_ISOP) && MyClient(sptr))
				{
					char errbuf[NICKLEN+15];
					ircsprintf(errbuf, "%s is a halfop", who->name);
					sendto_one(sptr, err_str(ERR_CANNOTDOCOMMAND),
						   me.name, sptr->name, "KICK",
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
					RunHook5(HOOKTYPE_LOCAL_KICK, cptr,sptr,who,chptr,comment);
				} else {
					RunHook5(HOOKTYPE_REMOTE_KICK, cptr, sptr, who, chptr, comment);
				}
				if (lp)
				{
					if ((chptr->mode.mode & MODE_AUDITORIUM) &&
					    !(lp->flags & (CHFL_CHANOP|CHFL_CHANPROT|CHFL_CHANOWNER)))
					{
						/* Send it only to chanops & victim */
						if (IsPerson(sptr))
							sendto_chanops_butone(who, chptr, ":%s!%s@%s KICK %s %s :%s",
								sptr->name, sptr->user->username, GetHost(sptr),
								chptr->chname, who->name, comment);
						else
							sendto_chanops_butone(who, chptr, ":%s KICK %s %s :%s",
								sptr->name, chptr->chname, who->name, comment);
						
						if (MyClient(who))
							sendto_prefix_one(who, sptr, ":%s KICK %s %s :%s",
								sptr->name, chptr->chname, who->name, comment);
					} else {
						/* NORMAL */
						sendto_channel_butserv(chptr,
						    sptr, ":%s KICK %s %s :%s",
						    parv[0], name, who->name, comment);
					}
				}
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
			if (MyClient(cptr))
				break;
		}		/* loop on parv[2] */
		if (MyClient(cptr))
			break;
	}			/* loop on parv[1] */

	return 0;
}
