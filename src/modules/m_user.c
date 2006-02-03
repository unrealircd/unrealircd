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
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC CMD_FUNC(m_user);

#define MSG_USER 	"USER"	
#define TOK_USER 	"%"	

ModuleHeader MOD_HEADER(m_user)
  = {
	"m_user",
	"$Id$",
	"command /user", 
	"3.2-b8-1",
	NULL 
    };

DLLFUNC int MOD_INIT(m_user)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_USER, TOK_USER, m_user, 4, M_USER|M_UNREGISTERED);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

DLLFUNC int MOD_LOAD(m_user)(int module_load)
{
	return MOD_SUCCESS;
}

DLLFUNC int MOD_UNLOAD(m_user)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_user
**	parv[0] = sender prefix
**	parv[1] = username (login name, account)
**	parv[2] = client host name (used only from other servers)
**	parv[3] = server host name (used only from other servers)
**	parv[4] = users real name info
**
** NOTE: Be advised that multiple USER messages are possible,
**       hence, always check if a certain struct is already allocated... -- Syzop
*/
DLLFUNC CMD_FUNC(m_user)
{
#define	UFLAGS	(UMODE_INVISIBLE|UMODE_WALLOP|UMODE_SERVNOTICE)
	char *username, *host, *server, *realname, *umodex = NULL, *virthost =
	    NULL, *ip = NULL;
	u_int32_t sstamp = 0;
	anUser *user;
	aClient *acptr;

	if (IsServer(cptr) && !IsUnknown(sptr))
		return 0;

	if (MyConnect(sptr) && (sptr->listener->umodes & LISTENER_SERVERSONLY))
	{
		return exit_client(cptr, sptr, sptr,
		    "This port is for servers only");
	}

	if (parc > 2 && (username = (char *)index(parv[1], '@')))
		*username = '\0';
	if (parc < 5 || *parv[1] == '\0' || *parv[2] == '\0' ||
	    *parv[3] == '\0' || *parv[4] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
			    parv[0], get_client_name(cptr, FALSE));
		else
			return 0;
	}


	/* Copy parameters into better documenting variables */

	username = (parc < 2 || BadPtr(parv[1])) ? "<bad-boy>" : parv[1];
	host = (parc < 3 || BadPtr(parv[2])) ? "<nohost>" : parv[2];
	server = (parc < 4 || BadPtr(parv[3])) ? "<noserver>" : parv[3];

	/* This we can remove as soon as all servers have upgraded. */

	if (parc == 6 && IsServer(cptr))
	{
		if (isdigit(*parv[4]))
			sstamp = atol(parv[4]);
		realname = (BadPtr(parv[5])) ? "<bad-realname>" : parv[5];
		umodex = NULL;
	}
	else if (parc == 8 && IsServer(cptr))
	{
		if (isdigit(*parv[4]))
			sstamp = atol(parv[4]);
		realname = (BadPtr(parv[7])) ? "<bad-realname>" : parv[7];
		umodex = parv[5];
		virthost = parv[6];
	}
	else if (parc == 9 && IsServer(cptr))
	{
		if (isdigit(*parv[4]))
			sstamp = atol(parv[4]);
		realname = (BadPtr(parv[8])) ? "<bad-realname>" : parv[8];
		umodex = parv[5];
		virthost = parv[6];
		ip = parv[7];
	}
	else
	{
		realname = (BadPtr(parv[4])) ? "<bad-realname>" : parv[4];
	}
	user = make_user(sptr);

	if (!MyConnect(sptr))
	{
		if (sptr->srvptr == NULL)
			sendto_ops("WARNING, User %s introduced as being "
			    "on non-existant server %s.", sptr->name, server);
		if (SupportNS(cptr))
		{
			acptr = (aClient *)find_server_b64_or_real(server);
			if (acptr)
				user->server = find_or_add(acptr->name);
			else
				user->server = find_or_add(server);
		}
		else
			user->server = find_or_add(server);
		strlcpy(user->realhost, host, sizeof(user->realhost));
		goto user_finish;
	}

	if (!IsUnknown(sptr))
	{
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, parv[0]);
		return 0;
	}

	if (!IsServer(cptr))
	{
		sptr->umodes |= CONN_MODES;
		if (CONNECT_SNOMASK)
		{
			sptr->umodes |= UMODE_SERVNOTICE;
			create_snomask(sptr, user, CONNECT_SNOMASK);
		}
	}

	/* Set it temporarely to at least something trusted,
	 * this was copying user supplied data directly into user->realhost
	 * which seemed bad. Not to say this is much better ;p. -- Syzop
	 */
	strncpyzt(user->realhost, Inet_ia2p(&sptr->ip), sizeof(user->realhost));
	if (!user->ip_str)
		user->ip_str = strdup(Inet_ia2p(&sptr->ip));
	user->server = me_hash;
      user_finish:
	user->servicestamp = sstamp;
	strlcpy(sptr->info, realname, sizeof(sptr->info));
	if (sptr->name[0] && (IsServer(cptr) ? 1 : IsNotSpoof(sptr)))
		/* NICK and no-spoof already received, now we have USER... */
	{
		if (USE_BAN_VERSION && MyConnect(sptr))
			sendto_one(sptr, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
				me.name, sptr->name);

		return(
		    register_user(cptr, sptr, sptr->name, username, umodex,
		    virthost,ip));
	}
	else
		strncpyzt(sptr->user->username, username, USERLEN + 1);

	return 0;
}
