/*
 *   Unreal Internet Relay Chat Daemon  - src/l_commands.c
 *   (C) 2000 Carsten Munk (Techie/Stskeeps) <stskeeps@tspre.org>
 *   
 *   Wrapper for making commands.so
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 */
#define DYNAMIC_LINKING
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
/* replace this with a common name of your module */
ModuleInfo l_commands_info
  = {
  	1,
	"commands",	/* Name of module */
	"$Id$", /* Version */
	"Wrapper library for m_ commands", /* Short description of module */
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
void    l_commands_init(void)
#endif
{
	/* extern variable to export l_commands_info to temporary
           ModuleInfo *modulebuffer;
	   the module_load() will use this to add to the modules linked 
	   list
	*/
	module_buffer = &l_commands_info;
	/*
	 * We call our add_Command crap here
	*/
	m_sethost_init();
	m_setname_init();
	m_chghost_init();
	m_chgident_init();
	m_setident_init();
	m_sdesc_init();
	
	module_buffer = &l_commands_info;
}

#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_unload(void)
#else
void	l_commands_unload(void)
#endif
{
	m_sethost_unload();
	m_setname_unload();
	m_chghost_unload();
	m_chgident_unload();
        m_setident_unload();
        m_sdesc_unload();
}

