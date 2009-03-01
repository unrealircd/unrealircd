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
#include "version.h"
#ifndef STATIC_LINKING
#define DYNAMIC_LINKING
#else
#undef DYNAMIC_LINKING
#endif

/* l_commands.c/commands.so is a special case so we have to do this manually :p */
#ifdef DYNAMIC_LINKING
#include "modversion.h"
#endif

extern ModuleHeader m_svsnoop_Header;
ModuleInfo *ModCmdsInfo;
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

extern int m_htm_Test(ModuleInfo *modinfo), m_join_Test(ModuleInfo *modinfo);
extern int m_mode_Test(ModuleInfo *modinfo), m_nick_Test(ModuleInfo *modinfo);
extern int m_tkl_Test(ModuleInfo *modinfo), m_list_Test(ModuleInfo *modinfo);
extern int m_message_Test(ModuleInfo *modinfo);

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
extern int m_akill_Init(ModuleInfo *modinfo), m_rakill_Init(ModuleInfo *modinfo), m_userip_Init(ModuleInfo *modinfo);
extern int m_unzline_Init(ModuleInfo *modinfo), m_unkline_Init(ModuleInfo *modinfo);
extern int m_sqline_Init(ModuleInfo *modinfo), m_unsqline_Init(ModuleInfo *modinfo), m_tkl_Init(ModuleInfo *modinfo);
extern int m_vhost_Init(ModuleInfo *modinfo), m_cycle_Init(ModuleInfo *modinfo), m_svsjoin_Init(ModuleInfo *modinfo);
extern int m_svspart_Init(ModuleInfo *modinfo), m_svslusers_Init(ModuleInfo *modinfo);
extern int m_svswatch_Init(ModuleInfo *modinfo), m_svssilence_Init(ModuleInfo *modinfo);
extern int m_sendsno_Init(ModuleInfo *modinfo), m_svssno_Init(ModuleInfo *modinfo);
extern int m_sajoin_Init(ModuleInfo *modinfo), m_sapart_Init(ModuleInfo *modinfo);
extern int m_kick_Init(ModuleInfo *modinfo), m_topic_Init(ModuleInfo *modinfo);
extern int m_invite_Init(ModuleInfo *modinfo), m_list_Init(ModuleInfo *modinfo);
extern int m_samode_Init(ModuleInfo *modinfo), m_time_Init(ModuleInfo *modinfo);
extern int m_svskill_Init(ModuleInfo *modinfo), m_sjoin_Init(ModuleInfo *modinfo);
extern int m_pass_Init(ModuleInfo *modinfo), m_userhost_Init(ModuleInfo *modinfo);
extern int m_ison_Init(ModuleInfo *modinfo), m_silence_Init(ModuleInfo *modinfo);
extern int m_knock_Init(ModuleInfo *modinfo), m_umode2_Init(ModuleInfo *modinfo);
extern int m_squit_Init(ModuleInfo *modinfo), m_protoctl_Init(ModuleInfo *modinfo);
extern int m_addline_Init(ModuleInfo *modinfo), m_addmotd_Init(ModuleInfo *modinfo);
extern int m_addomotd_Init(ModuleInfo *modinfo), m_wallops_Init(ModuleInfo *modinfo);
extern int m_admin_Init(ModuleInfo *modinfo), m_globops_Init(ModuleInfo *modinfo);
extern int m_locops_Init(ModuleInfo *modinfo), m_chatops_Init(ModuleInfo *modinfo);
extern int m_trace_Init(ModuleInfo *modinfo), m_netinfo_Init(ModuleInfo *modinfo);
extern int m_links_Init(ModuleInfo *modinfo), m_help_Init(ModuleInfo *modinfo);
extern int m_rules_Init(ModuleInfo *modinfo), m_close_Init(ModuleInfo *modinfo);
extern int m_map_Init(ModuleInfo *modinfo), m_eos_Init(ModuleInfo *modinfo);
extern int m_server_Init(ModuleInfo *modinfo), m_stats_Init(ModuleInfo *modinfo);
extern int m_svsfline_Init(ModuleInfo *modinfo), m_undccdeny_Init(ModuleInfo *modinfo);
extern int m_dccdeny_Init(ModuleInfo *modinfo), m_whowas_Init(ModuleInfo *modinfo);
extern int m_connect_Init(ModuleInfo *modinfo), m_dccallow_Init(ModuleInfo *modinfo);
extern int m_nick_Init(ModuleInfo *modinfo), m_user_Init(ModuleInfo *modinfo);
extern int m_mode_Init(ModuleInfo *modinfo), m_watch_Init(ModuleInfo *modinfo);
extern int m_part_Init(ModuleInfo *modinfo), m_join_Init(ModuleInfo *modinfo);
extern int m_motd_Init(ModuleInfo *modinfo), m_opermotd_Init(ModuleInfo *modinfo);
extern int m_botmotd_Init(ModuleInfo *modinfo), m_lusers_Init(ModuleInfo *modinfo);
extern int m_names_Init(ModuleInfo *modinfo);
extern int m_svsnolag_Init(ModuleInfo *modinfo);
#ifdef GUEST
extern int m_guest_Init(ModuleInfo *modinfo);
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
extern int m_akill_Load(int module_load), m_rakill_Load(int module_load), m_userip_Load(int unused);
extern int m_unzline_Load(int module_load), m_unkline_Load(int module_load);
extern int m_sqline_Load(int module_load), m_unsqline_Load(int module_load), m_tkl_Load(int module_load);
extern int m_vhost_Load(int module_load), m_cycle_Load(int module_load), m_svsjoin_Load(int module_load);
extern int m_svspart_Load(int module_load), m_svslusers_Load(int module_load);
extern int m_svswatch_Load(int module_load), m_svssilence_Load(int module_load);
extern int m_sendsno_Load(int module_load), m_svssno_Load(int module_load);
extern int m_sajoin_Load(int module_load), m_sapart_Load(int module_load);
extern int m_kick_Load(int module_load), m_topic_Load(int module_load);
extern int m_invite_Load(int module_load), m_list_Load(int module_load);
extern int m_samode_Load(int module_load), m_time_Load(int module_load);
extern int m_svskill_Load(int module_load), m_sjoin_Load(int module_load);
extern int m_pass_Load(int module_load), m_userhost_Load(int module_load);
extern int m_ison_Load(int module_load), m_silence_Load(int module_load);
extern int m_knock_Load(int module_load), m_umode2_Load(int module_load);
extern int m_squit_Load(int module_load), m_protoctl_Load(int module_load);
extern int m_addline_Load(int module_load), m_addmotd_Load(int module_load);
extern int m_addomotd_Load(int module_load), m_wallops_Load(int module_load);
extern int m_admin_Load(int module_load), m_globops_Load(int module_load);
extern int m_locops_Load(int module_load), m_chatops_Load(int module_load);
extern int m_trace_Load(int module_load), m_netinfo_Load(int module_load);
extern int m_links_Load(int module_load), m_help_Load(int module_load);
extern int m_rules_Load(int module_load), m_close_Load(int module_load);
extern int m_map_Load(int module_load), m_eos_Load(int module_load);
extern int m_server_Load(int module_load), m_stats_Load(int module_load);
extern int m_svsfline_Load(int module_load), m_undccdeny_Load(int module_load);
extern int m_dccdeny_Load(int module_load), m_whowas_Load(int module_load);
extern int m_connect_Load(int module_load), m_dccallow_Load(int module_load);
extern int m_nick_Load(int module_load), m_user_Load(int module_load);
extern int m_mode_Load(int module_load), m_watch_Load(int module_load);
extern int m_part_Load(int module_load), m_join_Load(int module_load);
extern int m_motd_Load(int module_load), m_opermotd_Load(int module_load);
extern int m_botmotd_Load(int module_load), m_lusers_Load(int module_load);
extern int m_names_Load(int module_load);
extern int m_svsnolag_Load(int module_load);
#ifdef GUEST
extern int m_guest_Load(int module_load);
#endif

