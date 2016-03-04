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

#include "unrealircd.h"

DLLFUNC CMD_FUNC(m_user);

#define MSG_USER 	"USER"	

ModuleHeader MOD_HEADER(m_user)
  = {
	"m_user",
	"4.0",
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
DLLFUNC CMD_FUNC(m_user)
{
#define	UFLAGS	(UMODE_INVISIBLE|UMODE_WALLOP|UMODE_SERVNOTICE)
	char *username, *host, *server, *realname, *umodex = NULL, *virthost =
	    NULL, *ip = NULL;
	char *sstamp = NULL;
	aClient *acptr;

	if (IsServer(cptr) && !IsUnknown(sptr))
		return 0;

	if (MyConnect(sptr) && (sptr->local->listener->options & LISTENER_SERVERSONLY))
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
		    me.name, sptr->name, "USER");
		if (IsServer(cptr))
			sendto_ops("bad USER param count for %s from %s",
			    sptr->name, get_client_name(cptr, FALSE));
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
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[5])) ? "<bad-realname>" : parv[5];
		umodex = NULL;
	}
	else if (parc == 8 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[7])) ? "<bad-realname>" : parv[7];
		umodex = parv[5];
		virthost = parv[6];
	}
	else if (parc == 9 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[8])) ? "<bad-realname>" : parv[8];
		umodex = parv[5];
		virthost = parv[6];
		ip = parv[7];
	}
	else if (parc == 10 && IsServer(cptr))
	{
		sstamp = (BadPtr(parv[4])) ? "0" : parv[4];
		realname = (BadPtr(parv[9])) ? "<bad-realname>" : parv[9];
		umodex = parv[5];
		virthost = parv[6];
		ip = parv[8];
	}
	else
	{
		realname = (BadPtr(parv[4])) ? "<bad-realname>" : parv[4];
	}
	
	make_user(sptr);

	if (!MyConnect(sptr))
	{
		if (sptr->srvptr == NULL)
			sendto_ops("WARNING, User %s introduced as being "
			    "on non-existant server %s.", sptr->name, server);
		sptr->user->server = find_or_add(sptr->srvptr->name);
		strlcpy(sptr->user->realhost, host, sizeof(sptr->user->realhost));
		goto user_finish;
	}

	if (!IsUnknown(sptr))
	{
		sendto_one(sptr, err_str(ERR_ALREADYREGISTRED),
		    me.name, sptr->name);
		return 0;
	}

	if (!IsServer(cptr))
	{
		sptr->umodes |= CONN_MODES;
		if (CONNECT_SNOMASK)
		{
			sptr->umodes |= UMODE_SERVNOTICE;
			create_snomask(sptr, sptr->user, CONNECT_SNOMASK);
		}
	}

	sptr->user->server = me_hash;
      user_finish:
	if (sstamp != NULL && *sstamp != '*')
		strlcpy(sptr->user->svid, sstamp, sizeof(sptr->user->svid));

	strlcpy(sptr->info, realname, sizeof(sptr->info));
	if (*sptr->name &&
		(IsServer(cptr) || (IsNotSpoof(sptr) && !CHECKPROTO(sptr, PROTO_CLICAP)))
           )
		/* NICK and no-spoof already received, now we have USER... */
	{
		if (USE_BAN_VERSION && MyConnect(sptr))
			sendto_one(sptr, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
				me.name, sptr->name);
		if (strlen(username) > USERLEN)
			username[USERLEN] = '\0'; /* cut-off */
		return(
		    register_user(cptr, sptr, sptr->name, username, umodex,
		    virthost,ip));
	}
	else
		strlcpy(sptr->user->username, username, USERLEN + 1);

	return 0;
}
