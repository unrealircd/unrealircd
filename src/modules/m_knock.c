/*
 *   IRC - Internet Relay Chat, src/modules/m_knock.c
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

DLLFUNC int m_knock(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_KNOCK 	"KNOCK"	
#define TOK_KNOCK 	"AI"	

ModuleHeader MOD_HEADER(m_knock)
  = {
	"m_knock",
	"$Id$",
	"command /knock", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_knock)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_KNOCK, TOK_KNOCK, m_knock, 2, M_USER|M_ANNOUNCE);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_knock)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_knock)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_knock
**	parv[0] - sender prefix
**	parv[1] - channel
**	parv[2] - reason
**
** Coded by Stskeeps
** Additional bugfixes/ideas by codemastr
** (C) codemastr & Stskeeps
** 
*/
CMD_FUNC(m_knock)
{
	aChannel *chptr;
	char buf[1024], chbuf[CHANNELLEN + 8];	
	
	if (IsServer(sptr))
		return 0;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "KNOCK");
		return -1;
	}

	if (MyConnect(sptr))
		clean_channelname(parv[1]);

	/* bugfix for /knock PRv Please? */
	if (*parv[1] != '#')
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name,
		    parv[1], "Remember to use a # prefix in channel name");

		return 0;
	}
	if (!(chptr = find_channel(parv[1], NullChn)))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, parv[1], "Channel does not exist!");
		return 0;
	}

	/* IsMember bugfix by codemastr */
	if (IsMember(sptr, chptr) == 1)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "You're already there!");
		return 0;
	}
	if (chptr->mode.mode & MODE_NOKNOCK)
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "No knocks are allowed! (+K)");
		return 0;
	}

	if (!(chptr->mode.mode & MODE_INVITEONLY))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name, chptr->chname, "Channel is not invite only!");
		return 0;
	}

	if (is_banned(sptr, chptr, BANCHK_JOIN))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name, sptr->name, chptr->chname, "You're banned!");
		return 0;
	}

	if ((chptr->mode.mode & MODE_NOINVITE) && !is_chan_op(sptr, chptr))
	{
		sendto_one(sptr, err_str(ERR_CANNOTKNOCK),
		    me.name,
		    sptr->name,
		    chptr->chname, "The channel does not allow invites (+V)");

		return 0;
	}

	ircsprintf(chbuf, "@%s", chptr->chname);
	ircsprintf(buf, "[Knock] by %s!%s@%s (%s)",
		sptr->name, sptr->user->username, GetHost(sptr),
		parv[2] ? parv[2] : "no reason specified");
	sendto_channelprefix_butone_tok(NULL, &me, chptr, PREFIX_OP|PREFIX_ADMIN|PREFIX_OWNER,
		MSG_NOTICE, TOK_NOTICE, chbuf, buf, 0);

	sendto_one(sptr, ":%s %s %s :Knocked on %s", me.name, IsWebTV(sptr) ? "PRIVMSG" : "NOTICE",
	    sptr->name, chptr->chname);

#ifdef NEWCHFLOODPROT
	if (chptr->mode.floodprot && !IsULine(sptr) &&
	    do_chanflood(chptr->mode.floodprot, FLD_KNOCK) && MyClient(sptr))
		do_chanflood_action(chptr, FLD_KNOCK, "knock");
#endif
	return 0;
}
