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
#ifndef STATIC_LINKING
#define DYNAMIC_LINKING
#else
#undef DYNAMIC_LINKING
#endif

#ifdef SCAN_API
extern MSymbolTable scan_socks_depend[];
#endif

/* Place includes here */
/* replace this with a common name of your module */
#ifdef DYNAMIC_LINKING
ModuleInfo mod_header
#else
ModuleInfo l_commands_info
#endif
  = {
  	2,
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
DLLFUNC int	mod_init(int module_load)
#else
int    l_commands_init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	m_sethost_init(module_load);
	m_setname_init(module_load);
	m_chghost_init(module_load);
	m_chgident_init(module_load);
	m_setident_init(module_load);
	m_sdesc_init(module_load);
	m_svsmode_init(module_load);
	m_swhois_init(module_load);
	m_svsmotd_init(module_load);
	m_svsnline_init(module_load);
	m_who_init(module_load);
#ifdef SCAN_API
	module_depend_resolve(&scan_socks_depend[0]);
	m_scan_init(module_load);
	scan_socks_init(module_load);
#endif

}
#ifdef DYNAMIC_LINKING
DLLFUNC void	mod_load(int module_load)
#else
void    l_commands_load(int module_load)
#endif
{
	m_sethost_load(module_load);
	m_setname_load(module_load);
	m_chghost_load(module_load);
	m_chgident_load(module_load);
	m_setident_load(module_load);
	m_sdesc_load(module_load);
	m_svsmode_load(module_load);
	m_swhois_load(module_load);
	m_svsmotd_load(module_load);
	m_svsnline_load(module_load);
	m_who_load(module_load);
#ifdef SCAN_API
	m_scan_load(module_load);
	scan_socks_load(module_load);
#endif
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
	m_svsmode_unload();
	m_swhois_unload();
	m_svsmotd_unload();
	m_svsnline_unload();
	m_who_unload();
#ifdef SCAN_API
	scan_socks_unload();
	m_scan_unload();
#endif
}

