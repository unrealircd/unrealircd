/*
 *   Unreal Internet Relay Chat Daemon, src/modules/m_rping.c
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
#include <sys/timeb.h>
#include <fcntl.h>
#include "h.h"
#include "proto.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_rping(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC int  m_rpong(aClient *cptr, aClient *sptr, int parc, char *parv[]);
DLLFUNC char *militime(char *sec, char *usec);

/* Place includes here */
#define MSG_RPING       "RPING"
#define TOK_RPING       "AM"
#define MSG_RPONG       "RPONG"
#define TOK_RPONG       "AN"

ModuleHeader MOD_HEADER(m_rping)
  = {
	"rping",	/* Name of module */
	"$Id$", /* Version */
	"command /rping, /rpong", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_rping)(ModuleInfo *modinfo)
{
	CommandAdd(modinfo->handle, MSG_RPING, TOK_RPING, m_rping, MAXPARA, 0);
	CommandAdd(modinfo->handle, MSG_RPONG, TOK_RPONG, m_rpong, MAXPARA, 0);
	MARK_AS_OFFICIAL_MODULE(modinfo);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_rping)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_rping)(int module_unload)
{
	return MOD_SUCCESS;	
}

/*
 * m_rping  -- by Run
 *
 *    parv[0] = sender (sptr->name thus)
 * if sender is a person: (traveling towards start server)
 *    parv[1] = pinged server[mask]
 *    parv[2] = start server (current target)
 *    parv[3] = optional remark
 * if sender is a server: (traveling towards pinged server)
 *    parv[1] = pinged server (current target)
 *    parv[2] = original sender (person)
 *    parv[3] = start time in s
 *    parv[4] = start time in us
 *    parv[5] = the optional remark
 */
DLLFUNC int  m_rping(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (!IsPrivileged(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
	
	if (parc < (IsAnOper(sptr) ? (MyConnect(sptr) ? 2 : 3) : 6))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS), me.name, parv[0],
		    "RPING");
		return 0;
	}
	if (MyClient(sptr))
	{
		if (parc == 2)
			parv[parc++] = me.name;
		else if (!(acptr =  (aClient *)find_match_server(parv[2])))
		{
			parv[3] = parv[2];
			parv[2] = me.name;
			parc++;
		}
		else
			parv[2] = acptr->name;
		if (parc == 3)
			parv[parc++] = "<No client start time>";
	}

	if (IsAnOper(sptr))
	{
		if (hunt_server_token(cptr, sptr, MSG_RPING, TOK_RPING, "%s %s :%s", 2, parc,
		    parv) != HUNTED_ISME)
			return 0;
		if (!(acptr = (aClient *)find_match_server(parv[1])) || !IsServer(acptr))
		{
			sendto_one(sptr, err_str(ERR_NOSUCHSERVER), me.name,
			    parv[0], parv[1]);
			return 0;
		}
		sendto_one(acptr, ":%s RPING %s %s %s :%s",
		    me.name, acptr->name, sptr->name, militime(NULL, NULL),
		    parv[3]);
	}
	else
	{
		if (hunt_server_token(cptr, sptr, MSG_RPING, TOK_RPING, "%s %s %s %s :%s", 1,
		    parc, parv) != HUNTED_ISME)
			return 0;
		sendto_one(cptr, ":%s RPONG %s %s %s %s :%s", me.name, parv[0],
		    parv[2], parv[3], parv[4], parv[5]);
	}
	return 0;
}
/*
 * m_rpong  -- by Run too :)
 *
 * parv[0] = sender prefix
 * parv[1] = from pinged server: start server; from start server: sender
 * parv[2] = from pinged server: sender; from start server: pinged server
 * parv[3] = pingtime in ms
 * parv[4] = client info (for instance start time)
 */
DLLFUNC int  m_rpong(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	aClient *acptr;

	if (!IsServer(sptr))
		return 0;

	if (parc < 5)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "RPING");
		return 0;
	}

	/* rping blowbug */
	if (!(acptr = find_client(parv[1], (aClient *)NULL)))
		return 0;


	if (!IsMe(acptr))
	{
		if (IsServer(acptr) && parc > 5)
		{
			sendto_one(acptr, ":%s RPONG %s %s %s %s :%s",
			    parv[0], parv[1], parv[2], parv[3], parv[4],
			    parv[5]);
			return 0;
		}
	}
	else
	{
		parv[1] = parv[2];
		parv[2] = sptr->name;
		parv[0] = me.name;
		parv[3] = militime(parv[3], parv[4]);
		parv[4] = parv[5];
		if (!(acptr = find_person(parv[1], (aClient *)NULL)))
			return 0;	/* No bouncing between servers ! */
	}

	sendto_one(acptr, ":%s RPONG %s %s %s :%s",
	    parv[0], parv[1], parv[2], parv[3], parv[4]);
	return 0;
}


DLLFUNC char *militime(char *sec, char *usec)
{
/* Now just as accurate on win as on linux -- codemastr
 * Actually no, coz windows has millitime and *NIX uses microtime,
 * this is a ugly *1000 workaround. One might consider breaking
 * compatability and just use msec instead of usec crap. -- Syzop
 */

#ifndef _WIN32
	struct timeval tv;
#else
	struct _timeb tv;
#endif
	static char timebuf[18];
#ifndef _WIN32
	gettimeofday(&tv, NULL);
#else
	_ftime(&tv);
#endif
	if (sec && usec)
		ircsprintf(timebuf, "%ld",
#ifndef _WIN32
		    (tv.tv_sec - atoi(sec)) * 1000 + (tv.tv_usec - atoi(usec)) / 1000);
#else
		    (tv.time - atoi(sec)) * 1000 + (tv.millitm - (atoi(usec)/1000)));
#endif
	else
#ifndef _WIN32
		ircsprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
#else
		ircsprintf(timebuf, "%ld %ld", tv.time, tv.millitm * 1000);
#endif
	return timebuf;
}
