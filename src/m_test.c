/*
 * Typical UnrealIRCd module skeleton
 * (C) Carsten V. Munk 2000, may be used for everything
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

/* Place includes here */
/*
 * Run a search and replace on modulename to a unique name of your module,
 * like dummy
 * -link will fail else
*/
#define MSG_DUMMY "DUMMY"
#define TOK_DUMMY "DU"

DLLFUNC int m_dummy(aClient *cptr, aClient *sptr, char *parv[], int parc);

ModuleInfo modulename_info
  = {
  	1,
	"dummy",	/* Name of module */
	"$Id$", /* Version */
	"description", /* Short description of module */
	NULL, /* Pointer to our dlopen() return value */
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_init(void)
#else
void    modulename_init(void)
#endif
{
	/* extern variable to export modulename_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	module_buffer = &modulename_info;
	/*
	 * We call our add_Command crap here
	*/
	add_Command(MSG_DUMMY, TOK_DUMMY, m_dummy, MAXPARA);
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	modulename_unload(void)
#endif
{
	if (del_Command(MSG_DUMMY, TOK_DUMMY, m_dummy) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				modulename_info.name);
	}
	/* do etc stuff here */
	sendto_ops("Unloaded");
}

DLLFUNC int	m_dummy(aClient *cptr, aClient *sptr, char *parv[], int parc)
{
	sendto_ops("MOOO");
}
