/*
 * Module skeleton, by Carsten V. Munk 2001 <stskeeps@tspre.org>
 * May be used, modified, or changed by anyone, no license applies.
 * You may relicense this, to any license
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

#ifndef DYNAMIC_LINKING
ModuleInfo m_rping_info
#else
#define m_rping_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"rping",	/* Name of module */
	"$Id$", /* Version */
	"command /rping, /rpong", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };


/* The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_init(int module_load)
#else
int    m_rping_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_RPING, TOK_RPING, m_rping, MAXPARA);
	add_Command(MSG_RPONG, TOK_RPONG, m_rpong, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_rping_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_rping_unload(void)
#endif
{
	if (del_Command(MSG_RPING, TOK_RPING, m_rping) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_rping_info.name);
	}
	if (del_Command(MSG_RPONG, TOK_RPONG, m_rpong) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_rping_info.name);
	}
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
		return 0;

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
		if (hunt_server(cptr, sptr, ":%s RPING %s %s :%s", 2, parc,
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
		if (hunt_server(cptr, sptr, ":%s RPING %s %s %s %s :%s", 1,
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
/* Now just as accurate on win as on linux -- codemastr */
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
		    (tv.time - atoi(sec)) * 1000 + (tv.millitm - atoi(usec)) / 1000);
#endif
	else
#ifndef _WIN32
		ircsprintf(timebuf, "%ld %ld", tv.tv_sec, tv.tv_usec);
#else
		ircsprintf(timebuf, "%ld %ld", tv.time, tv.millitm);
#endif
	return timebuf;
}
