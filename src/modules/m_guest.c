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
#define snprintf _snprintf
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

DLLFUNC int m_guest(aClient *cptr, aClient *sptr, int parc, char *parv[]);

/* Place includes here */

#ifndef DYNAMIC_LINKING
ModuleInfo m_guest_info
#else
#define m_guest_info mod_header
ModuleInfo mod_header
#endif
  = {
  	2,
	"guest",	/* Name of module */
	"$Id$", /* Version */
	"command /guest", /* Short description of module */
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
int    m_guest_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
#ifdef GUEST
	add_Hook(HOOKTYPE_GUEST, m_guest);
#endif
}

/* Is first run when server is 100% ready */
#ifdef DYNAMIC_LINKING
DLLFUNC int	mod_load(int module_load)
#else
int    m_guest_load(int module_load)
#endif
{
}


/* Called when module is unloaded */
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_guest_unload(void)
#endif
{
#ifdef GUEST
	del_Hook(HOOKTYPE_GUEST, m_guest);
#endif
}



DLLFUNC int m_guest(aClient *cptr, aClient *sptr, int parc, char *parv[])
{
int randnum;
char guestnick[NICKLEN];
char *param[2];

randnum = 1+(int) (99999.0*rand()/(RAND_MAX+10000.0));
snprintf(guestnick, NICKLEN, "Guest%li", randnum);

while(find_client(guestnick, (aClient *)NULL))
{ 
randnum = 1+(int) (99999.0*rand()/(RAND_MAX+10000.0));
snprintf(guestnick, NICKLEN, "Guest%li", randnum);
}
param[0] = sptr->name;
param[1] = guestnick;
m_nick(sptr,cptr,2,param);
}