extern int m_sethost_Unload(), m_setname_Unload(), m_chghost_Unload(), m_chgident_Unload();
extern int m_setident_Unload(), m_sdesc_Unload(), m_svsmode_Unload(), m_swhois_Unload();
extern int m_svsmotd_Unload(), m_svsnline_Unload(), m_who_Unload(), m_mkpasswd_Unload();
extern int m_away_Unload(), m_svsnoop_Unload(), m_svso_Unload(), m_svsnick_Unload();
extern int m_adminchat_Unload(), m_nachat_Unload(), m_lag_Unload(), m_rping_Unload(); 
extern int m_sendumode_Unload(), m_tsctl_Unload(), m_htm_Unload(), m_chgname_Unload();
extern int m_message_Unload(), m_whois_Unload(), m_quit_Unload(), m_kill_Unload();
extern int m_pingpong_Unload(), m_oper_Unload(), m_akill_Unload(), m_rakill_Unload();
extern int m_unzline_Unload(), m_unkline_Unload(), m_userip_Unload();
extern int m_sqline_Unload(), m_unsqline_Unload(), m_tkl_Unload(), m_vhost_Unload();
extern int m_cycle_Unload(), m_svsjoin_Unload(), m_svspart_Unload(), m_svslusers_Unload();
extern int m_svswatch_Unload(), m_svssilence_Unload(), m_svskill_Unload();
extern int m_sendsno_Unload(), m_svssno_Unload(), m_time_Unload();
extern int m_sajoin_Unload(), m_sapart_Unload();
extern int m_kick_Unload(), m_topic_Unload(), m_umode2_Unload();
extern int m_invite_Unload(), m_list_Unload(), m_squit_Unload();
extern int m_samode_Unload(), m_sjoin_Unload(), m_protoctl_Unload();
extern int m_pass_Unload(), m_userhost_Unload(), m_knock_Unload();
extern int m_ison_Unload(), m_silence_Unload();
extern int m_addline_Unload(), m_addmotd_Unload(), m_addomotd_Unload();
extern int m_wallops_Unload(), m_admin_Unload(), m_globops_Unload();
extern int m_locops_Unload(), m_chatops_Unload(), m_trace_Unload();
extern int m_netinfo_Unload(), m_links_Unload(), m_help_Unload();
extern int m_rules_Unload(), m_close_Unload(), m_map_Unload();
extern int m_eos_Unload(), m_server_Unload(), m_stats_Unload();
extern int m_svsfline_Unload(), m_dccdeny_Unload(), m_undccdeny_Unload();
extern int m_whowas_Unload(), m_connect_Unload(), m_dccallow_Unload();
extern int m_nick_Unload(), m_user_Unload(), m_mode_Unload();
extern int m_watch_Unload(), m_part_Unload(), m_join_Unload();
extern int m_motd_Unload(), m_opermotd_Unload(), m_botmotd_Unload();
extern int m_lusers_Unload(), m_names_Unload(), m_svsnolag_Unload();
#ifdef GUEST
extern int m_guest_Unload();
#endif

