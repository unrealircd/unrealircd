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
ModuleInfo ModCmdsInfo;
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
	"3.2-b8-1",
	NULL 
    };

/*
 * The purpose of these ifdefs, are that we can "static" link the ircd if we
 * want to
*/

extern int m_sethost_Init(ModuleInfo *modinfo), m_setname_Init(ModuleInfo *modinfo), m_chghost_Init(ModuleInfo *modinfo);
extern int m_chgident_Init(ModuleInfo *modinfo), m_setident_Init(ModuleInfo *modinfo), m_sdesc_Init(ModuleInfo *modinfo);
extern int m_svsmode_Init(ModuleInfo *modinfo), m_swhois_Init(ModuleInfo *modinfo), m_svsmotd_Init(ModuleInfo *modinfo);
extern int m_svsnline_Init(ModuleInfo *modinfo), m_who_Init(ModuleInfo *modinfo), m_mkpasswd_Init(ModuleInfo *modinfo);
extern int m_away_Init(ModuleInfo *modinfo), m_svsnoop_Init(ModuleInfo *modinfo), m_svso_Init(ModuleInfo *modinfo);
extern int m_svsnick_Init(ModuleInfo *modinfo), m_adminchat_Init(ModuleInfo *modinfo), m_nachat_Init(ModuleInfo *modinfo);
extern int m_lag_Init(ModuleInfo *modinfo), m_rping_Init(ModuleInfo *modinfo), m_sendumode_Init(ModuleInfo *modinfo);
extern int m_tsctl_Init(ModuleInfo *modinfo), m_htm_Init(ModuleInfo *modinfo), m_chgname_Init(ModuleInfo *modinfo);
extern int m_message_Init(ModuleInfo *modinfo), m_whois_Init(ModuleInfo *modinfo), m_quit_Init(ModuleInfo *modinfo);
extern int m_kill_Init(ModuleInfo *modinfo), m_pingpong_Init(ModuleInfo *modinfo), m_oper_Init(ModuleInfo *modinfo);
extern int m_akill_Init(ModuleInfo *modinfo), m_rakill_Init(ModuleInfo *modinfo), m_zline_Init(ModuleInfo *modinfo);
extern int m_unzline_Init(ModuleInfo *modinfo), m_kline_Init(ModuleInfo *modinfo), m_unkline_Init(ModuleInfo *modinfo);
extern int m_sqline_Init(ModuleInfo *modinfo), m_unsqline_Init(ModuleInfo *modinfo), m_tkl_Init(ModuleInfo *modinfo);
extern int m_vhost_Init(ModuleInfo *modinfo), m_cycle_Init(ModuleInfo *modinfo), m_svsjoin_Init(ModuleInfo *modinfo);
extern int m_svspart_Init(ModuleInfo *modinfo);
#ifdef GUEST
extern int m_guest_Init(ModuleInfo *modinfo);
#endif
#ifdef SCAN_API
extern int m_scan_Init(ModuleInfo *modinfo), scan_socks_Init(ModuleInfo *modinfo), scan_http_Init(ModuleInfo *modinfo);
#endif
#ifdef _WIN32
extern int invisibility_Init(ModuleInfo *modinfo);
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
extern int m_vhost_Load(int module_load), m_cycle_Load(int module_load), m_svsjoin_Load(int module_load);
extern int m_svspart_Load(int module_load);
#ifdef GUEST
extern int m_guest_Load(int module_load);
#endif
#ifdef SCAN_API
extern int m_scan_Load(int module_load), scan_socks_Load(int module_load), scan_http_Load(int module_load);
#endif
#ifdef _WIN32
extern int invisibility_Load(int module_load);
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
extern int m_sqline_Unload(), m_unsqline_Unload(), m_tkl_Unload(), m_vhost_Unload();
extern int m_cycle_Unload(), m_svsjoin_Unload(), m_svspart_Unload();
#ifdef GUEST
extern int m_guest_Unload();
#endif
#ifdef SCAN_API
extern int m_scan_Unload(), scan_socks_Unload(), scan_http_Unload();
#endif
#ifdef _WIN32
extern int invisibility_Unload();
#endif

