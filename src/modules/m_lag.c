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
#include <fcntl.h>
#include "h.h"
#ifdef STRIPBADWORDS
#include "badwords.h"
#endif
#ifdef _WIN32
#include "version.h"
#endif

DLLFUNC int m_lag(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_LAG         "LAG"   /* Lag detect */
#define TOK_LAG         "AF"    /* a or ? */

#ifndef DYNAMIC_LINKING
ModuleInfo m_lag_info
#else
#define m_lag_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"lag",	/* Name of module */
	"$Id$", /* Version */
	"command /lag", /* Short description of module */
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
int    m_lag_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_LAG, TOK_LAG, m_lag, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_lag_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_lag_unload(void)
#endif
{
	if (del_Command(MSG_LAG, TOK_LAG, m_lag) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_lag_info.name);
	}
}

/* m_lag (lag measure) - Stskeeps
 * parv[0] = prefix
 * parv[1] = server to query
*/

DLLFUNC int m_lag(aClient *cptr, aClient *sptr, int parc, char *parv[])
{

	if (MyClient(sptr))
		if (!IsAnOper(sptr))
		{
			sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name,
			    parv[0]);
			return 0;
		}

	if (parc < 2)
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (*parv[1] == '\0')
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "LAG");
		return 0;
	}
	if (hunt_server(cptr, sptr, ":%s LAG :%s", 1, parc,
	    parv) == HUNTED_NOSUCH)
	{
		return 0;
	}

	sendto_one(sptr, ":%s NOTICE %s :Lag reply -- %s %s %li",
	    me.name, sptr->name, me.name, parv[1], TStime());

	return 0;
}