#ifdef DYNAMIC_LINKING
DLLFUNC int Mod_Test(ModuleInfo *modinfo)
#else
int l_commands_Test(ModuleInfo *modinfo)
#endif
{
#ifdef SCAN_API
	Module p;
#endif
	MARK_AS_OFFICIAL_MODULE(modinfo);
	ModCmdsInfo = modinfo;
	m_htm_Test(ModCmdsInfo);
	m_join_Test(ModCmdsInfo);
	m_mode_Test(ModCmdsInfo);
	m_nick_Test(ModCmdsInfo);
	m_tkl_Test(ModCmdsInfo);
	m_list_Test(ModCmdsInfo);
	m_message_Test(ModCmdsInfo);
	return MOD_SUCCESS;
}


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
	module_load = ModCmdsInfo->module_load;
	m_sethost_Init(ModCmdsInfo);
	m_setname_Init(ModCmdsInfo);
	m_chghost_Init(ModCmdsInfo);
	m_chgident_Init(ModCmdsInfo);
	m_setident_Init(ModCmdsInfo);
	m_sdesc_Init(ModCmdsInfo);
	m_svsmode_Init(ModCmdsInfo);
	m_swhois_Init(ModCmdsInfo);
	m_svsmotd_Init(ModCmdsInfo);
	m_svsnline_Init(ModCmdsInfo);
	m_who_Init(ModCmdsInfo);
	m_mkpasswd_Init(ModCmdsInfo);
	m_away_Init(ModCmdsInfo);
	m_svsnoop_Init(ModCmdsInfo);
	m_svso_Init(ModCmdsInfo);
	m_svsnick_Init(ModCmdsInfo);
	m_adminchat_Init(ModCmdsInfo);
	m_nachat_Init(ModCmdsInfo);
	m_lag_Init(ModCmdsInfo);
	m_rping_Init(ModCmdsInfo);
	m_sendumode_Init(ModCmdsInfo);
	m_tsctl_Init(ModCmdsInfo);
	m_htm_Init(ModCmdsInfo);
	m_chgname_Init(ModCmdsInfo);
	m_message_Init(ModCmdsInfo);
	m_whois_Init(ModCmdsInfo);
	m_quit_Init(ModCmdsInfo);
	m_kill_Init(ModCmdsInfo);
	m_pingpong_Init(ModCmdsInfo);
	m_oper_Init(ModCmdsInfo);
	m_akill_Init(ModCmdsInfo);
	m_rakill_Init(ModCmdsInfo);
	m_unzline_Init(ModCmdsInfo);
	m_unkline_Init(ModCmdsInfo);
	m_sqline_Init(ModCmdsInfo);
	m_unsqline_Init(ModCmdsInfo);
	m_tkl_Init(ModCmdsInfo);
	m_vhost_Init(ModCmdsInfo);
	m_cycle_Init(ModCmdsInfo);
	m_svsjoin_Init(ModCmdsInfo);
	m_svspart_Init(ModCmdsInfo);
	m_svswatch_Init(ModCmdsInfo);
	m_svssilence_Init(ModCmdsInfo);
	m_svslusers_Init(ModCmdsInfo);
	m_sendsno_Init(ModCmdsInfo);
	m_svssno_Init(ModCmdsInfo);
	m_sajoin_Init(ModCmdsInfo);
	m_sapart_Init(ModCmdsInfo);
	m_samode_Init(ModCmdsInfo);
	m_kick_Init(ModCmdsInfo);
	m_topic_Init(ModCmdsInfo);
	m_invite_Init(ModCmdsInfo);
	m_list_Init(ModCmdsInfo);
	m_time_Init(ModCmdsInfo);
	m_sjoin_Init(ModCmdsInfo);
	m_pass_Init(ModCmdsInfo);
	m_userhost_Init(ModCmdsInfo);
	m_ison_Init(ModCmdsInfo);
	m_silence_Init(ModCmdsInfo);
	m_svskill_Init(ModCmdsInfo);
	m_knock_Init(ModCmdsInfo);
	m_umode2_Init(ModCmdsInfo);
	m_squit_Init(ModCmdsInfo);
	m_protoctl_Init(ModCmdsInfo);
	m_addline_Init(ModCmdsInfo);
	m_addmotd_Init(ModCmdsInfo);
	m_addomotd_Init(ModCmdsInfo);
	m_wallops_Init(ModCmdsInfo);
	m_admin_Init(ModCmdsInfo);
	m_globops_Init(ModCmdsInfo);
	m_locops_Init(ModCmdsInfo);
	m_chatops_Init(ModCmdsInfo);
	m_trace_Init(ModCmdsInfo);
	m_netinfo_Init(ModCmdsInfo);
	m_links_Init(ModCmdsInfo);
	m_help_Init(ModCmdsInfo);
	m_rules_Init(ModCmdsInfo);
	m_close_Init(ModCmdsInfo);
	m_map_Init(ModCmdsInfo);
	m_eos_Init(ModCmdsInfo);
	m_server_Init(ModCmdsInfo);
	m_stats_Init(ModCmdsInfo);
	m_svsfline_Init(ModCmdsInfo);
	m_dccdeny_Init(ModCmdsInfo);
	m_undccdeny_Init(ModCmdsInfo);
	m_whowas_Init(ModCmdsInfo);
	m_connect_Init(ModCmdsInfo);
	m_dccallow_Init(ModCmdsInfo);
	m_userip_Init(ModCmdsInfo);
	m_nick_Init(ModCmdsInfo);
	m_user_Init(ModCmdsInfo);
	m_mode_Init(ModCmdsInfo);
	m_watch_Init(ModCmdsInfo);
	m_part_Init(ModCmdsInfo);
	m_join_Init(ModCmdsInfo);
	m_motd_Init(ModCmdsInfo);
	m_opermotd_Init(ModCmdsInfo);
	m_botmotd_Init(ModCmdsInfo);
	m_lusers_Init(ModCmdsInfo);
	m_names_Init(ModCmdsInfo);
	m_svsnolag_Init(ModCmdsInfo);
