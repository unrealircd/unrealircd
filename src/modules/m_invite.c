/*
 *   IRC - Internet Relay Chat, src/modules/m_invite.c
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

DLLFUNC int m_invite(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_INVITE 	"INVITE"	
#define TOK_INVITE 	"*"	

ModuleHeader MOD_HEADER(m_invite)
  = {
	"m_invite",
	"$Id$",
	"command /invite", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_invite)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_INVITE, TOK_INVITE, m_invite, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_invite)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_invite)(int module_unload)
{
	return MOD_SUCCESS;
}

/* Send the user their list of active invites */
int send_invite_list(aClient *sptr)
{
	Link *inv;

	for (inv = sptr->user->invited; inv; inv = inv->next)
	{
		sendto_one(sptr, rpl_str(RPL_INVITELIST), me.name, sptr->name,
			   inv->value.chptr->chname);	
	}
	sendto_one(sptr, rpl_str(RPL_ENDOFINVITELIST), me.name, sptr->name);
	return 0;
}

/*
** m_invite
**	parv[0] - sender prefix
**	parv[1] - user to invite
**	parv[2] - channel number
*/
DLLFUNC CMD_FUNC(m_invite)
{
        aClient *acptr;
        aChannel *chptr;
        short over = 0;

	if (parc == 1)
		return send_invite_list(sptr);
        else if (parc < 3 || *parv[1] == '\0')
        {
                sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
                    me.name, parv[0], "INVITE");
                return -1;
        }

        if (!(acptr = find_person(parv[1], (aClient *)NULL)))
        {
                sendto_one(sptr, err_str(ERR_NOSUCHNICK),
                    me.name, parv[0], parv[1]);
                return -1;
        }

        if (MyConnect(sptr))
                clean_channelname(parv[2]);

        if (!(chptr = find_channel(parv[2], NullChn)))
        {
                sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
                    me.name, parv[0], parv[2]);
                return -1;
        }

        if (chptr->mode.mode & MODE_NOINVITE && !IsULine(sptr))
        {
#ifndef NO_OPEROVERRIDE
                if ((MyClient(sptr) ? (IsOper(sptr) && OPCanOverride(sptr)) :
		    IsOper(sptr)) && sptr == acptr)
                        over = 1;
                else {
#endif
                        sendto_one(sptr, err_str(ERR_NOINVITE),
                            me.name, parv[0], parv[2]);
                        return -1;
#ifndef NO_OPEROVERRIDE
                }
#endif
        }

        if (!IsMember(sptr, chptr) && !IsULine(sptr))
        {
#ifndef NO_OPEROVERRIDE
                if ((MyClient(sptr) ? (IsOper(sptr) && OPCanOverride(sptr)) :
		    IsOper(sptr)) && sptr == acptr)
                        over = 1;
                else {
#endif
                        sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
                            me.name, parv[0], parv[2]);
                        return -1;
#ifndef NO_OPEROVERRIDE
                }
#endif
        }

        if (IsMember(acptr, chptr))
        {
                sendto_one(sptr, err_str(ERR_USERONCHANNEL),
                    me.name, parv[0], parv[1], parv[2]);
                return 0;
        }

        if (chptr->mode.mode & MODE_INVITEONLY)
        {
                if (!is_chan_op(sptr, chptr) && !IsULine(sptr))
                {
#ifndef NO_OPEROVERRIDE
                if ((MyClient(sptr) ? (IsOper(sptr) && OPCanOverride(sptr)) :
		    IsOper(sptr)) && sptr == acptr)
                                over = 1;
                        else {
#endif
                                sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
                                    me.name, parv[0], chptr->chname);
                                return -1;
#ifndef NO_OPEROVERRIDE
                        }
#endif
                }
                else if (!IsMember(sptr, chptr) && !IsULine(sptr))
                {
#ifndef NO_OPEROVERRIDE
                if ((MyClient(sptr) ? (IsOper(sptr) && OPCanOverride(sptr)) :
		    IsOper(sptr)) && sptr == acptr)
                                over = 1;
                        else {
#endif
                                sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
                                    me.name, parv[0],
                                        ((chptr) ? (chptr->chname) : parv[2]));
                                return -1;
#ifndef NO_OPEROVERRIDE
                        }
#endif
                }
        }

		if (MyClient(sptr) && SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
		    !strcasecmp(chptr->chname, SPAMFILTER_VIRUSCHAN) &&
		    !is_chan_op(sptr, chptr) && !IsAnOper(sptr) && !IsULine(sptr))
		{
			sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				me.name, parv[0], chptr->chname);
			return -1;
		}

        if (MyConnect(sptr))
        {
                if (check_for_target_limit(sptr, acptr, acptr->name))
                        return 0;
                if (!over)
                {
                        sendto_one(sptr, rpl_str(RPL_INVITING), me.name,
                            parv[0], acptr->name,
                            ((chptr) ? (chptr->chname) : parv[2]));
                        if (acptr->user->away)
                                sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
                                    parv[0], acptr->name, acptr->user->away);
                }
        }
        /* Note: is_banned() here will cause some extra CPU load,
         *       and we're really only relying on the existence
         *       of the limit because we could momentarily have
         *       less people on channel.
         */


	if (over && MyConnect(acptr)) {
        	if ((chptr->mode.mode & MODE_ONLYSECURE) && !IsSecure(acptr))
	        {
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +z).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

                        /* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Secure Mode)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);
	        }
	        else if (is_banned(sptr, chptr, BANCHK_JOIN))
        	{
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +b).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Ban).",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

	        }
        	else if (chptr->mode.mode & MODE_INVITEONLY)
	        {
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +i).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

                        /* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Invite Only)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

	        }
        	else if (chptr->mode.limit)
	        {
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +l).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

                        /* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Limit)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

		}
        	else if (chptr->mode.mode & MODE_RGSTRONLY)
	        {
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +R).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

                        /* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Reg Nicks Only)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

	        }
        	else if (*chptr->mode.key)
	        {
                        sendto_snomask_global(SNO_EYES,
                          "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +k).",
                          sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

                        /* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding Key)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

	        }
#ifdef OPEROVERRIDE_VERIFY
        	else if (chptr->mode.mode & MODE_SECRET || chptr->mode.mode & MODE_PRIVATE)
	               over = -1;
#endif
        	else
                	return 0;
	}
	if (MyConnect(acptr)) {
		if (chptr && sptr->user
		    && (is_chan_op(sptr, chptr)
		    || IsULine(sptr)
#ifndef NO_OPEROVERRIDE
		    || IsOper(sptr)
#endif
		    )) {
		        if (over == 1)
                		sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		                  ":%s NOTICE @%s :OperOverride -- %s invited him/herself into the channel.",
                		  me.name, chptr->chname, sptr->name);
		        else if (over == 0)
		                sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
                		  ":%s NOTICE @%s :%s invited %s into the channel.",
		                  me.name, chptr->chname, sptr->name, acptr->name);

		        add_invite(acptr, chptr);
			}
	}
	if (!is_silenced(sptr, acptr))
		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s", parv[0],
			acptr->name, ((chptr) ? (chptr->chname) : parv[2]));

        return 0;
}
