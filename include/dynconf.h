/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dynconf.h
 *   Copyright (C) 1999 Carsten Munk
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
 *
 *   $Id$
 */


#define DYNCONF_H
/* config level */
#define DYNCONF_CONF_VERSION "1.5"
#define DYNCONF_NETWORK_VERSION "2.2"

typedef struct zNetwork aNetwork;
struct zNetwork {
	unsigned x_inah:1;
	char *x_ircnetwork;
	char *x_ircnet005;
	char *x_defserv;
	char *x_services_name;
	char *x_hidden_host;
	char *x_prefix_quit;
	char *x_helpchan;
	char *x_stats_server;
	char *x_sasl_server;
};

enum UHAllowed { UHALLOW_ALWAYS, UHALLOW_NOCHANS, UHALLOW_REJOIN, UHALLOW_NEVER };

struct ChMode {
        long mode;
	long extmodes;
	char *extparams[EXTCMODETABLESZ];
};

typedef struct _OperStat {
	struct _OperStat *prev, *next;
	char *flag;
} OperStat;

typedef struct zConfiguration aConfiguration;
struct zConfiguration {
	unsigned som:1;
	unsigned hide_ulines:1;
	unsigned flat_map:1;
	unsigned allow_chatops:1;
	unsigned ident_check:1;
	unsigned fail_oper_warn:1;
	unsigned show_connect_info:1;
	unsigned dont_resolve:1;
	unsigned use_ban_version:1;
	unsigned mkpasswd_for_everyone:1;
	unsigned hide_ban_reason;
	unsigned allow_insane_bans;
	unsigned allow_part_if_shunned:1;
	unsigned disable_cap:1;
	unsigned check_target_nick_bans:1;
	unsigned use_egd : 1;
	char *dns_bindip;
	char *link_bindip;
	long throttle_period;
	char throttle_count;
	char *kline_address;
	char *gline_address;
	long conn_modes;
	long oper_modes;
	char *oper_snomask;
	char *user_snomask;
	char *auto_join_chans;
	char *oper_auto_join_chans;
	char *oper_only_stats;
	OperStat *oper_only_stats_ext;
	int  maxchannelsperuser;
	int  maxdccallow;
	int  anti_spam_quit_message_time;
	char *egd_path;
	char *static_quit;
	char *static_part;
	char *x_server_cert_pem;
	char *x_server_key_pem;
	char *x_server_cipher_list;
	char *x_dh_pem;
	char *trusted_ca_file;
	long ssl_options;
	int ssl_renegotiate_bytes;
	int ssl_renegotiate_timeout;
	
	enum UHAllowed userhost_allowed;
	char *restrict_usermodes;
	char *restrict_channelmodes;
	char *restrict_extendedbans;
	int new_linking_protocol;
	char *channel_command_prefix;
	long unknown_flood_bantime;
	long unknown_flood_amount;
	struct ChMode modes_on_join;
	int level_on_join;
#ifdef NO_FLOOD_AWAY
	unsigned char away_count;
	long away_period;
#endif
	unsigned char nick_count;
	long nick_period;
	int ident_connect_timeout;
	int ident_read_timeout;
	long default_bantime;
	int who_limit;
	int silence_limit;
	unsigned char modef_default_unsettime;
	unsigned char modef_max_unsettime;
	long ban_version_tkl_time;
	long spamfilter_ban_time;
	char *spamfilter_ban_reason;
	char *spamfilter_virus_help_channel;
	char spamfilter_vchan_deny;
	SpamExcept *spamexcept;
	char *spamexcept_line;
	long spamfilter_detectslow_warn;
	long spamfilter_detectslow_fatal;
	int spamfilter_stop_on_first_match;
	int maxbans;
	int maxbanlength;
	int timesynch_enabled;
	int timesynch_timeout;
	char *timesynch_server;
	int watch_away_notification;
	int uhnames;
	aNetwork network;
	unsigned short default_ipv6_clone_mask;
	int ping_cookie;
	int nicklen;
};

#ifndef DYNCONF_C
extern MODVAR aConfiguration iConf;
extern MODVAR int ipv6_disabled;
#endif

