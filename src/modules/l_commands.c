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
extern ModuleHeader scan_socks_Header;
extern ModuleHeader scan_http_Header;
#endif
extern ModuleHeader m_svsnoop_Header;

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

extern int m_sethost_Init(int module_load), m_setname_Init(int module_load), m_chghost_Init(int module_load);
extern int m_chgident_Init(int module_load), m_setident_Init(int module_load), m_sdesc_Init(int module_load);
extern int m_svsmode_Init(int module_load), m_swhois_Init(int module_load), m_svsmotd_Init(int module_load);
extern int m_svsnline_Init(int module_load), m_who_Init(int module_load), m_mkpasswd_Init(int module_load);
extern int m_away_Init(int module_load), m_svsnoop_Init(int module_load), m_svso_Init(int module_load);
extern int m_svsnick_Init(int module_load), m_adminchat_Init(int module_load), m_nachat_Init(int module_load);
extern int m_lag_Init(int module_load), m_rping_Init(int module_load), m_sendumode_Init(int module_load);
extern int m_tsctl_Init(int module_load), m_htm_Init(int module_load), m_chgname_Init(int module_load);
extern int m_message_Init(int module_load), m_whois_Init(int module_load), m_quit_Init(int module_load);
extern int m_kill_Init(int module_load), m_pingpong_Init(int module_load), m_oper_Init(int module_load);
extern int m_akill_Init(int module_load), m_rakill_Init(int module_load), m_zline_Init(int module_load);
extern int m_unzline_Init(int module_load), m_kline_Init(int module_load), m_unkline_Init(int module_load);
extern int m_sqline_Init(int module_load), m_unsqline_Init(int module_load), m_tkl_Init(int module_load);
#ifdef GUEST
extern int m_guest_Init(int module_load);
#endif
#ifdef SCAN_API
extern int m_scan_Init(int module_load), scan_socks_Init(int module_load), scan_http_Init(int module_load);
#endif

extern int m_sethost_Load(int module_load), m_setname_Load(int module_load), m_chghost_Load(int module_load);
extern int m_chgident_Load(int module_load), m_setident_Load(int module_load), m_sdesc_Load(int module_load);
extern int m_svsmode_Load(int module_load), m_swhois_Load(int module_load), m_svsmotd_Load(int module_load);
extern int m_svsnline_Load(int module_load), m_who_Load(int module_load), m_mkpasswd_Load(int module_load);
extern int m_away_Load(int module_load), m_svsnoop_Load(int module_load), m_svso_Load(int module_load);
extern int m_svsnick_Load(int module_load), m_adminchat_Load(int module_load), m_nachat_Load(int module_load);
extern int m_lag_Load(int module_load), m_rping_Load(int module_load), m_sendumode_Load(int module_load);
extern int m_tsctl_Load(int module_load), m_htm_Load(int module_load), m_chgname_Load(int module_load);
extern int m_message_Load(int module_load), m_whois_Load(int module_load), m_quit_Load(int module_load);
extern int m_kill_Load(int module_load), m_pingpong_Load(int module_load), m_oper_Load(int module_load);
extern int m_akill_Load(int module_load), m_rakill_Load(int module_load), m_zline_Load(int module_load);
extern int m_unzline_Load(int module_load), m_kline_Load(int module_load), m_unkline_Load(int module_load);
extern int m_sqline_Load(int module_load), m_unsqline_Load(int module_load), m_tkl_Load(int module_load);
#ifdef GUEST
extern int m_guest_Load(int module_load);
#endif
#ifdef SCAN_API
extern int m_scan_Load(int module_load), scan_socks_Load(int module_load), scan_http_Load(int module_load);
#endif

extern int m_sethost_Unload(), m_setname_Unload(), m_chghost_Unload(), m_chgident_Unload();
extern int m_setident_Unload(), m_sdesc_Unload(), m_svsmode_Unload(), m_swhois_Unload();
extern int m_svsmotd_Unload(), m_svsnline_Unload(), m_who_Unload(), m_mkpasswd_Unload();
extern int m_away_Unload(), m_svsnoop_Unload(), m_svso_Unload(), m_svsnick_Unload();
extern int m_adminchat_Unload(), m_nachat_Unload(), m_lag_Unload(), m_rping_Unload(); 
extern int m_sendumode_Unload(), m_tsctl_Unload(), m_htm_Unload(), m_chgname_Unload();
extern int m_message_Unload(), m_whois_Unload(), m_quit_Unload(), m_kill_Unload();
extern int m_pingpong_Unload(), m_oper_Unload(), m_akill_Unload(), m_rakill_Unload();
extern int m_zline_Unload(), m_unzline_Unload(), m_kline_Unload(), m_unkline_Unload();
extern int m_sqline_Unload(), m_unsqline_Unload(), m_tkl_Unload();
#ifdef GUEST
extern int m_guest_Unload();
#endif
#ifdef SCAN_API
extern int m_scan_Unload(), scan_socks_Unload(), scan_http_Unload();
#endif

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(int module_load)
#else
int    l_commands_Init(int module_load)
#endif
{
#ifdef SCAN_API
	Module p;
#endif
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
	m_tkl_Init(module_load);
#ifdef GUEST
	m_guest_Init(module_load);
#endif
#ifdef SCAN_API
        p.header = &scan_socks_Header;
        Module_Depend_Resolve(&p);
        p.header = &scan_http_Header;
        Module_Depend_Resolve(&p);
	m_scan_Init(module_load);
	scan_socks_Init(module_load);
	scan_http_Init(module_load);
#endif
	return MOD_SUCCESS;
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
	m_tkl_Load(module_load);
	m_sqline_Load(module_load);
	m_unsqline_Load(module_load);
#ifdef GUEST
	m_guest_Load(module_load);
#endif
#ifdef SCAN_API
	m_scan_Load(module_load);
	scan_socks_Load(module_load);
	scan_http_Load(module_load);
#endif
	return MOD_SUCCESS;
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
	scan_socks_Unload();
	scan_http_Unload();
	m_scan_Unload();
#endif
	return MOD_SUCCESS;
}

