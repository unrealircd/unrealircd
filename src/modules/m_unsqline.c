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

DLLFUNC int m_unsqline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_UNSQLINE    "UNSQLINE"      /* UNSQLINE */
#define TOK_UNSQLINE    "d"     /* 99 */  


#ifndef DYNAMIC_LINKING
ModuleInfo m_unsqline_info
#else
#define m_unsqline_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"unsqline",	/* Name of module */
	"$Id$", /* Version */
	"command /unsqline", /* Short description of module */
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
int    m_unsqline_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_unsqline_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_unsqline_unload(void)
#endif
{
	if (del_Command(MSG_UNSQLINE, TOK_UNSQLINE, m_unsqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_unsqline_info.name);
	}
}

/*    m_unsqline
**	parv[0] = sender
**	parv[1] = nickmask
*/
DLLFUNC int m_unsqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban *bconf;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	sendto_serv_butone_token(cptr, parv[0], MSG_UNSQLINE, TOK_UNSQLINE,
	    "%s", parv[1]);

	if (bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL))
	{
		del_ConfigItem(bconf, &conf_ban);
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		MyFree(bconf);
	}
	else
		return;

	return 0;
}