#define KLINE_ADDRESS			iConf.kline_address
#define GLINE_ADDRESS			iConf.gline_address
#define CONN_MODES			iConf.conn_modes
#define OPER_MODES			iConf.oper_modes
#define OPER_SNOMASK			iConf.oper_snomask
#define CONNECT_SNOMASK			iConf.user_snomask
#define SHOWOPERMOTD			iConf.som
#define HIDE_ULINES			iConf.hide_ulines
#define FLAT_MAP			iConf.flat_map
#define ALLOW_CHATOPS			iConf.allow_chatops
#define MAXCHANNELSPERUSER		iConf.maxchannelsperuser
#define MAXDCCALLOW			iConf.maxdccallow
#define DONT_RESOLVE			iConf.dont_resolve
#define AUTO_JOIN_CHANS			iConf.auto_join_chans
#define OPER_AUTO_JOIN_CHANS		iConf.oper_auto_join_chans
#define DNS_BINDIP			iConf.dns_bindip
#define LINK_BINDIP			iConf.link_bindip
#define IDENT_CHECK			iConf.ident_check
#define FAILOPER_WARN			iConf.fail_oper_warn
#define SHOWCONNECTINFO			iConf.show_connect_info
#define OPER_ONLY_STATS			iConf.oper_only_stats
#define ANTI_SPAM_QUIT_MSG_TIME		iConf.anti_spam_quit_message_time
#ifdef HAVE_RAND_EGD
#define USE_EGD				iConf.use_egd
#else
#define USE_EGD				0
#endif
#define EGD_PATH			iConf.egd_path

#define ircnetwork			iConf.network.x_ircnetwork
#define ircnet005			iConf.network.x_ircnet005
#define defserv				iConf.network.x_defserv
#define SERVICES_NAME		iConf.network.x_services_name
#define hidden_host			iConf.network.x_hidden_host
#define helpchan			iConf.network.x_helpchan
#define STATS_SERVER			iConf.network.x_stats_server
#define SASL_SERVER			iConf.network.x_sasl_server
#define iNAH				iConf.network.x_inah
#define PREFIX_QUIT			iConf.network.x_prefix_quit
#define SSL_SERVER_CERT_PEM		iConf.x_server_cert_pem
#define SSL_SERVER_KEY_PEM		iConf.x_server_key_pem

#define STATIC_QUIT			iConf.static_quit
#define STATIC_PART			iConf.static_part
#define UHOST_ALLOWED			iConf.userhost_allowed
#define RESTRICT_USERMODES		iConf.restrict_usermodes
#define RESTRICT_CHANNELMODES		iConf.restrict_channelmodes
#define RESTRICT_EXTENDEDBANS		iConf.restrict_extendedbans
#define NEW_LINKING_PROTOCOL		iConf.new_linking_protocol
#define THROTTLING_PERIOD		iConf.throttle_period
#define THROTTLING_COUNT		iConf.throttle_count
#define USE_BAN_VERSION			iConf.use_ban_version
#define UNKNOWN_FLOOD_BANTIME		iConf.unknown_flood_bantime
#define UNKNOWN_FLOOD_AMOUNT		iConf.unknown_flood_amount
#define MODES_ON_JOIN			iConf.modes_on_join.mode
#define LEVEL_ON_JOIN			iConf.level_on_join

#ifdef NO_FLOOD_AWAY
#define AWAY_PERIOD			iConf.away_period
#define AWAY_COUNT			iConf.away_count
#endif
#define NICK_PERIOD			iConf.nick_period
#define NICK_COUNT			iConf.nick_count

#define IDENT_CONNECT_TIMEOUT	iConf.ident_connect_timeout
#define IDENT_READ_TIMEOUT		iConf.ident_read_timeout

#define MKPASSWD_FOR_EVERYONE	iConf.mkpasswd_for_everyone
#define HIDE_BAN_REASON		iConf.hide_ban_reason
#define ALLOW_INSANE_BANS		iConf.allow_insane_bans
#define CHANCMDPFX iConf.channel_command_prefix

#define DEFAULT_BANTIME			iConf.default_bantime
#define WHOLIMIT			iConf.who_limit

#define MODEF_DEFAULT_UNSETTIME	iConf.modef_default_unsettime
#define MODEF_MAX_UNSETTIME		iConf.modef_max_unsettime

#define ALLOW_PART_IF_SHUNNED	iConf.allow_part_if_shunned

#define DISABLE_CAP	iConf.disable_cap

#define DISABLE_IPV6	ipv6_disabled

#define BAN_VERSION_TKL_TIME	iConf.ban_version_tkl_time
#define SILENCE_LIMIT (iConf.silence_limit ? iConf.silence_limit : 15)

#define SPAMFILTER_BAN_TIME		iConf.spamfilter_ban_time
#define SPAMFILTER_BAN_REASON	iConf.spamfilter_ban_reason
#define SPAMFILTER_VIRUSCHAN	iConf.spamfilter_virus_help_channel
#define SPAMFILTER_VIRUSCHANDENY	iConf.spamfilter_vchan_deny
#define SPAMFILTER_EXCEPT		iConf.spamexcept_line
#define SPAMFILTER_DETECTSLOW_WARN	iConf.spamfilter_detectslow_warn
#define SPAMFILTER_DETECTSLOW_FATAL	iConf.spamfilter_detectslow_fatal
#define SPAMFILTER_STOP_ON_FIRST_MATCH	iConf.spamfilter_stop_on_first_match

#define CHECK_TARGET_NICK_BANS	iConf.check_target_nick_bans

#define MAXBANS		iConf.maxbans
#define MAXBANLENGTH	iConf.maxbanlength

