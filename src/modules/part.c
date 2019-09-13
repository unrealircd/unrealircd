/*
 *   IRC - Internet Relay Chat, src/modules/m_part.c
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

CMD_FUNC(m_part);

#define MSG_PART 	"PART"	

ModuleHeader MOD_HEADER
  = {
	"part",
	"5.0",
	"command /part", 
	"UnrealIRCd Team",
	"unrealircd-5",
    };

MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PART, m_part, 2, M_USER);
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
** m_part
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
CMD_FUNC(m_part)
{
	Channel *chptr;
	Membership *lp;
	char *p = NULL, *name;
	char *commentx = (parc > 2 && parv[2]) ? parv[2] : NULL;
	char *comment;
	int n;
	int ntargets = 0;
	int maxtargets = max_targets_for_command("PART");
	
	if (parc < 2 || parv[1][0] == '\0')
	{
		sendnumeric(sptr, ERR_NEEDMOREPARAMS, "PART");
		return 0;
	}

	if (MyUser(sptr))
	{
		if (IsShunned(sptr))
			commentx = NULL;
		if (STATIC_PART)
		{
			if (!strcasecmp(STATIC_PART, "yes") || !strcmp(STATIC_PART, "1"))
				commentx = NULL;
			else if (!strcasecmp(STATIC_PART, "no") || !strcmp(STATIC_PART, "0"))
				; /* keep original reason */
			else
				commentx = STATIC_PART;
		}
		if (commentx)
		{
			n = run_spamfilter(sptr, commentx, SPAMF_PART, parv[1], 0, NULL);
			if (n == FLUSH_BUFFER)
				return n;
			if (n < 0)
				commentx = NULL;
		}
	}

	for (; (name = strtoken(&p, parv[1], ",")); parv[1] = NULL)
	{
		MessageTag *mtags = NULL;

		if (MyUser(sptr) && (++ntargets > maxtargets))
		{
			sendnumeric(sptr, ERR_TOOMANYTARGETS, name, maxtargets, "PART");
			break;
		}

		chptr = get_channel(sptr, name, 0);
		if (!chptr)
		{
			sendnumeric(sptr, ERR_NOSUCHCHANNEL, name);
			continue;
		}

		/* 'commentx' is the general part msg, but it can be changed
		 * per-channel (eg some chans block badwords, strip colors, etc)
		 * so we copy it to 'comment' and use that in this for loop :)
		 */
		comment = commentx;

		if (!(lp = find_membership_link(sptr->user->channel, chptr)))
		{
			/* Normal to get get when our client did a kick
			   ** for a remote client (who sends back a PART),
			   ** so check for remote client or not --Run
			 */
			if (MyUser(sptr))
				sendnumeric(sptr, ERR_NOTONCHANNEL, name);
			continue;
		}

		if (!ValidatePermissionsForPath("channel:override:banpartmsg",sptr,NULL,chptr,NULL) && !is_chan_op(sptr, chptr)) {
			/* Banned? No comment allowed ;) */
			if (comment && is_banned(sptr, chptr, BANCHK_MSG, &comment, NULL))
				comment = NULL;
			if (comment && is_banned(sptr, chptr, BANCHK_LEAVE_MSG, &comment, NULL))
				comment = NULL;
			/* Same for +m */
			if ((chptr->mode.mode & MODE_MODERATED) && comment &&
				 !has_voice(sptr, chptr) && !is_half_op(sptr, chptr))
			{
				comment = NULL;
			}
		}

		if (MyConnect(sptr))
		{
			Hook *tmphook;
			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_PART]; tmphook; tmphook = tmphook->next) {
				comment = (*(tmphook->func.pcharfunc))(sptr, chptr, comment);
				if (!comment)
					break;
			}
		}

		/* Create a new message, this one is actually used by 8 calls (though at most 4 max) */
		new_message_special(sptr, recv_mtags, &mtags, ":%s PART %s", sptr->name, chptr->chname);

		/* Send to other servers... */
		sendto_server(cptr, PROTO_SID, 0, mtags, ":%s PART %s :%s",
			ID(sptr), chptr->chname, comment ? comment : "");
		sendto_server(cptr, 0, PROTO_SID, mtags, ":%s PART %s :%s",
			sptr->name, chptr->chname, comment ? comment : "");

		if (invisible_user_in_channel(sptr, chptr))
		{
			/* Show PART only to chanops and self */
			if (!comment)
			{
				sendto_channel(chptr, sptr, sptr,
					       CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANADMIN, 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s",
					       sptr->name, chptr->chname);
				if (MyUser(sptr))
				{
					sendto_one(sptr, mtags, ":%s!%s@%s PART %s",
						sptr->name, sptr->user->username, GetHost(sptr), chptr->chname);
				}
			}
			else
			{
				sendto_channel(chptr, sptr, sptr,
					       CHFL_HALFOP|CHFL_CHANOP|CHFL_CHANOWNER|CHFL_CHANADMIN, 0,
					       SEND_LOCAL, mtags,
					       ":%s PART %s %s",
					       sptr->name, chptr->chname, comment);
				if (MyUser(sptr))
				{
					sendto_one(sptr, mtags,
						":%s!%s@%s PART %s %s",
						sptr->name, sptr->user->username, GetHost(sptr),
						chptr->chname, comment);
				}
			}
		}
		else
		{
			/* Show PART to all users in channel */
			if (!comment)
			{
				sendto_channel(chptr, sptr, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s",
				               sptr->name, chptr->chname);
			} else {
				sendto_channel(chptr, sptr, NULL, 0, 0, SEND_LOCAL, mtags,
				               ":%s PART %s :%s",
				               sptr->name, chptr->chname, comment);
			}
		}

		if (MyUser(sptr))
			RunHook5(HOOKTYPE_LOCAL_PART, cptr, sptr, chptr, mtags, comment);
		else
			RunHook5(HOOKTYPE_REMOTE_PART, cptr, sptr, chptr, mtags, comment);

		free_message_tags(mtags);

		remove_user_from_channel(sptr, chptr);
	}
	return 0;
}
