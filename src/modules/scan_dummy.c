/*
 * Scanning API Client Skeleton, by Carsten V. Munk 2001 <stskeeps@tspre.org>
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
#include "userload.h"
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
#include "modules/scan.h"


void	scan_dummy_scan(HStruct *h);

ModuleInfo scan_dummy_info
  = {
  	1,
	"scan_dummy",	/* Name of module */
	"$Id$", /* Version */
	"scanning API: dummy", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

/* This is called on module init, before Server Ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_init(void)
#else
void    scan_dummy_init(void)
#endif
{
	/* extern variable to export scan_dummy_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	module_buffer = &scan_dummy_info;
	/*
	 * Add scanning hooks
	*/
	add_HookX(HOOKTYPE_SCAN_HOST, NULL, scan_dummy_scan); 
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(void)
#else
void    scan_dummy_load(void)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	scan_dummy_unload(void)
#endif
{
	del_HookX(HOOKTYPE_SCAN_HOST, NULL, scan_dummy_scan);
}

void	scan_dummy_scan(HStruct *h)
{
	char	host[SCAN_HOSTLENGTH];

	/* Get host */
	IRCMutexLock(HSlock);
	strcpy(host, h->host);
	IRCMutexUnlock(HSlock);
	IRCMutexLock(VSlock);
	IRCMutexUnlock(VSlock);

	/* Indicate we don't use that structure anymore */
	IRCMutexLock(HSlock);
	h->refcnt--;
	IRCMutexUnlock(HSlock);
	IRCExitThread(NULL);
}