#ifdef DYNAMIC_LINKING
DLLFUNC int	Mod_Init(ModuleInfo *modinfo)
#else
int    l_commands_Init(ModuleInfo *modinfo)
#endif
{
	int module_load;
#ifdef SCAN_API
	Module p;
#endif
	/*
	 * We call our add_Command crap here
	*/
	bcopy(modinfo,&ModCmdsInfo,modinfo->size);
	module_load = ModCmdsInfo.module_load;
	m_sethost_Init(&ModCmdsInfo);
	m_setname_Init(&ModCmdsInfo);
	m_chghost_Init(&ModCmdsInfo);
	m_chgident_Init(&ModCmdsInfo);
	m_setident_Init(&ModCmdsInfo);
	m_sdesc_Init(&ModCmdsInfo);
	m_svsmode_Init(&ModCmdsInfo);
	m_swhois_Init(&ModCmdsInfo);
	m_svsmotd_Init(&ModCmdsInfo);
	m_svsnline_Init(&ModCmdsInfo);
	m_who_Init(&ModCmdsInfo);
	m_mkpasswd_Init(&ModCmdsInfo);
	m_away_Init(&ModCmdsInfo);
	m_svsnoop_Init(&ModCmdsInfo);
	m_svso_Init(&ModCmdsInfo);
	m_svsnick_Init(&ModCmdsInfo);
	m_adminchat_Init(&ModCmdsInfo);
	m_nachat_Init(&ModCmdsInfo);
	m_lag_Init(&ModCmdsInfo);
	m_rping_Init(&ModCmdsInfo);
	m_sendumode_Init(&ModCmdsInfo);
	m_tsctl_Init(&ModCmdsInfo);
	m_htm_Init(&ModCmdsInfo);
	m_chgname_Init(&ModCmdsInfo);
	m_message_Init(&ModCmdsInfo);
	m_whois_Init(&ModCmdsInfo);
	m_quit_Init(&ModCmdsInfo);
	m_kill_Init(&ModCmdsInfo);
	m_pingpong_Init(&ModCmdsInfo);
	m_oper_Init(&ModCmdsInfo);
	m_akill_Init(&ModCmdsInfo);
	m_rakill_Init(&ModCmdsInfo);
	m_zline_Init(&ModCmdsInfo);
	m_unzline_Init(&ModCmdsInfo);
	m_kline_Init(&ModCmdsInfo);
	m_unkline_Init(&ModCmdsInfo);
	m_sqline_Init(&ModCmdsInfo);
	m_unsqline_Init(&ModCmdsInfo);
	m_tkl_Init(&ModCmdsInfo);
	m_vhost_Init(&ModCmdsInfo);
	m_cycle_Init(&ModCmdsInfo);
	m_svsjoin_Init(&ModCmdsInfo);
	m_svspart_Init(&ModCmdsInfo);
#ifdef GUEST
	m_guest_Init(&ModCmdsInfo);
#endif
#ifdef SCAN_API
        p.header = &scan_socks_Header;
        Module_Depend_Resolve(&p);
        p.header = &scan_http_Header;
        Module_Depend_Resolve(&p);
	m_scan_Init(&ModCmdsInfo);
	scan_socks_Init(&ModCmdsInfo);
	scan_http_Init(&ModCmdsInfo);
#endif
#ifdef _WIN32
	invisibility_Init(&ModCmdsInfo);
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
	m_vhost_Load(module_load);
	m_cycle_Load(module_load);
	m_svsjoin_Load(module_load);
	m_svspart_Load(module_load);
#ifdef GUEST
	m_guest_Load(module_load);
#endif
#ifdef SCAN_API
	m_scan_Load(module_load);
	scan_socks_Load(module_load);
	scan_http_Load(module_load);
#endif
#ifdef _WIN32
	invisibility_Load(module_load);
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
	m_vhost_Unload();
	m_cycle_Unload();
	m_svsjoin_Unload();
	m_svspart_Unload();
#ifdef GUEST
	m_guest_Unload();
#endif
#ifdef SCAN_API
	scan_socks_Unload();
	scan_http_Unload();
	m_scan_Unload();
#endif
#ifdef _WIN32
	invisibility_Unload();
#endif
	return MOD_SUCCESS;
}

