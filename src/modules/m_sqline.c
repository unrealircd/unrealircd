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

DLLFUNC int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_SQLINE      "SQLINE"        /* SQLINE */
#define TOK_SQLINE      "c"     /* 98 */


#ifndef DYNAMIC_LINKING
ModuleInfo m_sqline_info
#else
#define m_sqline_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"sqline",	/* Name of module */
	"$Id$", /* Version */
	"command /sqline", /* Short description of module */
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
int    m_sqline_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_SQLINE, TOK_SQLINE, m_sqline, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_sqline_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_sqline_unload(void)
#endif
{
	if (del_Command(MSG_SQLINE, TOK_SQLINE, m_sqline) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_sqline_info.name);
	}
}

/*    m_sqline
**	parv[0] = sender
**	parv[1] = nickmask
**	parv[2] = reason
*/
DLLFUNC int m_sqline(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	ConfigItem_ban	*bconf;
	/* So we do not make double entries */
	int		addit = 0;

	if (!IsServer(sptr) || parc < 2)
		return 0;

	if (parv[2])
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s :%s", parv[1], parv[2]);
	else
		sendto_serv_butone_token(cptr, parv[0], MSG_SQLINE, TOK_SQLINE,
		    "%s", parv[1]);

	/* Only replaces AKILL (global ban nick)'s */
	if (bconf = Find_banEx(parv[1], CONF_BAN_NICK, CONF_BAN_TYPE_AKILL))
	{
		if (bconf->mask)
			MyFree(bconf->mask);
		if (bconf->reason)
			MyFree(bconf->reason);
		bconf->mask = NULL;
		bconf->reason = NULL;
		addit = 0;
	}
	else
	{
		bconf = (ConfigItem_ban *) MyMallocEx(sizeof(ConfigItem_ban));
		addit = 1;
	}
	if (parv[2])
		DupString(bconf->reason, parv[2]);
	if (parv[1])
		DupString(bconf->mask, parv[1]);
		
	/* CONF_BAN_NICK && CONF_BAN_TYPE_AKILL == SQLINE */
	bconf->flag.type = CONF_BAN_NICK;
	bconf->flag.type2 = CONF_BAN_TYPE_AKILL;
	if (addit == 1)
		add_ConfigItem((ConfigItem *) bconf, (ConfigItem **) &conf_ban);

}
