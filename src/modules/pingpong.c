/*
 *   Unreal Internet Relay Chat Daemon, src/modules/pingpong.c
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

CMD_FUNC(cmd_ping);
CMD_FUNC(cmd_pong);
CMD_FUNC(cmd_nospoof);

/* Place includes here */
#define MSG_PING        "PING"  /* PING */
#define MSG_PONG        "PONG"  /* PONG */

ModuleHeader MOD_HEADER
  = {
	"pingpong",	/* Name of module */
	"5.0", /* Version */
	"ping, pong and nospoof", /* Short description of module */
	"UnrealIRCd Team",
	"unrealircd-5",
    };
/* This is called on module init, before Server Ready */
MOD_INIT()
{
	CommandAdd(modinfo->handle, MSG_PING, cmd_ping, MAXPARA, CMD_USER|CMD_SERVER|CMD_SHUN);
	CommandAdd(modinfo->handle, MSG_PONG, cmd_pong, MAXPARA, CMD_UNREGISTERED|CMD_USER|CMD_SERVER|CMD_SHUN|CMD_VIRUS);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
MOD_LOAD()
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
MOD_UNLOAD()
{
	return MOD_SUCCESS;
}

/*
** cmd_ping
**	parv[1] = origin
**	parv[2] = destination
*/
CMD_FUNC(cmd_ping)
{
	Client *acptr;
	char *origin, *destination;

	if (parc < 2 || BadPtr(parv[1]))
	{
		sendnumeric(sptr, ERR_NOORIGIN);
		return;
	}

	origin = parv[1];
	destination = parv[2];	/* Will get NULL or pointer (parc >= 2!!) */

	if (!MyUser(sptr))
		origin = sptr->name;

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0 && mycmp(destination, me.id) != 0)
	{
		if (MyUser(sptr))
			origin = sptr->name; /* Make sure origin is not spoofed */
		if ((acptr = find_server_quick(destination)) && (acptr != &me))
			sendto_one(acptr, NULL, ":%s PING %s :%s", sptr->name, origin, destination);
		else
		{
			sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
			return;
		}
	}
	else
		sendto_one(sptr, NULL, ":%s PONG %s :%s", me.name,
		    (destination) ? destination : me.name, origin);
}

/*
** cmd_nospoof - allows clients to respond to no spoofing patch
**	parv[1] = code
*/
CMD_FUNC(cmd_nospoof)
{
	unsigned long result;

	if (IsNotSpoof(sptr))
		return;
	if (IsRegistered(sptr))
		return;
	if (!*sptr->name)
		return;
	if (BadPtr(parv[1]))
	{
		sendnotice(sptr, "ERROR: Invalid PING response. Your client must respond back with PONG :<cookie>");
		return;
	}

	result = strtoul(parv[1], NULL, 16);

	if (result != sptr->local->nospoof)
	{
		/* Apparently we also accept PONG <irrelevant> <cookie>... */
		if (BadPtr(parv[2]))
		{
			sendnotice(sptr, "ERROR: Invalid PING response. Your client must respond back with PONG :<cookie>");
			return;
		}
		result = strtoul(parv[2], NULL, 16);
		if (result != sptr->local->nospoof)
		{
			sendnotice(sptr, "ERROR: Invalid PING response. Your client must respond back with PONG :<cookie>");
			return;
		}
	}

	sptr->local->nospoof = 0;

	if (USE_BAN_VERSION && MyConnect(sptr))
		sendto_one(sptr, NULL, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
			   me.name, sptr->name);

	if (is_handshake_finished(sptr))
		register_user(sptr, sptr->name, sptr->user->username, NULL, NULL, NULL);
}

/*
** cmd_pong
**	parv[1] = origin
**	parv[2] = destination
*/
CMD_FUNC(cmd_pong)
{
	Client *acptr;
	char *origin, *destination;

	if (!IsRegistered(sptr))
		return cmd_nospoof(sptr, recv_mtags, parc, parv);

	if (parc < 2 || *parv[1] == '\0')
	{
		sendnumeric(sptr, ERR_NOORIGIN);
		return;
	}

	origin = parv[1];
	destination = parv[2];
	ClearPingSent(sptr);
	ClearPingWarning(sptr);

	/* Remote pongs for clients? uhh... */
	if (MyUser(sptr) || !IsRegistered(sptr))
		return;

	/* PONG from a server - either for us, or needs relaying.. */
	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	{
		if ((acptr = find_client(destination, NULL)) ||
		    (acptr = find_server_quick(destination)))
		{
			if (IsUser(sptr) && !IsServer(acptr))
			{
				sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
				return;
			} else
			{
				sendto_one(acptr, NULL, ":%s PONG %s %s",
				    sptr->name, origin, destination);
			}
		}
		else
		{
			sendnumeric(sptr, ERR_NOSUCHSERVER, destination);
			return;
		}
	}
}
