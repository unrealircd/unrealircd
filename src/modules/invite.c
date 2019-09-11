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

ModuleHeader MOD_HEADER(invite)
  = {
	"invite",
	"5.0",
	"command /invite", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT(invite)
{
	CommandAdd(modinfo->handle, MSG_INVITE, m_invite, MAXPARA, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(invite)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(invite)
{
	return MOD_SUCCESS;
}

/* Send the user their list of active invites */
int send_invite_list(Client *sptr)
{
	Link *inv;

	for (inv = sptr->user->invited; inv; inv = inv->next)
	{
		sendnumeric(sptr, RPL_INVITELIST,
			   inv->value.chptr->chname);	
	}
	sendnumeric(sptr, RPL_ENDOFINVITELIST);
	return 0;
}

/*
** m_invite
**	parv[1] - user to invite
**	parv[2] - channel number
*/
CMD_FUNC(m_invite)
{
	Client *acptr;
	Channel *chptr;
	int override = 0;
	int i = 0;
	Hook *h;

	if (parc == 1)
		return send_invite_list(sptr);
	
	if (parc < 3 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "INVITE");
		return -1;
	}

	if (!(acptr = find_person(parv[1], NULL)))
	{
		sendnumeric(sptr, ERR_NOSUCHNICK, parv[1]);
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[2]);

	if (!(chptr = find_channel(parv[2], NULL)))
	{
		sendnumeric(sptr, ERR_NOSUCHCHANNEL, parv[2]);
		return -1;
	}

	for (h = Hooks[HOOKTYPE_PRE_INVITE]; h; h = h->next)
	{
		i = (*(h->func.intfunc))(sptr,acptr,chptr,&override);
		if (i == HOOK_DENY)
			return -1;
		if (i == HOOK_ALLOW)
			break;
	}

	if (!IsMember(sptr, chptr) && !IsULine(sptr))
	{
		if (ValidatePermissionsForPath("channel:override:invite:notinchannel",sptr,NULL,chptr,NULL) && sptr == acptr)
		{
			override = 1;
		} else {
			sendnumeric(sptr, ERR_NOTONCHANNEL, parv[2]);
			return -1;
		}
	}

	if (IsMember(acptr, chptr))
	{
		sendnumeric(sptr, ERR_USERONCHANNEL, parv[1], parv[2]);
		return 0;
	}

	if (chptr->mode.mode & MODE_INVITEONLY)
	{
		if (!is_chan_op(sptr, chptr) && !IsULine(sptr))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",sptr,NULL,chptr,NULL) && sptr == acptr)
			{
				override = 1;
			} else {
				sendnumeric(sptr, ERR_CHANOPRIVSNEEDED, chptr->chname);
				return -1;
			}
		}
		else if (!IsMember(sptr, chptr) && !IsULine(sptr))
		{
			if (ValidatePermissionsForPath("channel:override:invite:invite-only",sptr,NULL,chptr,NULL) && sptr == acptr)
			{
				override = 1;
			} else {
				sendnumeric(sptr, ERR_CHANOPRIVSNEEDED, chptr->chname);
				return -1;
			}
		}
	}

	if (SPAMFILTER_VIRUSCHANDENY && SPAMFILTER_VIRUSCHAN &&
	    !strcasecmp(chptr->chname, SPAMFILTER_VIRUSCHAN) &&
	    !is_chan_op(sptr, chptr) && !ValidatePermissionsForPath("immune:server-ban:viruschan",sptr,NULL,NULL,NULL))
	{
		sendnumeric(sptr, ERR_CHANOPRIVSNEEDED, chptr->chname);
		return -1;
	}

	if (MyConnect(sptr))
	{
		if (check_for_target_limit(sptr, acptr, acptr->name))
			return 0;

		if (!ValidatePermissionsForPath("immune:invite-flood",sptr,NULL,NULL,NULL))
		{
			if ((sptr->user->flood.invite_t + INVITE_PERIOD) <= timeofday)
			{
				sptr->user->flood.invite_c = 0;
				sptr->user->flood.invite_t = timeofday;
			}
			if (sptr->user->flood.invite_c <= INVITE_COUNT)
				sptr->user->flood.invite_c++;
			if (sptr->user->flood.invite_c > INVITE_COUNT)
			{
				sendnumeric(sptr, RPL_TRYAGAIN, "INVITE");
				return 0;
			}
		}

		if (!override)
		{
			sendnumeric(sptr, RPL_INVITING, acptr->name, chptr->chname);
			if (acptr->user->away)
			{
				sendnumeric(sptr, RPL_AWAY, acptr->name, acptr->user->away);
			}
		}
	}

	/* Send OperOverride messages */
	if (override && MyConnect(acptr))
	{
		if (is_banned(sptr, chptr, BANCHK_JOIN, NULL, NULL))
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
		else if (has_channel_mode(chptr, 'z'))
		{
			sendto_snomask_global(SNO_EYES,
			  "*** OperOverride -- %s (%s@%s) invited him/herself into %s (overriding +z).",
			  sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);

			/* Logging implementation added by XeRXeS */
			ircd_log(LOG_OVERRIDE,"OVERRIDE: %s (%s@%s) invited him/herself into %s (Overriding SSL/TLS-Only)",
				sptr->name, sptr->user->username, sptr->user->realhost, chptr->chname);
		}
#ifdef OPEROVERRIDE_VERIFY
		else if (chptr->mode.mode & MODE_SECRET || chptr->mode.mode & MODE_PRIVATE)
		       override = -1;
#endif
		else
			return 0;
	}

	if (MyConnect(acptr))
	{
		if (IsUser(sptr) 
		    && (is_chan_op(sptr, chptr)
		    || IsULine(sptr)
		    || ValidatePermissionsForPath("channel:override:invite:self",sptr,NULL,chptr,NULL)
		    ))
		{
			MessageTag *mtags = NULL;

			new_message(&me, NULL, &mtags);
			if (override == 1)
			{
				sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				               0, SEND_ALL, mtags,
				               ":%s NOTICE @%s :OperOverride -- %s invited him/herself into the channel.",
				               me.name, chptr->chname, sptr->name);
			} else
			if (override == 0)
			{
				sendto_channel(chptr, &me, NULL, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
				               0, SEND_ALL, mtags,
				               ":%s NOTICE @%s :%s invited %s into the channel.",
				               me.name, chptr->chname, sptr->name, acptr->name);
			}
			add_invite(sptr, acptr, chptr, mtags);
			free_message_tags(mtags);
		}
	}

	/* Notify the person who got invited */
	if (!is_silenced(sptr, acptr))
	{
		sendto_prefix_one(acptr, sptr, NULL, ":%s INVITE %s :%s", sptr->name,
			acptr->name, ((chptr) ? (chptr->chname) : parv[2]));
	}

	return 0;
}
