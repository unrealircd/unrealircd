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
	long	key;
	long	key2;
	long	key3;
	long	keycrc;
	unsigned x_inah:1;
	char *x_ircnetwork;
	char *x_ircnet005;
	char *x_defserv;
	char *x_services_name;
	char *x_oper_host;
	char *x_admin_host;
	char *x_locop_host;
	char *x_sadmin_host;
	char *x_netadmin_host;
	char *x_coadmin_host;
	char *x_hidden_host;
	char *x_prefix_quit;
	char *x_helpchan;
	char *x_stats_server;
};

enum UHAllowed { UHALLOW_ALWAYS, UHALLOW_NOCHANS, UHALLOW_REJOIN, UHALLOW_NEVER };

struct ChMode {
        long mode;
#ifdef NEWCHFLOODPROT
		ChanFloodProt	floodprot;
#else
        unsigned short  msgs;
        unsigned short  per; 
        unsigned char   kmode;
#endif
};

typedef struct _OperStat {
	struct _OperStat *prev, *next;
	char *flag;
} OperStat;

typedef struct zConfiguration aConfiguration;
struct zConfiguration {
	unsigned som:1;
	unsigned hide_ulines:1;
	unsigned allow_chatops:1;
	unsigned webtv_support:1;
	unsigned no_oper_hiding:1;
	unsigned ident_check:1;
	unsigned fail_oper_warn:1;
	unsigned show_connect_info:1;
	unsigned dont_resolve:1;
	unsigned use_ban_version:1;
	unsigned mkpasswd_for_everyone:1;
	unsigned use_egd;
	long host_timeout;
	int  host_retries;
	char *name_server;
#ifdef THROTTLING
	long throttle_period;
	char throttle_count;
#endif
	char *kline_address;
	long conn_modes;
	long oper_modes;
	char *oper_snomask;
	char *user_snomask;
	char *auto_join_chans;
	char *oper_auto_join_chans;
	char *oper_only_stats;
	OperStat *oper_only_stats_ext;
	int  maxchannelsperuser;
	int  anti_spam_quit_message_time;
	char *egd_path;
	char *static_quit;
#ifdef USE_SSL
	char *x_server_cert_pem;
	char *x_server_key_pem;
	char *trusted_ca_file;
	long ssl_options;
#endif
	enum UHAllowed userhost_allowed;
	char *restrict_usermodes;
	char *restrict_channelmodes;
	char *channel_command_prefix;
	long unknown_flood_bantime;
	long unknown_flood_amount;
	struct ChMode modes_on_join;
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
	aNetwork network;
};

#ifndef DYNCONF_C
extern aConfiguration iConf;
#endif

#define KLINE_ADDRESS		iConf.kline_address
#define CONN_MODES			iConf.conn_modes
#define OPER_MODES			iConf.oper_modes
#define OPER_SNOMASK			iConf.oper_snomask
#define CONNECT_SNOMASK			iConf.user_snomask
#define SHOWOPERMOTD			iConf.som
#define HIDE_ULINES			iConf.hide_ulines
#define ALLOW_CHATOPS			iConf.allow_chatops
#define MAXCHANNELSPERUSER		iConf.maxchannelsperuser
#define WEBTV_SUPPORT			iConf.webtv_support
#define NO_OPER_HIDING			iConf.no_oper_hiding
#define DONT_RESOLVE			iConf.dont_resolve
#define AUTO_JOIN_CHANS			iConf.auto_join_chans
#define OPER_AUTO_JOIN_CHANS		iConf.oper_auto_join_chans
#define HOST_TIMEOUT			iConf.host_timeout
#define HOST_RETRIES			iConf.host_retries
#define NAME_SERVER			iConf.name_server
#define IDENT_CHECK			iConf.ident_check
#define FAILOPER_WARN			iConf.fail_oper_warn
#define SHOWCONNECTINFO			iConf.show_connect_info
#define OPER_ONLY_STATS			iConf.oper_only_stats
#define ANTI_SPAM_QUIT_MSG_TIME		iConf.anti_spam_quit_message_time
#define USE_EGD				iConf.use_egd
#define EGD_PATH			iConf.egd_path

#define ircnetwork			iConf.network.x_ircnetwork
#define ircnet005			iConf.network.x_ircnet005
#define defserv				iConf.network.x_defserv
#define SERVICES_NAME		iConf.network.x_services_name
#define oper_host			iConf.network.x_oper_host
#define admin_host			iConf.network.x_admin_host
#define locop_host			iConf.network.x_locop_host
#define sadmin_host			iConf.network.x_sadmin_host
#define netadmin_host		iConf.network.x_netadmin_host
#define coadmin_host		iConf.network.x_coadmin_host
#define techadmin_host		iConf.network.x_techadmin_host
#define hidden_host			iConf.network.x_hidden_host
#define helpchan			iConf.network.x_helpchan
#define STATS_SERVER			iConf.network.x_stats_server
#define iNAH				iConf.network.x_inah
#define prefix_quit			iConf.network.x_prefix_quit
#define SSL_SERVER_CERT_PEM		(iConf.x_server_cert_pem ? iConf.x_server_cert_pem : "server.cert.pem")
#define SSL_SERVER_KEY_PEM		(iConf.x_server_key_pem ? iConf.x_server_key_pem : "server.key.pem")

#define CLOAK_KEY1			iConf.network.key
#define CLOAK_KEY2			iConf.network.key2
#define CLOAK_KEY3			iConf.network.key3
#define CLOAK_KEYCRC			iConf.network.keycrc
#define STATIC_QUIT			iConf.static_quit
#define UHOST_ALLOWED			iConf.userhost_allowed
#define RESTRICT_USERMODES		iConf.restrict_usermodes
#define RESTRICT_CHANNELMODES		iConf.restrict_channelmodes
#ifdef THROTTLING
#define THROTTLING_PERIOD		iConf.throttle_period
#define THROTTLING_COUNT		iConf.throttle_count
#endif
#define USE_BAN_VERSION			iConf.use_ban_version
#define UNKNOWN_FLOOD_BANTIME		iConf.unknown_flood_bantime
#define UNKNOWN_FLOOD_AMOUNT		iConf.unknown_flood_amount
#define MODES_ON_JOIN			iConf.modes_on_join.mode

#ifdef NO_FLOOD_AWAY
#define AWAY_PERIOD			iConf.away_period
#define AWAY_COUNT			iConf.away_count
#endif
#define NICK_PERIOD			iConf.nick_period
#define NICK_COUNT			iConf.nick_count

#define IDENT_CONNECT_TIMEOUT	iConf.ident_connect_timeout
#define IDENT_READ_TIMEOUT		iConf.ident_read_timeout

#define MKPASSWD_FOR_EVERYONE	iConf.mkpasswd_for_everyone
#define CHANCMDPFX iConf.channel_command_prefix

#define DEFAULT_BANTIME			iConf.default_bantime
#define WHOLIMIT			iConf.who_limit