#ifdef GUEST
	m_guest_Init(ModCmdsInfo);
#endif
	MARK_AS_OFFICIAL_MODULE(modinfo);
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
	m_unzline_Load(module_load);
	m_unkline_Load(module_load);
	m_tkl_Load(module_load);
	m_sqline_Load(module_load);
	m_unsqline_Load(module_load);
	m_vhost_Load(module_load);
	m_cycle_Load(module_load);
	m_svsjoin_Load(module_load);
	m_svspart_Load(module_load);
	m_svswatch_Load(module_load);
	m_svssilence_Load(module_load);
	m_svslusers_Load(module_load);
	m_sendsno_Load(module_load);
	m_svssno_Load(module_load);
	m_sajoin_Load(module_load);
	m_sapart_Load(module_load);
	m_samode_Load(module_load);
	m_kick_Load(module_load);
	m_topic_Load(module_load);
	m_invite_Load(module_load);
	m_list_Load(module_load);
	m_time_Load(module_load);
	m_svskill_Load(module_load);
	m_sjoin_Load(module_load);
	m_pass_Load(module_load);
	m_userhost_Load(module_load);
	m_ison_Load(module_load);
	m_silence_Load(module_load);
	m_knock_Load(module_load);
	m_umode2_Load(module_load);
	m_squit_Load(module_load);
	m_protoctl_Load(module_load);
	m_addline_Load(module_load);
	m_addmotd_Load(module_load);
	m_addomotd_Load(module_load);
	m_wallops_Load(module_load);
	m_admin_Load(module_load);
	m_globops_Load(module_load);
	m_locops_Load(module_load);
	m_chatops_Load(module_load);
	m_trace_Load(module_load);
	m_netinfo_Load(module_load);
	m_links_Load(module_load);
	m_help_Load(module_load);
	m_rules_Load(module_load);
	m_close_Load(module_load);
	m_map_Load(module_load);
	m_eos_Load(module_load);
	m_server_Load(module_load);
	m_stats_Load(module_load);
	m_svsfline_Load(module_load);
	m_dccdeny_Load(module_load);
	m_undccdeny_Load(module_load);
	m_whowas_Load(module_load);
	m_connect_Load(module_load);
	m_dccallow_Load(module_load);
	m_userip_Load(module_load);
	m_nick_Load(module_load);
	m_user_Load(module_load);
	m_mode_Load(module_load);
	m_watch_Load(module_load);
	m_part_Load(module_load);
	m_join_Load(module_load);
	m_motd_Load(module_load);
	m_opermotd_Load(module_load);
	m_botmotd_Load(module_load);
	m_lusers_Load(module_load);
	m_names_Load(module_load);
	m_svsnolag_Load(module_load);
