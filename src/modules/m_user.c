/*
 *   IRC - Internet Relay Chat, src/modules/m_user.c
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
#ifdef _WIN32
#include "version.h"
#endif

CMD_FUNC(m_user);

#define MSG_USER 	"USER"	

ModuleHeader MOD_HEADER(m_user)
  = {
	"m_user",
	"$Id$",
	"command /user", 
	"3.2-b8-1",
	NULL 
    };

MOD_INIT(m_user)
{
	CommandAdd(modinfo->handle, MSG_USER, m_user, 4, M_USER|M_UNREGISTERED);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

MOD_LOAD(m_user)
{
	return MOD_SUCCESS;
}

MOD_UNLOAD(m_user)
{
	return MOD_SUCCESS;
}

/*
** m_user
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
**
** NOTE: Be advised that multiple USER messages are possible,
**       hence, always check if a certain struct is already allocated... -- Syzop
*/
CMD_FUNC(m_user)
{
	char *username, *host, *server, *realname;
	aClient *acptr;

	if (!MyConnect(sptr))
		return 0;

	if (!IsUnknown(sptr))
	{
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, sptr->name);
		return 0;
	}

	if (sptr->local->listener->options & LISTENER_SERVERSONLY)
		return exit_client(cptr, sptr, sptr, "This port is for servers only");

	if (parc > 2 && (username = (char *)index(parv[1], '@')))
		*username = '\0';

	if (parc < 5 || BadPtr(parv[4]))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, sptr->name, "USER");
		return 0;
	}

	username = parv[1];
	host = parv[2];
	server = parv[3];
	realname = parv[4];
	
	if (!sptr->user)
		make_user(sptr);

	sptr->umodes |= CONN_MODES;
	if (CONNECT_SNOMASK)
	{
		sptr->umodes |= UMODE_SERVNOTICE;
		create_snomask(sptr, sptr->user, CONNECT_SNOMASK);
	}

	/* Set it temporarely to at least something trusted,
	 * this was copying user supplied data directly into user->realhost
	 * which seemed bad. Not to say this is much better ;p. -- Syzop
	 */
	strlcpy(sptr->user->realhost, GetIP(sptr), sizeof(sptr->user->realhost));
	sptr->user->server = me_hash;

	strlcpy(sptr->info, realname, sizeof(sptr->info));

	if (*sptr->name && (IsNotSpoof(sptr) && !CHECKPROTO(sptr, PROTO_CLICAP)))
	{
		/* NICK and no-spoof already received, now we have USER... */
		if (USE_BAN_VERSION && MyConnect(sptr))
			sendto_one(sptr, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1", me.name, sptr->name);
		if (strlen(username) > USERLEN)
			username[USERLEN] = '\0'; /* cut-off */
		return register_user(cptr, sptr, sptr->name, username, NULL, NULL, NULL);
	}
	else
		strlcpy(sptr->user->username, username, USERLEN + 1);

	return 0;
}
