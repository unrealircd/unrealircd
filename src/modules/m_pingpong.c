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

#include "config.h"
#include "struct.h"
#include "common.h"
#include "sys.h"
#include "numeric.h"
#include "msg.h"
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
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int m_nospoof(aClient *cptr, aClient *sptr, int parc, char *parv[]);


/* Place includes here */
#define MSG_PING        "PING"  /* PING */
#define TOK_PING        "8"     /* 56 */  
#define MSG_PONG        "PONG"  /* PONG */
#define TOK_PONG        "9"     /* 57 */  

ModuleHeader MOD_HEADER(m_pingpong)
  = {
	"pingpong",	/* Name of module */
	"$Id$", /* Version */
	"ping, pong and nospoof", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };
/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_pingpong)(ModuleInfo *modinfo)
{
	/*
	 * We call our add_Command crap here
	*/
	Debug((DEBUG_NOTICE, "INIT"));
	CommandAdd(modinfo->handle, MSG_PING, TOK_PING, m_ping, MAXPARA, 0);
	CommandAdd(modinfo->handle, MSG_PONG, TOK_PONG, m_pong, MAXPARA, M_UNREGISTERED|M_USER|M_SERVER|M_SHUN|M_VIRUS);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_pingpong)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_pingpong)(int module_unload)
{
	return MOD_SUCCESS;
}

/*
** m_ping
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
DLLFUNC int  m_ping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char *origin, *destination;

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
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

	if (!BadPtr(destination) && mycmp(destination, me.name) != 0)
	{
		if (MyClient(sptr))
			origin = sptr->name; /* Make sure origin is not spoofed */
		if ((acptr = find_server_quick(destination)) && (acptr != &me))
			sendto_one(acptr, ":%s PING %s :%s", parv[0], origin, destination);
		else
		{
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
			    me.name, parv[0], destination);
			return 0;
		}
	}
	else
		sendto_one(sptr, ":%s %s %s :%s", me.name,
		    IsToken(cptr) ? TOK_PONG : MSG_PONG,
		    (destination) ? destination : me.name, origin);
	return 0;
}

/*
** m_nospoof - allows clients to respond to no spoofing patch
**	parv[0] = prefix
**	parv[1] = code
*/
DLLFUNC int  m_nospoof(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	unsigned long result;
Debug((DEBUG_NOTICE, "NOSPOOF"));

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
	if (result != sptr->nospoof)
	{
		if (BadPtr(parv[2]))
			goto temp;
		result = strtoul(parv[2], NULL, 16);
		if (result != sptr->nospoof)
			goto temp;
	}
	sptr->nospoof = 0;
	if (USE_BAN_VERSION && MyConnect(sptr))
		sendto_one(sptr, ":IRC!IRC@%s PRIVMSG %s :\1VERSION\1",
			   me.name, sptr->name);

	if (sptr->user && sptr->name[0] && !CHECKPROTO(sptr, PROTO_CLICAP))
		return register_user(cptr, sptr, sptr->name,
		    sptr->user->username, NULL, NULL, NULL);
	return 0;
      temp:
	/* Homer compatibility */
	sendto_one(cptr, ":%X!nospoof@%s PRIVMSG %s :\1VERSION\1",
	    cptr->nospoof, me.name, cptr->name);
	return 0;
}

/*
** m_pong
**	parv[0] = sender prefix
**	parv[1] = origin
**	parv[2] = destination
*/
DLLFUNC int m_pong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;
	char *origin, *destination;

	if (!IsRegistered(cptr))
		return m_nospoof(cptr, sptr, parc, parv);

	if (parc < 2 || *parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NOORIGIN), me.name, parv[0]);
		return 0;
	}

	origin = parv[1];
	destination = parv[2];
	cptr->flags &= ~FLAGS_PINGSENT;
	sptr->flags &= ~FLAGS_PINGSENT;

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
				sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
				    me.name, parv[0], destination);
				return 0;
			}
			else
				sendto_one(acptr, ":%s PONG %s %s",
				    parv[0], origin, destination);
		}
		else
		{
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER),
			    me.name, parv[0], destination);
			return 0;
		}
	}
#ifdef	DEBUGMODE
	else
		Debug((DEBUG_NOTICE, "PONG: %s %s", origin,
		    destination ? destination : "*"));
#endif
	return 0;
}
