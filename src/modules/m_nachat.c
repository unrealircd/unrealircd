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

DLLFUNC int m_nachat(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_NACHAT      "NACHAT"        /* netadmin chat */
#define TOK_NACHAT      "AC"    /* *beep* */

#ifndef DYNAMIC_LINKING
ModuleInfo m_nachat_info
#else
#define m_nachat_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"Nachat",	/* Name of module */
	"$Id$", /* Version */
	"command /nachat", /* Short description of module */
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
int    m_nachat_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_NACHAT, TOK_NACHAT, m_nachat, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_nachat_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_nachat_unload(void)
#endif
{
	if (del_Command(MSG_NACHAT, TOK_NACHAT, m_nachat) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_nachat_info.name);
	}
}

/*
** m_nachat (netAdmin chat only) -Potvin - another sts cloning
**      parv[0] = sender prefix
**      parv[1] = message text
*/
DLLFUNC int m_nachat(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
	char *message;


	message = parc > 1 ? parv[1] : NULL;

	if (BadPtr(message))
	{
		sendto_one(sptr, err_str(ERR_NEEDMOREPARAMS),
		    me.name, parv[0], "NACHAT");
		return 0;
	}
#ifdef ADMINCHAT
	if (MyClient(sptr))
		if (!(IsNetAdmin(sptr) || IsTechAdmin(sptr)))
#else
	if (MyClient(sptr))
#endif
	{
		sendto_one(sptr, err_str(ERR_NOPRIVILEGES), me.name, parv[0]);
		return 0;
	}

	sendto_serv_butone_token(IsServer(cptr) ? cptr : NULL, parv[0],
	   MSG_NACHAT, TOK_NACHAT, ":%s", message);
#ifdef ADMINCHAT
	sendto_umode(UMODE_NETADMIN, "*** NetAdmin.Chat -- from %s: %s",
	    parv[0], message);
	sendto_umode(UMODE_TECHADMIN, "*** NetAdmin.Chat -- from %s: %s",
	    parv[0], message);
#endif
	return 0;
}
