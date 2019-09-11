/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_pingpong.c
 *   (C) 2000-2001 Carsten V. Munk and the UnrealIRCd Team
 *   Moved to modules by Fish (Justin Hammond)
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

CMD_FUNC(m_ping);
CMD_FUNC(m_pong);
CMD_FUNC(m_nospoof);

/* Place includes here */
#define MSG_PING        "PING"  /* PING */
#define MSG_PONG        "PONG"  /* PONG */

ModuleHeader MOD_HEADER(pingpong)
  = {
	"pingpong",	/* Name of module */
	"5.0", /* Version */
	"ping, pong and nospoof", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };
/* This is called on module init, before Server Ready */
MOD_INIT(pingpong)
{
	CommandAdd(modinfo->handle, MSG_PING, m_ping, MAXPARA, M_USER|M_SERVER|M_SHUN);
	CommandAdd(modinfo->handle, MSG_PONG, m_pong, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER|M_SHUN|M_VIRUS);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD(pingpong)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD(pingpong)
{
	return MOD_SUCCESS;
}

/*
** m_ping
**	parv[1] = origin
**	parv[2] = destination
*/
CMD_FUNC(m_ping)
{
	Client *acptr;
	char *origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NOORIGIN);
		return 0;
	}
	origin = parv[1];
	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if (!MyClient(sptr))
	{
		/* I've no idea who invented this or what it is supposed to do.. */
		acptr = find_client(origin, NULL);
		if (!acptr)
			acptr = find_server_quick(origin);
		if (acptr && acptr != sptr)
			origin = cptr->name;
	}

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0 && mycmp(destination, me.id) != 0)
	{
		if (MyClient(sptr))
			origin = sptr->name; /* Make sure origin is not spoofed */
		if ((acptr = find_server_quick(destination)) && (acptr != &me))
			sendto_one(acptr, NULL, ":%s PING %s :%s", sptr->name, origin, destination);
		else
		{
			sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
			return 0;
		}
	}
	else
		sendto_one(sptr, NULL, ":%s PONG %s :%s", me.name,
		    (destination) ? destination : me.name, origin);
	return 0;
}

/*
** m_nospoof - allows clients to respond to no spoofing patch
**	parv[1] = code
*/
CMD_FUNC(m_nospoof)
{
	unsigned long result;

	if (IsNotSpoof(cptr))
		return 0;
	if (IsRegistered(cptr))
		return 0;
	if (!*sptr->name)
		return 0;
	if (BadPtr(parv[1]))
		goto temp;
	result = strtoul(parv[1], NULL, 16);
	/* Accept code in second parameter (ircserv) */
	if (result != sptr->local->nospoof)
	{
		if (BadPtr(parv[2]))
			goto temp;
		result = strtoul(parv[2], NULL, 16);
		if (result != sptr->local->nospoof)
			goto temp;
	}
	sptr->local->nospoof = 0;
	if (USE_BAN_VERSION && MyConnect(sptr))
		sendto_one(sptr, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
			   me.name, sptr->name);

	if (is_handshake_finished(sptr))
		return register_user(cptr, sptr, sptr->name,
		    sptr->user->username, NULL, NULL, NULL);
	return 0;
      temp:
	/* Homer compatibility */
	sendto_one(cptr, NULL, ":%X!nospoof@%s PRIVMSG %s :\1VERSION\1",
	    cptr->local->nospoof, me.name, cptr->name);
	return 0;
}

/*
** m_pong
**	parv[1] = origin
**	parv[2] = destination
*/
CMD_FUNC(m_pong)
{
	Client *acptr;
	char *origin, *destination;

	if (!IsRegistered(cptr))
		return m_nospoof(cptr, sptr, recv_mtags, parc, parv);

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NOORIGIN);
		return 0;
	}

	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;
	ClearPingWarning(cptr);

	/* Remote pongs for clients? uhh... */
	if (MyClient(sptr) || !IsRegistered(sptr))
		destination = NULL;

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	{
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server_quick(destination)))
		{
			if (!IsServer(cptr) && !IsServer(acptr))
			{
				sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
				return 0;
			}
			else
				sendto_one(acptr, NULL, ":%s PONG %s %s",
				    sptr->name, origin, destination);
		}
		else
		{
			sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
			return 0;
		}
	}

	return 0;
}
