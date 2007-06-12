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


ModuleHeader MOD_HEADER(m_dummy)
  = {
	"dummy",	/* Name of module */
	"$Id$", /* Version */
	"command /dummy", /* Short description of module */
	"3.2-b8-1",
	NULL 
    };

/* This is called on module init, before Server Ready */
DLLFUNC int MOD_INIT(m_dummy)(ModuleInfo *modinfo)
{
	/*
	 * We call our CommandAdd crap here
	*/
	CommandAdd(modinfo->handle, MSG_DUMMY, TOK_DUMMY, m_dummy, MAXPARA, M_USER|M_SERVER);
	return MOD_SUCCESS;
}

/* Is first run when server is 100% ready */
DLLFUNC int MOD_LOAD(m_dummy)(int module_load)
{
	return MOD_SUCCESS;
}


/* Called when module is unloaded */
DLLFUNC int MOD_UNLOAD(m_dummy)(int module_unload)
{
	return MOD_SUCCESS;
}

DLLFUNC int m_dummy(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
}
