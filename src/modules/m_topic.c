/*
 *   IRC - Internet Relay Chat, src/modules/m_topic.c
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

DLLFUNC int m_topic(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_TOPIC 	"TOPIC"	
#define TOK_TOPIC 	")"	

ModuleHeader MOD_HEADER(m_topic)
  = {
	"m_topic",
	"$Id$",
	"command /topic", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_topic)(ModuleInfo *modinfo)
{
	add_Command(MSG_TOPIC, TOK_TOPIC, m_topic, 4);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_topic)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_topic)(int module_unload)
{
	if (del_Command(MSG_TOPIC, TOK_TOPIC, m_topic) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
			MOD_HEADER(m_topic).name);
	}
	return MOD_SUCCESS;
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
DLLFUNC CMD_FUNC(m_topic)
{
	aChannel *chptr = NullChn;
	char *topic = NULL, *name, *tnick = NULL;
	TS   ttime = 0;
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
			if (!MyClient(sptr) && !IsULine(sptr))
			{
				sendto_snomask
				    (SNO_JUNK,"Remote TOPIC for unknown channel %s (%s)",
				    parv[1], backupbuf);
			}
			sendto_one(sptr, rpl_str(ERR_NOSUCHCHANNEL),
			    me.name, parv[0], name);
			return 0;
		}
		if (parc > 2 || SecretChannel(chptr))
		{
			if (!IsMember(sptr, chptr) && !IsServer(sptr)
			    && !IsOper(sptr) && !IsULine(sptr))
			{
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
				    me.name, parv[0], name);
				return 0;
			}
			if (parc > 2)
				topic = parv[2];
		}
		if (parc > 4)
		{
			tnick = parv[2];
			ttime = TS2ts(parv[3]);
			topic = parv[4];

		}

		if (!topic)	/* only asking  for topic  */
		{
			if ((chptr->mode.mode & MODE_OPERONLY && !IsAnOper(sptr) && !IsMember(sptr, chptr)) ||
			    (chptr->mode.mode & MODE_ADMONLY && !IsAdmin(sptr) && !IsMember(sptr, chptr)) ||
			    (is_banned(sptr,chptr,BANCHK_JOIN) && !IsAnOper(sptr) && !IsMember(sptr, chptr))) {
				sendto_one(sptr, err_str(ERR_NOTONCHANNEL), me.name, parv[0], name);
				return 0;
			}
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
		    || IsULine(sptr)))
		{
			if (!chptr->topic_time || ttime > chptr->topic_time || IsULine(sptr))
			/* The IsUline is to allow services to use an old TS. Apparently
			 * some services do this in their topic enforcement -- codemastr 
			 */
			{
				/* setting a topic */
				topiClen = strlen(topic);
				nicKlen = strlen(tnick);

				if (chptr->topic)
					MyFree(chptr->topic);

				if (topiClen > (TOPICLEN))
					topiClen = TOPICLEN;

				if (nicKlen > (NICKLEN+USERLEN+HOSTLEN+5))
					nicKlen = (NICKLEN+USERLEN+HOSTLEN+5);

				chptr->topic = MyMalloc(topiClen + 1);
				strncpyzt(chptr->topic, topic, topiClen + 1);

				if (chptr->topic_nick)
					MyFree(chptr->topic_nick);

				chptr->topic_nick = MyMalloc(nicKlen + 1);
				strncpyzt(chptr->topic_nick, tnick,
				    nicKlen + 1);

				chptr->topic_time = ttime;
				RunHook4(HOOKTYPE_TOPIC, cptr, sptr, chptr, topic);
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
		    (is_chan_op(sptr, chptr)) || IsOper(sptr)
		    || IsULine(sptr) || is_halfop(sptr, chptr)) && topic)
		{
			/* setting a topic */
			if (chptr->mode.mode & MODE_TOPICLIMIT)
			{
				if (!is_halfop(sptr, chptr) && !IsULine(sptr) && !
					is_chan_op(sptr, chptr))
				{
#ifndef NO_OPEROVERRIDE
					if ((MyClient(sptr) ? (!IsOper(sptr) || !OPCanOverride(sptr)) : !IsOper(sptr)))
					{
#endif
					sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
					    me.name, parv[0], chptr->chname);
					return 0;
#ifndef NO_OPEROVERRIDE
					}
					else
						sendto_snomask(SNO_EYES,
						    "*** OperOverride -- %s (%s@%s) TOPIC %s \'%s\'",
						    sptr->name, sptr->user->username, sptr->user->realhost,
						    chptr->chname, topic);
						
						/* Logging implementation added by XeRXeS */
						ircd_log(LOG_OVERRIDE, "OVERRIDE: %s (%s@%s) TOPIC %s \'%s\'",
							sptr->name, sptr->user->username, sptr->user->realhost,
							chptr->chname, topic);

#endif
				}
			}				
			/* ready to set... */
			if (MyClient(sptr))
			{
				Hook *tmphook;
				for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_TOPIC]; tmphook; tmphook = tmphook->next) {
					topic = (*(tmphook->func.pcharfunc))(sptr, chptr, topic);
					if (!topic)
						return 0;
				}
				RunHook4(HOOKTYPE_LOCAL_TOPIC, cptr, sptr, chptr, topic);
			}
			/* setting a topic */
			topiClen = strlen(topic);
#ifndef TOPIC_NICK_IS_NUHOST
			nicKlen = strlen(sptr->name);
#else
			tnick = make_nick_user_host(sptr->name, sptr->user->username, GetHost(sptr));
			nicKlen = strlen(tnick);
#endif
			if (chptr->topic)
				MyFree(chptr->topic);

			if (topiClen > (TOPICLEN))
				topiClen = TOPICLEN;
			if (nicKlen > (NICKLEN+USERLEN+HOSTLEN+5))
				nicKlen = NICKLEN+USERLEN+HOSTLEN+5;
			chptr->topic = MyMalloc(topiClen + 1);
			strncpyzt(chptr->topic, topic, topiClen + 1);

			if (chptr->topic_nick)
				MyFree(chptr->topic_nick);

			chptr->topic_nick = MyMalloc(nicKlen + 1);
#ifndef TOPIC_NICK_IS_NUHOST
			strncpyzt(chptr->topic_nick, sptr->name, nicKlen + 1);
#else
			strncpyzt(chptr->topic_nick, tnick, nicKlen + 1);
#endif
			RunHook4(HOOKTYPE_TOPIC, cptr, sptr, chptr, topic);
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
