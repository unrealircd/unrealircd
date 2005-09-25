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

DLLFUNC CMD_FUNC(m_part);

#define MSG_PART 	"PART"	
#define TOK_PART 	"D"	

ModuleHeader MOD_HEADER(m_part)
  = {
	"m_part",
	"$Id$",
	"command /part", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_part)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_PART, TOK_PART, m_part, 2, M_USER);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_part)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_part)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_part
**	parv[0] = sender prefix
**	parv[1] = channel
**	parv[2] = comment (added by Lefler)
*/
DLLFUNC CMD_FUNC(m_part)
{
	aChannel *chptr;
	Membership *lp;
	char *p = NULL, *name;
	char *commentx = (parc > 2 && parv[2]) ? parv[2] : NULL;
	char *comment;
	int n;
	
	if (parc < 2 || parv[1][0] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "PART");
		return 0;
	}

	if (MyClient(sptr))
	{
		if (IsShunned(sptr))
			commentx = NULL;
		if (STATIC_PART)
		{
			if (!strcasecmp(STATIC_PART, "yes") || !strcmp(STATIC_PART, "1"))
				commentx = NULL;
			else
				commentx = STATIC_PART;
		}
		if (commentx)
		{
			n = dospamfilter(sptr, commentx, SPAMF_PART, parv[1], 0);
			if (n == FLUSH_BUFFER)
				return n;
			if (n < 0)
				commentx = NULL;
		}
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
			if (MyClient(sptr))
				sendto_one(sptr,
				    err_str(ERR_NOTONCHANNEL), me.name,
				    parv[0], name);
			continue;
		}

		if (!IsAnOper(sptr) && !is_chanownprotop(sptr, chptr)) {
#ifdef STRIPBADWORDS
			int blocked = 0;
#endif
			if ((chptr->mode.mode & MODE_NOCOLOR) && comment) {
				if (strchr((char *)comment, 3) || strchr((char *)comment, 27)) {
					comment = NULL;
				}
			}
			if ((chptr->mode.mode & MODE_MODERATED) && comment &&
				 !has_voice(sptr, chptr) && !is_halfop(sptr, chptr))
			{
				comment = NULL;
			}
			if ((chptr->mode.mode & MODE_STRIP) && comment) {
				comment = (char *)StripColors(comment);
			}
#ifdef STRIPBADWORDS
 #ifdef STRIPBADWORDS_CHAN_ALWAYS
			if (comment)
			{
				comment = (char *)stripbadwords_channel(comment, &blocked);
			}
 #else
			if ((chptr->mode.extmode & EXTMODE_STRIPBADWORDS) && comment) {
				comment = (char *)stripbadwords_channel(comment, &blocked);
			}
 #endif
#endif
			
		}
		/* +M and not +r? */
		if ((chptr->mode.mode & MODE_MODREG) && !IsRegNick(sptr) && !IsAnOper(sptr))
			comment = NULL;

		if (MyConnect(sptr))
		{
			Hook *tmphook;
			for (tmphook = Hooks[HOOKTYPE_PRE_LOCAL_PART]; tmphook; tmphook = tmphook->next) {
				comment = (*(tmphook->func.pcharfunc))(sptr, chptr, comment);
				if (!comment)
					break;
			}
		}

		/* Send to other servers... */
		if (!comment)
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s", chptr->chname);
		else
			sendto_serv_butone_token(cptr, parv[0],
			    MSG_PART, TOK_PART, "%s :%s", chptr->chname,
			    comment);

		if (1)
		{
			if ((chptr->mode.mode & MODE_AUDITORIUM) && !is_chanownprotop(sptr, chptr))
			{
				if (!comment)
				{
					sendto_chanops_butone(NULL,
					    chptr, ":%s!%s@%s PART %s",
					    sptr->name, sptr->user->username, GetHost(sptr),
					    chptr->chname);
					if (!is_chan_op(sptr, chptr) && MyClient(sptr))
						sendto_one(sptr, ":%s!%s@%s PART %s",
						    sptr->name, sptr->user->username, GetHost(sptr), chptr->chname);
				}
				else
				{
					sendto_chanops_butone(NULL,
					    chptr,
					    ":%s!%s@%s PART %s %s",
					    sptr->name,
					    sptr->user->username,
					    GetHost(sptr),
					    chptr->chname, comment);
					if (!is_chan_op(cptr, chptr) && MyClient(sptr))
						sendto_one(sptr,
						    ":%s!%s@%s PART %s %s",
						    sptr->name, sptr->user->username, GetHost(sptr),
						    chptr->chname, comment);
				}
			}
			else
			{


				if (!comment)

					sendto_channel_butserv(chptr,
					    sptr, PARTFMT, parv[0],
					    chptr->chname);
				else
					sendto_channel_butserv(chptr,
					    sptr, PARTFMT2, parv[0],
					    chptr->chname, comment);
			}
			if (MyClient(sptr))
				RunHook4(HOOKTYPE_LOCAL_PART, cptr, sptr, chptr, comment);
			else
				RunHook4(HOOKTYPE_REMOTE_PART, cptr, sptr, chptr, comment);

			remove_user_from_channel(sptr, chptr);
		}
	}
	return 0;
}