#ifdef GUEST
	m_guest_Load(module_load);
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
	m_unzline_Unload();
	m_unkline_Unload();
	m_tkl_Unload();
	m_sqline_Unload();
	m_unsqline_Unload();
	m_vhost_Unload();
	m_cycle_Unload();
	m_svsjoin_Unload();
	m_svspart_Unload();
	m_svswatch_Unload();
	m_svssilence_Unload();
	m_svslusers_Unload();
	m_sendsno_Unload();
	m_svssno_Unload();
	m_sajoin_Unload();
	m_sapart_Unload();
	m_samode_Unload();
	m_kick_Unload();
	m_topic_Unload();
	m_invite_Unload();
	m_list_Unload();
	m_time_Unload();
	m_svskill_Unload();
	m_sjoin_Unload();
	m_pass_Unload();
	m_userhost_Unload();
	m_ison_Unload();
	m_silence_Unload();
	m_knock_Unload();
	m_umode2_Unload();
	m_squit_Unload();
	m_protoctl_Unload();
	m_addline_Unload();
	m_addmotd_Unload();
	m_addomotd_Unload();
	m_wallops_Unload();
	m_admin_Unload();
	m_globops_Unload();
	m_locops_Unload();
	m_chatops_Unload();
	m_trace_Unload();
	m_netinfo_Unload();
	m_links_Unload();
	m_help_Unload();
	m_rules_Unload();
	m_close_Unload();
	m_map_Unload();
	m_eos_Unload();
	m_server_Unload();
	m_stats_Unload();
	m_svsfline_Unload();
	m_dccdeny_Unload();
	m_undccdeny_Unload();
	m_whowas_Unload();
	m_connect_Unload();
	m_dccallow_Unload();
	m_userip_Unload();
	m_nick_Unload();
	m_user_Unload();
	m_mode_Unload();
	m_watch_Unload();
	m_part_Unload();
	m_join_Unload();
	m_motd_Unload();
	m_opermotd_Unload();
	m_botmotd_Unload();
	m_lusers_Unload();
	m_names_Unload();
	m_svsnolag_Unload();
#ifdef GUEST
	m_guest_Unload();
#endif
	return MOD_SUCCESS;
}

