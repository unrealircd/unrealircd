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

DLLFUNC int m_%COMMAND%(aClient *cptr, aClient *sptr, int parc, char *parv[]);

#define MSG_%UCOMMAND% 	"%UCOMMAND%"	
#define TOK_%UCOMMAND% 	"%TOKEN%"	


ModuleInfo m_%COMMAND%_info
  = {
  	1,
	"test",
	"$Id$",
	"command /%COMMAND%", 
	NULL,
	NULL 
    };

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_init(void)
#else
void    m_%COMMAND%_init(void)
#endif
{
	module_buffer = &m_%COMMAND%_info;
	add_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%, %MAXPARA%);
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(void)
#else
void    m_%COMMAND%_load(void)
#endif
{
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	m_%COMMAND%_unload(void)
#endif
{
	if (del_Command(MSG_%UCOMMAND%, TOK_%UCOMMAND%, m_%COMMAND%) < 0)
	{
		sendto_realops("Failed to delete commands when unloading %s",
				m_%COMMAND%_info.name);
	}
}
