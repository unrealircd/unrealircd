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

#include "unrealircd.h"

CMD_FUNC(m_invite);

#define MSG_INVITE 	"INVITE"	

ModuleHeader MOD_HEADER(m_invite)
  = {
	"m_invite",
	"4.0",
	"command /invite", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_invite)
{
	CommandAdd(modinfo->handle, MSG_INVITE, m_invite, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_invite)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_invite)
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
**	parv[1] - user to invite
**	parv[2] - channel number
*/
CMD_FUNC(m_invite)
{
	aClient *acptr;
	aChannel *chptr;
	short over = 0;
	int i = 0;
	Hook *h;

	if (parc == 1)
		return send_invite_list(sptr);
	
	if (parc < 3 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, sptr->name, "INVITE");
		return -1;
	}

	if (!(acptr = find_person(parv[1], (aClient *)NULL)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHNICK),
		    me.name, sptr->name, parv[1]);
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[2]);

	if (!(chptr = find_channel(parv[2], NULL)))
	{
		sendto_one(sptr, err_str(ERR_NOSUCHCHANNEL),
		    me.name, sptr->name, parv[2]);
		return -1;
	}

	for (h = Hooks[HOOKTYPE_PRE_INVITE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(sptr,chptr);
		if (i == HOOK_DENY || i == HOOK_ALLOW)
			break;
	}

	if (i == HOOK_DENY && !IsULine(sptr))
	{
		if (ValidatePermissionsForPath("override:invite:nopermissions",sptr,NULL,chptr,NULL) && sptr == acptr)
		{
			over = 1;
		} else {
			sendto_one(sptr, err_str(ERR_NOINVITE),
			    me.name, sptr->name, parv[2]);
			return -1;
		}
	}

	if (!IsMember(sptr, chptr) && !IsULine(sptr))
	{
		if (ValidatePermissionsForPath("override:invite:notinchannel",sptr,NULL,chptr,NULL) && sptr == acptr)
		{
			over = 1;
		} else {
			sendto_one(sptr, err_str(ERR_NOTONCHANNEL),
			    me.name, sptr->name, parv[2]);
			return -1;
		}
	}

	if (IsMember(acptr, chptr))
	{
		sendto_one(sptr, err_str(ERR_USERONCHANNEL),
		    me.name, sptr->name, parv[1], parv[2]);
		return 0;
	}

	if (chptr->mode.mode & MODE_INVITEONLY)
	{
		if (!is_chan_op(sptr, chptr) && !IsULine(sptr))
		{
			if (ValidatePermissionsForPath("override:invite:nopermissions",sptr,NULL,chptr,NULL) && sptr == acptr)
			{
				over = 1;
			} else {
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				    me.name, sptr->name, chptr->chname);
				return -1;
			}
		}
		else if (!IsMember(sptr, chptr) && !IsULine(sptr))
		{
			if (ValidatePermissionsForPath("override:invite:nopermissions",sptr,NULL,chptr,NULL) && sptr == acptr)
			{
				over = 1;
			} else {
				sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
				    me.name, sptr->name, chptr->chname);
				return -1;
			}
		}
	}

	if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
	    !strcasecmp(chptr->chname, SPAMFILTER_VIRUSCHAN) &&
	    !is_chan_op(sptr, chptr) && !ValidatePermissionsForPath("immune:viruscheck",sptr,NULL,NULL,NULL))
	{
		sendto_one(sptr, err_str(ERR_CHANOPRIVSNEEDED),
			me.name, sptr->name, chptr->chname);
		return -1;
	}

	if (MyConnect(sptr))
	{
		if (check_for_target_limit(sptr, acptr, acptr->name))
			return 0;

		if (!over)
		{
			sendto_one(sptr, rpl_str(RPL_INVITING), me.name,
			    sptr->name, acptr->name, chptr->chname);
			if (acptr->user->away)
			{
				sendto_one(sptr, rpl_str(RPL_AWAY), me.name,
				    sptr->name, acptr->name, acptr->user->away);
			}
		}
	}

	/* Send OperOverride messages */
	if (over && MyConnect(acptr))
	{
		if (is_banned(sptr, chptr, BANCHK_JOIN))
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

	if (MyConnect(acptr))
	{
		if (IsPerson(sptr) 
		    && (is_chan_op(sptr, chptr)
		    || IsULine(sptr)
		    || ValidatePermissionsForPath("override:channel:invite",sptr,NULL,chptr,NULL)
		    ))
		{
			if (over == 1)
			{
				sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				  ":%s NOTICE @%s :OperOverride -- %s invited him/herself into the channel.",
				  me.name, chptr->chname, sptr->name);
			} else
			if (over == 0)
			{
				sendto_channelprefix_butone(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				  ":%s NOTICE @%s :%s invited %s into the channel.",
				  me.name, chptr->chname, sptr->name, acptr->name);
			}

			add_invite(sptr, acptr, chptr);
		}
	}

	/* Notify the person who got invited */
	if (!is_silenced(sptr, acptr))
	{
		sendto_prefix_one(acptr, sptr, ":%s INVITE %s :%s", sptr->name,
			acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	}

	return 0;
}
