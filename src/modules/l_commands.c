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
extern MSymbolTable scan_http_depend[];
#endif

/* Place includes here */
/* replace this with a common name of your module */
#ifdef DYNAMIC_LINKING
ModuleHeader Mod_Header
#else
ModuleHeader l_commands_Header
#endif
  = {
	"commands",	/* Name of module */
	"$Id$", /* Version */
	"Wrapper library for m_ commands", /* Short description of module */
	"3.2-b5",
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    l_commands_Init(int module_load)
#endif
{
	/*
	 * We call our add_Command crap here
	*/
	m_sethost_Init(module_load);
	m_setname_Init(module_load);
	m_chghost_Init(module_load);
	m_chgident_Init(module_load);
	m_setident_Init(module_load);
	m_sdesc_Init(module_load);
	m_svsmode_Init(module_load);
	m_swhois_Init(module_load);
	m_svsmotd_Init(module_load);
	m_svsnline_Init(module_load);
	m_who_Init(module_load);
	m_mkpasswd_Init(module_load);
	m_away_Init(module_load);
	m_svsnoop_Init(module_load);
	m_svso_Init(module_load);
	m_svsnick_Init(module_load);
	m_adminchat_Init(module_load);
	m_techat_Init(module_load);
	m_nachat_Init(module_load);
	m_lag_Init(module_load);
	m_rping_Init(module_load);
	m_sendumode_Init(module_load);
	m_tsctl_Init(module_load);
	m_htm_Init(module_load);
	m_chgname_Init(module_load);
	m_message_Init(module_load);
	m_whois_Init(module_load);
	m_quit_Init(module_load);
	m_kill_Init(module_load);
	m_pingpong_Init(module_load);
	m_oper_Init(module_load);
	m_akill_Init(module_load);
	m_rakill_Init(module_load);
	m_zline_Init(module_load);
	m_unzline_Init(module_load);
	m_kline_Init(module_load);
	m_unkline_Init(module_load);
	m_sqline_Init(module_load);
	m_unsqline_Init(module_load);
#ifdef GUEST
	m_guest_Init(module_load);
#endif
#ifdef SCAN_API
	module_depend_resolve(&scan_socks_depend[0]);
	module_depend_resolve(&scan_http_depend[0]);
	m_scan_init(module_load);
	scan_socks_init(module_load);
	scan_http_init(module_load);
#endif

}
#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Load(int module_load)
#else
int    l_commands_Load(int module_load)
#endif
{
	m_sethost_Load(module_load);
	m_setname_Load(module_load);
	m_chghost_Load(module_load);
	m_chgident_Load(module_load);
	m_setident_Load(module_load);
	m_sdesc_Load(module_load);
	m_svsmode_Load(module_load);
	m_swhois_Load(module_load);
	m_svsmotd_Load(module_load);
	m_svsnline_Load(module_load);
	m_who_Load(module_load);
	m_mkpasswd_Load(module_load);
	m_away_Load(module_load);
	m_svsnoop_Load(module_load);
	m_svso_Load(module_load);
	m_svsnick_Load(module_load);
	m_adminchat_Load(module_load);
	m_techat_Load(module_load);
	m_nachat_Load(module_load);
	m_lag_Load(module_load);
	m_rping_Load(module_load);
	m_sendumode_Load(module_load);
	m_tsctl_Load(module_load);
	m_htm_Load(module_load);
	m_chgname_Load(module_load);
	m_message_Load(module_load);
	m_whois_Load(module_load);
	m_quit_Load(module_load);
	m_kill_Load(module_load);
	m_pingpong_Load(module_load);
	m_oper_Load(module_load);
	m_akill_Load(module_load);
	m_rakill_Load(module_load);
	m_zline_Load(module_load);
	m_unzline_Load(module_load);
	m_kline_Load(module_load);
	m_unkline_Load(module_load);
	m_sqline_Load(module_load);
	m_unsqline_Load(module_load);
#ifdef GUEST
	m_guest_Load(module_load);
#endif
#ifdef SCAN_API
	m_scan_load(module_load);
	scan_socks_load(module_load);
	scan_http_load(module_load);
#endif
}

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Unload(int module_unload)
#else
int	l_commands_Unload(int module_unload)
#endif
{
	m_sethost_Unload();
	m_setname_Unload();
	m_chghost_Unload();
	m_chgident_Unload();
	m_setident_Unload();
	m_sdesc_Unload();
	m_svsmode_Unload();
	m_swhois_Unload();
	m_svsmotd_Unload();
	m_svsnline_Unload();
	m_who_Unload();
	m_mkpasswd_Unload();
	m_away_Unload();
	m_svsnoop_Unload();
	m_svso_Unload();
	m_svsnick_Unload();
	m_adminchat_Unload();
	m_techat_Unload();
	m_nachat_Unload();
	m_lag_Unload();
	m_rping_Unload();
	m_sendumode_Unload();
	m_tsctl_Unload();
	m_htm_Unload();
	m_chgname_Unload();
	m_message_Unload();
	m_whois_Unload();
	m_quit_Unload();
	m_kill_Unload();
	m_pingpong_Unload();
	m_oper_Unload();
	m_akill_Unload();
	m_rakill_Unload();
	m_zline_Unload();
	m_unzline_Unload();
	m_kline_Unload();
	m_unkline_Unload();
	m_sqline_Unload();
	m_unsqline_Unload();
#ifdef GUEST
	m_guest_Unload();
#endif
#ifdef SCAN_API
	scan_socks_unload();
	scan_http_unload();
	m_scan_unload();
#endif
}

