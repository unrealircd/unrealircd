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

DLLFUNC int m_tsctl(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_TSCTL       "TSCTL"
#define TOK_TSCTL       "AW"


#ifndef DYNAMIC_LINKING
ModuleInfo m_tsctl_info
#else
#define m_tsctl_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"tsctl",	/* Name of module */
	"$Id$", /* Version */
	"command /tsctl", /* Short description of module */
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
int    m_tsctl_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_TSCTL, TOK_TSCTL, m_tsctl, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_tsctl_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_tsctl_unload(void)
#endif
{
	if (del_Command(MSG_TSCTL, TOK_TSCTL, m_tsctl) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_tsctl_info.name);
	}
}

/*
** m_tsctl - Stskeeps
**      parv[0] = sender prefix
**      parv[1] = command
**      parv[2] = options
*/

DLLFUNC int m_tsctl(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	time_t timediff;


	if (!MyClient(sptr))
		goto doit;
	if (!IsAdmin(sptr) && !IsCoAdmin(sptr))
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}
      doit:
	if (parv[1])
	{
		if (*parv[1] == '\0')
		{
			sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
			    me.name, parv[0], "TSCTL");
			return 0;
		}

		if (strcmp(parv[1], "offset") == 0)
		{
			if (!parv[3])
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (*parv[2] == '\0' || *parv[3] == '\0')
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;
			}
			if (!(*parv[2] == '+' || *parv[2] == '-'))
			{
				sendto_one(sptr,
				    ":%s NOTICE %s :*** TSCTL OFFSET: /tsctl offset <+|-> <time>",
				    me.name, sptr->name);
				return 0;

			}

			switch (*parv[2])
			{
			  case '+':
				  timediff = atol(parv[3]);
				  TSoffset = timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed +%li",
				      sptr->name, timediff);
				  sendto_serv_butone(&me,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed +%li",
				      me.name, sptr->name, timediff);
				  break;
			  case '-':
				  timediff = atol(parv[3]);
				  TSoffset = -timediff;
				  sendto_ops
				      ("TS Control - %s set TStime() to be diffed -%li",
				      sptr->name, timediff);
				  sendto_serv_butone(&me,
				      ":%s GLOBOPS :TS Control - %s set TStime to be diffed -%li",
				      me.name, sptr->name, timediff);
				  break;
			}
			return 0;
		}
		if (strcmp(parv[1], "time") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, TStime(), time(NULL),
			    TSoffset);
			return 0;
		}
		if (strcmp(parv[1], "alltime") == 0)
		{
			sendto_one(sptr,
			    ":%s NOTICE %s :*** Server=%s TStime=%li time()=%li TSoffset=%li",
			    me.name, sptr->name, me.name, TStime(), time(NULL),
			    TSoffset);
			sendto_serv_butone(cptr, ":%s TSCTL alltime",
			    sptr->name);
			return 0;

		}
		if (strcmp(parv[1], "svstime") == 0)
		{
			if (parv[2] == '\0')
			{
				return 0;
			}
			if (!IsULine(sptr))
			{
				return 0;
			}

			timediff = atol(parv[2]);
			timediff = timediff - time(NULL);
			TSoffset = timediff;
			sendto_ops
			    ("TS Control - U:line set time to be %li (timediff: %li)",
			    atol(parv[2]), timediff);
			sendto_serv_butone(cptr, ":%s TSCTL svstime %li",
			    sptr->name, atol(parv[2]));
			return 0;
		}
	}
}