#define TIMESYNCH	iConf.timesynch_enabled
#define TIMESYNCH_TIMEOUT	iConf.timesynch_timeout
#define TIMESYNCH_SERVER	iConf.timesynch_server

#define WATCH_AWAY_NOTIFICATION	iConf.watch_away_notification

#define UHNAMES_ENABLED	iConf.uhnames

/* Used for "is present?" and duplicate checking */
struct SetCheck {
	unsigned has_show_opermotd:1;
	unsigned has_hide_ulines:1;
	unsigned has_flat_map:1;
	unsigned has_allow_chatops:1;
	unsigned has_ident_check:1;
	unsigned has_fail_oper_warn:1;
	unsigned has_show_connect_info:1;
	unsigned has_dont_resolve:1;
	unsigned has_mkpasswd_for_everyone:1;
	unsigned has_allow_part_if_shunned:1;
	unsigned has_ssl_egd:1;
	unsigned has_ssl_server_cipher_list :1;
	unsigned has_dns_bind_ip:1;
	unsigned has_link_bind_ip:1;
	unsigned has_throttle_period:1;
	unsigned has_throttle_connections:1;
	unsigned has_kline_address:1;
	unsigned has_gline_address:1;
	unsigned has_modes_on_connect:1;
	unsigned has_modes_on_oper:1;
	unsigned has_snomask_on_connect:1;
	unsigned has_snomask_on_oper:1;
	unsigned has_auto_join:1;
	unsigned has_oper_auto_join:1;
	unsigned has_check_target_nick_bans:1;
	unsigned has_watch_away_notification:1;
	unsigned has_uhnames:1;
	unsigned has_oper_only_stats:1;
	unsigned has_maxchannelsperuser:1;
	unsigned has_maxdccallow:1;
	unsigned has_anti_spam_quit_message_time:1;
	unsigned has_egd_path:1;
	unsigned has_static_quit:1;
	unsigned has_static_part:1;
	unsigned has_ssl_certificate:1;
	unsigned has_ssl_key:1;
	unsigned has_ssl_trusted_ca_file:1;
	unsigned has_ssl_options:1;
	unsigned has_ssl_dh:1;
	unsigned has_renegotiate_timeout : 1;
	unsigned has_renegotiate_bytes : 1;
	unsigned has_allow_userhost_change:1;
	unsigned has_restrict_usermodes:1;
	unsigned has_restrict_channelmodes:1;
	unsigned has_restrict_extendedbans:1;
	unsigned has_new_linking_protocol:1;
	unsigned has_channel_command_prefix:1;
	unsigned has_anti_flood_unknown_flood_bantime:1;
	unsigned has_anti_flood_unknown_flood_amount:1;
	unsigned has_modes_on_join:1;
	unsigned has_level_on_join:1;
#ifdef NO_FLOOD_AWAY
	unsigned has_anti_flood_away_count:1;
	unsigned has_anti_flood_away_period:1;
#endif
	unsigned has_anti_flood_nick_flood:1;
	unsigned has_anti_flood_connect_flood:1;
	unsigned has_ident_connect_timeout:1;
	unsigned has_ident_read_timeout:1;
	unsigned has_default_bantime:1;
	unsigned has_who_limit:1;
	unsigned has_maxbans:1;
	unsigned has_maxbanlength:1;
	unsigned has_silence_limit:1;
	unsigned has_modef_default_unsettime:1;
	unsigned has_modef_max_unsettime:1;
	unsigned has_ban_version_tkl_time:1;
	unsigned has_spamfilter_ban_time:1;
	unsigned has_spamfilter_ban_reason:1;
	unsigned has_spamfilter_virus_help_channel:1;
	unsigned has_spamfilter_virus_help_channel_deny:1;
	unsigned has_spamfilter_except:1;
	unsigned has_network_name:1;
	unsigned has_default_server:1;
	unsigned has_services_server:1;
	unsigned has_sasl_server:1;
	unsigned has_hiddenhost_prefix:1;
	unsigned has_prefix_quit:1;
	unsigned has_help_channel:1;
	unsigned has_stats_server:1;
	unsigned has_cloak_keys:1;
	unsigned has_options_hide_ulines:1;
	unsigned has_options_flat_map:1;
	unsigned has_options_show_opermotd:1;
	unsigned has_options_identd_check:1;
	unsigned has_options_fail_oper_warn:1;
	unsigned has_options_dont_resolve:1;
	unsigned has_options_show_connect_info:1;
	unsigned has_options_mkpasswd_for_everyone:1;
	unsigned has_options_allow_insane_bans:1;
	unsigned has_options_allow_part_if_shunned:1;
	unsigned has_options_disable_cap:1;
	unsigned has_options_disable_ipv6:1;
	unsigned has_ping_cookie:1;
	unsigned has_nicklen:1;
	unsigned has_hide_ban_reason:1;
};


