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

DLLFUNC int m_dummy(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */
#define MSG_DUMMY 	"DUMMY"	/* dummy */
#define TOK_DUMMY 	"DU"	/* 127 4ever !;) */

#ifndef STATIC_LINKING
ModuleInfo mod_header
#else
ModuleInfo m_dummy_info
#endif
  = {
  	2,
	"dummy",	/* Name of module */
	"$Id$", /* Version */
	"command /dummy", /* Short description of module */
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
int    m_dummy_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_DUMMY, TOK_DUMMY, m_dummy, MAXPARA);
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_dummy_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_dummy_unload(void)
#endif
{
	if (del_Command(MSG_DUMMY, TOK_DUMMY, m_dummy) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_dummy_info.name);
	}
}

DLLFUNC int m_dummy(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
}
