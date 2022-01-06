/************************************************************************
 *   Unreal Internet Relay Chat Daemon, include/dynconf.h
 *   Copyright (C) 1999-2003 Carsten Munk
 *   Copyright (C) 2003-2021 Bram Matthys
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

typedef struct FloodSettings FloodSettings;

struct FloodSettings {
	FloodSettings *prev, *next;
	char *name;
	int limit[MAXFLOODOPTIONS];
	long period[MAXFLOODOPTIONS];
};

enum UHAllowed { UHALLOW_ALWAYS, UHALLOW_NOCHANS, UHALLOW_REJOIN, UHALLOW_NEVER };

struct ChMode {
	long mode;
	long extmodes;
	char *extparams[256];
};

typedef struct OperStat {
	struct OperStat *prev, *next;
	char *flag;
} OperStat;

typedef enum BroadcastChannelMessagesOption { BROADCAST_CHANNEL_MESSAGES_AUTO=1, BROADCAST_CHANNEL_MESSAGES_ALWAYS=2, BROADCAST_CHANNEL_MESSAGES_NEVER=3 } BroadcastChannelMessagesOption;

typedef enum AllowedChannelChars { ALLOWED_CHANNELCHARS_ANY=1, ALLOWED_CHANNELCHARS_ASCII=2, ALLOWED_CHANNELCHARS_UTF8=3 } AllowedChannelChars;

typedef enum BanTarget { BAN_TARGET_IP=1, BAN_TARGET_USERIP=2, BAN_TARGET_HOST=3, BAN_TARGET_USERHOST=4, BAN_TARGET_ACCOUNT=5, BAN_TARGET_CERTFP=6 } BanTarget;

typedef enum HideIdleTimePolicy { HIDE_IDLE_TIME_NEVER=1, HIDE_IDLE_TIME_ALWAYS=2, HIDE_IDLE_TIME_USERMODE=3, HIDE_IDLE_TIME_OPER_USERMODE=4 } HideIdleTimePolicy;

/** The set { } block configuration */
typedef struct Configuration Configuration;
struct Configuration {
	unsigned show_opermotd:1;
	unsigned hide_ulines:1;
	unsigned flat_map:1;
	unsigned ident_check:1;
	unsigned fail_oper_warn:1;
	unsigned show_connect_info:1;
	unsigned no_connect_tls_info:1;
	unsigned dont_resolve:1;
	unsigned use_ban_version:1;
	unsigned mkpasswd_for_everyone:1;
	unsigned hide_ban_reason;
	unsigned allow_insane_bans;
	unsigned allow_part_if_shunned:1;
	unsigned disable_cap:1;
	unsigned check_target_nick_bans:1;
	char *link_bindip;
	long throttle_period;
	char throttle_count;
	char *kline_address;
	char *gline_address;
	long conn_modes;
	long oper_modes;
	char *oper_snomask;
	char *auto_join_chans;
	char *oper_auto_join_chans;
	char *allow_user_stats;
	OperStat *allow_user_stats_ext;
	int ping_warning;
	int maxchannelsperuser;
	int maxdccallow;
	int anti_spam_quit_message_time;
	char *static_quit;
	char *static_part;
	TLSOptions *tls_options;
	Policy plaintext_policy_user;
	MultiLine *plaintext_policy_user_message;
	Policy plaintext_policy_oper;
	MultiLine *plaintext_policy_oper_message;
	Policy plaintext_policy_server;
	Policy outdated_tls_policy_user;
	char *outdated_tls_policy_user_message;
	Policy outdated_tls_policy_oper;
	char *outdated_tls_policy_oper_message;
	Policy outdated_tls_policy_server;
	enum UHAllowed userhost_allowed;
	char *restrict_usermodes;
	char *restrict_channelmodes;
	char *restrict_extendedbans;
	int named_extended_bans;
	char *channel_command_prefix;
	long handshake_data_flood_amount;
	long handshake_data_flood_ban_time;
	int handshake_data_flood_ban_action;
	struct ChMode modes_on_join;
	int modes_on_join_set;
	char *level_on_join;
	FloodSettings *floodsettings;
	int ident_connect_timeout;
	int ident_read_timeout;
	long default_bantime;
	int who_limit;
	int silence_limit;
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
	int watch_away_notification;
	int uhnames;
	unsigned short default_ipv6_clone_mask;
	int ping_cookie;
	int min_nick_length;
	int nick_length;
	int topic_length;
	int kick_length;
	int quit_length;
	int away_length;
	int hide_list;
	int max_unknown_connections_per_ip;
	long handshake_timeout;
	long sasl_timeout;
	long handshake_delay;
	BanTarget automatic_ban_target;
	BanTarget manual_ban_target;
	char *reject_message_too_many_connections;
	char *reject_message_server_full;
	char *reject_message_unauthorized;
	char *reject_message_kline;
	char *reject_message_gline;
	int topic_setter;
	int ban_setter;
	int ban_setter_sync;
	int part_instead_of_quit_on_comment_change;
	BroadcastChannelMessagesOption broadcast_channel_messages;
	AllowedChannelChars allowed_channelchars;
	HideIdleTimePolicy hide_idle_time;
	unsigned inah:1;
	char *network_name;
	char *network_name_005;
	char *default_server;
	char *services_name;
	char *cloak_prefix;
	char *prefix_quit;
	char *helpchan;
	char *stats_server;
	char *sasl_server;
	int server_notice_colors;
	int server_notice_show_event;
};

extern MODVAR Configuration iConf;
extern MODVAR Configuration tempiConf;
extern MODVAR int ipv6_disabled;

#define KLINE_ADDRESS			iConf.kline_address
#define GLINE_ADDRESS			iConf.gline_address
#define CONN_MODES			iConf.conn_modes
#define OPER_MODES			iConf.oper_modes
#define OPER_SNOMASK			iConf.oper_snomask
#define SHOWOPERMOTD			iConf.show_opermotd
#define HIDE_ULINES			iConf.hide_ulines
#define FLAT_MAP			iConf.flat_map
#define ALLOW_CHATOPS			iConf.allow_chatops
#define PINGWARNING			iConf.ping_warning
#define MAXCHANNELSPERUSER		iConf.maxchannelsperuser
#define MAXDCCALLOW			iConf.maxdccallow
#define DONT_RESOLVE			iConf.dont_resolve
#define AUTO_JOIN_CHANS			iConf.auto_join_chans
#define OPER_AUTO_JOIN_CHANS		iConf.oper_auto_join_chans
#define LINK_BINDIP			iConf.link_bindip
#define IDENT_CHECK			iConf.ident_check
#define FAILOPER_WARN			iConf.fail_oper_warn
#define SHOWCONNECTINFO			iConf.show_connect_info
#define NOCONNECTTLSLINFO		iConf.no_connect_tls_info
#define ALLOW_USER_STATS			iConf.allow_user_stats
#define ANTI_SPAM_QUIT_MSG_TIME		iConf.anti_spam_quit_message_time

#define NETWORK_NAME			iConf.network_name
#define NETWORK_NAME_005		iConf.network_name_005
#define DEFAULT_SERVER			iConf.default_server
#define SERVICES_NAME			iConf.services_name
#define CLOAK_PREFIX			iConf.cloak_prefix
#define HELP_CHANNEL			iConf.helpchan
#define STATS_SERVER			iConf.stats_server
#define SASL_SERVER			iConf.sasl_server
#define iNAH				iConf.inah
#define PREFIX_QUIT			iConf.prefix_quit

#define STATIC_QUIT			iConf.static_quit
#define STATIC_PART			iConf.static_part
#define UHOST_ALLOWED			iConf.userhost_allowed
#define RESTRICT_USERMODES		iConf.restrict_usermodes
#define RESTRICT_CHANNELMODES		iConf.restrict_channelmodes
#define RESTRICT_EXTENDEDBANS		iConf.restrict_extendedbans
#define THROTTLING_PERIOD		iConf.throttle_period
#define THROTTLING_COUNT		iConf.throttle_count
#define USE_BAN_VERSION			iConf.use_ban_version
#define MODES_ON_JOIN			iConf.modes_on_join.extmodes
#define LEVEL_ON_JOIN			iConf.level_on_join

#define IDENT_CONNECT_TIMEOUT	iConf.ident_connect_timeout
#define IDENT_READ_TIMEOUT		iConf.ident_read_timeout

#define MKPASSWD_FOR_EVERYONE	iConf.mkpasswd_for_everyone
#define HIDE_BAN_REASON		iConf.hide_ban_reason
#define ALLOW_INSANE_BANS		iConf.allow_insane_bans
#define CHANCMDPFX iConf.channel_command_prefix

#define DEFAULT_BANTIME			iConf.default_bantime
#define WHOLIMIT			iConf.who_limit

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

#define WATCH_AWAY_NOTIFICATION	iConf.watch_away_notification

#define UHNAMES_ENABLED	iConf.uhnames

/** Used for testing the set { } block configuration.
 * It tests if a setting is present and is also used for duplicate checking.
 */
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
	unsigned has_tls_server_cipher_list :1;
	unsigned has_tls_protocols :1;
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
	unsigned has_allow_user_stats:1;
	unsigned has_ping_warning:1;
	unsigned has_maxchannelsperuser:1;
	unsigned has_maxdccallow:1;
	unsigned has_anti_spam_quit_message_time:1;
	unsigned has_static_quit:1;
	unsigned has_static_part:1;
	unsigned has_allow_userhost_change:1;
	unsigned has_restrict_usermodes:1;
	unsigned has_restrict_channelmodes:1;
	unsigned has_restrict_extendedbans:1;
	unsigned has_channel_command_prefix:1;
	unsigned has_modes_on_join:1;
	unsigned has_level_on_join:1;
	unsigned has_ident_connect_timeout:1;
	unsigned has_ident_read_timeout:1;
	unsigned has_default_bantime:1;
	unsigned has_who_limit:1;
	unsigned has_maxbans:1;
	unsigned has_maxbanlength:1;
	unsigned has_silence_limit:1;
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
	unsigned has_options_no_connect_tls_info:1;
	unsigned has_options_mkpasswd_for_everyone:1;
	unsigned has_options_allow_insane_bans:1;
	unsigned has_options_allow_part_if_shunned:1;
	unsigned has_options_disable_cap:1;
	unsigned has_options_disable_ipv6:1;
	unsigned has_ping_cookie:1;
	unsigned has_min_nick_length:1;
	unsigned has_nick_length:1;
	unsigned has_hide_ban_reason:1;
};
